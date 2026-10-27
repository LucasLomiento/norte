#include "navegador/browser/ui/importar_zen.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "components/user_data_importer/common/importer_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/containers/fixos.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace navegador::importar_zen {

namespace {

// A etiqueta precisa ser uma das conhecidas (sql/histograms.xml): a do
// importador de Firefox, que é o que isto é.
inline constexpr sql::Database::Tag kTag{"FirefoxImporter"};

// O que se copia do perfil do Zen (com o -wal dos sqlite, pra pegar o que
// ainda não foi gravado no arquivo principal com o Zen aberto).
constexpr const char* kArquivos[] = {
    "places.sqlite",     "places.sqlite-wal",     "favicons.sqlite",
    "favicons.sqlite-wal", "logins.json",         "key4.db",
    "cert9.db",          "pkcs11.txt",            "cookies.sqlite",
    "cookies.sqlite-wal", "containers.json",      "zen-sessions.jsonlz4",
    "prefs.js",          "formhistory.sqlite",    "formhistory.sqlite-wal",
    "compatibility.ini", "times.json",
};

struct ContainerDoZen {
  int id = 0;
  std::string nome;
  std::string cor;
  std::string icone;
};

struct EspacoDoZen {
  std::string uuid;
  std::string nome;
  std::string icone;
  int container = 0;
};

struct PastaDoZen {
  std::string id;
  std::string nome;
  std::string espaco;
  std::string pai;  // id da pasta do Zen em volta (vazio = solta)
};

struct FixadoDoZen {
  std::string espaco;
  std::string url;
  std::string titulo;
  std::string pasta;  // id da pasta do Zen (vazio = solto)
  bool essencial = false;
  int container = 0;
};

struct CookieDoZen {
  int container = 0;
  std::unique_ptr<net::CanonicalCookie> cookie;
  GURL url;
};

struct DadosDoZen {
  std::u16string erro;
  base::FilePath copia;
  std::vector<ContainerDoZen> containers;
  std::vector<EspacoDoZen> espacos;
  std::vector<PastaDoZen> pastas;
  std::vector<FixadoDoZen> fixados;
  std::vector<CookieDoZen> cookies;
};

// --- Achar e copiar o perfil (tarefa de fundo) ------------------------------

std::optional<base::FilePath> PerfilDentroDe(const base::FilePath& raiz) {
  std::string ini;
  if (!base::ReadFileToString(raiz.Append("profiles.ini"), &ini)) {
    return std::nullopt;
  }
  // [Install...] Default=<pasta> é o perfil em uso; senão o [Profile*] com
  // Default=1; senão o primeiro.
  std::string secao;
  std::string do_install;
  std::string padrao;
  std::string primeiro;
  std::string caminho;
  bool relativo = true;
  bool marcado = false;
  auto fechar_perfil = [&]() {
    if (caminho.empty()) {
      return;
    }
    const std::string completo =
        relativo ? raiz.Append(caminho).value() : caminho;
    if (primeiro.empty()) {
      primeiro = completo;
    }
    if (marcado && padrao.empty()) {
      padrao = completo;
    }
  };
  for (const std::string& bruta : base::SplitString(
           ini, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (bruta.starts_with("[")) {
      if (secao.starts_with("Profile")) {
        fechar_perfil();
      }
      secao = bruta.substr(1, bruta.size() - 2);
      caminho.clear();
      relativo = true;
      marcado = false;
      continue;
    }
    const size_t igual = bruta.find('=');
    if (igual == std::string::npos) {
      continue;
    }
    const std::string chave = bruta.substr(0, igual);
    const std::string valor = bruta.substr(igual + 1);
    if (secao.starts_with("Install") && chave == "Default" &&
        do_install.empty()) {
      do_install = raiz.Append(valor).value();
    } else if (secao.starts_with("Profile")) {
      if (chave == "Path") {
        caminho = valor;
      } else if (chave == "IsRelative") {
        relativo = valor == "1";
      } else if (chave == "Default") {
        marcado = valor == "1";
      }
    }
  }
  if (secao.starts_with("Profile")) {
    fechar_perfil();
  }
  for (const std::string& candidato : {do_install, padrao, primeiro}) {
    if (!candidato.empty() &&
        base::PathExists(base::FilePath(candidato).Append("cookies.sqlite"))) {
      return base::FilePath(candidato);
    }
  }
  return std::nullopt;
}

std::optional<base::FilePath> AcharPerfil() {
#if BUILDFLAG(IS_WIN)
  // %APPDATA%\zen (o profiles.ini fica ali, os perfis em Profiles\).
  base::FilePath roaming;
  if (base::PathService::Get(base::DIR_ROAMING_APP_DATA, &roaming)) {
    return PerfilDentroDe(roaming.AppendASCII("zen"));
  }
  return std::nullopt;
#else
  const base::FilePath casa = base::GetHomeDir();
  for (const char* raiz :
       {".config/zen", ".zen", ".var/app/app.zen_browser.zen/config/zen",
        ".var/app/app.zen_browser.zen/.zen"}) {
    if (std::optional<base::FilePath> perfil =
            PerfilDentroDe(casa.Append(raiz))) {
      return perfil;
    }
  }
  return std::nullopt;
#endif
}

// Bloco LZ4 (o "mozLz40" do Firefox é cabeçalho + tamanho + um bloco).
std::optional<std::string> DescomprimirMozLz4(const std::string& dados) {
  if (dados.size() < 12 || dados.compare(0, 8, std::string("mozLz40\0", 8))) {
    return std::nullopt;
  }
  const auto* p = reinterpret_cast<const uint8_t*>(dados.data());
  const uint32_t tamanho = p[8] | (p[9] << 8) | (p[10] << 16) |
                           (static_cast<uint32_t>(p[11]) << 24);
  if (tamanho > 512u * 1024 * 1024) {
    return std::nullopt;
  }
  std::string saida;
  saida.reserve(tamanho);
  size_t i = 12;
  const size_t fim = dados.size();
  while (i < fim) {
    const uint8_t marca = p[i++];
    size_t literais = marca >> 4;
    if (literais == 15) {
      uint8_t b;
      do {
        if (i >= fim) {
          return std::nullopt;
        }
        b = p[i++];
        literais += b;
      } while (b == 255);
    }
    if (i + literais > fim) {
      return std::nullopt;
    }
    saida.append(dados, i, literais);
    i += literais;
    if (i >= fim) {
      break;
    }
    if (i + 2 > fim) {
      return std::nullopt;
    }
    const size_t distancia = p[i] | (p[i + 1] << 8);
    i += 2;
    size_t copia = marca & 15;
    if (copia == 15) {
      uint8_t b;
      do {
        if (i >= fim) {
          return std::nullopt;
        }
        b = p[i++];
        copia += b;
      } while (b == 255);
    }
    copia += 4;
    if (distancia == 0 || distancia > saida.size() ||
        saida.size() + copia > tamanho) {
      return std::nullopt;
    }
    const size_t inicio = saida.size() - distancia;
    for (size_t k = 0; k < copia; ++k) {
      saida.push_back(saida[inicio + k]);
    }
  }
  return saida;
}

std::string CorDoZen(const std::string& cor) {
  static const std::map<std::string, std::string> cores = {
      {"blue", "azul"},      {"turquoise", "turquesa"}, {"green", "verde"},
      {"yellow", "amarelo"}, {"orange", "laranja"},     {"red", "vermelho"},
      {"pink", "rosa"},      {"purple", "roxo"},        {"violet", "roxo"},
      {"toolbar", "cinza"},
  };
  auto it = cores.find(cor);
  return it == cores.end() ? "azul" : it->second;
}

std::string IconeDoZen(const std::string& icone) {
  if (icone == "briefcase") {
    return "maleta";
  }
  if (icone == "dollar") {
    return "cifrao";
  }
  if (icone == "cart") {
    return "carrinho";
  }
  return "pessoa";
}

void LerContainers(const base::FilePath& copia, DadosDoZen& dados) {
  std::string texto;
  if (!base::ReadFileToString(copia.Append("containers.json"), &texto)) {
    return;
  }
  std::optional<base::Value> json = base::JSONReader::Read(
      texto, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  const base::ListValue* lista =
      json && json->is_dict() ? json->GetDict().FindList("identities")
                              : nullptr;
  if (!lista) {
    return;
  }
  static const std::map<std::string, std::string> nomes = {
      {"user-context-personal", "Pessoal"},
      {"user-context-work", "Trabalho"},
      {"user-context-banking", "Banco"},
      {"user-context-shopping", "Compras"},
  };
  for (const base::Value& item : *lista) {
    if (!item.is_dict() || !item.GetDict().FindBool("public").value_or(false)) {
      continue;
    }
    const base::DictValue& d = item.GetDict();
    ContainerDoZen c;
    c.id = d.FindInt("userContextId").value_or(0);
    if (const std::string* nome = d.FindString("name")) {
      c.nome = *nome;
    } else if (const std::string* l10n = d.FindString("l10nId")) {
      auto it = nomes.find(*l10n);
      c.nome = it == nomes.end() ? *l10n : it->second;
    }
    if (const std::string* cor = d.FindString("color")) {
      c.cor = CorDoZen(*cor);
    }
    if (const std::string* icone = d.FindString("icon")) {
      c.icone = IconeDoZen(*icone);
    }
    if (c.id > 0 && !c.nome.empty()) {
      dados.containers.push_back(std::move(c));
    }
  }
}

void LerSessao(const base::FilePath& copia, DadosDoZen& dados) {
  std::string comprimido;
  if (!base::ReadFileToString(copia.Append("zen-sessions.jsonlz4"),
                              &comprimido)) {
    return;
  }
  std::optional<std::string> texto = DescomprimirMozLz4(comprimido);
  std::optional<base::Value> json =
      texto ? base::JSONReader::Read(*texto, base::JSON_PARSE_RFC)
            : std::nullopt;
  if (!json || !json->is_dict()) {
    return;
  }
  const base::DictValue& raiz = json->GetDict();
  if (const base::ListValue* espacos = raiz.FindList("spaces")) {
    for (const base::Value& item : *espacos) {
      if (!item.is_dict()) {
        continue;
      }
      const base::DictValue& d = item.GetDict();
      EspacoDoZen e;
      if (const std::string* uuid = d.FindString("uuid")) {
        e.uuid = *uuid;
      }
      if (const std::string* nome = d.FindString("name")) {
        e.nome = *nome;
      }
      // Emoji fica; ícone de arquivo (chrome://...) não serve aqui.
      if (const std::string* icone = d.FindString("icon");
          icone && !icone->empty() && icone->size() <= 16 &&
          icone->find("://") == std::string::npos) {
        e.icone = *icone;
      }
      e.container = d.FindInt("containerTabId").value_or(0);
      if (!e.uuid.empty() && !e.nome.empty()) {
        dados.espacos.push_back(std::move(e));
      }
    }
  }
  // Pastas: as de dentro (vista dividida) contam como a pasta de fora.
  std::map<std::string, const base::DictValue*> por_id;
  if (const base::ListValue* pastas = raiz.FindList("folders")) {
    for (const base::Value& item : *pastas) {
      if (item.is_dict()) {
        if (const std::string* id = item.GetDict().FindString("id")) {
          por_id[*id] = &item.GetDict();
        }
      }
    }
  }
  auto pasta_de_cima = [&](std::string id) {
    for (int volta = 0; volta < 8 && !id.empty(); ++volta) {
      auto it = por_id.find(id);
      if (it == por_id.end()) {
        return std::string();
      }
      if (!it->second->FindBool("splitViewGroup").value_or(false)) {
        return id;
      }
      const std::string* pai = it->second->FindString("parentId");
      id = pai ? *pai : std::string();
    }
    return std::string();
  };
  for (const auto& [id, d] : por_id) {
    if (d->FindBool("splitViewGroup").value_or(false)) {
      continue;
    }
    PastaDoZen p;
    p.id = id;
    const std::string* nome = d->FindString("name");
    p.nome = nome && !nome->empty() ? *nome : "Pasta";
    if (const std::string* espaco = d->FindString("workspaceId")) {
      p.espaco = *espaco;
    }
    // Pasta dentro de pasta (a de fora de uma vista dividida conta).
    if (const std::string* pai = d->FindString("parentId")) {
      p.pai = pasta_de_cima(*pai);
    }
    dados.pastas.push_back(std::move(p));
  }
  if (const base::ListValue* abas = raiz.FindList("tabs")) {
    for (const base::Value& item : *abas) {
      if (!item.is_dict()) {
        continue;
      }
      const base::DictValue& d = item.GetDict();
      const bool essencial = d.FindBool("zenEssential").value_or(false);
      if (!d.FindBool("pinned").value_or(false) && !essencial) {
        continue;
      }
      FixadoDoZen f;
      f.essencial = essencial;
      f.container = d.FindInt("userContextId").value_or(0);
      if (const std::string* espaco = d.FindString("zenWorkspace")) {
        f.espaco = *espaco;
      }
      // O endereço fixado (o do pin), não o da página em que a aba parou.
      if (const base::DictValue* inicial =
              d.FindDictByDottedPath("_zenPinnedInitialState.entry")) {
        if (const std::string* url = inicial->FindString("url")) {
          f.url = *url;
        }
        if (const std::string* titulo = inicial->FindString("title")) {
          f.titulo = *titulo;
        }
      }
      if (f.url.empty()) {
        if (const base::ListValue* entradas = d.FindList("entries");
            entradas && !entradas->empty()) {
          const size_t atual = std::clamp<size_t>(
              d.FindInt("index").value_or(1), 1, entradas->size());
          const base::Value& entrada = (*entradas)[atual - 1];
          if (entrada.is_dict()) {
            if (const std::string* url = entrada.GetDict().FindString("url")) {
              f.url = *url;
            }
            if (const std::string* titulo =
                    entrada.GetDict().FindString("title")) {
              f.titulo = *titulo;
            }
          }
        }
      }
      if (const std::string* grupo = d.FindString("groupId")) {
        f.pasta = pasta_de_cima(*grupo);
      }
      if (GURL(f.url).SchemeIsHTTPOrHTTPS()) {
        dados.fixados.push_back(std::move(f));
      }
    }
  }
}

base::Time DeMicrossegundos(int64_t micro) {
  return base::Time::UnixEpoch() + base::Microseconds(micro);
}

void LerCookies(const base::FilePath& copia, DadosDoZen& dados) {
  sql::Database db(kTag);
  if (!db.Open(copia.Append("cookies.sqlite"))) {
    return;
  }
  sql::Statement s(db.GetUniqueStatement(
      "SELECT originAttributes, name, value, host, path, expiry, "
      "lastAccessed, creationTime, isSecure, isHttpOnly, sameSite "
      "FROM moz_cookies"));
  const base::Time agora = base::Time::Now();
  while (s.Step()) {
    const std::string atributos = s.ColumnString(0);
    // Particionados (cookies de terceiros isolados por site) e da janela
    // anônima ficam de fora: o login dos sites é o cookie primário.
    if (atributos.find("partitionKey=") != std::string::npos ||
        atributos.find("privateBrowsingId=1") != std::string::npos ||
        atributos.find("firstPartyDomain=") != std::string::npos) {
      continue;
    }
    int container = 0;
    if (const size_t pos = atributos.find("userContextId=");
        pos != std::string::npos) {
      container = std::atoi(atributos.c_str() + pos + 14);
    }
    std::string host = s.ColumnString(3);
    std::string caminho = s.ColumnString(4);
    if (host.empty()) {
      continue;
    }
    if (caminho.empty()) {
      caminho = "/";
    }
    // expiry: segundos (Firefox antigo) ou milissegundos (o de hoje).
    const int64_t expira_bruto = s.ColumnInt64(5);
    const base::Time expira =
        expira_bruto > 100000000000LL
            ? base::Time::FromMillisecondsSinceUnixEpoch(expira_bruto)
            : base::Time::FromSecondsSinceUnixEpoch(expira_bruto);
    if (expira <= agora) {
      continue;
    }
    const bool seguro = s.ColumnInt(8) != 0;
    net::CookieSameSite same_site = net::CookieSameSite::UNSPECIFIED;
    switch (s.ColumnInt(10)) {
      case 0:
        same_site = seguro ? net::CookieSameSite::NO_RESTRICTION
                           : net::CookieSameSite::UNSPECIFIED;
        break;
      case 1:
        same_site = net::CookieSameSite::LAX_MODE;
        break;
      case 2:
        same_site = net::CookieSameSite::STRICT_MODE;
        break;
    }
    const base::Time criado = DeMicrossegundos(s.ColumnInt64(7));
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::FromStorage(
            s.ColumnString(1), s.ColumnString(2), host, caminho, criado,
            expira, DeMicrossegundos(s.ColumnInt64(6)), criado, seguro,
            s.ColumnInt(9) != 0, same_site, net::COOKIE_PRIORITY_DEFAULT,
            std::nullopt,
            seguro ? net::CookieSourceScheme::kSecure
                   : net::CookieSourceScheme::kNonSecure,
            url::PORT_UNSPECIFIED, net::CookieSourceType::kOther,
            net::CanonicalCookieFromStorageCallSite::kCookieManager);
    if (!cookie) {
      continue;
    }
    std::string_view dominio = host;
    if (dominio.starts_with(".")) {
      dominio.remove_prefix(1);
    }
    GURL url(std::string(seguro ? "https://" : "http://") +
             std::string(dominio) + caminho);
    if (!url.is_valid()) {
      continue;
    }
    dados.cookies.push_back({container, std::move(cookie), std::move(url)});
  }
}

std::unique_ptr<DadosDoZen> Ler() {
  auto dados = std::make_unique<DadosDoZen>();
  std::optional<base::FilePath> perfil = AcharPerfil();
  if (!perfil) {
    dados->erro = u"O Zen não foi encontrado neste computador.";
    return dados;
  }
  if (!base::CreateNewTempDirectory("navegador-zen", &dados->copia)) {
    dados->erro = u"Não deu pra criar a pasta temporária.";
    return dados;
  }
  for (const char* nome : kArquivos) {
    const base::FilePath origem = perfil->Append(nome);
    if (base::PathExists(origem)) {
      base::CopyFile(origem, dados->copia.Append(nome));
    }
  }
  LerContainers(dados->copia, *dados);
  LerSessao(dados->copia, *dados);
  LerCookies(dados->copia, *dados);
  return dados;
}

// --- Aplicar no perfil (UI) --------------------------------------------------

bool MesmoNome(const std::string& a, const std::string& b) {
  return base::EqualsCaseInsensitiveASCII(base::TrimWhitespaceASCII(
                                              a, base::TRIM_ALL),
                                          base::TrimWhitespaceASCII(
                                              b, base::TRIM_ALL));
}

// Contagem dos cookies aceitos (as respostas chegam uma a uma).
struct Contagem : base::RefCounted<Contagem> {
  int aceitos = 0;

 private:
  friend class base::RefCounted<Contagem>;
  ~Contagem() = default;
};

// Fim do importador do Chromium (senhas, favoritos, histórico).
class Observador : public importer::ImporterProgressObserver {
 public:
  Observador(base::FilePath copia, base::OnceClosure pronto)
      : copia_(std::move(copia)), pronto_(std::move(pronto)) {}

  void ImportStarted() override {}
  void ImportItemStarted(user_data_importer::ImportItem item) override {}
  void ImportItemEnded(user_data_importer::ImportItem item) override {}
  void ImportEnded() override {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                       copia_));
    std::move(pronto_).Run();
    delete this;
  }

 private:
  base::FilePath copia_;
  base::OnceClosure pronto_;
};

void Aplicar(Profile* perfil,
             base::RepeatingCallback<void(const std::u16string&)> andamento,
             base::OnceCallback<void(const Resumo&)> pronto,
             std::unique_ptr<DadosDoZen> dados) {
  Resumo resumo;
  if (!dados->erro.empty()) {
    resumo.erro = dados->erro;
    std::move(pronto).Run(resumo);
    return;
  }

  // Containers: os do Zen viram os daqui (pelo nome), ou novos.
  std::map<int, std::string> container_daqui;
  for (const ContainerDoZen& c : dados->containers) {
    std::optional<std::string> achado;
    for (const containers::Container& daqui : containers::GetAll(perfil)) {
      if (MesmoNome(daqui.name, c.nome)) {
        achado = daqui.id;
      }
    }
    if (!achado) {
      achado = containers::Add(perfil, c.nome, c.cor, c.icone).id;
      ++resumo.containers;
    }
    container_daqui[c.id] = *achado;
  }

  // Espaços: os workspaces do Zen, com o container deles.
  std::map<std::string, std::string> espaco_daqui;
  for (const EspacoDoZen& e : dados->espacos) {
    const std::string container =
        e.container && container_daqui.count(e.container)
            ? container_daqui[e.container]
            : std::string();
    std::optional<espacos::Espaco> achado;
    for (const espacos::Espaco& daqui : espacos::Todos(perfil)) {
      if (MesmoNome(daqui.nome, e.nome)) {
        achado = daqui;
      }
    }
    if (achado) {
      if (achado->container.empty() && !container.empty()) {
        achado->container = container;
        espacos::Atualizar(perfil, *achado);
      }
    } else {
      std::string cor = espacos::ProximaCor(perfil);
      for (const ContainerDoZen& c : dados->containers) {
        if (c.id == e.container && !c.cor.empty()) {
          cor = c.cor;
        }
      }
      achado = espacos::Criar(perfil, e.nome, e.icone, cor, container);
      ++resumo.espacos;
    }
    espaco_daqui[e.uuid] = achado->id;
  }

  // Fixados e pastas (sem repetir o que já está fixado no espaço). As de
  // dentro de outras depois da de fora (umas voltas, no máximo uma por
  // nível).
  std::map<std::string, std::string> pasta_daqui;
  for (int volta = 0; volta <= fixos::kNiveisDePasta; ++volta) {
    for (const PastaDoZen& p : dados->pastas) {
      auto it = espaco_daqui.find(p.espaco);
      if (it == espaco_daqui.end() || pasta_daqui.count(p.id) ||
          (!p.pai.empty() && !pasta_daqui.count(p.pai) &&
           volta < fixos::kNiveisDePasta)) {
        continue;
      }
      // Sem a de fora (funda demais, ou de outro espaço): fica solta.
      const std::string dentro =
          pasta_daqui.count(p.pai) ? pasta_daqui[p.pai] : std::string();
      const std::vector<fixos::Entrada> todas = fixos::Do(perfil, it->second);
      const fixos::Entrada* de_fora =
          dentro.empty() ? nullptr : fixos::PastaPorId(todas, dentro);
      std::string id;
      for (const fixos::Entrada& entrada : de_fora ? de_fora->itens : todas) {
        if (entrada.pasta && MesmoNome(entrada.nome, p.nome)) {
          id = entrada.id;
        }
      }
      if (id.empty()) {
        id = fixos::CriarPasta(perfil, it->second, p.nome, dentro);
      }
      if (id.empty()) {
        id = fixos::CriarPasta(perfil, it->second, p.nome);
      }
      pasta_daqui[p.id] = id;
    }
  }
  for (const FixadoDoZen& f : dados->fixados) {
    std::string espaco;
    if (auto it = espaco_daqui.find(f.espaco); it != espaco_daqui.end()) {
      espaco = it->second;
    } else if (f.essencial) {
      // Essenciais do Zen são do container: vão pro espaço dele.
      const std::string container = container_daqui.count(f.container)
                                        ? container_daqui[f.container]
                                        : std::string();
      for (const espacos::Espaco& e : espacos::Todos(perfil)) {
        if (espaco.empty() && e.container == container) {
          espaco = e.id;
        }
      }
    }
    if (espaco.empty()) {
      continue;
    }
    std::set<std::string> ja;
    for (const fixos::Entrada& entrada : fixos::Do(perfil, espaco)) {
      if (!entrada.pasta) {
        ja.insert(entrada.site.url.spec());
      }
      if (entrada.pasta) {
        for (const fixos::Site& site : fixos::SitesDaPasta(entrada)) {
          ja.insert(site.url.spec());
        }
      }
    }
    for (const fixos::Site& site : fixos::Favoritos(perfil, espaco)) {
      ja.insert(site.url.spec());
    }
    const GURL url(f.url);
    if (ja.count(url.spec())) {
      continue;
    }
    if (f.essencial) {
      fixos::Favoritar(perfil, espaco, url, f.titulo);
    } else {
      const std::string pasta =
          pasta_daqui.count(f.pasta) ? pasta_daqui[f.pasta] : std::string();
      fixos::Fixar(perfil, espaco, url, f.titulo, pasta);
    }
    ++resumo.fixados;
  }

  // Cookies: cada um na partição do container dele.
  andamento.Run(u"Passando os cookies (logins dos sites)…");
  auto contagem = base::MakeRefCounted<Contagem>();
  const base::FilePath copia = dados->copia;
  base::OnceClosure senhas = base::BindOnce(
      [](Profile* perfil, base::FilePath copia,
         base::RepeatingCallback<void(const std::u16string&)> andamento,
         base::OnceCallback<void(const Resumo&)> pronto, Resumo resumo,
         scoped_refptr<Contagem> contagem) {
        resumo.cookies = contagem->aceitos;
        andamento.Run(u"Passando senhas, favoritos e histórico…");
        user_data_importer::SourceProfile fonte;
        fonte.importer_name = u"Zen";
        fonte.importer_type = user_data_importer::TYPE_FIREFOX;
        fonte.source_path = copia;
        fonte.app_path = copia;
        fonte.locale = g_browser_process->GetApplicationLocale();
        const uint16_t itens =
            user_data_importer::HISTORY | user_data_importer::FAVORITES |
            user_data_importer::PASSWORDS |
            user_data_importer::AUTOFILL_FORM_DATA;
        fonte.services_supported = itens;
        resumo.ok = true;
        auto* host = new ExternalProcessImporterHost;  // apaga a si mesmo
        host->set_headless();
        host->set_observer(new Observador(
            copia, base::BindOnce(std::move(pronto), resumo)));
        host->StartImportSettings(fonte, perfil, itens,
                                  new ProfileWriter(perfil));
      },
      perfil, copia, andamento, std::move(pronto), resumo, contagem);
  base::RepeatingClosure barreira =
      base::BarrierClosure(dados->cookies.size(), std::move(senhas));
  for (CookieDoZen& c : dados->cookies) {
    content::StoragePartition* particao = nullptr;
    if (c.container == 0) {
      particao = perfil->GetDefaultStoragePartition();
    } else if (container_daqui.count(c.container)) {
      particao = perfil->GetStoragePartition(
          containers::PartitionConfig(perfil, container_daqui[c.container]),
          /*can_create=*/true);
    }
    if (!particao) {
      barreira.Run();
      continue;
    }
    particao->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
        *c.cookie, c.url, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(
            [](scoped_refptr<Contagem> contagem, base::RepeatingClosure barreira,
               net::CookieAccessResult resultado) {
              if (resultado.status.IsInclude()) {
                ++contagem->aceitos;
              }
              barreira.Run();
            },
            contagem, barreira));
  }
}

}  // namespace

void ProcurarZen(base::OnceCallback<void(bool achou)> resposta) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() { return AcharPerfil().has_value(); }),
      std::move(resposta));
}

void Importar(Profile* perfil,
              base::RepeatingCallback<void(const std::u16string&)> andamento,
              base::OnceCallback<void(const Resumo&)> pronto) {
  andamento.Run(u"Copiando o perfil do Zen…");
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&Ler),
      base::BindOnce(&Aplicar, perfil, andamento, std::move(pronto)));
}

}  // namespace navegador::importar_zen
