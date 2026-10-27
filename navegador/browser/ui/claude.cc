#include "navegador/browser/ui/claude.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "navegador/browser/containers/claude.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/icones/menu/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace navegador::claude {

namespace {

// kCor ("#D97757").
constexpr SkColor kLaranja = SkColorSetRGB(0xD9, 0x77, 0x57);
constexpr int kRaio = 10;
constexpr int kIcone = 16;
constexpr int kEntreIconeETexto = 8;

// Comandos do menu do container.
constexpr int kDoEspaco = 1;
constexpr int kNenhum = 2;
constexpr int kPrimeiroContainer = 100;

std::u16string NomeDoContainer(Profile* perfil) {
  const std::optional<std::string> escolhido = Container(perfil);
  if (!escolhido) {
    return u"Container do espaço";
  }
  if (escolhido->empty()) {
    return u"Sem container";
  }
  if (std::optional<containers::Container> container =
          containers::Get(perfil, *escolhido)) {
    return base::UTF8ToUTF16(container->name);
  }
  return u"Container do espaço";
}

// "Trabalho ⌄": o container das abas do Claude; clicar abre a lista.
class SeletorDeContainer : public views::Button {
  METADATA_HEADER(SeletorDeContainer, views::Button)

 public:
  explicit SeletorDeContainer(PressedCallback apertou)
      : views::Button(std::move(apertou)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(1, 4), 2));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    texto_ = AddChildView(std::make_unique<views::Label>());
    texto_->SetFontList(gfx::FontList().DeriveWithSizeDelta(
        11 - gfx::FontList().GetFontSize()));
    texto_->SetElideBehavior(gfx::ELIDE_TAIL);
    texto_->SetCanProcessEventsWithinSubtree(false);
    seta_ = AddChildView(std::make_unique<views::ImageView>());
    seta_->SetCanProcessEventsWithinSubtree(false);
    SetTooltipText(u"Container em que o Claude abre abas");
    GetViewAccessibility().SetName(u"Container em que o Claude abre abas");
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 6);
  }

  void SetTexto(const std::u16string& texto) { texto_->SetText(texto); }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    const SkColor cor = GetColorProvider()->GetColor(
        kColorTabForegroundInactiveFrameActive);
    texto_->SetEnabledColor(cor);
    seta_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kKeyboardArrowDownIcon,
        kColorTabForegroundInactiveFrameActive, 12));
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (GetState() != STATE_HOVERED && GetState() != STATE_PRESSED) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(kLaranja, 0x33));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), 6, flags);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

 private:
  raw_ptr<views::Label> texto_ = nullptr;
  raw_ptr<views::ImageView> seta_ = nullptr;
};

BEGIN_METADATA(SeletorDeContainer)
END_METADATA

