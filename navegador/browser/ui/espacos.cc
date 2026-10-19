#include "navegador/browser/ui/espacos.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/containers/fixos.h"
#include "navegador/browser/ui/abas/abas.h"
#include "navegador/browser/ui/atalhos.h"
#include "navegador/browser/ui/barra.h"
#include "navegador/browser/ui/configuracoes.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "navegador/browser/ui/atualizador.h"
#include "navegador/browser/ui/downloads/botao.h"
#include "navegador/browser/ui/grupos_salvos.h"
#include "navegador/browser/ui/containers/espaco.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/menus/simple_menu_model.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_tracker.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/gfx/range/range.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

namespace navegador::espacos {

namespace {

// Medidas do seletor de espaços (como o do Dia).
constexpr int kAlturaFaixa = 38;
constexpr int kLarguraMaximaTitulo = 160;
constexpr int kEspacoEntreBolinhas = 3;
constexpr int kBolinha = 10;  // área clicável de cada bolinha
constexpr float kRaioBolinha = 3.0f;
constexpr float kRaioBolinhaAtual = 4.0f;
// Como o Zen: entre duas trocas pela rodinha, pelo menos isto.
constexpr base::TimeDelta kIntervaloRodinha = base::Milliseconds(200);
// Quanto um grupo novo sem nome espera pelo nome antes de ir pro espaço
// atual (FaixaDeEspacos::LigarGruposNovos).
constexpr base::TimeDelta kEsperaPeloNome = base::Seconds(1);

// Cor da paleta dos containers -> cor de grupo do Chromium.
tab_groups::TabGroupColorId CorDoGrupo(std::string_view cor_escolhida) {
  // Cor livre ("#RRGGBB"): a do grupo é a da paleta mais perto; a janela e
  // as bolinhas usam a cor exata.
  const std::string nome = containers::NomeMaisProximo(cor_escolhida);
  const std::string_view cor = nome;
  if (cor == "roxo") {
    return tab_groups::TabGroupColorId::kPurple;
  }
  if (cor == "verde") {
    return tab_groups::TabGroupColorId::kGreen;
  }
  if (cor == "laranja") {
    return tab_groups::TabGroupColorId::kOrange;
  }
  if (cor == "rosa") {
    return tab_groups::TabGroupColorId::kPink;
  }
  if (cor == "turquesa") {
    return tab_groups::TabGroupColorId::kCyan;
  }
  if (cor == "amarelo") {
    return tab_groups::TabGroupColorId::kYellow;
  }
  if (cor == "vermelho") {
    return tab_groups::TabGroupColorId::kRed;
  }
  if (cor == "cinza") {
    return tab_groups::TabGroupColorId::kGrey;
  }
  return tab_groups::TabGroupColorId::kBlue;
}

std::string CorDaPaleta(tab_groups::TabGroupColorId cor) {
  switch (cor) {
    case tab_groups::TabGroupColorId::kPurple:
      return "roxo";
    case tab_groups::TabGroupColorId::kGreen:
      return "verde";
    case tab_groups::TabGroupColorId::kOrange:
      return "laranja";
    case tab_groups::TabGroupColorId::kPink:
      return "rosa";
    case tab_groups::TabGroupColorId::kCyan:
      return "turquesa";
    case tab_groups::TabGroupColorId::kYellow:
      return "amarelo";
    case tab_groups::TabGroupColorId::kRed:
      return "vermelho";
    case tab_groups::TabGroupColorId::kGrey:
      return "cinza";
    default:
      return "azul";
  }
}

// Nome mostrado: o emoji (se tiver) e o nome.
std::u16string Titulo(const Espaco& espaco) {
  std::u16string nome = base::UTF8ToUTF16(espaco.nome);
  if (espaco.icone.empty()) {
    return nome;
  }
  return base::UTF8ToUTF16(espaco.icone) + u" " + nome;
}

class FaixaDeEspacos;

std::map<BrowserWindowInterface*, FaixaDeEspacos*>& Faixas() {
  static base::NoDestructor<std::map<BrowserWindowInterface*, FaixaDeEspacos*>>
      faixas;
  return *faixas;
}

// Uma bolinha: um espaço. Clicar leva pro espaço.
class Bolinha : public views::View {
  METADATA_HEADER(Bolinha, views::View)

 public:
  Bolinha(std::string cor,
          bool atual,
          std::u16string nome,
          base::RepeatingClosure ao_clicar)
      : cor_(std::move(cor)), atual_(atual), ao_clicar_(std::move(ao_clicar)) {
    SetTooltipText(nome);
    SetPreferredSize(gfx::Size(kBolinha, kBolinha));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    SkColor cor = containers::Cor(cor_);
    if (!atual_) {
      cor = SkColorSetA(cor, 0x66);
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(cor);
    canvas->DrawCircle(gfx::PointF(width() / 2.0f, height() / 2.0f),
                       atual_ ? kRaioBolinhaAtual : kRaioBolinha, flags);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    return event.IsOnlyLeftMouseButton();
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (HitTestPoint(event.location())) {
      ao_clicar_.Run();
    }
  }

 private:
  std::string cor_;
  bool atual_;
  base::RepeatingClosure ao_clicar_;
};

BEGIN_METADATA(Bolinha)
END_METADATA

// Nome do espaço no cabeçalho da lateral; clicar abre a lista de espaços,
// como o seletor de perfis do Dia ("Work ⌄": ✓ no atual, bolinha da cor nos
// outros, "Edit Profiles…").
class TituloDoEspaco : public views::Button,
                       public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(TituloDoEspaco, views::Button)

 public:
  explicit TituloDoEspaco(BrowserWindowInterface* browser)
      : views::Button(base::BindRepeating(&TituloDoEspaco::AbrirMenu,
                                          base::Unretained(this))),
        browser_(browser) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    rotulo_ = AddChildView(std::make_unique<views::Label>());
    rotulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rotulo_->SetElideBehavior(gfx::ELIDE_TAIL);
    // Texto onde o Dia põe o nome do perfil (~58 da borda da lateral).
    rotulo_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 8)));
    // Nome do espaço na Bricolage Grotesque (a fonte de títulos do desenho,
    // como o nome do servidor no Resenha); sem ela, a da interface.
    rotulo_->SetFontList(gfx::FontList(
        {"Bricolage Grotesque", gfx::FontList().GetPrimaryFont().GetFontName()},
        gfx::Font::NORMAL, 14, gfx::Font::Weight::SEMIBOLD));
    rotulo_->SetCanProcessEventsWithinSubtree(false);
    SetPreferredSize(gfx::Size(0, 32));
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColor(
        kColorTabForegroundActiveFrameActive);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 4);
    SetTooltipText(u"Trocar de espaço");
    // Janela anônima: sem espaços, o cabeçalho só diz o que ela é.
    if (browser->GetProfile()->IsOffTheRecord()) {
      views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
      SetTooltipText(u"Sem espaços nem containers; nada desta janela fica "
                     u"gravado.");
    }
  }

  void Mostrar(const std::u16string& texto) {
    rotulo_->SetText(texto);
    GetViewAccessibility().SetName(
        browser_->GetProfile()->IsOffTheRecord() ? texto : u"Espaço: " + texto);
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    rotulo_->SetEnabledColor(
        GetColorProvider()->GetColor(kColorTabForegroundActiveFrameActive));
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == kNovo) {
      espacos::NovoEspaco(browser_);
    } else if (command_id == kEditar) {
      configuracoes::Mostrar(browser_, configuracoes::Aba::kEspacos);
    } else if (command_id == kAtalhos) {
      configuracoes::Mostrar(browser_, configuracoes::Aba::kAtalhos);
    } else if (command_id == kConfiguracoes) {
      configuracoes::Mostrar(browser_);
    } else if (const size_t i = command_id - kPrimeiro; i < ids_.size()) {
      espacos::IrPara(browser_, ids_[i]);
    }
  }

 private:
  void AbrirMenu() {
    Profile* perfil = browser_->GetProfile();
    if (perfil->IsOffTheRecord()) {
      return;
    }
    const std::string atual = Ativo(perfil);
    std::string do_grupo;
    if (std::optional<tab_groups::TabGroupId> grupo =
            browser_->GetTabStripModel()->GetFocusedGroup()) {
      do_grupo = EspacoDoGrupo(perfil, grupo->ToString()).value_or("");
    }
    menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_->AddTitle(u"Espaços");
    ids_.clear();
    for (const Espaco& espaco : Todos(perfil)) {
      const bool e_o_atual =
          espaco.id == (do_grupo.empty() ? atual : do_grupo);
      menu_->AddItemWithIcon(
          kPrimeiro + ids_.size(), Titulo(espaco),
          e_o_atual ? ui::ImageModel::FromVectorIcon(
                          kCheckIcon, ui::kColorMenuIcon, 16)
                    : containers::Bolinha(espaco.cor));
      ids_.push_back(espaco.id);
    }
    menu_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_->AddItem(kNovo, u"Novo espaço…");
    menu_->AddItem(kEditar, u"Editar espaços…");
    menu_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_->AddItem(kAtalhos, u"Atalhos do teclado…");
    menu_->AddItem(kConfiguracoes, u"Configurações do Norte…");
    runner_ = std::make_unique<views::MenuRunner>(
        menu_.get(), views::MenuRunner::HAS_MNEMONICS,
        base::BindRepeating(&TituloDoEspaco::MenuFechou,
                            base::Unretained(this)));
    runner_->RunMenuAt(GetWidget(), nullptr, GetAnchorBoundsInScreen(),
                       views::MenuAnchorPosition::kTopLeft,
                       ui::mojom::MenuSourceType::kMouse);
  }

  // O menu pega o mouse: o botão não recebe a saída do ponteiro nem a
  // soltura do clique, e o destaque ficava preso no nome do espaço.
  void MenuFechou() {
    SetState(IsMouseHovered() ? STATE_HOVERED : STATE_NORMAL);
    views::InkDrop::Get(this)->GetInkDrop()->SnapToHidden();
    views::InkDrop::Get(this)->GetInkDrop()->SetHovered(IsMouseHovered());
    SchedulePaint();
  }

  static constexpr int kNovo = 1;
  static constexpr int kEditar = 2;
  static constexpr int kAtalhos = 3;
  static constexpr int kConfiguracoes = 4;
  static constexpr int kPrimeiro = 100;

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::Label> rotulo_ = nullptr;
  std::vector<std::string> ids_;
  std::unique_ptr<ui::SimpleMenuModel> menu_;
  std::unique_ptr<views::MenuRunner> runner_;
};

