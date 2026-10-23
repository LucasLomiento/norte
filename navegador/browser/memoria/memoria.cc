#include "navegador/browser/memoria/memoria.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/ui/links.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace navegador::memoria {

namespace {

// À vista, com o carregamento terminado, por esse tempo: a página entra.
constexpr base::TimeDelta kLeitura = base::Seconds(3);
// Saindo da aba esse tempo depois da última leitura, relê (o que carregou
// depois: comentários, o resto do feed).
constexpr base::TimeDelta kReler = base::Seconds(20);
// Texto por página, no máximo (um artigo longo tem umas 60 mil letras).
constexpr size_t kLimiteDoTexto = 120000;
constexpr size_t kLimiteDoTitulo = 400;
// Menos que isso de texto, e sem título, não vale guardar.
constexpr size_t kMinimoDoTexto = 40;
// Miniatura do alto da página, 16:10; uma nova a cada tanto.
constexpr int kLarguraDaMiniatura = 480;
constexpr int kAlturaDaMiniatura = 300;
constexpr base::TimeDelta kIdadeDaMiniatura = base::Hours(12);
// As miniaturas dos últimos 60 dias, no máximo 4 mil.
constexpr base::TimeDelta kPrazoDasMiniaturas = base::Days(60);
constexpr size_t kMaximoDeMiniaturas = 4000;
// Poda (o prazo da pessoa, as miniaturas): pouco depois de abrir e a cada
// 12 h.
constexpr base::TimeDelta kPrimeiraPoda = base::Minutes(2);
constexpr base::TimeDelta kIntervaloDaPoda = base::Hours(12);

// E-mail e conversas: de fora, a não ser que a pessoa ligue
// (kMemoriaConversasPref). O host (e os subdomínios) e, quando tem, o
// começo do caminho.
struct Conversa {
  std::string_view host;
  std::string_view caminho;
};
constexpr Conversa kConversas[] = {
    {"mail.google.com", ""},     {"chat.google.com", ""},
    {"outlook.live.com", ""},    {"outlook.office.com", ""},
    {"outlook.office365.com", ""}, {"mail.yahoo.com", ""},
    {"mail.proton.me", ""},      {"app.fastmail.com", ""},
    {"web.whatsapp.com", ""},    {"web.telegram.org", ""},
    {"app.slack.com", ""},       {"discord.com", "/channels"},
    {"messenger.com", ""},       {"teams.microsoft.com", ""},
    {"teams.live.com", ""},      {"instagram.com", "/direct"},
    {"x.com", "/messages"},      {"twitter.com", "/messages"},
    {"linkedin.com", "/messaging"}, {"facebook.com", "/messages"},
    {"icloud.com", "/mail"},
};

// Roda no mundo isolado do Norte (a página não vê nem atrapalha): o título e
// o texto que está na tela (innerText: o que a pessoa lê, sem o que está
// escondido), ou só o aviso de que tem campo de senha à vista.
constexpr char16_t kLer[] = uR"((() => {
  const d = document;
  if (!d.body) return null;
  for (const campo of d.querySelectorAll('input[type=password]')) {
    if (campo.getClientRects().length) return {senha: true};
  }
  const texto = d.body.innerText || '';
  return {
    titulo: d.title || '',
    texto: texto.length > 200000 ? texto.slice(0, 200000) : texto,
  };
})())";

bool NoDominio(std::string_view host, std::string_view dominio) {
  return host == dominio ||
         (host.size() > dominio.size() && host.ends_with(dominio) &&
          host[host.size() - dominio.size() - 1] == '.');
}

bool EConversa(const GURL& url) {
  for (const Conversa& conversa : kConversas) {
    if (NoDominio(url.host(), conversa.host) &&
        url.path().starts_with(conversa.caminho)) {
      return true;
    }
  }
  return false;
}

// "WWW.Exemplo.com/x " → "exemplo.com".
std::string HostDigitado(std::string_view texto) {
  std::string host = base::ToLowerASCII(
      base::TrimWhitespaceASCII(texto, base::TRIM_ALL));
  if (const size_t esquema = host.find("://"); esquema != std::string::npos) {
    host = host.substr(esquema + 3);
  }
  host = host.substr(0, host.find_first_of("/?#:"));
  if (host.starts_with("www.")) {
    host = host.substr(4);
  }
  return host;
}

PrefService* Prefs(Profile* perfil) {
  return perfil->GetOriginalProfile()->GetPrefs();
}

// --- Serviço (um por perfil) -------------------------------------------------

