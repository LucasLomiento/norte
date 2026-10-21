#include "navegador/browser/ui/atualizador.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include <sys/prctl.h>
#endif

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "base/version.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/prefs/pref_service.h"
#include "crypto/hash.h"
#include "crypto/secure_hash.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace navegador::atualizador {

namespace {

constexpr net::NetworkTrafficAnnotationTag kAnotacao =
    net::DefineNetworkTrafficAnnotation("navegador_atualizador", R"(
        semantics {
          sender: "Atualizador do Norte"
          description:
            "Procura a última versão do navegador nas Releases do GitHub."
          trigger: "Ao abrir o navegador, ou em 'Procurar agora'."
          data: "Nenhum dado do usuário."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "Configurações do Norte → Geral → Atualizações."
          policy_exception_justification: "Navegador próprio (Norte)."
        })");

// O arquivo da release pra este sistema: o pacote do pacman (Arch e
// derivados) ou o instalador do Windows (por usuário, sem pedir senha).
#if BUILDFLAG(IS_WIN)
constexpr std::string_view kFimDoPacote = "-windows-x64.exe";
#else
constexpr std::string_view kFimDoPacote = ".pkg.tar.zst";
#endif

// Sem mudança na pasta do Norte por este tempo, o pacman terminou: no
// pacote, o VERSAO vem antes do executável, e reiniciar no meio pegava o
// executável pela metade.
constexpr base::TimeDelta kFolgaDoPacman = base::Seconds(3);

struct Versao {
  base::Version chromium;
  int revisao = 0;
};

// "155.0.8059.79-9" ou "v155.0.8059.79-9".
std::optional<Versao> LerVersao(std::string texto) {
  texto = std::string(base::TrimWhitespaceASCII(texto, base::TRIM_ALL));
  if (texto.starts_with("v")) {
    texto.erase(0, 1);
  }
  const size_t traco = texto.rfind('-');
  if (traco == std::string::npos) {
    return std::nullopt;
  }
  Versao versao;
  versao.chromium = base::Version(texto.substr(0, traco));
  if (!versao.chromium.IsValid() ||
      !base::StringToInt(texto.substr(traco + 1), &versao.revisao)) {
    return std::nullopt;
  }
  return versao;
}

bool MaisNova(const Versao& a, const Versao& b) {
  const int ordem = a.chromium.CompareTo(b.chromium);
  return ordem > 0 || (ordem == 0 && a.revisao > b.revisao);
}

// Estado do processo inteiro (uma procura, avisos em todas as janelas e nas
// Configurações).
struct Estado {
  // A versão do processo aberto (o VERSAO lido na partida).
  std::string instalada;
  bool instalada_lida = false;
  // Quem espera a primeira leitura do VERSAO.
  std::vector<base::OnceClosure> lendo;
  // O VERSAO agora: muda quando o pacman instala outra versão.
  std::string no_disco;
  bool procurou = false;
  Andamento andamento;
  std::string url_pacote;
  // SHA-256 do pacote que o GitHub informa ("digest" do arquivo), em
  // hexadecimal; vazio se a release não traz.
  std::string sha256_pacote;
  // Tamanho do pacote em bytes (0 sem a informação), pra porcentagem.
  int64_t tamanho_pacote = 0;
  std::string url_pagina;
  std::unique_ptr<network::SimpleURLLoader> pedido;
  std::vector<base::OnceCallback<void(const std::u16string&)>> esperando;
  base::RepeatingClosureList mudou;
  // Vigia da pasta do Norte, numa sequência de fundo (o FilePathWatcher
  // precisa de uma que vigie descritores), e a folga até o pacman terminar.
  scoped_refptr<base::SequencedTaskRunner> sequencia_da_vigia;
  std::unique_ptr<base::FilePathWatcher> vigia;
  base::OneShotTimer folga_da_vigia;
};

Estado& Atual() {
  static base::NoDestructor<Estado> estado;
  return *estado;
}

void Avisar() {
  Atual().mudou.Notify();
}

void MudarPara(Etapa etapa) {
  Atual().andamento.etapa = etapa;
  Avisar();
}

void Falhou(std::u16string erro) {
  Atual().andamento.erro = std::move(erro);
  MudarPara(Etapa::kErro);
}

std::string LerArquivoDeVersao() {
  base::FilePath pasta;
  std::string texto;
  if (!base::PathService::Get(base::DIR_MODULE, &pasta) ||
      !base::ReadFileToString(pasta.Append("VERSAO"), &texto)) {
    return std::string();
  }
  return std::string(base::TrimWhitespaceASCII(texto, base::TRIM_ALL));
}

// --- Versão instalada por fora ----------------------------------------------

// Relê o VERSAO depois da folga: mais novo que o do processo aberto, a
// versão nova está instalada e falta reiniciar.
void ConferirDisco() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LerArquivoDeVersao),
      base::BindOnce([](std::string versao) {
        Estado& estado = Atual();
        if (versao.empty() || versao == estado.no_disco) {
          return;
        }
        estado.no_disco = versao;
        // Instalando pelo nosso "Atualizar": quem decide é o fim do pkexec.
        if (estado.andamento.etapa == Etapa::kBaixando ||
            estado.andamento.etapa == Etapa::kInstalando) {
          return;
        }
        const std::optional<Versao> disco = LerVersao(versao);
        const std::optional<Versao> aberta = LerVersao(estado.instalada);
        if (!disco || !aberta || !MaisNova(*disco, *aberta)) {
          return;
        }
        estado.andamento.nova = versao;
        estado.andamento.erro.clear();
        MudarPara(Etapa::kInstalada);
      }));
}