BEGIN_METADATA(TituloDoEspaco)
END_METADATA

// Eventos em cima da lateral inteira (pre-target handler nela): trocam de
// espaço sem passar pelas abas.
class TrocaNaLateral : public ui::EventHandler {
 public:
  explicit TrocaNaLateral(base::RepeatingCallback<void(bool)> girar,
                          base::RepeatingCallback<void(bool)> girar_rodinha)
      : girar_(std::move(girar)), girar_rodinha_(std::move(girar_rodinha)) {}

  void OnMouseEvent(ui::MouseEvent* event) override {
    // Botões laterais (voltar/avançar) com o ponteiro na lateral trocam de
    // espaço, como no Zen; em cima da página continuam voltar/avançar.
    if (event->type() == ui::EventType::kMousePressed ||
        event->type() == ui::EventType::kMouseReleased) {
      const int botao = event->changed_button_flags();
      if (botao & (ui::EF_BACK_MOUSE_BUTTON | ui::EF_FORWARD_MOUSE_BUTTON)) {
        if (event->type() == ui::EventType::kMousePressed) {
          girar_.Run(/*pra_frente=*/botao & ui::EF_FORWARD_MOUSE_BUTTON);
        }
        event->SetHandled();
        return;
      }
    }
    // Rolagem horizontal (sem tecla) ou Ctrl+rodinha: troca de espaço. A
    // vertical sozinha continua rolando a lista de abas.
    if (event->type() == ui::EventType::kMousewheel) {
      const ui::MouseWheelEvent* rodinha = event->AsMouseWheelEvent();
      int delta = 0;
      if (rodinha->x_offset() != 0 &&
          std::abs(rodinha->x_offset()) >= std::abs(rodinha->y_offset())) {
        delta = -rodinha->x_offset();
      } else if (rodinha->IsControlDown() && rodinha->y_offset() != 0) {
        delta = -rodinha->y_offset();
      }
      if (delta != 0) {
        girar_rodinha_.Run(delta > 0);
        event->SetHandled();
      }
    }
  }

  void OnScrollEvent(ui::ScrollEvent* event) override {
    // Touchpad: deslizar de lado troca de espaço uma vez por gesto, como o
    // Zen. O eixo trava nos primeiros 15 px; o gesto acaba com os dedos
    // levantando (fling) ou 300 ms parado. A rolagem de lado é toda nossa:
    // solta, ela virava rodinha (Widget::OnScrollEvent) e girava de novo.
    if (event->type() == ui::EventType::kScrollFlingStart ||
        event->type() == ui::EventType::kScrollFlingCancel) {
      ZerarGesto();
      return;
    }
    if (event->type() != ui::EventType::kScroll) {
      return;
    }
    if (event->time_stamp() - ultimo_ > base::Milliseconds(300)) {
      ZerarGesto();
    }
    ultimo_ = event->time_stamp();
    acumulado_ += gfx::Vector2dF(event->x_offset(), event->y_offset());
    if (eixo_ == Eixo::kNenhum &&
        std::max(std::abs(acumulado_.x()), std::abs(acumulado_.y())) >= 15) {
      eixo_ = std::abs(acumulado_.x()) > std::abs(acumulado_.y())
                  ? Eixo::kHorizontal
                  : Eixo::kVertical;
    }
    if (eixo_ == Eixo::kVertical ||
        (eixo_ == Eixo::kNenhum &&
         std::abs(event->x_offset()) < std::abs(event->y_offset()))) {
      return;  // Rola a lista de abas.
    }
    event->SetHandled();
    if (eixo_ == Eixo::kHorizontal && !girou_ &&
        std::abs(acumulado_.x()) >= 60) {
      girou_ = true;
      girar_rodinha_.Run(acumulado_.x() < 0);
    }
  }

 private:
  enum class Eixo { kNenhum, kHorizontal, kVertical };

  void ZerarGesto() {
    acumulado_ = gfx::Vector2dF();
    eixo_ = Eixo::kNenhum;
    girou_ = false;
  }

  base::RepeatingCallback<void(bool)> girar_;
  base::RepeatingCallback<void(bool)> girar_rodinha_;
  gfx::Vector2dF acumulado_;
  Eixo eixo_ = Eixo::kNenhum;
  bool girou_ = false;
  base::TimeTicks ultimo_;
};

