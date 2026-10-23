#include "navegador/browser/ui/previa.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/prefs/pref_service.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/ui/containers/espaco.h"
#include "navegador/browser/divisao/divisao.h"
#include "navegador/browser/ui/visual.h"
#include "navegador/icones/menu/vector_icons.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace navegador::previa {

namespace {

constexpr int kRaio = 12;
constexpr int kCabecalho = 42;
constexpr int kBotao = 28;
constexpr int kIcone = 15;

class Previa;

std::map<BrowserWindowInterface*, std::unique_ptr<Previa>>& Previas() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*, std::unique_ptr<Previa>>>
      previas;
  return *previas;
}

// Fecha a prévia da janela fora da pilha atual (ela pode estar dentro de um
// evento da própria página).
void FecharDepois(BrowserWindowInterface* browser);

// Conteúdo da janelinha da prévia: o fundo escurecido por cima da página e o
// cartão no meio, com o cabeçalho e a página.
class Tela : public views::View {
  METADATA_HEADER(Tela, views::View)

 public:
  Tela(base::RepeatingClosure fechar,
       base::RepeatingClosure abrir_como_aba,
       base::RepeatingClosure dividir)
      : fechar_(std::move(fechar)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBackgroundBlur(10);

    cabecalho_ = AddChildView(std::make_unique<views::View>());
    auto* layout =
        cabecalho_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::TLBR(0, 14, 0, 8), 2));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    icone_ = cabecalho_->AddChildView(std::make_unique<views::ImageView>());
    icone_->SetImageSize(gfx::Size(16, 16));
    icone_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 10));
    icone_->SetCanProcessEventsWithinSubtree(false);

    auto* textos = cabecalho_->AddChildView(std::make_unique<views::View>());
    textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 1));
    textos->SetCanProcessEventsWithinSubtree(false);
    layout->SetFlexForView(textos, 1);
    const gfx::FontList base;
    titulo_ = textos->AddChildView(std::make_unique<views::Label>());
    titulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    titulo_->SetElideBehavior(gfx::ELIDE_TAIL);
    titulo_->SetFontList(base.DeriveWithSizeDelta(13 - base.GetFontSize())
                             .DeriveWithWeight(gfx::Font::Weight::SEMIBOLD));
    endereco_ = textos->AddChildView(std::make_unique<views::Label>());
    endereco_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    endereco_->SetElideBehavior(gfx::ELIDE_MIDDLE);
    endereco_->SetFontList(base.DeriveWithSizeDelta(11 - base.GetFontSize()));

    botoes_.push_back(cabecalho_->AddChildView(CriarBotao(
        icones_menu::kColumns2Icon, u"Dividir com a aba", std::move(dividir))));
    botoes_.push_back(cabecalho_->AddChildView(
        CriarBotao(icones_menu::kSquareArrowOutUpRightIcon, u"Abrir como aba",
                   std::move(abrir_como_aba))));
    botoes_.push_back(cabecalho_->AddChildView(CriarBotao(
        vector_icons::kCloseIcon, u"Fechar a prévia (Esc)", fechar_)));

    pagina_ = AddChildView(std::make_unique<views::WebView>());
  }

  views::WebView* pagina() { return pagina_; }

  void DefinirTextos(const std::u16string& titulo,
                     const std::u16string& endereco) {
    titulo_->SetText(titulo);
    endereco_->SetText(endereco);
  }

  void DefinirIcone(const gfx::ImageSkia& icone) {
    icone_->SetImage(ui::ImageModel::FromImageSkia(icone));
  }

  // Onde fica o cartão: quase a área toda da página, com uma margem que
  // deixa a de trás à vista.
  gfx::Rect Cartao() const {
    gfx::Rect area = GetLocalBounds();
    const int lados = std::max(28, area.width() * 6 / 100);
    const int cima = std::max(18, area.height() * 3 / 100);
    area.Inset(gfx::Insets::VH(cima, lados));
    return area;
  }

  // views::View:
  void Layout(PassKey) override {
    const gfx::Rect cartao = Cartao();
    cabecalho_->SetBounds(cartao.x(), cartao.y(), cartao.width(), kCabecalho);
    pagina_->SetBoundsRect(gfx::Rect(cartao.x(), cartao.y() + kCabecalho,
                                     cartao.width(),
                                     cartao.height() - kCabecalho));
    pagina_->holder()->SetNativeViewCornerRadii(
        gfx::RoundedCornersF(0, 0, kRaio, kRaio));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const ui::ColorProvider* cores = GetColorProvider();
    canvas->DrawColor(SkColorSetARGB(0x73, 0x00, 0x00, 0x00));

    const gfx::RectF cartao(Cartao());
    cc::PaintFlags sombra;
    sombra.setAntiAlias(true);
    sombra.setColor(SK_ColorTRANSPARENT);
    sombra.setLooper(gfx::CreateShadowDrawLooper(
        {gfx::ShadowValue(gfx::Vector2d(0, 12), 48,
                          SkColorSetA(SK_ColorBLACK, 0x66)),
         gfx::ShadowValue(gfx::Vector2d(0, 2), 8,
                          SkColorSetA(SK_ColorBLACK, 0x40))}));
    canvas->DrawRoundRect(cartao, kRaio, sombra);

    cc::PaintFlags fundo;
    fundo.setAntiAlias(true);
    fundo.setColor(cores->GetColor(ui::kColorSysSurface));
    canvas->DrawRoundRect(cartao, kRaio, fundo);

    cc::PaintFlags borda;
    borda.setAntiAlias(true);
    borda.setStyle(cc::PaintFlags::kStroke_Style);
    borda.setStrokeWidth(1);
    borda.setColor(cores->GetColor(ui::kColorSysDivider));
    gfx::RectF contorno = cartao;
    contorno.Inset(0.5f);
    canvas->DrawRoundRect(contorno, kRaio, borda);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const ui::ColorProvider* cores = GetColorProvider();
    titulo_->SetEnabledColor(cores->GetColor(ui::kColorSysOnSurface));
    endereco_->SetEnabledColor(cores->GetColor(ui::kColorSysOnSurfaceSubtle));
    for (views::ImageButton* botao : botoes_) {
      views::InkDrop::Get(botao)->SetBaseColor(
          cores->GetColor(ui::kColorSysOnSurface));
    }
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Clicar fora do cartão fecha (como no Zen).
    if (!Cartao().Contains(event.location())) {
      fechar_.Run();
      return true;
    }
    return false;
  }

 private:
  static std::unique_ptr<views::ImageButton> CriarBotao(
      const gfx::VectorIcon& icone,
      std::u16string nome,
      base::RepeatingClosure apertou) {
    auto botao = std::make_unique<views::ImageButton>(base::BindRepeating(
        [](base::RepeatingClosure c) { c.Run(); }, std::move(apertou)));
    botao->SetPreferredSize(gfx::Size(kBotao, kBotao));
    botao->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    botao->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    botao->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(icone, ui::kColorSysOnSurface, kIcone));
    botao->SetTooltipText(nome);
    botao->GetViewAccessibility().SetName(nome);
    views::InkDrop::Get(botao.get())
        ->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InstallRoundRectHighlightPathGenerator(botao.get(), gfx::Insets(),
                                                  7);
    return botao;
  }

  base::RepeatingClosure fechar_;
  raw_ptr<views::View> cabecalho_ = nullptr;
  raw_ptr<views::ImageView> icone_ = nullptr;
  raw_ptr<views::Label> titulo_ = nullptr;
  raw_ptr<views::Label> endereco_ = nullptr;
  std::vector<raw_ptr<views::ImageButton>> botoes_;
  raw_ptr<views::WebView> pagina_ = nullptr;
};

