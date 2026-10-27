#include "navegador/browser/ui/fixos.h"

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/path_effect.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/favicon/core/favicon_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/containers/fixos.h"
#include "navegador/browser/containers/titulos.h"
#include "navegador/browser/ui/abas/abas.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "navegador/browser/ui/espacos.h"
#include "navegador/browser/ui/visual.h"
#include "navegador/icones/menu/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/paint_info.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace navegador::fixos {

namespace {

// Medidas das abas da lateral: destaque de 30, uma
// linha a cada 35, cantos 8; ícone 16 a 8 da borda e texto a 32.
constexpr int kAlturaLinha = 30;
constexpr int kRaio = 8;
constexpr int kRecuoDaPasta = 12;
// Linha embaixo dos fixados: 1 de altura, 5 em cima e embaixo, 12 dos lados.
constexpr int kAlturaDivisor = 15;
constexpr int kMargemDivisor = 12;

constexpr const char* kNovaPasta = containers::kFixarNovaPasta;
constexpr const char* kDesafixar = containers::kFixarDesafixar;

std::u16string NomeDoSite(const Site& site) {
  if (!site.titulo.empty()) {
    return base::UTF8ToUTF16(site.titulo);
  }
  return base::UTF8ToUTF16(site.url.host().empty() ? site.url.spec()
                                                   : site.url.host());
}

// O nome pra um site fixado a partir da aba: o que a pessoa deu, ou o da
// página.
std::string TituloParaFixar(content::WebContents* contents) {
  if (const std::u16string* nome = titulos::Titulo(contents)) {
    return base::UTF16ToUTF8(*nome);
  }
  return base::UTF16ToUTF8(contents->GetTitle());
}

GURL EnderecoDaAba(content::WebContents* contents) {
  return contents->GetLastCommittedURL().is_empty()
             ? contents->GetVisibleURL()
             : contents->GetLastCommittedURL();
}

// Liga a aba ao site e grava na sessão (pra voltar ligada ao reabrir).
void Ligar(content::WebContents* contents, const std::string& id) {
  LigarAba(contents, id);
  Profile* perfil = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (SessionService* sessao =
          SessionServiceFactory::GetForProfileIfExisting(perfil)) {
    sessao->AddTabExtraData(
        sessions::SessionTabHelper::IdForWindowContainingTab(contents),
        sessions::SessionTabHelper::IdForTab(contents), kSessionKey, id);
  }
}

// Janelinha que pede um nome (pasta nova, renomear).
void PedirNome(BrowserWindowInterface* browser,
               std::u16string titulo,
               std::u16string inicial,
               base::OnceCallback<void(const std::string&)> pronto) {
  auto dialogo = std::make_unique<views::DialogDelegate>();
  dialogo->SetTitle(titulo);
  dialogo->SetModalType(ui::mojom::ModalType::kWindow);
  dialogo->SetShowCloseButton(true);
  dialogo->SetButtonLabel(ui::mojom::DialogButton::kOk, u"Salvar");
  dialogo->SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancelar");
  dialogo->set_fixed_width(360);
  auto conteudo = std::make_unique<views::View>();
  conteudo->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(4, 20, 8, 20)));
  auto* campo = conteudo->AddChildView(std::make_unique<views::Textfield>());
  campo->SetText(inicial);
  campo->SelectAll(false);
  campo->GetViewAccessibility().SetName(titulo);
  dialogo->SetInitiallyFocusedView(campo);
  dialogo->SetAcceptCallback(base::BindOnce(
      [](views::Textfield* campo,
         base::OnceCallback<void(const std::string&)> pronto) {
        std::string nome = base::UTF16ToUTF8(campo->GetText());
        if (!nome.empty()) {
          std::move(pronto).Run(nome);
        }
      },
      base::Unretained(campo), std::move(pronto)));
  dialogo->SetContentsView(std::move(conteudo));
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!view || !view->GetWidget()) {
    return;
  }
  constrained_window::CreateBrowserModalDialogViews(
      std::move(dialogo), view->GetWidget()->GetNativeWindow())
      ->Show();
  // Subsuperfície no Wayland: só aparece com um quadro novo da janela.
  view->GetWidget()->GetRootView()->SchedulePaint();
}

class AreaDeFixos;

std::map<BrowserWindowInterface*, AreaDeFixos*>& Areas() {
  static base::NoDestructor<std::map<BrowserWindowInterface*, AreaDeFixos*>>
      areas;
  return *areas;
}