// A faixa no pé da lateral e o controle dos espaços da janela.
class FaixaDeEspacos : public views::View,
                       public TabStripModelObserver,
                       public views::ContextMenuController,
                       public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(FaixaDeEspacos, views::View)

 public:
  FaixaDeEspacos(views::View* lateral, BrowserWindowInterface* browser)
      : lateral_(lateral),
        browser_(browser),
        model_(browser->GetTabStripModel()),
        anonima_(browser->GetProfile()->IsOffTheRecord()) {
    layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 6),
        8));
    layout_->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Downloads à esquerda, sempre à vista; as bolinhas e o "+" no meio da
    // lateral (um contrapeso do tamanho do botão à direita deixa o meio no
    // meio de verdade).
    baixar_ = AddChildView(downloads::CriarBotao(browser));
    auto* antes = AddChildView(std::make_unique<views::View>());
    layout_->SetFlexForView(antes, 1);

    titulo_ = AddChildView(std::make_unique<views::Label>());
    titulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    titulo_->SetElideBehavior(gfx::ELIDE_TAIL);
    titulo_->SetMaximumWidthSingleLine(kLarguraMaximaTitulo);
    titulo_->SetFontList(
        gfx::FontList()
            .DeriveWithSizeDelta(13 - gfx::FontList().GetFontSize())
            .DeriveWithWeight(gfx::Font::Weight::MEDIUM));

    bolinhas_ = AddChildView(std::make_unique<views::View>());
    bolinhas_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kEspacoEntreBolinhas));

    novo_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(&FaixaDeEspacos::NovoEspaco,
                            base::Unretained(this)),
        kAddIcon, 14));
    novo_->SetTooltipText(u"Novo espaço");
    auto* depois = AddChildView(std::make_unique<views::View>());
    layout_->SetFlexForView(depois, 1);
    AddChildView(std::make_unique<views::View>())
        ->SetPreferredSize(baixar_->GetPreferredSize());

    Faixas()[browser_] = this;
    prefs_.Init(Perfil()->GetPrefs());
    prefs_.Add(kAtalhosPref,
               base::BindRepeating(&FaixaDeEspacos::RegistrarAtalhos,
                                   base::Unretained(this)));
    // Janela anônima (e a de visitante): sem espaços, como uma janela comum
    // do Chromium. Fica só o botão de downloads e os atalhos que não são de
    // espaço; as abas não entram em grupos e nada vai pras prefs.
    if (anonima_) {
      titulo_->SetVisible(false);
      bolinhas_->SetVisible(false);
      novo_->SetVisible(false);
      Atualizar();
      return;
    }

    model_->AddObserver(this);
    set_context_menu_controller(this);
    prefs_.Add(kListaPref,
               base::BindRepeating(&FaixaDeEspacos::QuandoListaMudar,
                                   base::Unretained(this)));

    Atualizar();
  }

  std::optional<SkColor> Tom() const { return tom_; }

  ~FaixaDeEspacos() override {
    if (!anonima_) {
      model_->RemoveObserver(this);
    }
    auto it = Faixas().find(browser_);
    if (it != Faixas().end() && it->second == this) {
      Faixas().erase(it);
    }
  }

  // --- Troca de espaço ----------------------------------------------------

  void Focar(const std::string& espaco) {
    if (anonima_ || !Get(Perfil(), espaco)) {
      return;
    }
    // O temporário é só da janela da extensão: noutra janela não ganha aba
    // nem vira o Ativo (a janela nova nasceria nele).
    const bool temporario = Temporario(espaco);
    if (temporario && !GrupoDoEspaco(espaco)) {
      return;
    }
    if (std::optional<tab_groups::TabGroupId> grupo = GrupoDoEspaco(espaco)) {
      // Volta pra última aba usada no espaço: ativar uma aba de outro grupo
      // já foca o grupo dela (patch no TabStripModel::ActivateTab). Sem
      // ela, o Chromium foca na primeira aba do grupo.
      int index = TabStripModel::kNoTab;
      auto ultima = ultima_aba_.find(espaco);
      if (ultima != ultima_aba_.end()) {
        if (tabs::TabInterface* aba = ultima->second.Get()) {
          index = model_->GetIndexOfTab(aba);
        }
      }
      if (index != TabStripModel::kNoTab &&
          model_->GetTabGroupForTab(index) == *grupo) {
        if (index != model_->active_index()) {
          model_->ActivateTabAt(index);
        }
        if (model_->GetFocusedGroup() != *grupo) {
          model_->SetFocusedGroup(*grupo);
        }
      } else {
        model_->SetFocusedGroup(*grupo);
      }
    } else {
      CriarAbaNoEspaco(espaco, GURL(chrome::kChromeUINewTabURL));
    }
    if (!temporario) {
      DefinirAtivo(Perfil(), espaco);
    }
  }

  // Os espaços por onde a troca passa: o temporário (o grupo de uma
  // extensão) só na janela que tem o grupo dele.
  std::vector<Espaco> Navegaveis() const {
    std::vector<Espaco> todos = Todos(Perfil());
    std::erase_if(todos, [this](const Espaco& espaco) {
      return espaco.temporario && !GrupoDoEspaco(espaco.id);
    });
    return todos;
  }

  void Girar(bool pra_frente) {
    if (anonima_) {
      return;
    }
    const std::vector<Espaco> todos = Navegaveis();
    if (todos.size() < 2) {
      return;
    }
    const std::string atual = EspacoAtual();
    size_t i = 0;
    while (i < todos.size() && todos[i].id != atual) {
      ++i;
    }
    if (i == todos.size()) {
      i = 0;
    }
    const size_t n = todos.size();
    Focar(todos[(i + (pra_frente ? 1 : n - 1)) % n].id);
  }

  void IrParaPosicao(size_t posicao) {
    const std::vector<Espaco> todos = Navegaveis();
    if (posicao < todos.size()) {
      Focar(todos[posicao].id);
    }
  }

  void NovoEspaco() {
    if (anonima_) {
      return;
    }
    configuracoes::EditarEspaco(
        browser_, std::string(),
        base::BindOnce(&FaixaDeEspacos::Focar, weak_factory_.GetWeakPtr()));
  }

  void MoverAbaPara(int index, const std::string& espaco) {
    if (anonima_ || index < 0 || index >= model_->count() ||
        model_->IsTabPinned(index)) {
      return;
    }
    const std::optional<tab_groups::TabGroupId> origem =
        model_->GetTabGroupForTab(index);
    const bool era_ativa = index == model_->active_index();
    if (std::optional<tab_groups::TabGroupId> destino =
            GrupoDoEspaco(espaco)) {
      if (destino == origem) {
        return;
      }
      model_->AddToExistingGroup({index}, *destino, /*add_to_end=*/true);
    } else {
      const tab_groups::TabGroupId grupo = model_->AddToNewGroup({index});
      Ligar(grupo, espaco);
    }
    // Como no Zen: quem move a aba fica no espaço; a aba ativa passa pra
    // outra do espaço (ou, se era a última, a janela segue a aba).
    if (era_ativa && origem) {
      // A aba era a única do espaço: o grupo já sumiu (o GetTabGroup de
      // um grupo que não existe dá CHECK).
      const TabGroup* grupo =
          model_->group_model()->ContainsTabGroup(*origem)
              ? model_->group_model()->GetTabGroup(*origem)
              : nullptr;
      if (grupo && grupo->GetFirstTab()) {
        model_->ActivateTabAt(model_->GetIndexOfTab(grupo->GetFirstTab()));
      } else {
        Focar(espaco);
      }
    }
  }

  // Fecha as abas do espaço nesta janela (antes, se a janela está nele,
  // passa pro espaço seguinte).
  void FecharEspaco(const std::string& espaco) {
    if (anonima_) {
      return;
    }
    std::optional<tab_groups::TabGroupId> grupo = GrupoDoEspaco(espaco);
    if (!grupo) {
      return;
    }
    if (model_->GetFocusedGroup() == *grupo) {
      Girar(/*pra_frente=*/true);
      if (model_->GetFocusedGroup() == *grupo) {
        // Era o único espaço com abas nesta janela: abre o seguinte.
        for (const Espaco& outro : Todos(Perfil())) {
          if (outro.id != espaco) {
            Focar(outro.id);
            break;
          }
        }
      }
    }
    if (model_->group_model()->ContainsTabGroup(*grupo)) {
      model_->CloseAllTabsInGroup(*grupo);
    }
  }

  Profile* PerfilDaJanela() const { return Perfil(); }

  void DefinirTitulo(TituloDoEspaco* titulo) {
    titulo_do_cabecalho_.SetView(titulo);
    Atualizar();
  }

  // --- views::View ---------------------------------------------------------

  void AddedToWidget() override {
    RegistrarAtalhos();
    if (anonima_) {
      return;  // Voltar/avançar e a rolagem na lateral ficam os de sempre.
    }
    lateral_->AddPreTargetHandler(&na_lateral_);
    // A sessão restaurada chega depois: arruma no próximo giro.
    GarantirDepois();
  }

  void RemovedFromWidget() override {
    if (views::FocusManager* foco = GetFocusManager()) {
      foco->UnregisterAccelerators(this);
    }
    if (!anonima_) {
      lateral_->RemovePreTargetHandler(&na_lateral_);
    }
  }

  // Divisão de tela pelo teclado (Ctrl+Alt+V/H, como no Zen): divide a aba
  // ativa com a outra selecionada (Ctrl+clique) ou com uma aba nova, que
  // escolhe o que abrir. Já dividida: o mesmo atalho desfaz, o outro vira.
  void Dividir(split_tabs::SplitTabLayout layout) {
    TabStripModel* model = browser_->GetTabStripModel();
    tabs::TabInterface* ativa = model->GetActiveTab();
    if (!ativa) {
      return;
    }
    if (std::optional<split_tabs::SplitTabId> divisao = ativa->GetSplit()) {
      if (model->GetSplitData(*divisao)->visual_data()->split_layout() ==
          layout) {
        model->RemoveSplit(*divisao);
      } else {
        model->UpdateSplitLayout(*divisao, layout);
        model->UpdateSplitRatio(*divisao, 0.5);
      }
      return;
    }
    std::vector<int> outras;
    for (tabs::TabInterface* aba : model->selection_model().selected_tabs()) {
      if (aba != ativa && !aba->IsSplit()) {
        outras.push_back(model->GetIndexOfTab(aba));
      }
    }
    if (outras.size() == 1) {
      model->AddToNewSplit(outras, split_tabs::SplitTabVisualData(layout),
                           split_tabs::SplitTabCreatedSource::kKeyboardShortcut);
      return;
    }
    chrome::NewSplitTab(browser_, layout,
                        split_tabs::SplitTabCreatedSource::kKeyboardShortcut);
  }

  bool AcceleratorPressed(const ui::Accelerator& atalho) override {
    auto it = acoes_.find(atalho);
    if (it == acoes_.end()) {
      return false;
    }
    const std::string& acao = it->second;
    if (acao == "alternar_lateral") {
      chrome::ExecuteCommand(browser_, IDC_TOGGLE_VERTICAL_TABS_COLLAPSE);
    } else if (acao == "alternar_barra") {
      barra::Alternar(browser_);
    } else if (acao == "dividir_lado") {
      Dividir(split_tabs::SplitTabLayout::kSideBySide);
    } else if (acao == "dividir_cima") {
      Dividir(split_tabs::SplitTabLayout::kStacked);
    } else if (acao == "trocar_lados") {
      if (tabs::TabInterface* ativa = browser_->GetTabStripModel()->GetActiveTab();
          ativa && ativa->GetSplit()) {
        browser_->GetTabStripModel()->ReverseTabsInSplit(*ativa->GetSplit());
      }
    } else if (acao == "proximo_espaco") {
      Girar(/*pra_frente=*/true);
    } else if (acao == "espaco_anterior") {
      Girar(/*pra_frente=*/false);
    } else if (acao == "novo_espaco") {
      NovoEspaco();
    } else if (acao.starts_with("espaco_")) {
      IrParaPosicao(acao.back() - '1');
    } else if (acao == "configuracoes") {
      configuracoes::Mostrar(browser_);
    } else if (acao == "containers") {
      configuracoes::Mostrar(browser_, configuracoes::Aba::kContainers);
    } else {
      return false;
    }
    return true;
  }

  // Valem mesmo com a faixa escondida (lateral recolhida).
  bool CanHandleAccelerators() const override {
    return GetWidget() && GetWidget()->IsActive();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(views::View::CalculatePreferredSize(available_size).width(),
                     kAlturaFaixa);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    titulo_->SetEnabledColor(
        GetColorProvider()->GetColor(kColorTabForegroundActiveFrameActive));
  }

  // A rodinha em cima da faixa passa de um espaço pro outro (o "deslizar"
  // do Dia). Em cima do resto da lateral, ver OnMouseEvent.
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override {
    if (anonima_) {
      return false;
    }
    const int delta =
        event.y_offset() != 0 ? event.y_offset() : event.x_offset();
    if (delta == 0) {
      return false;
    }
    GirarPelaRodinha(delta < 0);
    return true;
  }

  // --- views::ContextMenuController: clique direito na faixa ---------------

  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_->AddItem(kEditar, u"Editar espaço…");
    containers_ = containers::GetAll(Perfil());
    submenu_containers_ = std::make_unique<ui::SimpleMenuModel>(this);
    submenu_containers_->AddCheckItem(kSemContainer, u"Nenhum");
    for (size_t i = 0; i < containers_.size(); ++i) {
      submenu_containers_->AddCheckItem(
          kPrimeiroContainer + i, base::UTF8ToUTF16(containers_[i].name));
      submenu_containers_->SetIcon(submenu_containers_->GetItemCount() - 1,
                                   containers::Bolinha(containers_[i].color));
    }
    menu_->AddSubMenu(kContainerDoEspaco, u"Container do espaço",
                      submenu_containers_.get());
    menu_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_->AddItem(kNovo, u"Novo espaço");
    menu_->AddItem(kConfigurar, u"Configurar espaços…");
    menu_->AddItem(kAtalhosCmd, u"Atalhos do teclado…");
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_.get(), views::MenuRunner::CONTEXT_MENU);
    menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // --- ui::SimpleMenuModel::Delegate ---------------------------------------

  bool IsCommandIdChecked(int command_id) const override {
    const std::optional<std::string> container =
        containers::ContainerDoEspaco(Perfil(), EspacoAtual());
    if (command_id == kSemContainer) {
      return !container;
    }
    const size_t i = command_id - kPrimeiroContainer;
    return container && i < containers_.size() &&
           containers_[i].id == *container;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    const std::string atual = EspacoAtual();
    switch (command_id) {
      case kNovo:
        NovoEspaco();
        return;
      case kEditar:
        configuracoes::EditarEspaco(browser_, atual, base::DoNothing());
        return;
      case kConfigurar:
        configuracoes::Mostrar(browser_, configuracoes::Aba::kEspacos);
        return;
      case kAtalhosCmd:
        configuracoes::Mostrar(browser_, configuracoes::Aba::kAtalhos);
        return;
      case kSemContainer:
        containers::DefinirContainerDoEspaco(Perfil(), atual, "");
        return;
    }
    const size_t i = command_id - kPrimeiroContainer;
    if (i < containers_.size()) {
      containers::DefinirContainerDoEspaco(Perfil(), atual,
                                           containers_[i].id);
    }
  }

  // O grupo do espaço que a janela mostra, pra lateral sem grupo focado
  // (espacos_dados.h, GrupoVisivel).
  std::optional<tab_groups::TabGroupId> GrupoVisivel() const {
    if (espaco_visivel_.empty() || !model_->group_model()) {
      return std::nullopt;
    }
    if (std::optional<tab_groups::TabGroupId> grupo =
            GrupoDoEspaco(espaco_visivel_)) {
      return grupo;
    }
    // O espaço da janela ficou sem abas (a última saiu num arrasto): a
    // lateral fica vazia em vez de mostrar os outros espaços.
    static const tab_groups::TabGroupId nenhum =
        tab_groups::TabGroupId::GenerateNew();
    return nenhum;
  }

  // --- TabStripModelObserver -----------------------------------------------

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      if (TabDragController::IsActive()) {
        for (const auto& inserida : change.GetInsert()->contents) {
          if (inserida.tab) {
            chegaram_.push_back(inserida.tab->GetHandle());
          }
        }
      } else if (!SessionRestore::IsRestoring(Perfil())) {
        entrou_aba_ = true;
      }
      GarantirDepois();
    }
    if (selection.active_tab_changed() && selection.new_tab) {
      if (std::optional<tab_groups::TabGroupId> grupo =
              selection.new_tab->GetGroup()) {
        if (std::optional<std::string> espaco =
                EspacoDoGrupo(Perfil(), grupo->ToString())) {
          ultima_aba_[*espaco] = selection.new_tab->GetHandle();
        }
      }
    }
  }

  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override {
    if (new_focused_group_id) {
      if (std::optional<std::string> espaco =
              EspacoDoGrupo(Perfil(), new_focused_group_id->ToString())) {
        // O temporário não vira o ativo: janela nova não nasce nele.
        if (!Temporario(*espaco)) {
          DefinirAtivo(Perfil(), *espaco);
        }
        if (!TabDragController::IsActive()) {
          espaco_visivel_ = *espaco;
        }
      }
    } else {
      // Sem espaço não existe: volta a focar um.
      GarantirDepois();
    }
    AtualizarDepois();
  }

  void OnTabGroupAdded(const tab_groups::TabGroupId& group_id) override {
    // Fora de arrasto e da sessão, pode ser o grupo de uma extensão
    // (LigarGruposNovos). Os nossos já nascem ligados a um espaço.
    if (!TabDragController::IsActive() &&
        !SessionRestore::IsRestoring(Perfil())) {
      novos_[group_id] = base::TimeTicks::Now();
    }
    GarantirDepois();
  }

  void OnTabGroupWillBeRemoved(
      const tab_groups::TabGroupId& group_id) override {
    novos_.erase(group_id);
    // A janela inteira fechando: os grupos saem junto, mas continuam dos
    // espaços (a sessão traz eles de volta). Desligar e reabrir o espaço com
    // aba nova aqui estragava o "continuar de onde parou": o espaço em uso
    // voltava só com uma "Nova guia".
    if (model_->closing_all()) {
      return;
    }
    // A última aba do espaço fechou: como no Zen, a janela fica no espaço,
    // com uma aba nova. O temporário vai embora com o grupo
    // (OnTabGroupChanged).
    if (model_->GetFocusedGroup() == group_id && model_->count() > 1 &&
        !GrupoTemporario(Perfil(), group_id.ToString())) {
      if (std::optional<std::string> espaco =
              EspacoDoGrupo(Perfil(), group_id.ToString())) {
        espaco_a_manter_ = *espaco;
      }
    }
    DesligarGrupo(Perfil(), group_id.ToString());
    GarantirDepois();
  }

  void OnTabGroupChanged(const TabGroupChange& change) override {
    // Grupo de espaço temporário que esvaziou (a extensão fechou as abas;
    // não saiu pra outra janela): o espaço vai junto, mesmo com a janela
    // fechando por ser a última aba dela. Menos com o navegador saindo ou a
    // última janela fechando: aí a sessão traz o grupo de volta, ou a
    // limpeza da partida tira o espaço.
    if (change.type != TabGroupChange::kClosed) {
      return;
    }
    const TabGroupChange::CloseChange* fechou = change.GetCloseChange();
    if ((fechou && fechou->reason() == TabGroupChange::TabGroupClosureReason::
                                          kDetachedToAnotherTabstrip) ||
        !GrupoTemporario(Perfil(), change.group.ToString()) ||
        browser_shutdown::IsTryingToQuit()) {
      return;
    }
    if (model_->closing_all() &&
        std::ranges::none_of(Faixas(), [this](const auto& janela) {
          return janela.second != this &&
                 janela.first->GetProfile() == Perfil();
        })) {
      return;
    }
    DesligarGrupo(Perfil(), change.group.ToString());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FaixaDeEspacos::LimparTemporarios,
                                  Perfil()->GetWeakPtr()));
  }

  // Espaços temporários sem grupo em nenhuma janela do perfil somem. Não
  // durante a restauração da sessão: os grupos deles ainda vão chegar.
  // Uma vez por perfil, depois da restauração da partida: os grupos que
  // sobraram na pref de janelas que já fecharam.
  static void PodarGruposUmaVez(Profile* perfil) {
    static base::NoDestructor<std::set<base::FilePath>> podados;
    // A anônima tem o mesmo caminho e os grupos dela não ficam gravados.
    if (perfil->IsOffTheRecord() ||
        !podados->insert(perfil->GetPath()).second) {
      return;
    }
    std::set<std::string> vivos;
    for (const auto& [janela, faixa] : Faixas()) {
      if (janela->GetProfile() != perfil || !janela->GetTabStripModel() ||
          !janela->GetTabStripModel()->group_model()) {
        continue;
      }
      for (const tab_groups::TabGroupId& id :
           janela->GetTabStripModel()->group_model()->ListTabGroups()) {
        vivos.insert(id.ToString());
      }
    }
    PodarGrupos(perfil, vivos);
  }

  static void LimparTemporarios(base::WeakPtr<Profile> perfil) {
    if (!perfil || SessionRestore::IsRestoring(perfil.get()) ||
        !grupos_salvos::Pronto(perfil.get())) {
      return;
    }
    for (const Espaco& espaco : Todos(perfil.get())) {
      if (!espaco.temporario) {
        continue;
      }
      const bool tem_grupo =
          std::ranges::any_of(Faixas(), [&](const auto& janela) {
            return janela.first->GetProfile() == perfil.get() &&
                   janela.second->GrupoDoEspaco(espaco.id);
          });
      if (!tem_grupo) {
        Remover(perfil.get(), espaco.id);
        fixos::RemoverEspaco(perfil.get(), espaco.id);
      }
    }
  }

 private:
  Profile* Perfil() const { return browser_->GetProfile(); }

  std::optional<tab_groups::TabGroupId> GrupoDoEspaco(
      const std::string& espaco) const {
    if (!model_->group_model()) {
      return std::nullopt;
    }
    for (const tab_groups::TabGroupId& id :
         model_->group_model()->ListTabGroups()) {
      if (EspacoDoGrupo(Perfil(), id.ToString()) == espaco) {
        return id;
      }
    }
    return std::nullopt;
  }

  bool Temporario(const std::string& espaco) const {
    std::optional<Espaco> dados = Get(Perfil(), espaco);
    return dados && dados->temporario;
  }

  std::string EspacoAtual() const {
    if (std::optional<tab_groups::TabGroupId> grupo =
            model_->GetFocusedGroup()) {
      if (std::optional<std::string> espaco =
              EspacoDoGrupo(Perfil(), grupo->ToString())) {
        return *espaco;
      }
    }
    return EspacoDaJanela();
  }

  // O espaço que esta janela mostra (o Chromium tira o foco do grupo a cada
  // aba inserida sem grupo). O Ativo é do perfil inteiro, o último usado em
  // qualquer janela: só vale pra janela que ainda não entrou em nenhum.
  std::string EspacoDaJanela() const {
    if (!espaco_visivel_.empty() && Get(Perfil(), espaco_visivel_)) {
      return espaco_visivel_;
    }
    return Ativo(Perfil());
  }

  void Ligar(const tab_groups::TabGroupId& grupo, const std::string& espaco) {
    LigarGrupo(Perfil(), grupo.ToString(), espaco);
    AplicarVisual(grupo, espaco);
  }

  void AplicarVisual(const tab_groups::TabGroupId& grupo,
                     const std::string& espaco) {
    std::optional<Espaco> dados = Get(Perfil(), espaco);
    const TabGroup* tab_group = model_->group_model()->GetTabGroup(grupo);
    if (!dados || !tab_group) {
      return;
    }
    const tab_groups::TabGroupVisualData visual(Titulo(*dados),
                                                CorDoGrupo(dados->cor));
    if (tab_group->visual_data()->title() != visual.title() ||
        tab_group->visual_data()->color() != visual.color()) {
      model_->ChangeTabGroupVisuals(grupo, visual, /*is_customized=*/true);
    }
  }

  // Primeira aba de um espaço que ainda não tem grupo nesta janela: abre a
  // aba (no container do espaço), põe num grupo novo ligado ao espaço e foca.
  void CriarAbaNoEspaco(const std::string& espaco, const GURL& url) {
    content::WebContents* aba = nullptr;
    {
      containers::EspacoPendente pendente(browser_, espaco);
      NavigateParams params(browser_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      Navigate(&params);
      aba = params.navigated_or_inserted_contents;
    }
    const int index = aba ? model_->GetIndexOfWebContents(aba)
                          : TabStripModel::kNoTab;
    if (index == TabStripModel::kNoTab) {
      return;
    }
    const tab_groups::TabGroupId grupo = model_->AddToNewGroup({index});
    Ligar(grupo, espaco);
    model_->SetFocusedGroup(grupo);
    model_->ActivateTabAt(model_->GetIndexOfWebContents(aba));
  }

  void GarantirDepois() {
    if (garantia_pendente_) {
      return;
    }
    garantia_pendente_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FaixaDeEspacos::Garantir,
                                  weak_factory_.GetWeakPtr()));
  }

  void GarantirEm(base::TimeDelta espera) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FaixaDeEspacos::GarantirDepois,
                       weak_factory_.GetWeakPtr()),
        espera);
  }

  // Grupo que nasceu agora sem espaço, fora de arrasto e da sessão: é de uma
  // extensão. A do Claude cria o grupo e dá o nome ("Claude") logo depois,
  // noutra chamada, e se perder o grupo para de funcionar. Sem nome, espera
  // um pouco sem mexer em nada da janela (senão ela voltava pro espaço de
  // onde a aba saiu, com uma aba nova); com nome, vira um espaço temporário,
  // que some quando o grupo fechar. Nunca ganhou nome: segue a regra de
  // sempre (vai pro espaço atual). Devolve false enquanto espera.
  bool LigarGruposNovos() {
    Profile* perfil = Perfil();
    const base::TimeTicks agora = base::TimeTicks::Now();
    for (auto it = novos_.begin(); it != novos_.end();) {
      const tab_groups::TabGroupId id = it->first;
      if (!model_->group_model()->ContainsTabGroup(id) ||
          EspacoDoGrupo(perfil, id.ToString())) {
        it = novos_.erase(it);
        continue;
      }
      const TabGroup* grupo = model_->group_model()->GetTabGroup(id);
      const std::u16string nome = grupo->visual_data()->title();
      if (nome.empty()) {
        const base::TimeDelta idade = agora - it->second;
        if (idade < kEsperaPeloNome) {
          GarantirEm(kEsperaPeloNome - idade);
          return false;
        }
        it = novos_.erase(it);
        continue;
      }
      it = novos_.erase(it);
      // Nome de um espaço que já existe (grupo reaberto com Ctrl+Shift+T,
      // aba movida de outra janela): fica com a regra de sempre, que junta
      // no espaço.
      if (std::ranges::any_of(Todos(perfil), [&nome](const Espaco& existente) {
            return base::UTF8ToUTF16(existente.nome) == nome ||
                   Titulo(existente) == nome;
          })) {
        continue;
      }
      const Espaco espaco =
          Criar(perfil, base::UTF16ToUTF8(nome), "",
                CorDaPaleta(grupo->visual_data()->color()), "",
                /*temporario=*/true);
      Ligar(id, espaco.id);
      // A aba em uso está nele (a janela que a extensão abriu): a janela
      // mostra o espaço novo.
      if (model_->active_index() != TabStripModel::kNoTab &&
          model_->GetTabGroupForTab(model_->active_index()) == id) {
        model_->SetFocusedGroup(id);
        espaco_visivel_ = espaco.id;
        espaco_a_manter_.clear();
      }
    }
    return true;
  }

  // Regras da janela: todo grupo é de um espaço, toda aba solta (não fixada)
  // entra no espaço atual e sempre tem um espaço focado.
  void Garantir() {
    garantia_pendente_ = false;
    // Com uma aba sendo arrastada (pra fora, pra outra janela), mexer nos
    // grupos confunde o arrasto do Chromium: a aba da janela nova entrava no
    // grupo do espaço no meio do arrasto, e a janela travava ou o navegador
    // caía (CHECK ao mover um grupo pra dentro de outro). Espera acabar.
    if (TabDragController::IsActive()) {
      garantia_pendente_ = true;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FaixaDeEspacos::Garantir,
                         weak_factory_.GetWeakPtr()),
          base::Milliseconds(150));
      return;
    }
    if (!model_->group_model() || model_->empty() || model_->closing_all()) {
      return;
    }
    Profile* perfil = Perfil();

    if (!LigarGruposNovos()) {
      return;
    }
    // Temporários que sobraram de antes (o navegador fechou com o grupo
    // aberto e a sessão não voltou).
    if (!limpou_temporarios_ && !SessionRestore::IsRestoring(perfil) &&
        grupos_salvos::Pronto(perfil)) {
      limpou_temporarios_ = true;
      LimparTemporarios(perfil->GetWeakPtr());
      PodarGruposUmaVez(perfil);
    }

    // A última aba de um espaço fechou: reabre o espaço com aba nova.
    if (!espaco_a_manter_.empty()) {
      const std::string espaco = std::move(espaco_a_manter_);
      espaco_a_manter_.clear();
      if (Get(perfil, espaco) && !GrupoDoEspaco(espaco)) {
        Focar(espaco);
      }
    }
    // A última aba do espaço que a janela mostra saiu num arrasto (pra outra
    // janela ou uma nova): a janela fica nele, com uma aba nova, em vez de
    // pular pro espaço da aba que o Chromium ativou. O temporário sem grupo
    // acabou: a janela vai pro espaço da aba ativa.
    if (!espaco_visivel_.empty() && !GrupoDoEspaco(espaco_visivel_)) {
      if (Temporario(espaco_visivel_)) {
        espaco_visivel_.clear();
      } else if (Get(perfil, espaco_visivel_)) {
        Focar(espaco_visivel_);
      }
    }

    // Grupo sem espaço (sessão antiga, grupo vindo de outra janela): vira o
    // espaço do mesmo nome, ou um espaço novo com o nome e a cor dele.
    for (const tab_groups::TabGroupId& id :
         model_->group_model()->ListTabGroups()) {
      if (!model_->group_model()->ContainsTabGroup(id) ||
          EspacoDoGrupo(perfil, id.ToString())) {
        continue;
      }
      const TabGroup* grupo = model_->group_model()->GetTabGroup(id);
      const std::string nome =
          base::UTF16ToUTF8(grupo->visual_data()->title());
      std::optional<std::string> espaco;
      for (const Espaco& existente : Todos(perfil)) {
        if (!nome.empty() && (existente.nome == nome ||
                              base::UTF16ToUTF8(Titulo(existente)) == nome)) {
          espaco = existente.id;
          break;
        }
      }
      // Grupo sem nome (aba solta de outra janela, sessão antiga): é do
      // espaço atual.
      if (!espaco && nome.empty()) {
        espaco = EspacoAtual();
      }
      // O espaço já tem grupo nesta janela: junta as abas nele.
      if (espaco) {
        if (std::optional<tab_groups::TabGroupId> existente =
                GrupoDoEspaco(*espaco);
            existente && *existente != id) {
          std::vector<int> abas;
          for (int i = 0; i < model_->count(); ++i) {
            if (model_->GetTabGroupForTab(i) == id) {
              abas.push_back(i);
            }
          }
          model_->AddToExistingGroup(abas, *existente, /*add_to_end=*/true);
          GarantirDepois();
          continue;
        }
      }
      if (!espaco) {
        // Versão antiga: o container era do grupo.
        const std::string* container =
            perfil->GetPrefs()
                ->GetDict(containers::kEspacosPref)
                .FindString(id.ToString());
        espaco = Criar(perfil, nome.empty() ? "Espaço" : nome, "",
                       CorDaPaleta(grupo->visual_data()->color()),
                       container ? *container : "")
                     .id;
      }
      Ligar(id, *espaco);
    }

    // Abas arrastadas de outra janela pra esta: entram no espaço que esta
    // janela mostra (onde foram soltas), sem trazer o espaço delas junto. O
    // container é da aba e vai com ela.
    if (!chegaram_.empty()) {
      const std::vector<tabs::TabHandle> chegaram = std::move(chegaram_);
      chegaram_.clear();
      std::vector<int> mover;
      tabs::TabInterface* ativa = nullptr;
      for (const tabs::TabHandle& handle : chegaram) {
        tabs::TabInterface* aba = handle.Get();
        const int index =
            aba ? model_->GetIndexOfTab(aba) : TabStripModel::kNoTab;
        if (espaco_visivel_.empty() || index == TabStripModel::kNoTab ||
            model_->IsTabPinned(index)) {
          continue;
        }
        const std::optional<tab_groups::TabGroupId> grupo = aba->GetGroup();
        if (grupo &&
            EspacoDoGrupo(perfil, grupo->ToString()) == espaco_visivel_) {
          continue;
        }
        mover.push_back(index);
        if (aba == model_->GetActiveTab()) {
          ativa = aba;
        }
      }
      if (!mover.empty()) {
        std::ranges::sort(mover);
        if (std::optional<tab_groups::TabGroupId> destino =
                GrupoDoEspaco(espaco_visivel_)) {
          // Foco antes: o grupo de onde elas saem some, e sumir o grupo
          // focado reabria o espaço dele (espaco_a_manter_).
          model_->SetFocusedGroup(*destino);
          model_->AddToExistingGroup(mover, *destino);
        } else {
          const tab_groups::TabGroupId novo = model_->AddToNewGroup(mover);
          Ligar(novo, espaco_visivel_);
          model_->SetFocusedGroup(novo);
        }
        espaco_a_manter_.clear();
        if (ativa) {
          model_->ActivateTabAt(model_->GetIndexOfTab(ativa));
        }
      }
    }

    // Dois grupos do mesmo espaço na janela: a aba que saiu pra outra janela
    // voltou com o grupo de lá. O foco ficava no grupo que chegou (só com
    // ela) e as outras abas do espaço sumiam da lateral. Junta no grupo com
    // mais abas; as que chegaram entram onde foram soltas (antes ou depois).
    std::map<std::string, std::vector<tab_groups::TabGroupId>> por_espaco;
    for (const tab_groups::TabGroupId& id :
         model_->group_model()->ListTabGroups()) {
      if (std::optional<std::string> espaco =
              EspacoDoGrupo(perfil, id.ToString())) {
        por_espaco[*espaco].push_back(id);
      }
    }
    for (const auto& [espaco, grupos] : por_espaco) {
      if (grupos.size() < 2) {
        continue;
      }
      if (!grupos_salvos::Pronto(perfil)) {
        // Sessão restaurando: espera o serviço de grupos salvos carregar.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FaixaDeEspacos::GarantirDepois,
                           weak_factory_.GetWeakPtr()),
            base::Milliseconds(300));
        break;
      }
      const std::optional<tab_groups::TabGroupId> focado =
          model_->GetFocusedGroup();
      const tab_groups::TabGroupId fica = *std::ranges::max_element(
          grupos, {}, [this](const tab_groups::TabGroupId& grupo) {
            return model_->group_model()->GetTabGroup(grupo)->tab_count();
          });
      for (const tab_groups::TabGroupId& grupo : grupos) {
        if (grupo == fica || !model_->group_model()->ContainsTabGroup(grupo)) {
          continue;
        }
        const gfx::Range faixa =
            model_->group_model()->GetTabGroup(grupo)->ListTabs();
        std::vector<int> abas;
        for (size_t i = faixa.start(); i < faixa.end(); ++i) {
          abas.push_back(static_cast<int>(i));
        }
        model_->AddToExistingGroup(abas, fica);
      }
      if (focado && std::ranges::find(grupos, *focado) != grupos.end()) {
        model_->SetFocusedGroup(fica);
      }
    }

    // Abas soltas -> espaço atual.
    std::vector<int> soltas;
    for (int i = 0; i < model_->count(); ++i) {
      if (!model_->IsTabPinned(i) && !model_->GetTabGroupForTab(i)) {
        soltas.push_back(i);
      }
    }
    if (!soltas.empty()) {
      std::optional<tab_groups::TabGroupId> alvo = model_->GetFocusedGroup();
      if (!alvo) {
        alvo = GrupoDoEspaco(EspacoDaJanela());
      }
      if (alvo) {
        model_->AddToExistingGroup(soltas, *alvo, /*add_to_end=*/true);
      } else {
        const tab_groups::TabGroupId novo = model_->AddToNewGroup(soltas);
        Ligar(novo, EspacoDaJanela());
      }
    }

    // Sempre um espaço focado: o da aba ativa, senão o último usado, senão o
    // primeiro grupo.
    if (!model_->GetFocusedGroup()) {
      std::optional<tab_groups::TabGroupId> foco;
      if (model_->active_index() != TabStripModel::kNoTab) {
        foco = model_->GetTabGroupForTab(model_->active_index());
      }
      if (!foco) {
        foco = GrupoDoEspaco(EspacoDaJanela());
      }
      if (!foco && !model_->group_model()->ListTabGroups().empty()) {
        foco = model_->group_model()->ListTabGroups().front();
      }
      if (foco) {
        model_->SetFocusedGroup(*foco);
      }
    }

    for (const tab_groups::TabGroupId& id :
         model_->group_model()->ListTabGroups()) {
      if (std::optional<std::string> espaco =
              EspacoDoGrupo(perfil, id.ToString())) {
        AplicarVisual(id, *espaco);
      }
    }
    if (espaco_visivel_.empty() && model_->GetFocusedGroup()) {
      espaco_visivel_ = EspacoAtual();
    }
    if (std::exchange(entrou_aba_, false)) {
      FecharNovasGuiasVazias();
    }
    Atualizar();
  }

  // Uma aba entrou no espaço (Ctrl+Shift+T, link, aba nova): as "Nova guia"
  // vazias que sobraram nele (a que entra quando a última aba fecha, outra
  // reaberta) saem, menos a ativa. Sem guardar no Ctrl+Shift+T.
  void FecharNovasGuiasVazias() {
    const std::optional<tab_groups::TabGroupId> grupo =
        model_->GetFocusedGroup();
    if (!grupo || !model_->group_model()->ContainsTabGroup(*grupo)) {
      return;
    }
    const gfx::Range faixa =
        model_->group_model()->GetTabGroup(*grupo)->ListTabs();
    if (faixa.length() < 2) {
      return;
    }
    std::vector<content::WebContents*> vazias;
    for (size_t i = faixa.start(); i < faixa.end(); ++i) {
      content::WebContents* aba = model_->GetWebContentsAt(static_cast<int>(i));
      // A metade de uma divisão (Ctrl+Alt+V/H na Nova guia) fica.
      tabs::TabInterface* tab = model_->GetTabAtIndex(static_cast<int>(i));
      if (aba && aba != model_->GetActiveWebContents() && !aba->IsLoading() &&
          !(tab && tab->IsSplit()) &&
          aba->GetVisibleURL() == GURL(chrome::kChromeUINewTabURL) &&
          aba->GetController().GetEntryCount() <= 1) {
        vazias.push_back(aba);
      }
    }
    for (content::WebContents* aba : vazias) {
      const int index = model_->GetIndexOfWebContents(aba);
      if (index != TabStripModel::kNoTab) {
        model_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
      }
    }
  }

  void QuandoListaMudar() {
    // Espaço apagado: os grupos dele ficam soltos e o Garantir religa.
    GarantirDepois();
    AtualizarDepois();
  }

  void RegistrarAtalhos() {
    views::FocusManager* foco = GetFocusManager();
    if (!foco) {
      return;
    }
    foco->UnregisterAccelerators(this);
    acoes_.clear();
    for (const Atalho& atalho : Atalhos()) {
      if (anonima_ && AcaoDeEspaco(atalho.acao)) {
        continue;
      }
      if (std::optional<ui::Accelerator> tecla =
              atalhos::Ler(AtalhoDe(Perfil(), atalho.acao))) {
        acoes_[*tecla] = atalho.acao;
        foco->RegisterAccelerator(*tecla,
                                  ui::AcceleratorManager::kHighPriority, this);
      }
    }
  }

  void GirarPelaRodinha(bool pra_frente) {
    const base::TimeTicks agora = base::TimeTicks::Now();
    if (agora - ultima_rodinha_ < kIntervaloRodinha) {
      return;
    }
    ultima_rodinha_ = agora;
    Girar(pra_frente);
  }

  // Os avisos do modelo chegam no meio das mudanças: redesenha depois.
  void AtualizarDepois() {
    if (atualizacao_pendente_) {
      return;
    }
    atualizacao_pendente_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FaixaDeEspacos::Atualizar,
                                  weak_factory_.GetWeakPtr()));
  }

  // Cor do espaço da janela pra moldura e a lateral (CorDaJanela, lida no
  // BrowserWidget::GetColorProviderKey a cada cor pedida). A do Chromium (cor
  // do grupo focado) só vem na troca de foco com a lateral já montada: na
  // abertura da janela não vinha, e a janela ficava na cor neutra.
  void AtualizarTom() {
    std::optional<SkColor> cor;
    if (model_->GetFocusedGroup()) {
      if (std::optional<Espaco> espaco = Get(Perfil(), EspacoAtual())) {
        cor = containers::Cor(espaco->cor);
      }
    }
    if (tom_ == cor) {
      return;
    }
    tom_ = cor;
    views::Widget* janela =
        GetWidget() ? GetWidget()->GetTopLevelWidget() : nullptr;
    if (!janela) {
      return;
    }
    // Como o Chromium faz na troca de foco do grupo: troca o tema e repinta
    // a moldura (que pinta o vidro atrás da lateral).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<views::Widget> janela) {
                         if (!janela) {
                           return;
                         }
                         janela->ThemeChanged();
                         if (janela->non_client_view() &&
                             janela->non_client_view()->frame_view()) {
                           janela->non_client_view()
                               ->frame_view()
                               ->SchedulePaint();
                         }
                         janela->GetRootView()->SchedulePaint();
                       },
                       janela->GetWeakPtr()));
  }

  void Atualizar() {
    atualizacao_pendente_ = false;
    if (anonima_) {
      if (auto* cabecalho = static_cast<TituloDoEspaco*>(
              titulo_do_cabecalho_.view())) {
        cabecalho->Mostrar(Perfil()->IsGuestSession() ? u"Visitante"
                                                      : u"Anônima");
      }
      return;
    }
    AtualizarTom();
    const std::vector<Espaco> todos = Todos(Perfil());
    const std::string atual = EspacoAtual();
    bolinhas_->RemoveAllChildViews();
    std::u16string titulo;
    for (const Espaco& espaco : todos) {
      if (espaco.id == atual) {
        titulo = Titulo(espaco);
      }
      bolinhas_->AddChildView(std::make_unique<Bolinha>(
          espaco.cor, espaco.id == atual, Titulo(espaco),
          base::BindRepeating(&FaixaDeEspacos::Focar, base::Unretained(this),
                              espaco.id)));
    }
    titulo_->SetText(titulo);
    if (auto* cabecalho = static_cast<TituloDoEspaco*>(
            titulo_do_cabecalho_.view())) {
      cabecalho->Mostrar(titulo);
    }
    // O nome fica no cabeçalho (como o perfil no Dia); aqui, com um espaço
    // só o rodapé fica limpo, só com o "+" discreto à direita; as bolinhas
    // aparecem a partir do segundo.
    const bool varios = todos.size() > 1;
    titulo_->SetVisible(false);
    bolinhas_->SetVisible(varios);
    InvalidateLayout();
    SchedulePaint();
  }

  static constexpr int kEditar = 1;
  static constexpr int kNovo = 2;
  static constexpr int kContainerDoEspaco = 3;
  static constexpr int kConfigurar = 4;
  static constexpr int kAtalhosCmd = 5;
  static constexpr int kSemContainer = 100;
  static constexpr int kPrimeiroContainer = 101;

  raw_ptr<views::View> lateral_;
  raw_ptr<BrowserWindowInterface> browser_;
  TrocaNaLateral na_lateral_{
      base::BindRepeating(&FaixaDeEspacos::Girar, base::Unretained(this)),
      base::BindRepeating(&FaixaDeEspacos::GirarPelaRodinha,
                          base::Unretained(this))};
  raw_ptr<TabStripModel> model_;
  // Janela anônima ou de visitante: sem espaços (ver o construtor).
  const bool anonima_;
  PrefChangeRegistrar prefs_;
  std::map<ui::Accelerator, std::string> acoes_;
  std::map<std::string, tabs::TabHandle> ultima_aba_;
  std::string espaco_a_manter_;
  // Grupos que nasceram fora de arrasto e da sessão, ainda sem espaço, e
  // quando (LigarGruposNovos).
  std::map<tab_groups::TabGroupId, base::TimeTicks> novos_;
  bool limpou_temporarios_ = false;
  // O espaço que esta janela mostra (vazio numa janela recém-criada, até o
  // primeiro Garantir). Não muda no meio de um arrasto.
  std::string espaco_visivel_;
  // Abas que entraram nesta janela durante um arrasto (vindas de outra).
  std::vector<tabs::TabHandle> chegaram_;
  // Entrou aba fora de arrasto e de restauração: hora de tirar as "Nova guia"
  // vazias que sobraram no espaço.
  bool entrou_aba_ = false;
  base::TimeTicks ultima_rodinha_;
  std::unique_ptr<ui::SimpleMenuModel> menu_;
  std::unique_ptr<ui::SimpleMenuModel> submenu_containers_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::vector<containers::Container> containers_;
  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<views::Label> titulo_ = nullptr;
  raw_ptr<views::View> baixar_ = nullptr;
  raw_ptr<views::View> bolinhas_ = nullptr;
  raw_ptr<views::ImageButton> novo_ = nullptr;
  views::ViewTracker titulo_do_cabecalho_;
  bool atualizacao_pendente_ = false;
  bool garantia_pendente_ = false;
  // Cor do espaço da janela (nada = sem espaço focado), ver AtualizarTom.
  std::optional<SkColor> tom_;
  base::WeakPtrFactory<FaixaDeEspacos> weak_factory_{this};
};

