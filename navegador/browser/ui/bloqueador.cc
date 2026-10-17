#include "navegador/browser/ui/bloqueador.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/ruleset_version.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace navegador::bloqueador {

namespace {

// --- Regras de esconder elementos (cosmetico.txt) ----------------------------

// Seletores por bloco de CSS: um seletor que o Chromium não entende derruba o
// bloco inteiro; num bloco por seletor, o CSS fica ~60% mais lento de aplicar.
constexpr size_t kSeletoresPorBloco = 50;

struct RegraDeSite {
  std::vector<std::string> fora;  // "~dominio": não vale nesses.
  std::string seletor;
};

// Scriptlet "set" do uBlock: `caminho` (ex.: "UOLPD.dataLayer") passa a valer
// `valor`, que a página não consegue trocar.
struct Constante {
  std::vector<std::string> fora;
  std::string caminho;
  std::string valor;
};

struct Cosmetico {
  std::vector<std::string> genericos;
  std::string genericos_css;  // Os genéricos já em CSS (o caso comum).
  // Genéricos que não valem em alguns sites ("~a.com##.x").
  std::vector<RegraDeSite> genericos_com_excecao;
  std::map<std::string, std::vector<RegraDeSite>, std::less<>> por_site;
  std::map<std::string, std::set<std::string>, std::less<>> excecoes;
  std::map<std::string, std::vector<Constante>, std::less<>> constantes;
};

std::unique_ptr<Cosmetico>& Regras() {
  static base::NoDestructor<std::unique_ptr<Cosmetico>> regras;
  return *regras;
}

void Juntar(std::string& css, const std::vector<std::string_view>& seletores) {
  for (size_t i = 0; i < seletores.size(); i += kSeletoresPorBloco) {
    const size_t fim = std::min(seletores.size(), i + kSeletoresPorBloco);
    for (size_t j = i; j < fim; ++j) {
      if (j > i) {
        css += ',';
      }
      css += seletores[j];
    }
    css += "{display:none!important}\n";
  }
}

std::optional<std::string_view> ValorConhecido(std::string_view valor) {
  static constexpr std::pair<std::string_view, std::string_view> kValores[] = {
      {"undefined", "undefined"},
      {"false", "false"},
      {"true", "true"},
      {"null", "null"},
      {"noopFunc", "function() {}"},
      {"trueFunc", "function() { return true; }"},
      {"falseFunc", "function() { return false; }"},
      {"emptyObj", "{}"},
      {"{}", "{}"},
      {"emptyArr", "[]"},
      {"[]", "[]"},
      {"''", "''"},
      {"emptyStr", "''"},
      {"yes", "'yes'"},
      {"no", "'no'"},
  };
  for (const auto& [nome, js] : kValores) {
    if (nome == valor) {
      return js;
    }
  }
  return std::nullopt;
}

std::string_view ValorEmJs(std::string_view valor) {
  return ValorConhecido(valor).value_or(valor);  // Número (ValorValido).
}

bool CaminhoValido(std::string_view caminho) {
  return !caminho.empty() && std::ranges::all_of(caminho, [](char c) {
    return base::IsAsciiAlphaNumeric(c) || c == '_' || c == '$' || c == '.';
  });
}

bool ValorValido(std::string_view valor) {
  if (ValorConhecido(valor)) {
    return true;
  }
  if (valor.starts_with('-')) {
    valor.remove_prefix(1);
  }
  return !valor.empty() && valor.size() <= 16 &&
         std::ranges::all_of(valor, [](char c) {
           return base::IsAsciiDigit(c) || c == '.';
         });
}

std::unique_ptr<Cosmetico> LerCosmetico(const base::FilePath& caminho) {
  std::string texto;
  if (!base::ReadFileToString(caminho, &texto)) {
    return nullptr;
  }
  auto regras = std::make_unique<Cosmetico>();
  for (std::string_view linha : base::SplitStringPiece(
           texto, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> campos = base::SplitStringPiece(
        linha, "\t", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (campos.size() == 2 && campos[0] == "G") {
      regras->genericos.emplace_back(campos[1]);
      continue;
    }
    if (campos.size() == 4 && campos[0] == "J") {
      // Vão pro JavaScript da página: só caminho de variável e valor
      // conhecido ou número (o tools/cosmetico.py já filtra; aqui é a
      // garantia, caso o arquivo venha de outro lugar).
      if (!CaminhoValido(campos[2]) || !ValorValido(campos[3])) {
        continue;
      }
      Constante constante{.caminho = std::string(campos[2]),
                          .valor = std::string(campos[3])};
      std::vector<std::string> dentro;
      for (std::string_view dominio :
           base::SplitStringPiece(campos[1], ",", base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY)) {
        if (dominio.starts_with('~')) {
          constante.fora.emplace_back(dominio.substr(1));
        } else {
          dentro.emplace_back(dominio);
        }
      }
      for (const std::string& dominio : dentro) {
        regras->constantes[dominio].push_back(constante);
      }
      continue;
    }
    if (campos.size() != 3) {
      continue;
    }
    std::vector<std::string> dentro;
    RegraDeSite regra{.seletor = std::string(campos[2])};
    for (std::string_view dominio : base::SplitStringPiece(
             campos[1], ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      if (dominio.starts_with('~')) {
        regra.fora.emplace_back(dominio.substr(1));
      } else {
        dentro.emplace_back(dominio);
      }
    }
    if (campos[0] == "E") {
      for (const std::string& dominio : dentro) {
        regras->excecoes[dominio].insert(regra.seletor);
      }
    } else if (campos[0] == "S") {
      if (dentro.empty()) {
        regras->genericos_com_excecao.push_back(std::move(regra));
      } else {
        for (const std::string& dominio : dentro) {
          regras->por_site[dominio].push_back(regra);
        }
      }
    }
  }
  std::vector<std::string_view> todos(regras->genericos.begin(),
                                      regras->genericos.end());
  Juntar(regras->genericos_css, todos);
  return regras;
}

// "a.b.exemplo.com" → a.b.exemplo.com, b.exemplo.com, exemplo.com, com: uma
// regra de "exemplo.com" vale nos subdomínios.
std::vector<std::string_view> Sufixos(std::string_view host) {
  std::vector<std::string_view> sufixos;
  while (!host.empty()) {
    sufixos.push_back(host);
    const size_t ponto = host.find('.');
    if (ponto == std::string_view::npos) {
      break;
    }
    host.remove_prefix(ponto + 1);
  }
  return sufixos;
}

bool Algum(const std::vector<std::string>& dominios,
           const std::vector<std::string_view>& sufixos) {
  for (const std::string& dominio : dominios) {
    for (std::string_view sufixo : sufixos) {
      if (dominio == sufixo) {
        return true;
      }
    }
  }
  return false;
}

std::string CssDoSite(const Cosmetico& regras, std::string_view host) {
  const std::vector<std::string_view> sufixos = Sufixos(host);
  std::set<std::string_view> excecoes;
  for (std::string_view sufixo : sufixos) {
    auto it = regras.excecoes.find(sufixo);
    if (it != regras.excecoes.end()) {
      excecoes.insert(it->second.begin(), it->second.end());
    }
  }

  std::string css;
  bool generico_com_excecao = false;
  if (!excecoes.empty()) {
    for (const std::string& seletor : regras.genericos) {
      if (excecoes.contains(seletor)) {
        generico_com_excecao = true;
        break;
      }
    }
  }
  if (generico_com_excecao) {
    // Raro (umas centenas de sites): refaz os genéricos sem as exceções.
    std::vector<std::string_view> valem;
    for (const std::string& seletor : regras.genericos) {
      if (!excecoes.contains(seletor)) {
        valem.push_back(seletor);
      }
    }
    Juntar(css, valem);
  } else {
    css = regras.genericos_css;
  }

  std::vector<std::string_view> deste_site;
  for (const RegraDeSite& regra : regras.genericos_com_excecao) {
    if (!Algum(regra.fora, sufixos) && !excecoes.contains(regra.seletor)) {
      deste_site.push_back(regra.seletor);
    }
  }
  for (std::string_view sufixo : sufixos) {
    auto it = regras.por_site.find(sufixo);
    if (it == regras.por_site.end()) {
      continue;
    }
    for (const RegraDeSite& regra : it->second) {
      if (!Algum(regra.fora, sufixos) && !excecoes.contains(regra.seletor)) {
        deste_site.push_back(regra.seletor);
      }
    }
  }
  // Um por bloco: as regras de site são poucas e às vezes usam seletores
  // que outros navegadores aceitam e o Chromium não.
  for (std::string_view seletor : deste_site) {
    css += seletor;
    css += "{display:none!important}\n";
  }
  return css;
}

// Os valores do "set" em JS (tools/cosmetico.py só deixa passar estes e
// números).

// Roda no começo do documento, antes dos scripts da página. Pra "a.b.c": se
// "a" ou "a.b" ainda não existem, fica esperando a página criar (um setter
// no lugar) e fixa o resto quando ela cria.
constexpr char kFixarConstantes[] = R"(
(() => {
  const fixar = (dono, partes, valor) => {
    const nome = partes[0];
    if (partes.length === 1) {
      try {
        Object.defineProperty(dono, nome, {
          configurable: true, get: () => valor, set: () => {},
        });
      } catch (e) {}
      return;
    }
    const atual = dono[nome];
    if (atual !== null && (typeof atual === 'object' ||
                           typeof atual === 'function')) {
      fixar(atual, partes.slice(1), valor);
      return;
    }
    let guardado = atual;
    try {
      Object.defineProperty(dono, nome, {
        configurable: true,
        get: () => guardado,
        set: (novo) => {
          guardado = novo;
          if (novo !== null && (typeof novo === 'object' ||
                                typeof novo === 'function')) {
            fixar(novo, partes.slice(1), valor);
          }
        },
      });
    } catch (e) {}
  };
)";

std::string JsDoSite(const Cosmetico& regras, std::string_view host) {
  const std::vector<std::string_view> sufixos = Sufixos(host);
  std::string js;
  for (std::string_view sufixo : sufixos) {
    auto it = regras.constantes.find(sufixo);
    if (it == regras.constantes.end()) {
      continue;
    }
    for (const Constante& constante : it->second) {
      if (Algum(constante.fora, sufixos)) {
        continue;
      }
      js += "  fixar(window, '";
      js += constante.caminho;  // Só [\w$.] (tools/cosmetico.py).
      js += "'.split('.'), ";
      js += ValorEmJs(constante.valor);
      js += ");\n";
    }
  }
  if (js.empty()) {
    return js;
  }
  return std::string(kFixarConstantes) + js + "})();\n";
}

// Esconde os elementos de anúncio da página principal de cada aba, logo que
// ela começa: o filtro de rede barra o anúncio, mas o espaço dele (às vezes
// com animação de "carregando" que nunca termina) ficava na página. E fixa as
// variáveis que os scripts barrados criariam ("set" do uBlock), senão partes
// do site esperam por elas pra sempre (a barra de cotações do UOL).
class Escondedor : public content::WebContentsObserver,
                   public content::WebContentsUserData<Escondedor> {
 public:
  ~Escondedor() override { Todos().erase(this); }

  // content::WebContentsObserver:
  // Manda as regras antes do documento novo começar: o quadro guarda e aplica
  // no começo dele, antes dos scripts da página (como o "document_start" das
  // extensões). Também nas páginas pré-carregadas (ainda fora da vista).
  void ReadyToCommitNavigation(content::NavigationHandle* navegacao) override {
    if (!navegacao->IsInMainFrame() || navegacao->IsSameDocument() ||
        navegacao->IsErrorPage() || navegacao->IsServedFromBackForwardCache() ||
        navegacao->IsPrerenderedPageActivation()) {
      return;
    }
    Mandar(navegacao->GetRenderFrameHost(), navegacao->GetURL());
  }

  // As regras terminaram de carregar (na abertura do navegador, depois das
  // abas restauradas ou abertas pela linha de comando): aplica nas páginas
  // que já estão abertas (o quadro aplica na hora, se ainda não tem).
  void RegrasCarregadas() {
    content::RenderFrameHost* quadro = web_contents()->GetPrimaryMainFrame();
    if (quadro->IsRenderFrameLive()) {
      Mandar(quadro, quadro->GetLastCommittedURL());
    }
  }

  static std::set<Escondedor*>& Todos() {
    static base::NoDestructor<std::set<Escondedor*>> todos;
    return *todos;
  }

 private:
  friend class content::WebContentsUserData<Escondedor>;
  explicit Escondedor(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<Escondedor>(*contents) {
    Todos().insert(this);
  }

  void Mandar(content::RenderFrameHost* destino, const GURL& url) {
    if (!Regras() || !url.SchemeIsHTTPOrHTTPS() || !url.has_host()) {
      return;
    }
    // Liberado no site (ou desligado no perfil): "Anúncios" permitidos.
    HostContentSettingsMap* configuracoes =
        HostContentSettingsMapFactory::GetForProfile(
            web_contents()->GetBrowserContext());
    if (configuracoes->GetContentSetting(url, url, ContentSettingsType::ADS) ==
        CONTENT_SETTING_ALLOW) {
      return;
    }
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> quadro;
    destino->GetRemoteAssociatedInterfaces()->GetInterface(&quadro);
    quadro->NavegadorEsconderAnuncios(CssDoSite(*Regras(), url.host()),
                                      JsDoSite(*Regras(), url.host()), url);
  }

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(Escondedor);

// --- Carregar ----------------------------------------------------------------

struct Pasta {
  std::optional<subresource_filter::UnindexedRulesetInfo> rede;
  std::unique_ptr<Cosmetico> cosmetico;
};

// Pasta das regras, ao lado do módulo: o pacote do Linux instala em
// /usr/lib/navegador/bloqueador, o instalador do Windows na pasta da versão
// (ao lado do chrome.dll); numa compilação de desenvolvimento, um link pra
// pacote/bloqueador no out/.
Pasta LerPasta() {
  Pasta lida;
  base::FilePath exe;
  if (!base::PathService::Get(base::DIR_MODULE, &exe)) {
    return lida;
  }
  const base::FilePath pasta = exe.AppendASCII("bloqueador");
  lida.cosmetico = LerCosmetico(pasta.AppendASCII("cosmetico.txt"));
  std::string versao;
  if (!base::ReadFileToString(pasta.AppendASCII("VERSAO"), &versao)) {
    return lida;
  }
  versao = std::string(base::TrimWhitespaceASCII(versao, base::TRIM_ALL));
  const base::FilePath regras = pasta.AppendASCII("Filtering Rules");
  if (versao.empty() || !base::PathExists(regras)) {
    return lida;
  }
  subresource_filter::UnindexedRulesetInfo info;
  info.content_version = versao;
  info.ruleset_path = regras;
  info.license_path = pasta.AppendASCII("LICENSE");
  lida.rede = info;
  return lida;
}

void Publicar(Pasta lida) {
  Regras() = std::move(lida.cosmetico);
  if (Regras()) {
    for (Escondedor* escondedor : Escondedor::Todos()) {
      escondedor->RegrasCarregadas();
    }
  }
  if (!lida.rede) {
    return;
  }
  // O próprio serviço só indexa depois da partida, em segundo plano, e só
  // quando a versão muda; até lá vale a última indexada.
  if (subresource_filter::RulesetService* servico =
          g_browser_process->subresource_filter_ruleset_service()) {
    servico->IndexAndStoreAndPublishRulesetIfNeeded(*lida.rede);
  }
}

}  // namespace

void CarregarListas() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&LerPasta), base::BindOnce(&Publicar));
}

void AcompanharAba(content::WebContents* contents) {
  Escondedor::CreateForWebContents(contents);
}

bool Ligado(Profile* perfil) {
  return HostContentSettingsMapFactory::GetForProfile(perfil)
             ->GetDefaultContentSetting(ContentSettingsType::ADS) !=
         CONTENT_SETTING_ALLOW;
}

void Ligar(Profile* perfil, bool ligado) {
  HostContentSettingsMapFactory::GetForProfile(perfil)
      ->SetDefaultContentSetting(
          ContentSettingsType::ADS,
          ligado ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
}

}  // namespace navegador::bloqueador