// Fundo arredondado das linhas (como a aba): ativa ou com o mouse em cima.
void PintarFundo(gfx::Canvas* canvas,
                 const views::View& linha,
                 bool ativa,
                 bool hover) {
  const ui::ColorProvider* cores = linha.GetColorProvider();
  if (!cores || (!ativa && !hover)) {
    return;
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(cores->GetColor(
      ativa ? kColorTabBackgroundActiveFrameActive
            : kColorTabBackgroundInactiveHoverFrameActive));
  canvas->DrawRoundRect(gfx::RectF(linha.GetLocalBounds()), kRaio, flags);
}

// Um site fixado: ícone, nome e o "x" (com a aba aberta e o mouse em cima).
class LinhaDoSite : public views::Button {
  METADATA_HEADER(LinhaDoSite, views::Button)

 public:
  LinhaDoSite(const Site& site,
              int recuo,
              base::RepeatingClosure abrir,
              base::RepeatingClosure fechar,
              base::RepeatingCallback<bool()> clique_duplo)
      : views::Button(std::move(abrir)),
        site_(site),
        clique_duplo_(std::move(clique_duplo)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(0, 8 + recuo, 0, 4), 8));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    icone_ = AddChildView(std::make_unique<views::ImageView>());
    icone_->SetImageSize(gfx::Size(16, 16));
    icone_->SetCanProcessEventsWithinSubtree(false);
    titulo_ = AddChildView(std::make_unique<views::Label>(NomeDoSite(site)));
    titulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    titulo_->SetElideBehavior(gfx::ELIDE_TAIL);
    titulo_->SetFontList(gfx::FontList().DeriveWithSizeDelta(
        13 - gfx::FontList().GetFontSize()));
    titulo_->SetCanProcessEventsWithinSubtree(false);
    layout->SetFlexForView(titulo_, 1);
    // A aba do site tocando som (como o ícone de som das abas).
    som_ = AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(icones_menu::kVolume2Icon,
                                       kColorTabForegroundInactiveFrameActive,
                                       14)));
    som_->SetCanProcessEventsWithinSubtree(false);
    som_->SetTooltipText(u"Tocando");
    som_->SetVisible(false);
    fechar_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        std::move(fechar), vector_icons::kCloseSmallIcon, 16));
    fechar_->SetTooltipText(u"Fechar a aba (o site continua aqui)");
    fechar_->SetVisible(false);
    SetPreferredSize(gfx::Size(0, kAlturaLinha));
    SetTooltipText(base::UTF8ToUTF16(site.url.spec()));
    GetViewAccessibility().SetName(NomeDoSite(site));
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  const Site& site() const { return site_; }
  views::Label& rotulo() { return *titulo_; }

  // Clique duplo renomeia. Quem conta é a área (clique_duplo_): o primeiro
  // clique pode refazer a linha (a pasta abre ou fecha), e aí o Views não
  // marca o segundo como duplo.
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && clique_duplo_.Run()) {
      ignorar_soltar_ = true;
      return true;
    }
    return views::Button::OnMousePressed(event);
  }

  // As teclas do campo de renomear não clicam a linha (abas::Editando).
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    return !abas::Editando(this) && views::Button::OnKeyPressed(event);
  }

  bool OnKeyReleased(const ui::KeyEvent& event) override {
    return !abas::Editando(this) && views::Button::OnKeyReleased(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (std::exchange(ignorar_soltar_, false)) {
      SetState(IsMouseHovered() ? STATE_HOVERED : STATE_NORMAL);
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  void Mostrar(bool aberta,
               bool ativa,
               const ui::ImageModel& icone,
               bool tocando = false) {
    aberta_ = aberta;
    ativa_ = ativa;
    icone_->SetImage(icone);
    som_->SetVisible(tocando);
    fechar_->SetVisible(aberta_ && (ativa_ || IsMouseHovered()));
    AtualizarCor();
    SchedulePaint();
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    AtualizarCor();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    PintarFundo(canvas, *this, ativa_, IsMouseHovered());
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    fechar_->SetVisible(aberta_);
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    fechar_->SetVisible(aberta_ && ativa_);
    SchedulePaint();
  }

 private:
  // Como as abas: o site aberto e ativo no branco da aba ativa, aberto no
  // cinza das inativas e fechado mais apagado ainda (antes, aberto ficava no
  // branco e fechado quase no cinza das abas, o contrário delas).
  void AtualizarCor() {
    if (const ui::ColorProvider* cores = GetColorProvider()) {
      const SkColor inativa =
          cores->GetColor(kColorTabForegroundInactiveFrameActive);
      titulo_->SetEnabledColor(
          ativa_    ? cores->GetColor(kColorTabForegroundActiveFrameActive)
          : aberta_ ? inativa
                    : SkColorSetA(inativa, 0x99));
    }
  }

  Site site_;
  base::RepeatingCallback<bool()> clique_duplo_;
  bool ignorar_soltar_ = false;
  bool aberta_ = false;
  bool ativa_ = false;
  raw_ptr<views::ImageView> icone_ = nullptr;
  raw_ptr<views::Label> titulo_ = nullptr;
  raw_ptr<views::ImageView> som_ = nullptr;
  raw_ptr<views::ImageButton> fechar_ = nullptr;
};

BEGIN_METADATA(LinhaDoSite)
END_METADATA

// Cabeçalho de uma pasta: ícone, nome em cinza e a seta, como o grupo no Dia.
class CabecalhoDaPasta : public views::Button {
  METADATA_HEADER(CabecalhoDaPasta, views::Button)

 public:
  CabecalhoDaPasta(const Entrada& pasta,
                   int recuo,
                   base::RepeatingClosure alternar,
                   base::RepeatingCallback<bool()> clique_duplo)
      : views::Button(std::move(alternar)),
        aberta_(pasta.aberta),
        clique_duplo_(std::move(clique_duplo)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(0, 8 + recuo, 0, 8), 8));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    icone_ = AddChildView(std::make_unique<views::ImageView>());
    icone_->SetCanProcessEventsWithinSubtree(false);
    nome_ = AddChildView(
        std::make_unique<views::Label>(base::UTF8ToUTF16(pasta.nome)));
    nome_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    nome_->SetElideBehavior(gfx::ELIDE_TAIL);
    nome_->SetFontList(gfx::FontList().DeriveWithSizeDelta(
        13 - gfx::FontList().GetFontSize()));
    nome_->SetCanProcessEventsWithinSubtree(false);
    layout->SetFlexForView(nome_, 1);
    seta_ = AddChildView(std::make_unique<views::ImageView>());
    seta_->SetCanProcessEventsWithinSubtree(false);
    SetPreferredSize(gfx::Size(0, kAlturaLinha));
    GetViewAccessibility().SetName(u"Pasta " +
                                   base::UTF8ToUTF16(pasta.nome));
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    const ui::ColorId cor = kColorTabForegroundInactiveFrameActive;
    nome_->SetEnabledColor(GetColorProvider()->GetColor(cor));
    icone_->SetImage(ui::ImageModel::FromVectorIcon(
        aberta_ ? vector_icons::kFolderOpenIcon
                : vector_icons::kFolderFlippableIcon,
        cor, 16));
    seta_->SetImage(ui::ImageModel::FromVectorIcon(
        aberta_ ? vector_icons::kKeyboardArrowDownIcon
                : kKeyboardArrowRightIcon,
        cor, 14));
  }

  views::Label& rotulo() { return *nome_; }

  // Clique duplo renomeia. Quem conta é a área (clique_duplo_): o primeiro
  // clique pode refazer a linha (a pasta abre ou fecha), e aí o Views não
  // marca o segundo como duplo.
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && clique_duplo_.Run()) {
      ignorar_soltar_ = true;
      return true;
    }
    return views::Button::OnMousePressed(event);
  }

  // As teclas do campo de renomear não clicam a linha (abas::Editando).
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    return !abas::Editando(this) && views::Button::OnKeyPressed(event);
  }

  bool OnKeyReleased(const ui::KeyEvent& event) override {
    return !abas::Editando(this) && views::Button::OnKeyReleased(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (std::exchange(ignorar_soltar_, false)) {
      SetState(IsMouseHovered() ? STATE_HOVERED : STATE_NORMAL);
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    PintarFundo(canvas, *this, /*ativa=*/false, IsMouseHovered());
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    SchedulePaint();
  }

 private:
  bool aberta_;
  base::RepeatingCallback<bool()> clique_duplo_;
  bool ignorar_soltar_ = false;
  raw_ptr<views::ImageView> icone_ = nullptr;
  raw_ptr<views::Label> nome_ = nullptr;
  raw_ptr<views::ImageView> seta_ = nullptr;
};

BEGIN_METADATA(CabecalhoDaPasta)
END_METADATA

// Um favorito: ladrilho com o ícone do site (54x41, cantos 8, um pouco mais
// claro que a lateral e borda fina; medido no Dia).
class Ladrilho : public views::Button {
  METADATA_HEADER(Ladrilho, views::Button)

 public:
  Ladrilho(const Site& site, base::RepeatingClosure abrir)
      : views::Button(std::move(abrir)), site_(site) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    icone_ = AddChildView(std::make_unique<views::ImageView>());
    icone_->SetImageSize(gfx::Size(18, 18));
    icone_->SetCanProcessEventsWithinSubtree(false);
    SetTooltipText(NomeDoSite(site));
    GetViewAccessibility().SetName(NomeDoSite(site));
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }

  const Site& site() const { return site_; }

  void Mostrar(bool aberta, bool ativa, const ui::ImageModel& icone) {
    aberta_ = aberta;
    ativa_ = ativa;
    icone_->SetImage(icone);
    SchedulePaint();
  }

  // Apertado, encolhe um pouco, como uma tecla; com "menos movimento" no
  // sistema, não.
  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    const bool apertado = GetState() == STATE_PRESSED;
    if (apertado == (old_state == STATE_PRESSED) ||
        !gfx::Animation::ShouldRenderRichAnimation()) {
      return;
    }
    if (!layer()) {
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);
    }
    views::AnimationBuilder()
        .Once()
        .SetDuration(base::Milliseconds(100))
        .SetTransform(this,
                      apertado ? gfx::GetScaleTransform(
                                     GetLocalBounds().CenterPoint(), 0.96f)
                               : gfx::Transform(),
                      gfx::Tween::EASE_OUT);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorProvider* cores = GetColorProvider();
    if (!cores) {
      return;
    }
    const bool escuro =
        color_utils::IsDark(cores->GetColor(ui::kColorFrameActive));
    // Parado < hover < ativo, em cinza (o hover acendia mais que o ativo; e
    // no Resenha fundo de linha não leva cor). O ativo também tem o contorno
    // mais forte.
    SkColor fundo = escuro ? SkColorSetARGB(0x0D, 0xFF, 0xFF, 0xFF)
                           : SkColorSetARGB(0x99, 0xFF, 0xFF, 0xFF);
    if (ativa_) {
      fundo = escuro ? SkColorSetARGB(0x24, 0xFF, 0xFF, 0xFF) : SK_ColorWHITE;
    } else if (IsMouseHovered()) {
      fundo = escuro ? SkColorSetARGB(0x17, 0xFF, 0xFF, 0xFF)
                     : SkColorSetARGB(0xC2, 0xFF, 0xFF, 0xFF);
    }
    const gfx::RectF caixa(GetLocalBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(fundo);
    canvas->DrawRoundRect(caixa, kRaio, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(escuro ? SkColorSetARGB(ativa_ ? 0x2E : 0x14, 0xFF, 0xFF,
                                           0xFF)
                          : SkColorSetARGB(ativa_ ? 0x26 : 0x14, 0x00, 0x00,
                                           0x00));
    gfx::RectF borda = caixa;
    borda.Inset(0.5f);
    canvas->DrawRoundRect(borda, kRaio, flags);
    // Aberto numa aba (e não é a ativa): um ponto embaixo, como num dock.
    if (aberta_ && !ativa_) {
      cc::PaintFlags ponto;
      ponto.setAntiAlias(true);
      ponto.setColor(cores->GetColor(kColorTabForegroundInactiveFrameActive));
      canvas->DrawCircle(gfx::PointF(width() / 2.0f, height() - 4.0f), 1.5f,
                         ponto);
    }
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    SchedulePaint();
  }

 private:
  Site site_;
  bool aberta_ = false;
  bool ativa_ = false;
  raw_ptr<views::ImageView> icone_ = nullptr;
};

BEGIN_METADATA(Ladrilho)
END_METADATA

// Sem favoritos no espaço: o quadro tracejado com o alfinete do Dia. Clicar
// põe a aba atual nos favoritos.
class SemFavoritos : public views::Button {
  METADATA_HEADER(SemFavoritos, views::Button)

 public:
  explicit SemFavoritos(base::RepeatingClosure favoritar)
      : views::Button(std::move(favoritar)) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    icone_ = AddChildView(std::make_unique<views::ImageView>());
    icone_->SetCanProcessEventsWithinSubtree(false);
    SetPreferredSize(gfx::Size(0, 40));
    SetTooltipText(u"Adicionar a aba atual aos favoritos");
    GetViewAccessibility().SetName(u"Adicionar a aba atual aos favoritos");
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    icone_->SetImage(ui::ImageModel::FromVectorIcon(
        views::kKeepIcon, kColorTabForegroundInactiveFrameActive, 16));
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const SkColor cor = GetColorProvider()->GetColor(
        kColorTabForegroundInactiveFrameActive);
    gfx::RectF caixa(GetLocalBounds());
    caixa.Inset(0.5f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (IsMouseHovered()) {
      flags.setColor(SkColorSetA(cor, 0x10));
      canvas->DrawRoundRect(caixa, kRaio, flags);
    }
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(SkColorSetA(cor, 0x66));
    const SkScalar tracos[] = {3, 3};
    flags.setPathEffect(cc::PathEffect::MakeDash(tracos, 2, 0));
    canvas->DrawRoundRect(caixa, kRaio, flags);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    SchedulePaint();
  }

 private:
  raw_ptr<views::ImageView> icone_ = nullptr;
};

BEGIN_METADATA(SemFavoritos)
END_METADATA

// Os favoritos em grade de 3 colunas (como no Dia), 6 entre eles.
class GradeDeFavoritos : public views::View {
  METADATA_HEADER(GradeDeFavoritos, views::View)