BEGIN_METADATA(FaixaDeEspacos)
END_METADATA

FaixaDeEspacos* FaixaDe(BrowserWindowInterface* browser) {
  auto it = Faixas().find(browser);
  return it == Faixas().end() ? nullptr : it->second;
}

}  // namespace

std::optional<SkColor> CorDaJanela(BrowserWindowInterface* browser) {
  auto it = Faixas().find(browser);
  return it != Faixas().end() && it->second ? it->second->Tom()
                                             : std::nullopt;
}

void AdicionarFaixa(views::View* lateral, BrowserWindowInterface* browser) {
  // Os espaços usam grupos de abas; o Chromium não guarda mais esses grupos
  // na lista de salvos (ver grupos_salvos.h).
  grupos_salvos::Desligar(browser->GetProfile());
  // "Mover para o espaço" no menu da aba (containers_ui não enxerga daqui).
  static bool ponte_ligada = false;
  if (!ponte_ligada) {
    ponte_ligada = true;
    containers::DefinirMoverAba(base::BindRepeating(
        [](TabStripModel* tab_strip, int index, const std::string& espaco) {
          for (const auto& [janela, faixa] : Faixas()) {
            if (janela->GetTabStripModel() == tab_strip) {
              faixa->MoverAbaPara(index, espaco);
              return;
            }
          }
        }));
    DefinirGrupoVisivel(base::BindRepeating(
        [](const TabStripModel* model)
            -> std::optional<tab_groups::TabGroupId> {
          for (const auto& [janela, faixa] : Faixas()) {
            if (janela->GetTabStripModel() == model) {
              return faixa->GrupoVisivel();
            }
          }
          return std::nullopt;
        }));
    containers::DefinirAbrirConfiguracoes(
        base::BindRepeating([](BrowserWindowInterface* janela) {
          configuracoes::Mostrar(janela);
        }));
    // Clique duplo no nome da aba (as abas não enxergam o TabUIHelper).
    abas::DefinirRenomeador(base::BindRepeating(
        [](tabs::TabInterface* aba, const std::u16string& nome) {
          if (TabUIHelper* ajudante = TabUIHelper::From(aba)) {
            ajudante->Renomear(nome);
          }
        }));
  }
  // Espaçador: ocupa o que sobra embaixo das abas e empurra a faixa pro pé.
  // No KDE e no GNOME ele é barra de título: a barra de cima é quase toda
  // endereço, e a janela precisa de onde ser arrastada. Sem receber eventos,
  // o VerticalTabStripRegionView::IsPositionInWindowCaption não conta com ele.
  auto* espacador = lateral->AddChildView(std::make_unique<views::View>());
  espacador->SetCanProcessEventsWithinSubtree(!JanelasPeloMouse());
  espacador->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Aviso de versão nova, logo acima da faixa (escondido quando não há).
  lateral->AddChildView(atualizador::CriarAviso(browser));
  lateral->AddChildView(std::make_unique<FaixaDeEspacos>(lateral, browser));
}