void PastaMudou() {
  // Cada arquivo que o pacman escreve avisa: espera ele parar.
  Atual().folga_da_vigia.Start(FROM_HERE, kFolgaDoPacman,
                               base::BindOnce(&ConferirDisco));
}

// Vigia a pasta do executável (o /usr/lib/norte do pacote). Sem a vigia
// (não deu pra ligar), só fica sem o aviso de reiniciar.
void Vigiar() {
  Estado& estado = Atual();
  base::FilePath pasta;
  if (estado.vigia || !base::PathService::Get(base::DIR_MODULE, &pasta)) {
    return;
  }
  estado.sequencia_da_vigia = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()});
  estado.vigia = std::make_unique<base::FilePathWatcher>();
  // A vigia mora no Estado (que nunca é destruído): o Unretained vale.
  estado.sequencia_da_vigia->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePathWatcher* vigia, base::FilePath pasta,
             scoped_refptr<base::SequencedTaskRunner> aqui) {
            vigia->Watch(
                pasta, base::FilePathWatcher::Type::kNonRecursive,
                base::BindRepeating(
                    [](scoped_refptr<base::SequencedTaskRunner> aqui,
                       const base::FilePath&, bool erro) {
                      if (!erro) {
                        aqui->PostTask(FROM_HERE, base::BindOnce(&PastaMudou));
                      }
                    },
                    aqui));
          },
          base::Unretained(estado.vigia.get()), std::move(pasta),
          base::SequencedTaskRunner::GetCurrentDefault()));
}

// Lê o VERSAO uma vez (a versão do processo aberto) e roda `depois`. Na
// primeira leitura, com o pacote, liga a vigia da pasta.
void Ler(base::OnceClosure depois) {
  Estado& estado = Atual();
  if (estado.instalada_lida) {
    std::move(depois).Run();
    return;
  }
  estado.lendo.push_back(std::move(depois));
  if (estado.lendo.size() > 1) {
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&LerArquivoDeVersao),
      base::BindOnce([](std::string versao) {
        Estado& estado = Atual();
        estado.instalada = versao;
        estado.no_disco = versao;
        estado.instalada_lida = true;
        if (!versao.empty()) {
          Vigiar();
        }
        std::vector<base::OnceClosure> lendo = std::move(estado.lendo);
        for (base::OnceClosure& depois : lendo) {
          std::move(depois).Run();
        }
      }));
}