class Servico;

std::map<Profile*, std::unique_ptr<Servico>>& Servicos() {
  static base::NoDestructor<std::map<Profile*, std::unique_ptr<Servico>>>
      servicos;
  return *servicos;
}

class Servico : public history::HistoryServiceObserver, public ProfileObserver {
 public:
  explicit Servico(Profile* perfil)
      : perfil_(perfil),
        banco_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                    base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
               perfil->GetPath().AppendASCII("Norte Memoria")) {
    banco_.AsyncCall(base::IgnoreResult(&Banco::Abrir));
    if (history::HistoryService* historico =
            HistoryServiceFactory::GetForProfile(
                perfil, ServiceAccessType::EXPLICIT_ACCESS)) {
      historico_.Observe(historico);
    }
    perfil_observado_.Observe(perfil);
    // Container excluído: as páginas dele saem junto.
    for (const containers::Container& container :
         containers::GetAll(perfil)) {
      containers_.insert(container.id);
    }
    prefs_.Init(perfil->GetPrefs());
    prefs_.Add(containers::kContainersPref,
               base::BindRepeating(&Servico::ContainersMudaram,
                                   base::Unretained(this)));
    primeira_poda_.Start(FROM_HERE, kPrimeiraPoda, this, &Servico::Podar);
    poda_.Start(FROM_HERE, kIntervaloDaPoda, this, &Servico::Podar);
  }

  ~Servico() override = default;

  base::SequenceBound<Banco>& banco() { return banco_; }

  // history::HistoryServiceObserver: apagou do histórico, apaga daqui.
  void OnHistoryDeletions(history::HistoryService* historico,
                          const history::DeletionInfo& apagado) override {
    // O histórico vence sozinho em 90 dias; a memória tem o prazo dela.
    if (apagado.is_from_expiration()) {
      return;
    }
    if (apagado.IsAllHistory()) {
      banco_.AsyncCall(&Banco::EsquecerTudo);
      return;
    }
    if (apagado.time_range().IsValid() && !apagado.restrict_urls()) {
      banco_.AsyncCall(&Banco::EsquecerPeriodo)
          .WithArgs(apagado.time_range().begin(), apagado.time_range().end());
    }
    std::vector<std::string> chaves;
    for (const history::URLRow& linha : apagado.deleted_rows()) {
      if (std::string chave = Chave(linha.url()); !chave.empty()) {
        chaves.push_back(std::move(chave));
      }
    }
    if (!chaves.empty()) {
      banco_.AsyncCall(&Banco::EsquecerEnderecos).WithArgs(std::move(chaves));
    }
  }

  void HistoryServiceBeingDeleted(history::HistoryService* historico) override {
    historico_.Reset();
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* perfil) override {
    // Apaga a si mesmo: nada de membro depois daqui.
    Servicos().erase(perfil);
  }

 private:
  void Podar() {
    const int dias = perfil_->GetPrefs()->GetInteger(espacos::kMemoriaDiasPref);
    const base::Time agora = base::Time::Now();
    banco_.AsyncCall(&Banco::Podar)
        .WithArgs(dias > 0 ? agora - base::Days(dias) : base::Time(),
                  agora - kPrazoDasMiniaturas, kMaximoDeMiniaturas);
  }

  void ContainersMudaram() {
    std::set<std::string> agora;
    for (const containers::Container& container :
         containers::GetAll(perfil_)) {
      agora.insert(container.id);
    }
    for (const std::string& id : containers_) {
      if (!agora.contains(id)) {
        banco_.AsyncCall(&Banco::EsquecerContainer).WithArgs(id);
      }
    }
    containers_ = std::move(agora);
  }

  raw_ptr<Profile> perfil_;
  base::SequenceBound<Banco> banco_;
  std::set<std::string> containers_;
  PrefChangeRegistrar prefs_;
  base::OneShotTimer primeira_poda_;
  base::RepeatingTimer poda_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      historico_{this};
  base::ScopedObservation<Profile, ProfileObserver> perfil_observado_{this};
};

// O serviço do perfil normal; nada pra janela anônima e de visitante.
Servico* Pegar(Profile* perfil) {
  if (!perfil || perfil->IsOffTheRecord() || perfil->IsGuestSession() ||
      browser_shutdown::HasShutdownStarted()) {
    return nullptr;
  }
  auto& servicos = Servicos();
  auto it = servicos.find(perfil);
  if (it == servicos.end()) {
    it = servicos.emplace(perfil, std::make_unique<Servico>(perfil)).first;
  }
  return it->second.get();
}