std::unique_ptr<views::View> CriarTitulo(BrowserWindowInterface* browser) {
  auto titulo = std::make_unique<TituloDoEspaco>(browser);
  if (FaixaDeEspacos* faixa = FaixaDe(browser)) {
    faixa->DefinirTitulo(titulo.get());
  }
  return titulo;
}

void IrPara(BrowserWindowInterface* browser, const std::string& espaco) {
  if (FaixaDeEspacos* faixa = FaixaDe(browser)) {
    faixa->Focar(espaco);
  }
}

void Girar(BrowserWindowInterface* browser, bool pra_frente) {
  if (FaixaDeEspacos* faixa = FaixaDe(browser)) {
    faixa->Girar(pra_frente);
  }
}

void NovoEspaco(BrowserWindowInterface* browser) {
  if (FaixaDeEspacos* faixa = FaixaDe(browser)) {
    faixa->NovoEspaco();
  }
}

void MoverAbaPara(BrowserWindowInterface* browser,
                  int index,
                  const std::string& espaco) {
  if (FaixaDeEspacos* faixa = FaixaDe(browser)) {
    faixa->MoverAbaPara(index, espaco);
  }
}

void Excluir(Profile* profile, const std::string& espaco) {
  if (Todos(profile).size() <= 1) {
    return;
  }
  std::vector<FaixaDeEspacos*> faixas;
  for (const auto& [browser, faixa] : Faixas()) {
    if (faixa->PerfilDaJanela() == profile) {
      faixas.push_back(faixa);
    }
  }
  for (FaixaDeEspacos* faixa : faixas) {
    faixa->FecharEspaco(espaco);
  }
  Remover(profile, espaco);
  fixos::RemoverEspaco(profile, espaco);
}

}  // namespace navegador::espacos