// --- Procurar ----------------------------------------------------------------

void Responder(const std::u16string& frase) {
  std::vector<base::OnceCallback<void(const std::u16string&)>> esperando =
      std::move(Atual().esperando);
  for (auto& resposta : esperando) {
    std::move(resposta).Run(frase);
  }
}

void Chegou(std::optional<std::string> corpo) {
  Estado& estado = Atual();
  estado.pedido.reset();
  std::optional<base::Value> json =
      corpo ? base::JSONReader::Read(*corpo, base::JSON_PARSE_RFC)
            : std::nullopt;
  if (!json || !json->is_dict()) {
    Responder(u"Não deu pra consultar o GitHub agora.");
    return;
  }
  const base::DictValue& release = json->GetDict();
  const std::string* tag = release.FindString("tag_name");
  std::optional<Versao> nova = tag ? LerVersao(*tag) : std::nullopt;
  std::optional<Versao> instalada = LerVersao(estado.instalada);
  if (!nova || !instalada) {
    Responder(u"Não deu pra comparar as versões.");
    return;
  }
  if (!MaisNova(*nova, *instalada)) {
    Responder(u"Você está na versão mais nova (" +
              base::UTF8ToUTF16(estado.instalada) + u").");
    return;
  }
  // Já instalada por fora com o navegador aberto: só falta reiniciar.
  if (std::optional<Versao> disco = LerVersao(estado.no_disco);
      disco && !MaisNova(*nova, *disco)) {
    estado.andamento.nova = estado.no_disco;
    estado.andamento.erro.clear();
    Responder(u"A versão " + base::UTF8ToUTF16(estado.no_disco) +
              u" já está instalada: falta reiniciar.");
    MudarPara(Etapa::kInstalada);
    return;
  }
  std::string url;
  std::string sha256;
  int64_t tamanho = 0;
  if (const base::ListValue* arquivos = release.FindList("assets")) {
    for (const base::Value& arquivo : *arquivos) {
      const base::DictValue* d = arquivo.GetIfDict();
      const std::string* nome = d ? d->FindString("name") : nullptr;
      const std::string* endereco =
          d ? d->FindString("browser_download_url") : nullptr;
      if (nome && endereco && nome->ends_with(kFimDoPacote)) {
        url = *endereco;
        const std::string* digest = d->FindString("digest");
        sha256 = digest && digest->starts_with("sha256:")
                     ? base::ToLowerASCII(digest->substr(7))
                     : std::string();
        tamanho = static_cast<int64_t>(d->FindDouble("size").value_or(0));
      }
    }
  }
  if (url.empty()) {
    Responder(u"A versão nova ainda não tem o pacote.");
    return;
  }
  estado.andamento.nova = tag->starts_with("v") ? tag->substr(1) : *tag;
  estado.andamento.erro.clear();
  estado.url_pacote = url;
  estado.sha256_pacote = sha256;
  estado.tamanho_pacote = tamanho;
  if (const std::string* pagina = release.FindString("html_url")) {
    estado.url_pagina = *pagina;
  }
  Responder(u"Versão nova: " + base::UTF8ToUTF16(estado.andamento.nova) +
            u".");
  MudarPara(Etapa::kDisponivel);
}