 public:
  static constexpr int kColunas = 3;
  static constexpr int kAltura = 41;
  static constexpr int kFolga = 6;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int linhas = (static_cast<int>(children().size()) + kColunas - 1) /
                       kColunas;
    return gfx::Size(
        kColunas * 54 + (kColunas - 1) * kFolga,
        linhas == 0 ? 0 : linhas * kAltura + (linhas - 1) * kFolga);
  }

  void Layout(PassKey) override {
    const int largura = (width() - (kColunas - 1) * kFolga) / kColunas;
    int i = 0;
    for (views::View* filho : children()) {
      const int linha = i / kColunas;
      const int coluna = i % kColunas;
      filho->SetBounds(coluna * (largura + kFolga), linha * (kAltura + kFolga),
                       largura, kAltura);
      ++i;
    }
  }
};

BEGIN_METADATA(GradeDeFavoritos)
END_METADATA

// A linha entre os fixados e as abas. Com o ponteiro em cima, um "Limpar"
// no fim dela fecha as abas do espaço (os fixados ficam), como no Dia e no
// Zen.
class Divisor : public views::View {
  METADATA_HEADER(Divisor, views::View)

 public:
  explicit Divisor(BrowserWindowInterface* browser) {
    SetPreferredSize(gfx::Size(0, kAlturaDivisor));
    SetNotifyEnterExitOnChild(true);
    limpar_ = AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&espacos::FecharAbasDoEspaco,
                            base::Unretained(browser)),
        u"Limpar"));
    limpar_->SetTooltipText(u"Fechar as abas deste espaço (os fixados ficam)");
    limpar_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 4)));
    limpar_->SetLabelStyle(views::style::STYLE_BODY_5);
    limpar_->SetVisible(false);
  }

  void Layout(PassKey) override {
    const gfx::Size tamanho = limpar_->GetPreferredSize();
    limpar_->SetBounds(width() - kMargemDivisor - tamanho.width(),
                       (height() - tamanho.height()) / 2, tamanho.width(),
                       tamanho.height());
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    limpar_->SetVisible(true);
    SchedulePaint();
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    limpar_->SetVisible(false);
    SchedulePaint();
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    limpar_->SetEnabledTextColors(SkColorSetA(
        GetColorProvider()->GetColor(kColorTabForegroundInactiveFrameActive),
        0xB3));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor cor = GetColorProvider()->GetColor(
        kColorTabForegroundInactiveFrameActive);
    const int fim = limpar_->GetVisible() ? limpar_->x() - 4
                                          : width() - kMargemDivisor;
    canvas->FillRect(
        gfx::Rect(kMargemDivisor, kAlturaDivisor / 2, fim - kMargemDivisor, 1),
        SkColorSetA(cor, 0x33));
  }

 private:
  raw_ptr<views::LabelButton> limpar_ = nullptr;
};

BEGIN_METADATA(Divisor)
END_METADATA

// Sem nada fixado no espaço, enquanto uma aba é arrastada pela lateral: o
// lugar de soltar pra fixar (tracejado, como o quadro dos favoritos).
class ZonaDeFixar : public views::View {
  METADATA_HEADER(ZonaDeFixar, views::View)

 public:
  ZonaDeFixar() {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    texto_ = AddChildView(std::make_unique<views::Label>(u"Solte aqui pra fixar"));
    texto_->SetFontList(gfx::FontList().DeriveWithSizeDelta(
        12 - gfx::FontList().GetFontSize()));
    texto_->SetCanProcessEventsWithinSubtree(false);
    SetPreferredSize(gfx::Size(0, kAlturaLinha));
    SetProperty(views::kMarginsKey,
                gfx::Insets::TLBR(0, 0, visual::kEspacoEntreAbas, 0));
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    texto_->SetEnabledColor(SkColorSetA(
        GetColorProvider()->GetColor(kColorTabForegroundInactiveFrameActive),
        0xB3));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor cor = GetColorProvider()->GetColor(
        kColorTabForegroundInactiveFrameActive);
    gfx::RectF caixa(GetLocalBounds());
    caixa.Inset(0.5f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(SkColorSetA(cor, 0x66));
    const SkScalar tracos[] = {3, 3};
    flags.setPathEffect(cc::PathEffect::MakeDash(tracos, 2, 0));
    canvas->DrawRoundRect(caixa, kRaio, flags);
  }

 private:
  raw_ptr<views::Label> texto_ = nullptr;
};

BEGIN_METADATA(ZonaDeFixar)
END_METADATA

class AreaDeFixos : public views::View,
                    public TabStripModelObserver,
                    public views::ContextMenuController,
                    public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(AreaDeFixos, views::View)