BEGIN_METADATA(Tela)
END_METADATA

// O fundo da página da prévia antes de ela pintar, na cor do cartão, como a
// aba faz com o dela (ContentsWebView): sem isso, um clarão branco até a
// página chegar. A página nova (outro processo) ganha o fundo também.
class FundoDaPrevia : public content::WebContentsObserver {
 public:
  FundoDaPrevia(content::WebContents* contents, SkColor cor)
      : content::WebContentsObserver(contents),
        cor_(SkColorSetA(cor, SK_AlphaOPAQUE)) {
    Aplicar();
  }

  void RenderViewReady() override { Aplicar(); }
  void RenderViewHostChanged(content::RenderViewHost* antiga,
                             content::RenderViewHost* nova) override {
    Aplicar();
  }

 private:
  void Aplicar() {
    if (content::RenderWidgetHostView* view =
            web_contents()->GetRenderWidgetHostView()) {
      view->SetBackgroundColor(cor_);
    }
  }

  const SkColor cor_;
};

class Previa : public content::WebContentsDelegate,
               public TabStripModelObserver,
               public views::WidgetObserver,
               public favicon::FaviconDriverObserver {
 public:
  Previa(BrowserWindowInterface* browser,
         BrowserView* janela,
         content::WebContents* origem)
      : browser_(browser), janela_(janela), origem_(origem) {}

  Previa(const Previa&) = delete;
  Previa& operator=(const Previa&) = delete;

  ~Previa() override {
    TirarTela();
    fundo_.reset();
    if (contents_) {
      contents_->SetDelegate(nullptr);
    }
    contents_.reset();
  }

  void Comecar(const GURL& url,
               const content::Referrer& referrer,
               const std::optional<url::Origin>& iniciador) {
    Profile* perfil = browser_->GetProfile();
    // No container da aba de origem (ou na partição padrão, sem container).
    scoped_refptr<content::SiteInstance> instancia =
        containers::SiteInstanceForNewTab(
            perfil, origem_, containers::EspacoFocado(browser_), url);
    if (!instancia) {
      instancia = content::SiteInstance::CreateForURL(perfil, url);
    }
    content::WebContents::CreateParams parametros(perfil, instancia);
    parametros.context = janela_->GetWidget()->GetNativeWindow();
    contents_ = content::WebContents::Create(parametros);
    contents_->SetDelegate(this);
    // O ícone do site no cabeçalho (o mesmo driver que a aba usa depois).
    favicon::CreateContentFaviconDriverForWebContents(contents_.get());
    if (favicon::FaviconDriver* icones =
            favicon::ContentFaviconDriver::FromWebContents(contents_.get())) {
      icones_.Observe(icones);
    }

    // Camada por cima das páginas, filha do MultiContentsView (o layout
    // dele estica ela pela área toda). Na mesma janela, a página da prévia
    // recebe o teclado como uma aba.
    views::View* area = janela_->contents_container();
    tela_ = area->AddChildView(std::make_unique<Tela>(
        base::BindRepeating(&FecharDepois, browser_.get()),
        base::BindRepeating(&Previa::AbrirComoAba, base::Unretained(this),
                            /*dividir=*/false),
        base::BindRepeating(&Previa::AbrirComoAba, base::Unretained(this),
                            /*dividir=*/true)));
    tela_->SetID(visual::kIdPrevia);
    tela_->DefinirIcone(favicon::GetDefaultFavicon().AsImageSkia());
    tela_->pagina()->SetWebContents(contents_.get());
    fundo_ = std::make_unique<FundoDaPrevia>(
        contents_.get(), tela_->GetColorProvider()->GetColor(ui::kColorSysSurface));
    AtualizarTextos(url);
    area->InvalidateLayout();

    content::NavigationController::LoadURLParams carregar(url);
    carregar.referrer = referrer;
    carregar.initiator_origin = iniciador;
    carregar.transition_type = ui::PAGE_TRANSITION_LINK;
    contents_->GetController().LoadURLWithParams(carregar);

    // Nasce do ponto do clique (o ponteiro, no Shift+clique), crescendo e
    // aparecendo. Com "menos movimento" no sistema, aparece de uma vez.
    gfx::Point ponto = display::Screen::Get()->GetCursorScreenPoint();
    views::View::ConvertPointFromScreen(tela_, &ponto);
    gfx::Transform inicio;
    inicio.Translate(ponto.x(), ponto.y());
    inicio.Scale(0.92, 0.92);
    inicio.Translate(-ponto.x(), -ponto.y());
    tela_->layer()->SetTransform(inicio);
    tela_->layer()->SetOpacity(0.0f);
    views::AnimationBuilder()
        .Once()
        .SetDuration(
            gfx::Animation::RichAnimationDuration(base::Milliseconds(260)))
        .SetOpacity(tela_->layer(), 1.0f, gfx::Tween::EASE_OUT)
        .SetTransform(tela_->layer(), gfx::Transform(),
                      gfx::Tween::FAST_OUT_SLOW_IN);
    tela_->pagina()->RequestFocus();

    browser_->GetTabStripModel()->AddObserver(this);
    janela_widget_.Observe(janela_->GetWidget());
  }

  void Esconder() {
    if (tela_) {
      tela_->SetVisible(false);
    }
  }

  // Fecha esmaecendo (120 ms; com "menos movimento", na hora). Devolve
  // quanto falta pra sumir de vez.
  base::TimeDelta EsconderAnimado() {
    const base::TimeDelta tempo =
        gfx::Animation::RichAnimationDuration(base::Milliseconds(120));
    if (!tela_ || !tela_->GetVisible() || tempo.is_zero()) {
      Esconder();
      return base::TimeDelta();
    }
    // Sumindo, não pega mais clique nem tecla.
    tela_->SetCanProcessEventsWithinSubtree(false);
    views::AnimationBuilder()
        .Once()
        .SetDuration(tempo)
        .SetOpacity(tela_->layer(), 0.0f, gfx::Tween::EASE_IN);
    return tempo;
  }

  // Volta o foco pra aba de origem (a prévia está fechando).
  void DevolverFoco() {
    if (!janela_fechando_ && !virou_aba_ && origem_ &&
        browser_->GetTabStripModel()->GetIndexOfWebContents(origem_) !=
            TabStripModel::kNoTab) {
      origem_->Focus();
    }
  }

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
      content::NavigationController::LoadURLParams carregar(params);
      source->GetController().LoadURLWithParams(carregar);
      return source;
    }
    // Aba nova (Ctrl+clique etc.): como se viesse da aba de origem (mesmo
    // container e espaço). Shift+clique aqui dentro vira aba também.
    NavigateParams nav(browser_, params.url, params.transition);
    nav.FillNavigateParamsFromOpenURLParams(params);
    if (!origem_ || janela_fechando_) {
      return nullptr;
    }
    nav.source_contents = origem_;
    if (nav.disposition == WindowOpenDisposition::NEW_WINDOW ||
        nav.disposition == WindowOpenDisposition::NEW_POPUP) {
      nav.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }
    if (nav.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
      FecharDepois(browser_);
    }
    Navigate(&nav);
    return nav.navigated_or_inserted_contents;
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    // target=_blank e window.open: a janela que a página pede (popup de
    // login continua popup, com o window.opener).
    if (!origem_ || janela_fechando_) {
      return nullptr;
    }
    NavigateParams nav(browser_, std::move(new_contents));
    nav.source_contents = origem_;
    nav.disposition = disposition == WindowOpenDisposition::NEW_POPUP
                          ? disposition
                          : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    nav.window_features = window_features;
    nav.user_gesture = user_gesture;
    if (nav.disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
      FecharDepois(browser_);
    }
    Navigate(&nav);
    return nav.navigated_or_inserted_contents;
  }

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    if (event.GetType() != blink::WebInputEvent::Type::kRawKeyDown) {
      return content::KeyboardEventProcessingResult::NOT_HANDLED;
    }
    const int modificadores =
        event.GetModifiers() & blink::WebInputEvent::kKeyModifiers;
    // Esc fecha a prévia, e o Ctrl+W também (em vez de fechar a aba de trás).
    if ((event.windows_key_code == ui::VKEY_ESCAPE && modificadores == 0) ||
        (event.windows_key_code == ui::VKEY_W &&
         modificadores == blink::WebInputEvent::kControlKey)) {
      FecharDepois(browser_);
      return content::KeyboardEventProcessingResult::HANDLED;
    }
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    // Os atalhos do navegador valem com o foco na prévia.
    return tela_ &&
           teclas_.HandleKeyboardEvent(event, janela_->GetFocusManager());
  }

  void CloseContents(content::WebContents* source) override {
    FecharDepois(browser_);
  }

  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override {
    if (changed_flags &
        (content::INVALIDATE_TYPE_TITLE | content::INVALIDATE_TYPE_URL)) {
      AtualizarTextos(source->GetVisibleURL());
    }
  }

  // TabStripModelObserver: trocar de aba (ou fechar a de origem) fecha a
  // prévia, que é da aba.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (tab_strip_model->GetIndexOfWebContents(origem_) ==
        TabStripModel::kNoTab) {
      origem_ = nullptr;
      FecharDepois(browser_);
    } else if (selection.active_tab_changed()) {
      FecharDepois(browser_);
    }
  }

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    if (tela_ && notification_icon_type == NON_TOUCH_16_DIP &&
        !image.IsEmpty()) {
      tela_->DefinirIcone(image.AsImageSkia());
    }
  }

  // views::WidgetObserver: a janela do navegador está fechando.
  void OnWidgetDestroying(views::Widget* widget) override {
    janela_fechando_ = true;
    origem_ = nullptr;
    browser_->GetTabStripModel()->RemoveObserver(this);
    janela_widget_.Reset();
    TirarTela();
    // A página sai já: na saída do navegador a tarefa do FecharDepois pode
    // não rodar, e uma WebContents viva segura o perfil. O fechar da janela
    // é assíncrono, então não estamos dentro de um evento dela.
    fundo_.reset();
    if (contents_) {
      contents_->SetDelegate(nullptr);
      contents_.reset();
    }
    FecharDepois(browser_);
  }

 private:
  // Tira a camada da janela (a página dela sai antes).
  void TirarTela() {
    icones_.Reset();
    if (!tela_) {
      return;
    }
    tela_->pagina()->SetWebContents(nullptr);
    views::View* area = tela_->parent();
    area->RemoveChildViewT(tela_.ExtractAsDangling());
    area->InvalidateLayout();
  }

  void AtualizarTextos(const GURL& url) {
    if (!tela_ || !contents_) {
      return;
    }
    std::u16string titulo = contents_->GetTitle();
    const std::u16string host = base::UTF8ToUTF16(url.host());
    if (titulo.empty()) {
      titulo = host;
    }
    tela_->DefinirTextos(titulo, base::UTF8ToUTF16(url.spec()));
  }

  void AbrirComoAba(bool dividir) {
    TabStripModel* modelo = browser_->GetTabStripModel();
    const int indice = modelo->GetIndexOfWebContents(origem_);
    if (!contents_ || indice == TabStripModel::kNoTab) {
      FecharDepois(browser_);
      return;
    }
    browser_->GetTabStripModel()->RemoveObserver(this);
    icones_.Reset();
    fundo_.reset();
    tela_->pagina()->SetWebContents(nullptr);
    contents_->SetDelegate(nullptr);
    std::unique_ptr<content::WebContents> pagina = std::move(contents_);
    content::WebContents* nova = pagina.get();
    Esconder();
    // O foco fica na página que veio da prévia, não volta pra origem.
    virou_aba_ = true;
    // Ao lado da aba de origem (depois da divisão dela, sem quebrar a
    // divisão), no grupo (espaço) dela.
    tabs::TabInterface* const aba_de_origem = modelo->GetTabAtIndex(indice);
    int onde = indice + 1;
    if (aba_de_origem->IsSplit()) {
      onde = static_cast<int>(
          modelo->GetSplitData(*aba_de_origem->GetSplit())
              ->GetIndexRange()
              .end());
    }
    modelo->InsertWebContentsAt(onde, std::move(pagina),
                                AddTabTypes::ADD_ACTIVE,
                                modelo->GetTabGroupForTab(indice));
    if (dividir) {
      // À direita da origem; numa divisão, entra nela (até 4 abas).
      tabs::TabInterface* const aba_nova =
          tabs::TabInterface::MaybeGetFromContents(nova);
      if (aba_nova) {
        divisao::Juntar(modelo, aba_nova, aba_de_origem,
                        divisao::Lado::kDireita);
      }
    }
    nova->Focus();
    FecharDepois(browser_);
  }

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<BrowserView> janela_;
  raw_ptr<content::WebContents> origem_;
  bool janela_fechando_ = false;
  bool virou_aba_ = false;
  std::unique_ptr<content::WebContents> contents_;
  // Declarado depois do contents_: sai antes dele.
  std::unique_ptr<FundoDaPrevia> fundo_;
  raw_ptr<Tela> tela_ = nullptr;
  views::UnhandledKeyboardEventHandler teclas_;
  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      icones_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver> janela_widget_{
      this};
};