// Onde procurar a última versão: a API do GitHub ou, pra testar o
// atualizador sem publicar versão, o endereço em NORTE_ATUALIZACAO_TESTE
// (um JSON como o da API, servido nesta máquina: endereço de fora é
// ignorado). Ver "Testar com a senha de verdade" no docs/ARQUITETURA.md.
GURL EnderecoDaProcura() {
  const GURL github(std::string("https://api.github.com/repos/") +
                    kRepositorio + "/releases/latest");
  const std::optional<std::string> teste =
      base::Environment::Create()->GetVar("NORTE_ATUALIZACAO_TESTE");
  if (!teste) {
    return github;
  }
  const GURL endereco(*teste);
  const bool desta_maquina =
      endereco.is_valid() && endereco.SchemeIs("http") &&
      (endereco.host() == "127.0.0.1" || endereco.host() == "localhost");
  return desta_maquina ? endereco : github;
}

void Procurar() {
  Estado& estado = Atual();
  switch (estado.andamento.etapa) {
    case Etapa::kBaixando:
    case Etapa::kInstalando:
      Responder(u"A atualização já está em andamento.");
      return;
    case Etapa::kInstalada:
      Responder(u"A versão " + base::UTF8ToUTF16(estado.andamento.nova) +
                u" já está instalada: falta reiniciar.");
      return;
    case Etapa::kNada:
    case Etapa::kDisponivel:
    case Etapa::kErro:
      break;
  }
  if (estado.instalada.empty()) {
    Responder(u"Esta cópia não veio do pacote: sem atualização automática.");
    return;
  }
  // Uma procura já em andamento responde a todos.
  if (estado.pedido) {
    return;
  }
  auto pedido = std::make_unique<network::ResourceRequest>();
  pedido->url = EnderecoDaProcura();
  pedido->credentials_mode = network::mojom::CredentialsMode::kOmit;
  pedido->headers.SetHeader("Accept", "application/vnd.github+json");
  estado.pedido =
      network::SimpleURLLoader::Create(std::move(pedido), kAnotacao);
  estado.pedido->DownloadToString(
      g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce(&Chegou), 1024 * 1024);
}

// --- Baixar e instalar -------------------------------------------------------

void Progresso(uint64_t veio) {
  Estado& estado = Atual();
  if (estado.tamanho_pacote <= 0 ||
      estado.andamento.etapa != Etapa::kBaixando) {
    return;
  }
  const int porcento = static_cast<int>(std::clamp<int64_t>(
      static_cast<int64_t>(veio) * 100 / estado.tamanho_pacote, 0, 100));
  if (porcento == estado.andamento.porcento) {
    return;
  }
  estado.andamento.porcento = porcento;
  Avisar();
}

#if !BUILDFLAG(IS_WIN)
void InstalarArquivo(base::FilePath pacote);
#endif

void Baixar() {
  Estado& estado = Atual();
  if (estado.andamento.etapa != Etapa::kDisponivel &&
      estado.andamento.etapa != Etapa::kErro) {
    return;
  }
  estado.andamento.erro.clear();
  estado.andamento.porcento = estado.tamanho_pacote > 0 ? 0 : -1;
  MudarPara(Etapa::kBaixando);
  auto pedido = std::make_unique<network::ResourceRequest>();
  pedido->url = GURL(estado.url_pacote);
  pedido->credentials_mode = network::mojom::CredentialsMode::kOmit;
  estado.pedido =
      network::SimpleURLLoader::Create(std::move(pedido), kAnotacao);
  estado.pedido->SetOnDownloadProgressCallback(base::BindRepeating(&Progresso));
#if BUILDFLAG(IS_WIN)
  // Baixa o instalador e roda: ele põe a versão nova ao lado da atual (a
  // troca vale ao reiniciar), sem pedir senha.
  base::FilePath destino;
  base::GetTempDir(&destino);
  destino = destino.AppendASCII("navegador-instalador.exe");
  estado.pedido->DownloadToFile(
      g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce([](base::FilePath instalador) {
        Atual().pedido.reset();
        if (instalador.empty()) {
          Falhou(u"Não deu pra baixar o instalador.");
          return;
        }
        MudarPara(Etapa::kInstalando);
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                [](base::FilePath instalador) {
                  base::CommandLine comando(instalador);
                  comando.AppendSwitch("do-not-launch-chrome");
                  base::Process processo =
                      base::LaunchProcess(comando, base::LaunchOptions());
                  int codigo = -1;
                  if (!processo.IsValid() ||
                      !processo.WaitForExitWithTimeout(base::Minutes(15),
                                                       &codigo)) {
                    return -1;
                  }
                  base::DeleteFile(instalador);
                  return codigo;
                },
                instalador),
            base::BindOnce([](int codigo) {
              // Códigos do instalador do Chromium (installer::InstallStatus):
              // 0 primeira instalação, 1 reparo, 2 versão nova, 4 já tinha
              // uma mais nova.
              if (codigo == 0 || codigo == 1 || codigo == 2 || codigo == 4) {
                MudarPara(Etapa::kInstalada);
              } else {
                Falhou(u"O instalador não conseguiu atualizar (código " +
                       base::NumberToString16(codigo) + u").");
              }
            }));
      }),
      destino, 512 * 1024 * 1024);