 public:
  AreaDeFixos(BrowserWindowInterface* browser, views::View* lateral)
      : browser_(browser),
        model_(browser->GetTabStripModel()),
        lateral_(lateral) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        visual::kEspacoEntreAbas));
    set_context_menu_controller(this);
    AddPreTargetHandler(&arrasto_);
    model_->AddObserver(this);
    Areas()[browser_] = this;
    prefs_.Init(Perfil()->GetPrefs());
    prefs_.Add(kFixosPref,
               base::BindRepeating(&AreaDeFixos::ReconstruirDepois,
                                   base::Unretained(this)));
    prefs_.Add(espacos::kListaPref,
               base::BindRepeating(&AreaDeFixos::ReconstruirDepois,
                                   base::Unretained(this)));
    prefs_.Add(espacos::kQuadroDeFavoritosPref,
               base::BindRepeating(&AreaDeFixos::ReconstruirDepois,
                                   base::Unretained(this)));
    Reconstruir();
  }

  ~AreaDeFixos() override {
    RemovePreTargetHandler(&arrasto_);
    model_->RemoveObserver(this);
    auto it = Areas().find(browser_);
    if (it != Areas().end() && it->second == this) {
      Areas().erase(it);
    }
  }

  // Do menu da aba ("Fixar no espaço"): `destino` vazio = solto; id de
  // pasta; kNovaPasta; kDesafixar.
  void FixarAba(int index, const std::string& destino) {
    if (index < 0 || index >= model_->count()) {
      return;
    }
    content::WebContents* aba = model_->GetWebContentsAt(index);
    if (destino == kDesafixar) {
      if (std::optional<std::string> id = SiteDaAba(aba)) {
        Desligar(aba);
        Remover(Perfil(), *id);
      }
      return;
    }
    if (destino == kNovaPasta) {
      PedirNome(browser_, u"Nova pasta", u"Pasta",
                base::BindOnce(&AreaDeFixos::FixarNaPastaNova,
                               weak_factory_.GetWeakPtr(),
                               model_->GetTabAtIndex(index)->GetHandle()));
      return;
    }
    if (model_->IsTabPinned(index)) {
      index = model_->SetTabPinned(index, false);
    }
    const std::string id =
        Fixar(Perfil(), EspacoAtual(), EnderecoDaAba(aba),
              TituloParaFixar(aba), destino);
    if (!destino.empty()) {
      AbrirPasta(Perfil(), destino, true);
    }
    Ligar(aba, id);
    InvalidarLista();
    Reconstruir();
  }

  // "Adicionar aos favoritos" / "Tirar dos favoritos" do menu da aba. Aba
  // fixada do Chromium (ladrilho antigo) segue o caminho do Chromium.
  bool AlternarFavoritoDaAba(content::WebContents* aba) {
    const int index = model_->GetIndexOfWebContents(aba);
    if (index == TabStripModel::kNoTab || model_->IsTabPinned(index)) {
      return false;
    }
    if (std::optional<std::string> id = SiteDaAba(aba);
        id && EFavorito(Perfil(), *id)) {
      Desligar(aba);
      Remover(Perfil(), *id);
      return true;
    }
    const std::string id =
        Favoritar(Perfil(), EspacoAtual(), EnderecoDaAba(aba),
                  TituloParaFixar(aba));
    Ligar(aba, id);
    InvalidarLista();
    Reconstruir();
    return true;
  }

  bool TemAba(content::WebContents* aba) const {
    return model_->GetIndexOfWebContents(aba) != TabStripModel::kNoTab;
  }

  // --- views::View ---------------------------------------------------------

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    AtualizarEstados();
  }

  // --- Arrastar pra reorganizar (sites, pastas e favoritos) -----------------

  void Arrastar(ui::MouseEvent* event) {
    // O campo de renomear fica de fora (selecionar texto não arrasta).
    if (views::IsViewClass<views::Textfield>(
            static_cast<views::View*>(event->target()))) {
      return;
    }
    gfx::Point ponto = event->location();
    views::View::ConvertPointToTarget(
        static_cast<views::View*>(event->target()), this, &ponto);
    switch (event->type()) {
      case ui::EventType::kMousePressed:
        CancelarArrasto();
        if (event->IsOnlyLeftMouseButton()) {
          arrastado_ = ArrastavelEm(ponto);
          inicio_ = ponto;
        }
        return;
      case ui::EventType::kMouseDragged:
        if (!arrastado_) {
          return;
        }
        if (!arrastando_) {
          if ((ponto - inicio_).Length() < 6) {
            return;
          }
          arrastando_ = true;
          // O clique não vale mais: soltar não abre o site.
          if (auto* botao = views::Button::AsButton(arrastado_)) {
            botao->SetState(views::Button::STATE_NORMAL);
          }
        }
        CalcularDestino(ponto);
        SchedulePaint();
        event->SetHandled();
        return;
      case ui::EventType::kMouseReleased:
        if (arrastando_) {
          event->SetHandled();
          Soltar();
        }
        CancelarArrasto();
        return;
      case ui::EventType::kMouseCaptureChanged:
        CancelarArrasto();
        return;
      default:
        return;
    }
  }

  void PaintChildren(const views::PaintInfo& info) override {
    views::View::PaintChildren(info);
    const bool interno = arrastando_ && arrastado_;
    if (!interno && !vizinho_) {
      return;
    }
    ui::PaintRecorder recorder(info.context(), size());
    gfx::Canvas* canvas = recorder.canvas();
    const SkColor cor = GetColorProvider()->GetColor(ui::kColorAccent);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    // O que está sendo arrastado: contorno tracejado.
    if (interno) {
      gfx::RectF origem(AreaDe(arrastado_));
      origem.Inset(1);
      cc::PaintFlags traco;
      traco.setAntiAlias(true);
      traco.setStyle(cc::PaintFlags::kStroke_Style);
      traco.setStrokeWidth(1);
      traco.setColor(SkColorSetA(cor, 0x99));
      const SkScalar intervalos[] = {3, 3};
      traco.setPathEffect(cc::PathEffect::MakeDash(intervalos, 2, 0));
      canvas->DrawRoundRect(origem, kRaio, traco);
    }
    if (!vizinho_) {
      return;
    }
    const gfx::Rect area = AreaDe(vizinho_);
    flags.setColor(cor);
    if (onde_ == Onde::kDentro) {
      gfx::RectF caixa(area);
      caixa.Inset(1);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(2);
      canvas->DrawRoundRect(caixa, kRaio, flags);
    } else if (views::IsViewClass<Ladrilho>(vizinho_)) {
      const float x =
          onde_ == Onde::kAntes ? area.x() - 4.0f : area.right() + 2.0f;
      canvas->DrawRoundRect(
          gfx::RectF(x, area.y() + 6, 2, area.height() - 12), 1, flags);
    } else {
      // Recuada como a linha (dentro das pastas de dentro, mais).
      auto nivel = niveis_.find(vizinho_);
      const int recuo =
          nivel == niveis_.end() ? 0 : nivel->second * kRecuoDaPasta;
      const float y = onde_ == Onde::kAntes ? area.y() - 3.0f
                                            : area.bottom() + 1.0f;
      canvas->DrawRoundRect(
          gfx::RectF(area.x() + 6 + recuo, y, area.width() - 12 - recuo, 2), 1,
          flags);
    }
  }

  // --- Aba da lista arrastada pra cá (AlvoDosFixos) -------------------------

  TabDragTarget* AlvoEm(const gfx::Point& ponto_na_tela) {
    const TabDragController* arrasto = TabDragController::NavegadorAtual();
    if (!arrasto) {
      return nullptr;
    }
    TabDragContext* const anexado =
        const_cast<TabDragContext*>(arrasto->GetAttachedContext());
    const DragSessionData& dados = arrasto->GetSessionData();
    // Uma aba comum desta janela: não um espaço, uma divisão, uma fixada do
    // Chromium nem uma que já é de um site.
    if (!anexado || anexado->GetTabStripModel() != model_ ||
        dados.num_dragging_tabs() != 1 || !dados.dragging_groups.empty() ||
        dados.group_header_drag_data_.has_value() ||
        dados.source_view_index_ >= dados.tab_drag_data_.size()) {
      return nullptr;
    }
    content::WebContents* aba = dados.source_dragged_contents();
    const int index =
        aba ? model_->GetIndexOfWebContents(aba) : TabStripModel::kNoTab;
    if (index == TabStripModel::kNoTab || model_->IsTabPinned(index) ||
        model_->GetTabAtIndex(index)->IsSplit() || SiteDaAba(aba)) {
      return nullptr;
    }
    LigarZona();
    views::View* rolagem = parent();
    while (rolagem && rolagem->GetID() != visual::kIdAreaDeFixos) {
      rolagem = rolagem->parent();
    }
    if (!rolagem || !rolagem->IsDrawn() ||
        !rolagem->GetBoundsInScreen().Contains(ponto_na_tela)) {
      return nullptr;
    }
    return &alvo_de_aba_;
  }

  // --- views::ContextMenuController ----------------------------------------

  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    submenu_ = nullptr;
    alvo_.clear();
    pastas_.clear();
    const std::string id = IdDaLinha(source);
    std::string pasta_do_alvo;
    std::optional<Site> site =
        id.empty() ? std::nullopt
                   : SitePorId(Perfil(), id, nullptr, &pasta_do_alvo);
    alvo_ = id;
    if (site && EFavorito(Perfil(), id)) {
      content::WebContents* aba = AbaDoSite(id);
      if (aba && EnderecoDaAba(aba) != site->url) {
        menu_->AddItem(kVoltarAoEndereco, u"Voltar ao endereço fixado");
        menu_->AddItem(kUsarEndereco, u"Usar o endereço atual");
      }
      if (aba) {
        menu_->AddItem(kFecharAba, u"Fechar a aba");
      }
      menu_->AddItem(kSubir, u"Mover pra esquerda");
      menu_->AddItem(kDescer, u"Mover pra direita");
      menu_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_->AddItem(kDesafixarCmd, u"Tirar dos favoritos");
    } else if (site) {
      content::WebContents* aba = AbaDoSite(id);
      menu_->AddItem(kRenomear, u"Renomear…");
      if (aba && EnderecoDaAba(aba) != site->url) {
        menu_->AddItem(kVoltarAoEndereco, u"Voltar ao endereço fixado");
        menu_->AddItem(kUsarEndereco, u"Usar o endereço atual");
      }
      if (aba) {
        menu_->AddItem(kFecharAba, u"Fechar a aba");
      }
      menu_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_->AddItem(kSubir, u"Mover pra cima");
      menu_->AddItem(kDescer, u"Mover pra baixo");
      AdicionarMoverParaPasta(id, pasta_do_alvo);
      menu_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_->AddItem(kNovaPastaCmd, u"Nova pasta…");
      menu_->AddItem(kDesafixarCmd, u"Desafixar");
    } else if (!id.empty()) {
      // Pasta.
      menu_->AddItem(kRenomear, u"Renomear pasta…");
      menu_->AddItem(kAbrirTodas, u"Abrir todos os sites");
      menu_->AddItem(kFecharTodas, u"Fechar as abas da pasta");
      menu_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_->AddItem(kSubir, u"Mover pra cima");
      menu_->AddItem(kDescer, u"Mover pra baixo");
      AdicionarMoverParaPasta(id, pasta_do_alvo);
      menu_->AddSeparator(ui::NORMAL_SEPARATOR);
      int nivel = 0;
      for (const PastaNoMenu& pasta : PastasDoEspaco(Perfil(), EspacoAtual())) {
        if (pasta.id == id) {
          nivel = pasta.nivel;
        }
      }
      if (nivel + 2 <= kNiveisDePasta) {
        menu_->AddItem(kNovaSubpasta, u"Nova pasta aqui dentro…");
      }
      menu_->AddItem(kNovaPastaCmd, u"Nova pasta…");
      menu_->AddItem(kExcluirPasta, u"Excluir a pasta");
    } else {
      if (views::IsViewClass<SemFavoritos>(source)) {
        menu_->AddItem(kOcultarQuadro, u"Ocultar este quadro");
        menu_->AddSeparator(ui::NORMAL_SEPARATOR);
      }
      menu_->AddItem(kNovaPastaCmd, u"Nova pasta…");
    }
    runner_ = std::make_unique<views::MenuRunner>(
        menu_.get(), views::MenuRunner::CONTEXT_MENU);
    runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                       views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // "Mover para a pasta ›" de um site ou de uma pasta: as pastas do espaço
  // (as de dentro com o caminho), menos a em que ele já está e, pra uma
  // pasta, ela mesma, as de dentro dela e as fundas demais.
  void AdicionarMoverParaPasta(const std::string& id,
                               const std::string& pasta_do_alvo) {
    submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
    if (!pasta_do_alvo.empty()) {
      submenu_->AddItem(kForaDaPasta, u"Fora das pastas");
    }
    for (const PastaNoMenu& pasta : PastasDoEspaco(Perfil(), EspacoAtual())) {
      if (pasta.id == pasta_do_alvo || !CabeEm(Perfil(), id, pasta.id)) {
        continue;
      }
      submenu_->AddItemWithIcon(
          kPrimeiraPasta + pastas_.size(), pasta.caminho,
          ui::ImageModel::FromVectorIcon(vector_icons::kFolderFlippableIcon,
                                         ui::kColorMenuIcon, 16));
      pastas_.push_back(pasta.id);
    }
    if (submenu_->GetItemCount() > 0) {
      menu_->AddSubMenu(kMoverParaPasta, u"Mover para a pasta",
                        submenu_.get());
    }
  }

  // --- ui::SimpleMenuModel::Delegate ---------------------------------------

  void ExecuteCommand(int command_id, int event_flags) override {
    Profile* perfil = Perfil();
    const std::string id = alvo_;
    switch (command_id) {
      case kRenomear: {
        std::optional<Site> site = SitePorId(perfil, id);
        std::u16string atual;
        const std::vector<Entrada> entradas = Do(perfil, EspacoAtual());
        if (site) {
          atual = NomeDoSite(*site);
        } else if (const Entrada* pasta = PastaPorId(entradas, id)) {
          atual = base::UTF8ToUTF16(pasta->nome);
        }
        PedirNome(browser_, site ? u"Renomear" : u"Renomear pasta", atual,
                  base::BindOnce(
                      [](Profile* perfil, std::string id, bool e_site,
                         const std::string& nome) {
                        if (e_site) {
                          RenomearSite(perfil, id, nome);
                        } else {
                          RenomearPasta(perfil, id, nome);
                        }
                      },
                      perfil, id, site.has_value()));
        return;
      }
      case kUsarEndereco:
        if (content::WebContents* aba = AbaDoSite(id)) {
          TrocarEndereco(perfil, id, EnderecoDaAba(aba));
        }
        return;
      case kVoltarAoEndereco:
        // A aba ligada volta pro endereço do site (como o "Back to Pinned"
        // do Dia), no histórico dela.
        if (content::WebContents* aba = AbaDoSite(id)) {
          if (std::optional<Site> fixado = SitePorId(perfil, id)) {
            aba->GetController().LoadURL(fixado->url, content::Referrer(),
                                         ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                         std::string());
          }
        }
        return;
      case kFecharAba:
        FecharAbaDoSite(id);
        return;
      case kOcultarQuadro:
        // Volta pelas Configurações do Navegador (Geral).
        perfil->GetPrefs()->SetBoolean(espacos::kQuadroDeFavoritosPref, false);
        return;
      case kSubir:
        Mover(perfil, id, -1);
        return;
      case kDescer:
        Mover(perfil, id, 1);
        return;
      case kForaDaPasta:
        MoverParaPasta(perfil, id, "");
        return;
      case kNovaPastaCmd:
      case kNovaSubpasta:
        PedirNome(browser_, u"Nova pasta", u"Pasta",
                  base::BindOnce(
                      [](Profile* perfil, std::string espaco,
                         std::string dentro, const std::string& nome) {
                        CriarPasta(perfil, espaco, nome, dentro);
                      },
                      perfil, EspacoAtual(),
                      command_id == kNovaSubpasta ? id : std::string()));
        return;
      case kDesafixarCmd:
        if (content::WebContents* aba = AbaDoSite(id)) {
          Desligar(aba);
        }
        Remover(perfil, id);
        return;
      case kAbrirTodas:
      case kFecharTodas: {
        // Os das pastas de dentro também.
        const std::vector<Entrada> entradas = Do(perfil, EspacoAtual());
        if (const Entrada* pasta = PastaPorId(entradas, id)) {
          for (const Site& site : SitesDaPasta(*pasta)) {
            if (command_id == kFecharTodas) {
              FecharAbaDoSite(site.id);
            } else if (!AbaDoSite(site.id)) {
              AbrirSite(site, /*ativar=*/false);
            }
          }
        }
        return;
      }
      case kExcluirPasta: {
        // As abas abertas dos sites da pasta (e das de dentro dela)
        // continuam, como abas comuns.
        const std::vector<Entrada> entradas = Do(perfil, EspacoAtual());
        if (const Entrada* pasta = PastaPorId(entradas, id)) {
          for (const Site& site : SitesDaPasta(*pasta)) {
            if (content::WebContents* aba = AbaDoSite(site.id)) {
              Desligar(aba);
            }
          }
        }
        Remover(perfil, id);
        return;
      }
    }
    const size_t i = command_id - kPrimeiraPasta;
    if (i < pastas_.size()) {
      MoverParaPasta(perfil, id, pastas_[i]);
      AbrirPasta(perfil, pastas_[i], true);
    }
  }

  // --- TabStripModelObserver -----------------------------------------------

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kSelectionOnly ||
        selection.active_tab_changed()) {
      AtualizarDepois();
    }
    // Pasta fechada mostra o site da aba ativa: muda com ela.
    if (selection.active_tab_changed() && tem_pasta_fechada_) {
      ReconstruirDepois();
    }
  }

  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override {
    if (tab && SiteDaAba(tab->GetContents())) {
      AtualizarDepois();
    }
  }

  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override {
    ReconstruirDepois();
  }

  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override {
    AtualizarDepois();
  }

  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override {
    AtualizarDepois();
  }

 private:
  Profile* Perfil() const { return browser_->GetProfile(); }

  std::string EspacoAtual() const {
    if (std::optional<tab_groups::TabGroupId> grupo =
            model_->GetFocusedGroup()) {
      if (std::optional<std::string> espaco =
              espacos::EspacoDoGrupo(Perfil(), grupo->ToString())) {
        return *espaco;
      }
    }
    return espacos::Ativo(Perfil());
  }

  std::string IdDaLinha(views::View* linha) const {
    auto it = ids_.find(linha);
    return it == ids_.end() ? std::string() : it->second;
  }

  // Aba desta janela ligada ao site.
  content::WebContents* AbaDoSite(const std::string& id) const {
    for (int i = 0; i < model_->count(); ++i) {
      content::WebContents* aba = model_->GetWebContentsAt(i);
      if (SiteDaAba(aba) == id) {
        return aba;
      }
    }
    return nullptr;
  }

  void Desligar(content::WebContents* aba) {
    Ligar(aba, std::string());
    InvalidarLista();
  }

  void FecharAbaDoSite(const std::string& id) {
    if (content::WebContents* aba = AbaDoSite(id)) {
      const int index = model_->GetIndexOfWebContents(aba);
      if (index != TabStripModel::kNoTab) {
        // Vai pro histórico: Ctrl+Shift+T traz de volta (ligada ao site).
        model_->CloseWebContentsAt(
            index, TabCloseTypes::CLOSE_USER_GESTURE |
                       TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      }
    }
  }

  void FixarNaPastaNova(tabs::TabHandle aba, const std::string& nome) {
    tabs::TabInterface* tab = aba.Get();
    const int index = tab ? model_->GetIndexOfTab(tab) : TabStripModel::kNoTab;
    if (index == TabStripModel::kNoTab) {
      return;
    }
    FixarAba(index, CriarPasta(Perfil(), EspacoAtual(), nome));
  }

  // Clique no site: a aba dele (se aberta) ou uma aba nova ligada a ele, no
  // espaço atual (e no container do espaço).
  void Abrir(const std::string& id) {
    if (content::WebContents* aba = AbaDoSite(id)) {
      const int index = model_->GetIndexOfWebContents(aba);
      if (index != TabStripModel::kNoTab) {
        model_->ActivateTabAt(
            index, TabStripUserGestureDetails(
                       TabStripUserGestureDetails::GestureType::kOther));
      }
      return;
    }
    if (std::optional<Site> site = SitePorId(Perfil(), id)) {
      AbrirSite(*site, /*ativar=*/true);
    }
  }

  void AbrirSite(const Site& site, bool ativar) {
    NavigateParams params(browser_, site.url,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = ativar ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    Navigate(&params);
    content::WebContents* aba = params.navigated_or_inserted_contents;
    if (!aba) {
      return;
    }
    // Liga antes do próximo layout: a aba já nasce fora da lista.
    Ligar(aba, site.id);
    // No grupo do espaço atual (o Navigate pode pôr a aba solta).
    const int index = model_->GetIndexOfWebContents(aba);
    if (index != TabStripModel::kNoTab) {
      if (std::optional<tab_groups::TabGroupId> grupo =
              model_->GetFocusedGroup();
          grupo && model_->GetTabGroupForTab(index) != grupo) {
        model_->AddToExistingGroup({index}, *grupo, /*add_to_end=*/true);
      }
    }
    InvalidarLista();
    AtualizarDepois();
  }

  // A lista de abas (TabGroupView do espaço) esconde as abas ligadas: depois
  // de ligar ou desligar, refaz o layout dela.
  void InvalidarLista() {
    views::View* raiz = lateral_;
    std::vector<views::View*> pilha = {raiz};
    while (!pilha.empty()) {
      views::View* view = pilha.back();
      pilha.pop_back();
      view->InvalidateLayout();
      for (views::View* filho : view->children()) {
        pilha.push_back(filho);
      }
    }
  }

  void ReconstruirDepois() {
    if (reconstrucao_pendente_) {
      return;
    }
    reconstrucao_pendente_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AreaDeFixos::Reconstruir,
                                  weak_factory_.GetWeakPtr()));
  }

  void AtualizarDepois() {
    if (atualizacao_pendente_) {
      return;
    }
    atualizacao_pendente_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AreaDeFixos::AtualizarEstados,
                                  weak_factory_.GetWeakPtr()));
  }

  void Reconstruir() {
    reconstrucao_pendente_ = false;
    CancelarArrasto();
    RemoveAllChildViews();
    linhas_.clear();
    ladrilhos_.clear();
    ids_.clear();
    niveis_.clear();
    pais_.clear();
    quadro_ = nullptr;
    divisor_ = nullptr;
    zona_ = nullptr;
    const std::vector<Site> favoritos = Favoritos(Perfil(), EspacoAtual());
    if (favoritos.empty()) {
      if (espacos::MostrarQuadroDeFavoritos(Perfil())) {
        quadro_ = AddChildView(std::make_unique<SemFavoritos>(base::BindRepeating(
            [](AreaDeFixos* area) {
              if (content::WebContents* aba =
                      area->model_->GetActiveWebContents()) {
                area->AlternarFavoritoDaAba(aba);
              }
            },
            base::Unretained(this))));
        quadro_->set_context_menu_controller(this);
      }
    } else {
      auto* grade = AddChildView(std::make_unique<GradeDeFavoritos>());
      grade->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 3, 0));
      for (const Site& site : favoritos) {
        auto* ladrilho = grade->AddChildView(std::make_unique<Ladrilho>(
            site, base::BindRepeating(&AreaDeFixos::Abrir,
                                      base::Unretained(this), site.id)));
        ladrilho->set_context_menu_controller(this);
        ladrilhos_.push_back(ladrilho);
        ids_[ladrilho] = site.id;
      }
    }
    const std::vector<Entrada> entradas = Do(Perfil(), EspacoAtual());
    tem_pasta_fechada_ = false;
    AdicionarEntradas(entradas, /*nivel=*/0, /*pasta=*/std::string(),
                      SiteDaAba(model_->GetActiveWebContents()));
    if (!entradas.empty()) {
      divisor_ = AddChildView(std::make_unique<Divisor>(browser_));
    } else if (zona_ligada_) {
      // Aba sendo arrastada na janela sem nada fixado: o lugar de soltar.
      zona_ = AddChildView(std::make_unique<ZonaDeFixar>());
    }
    // Sem a linha no fim, a folga de uma aba até a lista.
    SetBorder(entradas.empty() ? views::CreateEmptyBorder(gfx::Insets::TLBR(
                                     0, 0, visual::kEspacoEntreAbas, 0))
                               : nullptr);
    AtualizarEstados();
    PreferredSizeChanged();
    // Renomear pasta pedido antes da reconstrução (ver EditarPasta).
    if (!edicao_pendente_.empty()) {
      EditarPastaAgora(std::exchange(edicao_pendente_, std::string()));
    }
  }

  // --- Renomear no lugar (clique duplo) -------------------------------------

  // Cada clique (esquerdo) num site ou pasta passa aqui: o segundo no mesmo
  // item, dentro do tempo do clique duplo, abre o campo de renomear.
  bool CliqueDuplo(const std::string& id, bool pasta) {
    const base::TimeTicks agora = base::TimeTicks::Now();
    const bool duplo =
        id == ultimo_clique_ &&
        agora - quando_ultimo_clique_ <=
            base::Milliseconds(ui::kDoubleClickTimeMs);
    ultimo_clique_ = duplo ? std::string() : id;
    quando_ultimo_clique_ = agora;
    if (duplo) {
      if (pasta) {
        EditarPasta(id);
      } else {
        EditarSite(id);
      }
    }
    return duplo;
  }

  void EditarSite(const std::string& id) {
    for (LinhaDoSite* linha : linhas_) {
      if (ids_[linha] == id) {
        abas::EditarNoLugar(
            linha, linha->rotulo(), linha->rotulo().GetText(),
            base::BindOnce(
                [](Profile* perfil, std::string id, std::u16string nome) {
                  // Vazio volta o endereço (NomeDoSite).
                  RenomearSite(perfil, id, base::UTF16ToUTF8(nome));
                },
                Perfil(), id));
        return;
      }
    }
  }

  // O primeiro clique do duplo já abriu (ou fechou) a pasta e pediu pra
  // reconstruir: desfaz, e o campo entra no cabeçalho novo.
  void EditarPasta(const std::string& id) {
    const std::vector<Entrada> entradas = Do(Perfil(), EspacoAtual());
    if (const Entrada* pasta = PastaPorId(entradas, id)) {
      edicao_pendente_ = id;
      AbrirPasta(Perfil(), id, !pasta->aberta);
      if (!reconstrucao_pendente_) {
        EditarPastaAgora(std::exchange(edicao_pendente_, std::string()));
      }
    }
  }

  void EditarPastaAgora(const std::string& id) {
    for (views::View* filho : children()) {
      auto* cabecalho = views::AsViewClass<CabecalhoDaPasta>(filho);
      if (cabecalho && ids_[cabecalho] == id) {
        abas::EditarNoLugar(
            cabecalho, cabecalho->rotulo(), cabecalho->rotulo().GetText(),
            base::BindOnce(
                [](Profile* perfil, std::string id, std::u16string nome) {
                  if (!nome.empty()) {
                    RenomearPasta(perfil, id, base::UTF16ToUTF8(nome));
                  }
                },
                Perfil(), id));
        return;
      }
    }
  }

  // Os itens de uma lista (a do espaço ou a de dentro de uma pasta), com as
  // pastas de dentro recuadas mais um passo cada.
  void AdicionarEntradas(const std::vector<Entrada>& entradas,
                         int nivel,
                         const std::string& pasta,
                         const std::optional<std::string>& site_ativo) {
    for (const Entrada& entrada : entradas) {
      if (!entrada.pasta) {
        AdicionarSite(entrada.site, nivel, pasta);
        continue;
      }
      tem_pasta_fechada_ |= !entrada.aberta;
      auto* cabecalho = AddChildView(std::make_unique<CabecalhoDaPasta>(
          entrada, nivel * kRecuoDaPasta,
          base::BindRepeating(
              [](Profile* perfil, std::string id, bool aberta) {
                AbrirPasta(perfil, id, !aberta);
              },
              Perfil(), entrada.id, entrada.aberta),
          base::BindRepeating(&AreaDeFixos::CliqueDuplo,
                              base::Unretained(this), entrada.id,
                              /*pasta=*/true)));
      cabecalho->set_context_menu_controller(this);
      ids_[cabecalho] = entrada.id;
      niveis_[cabecalho] = nivel;
      pais_[cabecalho] = pasta;
      if (entrada.aberta) {
        AdicionarEntradas(entrada.itens, nivel + 1, entrada.id, site_ativo);
      } else if (site_ativo) {
        // Pasta fechada: só o site da aba ativa fica à vista (como no Zen),
        // mesmo vindo de uma pasta de dentro dela.
        std::string de_quem;
        if (const Entrada* site = SiteNaPasta(entrada, *site_ativo, &de_quem)) {
          AdicionarSite(site->site, nivel + 1, de_quem);
        }
      }
    }
  }

  // O site `id` dentro da pasta (em qualquer nível); `pasta` recebe a pasta
  // em que ele está.
  static const Entrada* SiteNaPasta(const Entrada& de_fora,
                                    const std::string& id,
                                    std::string* pasta) {
    for (const Entrada& item : de_fora.itens) {
      if (!item.pasta && item.id == id) {
        *pasta = de_fora.id;
        return &item;
      }
      if (item.pasta) {
        if (const Entrada* achado = SiteNaPasta(item, id, pasta)) {
          return achado;
        }
      }
    }
    return nullptr;
  }

  void AdicionarSite(const Site& site, int nivel, const std::string& pasta) {
    auto* linha = AddChildView(std::make_unique<LinhaDoSite>(
        site, nivel * kRecuoDaPasta,
        base::BindRepeating(&AreaDeFixos::Abrir, base::Unretained(this),
                            site.id),
        base::BindRepeating(&AreaDeFixos::FecharAbaDoSite,
                            base::Unretained(this), site.id),
        base::BindRepeating(&AreaDeFixos::CliqueDuplo, base::Unretained(this),
                            site.id, /*pasta=*/false)));
    linha->set_context_menu_controller(this);
    linhas_.push_back(linha);
    ids_[linha] = site.id;
    niveis_[linha] = nivel;
    pais_[linha] = pasta;
  }

  // Aba aberta/ativa e ícone de cada site; e arruma as ligações: aba ligada
  // a site que não existe mais (ou que é de outro espaço) vira aba comum, e
  // duas abas no mesmo site ficam só com a primeira.
  void AtualizarEstados() {
    atualizacao_pendente_ = false;
    Profile* perfil = Perfil();
    // Converter mexe no TabStripModel, e isto roda também no OnThemeChanged,
    // que vem no meio de notificações dele (AddTab, ActivateTab): mexer ali
    // dá CHECK de reentrância. Fica pra uma tarefa.
    if (model_->IndexOfFirstNonPinnedTab() > 0) {
      ConverterDepois();
    }
    std::map<std::string, content::WebContents*> abertas;
    bool mudou = false;
    for (int i = 0; i < model_->count(); ++i) {
      content::WebContents* aba = model_->GetWebContentsAt(i);
      std::optional<std::string> id = SiteDaAba(aba);
      if (!id) {
        continue;
      }
      std::string espaco;
      const bool existe = SitePorId(perfil, *id, &espaco).has_value();
      std::optional<std::string> espaco_da_aba;
      if (std::optional<tab_groups::TabGroupId> grupo =
              model_->GetTabGroupForTab(i)) {
        espaco_da_aba = espacos::EspacoDoGrupo(perfil, grupo->ToString());
      }
      // Na divisão de tela a aba aparece no par, na lista: vira aba comum
      // (senão aparecia duas vezes, ali e no lugar do site).
      tabs::TabInterface* tab = model_->GetTabAtIndex(i);
      if (!existe || abertas.contains(*id) || (tab && tab->IsSplit()) ||
          (espaco_da_aba && *espaco_da_aba != espaco)) {
        Ligar(aba, std::string());
        mudou = true;
        continue;
      }
      abertas[*id] = aba;
    }
    if (mudou) {
      InvalidarLista();
    }

    content::WebContents* ativa = model_->GetActiveWebContents();
    for (LinhaDoSite* linha : linhas_) {
      auto it = abertas.find(linha->site().id);
      content::WebContents* aba = it == abertas.end() ? nullptr : it->second;
      linha->Mostrar(aba != nullptr, aba && aba == ativa,
                     IconeDe(linha->site(), aba),
                     aba && aba->IsCurrentlyAudible());
    }
    for (Ladrilho* ladrilho : ladrilhos_) {
      auto it = abertas.find(ladrilho->site().id);
      content::WebContents* aba = it == abertas.end() ? nullptr : it->second;
      ladrilho->Mostrar(aba != nullptr, aba && aba == ativa,
                        IconeDe(ladrilho->site(), aba));
    }
  }

  void ConverterDepois() {
    if (conversao_pendente_) {
      return;
    }
    conversao_pendente_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AreaDeFixos::Converter,
                                  weak_factory_.GetWeakPtr()));
  }

  void Converter() {
    // No meio de um arrasto a aba ainda vai mudar de lugar (ou de janela).
    if (TabDragController::IsActive()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AreaDeFixos::Converter, weak_factory_.GetWeakPtr()),
          base::Milliseconds(300));
      return;
    }
    conversao_pendente_ = false;
    if (ConverterFixadasDoChromium()) {
      Reconstruir();
    }
  }

  // Aba fixada do jeito do Chromium (sessão do pacote anterior, ou aba
  // arrastada pra área dos ladrilhos) vira favorito do espaço atual: assim os
  // ladrilhos são sempre do espaço e iguais em todas as janelas.
  bool ConverterFixadasDoChromium() {
    std::vector<content::WebContents*> fixadas;
    for (int i = 0; i < model_->IndexOfFirstNonPinnedTab(); ++i) {
      fixadas.push_back(model_->GetWebContentsAt(i));
    }
    for (content::WebContents* aba : fixadas) {
      int index = model_->GetIndexOfWebContents(aba);
      if (index == TabStripModel::kNoTab) {
        continue;
      }
      std::string id;
      if (std::optional<std::string> atual = SiteDaAba(aba);
          atual && EFavorito(Perfil(), *atual)) {
        id = *atual;
      } else {
        id = Favoritar(Perfil(), EspacoAtual(), EnderecoDaAba(aba),
                       TituloParaFixar(aba));
      }
      Ligar(aba, id);
      index = model_->SetTabPinned(index, false);
      if (std::optional<tab_groups::TabGroupId> grupo =
              model_->GetFocusedGroup();
          grupo && model_->GetTabGroupForTab(index) != grupo) {
        model_->AddToExistingGroup({index}, *grupo, /*add_to_end=*/true);
      }
    }
    if (!fixadas.empty()) {
      InvalidarLista();
    }
    return !fixadas.empty();
  }

  // Ícone da aba aberta, ou o guardado no histórico de favicons.
  ui::ImageModel IconeDe(const Site& site, content::WebContents* aba) {
    ui::ImageModel icone;
    if (aba) {
      if (tabs::TabInterface* tab =
              tabs::TabInterface::MaybeGetFromContents(aba)) {
        if (TabUIHelper* ajudante = TabUIHelper::From(tab)) {
          icone = ajudante->GetFavicon();
        }
      }
      // Guarda pra quando a aba fechar (site que não estava no histórico
      // de favicons ficava com o globo).
      if (!icone.IsEmpty()) {
        icones_[site.url] = icone;
      }
    } else {
      auto guardado = icones_.find(site.url);
      if (guardado != icones_.end()) {
        icone = guardado->second;
      } else {
        BuscarIcone(site.url);
      }
    }
    if (icone.IsEmpty()) {
      icone = favicon::GetDefaultFaviconModel();
    }
    return icone;
  }

  // Ícone de site sem aba aberta: do histórico de favicons.
  void BuscarIcone(const GURL& url) {
    if (buscando_.contains(url)) {
      return;
    }
    favicon::FaviconService* servico = FaviconServiceFactory::GetForProfile(
        Perfil(), ServiceAccessType::EXPLICIT_ACCESS);
    if (!servico) {
      return;
    }
    buscando_.insert(url);
    servico->GetFaviconImageForPageURL(
        url,
        base::BindOnce(
            [](base::WeakPtr<AreaDeFixos> area, GURL url,
               const favicon_base::FaviconImageResult& resultado) {
              if (!area || resultado.image.IsEmpty()) {
                return;
              }
              area->icones_[url] =
                  ui::ImageModel::FromImage(resultado.image);
              area->AtualizarDepois();
            },
            weak_factory_.GetWeakPtr(), url),
        &rastreador_);
  }

  static constexpr int kRenomear = 1;
  static constexpr int kUsarEndereco = 2;
  static constexpr int kFecharAba = 3;
  static constexpr int kSubir = 4;
  static constexpr int kDescer = 5;
  static constexpr int kMoverParaPasta = 6;
  static constexpr int kForaDaPasta = 7;
  static constexpr int kNovaPastaCmd = 8;
  static constexpr int kDesafixarCmd = 9;
  static constexpr int kAbrirTodas = 10;
  static constexpr int kFecharTodas = 11;
  static constexpr int kExcluirPasta = 12;
  static constexpr int kOcultarQuadro = 13;
  static constexpr int kVoltarAoEndereco = 14;
  static constexpr int kNovaSubpasta = 15;
  static constexpr int kPrimeiraPasta = 100;

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<TabStripModel> model_;
  // A lateral (TabStripView): a lista de abas que esconde as ligadas é
  // filha dela, não da rolagem em que a área mora.
  raw_ptr<views::View> lateral_;
  PrefChangeRegistrar prefs_;
  std::vector<raw_ptr<LinhaDoSite>> linhas_;
  std::vector<raw_ptr<Ladrilho>> ladrilhos_;
  std::map<views::View*, std::string> ids_;
  // De cada linha de site ou pasta: quantas pastas em volta e a pasta em que
  // está (vazia = solta).
  std::map<views::View*, int> niveis_;
  std::map<views::View*, std::string> pais_;
  // Lugares de soltar uma aba arrastada da lista: o quadro dos favoritos, a
  // linha embaixo dos fixados e, sem nada fixado, a zona que aparece
  // durante o arrasto.
  raw_ptr<views::View> quadro_ = nullptr;
  raw_ptr<views::View> divisor_ = nullptr;
  raw_ptr<views::View> zona_ = nullptr;
  bool zona_ligada_ = false;
  std::map<GURL, ui::ImageModel> icones_;
  std::set<GURL> buscando_;
  base::CancelableTaskTracker rastreador_;
  std::string alvo_;
  std::vector<std::string> pastas_;
  std::unique_ptr<ui::SimpleMenuModel> menu_;
  std::unique_ptr<ui::SimpleMenuModel> submenu_;
  std::unique_ptr<views::MenuRunner> runner_;
  bool reconstrucao_pendente_ = false;
  // Pasta pra abrir o "renomear" depois da próxima reconstrução.
  std::string edicao_pendente_;
  // O último clique num site ou pasta (CliqueDuplo).
  std::string ultimo_clique_;
  base::TimeTicks quando_ultimo_clique_;
  bool atualizacao_pendente_ = false;
  bool conversao_pendente_ = false;
  bool tem_pasta_fechada_ = false;

  // Arrastar: o que, de onde, e pra perto de quem.
  enum class Onde { kAntes, kDepois, kDentro };
  class Arrasto : public ui::EventHandler {
   public:
    explicit Arrasto(AreaDeFixos* area) : area_(area) {}
    void OnMouseEvent(ui::MouseEvent* event) override {
      area_->Arrastar(event);
    }

   private:
    raw_ptr<AreaDeFixos> area_;
  };
  Arrasto arrasto_{this};
  raw_ptr<views::View> arrastado_ = nullptr;
  raw_ptr<views::View> vizinho_ = nullptr;
  Onde onde_ = Onde::kAntes;
  gfx::Point inicio_;
  bool arrastando_ = false;
  // Uma aba da lista está sendo arrastada por cima (sem `arrastado_`).
  bool externo_ = false;
  base::RepeatingTimer vigia_da_zona_;

  // O alvo do TabDragController pra aba da lista (BrowserView::
  // GetTabDragTarget, via AlvoDosFixos).
  class AlvoDeAba : public TabDragTarget {
   public:
    explicit AlvoDeAba(AreaDeFixos* area) : area_(area) {}
    ~AlvoDeAba() override { ao_destruir_.Notify(); }

    void OnTabDragEntered() override {}
    // A aba fica parada no lugar dela na lista enquanto passa por cima
    // (o ContinueDragging não move a aba com um alvo).
    TabDragContext* OnTabDragUpdated(
        TabDragTarget::DragController& controller,
        const gfx::Point& point_in_screen) override {
      area_->ArrastoDeAba(point_in_screen);
      return const_cast<TabDragContext*>(controller.GetAttachedContext());
    }
    void OnTabDragExited(const gfx::Point& point_in_screen) override {
      area_->FimDoArrastoDeAba();
    }
    void OnTabDragEnded() override { area_->FimDoArrastoDeAba(); }
    bool CanDropTab() override { return area_->PodeSoltarAba(); }
    void HandleTabDrop(TabDragTarget::DragController& controller) override {
      const DragSessionData& dados = controller.GetSessionData();
      area_->SoltarAba(dados.source_view_index_ < dados.tab_drag_data_.size()
                           ? dados.source_dragged_contents()
                           : nullptr);
    }
    base::CallbackListSubscription RegisterWillDestroyCallback(
        base::OnceClosure callback) override {
      return ao_destruir_.Add(std::move(callback));
    }

   private:
    raw_ptr<AreaDeFixos> area_;
    base::OnceClosureList ao_destruir_;
  };
  // Por último: some antes do resto da área (avisa o TabDragController).
  AlvoDeAba alvo_de_aba_{this};

  gfx::Rect AreaDe(views::View* view) {
    return gfx::ToEnclosingRect(views::View::ConvertRectToTarget(
        view->parent(), this, gfx::RectF(view->bounds())));
  }

  views::View* ArrastavelEm(const gfx::Point& ponto) {
    for (const auto& [view, id] : ids_) {
      if (AreaDe(view).Contains(ponto)) {
        return view;
      }
    }
    return nullptr;
  }

  void CalcularDestino(const gfx::Point& ponto) {
    vizinho_ = nullptr;
    auto arrastado = arrastado_ ? ids_.find(arrastado_) : ids_.end();
    const std::string id =
        arrastado == ids_.end() ? std::string() : arrastado->second;
    const bool pasta_arrastada =
        views::IsViewClass<CabecalhoDaPasta>(arrastado_);
    for (const auto& [view, outro] : ids_) {
      if (view == arrastado_) {
        continue;
      }
      const gfx::Rect area = AreaDe(view);
      if (views::IsViewClass<Ladrilho>(view)) {
        // Pasta não vira favorito.
        if (pasta_arrastada || !area.Contains(ponto)) {
          continue;
        }
        vizinho_ = view;
        onde_ = ponto.x() < area.CenterPoint().x() ? Onde::kAntes
                                                   : Onde::kDepois;
        return;
      }
      // Linhas e pastas: pela altura (ocupam a largura da lateral).
      if (ponto.y() < area.y() || ponto.y() >= area.bottom()) {
        continue;
      }
      // Pasta: metade de cima é antes dela; de baixo, pra dentro.
      const Onde onde = ponto.y() < area.CenterPoint().y() ? Onde::kAntes
                        : views::IsViewClass<CabecalhoDaPasta>(view)
                            ? Onde::kDentro
                            : Onde::kDepois;
      // Uma pasta não entra nela mesma (nem numa de dentro dela) nem passa
      // do limite de pastas numa fila.
      if (pasta_arrastada &&
          !CabeEm(Perfil(), id, onde == Onde::kDentro ? outro : pais_[view])) {
        return;
      }
      vizinho_ = view;
      onde_ = onde;
      return;
    }
    // A linha embaixo dos fixados: vai pro fim, fora das pastas. Uma aba da
    // lista também vai pro quadro dos favoritos e pra zona de soltar.
    for (views::View* lugar :
         {divisor_.get(), externo_ ? quadro_.get() : nullptr, zona_.get()}) {
      if (!lugar || !AreaDe(lugar).Contains(ponto)) {
        continue;
      }
      if (pasta_arrastada && !CabeEm(Perfil(), id, std::string())) {
        return;
      }
      vizinho_ = lugar;
      onde_ = lugar == divisor_ ? Onde::kAntes : Onde::kDentro;
      return;
    }
  }

  void Soltar() {
    if (!vizinho_ || !ids_.count(arrastado_)) {
      return;
    }
    const std::string id = ids_[arrastado_];
    if (vizinho_ == divisor_) {
      MoverParaPasta(Perfil(), id, std::string());
      return;
    }
    if (!ids_.count(vizinho_)) {
      return;
    }
    const std::string outro = ids_[vizinho_];
    if (onde_ == Onde::kDentro) {
      if (MoverParaPasta(Perfil(), id, outro)) {
        AbrirPasta(Perfil(), outro, true);
      }
    } else {
      Posicionar(Perfil(), id, outro, onde_ == Onde::kDepois);
    }
  }

  // A cada movimento de uma aba da lista por cima da área.
  void ArrastoDeAba(const gfx::Point& ponto_na_tela) {
    externo_ = true;
    gfx::Point ponto = ponto_na_tela;
    views::View::ConvertPointFromScreen(this, &ponto);
    views::View* antes = vizinho_;
    const Onde onde_antes = onde_;
    CalcularDestino(ponto);
    if (vizinho_ != antes || onde_ != onde_antes) {
      SchedulePaint();
    }
  }

  void FimDoArrastoDeAba() {
    externo_ = false;
    if (vizinho_) {
      SchedulePaint();
    }
    vizinho_ = nullptr;
  }

  bool PodeSoltarAba() const { return externo_ && vizinho_; }

  // A aba (da lista desta janela) solta aqui vira um site fixado, ligado a
  // ela: sai da lista e aparece no lugar onde caiu.
  void SoltarAba(content::WebContents* aba) {
    views::View* lugar = vizinho_;
    const Onde onde = onde_;
    FimDoArrastoDeAba();
    if (!lugar || !aba || SiteDaAba(aba)) {
      return;
    }
    Profile* perfil = Perfil();
    const std::string espaco = EspacoAtual();
    const GURL url = EnderecoDaAba(aba);
    const std::string titulo = TituloParaFixar(aba);
    std::string id;
    if (lugar == quadro_) {
      id = Favoritar(perfil, espaco, url, titulo);
    } else if (lugar == divisor_ || lugar == zona_) {
      id = Fixar(perfil, espaco, url, titulo, std::string());
    } else if (auto it = ids_.find(lugar); it != ids_.end()) {
      const std::string outro = it->second;
      if (views::IsViewClass<Ladrilho>(lugar)) {
        id = Favoritar(perfil, espaco, url, titulo);
        Posicionar(perfil, id, outro, onde == Onde::kDepois);
      } else if (onde == Onde::kDentro) {
        id = Fixar(perfil, espaco, url, titulo, outro);
        AbrirPasta(perfil, outro, true);
      } else {
        id = FixarPerto(perfil, espaco, url, titulo, outro,
                        onde == Onde::kDepois);
      }
    }
    if (id.empty()) {
      return;
    }
    Ligar(aba, id);
    InvalidarLista();
    ReconstruirDepois();
  }

  // Sem nada fixado, a zona de soltar aparece enquanto uma aba é arrastada
  // pela lateral (e some quando o arrasto acaba).
  void LigarZona() {
    if (zona_ligada_) {
      return;
    }
    zona_ligada_ = true;
    if (Do(Perfil(), EspacoAtual()).empty()) {
      ReconstruirDepois();
    }
    vigia_da_zona_.Start(FROM_HERE, base::Milliseconds(200),
                         base::BindRepeating(&AreaDeFixos::ConferirZona,
                                             base::Unretained(this)));
  }

  void ConferirZona() {
    if (TabDragController::IsActive()) {
      return;
    }
    vigia_da_zona_.Stop();
    zona_ligada_ = false;
    if (zona_) {
      ReconstruirDepois();
    }
  }

  void CancelarArrasto() {
    if (arrastando_) {
      SchedulePaint();
    }
    arrastado_ = nullptr;
    vizinho_ = nullptr;
    arrastando_ = false;
  }

  base::WeakPtrFactory<AreaDeFixos> weak_factory_{this};
};