void GuardarMiniatura(base::WeakPtr<Profile> perfil,
                      std::string chave,
                      std::optional<std::vector<uint8_t>> jpeg) {
  Servico* servico = perfil ? Pegar(perfil.get()) : nullptr;
  if (!servico || !jpeg) {
    return;
  }
  servico->banco()
      .AsyncCall(&Banco::GuardarMiniatura)
      .WithArgs(std::move(chave), std::move(*jpeg), base::Time::Now());
}

// --- Leitura (uma por aba) ---------------------------------------------------

class Leitor : public content::WebContentsObserver,
               public content::WebContentsUserData<Leitor> {
 public:
  ~Leitor() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* navegacao) override {
    if (!navegacao->IsInPrimaryMainFrame() || !navegacao->HasCommitted()) {
      return;
    }
    const GURL& url = navegacao->GetURL();
    if (navegacao->IsSameDocument()) {
      // Âncora (#seção) na mesma página: nada muda.
      if (Chave(url) == chave_) {
        return;
      }
      // Página nova sem recarregar (pushState: YouTube, GitHub): o
      // documento é o mesmo, e o que valia pra ele (no-store) continua.
      Encerrar();
      Comecar(url, fora_do_documento_);
      carregou_ = true;
      Agendar();
      return;
    }
    Encerrar();
    // Sem os cabeçalhos (página de erro, a que voltou do cache de
    // vai-e-volta sem eles) não guarda: não dá pra saber se ela pediu.
    const net::HttpResponseHeaders* cabecalhos =
        navegacao->GetResponseHeaders();
    fora_do_documento_ =
        navegacao->IsErrorPage() || !cabecalhos ||
        cabecalhos->HasHeaderValue("cache-control", "no-store");
    Comecar(url, fora_do_documento_);
    // Do cache de vai-e-volta, já chega carregada.
    carregou_ = navegacao->IsServedFromBackForwardCache();
    Agendar();
  }

  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    carregou_ = true;
    Agendar();
  }

  void OnVisibilityChanged(content::Visibility visibilidade) override {
    const base::TimeTicks agora = base::TimeTicks::Now();
    if (visibilidade == content::Visibility::VISIBLE) {
      if (a_vista_desde_.is_null()) {
        a_vista_desde_ = agora;
      }
      Agendar();
      return;
    }
    relogio_.Stop();
    if (!a_vista_desde_.is_null()) {
      tempo_ += agora - a_vista_desde_;
      a_vista_desde_ = base::TimeTicks();
    }
    if (lida_ && agora - ultima_leitura_ >= kReler) {
      Ler(/*visita=*/false);
    }
    GuardarTempo();
  }

  void WebContentsDestroyed() override { Encerrar(); }

 private:
  friend class content::WebContentsUserData<Leitor>;

  explicit Leitor(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<Leitor>(*contents) {
    if (contents->GetVisibility() == content::Visibility::VISIBLE) {
      a_vista_desde_ = base::TimeTicks::Now();
    }
  }

  Profile* Perfil() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  void Comecar(const GURL& url, bool fora) {
    url_ = url;
    chave_ = Chave(url);
    fora_ = fora || chave_.empty() || net::IsLocalhost(url);
    carregou_ = false;
    lida_ = false;
    senha_ = false;
    ultima_leitura_ = base::TimeTicks();
    tempo_ = base::TimeDelta();
    if (!a_vista_desde_.is_null()) {
      a_vista_desde_ = base::TimeTicks::Now();
    }
    ++pagina_;
  }

  // A página atual acabou (outra navegação, a aba fechou): grava o tempo
  // dela.
  void Encerrar() {
    relogio_.Stop();
    if (!a_vista_desde_.is_null()) {
      const base::TimeTicks agora = base::TimeTicks::Now();
      tempo_ += agora - a_vista_desde_;
      a_vista_desde_ = agora;
    }
    GuardarTempo();
  }

  void GuardarTempo() {
    const int segundos = static_cast<int>(tempo_.InSeconds());
    if (!lida_ || segundos <= 0) {
      return;
    }
    tempo_ -= base::Seconds(segundos);
    if (Servico* servico = Pegar(Perfil())) {
      servico->banco().AsyncCall(&Banco::SomarTempo).WithArgs(chave_, segundos);
    }
  }

  void Agendar() {
    if (fora_ || senha_ || lida_ || !carregou_ ||
        web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
      return;
    }
    relogio_.Start(FROM_HERE, kLeitura,
                   base::BindOnce(&Leitor::Ler, base::Unretained(this),
                                  /*visita=*/true));
  }

  // As regras que a pessoa pode mudar a qualquer hora: vale o que está
  // valendo na hora de ler.
  bool Pode() {
    Profile* perfil = Perfil();
    if (!Ligada(perfil) || SiteDeFora(perfil, url_) ||
        (!Prefs(perfil)->GetBoolean(espacos::kMemoriaConversasPref) &&
         EConversa(url_))) {
      return false;
    }
    if (std::optional<std::string> container =
            containers::IdForWebContents(web_contents())) {
      std::optional<containers::Container> dados =
          containers::Get(perfil, *container);
      if (!dados || dados->temporario || ContainerDeFora(perfil, *container)) {
        return false;
      }
    }
    // A página de resultados de um buscador (o padrão ou qualquer outro da
    // lista) é a lista dos outros: não é leitura.
    if (TemplateURLService* buscadores =
            TemplateURLServiceFactory::GetForProfile(perfil)) {
      if (buscadores->IsSearchResultsPageFromDefaultSearchProvider(url_)) {
        return false;
      }
      for (const TemplateURL* buscador : buscadores->GetTemplateURLs()) {
        if (buscador->IsSearchURL(url_, buscadores->search_terms_data())) {
          return false;
        }
      }
    }
    // Só abas de janela comum (apps instalados e pop-ups ficam de fora).
    tabs::TabInterface* aba =
        tabs::TabInterface::MaybeGetFromContents(web_contents());
    BrowserWindowInterface* janela =
        aba ? aba->GetBrowserWindowInterface() : nullptr;
    return janela &&
           janela->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
  }

  void Ler(bool visita) {
    if (fora_ || senha_ || !Pode()) {
      return;
    }
    content::RenderFrameHost* quadro = web_contents()->GetPrimaryMainFrame();
    if (!quadro->IsRenderFrameLive()) {
      return;
    }
    quadro->ExecuteJavaScriptInIsolatedWorld(
        kLer,
        base::BindOnce(&Leitor::Leu, weak_factory_.GetWeakPtr(), pagina_,
                       visita),
        ISOLATED_WORLD_ID_NAVEGADOR);
  }

  void Leu(int pagina, bool visita, base::Value resultado) {
    if (pagina != pagina_ || !resultado.is_dict()) {
      return;
    }
    const base::DictValue& lido = resultado.GetDict();
    if (lido.FindBool("senha").value_or(false)) {
      senha_ = true;
      return;
    }
    const std::string* titulo = lido.FindString("titulo");
    const std::string* texto = lido.FindString("texto");
    if (!titulo || !texto) {
      return;
    }
    Leitura leitura;
    leitura.texto = texto::Limpar(base::UTF8ToUTF16(*texto), kLimiteDoTexto);
    leitura.titulo =
        texto::Limpar(base::UTF8ToUTF16(*titulo), kLimiteDoTitulo);
    base::ReplaceChars(leitura.titulo, u"\n", u" ", &leitura.titulo);
    if (leitura.texto.size() < kMinimoDoTexto && leitura.titulo.empty()) {
      return;
    }
    Profile* perfil = Perfil();
    Servico* servico = Pegar(perfil);
    if (!servico) {
      return;
    }
    leitura.url = chave_;
    leitura.host = url_.host();
    if (leitura.host.starts_with("www.")) {
      leitura.host = leitura.host.substr(4);
    }
    leitura.container =
        containers::IdForWebContents(web_contents()).value_or(std::string());
    if (tabs::TabInterface* aba =
            tabs::TabInterface::MaybeGetFromContents(web_contents())) {
      if (std::optional<tab_groups::TabGroupId> grupo = aba->GetGroup()) {
        leitura.espaco = espacos::EspacoDoGrupo(perfil, grupo->ToString())
                             .value_or(std::string());
      }
    }
    leitura.quando = base::Time::Now();
    leitura.visita = visita;
    servico->banco().AsyncCall(&Banco::Lembrar).WithArgs(std::move(leitura));
    lida_ = true;
    ultima_leitura_ = base::TimeTicks::Now();
    if (visita) {
      servico->banco()
          .AsyncCall(&Banco::TemMiniatura)
          .WithArgs(chave_, kIdadeDaMiniatura)
          .Then(base::BindOnce(&Leitor::TalvezMiniatura,
                               weak_factory_.GetWeakPtr(), pagina_));
    }
  }

  // A miniatura é o alto da página como está na tela.
  void TalvezMiniatura(int pagina, bool tem) {
    if (tem || pagina != pagina_ ||
        web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
      return;
    }
    content::RenderWidgetHostView* vista =
        web_contents()->GetRenderWidgetHostView();
    if (!vista || !vista->IsShowing()) {
      return;
    }
    const gfx::Size tamanho = vista->GetViewBounds().size();
    if (tamanho.IsEmpty()) {
      return;
    }
    const int altura = std::min(tamanho.height(), tamanho.width() *
                                                      kAlturaDaMiniatura /
                                                      kLarguraDaMiniatura);
    vista->CopyFromSurface(
        gfx::Rect(0, 0, tamanho.width(), altura),
        gfx::Size(kLarguraDaMiniatura, kAlturaDaMiniatura), base::TimeDelta(),
        base::BindOnce(&Leitor::Copiou, weak_factory_.GetWeakPtr(), pagina_));
  }

  void Copiou(int pagina, const content::CopyFromSurfaceResult& copia) {
    if (pagina != pagina_ || !copia.has_value() ||
        copia->bitmap.drawsNothing()) {
      return;
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](SkBitmap bitmap) { return gfx::JPEGCodec::Encode(bitmap, 72); },
            copia->bitmap),
        base::BindOnce(&GuardarMiniatura, Perfil()->GetWeakPtr(), chave_));
  }

  GURL url_;
  std::string chave_;
  // Conta as páginas da aba: resposta de uma leitura que chega depois da
  // troca de página não vale.
  int pagina_ = 0;
  // O documento pediu pra não ser guardado (vale também pras páginas que ele
  // troca sem recarregar).
  bool fora_do_documento_ = true;
  bool fora_ = true;
  bool carregou_ = false;
  bool lida_ = false;
  bool senha_ = false;
  base::TimeTicks ultima_leitura_;
  base::TimeTicks a_vista_desde_;
  base::TimeDelta tempo_;
  base::OneShotTimer relogio_;
  base::WeakPtrFactory<Leitor> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(Leitor);

}  // namespace