#else
  // Baixa o pacote e instala o arquivo. O `pacman -U <endereço>` exige a
  // assinatura (.sig) de pacote baixado por link (RemoteFileSigLevel =
  // Required, o padrão do Arch), e os pacotes não são assinados: o GitHub
  // dava 404 pro .sig e o pacman saía com 255. Arquivo local vale pelo
  // LocalFileSigLevel (Optional). O pkexec pede a senha numa janela do
  // sistema (o agente do polkit).
  std::string nome = GURL(estado.url_pacote).ExtractFileName();
  if (!base::EndsWith(nome, ".pkg.tar.zst")) {
    nome = "navegador.pkg.tar.zst";
  }
  // Numa pasta só nossa (0700): no /tmp aberto, com nome previsível, outro
  // usuário da máquina podia pôr ou trocar o arquivo antes do pacman, que
  // instala como root.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() {
        base::FilePath pasta;
        if (!base::CreateNewTempDirectory("norte-atualizacao-", &pasta)) {
          pasta.clear();
        }
        return pasta;
      }),
      base::BindOnce(
          [](std::string nome, base::FilePath pasta) {
            Estado& estado = Atual();
            if (pasta.empty() || !estado.pedido) {
              estado.pedido.reset();
              Falhou(u"Não deu pra baixar a versão nova.");
              return;
            }
            estado.pedido->DownloadToFile(
                g_browser_process->shared_url_loader_factory().get(),
                base::BindOnce(&InstalarArquivo), pasta.AppendASCII(nome),
                512 * 1024 * 1024);
          },
          std::move(nome)));
#endif
}

#if !BUILDFLAG(IS_WIN)
// Código de saída nosso: o SHA-256 do pacote não bateu (o pacman nem roda).
constexpr int kPacoteDiferente = -2;

// SHA-256 do arquivo em hexadecimal minúsculo (vazio se não deu pra ler).
std::string Sha256DoArquivo(const base::FilePath& caminho) {
  base::File arquivo(caminho, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!arquivo.IsValid()) {
    return std::string();
  }
  std::unique_ptr<crypto::SecureHash> hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  std::vector<uint8_t> bloco(1 << 20);
  while (true) {
    std::optional<size_t> lidos = arquivo.ReadAtCurrentPos(bloco);
    if (!lidos) {
      return std::string();
    }
    if (*lidos == 0) {
      break;
    }
    hash->Update(base::span(bloco).first(*lidos));
  }
  std::array<uint8_t, crypto::hash::kSha256Size> resultado;
  hash->Finish(resultado);
  return base::HexEncodeLower(resultado);
}