void Fechar(BrowserWindowInterface* browser) {
  auto it = Previas().find(browser);
  if (it == Previas().end()) {
    return;
  }
  std::unique_ptr<Previa> previa = std::move(it->second);
  Previas().erase(it);
  previa->Esconder();
  previa->DevolverFoco();
}

void FecharDepois(BrowserWindowInterface* browser) {
  auto it = Previas().find(browser);
  if (it == Previas().end()) {
    return;
  }
  // Some esmaecendo; o resto (a página) é destruído fora do evento, depois
  // que a animação termina (e um quadro de folga).
  const base::TimeDelta tempo = it->second->EsconderAnimado();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&Fechar, browser),
      tempo.is_zero() ? base::TimeDelta() : tempo + base::Milliseconds(16));
}

}  // namespace

bool Abrir(BrowserWindowInterface* browser,
           content::WebContents* origem,
           const GURL& url,
           const content::Referrer& referrer,
           const std::optional<url::Origin>& iniciador) {
  if (!browser->GetProfile()->GetPrefs()->GetBoolean(espacos::kPreviaPref) ||
      browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL ||
      !url.SchemeIsHTTPOrHTTPS() ||
      browser->GetTabStripModel()->GetIndexOfWebContents(origem) ==
          TabStripModel::kNoTab) {
    return false;
  }
  BrowserView* janela = BrowserView::GetBrowserViewForBrowser(browser);
  if (!janela || !janela->GetWidget() || !janela->contents_container()) {
    return false;
  }
  // Uma prévia por janela: a nova toma o lugar da anterior.
  if (auto it = Previas().find(browser); it != Previas().end()) {
    std::unique_ptr<Previa> anterior = std::move(it->second);
    Previas().erase(it);
    anterior->Esconder();
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(anterior));
  }
  auto previa = std::make_unique<Previa>(browser, janela, origem);
  Previa* nova = previa.get();
  Previas()[browser] = std::move(previa);
  nova->Comecar(url, referrer, iniciador);
  return true;
}

}  // namespace navegador::previa