void AcompanharAba(content::WebContents* contents) {
  Profile* perfil = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (perfil->IsOffTheRecord() || perfil->IsGuestSession()) {
    return;
  }
  Leitor::CreateForWebContents(contents);
}

bool Ligada(Profile* perfil) {
  return Prefs(perfil)->GetBoolean(espacos::kMemoriaPref);
}

std::string Chave(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() || !url.has_host()) {
    return std::string();
  }
  GURL::Replacements limpar;
  limpar.ClearRef();
  limpar.ClearUsername();
  limpar.ClearPassword();
  return links::Limpar(url).ReplaceComponents(limpar).spec();
}

GURL EnderecoNoTrecho(const Resultado& resultado) {
  GURL url(resultado.url);
  const std::string fragmento = texto::Fragmento(resultado.trecho.frase);
  if (fragmento.empty() || !url.is_valid()) {
    return url;
  }
  GURL::Replacements trecho;
  trecho.SetRefStr(fragmento);
  return url.ReplaceComponents(trecho);
}

void Buscar(Profile* perfil,
            Filtro filtro,
            base::OnceCallback<void(std::vector<Resultado>)> pronto) {
  Servico* servico = Pegar(perfil);
  if (!servico) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pronto), std::vector<Resultado>()));
    return;
  }
  servico->banco()
      .AsyncCall(&Banco::Buscar)
      .WithArgs(std::move(filtro))
      .Then(std::move(pronto));
}