// O pacote já baixado: pkexec pacman -U no arquivo, que é apagado depois
// (com a pasta dele).
void InstalarArquivo(base::FilePath pacote) {
  Atual().pedido.reset();
  if (pacote.empty()) {
    Falhou(u"Não deu pra baixar a versão nova.");
    return;
  }
  // Agora é a janela de senha do sistema.
  MudarPara(Etapa::kInstalando);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](base::FilePath pacote, std::string sha256) {
            // O pacote tem que ser o que a release diz (download cortado ou
            // trocado no caminho não chega no pacman).
            if (!sha256.empty() && Sha256DoArquivo(pacote) != sha256) {
              base::DeletePathRecursively(pacote.DirName());
              return kPacoteDiferente;
            }
            // O Chromium lança os filhos com PR_SET_NO_NEW_PRIVS, e com essa
            // trava o pkexec (setuid) não vira root: sai na hora com 127
            // ("pkexec must be setuid root"), sem pedir a senha. Daí o
            // allow_new_privs. Se o próprio navegador já nasceu com a trava
            // (aberto por quem a liga, como um app Electron), ela é herdada e
            // não sai: o pkexec vai pelo gerenciador do usuário
            // (systemd-run --user), que roda limpo; o --wait devolve o código
            // do pkexec.
            base::CommandLine comando(base::FilePath("/usr/bin/pkexec"));
            if (prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) == 1) {
              comando = base::CommandLine(
                  base::FilePath("/usr/bin/systemd-run"));
              for (const char* arg :
                   {"--user", "--wait", "--collect", "--quiet",
                    "--service-type=exec", "/usr/bin/pkexec"}) {
                comando.AppendArg(arg);
              }
            }
            comando.AppendArg("/usr/bin/pacman");
            comando.AppendArg("-U");
            comando.AppendArg("--noconfirm");
            // Responde "sim" a "X e Y estão em conflito. Remover Y?" (o
            // --noconfirm responde "não"): o pacote vai trocar de nome
            // (navegador -> norte) e o novo substitui o antigo.
            comando.AppendArg("--ask=4");
            comando.AppendArgPath(pacote);
            base::LaunchOptions opcoes;
            opcoes.allow_new_privs = true;
            base::Process processo = base::LaunchProcess(comando, opcoes);
            int codigo = -1;
            if (!processo.IsValid() ||
                !processo.WaitForExitWithTimeout(base::Minutes(15), &codigo)) {
              codigo = -1;
            }
            base::DeletePathRecursively(pacote.DirName());
            return codigo;
          },
          pacote, Atual().sha256_pacote),
      base::BindOnce([](int codigo) {
        if (codigo == 0) {
          MudarPara(Etapa::kInstalada);
        } else if (codigo == kPacoteDiferente) {
          Falhou(u"O pacote baixado não confere com o da release. "
                 u"Tente de novo.");
        } else {
          // 126/127: senha cancelada ou sem a janela de senha do sistema.
          Falhou(codigo == 126 || codigo == 127
                     ? u"Cancelado (ou o sistema não pediu a senha). "
                       u"No terminal: sudo pacman -Syu"
                     : u"O pacman não conseguiu instalar (código " +
                           base::NumberToString16(codigo) + u").");
        }
      }));
}
#endif  // !BUILDFLAG(IS_WIN)

// --- Aviso no pé da lateral -------------------------------------------------

class Aviso : public views::View {
  METADATA_HEADER(Aviso, views::View)