BEGIN_METADATA(AreaDeFixos)
END_METADATA

}  // namespace

TabDragTarget* AlvoDosFixos(BrowserWindowInterface* browser,
                            const gfx::Point& ponto_na_tela) {
  auto it = Areas().find(browser);
  return it == Areas().end() ? nullptr : it->second->AlvoEm(ponto_na_tela);
}

void AdicionarArea(views::View* tab_strip_view,
                   BrowserWindowInterface* browser) {
  // "Fixar no espaço" no menu da aba (containers_ui não enxerga daqui).
  static bool ponte_ligada = false;
  if (!ponte_ligada) {
    ponte_ligada = true;
    containers::DefinirFixarAba(base::BindRepeating(
        [](TabStripModel* tab_strip, int index, const std::string& destino) {
          for (const auto& [janela, area] : Areas()) {
            if (janela->GetTabStripModel() == tab_strip) {
              area->FixarAba(index, destino);
              return;
            }
          }
        }));
  }
  static bool ponte_dos_favoritos = false;
  if (!ponte_dos_favoritos) {
    ponte_dos_favoritos = true;
    DefinirAlternarFavorito(base::BindRepeating([](content::WebContents* aba) {
      for (const auto& [janela, area] : Areas()) {
        if (area->TemAba(aba)) {
          return area->AlternarFavoritoDaAba(aba);
        }
      }
      return false;
    }));
  }
  // A janela anônima não tem fixados nem favoritos do espaço: as abas
  // fixadas são as do Chromium.
  if (browser->GetProfile()->IsOffTheRecord()) {
    return;
  }
  for (views::View* filho : tab_strip_view->children()) {
    if (filho->GetID() == visual::kIdAreaDeFixos) {
      return;
    }
  }
  // Numa rolagem só dela: com muitos fixados ou favoritos, a área ocupava a
  // lateral inteira e a lista de abas sumia (o layout dá a ela no máximo
  // uma parte da altura: tab_strip_view_layout.cc).
  auto rolagem = std::make_unique<views::ScrollView>();
  rolagem->SetID(visual::kIdAreaDeFixos);
  rolagem->SetBackgroundColor(std::nullopt);
  rolagem->SetDrawOverflowIndicator(false);
  rolagem->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  rolagem->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  rolagem->ClipHeightTo(0, std::numeric_limits<int>::max());
  rolagem->SetContents(std::make_unique<AreaDeFixos>(browser, tab_strip_view));
  tab_strip_view->AddChildView(std::move(rolagem));
}

}  // namespace navegador::fixos