void Miniatura(Profile* perfil,
               int64_t id,
               base::OnceCallback<void(const gfx::ImageSkia&)> pronto) {
  Servico* servico = Pegar(perfil);
  if (!servico) {
    std::move(pronto).Run(gfx::ImageSkia());
    return;
  }
  servico->banco()
      .AsyncCall(&Banco::Miniatura)
      .WithArgs(id)
      .Then(base::BindOnce(
          [](base::OnceCallback<void(const gfx::ImageSkia&)> pronto,
             std::vector<uint8_t> jpeg) {
            if (jpeg.empty()) {
              std::move(pronto).Run(gfx::ImageSkia());
              return;
            }
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, {base::TaskPriority::USER_VISIBLE},
                base::BindOnce(
                    [](std::vector<uint8_t> jpeg) {
                      return gfx::JPEGCodec::Decode(jpeg);
                    },
                    std::move(jpeg)),
                base::BindOnce(
                    [](base::OnceCallback<void(const gfx::ImageSkia&)> pronto,
                       SkBitmap bitmap) {
                      std::move(pronto).Run(
                          bitmap.drawsNothing()
                              ? gfx::ImageSkia()
                              : gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
                    },
                    std::move(pronto)));
          },
          std::move(pronto)));
}

void Resumir(Profile* perfil, base::OnceCallback<void(Resumo)> pronto) {
  Servico* servico = Pegar(perfil);
  if (!servico) {
    std::move(pronto).Run(Resumo());
    return;
  }
  servico->banco().AsyncCall(&Banco::Resumir).Then(std::move(pronto));
}