class Cartao : public views::View, public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(Cartao, views::View)

 public:
  explicit Cartao(BrowserWindowInterface* browser)
      : perfil_(browser->GetProfile()->GetOriginalProfile()) {
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 8, 6, 8));
    GetViewAccessibility().SetRole(ax::mojom::Role::kStatus);
    GetViewAccessibility().SetName(u"Claude está usando o navegador");
    // Em cima o ícone e o aviso (a largura toda); embaixo o container e
    // "Parar".
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::TLBR(8, 10, 5, 6), 2));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    auto* linha = AddChildView(std::make_unique<views::View>());
    auto* linha_layout =
        linha->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            kEntreIconeETexto));
    linha_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    auto* icone = linha->AddChildView(std::make_unique<views::ImageView>(
        ui::ImageModel::FromVectorIcon(icones_menu::kSparkleIcon, kLaranja,
                                       kIcone)));
    icone->SetCanProcessEventsWithinSubtree(false);
    const gfx::FontList base;
    titulo_ = linha->AddChildView(
        std::make_unique<views::Label>(u"Claude está usando o navegador"));
    titulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    titulo_->SetMultiLine(true);
    titulo_->SetMaxLines(2);
    titulo_->SetFontList(base.DeriveWithSizeDelta(12 - base.GetFontSize())
                             .DeriveWithWeight(gfx::Font::Weight::SEMIBOLD));
    titulo_->SetTooltipText(
        u"A extensão do Claude pode ver e mexer nas páginas enquanto este "
        u"aviso estiver aqui.");
    linha_layout->SetFlexForView(titulo_, 1);

    auto* controles = AddChildView(std::make_unique<views::View>());
    auto* controles_layout =
        controles->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 4));
    controles_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    // Alinhado com o texto do aviso (o seletor tem 4 de folga por dentro).
    seletor_ = controles->AddChildView(
        std::make_unique<SeletorDeContainer>(base::BindRepeating(
            &Cartao::AbrirMenu, base::Unretained(this))));
    seletor_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, kIcone + kEntreIconeETexto - 4, 0, 0));
    controles_layout->SetFlexForView(
        controles->AddChildView(std::make_unique<views::View>()), 1);
    parar_ = controles->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating([] { Parar(); }), u"Parar"));
    parar_->SetTooltipText(u"Desconectar o Claude deste navegador");
    parar_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(3, 8)));
    parar_->SetLabelStyle(views::style::STYLE_BODY_5);
    views::InkDrop::Get(parar_)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(parar_)->SetBaseColor(kLaranja);
    views::InstallRoundRectHighlightPathGenerator(parar_, gfx::Insets(), 6);

    uso_ = ObservarUso(
        base::BindRepeating(&Cartao::Atualizar, base::Unretained(this)));
    prefs_.Init(perfil_->GetPrefs());
    prefs_.Add(kContainerPref, base::BindRepeating(&Cartao::Atualizar,
                                                   base::Unretained(this)));
    prefs_.Add(containers::kContainersPref,
               base::BindRepeating(&Cartao::Atualizar,
                                   base::Unretained(this)));
    SetVisible(false);
    Atualizar();
  }

  Cartao(const Cartao&) = delete;
  Cartao& operator=(const Cartao&) = delete;
  ~Cartao() override = default;

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    gfx::RectF area(GetLocalBounds());
    area.Inset(0.5f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(kLaranja, 0x26));
    canvas->DrawRoundRect(area, kRaio, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(SkColorSetA(kLaranja, 0x8C));
    canvas->DrawRoundRect(area, kRaio, flags);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    titulo_->SetEnabledColor(
        GetColorProvider()->GetColor(kColorTabForegroundActiveFrameActive));
    parar_->SetEnabledTextColors(
        GetColorProvider()->GetColor(kColorTabForegroundActiveFrameActive));
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    const std::optional<std::string> escolhido = Container(perfil_);
    if (command_id == kDoEspaco) {
      return !escolhido;
    }
    if (command_id == kNenhum) {
      return escolhido && escolhido->empty();
    }
    const size_t i = command_id - kPrimeiroContainer;
    return escolhido && i < ids_.size() && ids_[i] == *escolhido;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == kDoEspaco) {
      EscolherContainer(perfil_, std::nullopt);
    } else if (command_id == kNenhum) {
      EscolherContainer(perfil_, std::string());
    } else if (const size_t i = command_id - kPrimeiroContainer;
               i < ids_.size()) {
      EscolherContainer(perfil_, ids_[i]);
    }
  }

 private:
  void Atualizar() {
    seletor_->SetTexto(NomeDoContainer(perfil_));
    const bool mostrar = EmUso();
    if (GetVisible() != mostrar) {
      SetVisible(mostrar);
      PreferredSizeChanged();
    }
  }

  void AbrirMenu() {
    menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    ids_.clear();
    menu_->AddRadioItem(kDoEspaco, u"O do espaço em que você está", 0);
    menu_->AddRadioItem(kNenhum, u"Sem container", 0);
    menu_->AddSeparator(ui::NORMAL_SEPARATOR);
    for (const containers::Container& container :
         containers::GetAll(perfil_)) {
      menu_->AddRadioItem(kPrimeiroContainer + ids_.size(),
                          base::UTF8ToUTF16(container.name), 0);
      ids_.push_back(container.id);
    }
    // O menu pega o mouse: o seletor não vê a saída do ponteiro.
    runner_ = std::make_unique<views::MenuRunner>(
        menu_.get(), views::MenuRunner::NO_FLAGS,
        base::BindRepeating(
            [](SeletorDeContainer* seletor) {
              seletor->SetState(views::Button::STATE_NORMAL);
            },
            base::Unretained(seletor_.get())));
    runner_->RunMenuAt(GetWidget(), nullptr, seletor_->GetBoundsInScreen(),
                       views::MenuAnchorPosition::kTopLeft,
                       ui::mojom::MenuSourceType::kMouse);
  }

  raw_ptr<Profile> perfil_;
  raw_ptr<views::Label> titulo_ = nullptr;
  raw_ptr<SeletorDeContainer> seletor_ = nullptr;
  raw_ptr<views::LabelButton> parar_ = nullptr;
  base::CallbackListSubscription uso_;
  PrefChangeRegistrar prefs_;
  std::unique_ptr<ui::SimpleMenuModel> menu_;
  std::unique_ptr<views::MenuRunner> runner_;
  std::vector<std::string> ids_;
};

BEGIN_METADATA(Cartao)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CriarCartao(BrowserWindowInterface* browser) {
  // A janela anônima não tem containers nem espaços: o cartão fica nas
  // janelas normais (a extensão só entra na anônima com permissão).
  if (browser->GetProfile()->IsOffTheRecord()) {
    return std::make_unique<views::View>();
  }
  return std::make_unique<Cartao>(browser);
}

void EscolherContainer(Profile* perfil,
                       const std::optional<std::string>& container) {
  DefinirContainer(perfil, container);
  // Os espaços do Claude abertos passam a abrir abas novas nele também.
  if (!container) {
    return;
  }
  for (espacos::Espaco espaco : espacos::Todos(perfil->GetOriginalProfile())) {
    if (espaco.temporario && NomeDoClaude(espaco.nome) &&
        espaco.container != *container) {
      espaco.container = *container;
      espacos::Atualizar(perfil->GetOriginalProfile(), espaco);
    }
  }
}

}  // namespace navegador::claude