 public:
  Aviso() {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(10, 12), 6));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 8, 8, 8));
    titulo_ = AddChildView(std::make_unique<views::Label>());
    titulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    titulo_->SetFontList(gfx::FontList().DeriveWithWeight(
        gfx::Font::Weight::SEMIBOLD));
    detalhe_ = AddChildView(std::make_unique<views::Label>());
    detalhe_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    detalhe_->SetMultiLine(true);
    detalhe_->SetFontList(gfx::FontList().DeriveWithSizeDelta(-1));
    botao_ = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(&Aviso::Apertou, base::Unretained(this)),
        std::u16string()));
    botao_->SetStyle(ui::ButtonStyle::kProminent);
    botao_->SetProperty(views::kCrossAxisAlignmentKey,
                        views::LayoutAlignment::kStart);
    inscricao_ = Atual().mudou.Add(
        base::BindRepeating(&Aviso::Mostrar, base::Unretained(this)));
    Mostrar();
  }

  void Mostrar() {
    const Andamento& andamento = Atual().andamento;
    const std::u16string nova = base::UTF8ToUTF16(andamento.nova);
    SetVisible(andamento.etapa != Etapa::kNada);
    switch (andamento.etapa) {
      case Etapa::kNada:
        break;
      case Etapa::kDisponivel:
        Escrever(u"Nova versão", u"Versão " + nova, u"Atualizar", true);
        break;
      case Etapa::kBaixando:
        Escrever(u"Baixando…",
                 andamento.porcento >= 0
                     ? base::NumberToString16(andamento.porcento) +
                           u"% da versão " + nova
                     : u"Versão " + nova,
                 u"Atualizando…", false);
        break;
      case Etapa::kInstalando:
        Escrever(u"Instalando…", u"Digite a senha na janela do sistema.",
                 u"Atualizando…", false);
        break;
      case Etapa::kInstalada:
        Escrever(u"Atualizado", u"Reinicie pra usar a versão " + nova + u".",
                 u"Reiniciar", true);
        break;
      case Etapa::kErro:
        Escrever(u"Não atualizou", andamento.erro, u"Tentar de novo", true);
        break;
    }
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(GetColorProvider()->GetColor(ui::kColorAccent),
                               0x26));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), 8, flags);
  }

 private:
  void Escrever(std::u16string titulo,
                std::u16string detalhe,
                std::u16string botao,
                bool ligado) {
    titulo_->SetText(titulo);
    detalhe_->SetText(detalhe);
    detalhe_->SizeToFit(150);
    botao_->SetText(botao);
    botao_->SetEnabled(ligado);
    PreferredSizeChanged();
  }

  void Apertou() {
    if (Atual().andamento.etapa == Etapa::kInstalada) {
      Reiniciar();
      return;
    }
    Atualizar();
  }

  raw_ptr<views::Label> titulo_ = nullptr;
  raw_ptr<views::Label> detalhe_ = nullptr;
  raw_ptr<views::MdTextButton> botao_ = nullptr;
  base::CallbackListSubscription inscricao_;
};

BEGIN_METADATA(Aviso)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CriarAviso(BrowserWindowInterface* browser) {
  Estado& estado = Atual();
  if (!estado.procurou) {
    estado.procurou = true;
    // A versão do processo aberto já na partida (antes de o pacman poder
    // trocar o VERSAO), com a vigia da pasta.
    Ler(base::DoNothing());
    if (browser->GetProfile()->GetPrefs()->GetBoolean(
            espacos::kProcurarAtualizacoesPref)) {
      // Depois de a janela abrir (não atrasa a abertura).
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&Ler, base::BindOnce(&Procurar)),
          base::Seconds(5));
    }
  }
  return std::make_unique<Aviso>();
}

void ProcurarAgora(Profile* perfil,
                   base::OnceCallback<void(const std::u16string&)> resposta) {
  Atual().esperando.push_back(std::move(resposta));
  Ler(base::BindOnce(&Procurar));
}

void LerVersaoInstalada(
    base::OnceCallback<void(const std::string&)> resposta) {
  Ler(base::BindOnce(
      [](base::OnceCallback<void(const std::string&)> resposta) {
        std::move(resposta).Run(Atual().instalada);
      },
      std::move(resposta)));
}

Andamento Agora() {
  return Atual().andamento;
}

base::CallbackListSubscription AoMudar(base::RepeatingClosure mudou) {
  return Atual().mudou.Add(std::move(mudou));
}

void Atualizar() {
  Baixar();
}

void Reiniciar() {
  chrome::AttemptRestart();
}

}  // namespace navegador::atualizador