void Esquecer(Profile* perfil, int64_t id) {
  if (Servico* servico = Pegar(perfil)) {
    servico->banco().AsyncCall(&Banco::EsquecerPagina).WithArgs(id);
  }
}

void EsquecerEndereco(Profile* perfil, const GURL& url) {
  Servico* servico = Pegar(perfil);
  std::string chave = Chave(url);
  if (servico && !chave.empty()) {
    servico->banco()
        .AsyncCall(&Banco::EsquecerEnderecos)
        .WithArgs(std::vector<std::string>{std::move(chave)});
  }
}

void EsquecerTudo(Profile* perfil, base::OnceClosure pronto) {
  Servico* servico = Pegar(perfil);
  if (!servico) {
    std::move(pronto).Run();
    return;
  }
  servico->banco().AsyncCall(&Banco::EsquecerTudo).Then(std::move(pronto));
}

bool SiteDeFora(Profile* perfil, const GURL& url) {
  for (const base::Value& site :
       Prefs(perfil)->GetList(espacos::kMemoriaForaSitesPref)) {
    if (site.is_string() && NoDominio(url.host(), site.GetString())) {
      return true;
    }
  }
  return false;
}

void DeixarDeFora(Profile* perfil, const std::string& host, bool fora) {
  const std::string limpo = HostDigitado(host);
  if (limpo.empty()) {
    return;
  }
  {
    ScopedListPrefUpdate lista(Prefs(perfil), espacos::kMemoriaForaSitesPref);
    lista->EraseValue(base::Value(limpo));
    if (fora) {
      lista->Append(limpo);
    }
  }
  if (Servico* servico = fora ? Pegar(perfil->GetOriginalProfile()) : nullptr) {
    servico->banco().AsyncCall(&Banco::EsquecerSite).WithArgs(limpo);
  }
}

std::string SitesDeFora(Profile* perfil) {
  std::vector<std::string> sites;
  for (const base::Value& site :
       Prefs(perfil)->GetList(espacos::kMemoriaForaSitesPref)) {
    if (site.is_string()) {
      sites.push_back(site.GetString());
    }
  }
  return base::JoinString(sites, ", ");
}

void DefinirSitesDeFora(Profile* perfil, const std::string& sites) {
  std::vector<std::string> antes;
  for (const base::Value& site :
       Prefs(perfil)->GetList(espacos::kMemoriaForaSitesPref)) {
    if (site.is_string()) {
      antes.push_back(site.GetString());
    }
  }
  base::ListValue lista;
  std::set<std::string> vistos;
  for (std::string_view pedaco : base::SplitStringPiece(
           sites, ", \n\t;", base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    const std::string host = HostDigitado(pedaco);
    if (!host.empty() && vistos.insert(host).second) {
      lista.Append(host);
      if (!std::ranges::contains(antes, host)) {
        if (Servico* servico = Pegar(perfil->GetOriginalProfile())) {
          servico->banco().AsyncCall(&Banco::EsquecerSite).WithArgs(host);
        }
      }
    }
  }
  Prefs(perfil)->SetList(espacos::kMemoriaForaSitesPref, std::move(lista));
}

bool ContainerDeFora(Profile* perfil, const std::string& container) {
  return Prefs(perfil)
      ->GetList(espacos::kMemoriaForaContainersPref)
      .contains(container);
}

void DeixarContainerDeFora(Profile* perfil,
                           const std::string& container,
                           bool fora) {
  {
    ScopedListPrefUpdate lista(Prefs(perfil),
                               espacos::kMemoriaForaContainersPref);
    lista->EraseValue(base::Value(container));
    if (fora) {
      lista->Append(container);
    }
  }
  if (Servico* servico = fora ? Pegar(perfil->GetOriginalProfile()) : nullptr) {
    servico->banco().AsyncCall(&Banco::EsquecerContainer).WithArgs(container);
  }
}

}  // namespace navegador::memoria
