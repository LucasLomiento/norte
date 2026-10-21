#include "navegador/browser/ui/configuracoes.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/ui/atalhos.h"
#include "navegador/browser/ui/atualizador.h"
#include "navegador/browser/ui/barra.h"
#include "navegador/browser/ui/bloqueador.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "navegador/browser/ui/containers/gerenciador.h"
#include "navegador/browser/ui/espacos.h"
#include "navegador/browser/ui/exportar.h"
#include "navegador/browser/ui/importar_chromium.h"
#include "navegador/browser/ui/importar_zen.h"
#include "navegador/browser/ui/inicio.h"
#include "navegador/browser/sem_ia/sem_ia.h"
#include "navegador/browser/ui/seletor_de_cor.h"
#include "navegador/icones/menu/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/slider.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace navegador::configuracoes {

namespace {

// Medidas da janela de configurações do Dia (Preferences/*.xaml do Dia de
// Windows): menu de 200 à esquerda, página com título de 20, seções com
// título sobre um cartão, linhas com título de 14 e descrição de 12.
constexpr int kLarguraJanela = 820;
constexpr int kAlturaJanela = 600;
constexpr int kLarguraMenu = 200;
constexpr int kAlturaItemMenu = 36;
constexpr int kRaioCartao = 12;
constexpr int kLarguraDescricao = 340;

gfx::FontList Fonte(int tamanho, gfx::Font::Weight peso) {
  return gfx::FontList()
      .DeriveWithSizeDelta(tamanho - gfx::FontList().GetFontSize())
      .DeriveWithWeight(peso);
}

// Títulos de página e de seção na Bricolage Grotesque (a fonte de marca do
// desenho, que vai no pacote); sem ela, a da interface.
gfx::FontList FonteTitulo(int tamanho) {
  return gfx::FontList({"Bricolage Grotesque",
                        gfx::FontList().GetPrimaryFont().GetFontName()},
                       gfx::Font::NORMAL, tamanho,
                       gfx::Font::Weight::SEMIBOLD);
}

bool Escuro(const views::View* view) {
  const ui::ColorProvider* cores = view->GetColorProvider();
  return !cores ||
         color_utils::IsDark(cores->GetColor(ui::kColorDialogBackground));
}

gfx::NativeWindow JanelaDe(BrowserWindowInterface* browser) {
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  return view && view->GetWidget() ? view->GetWidget()->GetNativeWindow()
                                   : gfx::NativeWindow();
}

// No Wayland o diálogo é uma subsuperfície da janela: só aparece quando a
// janela manda um quadro novo. Aberto por atalho (sem menu fechando por
// cima), nada redesenha a janela e o diálogo ficava invisível com o foco.
void MostrarSobre(views::Widget* dialogo, BrowserWindowInterface* browser) {
  dialogo->Show();
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  if (view && view->GetWidget() && view->GetWidget()->GetRootView()) {
    view->GetWidget()->GetRootView()->SchedulePaint();
  }
}

// Refaz o layout (e as cores) das janelas abertas do perfil: a moldura, os
// botões da janela, a opacidade da lateral e a barra na cor do site são
// lidos no layout e nas cores da janela.
// As configurações são do perfil normal mesmo abertas numa janela anônima:
// lá as prefs são uma cópia em memória, que some ao fechar a janela.
void RefazerJanelas(Profile* perfil) {
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile()->GetOriginalProfile() != perfil) {
      continue;
    }
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
    views::Widget* widget = view ? view->GetWidget() : nullptr;
    if (!widget) {
      continue;
    }
    if (widget->non_client_view() &&
        widget->non_client_view()->frame_view()) {
      widget->non_client_view()->frame_view()->InvalidateLayout();
      widget->non_client_view()->frame_view()->SchedulePaint();
    }
    view->InvalidateLayout();
    widget->ThemeChanged();
    widget->GetRootView()->SchedulePaint();
  }
}

// As Configurações abertas em cada janela.
std::map<BrowserWindowInterface*, base::WeakPtr<views::Widget>>& Abertas() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*, base::WeakPtr<views::Widget>>>
      abertas;
  return *abertas;
}

// Abre `endereco` numa aba da janela. As Configurações travam a janela (são
// um diálogo dela): fecham antes, senão a aba nova ficava presa atrás.
void AbrirPagina(BrowserWindowInterface* browser, std::string endereco) {
  if (auto it = Abertas().find(browser);
      it != Abertas().end() && it->second) {
    it->second->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
  NavigateParams params(browser, GURL(endereco),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  Navigate(&params);
}

// --- Peças da página ---------------------------------------------------------

// Fundo do cartão de uma seção: um degrau acima do fundo da janela, com
// contorno de 1 px (CardBackground/CardStroke do Dia).
class FundoDoCartao : public views::Background {
 public:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const bool escuro = Escuro(view);
    gfx::RectF area(view->GetLocalBounds());
    area.Inset(0.5f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(escuro ? SkColorSetA(SK_ColorWHITE, 0x0D)
                          : SkColorSetA(SK_ColorWHITE, 0xD0));
    canvas->DrawRoundRect(area, kRaioCartao, flags);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(escuro ? SkColorSetA(SK_ColorWHITE, 0x14)
                          : SkColorSetA(SK_ColorBLACK, 0x10));
    canvas->DrawRoundRect(area, kRaioCartao, flags);
  }
};

class Cartao : public views::View {
  METADATA_HEADER(Cartao, views::View)

 public:
  Cartao() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    SetBackground(std::make_unique<FundoDoCartao>());
  }

  // Linhas separadas por um fio de ponta a ponta, como no Dia.
  views::View* Adicionar(std::unique_ptr<views::View> linha) {
    if (!children().empty()) {
      AddChildView(std::make_unique<views::Separator>())
          ->SetColorId(ui::kColorSeparator);
    }
    return AddChildView(std::move(linha));
  }
};

BEGIN_METADATA(Cartao)
END_METADATA

std::unique_ptr<views::Label> Titulo(std::u16string texto,
                                     int tamanho,
                                     gfx::Font::Weight peso) {
  auto titulo = std::make_unique<views::Label>(texto);
  titulo->SetFontList(Fonte(tamanho, peso));
  titulo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return titulo;
}

// Descrição de várias linhas que acompanha a largura que sobra na linha. O
// controle à direita muda de largura de uma linha pra outra, e com a altura
// sempre a de `kLarguraDescricao` o texto ficava cortado (controles largos,
// como os do importar) ou com sobra embaixo (interruptor).
class TextoDeDescricao : public views::Label {
  METADATA_HEADER(TextoDeDescricao, views::Label)

 public:
  using views::Label::Label;

  void OnBoundsChanged(const gfx::Rect& anteriores) override {
    views::Label::OnBoundsChanged(anteriores);
    if (width() > 0 && width() != largura_) {
      largura_ = width();
      SizeToFit(largura_);
      PreferredSizeChanged();
    }
  }

 private:
  int largura_ = 0;
};

BEGIN_METADATA(TextoDeDescricao)
END_METADATA

std::unique_ptr<views::Label> Descricao(std::u16string texto,
                                        int largura = kLarguraDescricao) {
  auto descricao = std::make_unique<TextoDeDescricao>(texto);
  descricao->SetFontList(Fonte(12, gfx::Font::Weight::NORMAL));
  descricao->SetMultiLine(true);
  descricao->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // A altura é a das linhas na largura fixa; esticada mais larga, o texto
  // ocupa menos linhas e ficava centrado nela (vãos de 25 a 36 px entre o
  // título e a descrição). No alto, o vão é sempre o mesmo. Entrelinha de
  // 16 (a da tipografia do Chromium dava 23 na fonte de 12).
  descricao->SetVerticalAlignment(gfx::ALIGN_TOP);
  descricao->SetLineHeight(16);
  descricao->SetEnabledColor(ui::kColorLabelForegroundSecondary);
  descricao->SizeToFit(largura);
  return descricao;
}

// Linha de um cartão: título (e descrição embaixo) à esquerda, o controle à
// direita.
std::unique_ptr<views::View> Linha(std::u16string titulo,
                                   std::u16string descricao,
                                   std::unique_ptr<views::View> controle) {
  auto linha = std::make_unique<views::View>();
  auto* layout = linha->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(12, 16),
      16));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_minimum_cross_axis_size(44);
  auto* textos = linha->AddChildView(std::make_unique<views::View>());
  textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
  textos->AddChildView(Titulo(titulo, 14, gfx::Font::Weight::NORMAL));
  if (!descricao.empty()) {
    textos->AddChildView(Descricao(descricao));
  }
  layout->SetFlexForView(textos, 1);
  if (controle) {
    linha->AddChildView(std::move(controle));
  }
  return linha;
}

std::unique_ptr<views::View> Interruptor(
    bool ligado,
    std::u16string nome,
    base::RepeatingCallback<void(bool)> mudou) {
  auto interruptor = std::make_unique<views::ToggleButton>();
  interruptor->SetIsOn(ligado);
  interruptor->GetViewAccessibility().SetName(nome);
  views::ToggleButton* ponteiro = interruptor.get();
  interruptor->SetCallback(base::BindRepeating(
      [](views::ToggleButton* botao,
         base::RepeatingCallback<void(bool)> mudou) {
        mudou.Run(botao->GetIsOn());
      },
      base::Unretained(ponteiro), std::move(mudou)));
  return interruptor;
}

// Deslizador com o valor escrito ao lado ("90%", "150 ms").
class Deslizador : public views::View, public views::SliderListener {
  METADATA_HEADER(Deslizador, views::View)

 public:
  Deslizador(int minimo,
             int maximo,
             int passo,
             int valor,
             std::u16string unidade,
             std::u16string nome,
             base::RepeatingCallback<void(int)> mudou)
      : minimo_(minimo),
        maximo_(maximo),
        passo_(passo),
        unidade_(std::move(unidade)),
        mudou_(std::move(mudou)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    slider_ = AddChildView(std::make_unique<views::Slider>(this));
    slider_->SetPreferredSize(gfx::Size(170, 28));
    slider_->GetViewAccessibility().SetName(nome);
    valor_ = AddChildView(std::make_unique<views::Label>());
    valor_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    valor_->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    valor_->SetPreferredSize(gfx::Size(56, 20));
    atual_ = std::clamp(valor, minimo_, maximo_);
    slider_->SetValue(static_cast<float>(atual_ - minimo_) /
                      (maximo_ - minimo_));
    Escrever();
  }

  // views::SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override {
    if (reason != views::SliderChangeReason::kByUser) {
      return;
    }
    const double bruto = value * (maximo_ - minimo_);
    const int valor = std::clamp(
        minimo_ + static_cast<int>(std::lround(bruto / passo_)) * passo_,
        minimo_, maximo_);
    if (valor == atual_) {
      return;
    }
    atual_ = valor;
    Escrever();
    mudou_.Run(atual_);
  }

 private:
  void Escrever() {
    valor_->SetText(base::NumberToString16(atual_) + unidade_);
  }

  int minimo_;
  int maximo_;
  int passo_;
  int atual_ = 0;
  std::u16string unidade_;
  base::RepeatingCallback<void(int)> mudou_;
  raw_ptr<views::Slider> slider_ = nullptr;
  raw_ptr<views::Label> valor_ = nullptr;
};

BEGIN_METADATA(Deslizador)
END_METADATA

std::unique_ptr<views::Combobox> Escolha(
    std::vector<std::u16string> opcoes,
    size_t escolhida,
    std::u16string nome,
    base::RepeatingCallback<void(size_t)> mudou) {
  std::vector<ui::SimpleComboboxModel::Item> itens;
  for (std::u16string& opcao : opcoes) {
    itens.emplace_back(std::move(opcao));
  }
  auto escolha = std::make_unique<views::Combobox>(
      std::make_unique<ui::SimpleComboboxModel>(std::move(itens)));
  escolha->SetSelectedIndex(escolhida);
  escolha->SetAccessibleName(nome);
  escolha->SetSizeToLargestLabel(true);
  views::Combobox* ponteiro = escolha.get();
  escolha->SetCallback(base::BindRepeating(
      [](views::Combobox* combobox,
         base::RepeatingCallback<void(size_t)> mudou) {
        if (std::optional<size_t> indice = combobox->GetSelectedIndex()) {
          mudou.Run(*indice);
        }
      },
      base::Unretained(ponteiro), std::move(mudou)));
  return escolha;
}

std::unique_ptr<views::MdTextButton> Botao(
    std::u16string texto,
    views::Button::PressedCallback callback) {
  auto botao =
      std::make_unique<views::MdTextButton>(std::move(callback), texto);
  botao->SetStyle(ui::ButtonStyle::kTonal);
  return botao;
}

// Uma página: título grande e as seções (título + cartão).
class Pagina : public views::View {
  METADATA_HEADER(Pagina, views::View)

 public:
  explicit Pagina(std::u16string titulo, std::u16string subtitulo = u"") {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::TLBR(20, 28, 28, 28), 0));
    views::Label* rotulo =
        AddChildView(Titulo(titulo, 22, gfx::Font::Weight::SEMIBOLD));
    rotulo->SetFontList(FonteTitulo(22));
    rotulo->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 6, 0));
    if (!subtitulo.empty()) {
      AddChildView(Descricao(subtitulo, 520))
          ->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 4, 0));
    }
  }

  Cartao* Secao(std::u16string titulo) {
    if (!titulo.empty()) {
      auto* cabecalho =
          AddChildView(Titulo(titulo, 15, gfx::Font::Weight::SEMIBOLD));
      cabecalho->SetFontList(FonteTitulo(15));
      cabecalho->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 2)));
      cabecalho->SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(18, 0, 8, 0));
    } else {
      AddChildView(std::make_unique<views::View>())
          ->SetPreferredSize(gfx::Size(0, 14));
    }
    return AddChildView(std::make_unique<Cartao>());
  }

  // Texto pequeno embaixo de um cartão.
  void Rodape(std::u16string texto) {
    AddChildView(Descricao(texto, 520))
        ->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(8, 2, 0, 0));
  }
};

BEGIN_METADATA(Pagina)
END_METADATA

std::unique_ptr<views::View> Rolando(std::unique_ptr<views::View> pagina) {
  auto rolagem = std::make_unique<views::ScrollView>();
  rolagem->SetBackgroundColor(std::nullopt);
  rolagem->SetContents(std::move(pagina));
  // Sem limites de altura a ScrollView não dimensiona o conteúdo (a página
  // ficava com tamanho zero, em branco).
  rolagem->ClipHeightTo(0, kAlturaJanela);
  return rolagem;
}

// --- Geral -------------------------------------------------------------------

// "Importar do Zen": a linha mostra o andamento e o resumo no lugar da
// descrição.
class LinhaDeImportar : public views::View {
  METADATA_HEADER(LinhaDeImportar, views::View)

 public:
  explicit LinhaDeImportar(Profile* perfil) : perfil_(perfil) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(12, 16),
        16));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_minimum_cross_axis_size(44);
    auto* textos = AddChildView(std::make_unique<views::View>());
    textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
    textos->AddChildView(
        Titulo(u"Importar do Zen", 14, gfx::Font::Weight::NORMAL));
    situacao_ = textos->AddChildView(Descricao(
        u"Containers, espaços, sites fixados e pastas, cookies de cada "
        u"container (os logins dos sites), senhas, favoritos e histórico. "
        u"Pode deixar o Zen aberto."));
    layout->SetFlexForView(textos, 1);
    botao_ = AddChildView(Botao(
        u"Importar", base::BindRepeating(&LinhaDeImportar::Importar,
                                         base::Unretained(this))));
    botao_->SetEnabled(false);
    importar_zen::ProcurarZen(base::BindOnce(&LinhaDeImportar::Achou,
                                             weak_factory_.GetWeakPtr()));
  }

 private:
  void Achou(bool achou) {
    botao_->SetEnabled(achou);
    if (!achou) {
      Escrever(u"O Zen não foi encontrado neste computador.");
    }
  }

  void Importar() {
    botao_->SetEnabled(false);
    importar_zen::Importar(
        perfil_,
        base::BindRepeating(&LinhaDeImportar::Escrever,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&LinhaDeImportar::Pronto, weak_factory_.GetWeakPtr()));
  }

  void Pronto(const importar_zen::Resumo& resumo) {
    botao_->SetEnabled(true);
    if (!resumo.ok) {
      Escrever(resumo.erro);
      return;
    }
    Escrever(u"Pronto: " + base::NumberToString16(resumo.cookies) +
             u" cookies, " + base::NumberToString16(resumo.containers) +
             u" containers e " + base::NumberToString16(resumo.espacos) +
             u" espaços novos, " + base::NumberToString16(resumo.fixados) +
             u" sites fixados, mais senhas, favoritos e histórico. Os sites "
             u"que guardam o login fora dos cookies podem pedir login de "
             u"novo.");
  }

  void Escrever(const std::u16string& texto) {
    situacao_->SetText(texto);
    PreferredSizeChanged();
  }

  raw_ptr<Profile> perfil_;
  raw_ptr<views::Label> situacao_ = nullptr;
  raw_ptr<views::View> botao_ = nullptr;
  base::WeakPtrFactory<LinhaDeImportar> weak_factory_{this};
};

BEGIN_METADATA(LinhaDeImportar)
END_METADATA

// Favoritos e histórico de um navegador do Chromium achado aqui (Chrome,
// Brave, Vivaldi...; importar_chromium.h).
class LinhaDeImportarChromium : public views::View {
  METADATA_HEADER(LinhaDeImportarChromium, views::View)

 public:
  explicit LinhaDeImportarChromium(Profile* perfil) : perfil_(perfil) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(12, 16),
        16));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_minimum_cross_axis_size(44);
    auto* textos = AddChildView(std::make_unique<views::View>());
    textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
    textos->AddChildView(Titulo(u"Importar do Chrome, Brave ou Vivaldi", 14,
                                gfx::Font::Weight::NORMAL));
    situacao_ = textos->AddChildView(
        Descricao(u"Procurando os navegadores deste computador…"));
    layout->SetFlexForView(textos, 1);
    controles_ = AddChildView(std::make_unique<views::View>());
    controles_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
    controles_->SetVisible(false);
    importar_chromium::Procurar(base::BindOnce(
        &LinhaDeImportarChromium::Achou, weak_factory_.GetWeakPtr()));
  }

 private:
  void Achou(std::vector<importar_chromium::Origem> origens) {
    origens_ = std::move(origens);
    if (origens_.empty()) {
      Escrever(u"Nenhum navegador do Chromium foi encontrado neste "
               u"computador.");
      return;
    }
    Escrever(kExplicacao);
    std::vector<std::u16string> nomes;
    for (const importar_chromium::Origem& origem : origens_) {
      nomes.push_back(origem.nome);
    }
    controles_->AddChildView(Escolha(
        nomes, 0, u"De qual navegador",
        base::BindRepeating(
            [](LinhaDeImportarChromium* linha, size_t escolha) {
              linha->escolhida_ = escolha;
            },
            base::Unretained(this))));
    botao_ = controles_->AddChildView(Botao(
        u"Importar", base::BindRepeating(&LinhaDeImportarChromium::Importar,
                                         base::Unretained(this))));
    controles_->SetVisible(true);
    PreferredSizeChanged();
  }

  void Importar() {
    if (escolhida_ >= origens_.size()) {
      return;
    }
    botao_->SetEnabled(false);
    Escrever(u"Importando de " + origens_[escolhida_].nome + u"…");
    importar_chromium::Importar(
        perfil_, origens_[escolhida_],
        base::BindOnce(&LinhaDeImportarChromium::Pronto,
                       weak_factory_.GetWeakPtr()));
  }

  void Pronto(const importar_chromium::Resumo& resumo) {
    botao_->SetEnabled(true);
    if (!resumo.ok) {
      Escrever(resumo.erro);
      return;
    }
    Escrever(u"Pronto: " + base::NumberToString16(resumo.favoritos) +
             u" favoritos (em Outros favoritos) e " +
             base::NumberToString16(resumo.historico) +
             u" páginas do histórico.");
  }

  void Escrever(const std::u16string& texto) {
    situacao_->SetText(texto);
    PreferredSizeChanged();
  }

  static constexpr char16_t kExplicacao[] =
      u"Favoritos e histórico (as senhas e os logins ficam: o Chrome os "
      u"guarda cifrados). Pode deixar o outro navegador aberto.";

  raw_ptr<Profile> perfil_;
  raw_ptr<views::Label> situacao_ = nullptr;
  raw_ptr<views::View> controles_ = nullptr;
  raw_ptr<views::MdTextButton> botao_ = nullptr;
  std::vector<importar_chromium::Origem> origens_;
  size_t escolhida_ = 0;
  base::WeakPtrFactory<LinhaDeImportarChromium> weak_factory_{this};
};

BEGIN_METADATA(LinhaDeImportarChromium)
END_METADATA

// Exportar e importar o jeito do Norte (exportar.h): espaços, fixados,
// containers, atalhos e opções num arquivo.
// Depois de importar: refaz a página Geral da janela de configurações em que
// `de_dentro` está (os controles ficavam com os valores de antes) e põe a
// frase de volta na linha de exportar. Definida depois da janela.
void RefazerDepoisDeImportar(views::View* de_dentro, std::u16string frase);

class LinhaDeExportar : public views::View {
  METADATA_HEADER(LinhaDeExportar, views::View)

 public:
  explicit LinhaDeExportar(BrowserWindowInterface* browser)
      : browser_(browser) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(12, 16),
        16));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_minimum_cross_axis_size(44);
    auto* textos = AddChildView(std::make_unique<views::View>());
    textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
    textos->AddChildView(Titulo(u"Levar pra outra máquina", 14,
                                gfx::Font::Weight::NORMAL));
    situacao_ = textos->AddChildView(Descricao(
        u"Espaços, sites fixados e favoritos, containers, atalhos e as opções, "
        u"num arquivo. Importar substitui os deste perfil."));
    layout->SetFlexForView(textos, 1);
    AddChildView(Botao(u"Exportar…",
                       base::BindRepeating(&LinhaDeExportar::Exportar,
                                           base::Unretained(this))));
    AddChildView(Botao(u"Importar…",
                       base::BindRepeating(&LinhaDeExportar::Importar,
                                           base::Unretained(this))));
  }

  // A frase embaixo do título: o resultado de exportar ou importar.
  void Escrever(const std::u16string& texto) {
    situacao_->SetText(texto);
    PreferredSizeChanged();
  }

 private:
  void Exportar() {
    exportar::Exportar(
        browser_,
        base::BindOnce(
            [](base::WeakPtr<LinhaDeExportar> linha, bool ok,
               const std::u16string& frase) {
              if (linha) {
                linha->Escrever(frase);
              }
            },
            weak_factory_.GetWeakPtr()));
  }

  void Importar() {
    exportar::Importar(browser_,
                       base::BindOnce(&LinhaDeExportar::Importou,
                                      weak_factory_.GetWeakPtr()));
  }

  void Importou(bool ok, const std::u16string& frase) {
    if (ok) {
      RefazerDepoisDeImportar(this, frase);
    } else {
      Escrever(frase);
    }
  }

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<views::Label> situacao_ = nullptr;
  base::WeakPtrFactory<LinhaDeExportar> weak_factory_{this};
};

BEGIN_METADATA(LinhaDeExportar)
END_METADATA

// Versão instalada e a atualização inteira, sem sair daqui: procurar,
// atualizar (a senha vai na janela do sistema) e reiniciar. Segue o mesmo
// andamento do aviso no pé da lateral, que fica fora de alcance com as
// Configurações abertas (elas travam a janela).
class LinhaDeVersao : public views::View {
  METADATA_HEADER(LinhaDeVersao, views::View)

 public:
  explicit LinhaDeVersao(BrowserWindowInterface* browser)
      : browser_(browser),
        perfil_(browser->GetProfile()->GetOriginalProfile()) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(12, 16),
        16));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_minimum_cross_axis_size(44);
    auto* textos = AddChildView(std::make_unique<views::View>());
    textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
    textos->AddChildView(
        Titulo(u"Versão instalada", 14, gfx::Font::Weight::NORMAL));
    situacao_ = textos->AddChildView(Descricao(u"…"));
    notas_ = textos->AddChildView(std::make_unique<views::Link>(u"O que mudou"));
    notas_->SetFontList(Fonte(12, gfx::Font::Weight::NORMAL));
    notas_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    notas_->SetCallback(base::BindRepeating(
        [](BrowserWindowInterface* browser) {
          AbrirPagina(browser, atualizador::Agora().pagina);
        },
        browser));
    layout->SetFlexForView(textos, 1);
    botao_ = AddChildView(Botao(
        u"Procurar agora", base::BindRepeating(&LinhaDeVersao::Apertou,
                                               base::Unretained(this))));
    inscricao_ = atualizador::AoMudar(base::BindRepeating(
        &LinhaDeVersao::Mostrar, base::Unretained(this)));
    atualizador::LerVersaoInstalada(base::BindOnce(
        &LinhaDeVersao::Versao, weak_factory_.GetWeakPtr()));
  }

 private:
  void Versao(const std::string& versao) {
    versao_ = base::UTF8ToUTF16(versao);
    frase_ = versao.empty() ? u"Cópia de desenvolvimento (sem atualização "
                              u"automática)."
                            : versao_;
    Mostrar();
  }

  void Apertou() {
    switch (atualizador::Agora().etapa) {
      case atualizador::Etapa::kNada:
        procurando_ = true;
        Mostrar();
        atualizador::ProcurarAgora(
            perfil_, base::BindOnce(&LinhaDeVersao::Resposta,
                                    weak_factory_.GetWeakPtr()));
        break;
      case atualizador::Etapa::kDisponivel:
      case atualizador::Etapa::kErro:
        atualizador::Atualizar();
        break;
      case atualizador::Etapa::kInstalada:
        atualizador::Reiniciar();
        break;
      case atualizador::Etapa::kBaixando:
      case atualizador::Etapa::kInstalando:
        break;
    }
  }

  void Resposta(const std::u16string& frase) {
    procurando_ = false;
    frase_ = frase;
    Mostrar();
  }

  // O texto e o botão da etapa em que a atualização está.
  void Mostrar() {
    const atualizador::Andamento andamento = atualizador::Agora();
    const std::u16string nova = base::UTF8ToUTF16(andamento.nova);
    std::u16string texto;
    std::u16string botao;
    bool ligado = true;
    bool destaque = false;
    switch (andamento.etapa) {
      case atualizador::Etapa::kNada:
        texto = procurando_ ? u"Procurando…" : frase_;
        botao = u"Procurar agora";
        ligado = !procurando_;
        break;
      case atualizador::Etapa::kDisponivel:
        texto = u"Versão nova: " + nova +
                (versao_.empty() ? u"." : u" (esta é a " + versao_ + u").");
        botao = u"Atualizar agora";
        destaque = true;
        break;
      case atualizador::Etapa::kBaixando:
        texto = u"Baixando a versão " + nova + u"…" +
                (andamento.porcento >= 0
                     ? u" " + base::NumberToString16(andamento.porcento) +
                           u"%"
                     : u"");
        botao = u"Atualizando…";
        ligado = false;
        break;
      case atualizador::Etapa::kInstalando:
        texto = u"Digite a senha na janela do sistema pra instalar a "
                u"versão " +
                nova + u".";
        botao = u"Atualizando…";
        ligado = false;
        break;
      case atualizador::Etapa::kInstalada:
        texto = u"Versão " + nova +
                u" instalada. Reinicie pra usar: todas as janelas fecham e "
                u"abrem de novo, com as abas.";
        botao = u"Reiniciar agora";
        destaque = true;
        break;
      case atualizador::Etapa::kErro:
        texto = andamento.erro;
        botao = u"Tentar de novo";
        break;
    }
    botao_->SetText(botao);
    botao_->SetEnabled(ligado);
    botao_->SetStyle(destaque ? ui::ButtonStyle::kProminent
                              : ui::ButtonStyle::kTonal);
    situacao_->SetText(texto);
    notas_->SetVisible((andamento.etapa == atualizador::Etapa::kDisponivel ||
                        andamento.etapa == atualizador::Etapa::kInstalada) &&
                       !andamento.pagina.empty());
    PreferredSizeChanged();
  }

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<Profile> perfil_;
  raw_ptr<views::Label> situacao_ = nullptr;
  raw_ptr<views::Link> notas_ = nullptr;
  raw_ptr<views::MdTextButton> botao_ = nullptr;
  std::u16string versao_;
  // A resposta do último "Procurar agora" (ou a versão instalada).
  std::u16string frase_ = u"…";
  bool procurando_ = false;
  base::CallbackListSubscription inscricao_;
  base::WeakPtrFactory<LinhaDeVersao> weak_factory_{this};
};

BEGIN_METADATA(LinhaDeVersao)
END_METADATA

std::unique_ptr<views::View> PaginaGeral(BrowserWindowInterface* browser) {
  Profile* perfil = browser->GetProfile()->GetOriginalProfile();
  auto pagina = std::make_unique<Pagina>(u"Geral");

  Cartao* abrir = pagina->Secao(u"Ao abrir o navegador");
  const bool continuar = SessionStartupPref::GetStartupPref(perfil).type ==
                         SessionStartupPref::LAST;
  abrir->Adicionar(Linha(
      u"Começar com", u"O que aparece quando o Norte abre.",
      Escolha({u"Uma nova aba", u"As abas de onde parei"}, continuar ? 1 : 0,
              u"Começar com",
              base::BindRepeating(
                  [](Profile* perfil, size_t escolha) {
                    SessionStartupPref::SetStartupPref(
                        perfil,
                        SessionStartupPref(escolha == 1
                                               ? SessionStartupPref::LAST
                                               : SessionStartupPref::DEFAULT));
                  },
                  perfil))));

  Cartao* links = pagina->Secao(u"Links");
  links->Adicionar(Linha(
      u"Prévia com Shift + clique",
      u"Shift + clique num link abre a página num cartão por cima da aba, "
      u"sem sair dela. Esc fecha; dali dá pra abrir como aba ou dividir a "
      u"tela. Desligado, abre numa janela nova.",
      Interruptor(perfil->GetPrefs()->GetBoolean(espacos::kPreviaPref),
                  u"Prévia com Shift + clique",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(espacos::kPreviaPref,
                                                       ligado);
                      },
                      perfil))));

  // No Local State: a lista de recursos é montada antes das prefs do perfil.
  Cartao* memoria = pagina->Secao(u"Memória");
  memoria->Adicionar(Linha(
      u"Congelar as abas dos outros espaços",
      u"Depois de 15 minutos sem uso, elas param de rodar (gastam menos "
      u"processador e bateria) e voltam na hora ao abrir. Música, chamada, "
      u"upload e sites com notificação continuam. Vale ao reiniciar o Norte.",
      Interruptor(g_browser_process->local_state()->GetBoolean(
                      kCongelarOutrosEspacosPref),
                  u"Congelar as abas dos outros espaços",
                  base::BindRepeating([](bool ligado) {
                    g_browser_process->local_state()->SetBoolean(
                        kCongelarOutrosEspacosPref, ligado);
                  }))));

  // Vídeo em janelinha (PiP) sozinho ao sair da aba: a escolha é o padrão do
  // Chromium pra todos os sites (cada site ainda pode ter a dele).
  Cartao* midia = pagina->Secao(u"Mídia");
  constexpr ContentSetting kEscolhasDoPip[] = {
      CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK};
  HostContentSettingsMap* sites =
      HostContentSettingsMapFactory::GetForProfile(perfil);
  const ContentSetting pip = sites->GetDefaultContentSetting(
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
  midia->Adicionar(Linha(
      u"Vídeo em janelinha ao trocar de aba",
      u"O vídeo que está tocando passa pra uma janelinha por cima quando "
      u"você troca de aba ou de espaço, e volta pra página quando você volta.",
      Escolha({u"Perguntar a cada site", u"Sempre", u"Nunca"},
              pip == CONTENT_SETTING_ALLOW   ? 1
              : pip == CONTENT_SETTING_BLOCK ? 2
                                             : 0,
              u"Vídeo em janelinha ao trocar de aba",
              base::BindRepeating(
                  [](Profile* perfil, size_t escolha) {
                    HostContentSettingsMapFactory::GetForProfile(perfil)
                        ->SetDefaultContentSetting(
                            ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
                            kEscolhasDoPip[std::min<size_t>(escolha, 2)]);
                  },
                  perfil))));

  Cartao* mouse = pagina->Secao(u"Mouse");
  mouse->Adicionar(Linha(
      u"Rolar com o botão do meio",
      u"Clique com o botão do meio na página (fora de link e de campo) e "
      u"mova o ponteiro pra rolar. Nos campos, o botão do meio continua "
      u"colando a seleção. Vale ao reiniciar o Norte.",
      Interruptor(
          g_browser_process->local_state()->GetBoolean(kRolagemComOMeioPref),
          u"Rolar com o botão do meio", base::BindRepeating([](bool ligado) {
            g_browser_process->local_state()->SetBoolean(kRolagemComOMeioPref,
                                                         ligado);
          }))));

  Cartao* atualizacoes = pagina->Secao(u"Atualizações");
  atualizacoes->Adicionar(Linha(
      u"Procurar versão nova",
      u"Ao abrir o navegador e a cada 6 horas, procura uma versão nova no "
      u"GitHub e mostra um aviso no pé da lateral. Dá pra atualizar por ali "
      u"ou por aqui.",
      Interruptor(perfil->GetPrefs()->GetBoolean(
                      espacos::kProcurarAtualizacoesPref),
                  u"Procurar versão nova",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kProcurarAtualizacoesPref, ligado);
                      },
                      perfil))));
  atualizacoes->Adicionar(std::make_unique<LinhaDeVersao>(browser));

  Cartao* importar = pagina->Secao(u"Trazer de outro navegador");
  importar->Adicionar(std::make_unique<LinhaDeImportar>(perfil));
  importar->Adicionar(std::make_unique<LinhaDeImportarChromium>(perfil));
  importar->Adicionar(std::make_unique<LinhaDeExportar>(browser));
  importar->Adicionar(Linha(
      u"Importar do Firefox ou de um arquivo de favoritos",
      u"Pelo importador do Chromium: favoritos, histórico e senhas.",
      Botao(u"Abrir", base::BindRepeating(
                          &AbrirPagina, browser,
                          std::string("chrome://settings/importData")))));

  Cartao* janela = pagina->Secao(u"Janela");
  janela->Adicionar(Linha(
      u"Botões da janela",
      u"Fechar, minimizar e maximizar no canto da barra. Desligados, a "
      u"janela fecha pelo atalho do sistema.",
      Interruptor(espacos::MostrarBotoesDaJanela(perfil), u"Botões da janela",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kBotoesDaJanelaPref, ligado);
                        RefazerJanelas(perfil);
                      },
                      perfil))));

  Cartao* chromium = pagina->Secao(u"Mais configurações");
  chromium->Adicionar(Linha(
      u"Configurações avançadas",
      u"Senhas, privacidade, downloads, idioma e mecanismo de busca.",
      Botao(u"Abrir", base::BindRepeating(&AbrirPagina, browser,
                                          std::string("chrome://settings")))));
  chromium->Adicionar(Linha(
      u"Extensões", std::u16string(),
      Botao(u"Abrir", base::BindRepeating(&AbrirPagina, browser,
                                          std::string("chrome://extensions")))));
  return pagina;
}

// --- Privacidade ------------------------------------------------------------

// Os servidores do DNS seguro que dá pra escolher (o "automático" usa o do
// sistema, se ele tiver).
constexpr char kDnsCloudflare[] = "https://chrome.cloudflare-dns.com/dns-query";
constexpr char kDnsQuad9[] = "https://dns.quad9.net/dns-query";

std::unique_ptr<views::View> PaginaPrivacidade(
    BrowserWindowInterface* browser) {
  Profile* perfil = browser->GetProfile()->GetOriginalProfile();
  auto pagina = std::make_unique<Pagina>(u"Privacidade");

  Cartao* bloqueio = pagina->Secao(u"Bloqueador");
  bloqueio->Adicionar(Linha(
      u"Bloquear anúncios e rastreadores",
      u"Com as listas EasyList e EasyPrivacy: as páginas carregam menos "
      u"coisa e gastam menos. Pra liberar só um site, clique com o botão "
      u"direito na página → Desligar o bloqueador neste site.",
      Interruptor(bloqueador::Ligado(perfil),
                  u"Bloquear anúncios e rastreadores",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        bloqueador::Ligar(perfil, ligado);
                      },
                      perfil))));
  bloqueio->Adicionar(Linha(
      u"Esconder avisos de cookies",
      u"Some com os avisos de consentimento de cookies da maioria dos sites "
      u"(a lista EasyList Cookie), nas páginas que abrirem depois. Se um "
      u"site travar a rolagem por isso, desligue o bloqueador nele.",
      Interruptor(perfil->GetPrefs()->GetBoolean(espacos::kAvisosDeCookiesPref),
                  u"Esconder avisos de cookies",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kAvisosDeCookiesPref, ligado);
                      },
                      perfil))));

  Cartao* cookies = pagina->Secao(u"Cookies");
  cookies->Adicionar(Linha(
      u"Bloquear cookies de terceiros",
      u"Os de outros sites dentro da página (anúncios, botões de redes "
      u"sociais). Alguns logins embutidos (Google, Microsoft, gov.br) podem "
      u"parar: dá pra liberar um site pelo ícone dele na barra.",
      Interruptor(
          perfil->GetPrefs()->GetInteger(prefs::kCookieControlsMode) ==
              static_cast<int>(
                  content_settings::CookieControlsMode::kBlockThirdParty),
          u"Bloquear cookies de terceiros",
          base::BindRepeating(
              [](Profile* perfil, bool ligado) {
                perfil->GetPrefs()->SetInteger(
                    prefs::kCookieControlsMode,
                    static_cast<int>(
                        ligado ? content_settings::CookieControlsMode::
                                     kBlockThirdParty
                               : content_settings::CookieControlsMode::
                                     kIncognitoOnly));
              },
              perfil))));

  Cartao* conexao = pagina->Secao(u"Conexão");
  conexao->Adicionar(Linha(
      u"Avisar antes de abrir um site sem HTTPS",
      u"Tenta sempre a versão com cadeado e avisa quando o site só tem a "
      u"sem.",
      Interruptor(perfil->GetPrefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled),
                  u"Avisar antes de abrir um site sem HTTPS",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            prefs::kHttpsOnlyModeEnabled, ligado);
                      },
                      perfil))));
  // O DNS seguro é do navegador inteiro (Local State), não do perfil.
  PrefService* local = g_browser_process->local_state();
  const std::string modo = local->GetString(prefs::kDnsOverHttpsMode);
  const std::string servidor = local->GetString(prefs::kDnsOverHttpsTemplates);
  size_t dns = 0;
  if (modo == "off") {
    dns = 3;
  } else if (modo == "secure") {
    dns = servidor == kDnsQuad9 ? 2 : 1;
  }
  conexao->Adicionar(Linha(
      u"DNS seguro",
      u"O endereço de cada site é procurado com criptografia: o provedor de "
      u"internet não vê quais sites você abre.",
      Escolha({u"Automático", u"Cloudflare", u"Quad9", u"Desligado"}, dns,
              u"DNS seguro", base::BindRepeating([](size_t escolha) {
                PrefService* local = g_browser_process->local_state();
                switch (escolha) {
                  case 0:
                    local->SetString(prefs::kDnsOverHttpsMode, "automatic");
                    local->ClearPref(prefs::kDnsOverHttpsTemplates);
                    break;
                  case 1:
                  case 2:
                    local->SetString(prefs::kDnsOverHttpsMode, "secure");
                    local->SetString(prefs::kDnsOverHttpsTemplates,
                                     escolha == 1 ? kDnsCloudflare : kDnsQuad9);
                    break;
                  default:
                    local->SetString(prefs::kDnsOverHttpsMode, "off");
                    break;
                }
              }))));
  conexao->Adicionar(Linha(
      u"Esconder o IP local nas chamadas",
      u"Nas chamadas de vídeo e de voz pelo navegador (WebRTC), os sites não "
      u"veem o endereço da sua rede local.",
      Interruptor(perfil->GetPrefs()->GetString(
                      prefs::kWebRTCIPHandlingPolicy) ==
                      "default_public_interface_only",
                  u"Esconder o IP local nas chamadas",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetString(
                            prefs::kWebRTCIPHandlingPolicy,
                            ligado ? "default_public_interface_only"
                                   : "default");
                      },
                      perfil))));
  pagina->Rodape(
      u"Mais opções em Geral → Configurações avançadas (Privacidade e "
      u"segurança).");
  return pagina;
}

// --- Aparência ---------------------------------------------------------------

std::unique_ptr<views::View> PaginaAparencia(BrowserWindowInterface* browser) {
  Profile* perfil = browser->GetProfile()->GetOriginalProfile();
  auto pagina = std::make_unique<Pagina>(u"Aparência");

  Cartao* tema = pagina->Secao(u"Tema");
  ThemeService* temas = ThemeServiceFactory::GetForProfile(perfil);
  tema->Adicionar(Linha(
      u"Modo", u"Claro, escuro ou o mesmo do sistema.",
      Escolha({u"Sistema", u"Claro", u"Escuro"},
              static_cast<size_t>(temas->GetBrowserColorScheme()), u"Modo",
              base::BindRepeating(
                  [](ThemeService* temas, size_t escolha) {
                    temas->SetBrowserColorScheme(
                        static_cast<ThemeService::BrowserColorScheme>(escolha));
                  },
                  base::Unretained(temas)))));

  Cartao* janela = pagina->Secao(u"Janela");
  janela->Adicionar(Linha(
      u"Espessura da moldura", u"A borda em volta da barra e da página.",
      std::make_unique<Deslizador>(
          0, 12, 1, espacos::Moldura(perfil), u" px", u"Espessura da moldura",
          base::BindRepeating(
              [](Profile* perfil, int valor) {
                perfil->GetPrefs()->SetInteger(espacos::kMolduraPref, valor);
                RefazerJanelas(perfil);
              },
              perfil))));
  janela->Adicionar(Linha(
      u"Barra e abas na tela cheia",
      u"Na tela cheia (F11 ou o Super+F do sistema), a barra de endereço e "
      u"a lateral continuam. Desligado, somem como no Chrome. Vídeo em tela "
      u"cheia sempre esconde tudo.",
      Interruptor(
          perfil->GetPrefs()->GetBoolean(espacos::kBarraNaTelaCheiaPref),
          u"Barra e abas na tela cheia",
          base::BindRepeating(
              [](Profile* perfil, bool ligado) {
                perfil->GetPrefs()->SetBoolean(espacos::kBarraNaTelaCheiaPref,
                                               ligado);
                RefazerJanelas(perfil);
              },
              perfil))));
  janela->Adicionar(Linha(
      u"Barra na cor do site",
      u"A barra de endereço pega a cor do topo da página aberta.",
      Interruptor(
          perfil->GetPrefs()->GetBoolean(espacos::kBarraNaCorDoSitePref),
          u"Barra na cor do site",
          base::BindRepeating(
              [](Profile* perfil, bool ligado) {
                perfil->GetPrefs()->SetBoolean(espacos::kBarraNaCorDoSitePref,
                                               ligado);
                RefazerJanelas(perfil);
              },
              perfil))));
  janela->Adicionar(Linha(
      u"Esconder a barra de endereço",
      u"A página ocupa o cartão inteiro. A barra aparece ao encostar o "
      u"ponteiro no alto da página, e o Ctrl+L e a nova aba mostram ela "
      u"enquanto você digita o endereço.",
      Interruptor(
          perfil->GetPrefs()->GetBoolean(espacos::kBarraEscondidaPref),
          u"Esconder a barra de endereço",
          base::BindRepeating(
              [](Profile* perfil, bool ligado) {
                barra::Definir(perfil, ligado);
              },
              perfil))));

  // O mesmo do "Personalizar" no canto da nova aba (inicio.cc).
  Cartao* nova_aba = pagina->Secao(u"Nova aba");
  nova_aba->Adicionar(Linha(
      u"Hora", std::u16string(),
      Interruptor(perfil->GetPrefs()->GetBoolean(espacos::kInicioHoraPref),
                  u"Hora na nova aba",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kInicioHoraPref, ligado);
                      },
                      perfil))));
  nova_aba->Adicionar(Linha(
      u"Data", std::u16string(),
      Interruptor(perfil->GetPrefs()->GetBoolean(espacos::kInicioDataPref),
                  u"Data na nova aba",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kInicioDataPref, ligado);
                      },
                      perfil))));
  nova_aba->Adicionar(Linha(
      u"Atalhos",
      u"Os sites que você mais visita, uma lista sua (adicione pelo + na "
      u"nova aba) ou nada.",
      Escolha({u"Mais visitados", u"Meus atalhos", u"Nenhum"},
              static_cast<size_t>(inicio::LerAtalhos(perfil->GetPrefs())),
              u"Atalhos na nova aba",
              base::BindRepeating(
                  [](Profile* perfil, size_t escolha) {
                    inicio::DefinirAtalhos(
                        perfil->GetPrefs(),
                        static_cast<inicio::Atalhos>(escolha));
                  },
                  perfil))));

  pagina->Rodape(u"A cor de cada espaço (que tinge a janela) fica em Espaços.");
  return pagina;
}

// --- Lateral -----------------------------------------------------------------

void DefinirInteiro(Profile* perfil, const char* pref, int valor) {
  perfil->GetPrefs()->SetInteger(pref, valor);
  RefazerJanelas(perfil);
}

std::unique_ptr<views::View> PaginaLateral(BrowserWindowInterface* browser) {
  Profile* perfil = browser->GetProfile()->GetOriginalProfile();
  auto pagina = std::make_unique<Pagina>(
      u"Lateral",
      u"Escondida (Ctrl+S), a lateral aparece por cima da página quando o "
      u"ponteiro encosta na borda esquerda.");

  Cartao* escondida = pagina->Secao(u"Lateral escondida");
  escondida->Adicionar(Linha(
      u"Mostrar ao encostar na borda", u"Desligado, ela só volta pelo atalho.",
      Interruptor(espacos::MostrarNaBorda(perfil),
                  u"Mostrar ao encostar na borda",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kMostrarNaBordaPref, ligado);
                      },
                      perfil))));
  escondida->Adicionar(Linha(
      u"Abrir inteira mesmo passando rápido",
      u"Ligado, um toque rápido na borda abre a lateral toda antes de contar "
      u"o tempo pra esconder. Desligado, ela abre um pedaço e volta.",
      Interruptor(espacos::AbrirInteira(perfil),
                  u"Abrir inteira mesmo passando rápido",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kAbrirInteiraPref, ligado);
                      },
                      perfil))));
  escondida->Adicionar(Linha(
      u"Esconder ao ir pra página",
      u"Quanto ela espera depois que o ponteiro sai dela pra página: folga "
      u"pra tirar sem querer e voltar.",
      std::make_unique<Deslizador>(
          0, 2000, 50, espacos::EsconderDepoisMs(perfil), u" ms",
          u"Esconder ao ir pra página",
          base::BindRepeating(&DefinirInteiro, perfil,
                              espacos::kEsconderDepoisPref))));
  escondida->Adicionar(Linha(
      u"Esconder ao sair da janela",
      u"Quando o ponteiro sai da janela (ou vai pra outra janela). Também é o "
      u"tempo que ela fica aberta ao jogar o ponteiro pro canto da tela.",
      std::make_unique<Deslizador>(
          0, 2000, 50, espacos::EsconderForaMs(perfil), u" ms",
          u"Esconder ao sair da janela",
          base::BindRepeating(&DefinirInteiro, perfil,
                              espacos::kEsconderForaPref))));

  Cartao* flutuante = pagina->Secao(u"Por cima da página");
  flutuante->Adicionar(Linha(
      u"Opacidade", u"Menos opaca, dá pra ver a página atrás.",
      std::make_unique<Deslizador>(
          20, 100, 5, espacos::FlutuanteOpacidade(perfil), u"%", u"Opacidade",
          base::BindRepeating(&DefinirInteiro, perfil,
                              espacos::kFlutuanteOpacidadePref))));
  flutuante->Adicionar(Linha(
      u"Desfoque do fundo", u"Embaça a página atrás da lateral (vidro).",
      std::make_unique<Deslizador>(
          0, 40, 2, espacos::FlutuanteDesfoque(perfil), u" px",
          u"Desfoque do fundo",
          base::BindRepeating(&DefinirInteiro, perfil,
                              espacos::kFlutuanteDesfoquePref))));

  Cartao* favoritos = pagina->Secao(u"Favoritos");
  favoritos->Adicionar(Linha(
      u"Quadro de favoritos vazio",
      u"O quadro tracejado no alto da lateral, quando o espaço ainda não tem "
      u"favoritos.",
      Interruptor(espacos::MostrarQuadroDeFavoritos(perfil),
                  u"Quadro de favoritos vazio",
                  base::BindRepeating(
                      [](Profile* perfil, bool ligado) {
                        perfil->GetPrefs()->SetBoolean(
                            espacos::kQuadroDeFavoritosPref, ligado);
                      },
                      perfil))));
  return pagina;
}

// --- Espaços -----------------------------------------------------------------

// Itens do seletor de container: "Sem container" e os do perfil.
std::unique_ptr<ui::SimpleComboboxModel> ModeloDeContainers(
    const std::vector<containers::Container>& lista) {
  std::vector<ui::SimpleComboboxModel::Item> itens;
  itens.emplace_back(u"Sem container");
  for (const containers::Container& container : lista) {
    itens.emplace_back(base::UTF8ToUTF16(container.name), std::u16string(),
                       containers::Bolinha(container.color));
  }
  return std::make_unique<ui::SimpleComboboxModel>(std::move(itens));
}

size_t IndiceDoContainer(const std::vector<containers::Container>& lista,
                         const std::string& id) {
  for (size_t i = 0; i < lista.size(); ++i) {
    if (lista[i].id == id) {
      return i + 1;
    }
  }
  return 0;
}

// Bolinha da cor do espaço (20, como a lista de perfis do Dia).
class BolinhaDoEspaco : public views::View {
  METADATA_HEADER(BolinhaDoEspaco, views::View)

 public:
  explicit BolinhaDoEspaco(std::string cor) : cor_(std::move(cor)) {
    SetPreferredSize(gfx::Size(20, 20));
    SetCanProcessEventsWithinSubtree(false);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(containers::Cor(cor_));
    canvas->DrawCircle(gfx::PointF(10, 10), 10, flags);
  }

 private:
  std::string cor_;
};

BEGIN_METADATA(BolinhaDoEspaco)
END_METADATA

// Linha clicável da lista de espaços (abre o detalhe).
class LinhaDoEspaco : public views::Button {
  METADATA_HEADER(LinhaDoEspaco, views::Button)

 public:
  LinhaDoEspaco(const espacos::Espaco& espaco,
                std::u16string container,
                PressedCallback callback)
      : views::Button(std::move(callback)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(12, 16),
        12));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_minimum_cross_axis_size(44);
    AddChildView(std::make_unique<BolinhaDoEspaco>(espaco.cor));
    std::u16string nome = base::UTF8ToUTF16(espaco.nome);
    if (!espaco.icone.empty()) {
      nome = base::UTF8ToUTF16(espaco.icone) + u"  " + nome;
    }
    auto* rotulo = AddChildView(Titulo(nome, 14, gfx::Font::Weight::NORMAL));
    rotulo->SetCanProcessEventsWithinSubtree(false);
    layout->SetFlexForView(rotulo, 1);
    auto* detalhe = AddChildView(std::make_unique<views::Label>(container));
    detalhe->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    detalhe->SetCanProcessEventsWithinSubtree(false);
    AddChildView(std::make_unique<views::ImageView>(
                     ui::ImageModel::FromVectorIcon(
                         kChevronRightIcon, ui::kColorIconSecondary, 16)))
        ->SetCanProcessEventsWithinSubtree(false);
    GetViewAccessibility().SetName(base::UTF8ToUTF16(espaco.nome));
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColor(ui::kColorSysOnSurface);
  }
};

BEGIN_METADATA(LinhaDoEspaco)
END_METADATA

class PaginaDeEspacos : public views::View,
                        public views::TextfieldController {
  METADATA_HEADER(PaginaDeEspacos, views::View)

 public:
  explicit PaginaDeEspacos(BrowserWindowInterface* browser)
      : browser_(browser), perfil_(browser->GetProfile()->GetOriginalProfile()) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    prefs_.Init(perfil_->GetPrefs());
    prefs_.Add(espacos::kListaPref,
               base::BindRepeating(&PaginaDeEspacos::QuandoMudar,
                                   base::Unretained(this)));
    prefs_.Add(containers::kContainersPref,
               base::BindRepeating(&PaginaDeEspacos::QuandoMudar,
                                   base::Unretained(this)));
    Lista();
  }

  // views::TextfieldController: nome e emoji mudam enquanto digita.
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, aberto_);
    if (!espaco) {
      return;
    }
    if (sender == nome_) {
      espaco->nome = base::UTF16ToUTF8(new_contents);
    } else if (sender == icone_) {
      espaco->icone = base::UTF16ToUTF8(new_contents);
    } else {
      return;
    }
    Gravar(*espaco);
  }

 private:
  // Mudou por fora (outra janela, a faixa): redesenha, menos quando a mudança
  // é daqui (perderia o foco do campo e o arrasto do seletor de cor).
  void QuandoMudar() {
    if (gravando_) {
      return;
    }
    if (!aberto_.empty() && espacos::Get(perfil_, aberto_)) {
      DetalheDepois(aberto_);
    } else {
      ListaDepois();
    }
  }

  void Gravar(const espacos::Espaco& espaco) {
    gravando_ = true;
    espacos::Atualizar(perfil_, espaco);
    gravando_ = false;
  }

  // Botões da página atual ainda estão na pilha quando pedem a troca.
  void ListaDepois() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PaginaDeEspacos::Lista, weak_factory_.GetWeakPtr()));
  }

  void DetalheDepois(std::string id) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PaginaDeEspacos::Detalhe,
                                  weak_factory_.GetWeakPtr(), std::move(id)));
  }

  void Trocar(std::unique_ptr<views::View> conteudo) {
    nome_ = nullptr;
    icone_ = nullptr;
    RemoveAllChildViews();
    AddChildView(Rolando(std::move(conteudo)));
    InvalidateLayout();
    SchedulePaint();
  }

  void Lista() {
    aberto_.clear();
    auto pagina = std::make_unique<Pagina>(
        u"Espaços",
        u"Cada espaço tem as próprias abas, fixados e favoritos, e um "
        u"container (cookies e logins separados). Troque com Ctrl+Alt+setas, "
        u"pelas bolinhas no pé da lateral ou com os botões laterais do mouse "
        u"em cima dela.");
    Cartao* lista = pagina->Secao(std::u16string());
    const std::vector<containers::Container> todos_containers =
        containers::GetAll(perfil_);
    for (const espacos::Espaco& espaco : espacos::Todos(perfil_)) {
      std::u16string container = u"Sem container";
      for (const containers::Container& c : todos_containers) {
        if (c.id == espaco.container) {
          container = base::UTF8ToUTF16(c.name);
        }
      }
      lista->Adicionar(std::make_unique<LinhaDoEspaco>(
          espaco, container,
          base::BindRepeating(&PaginaDeEspacos::DetalheDepois,
                              base::Unretained(this), espaco.id)));
    }
    auto* novo = pagina->AddChildView(std::make_unique<views::View>());
    novo->SetLayoutManager(std::make_unique<views::BoxLayout>(
                               views::BoxLayout::Orientation::kHorizontal))
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
    novo->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 0, 0));
    novo->AddChildView(Botao(
        u"Novo espaço…",
        base::BindRepeating(&PaginaDeEspacos::Novo, base::Unretained(this))));
    Trocar(std::move(pagina));
  }

  void Detalhe(std::string id) {
    std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, id);
    if (!espaco) {
      Lista();
      return;
    }
    aberto_ = id;
    containers_ = containers::GetAll(perfil_);
    auto pagina = std::make_unique<Pagina>(u"Espaços");
    // Voltar pra lista, como o detalhe do perfil no Dia.
    auto* voltar = pagina->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&PaginaDeEspacos::ListaDepois,
                            base::Unretained(this)),
        u"Todos os espaços"));
    voltar->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kArrowBackIcon,
                                       ui::kColorIconSecondary, 16));
    voltar->SetEnabledTextColors(ui::kColorLabelForegroundSecondary);
    voltar->SetProperty(views::kCrossAxisAlignmentKey,
                        views::LayoutAlignment::kStart);

    Cartao* dados = pagina->Secao(base::UTF8ToUTF16(espaco->nome));
    auto nome = std::make_unique<views::Textfield>();
    nome->SetText(base::UTF8ToUTF16(espaco->nome));
    nome->SetDefaultWidthInChars(18);
    nome->SetAccessibleName(u"Nome do espaço");
    nome->set_controller(this);
    nome_ = nome.get();
    dados->Adicionar(Linha(u"Nome", std::u16string(), std::move(nome)));

    auto icone = std::make_unique<views::Textfield>();
    icone->SetText(base::UTF8ToUTF16(espaco->icone));
    icone->SetPlaceholderText(u"＋");
    icone->SetDefaultWidthInChars(3);
    icone->SetAccessibleName(u"Emoji do espaço");
    icone->set_controller(this);
    icone_ = icone.get();
    dados->Adicionar(Linha(u"Emoji",
                           u"Aparece no lugar da bolinha. Vazio, fica a "
                           u"bolinha.",
                           std::move(icone)));

    auto container =
        std::make_unique<views::Combobox>(ModeloDeContainers(containers_));
    container->SetSelectedIndex(
        IndiceDoContainer(containers_, espaco->container));
    container->SetAccessibleName(u"Container do espaço");
    container->SetSizeToLargestLabel(true);
    views::Combobox* ponteiro = container.get();
    container->SetCallback(base::BindRepeating(
        &PaginaDeEspacos::TrocarContainer, base::Unretained(this),
        base::Unretained(ponteiro)));
    dados->Adicionar(Linha(u"Container",
                           u"As abas novas do espaço abrem nele.",
                           std::move(container)));

    Cartao* cor = pagina->Secao(u"Cor");
    auto seletor = seletor_de_cor::Criar(
        espaco->cor, base::BindRepeating(&PaginaDeEspacos::TrocarCor,
                                         base::Unretained(this)));
    seletor->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 14)));
    cor->Adicionar(std::move(seletor));

    const std::vector<espacos::Espaco> todos = espacos::Todos(perfil_);
    size_t posicao = 0;
    while (posicao < todos.size() && todos[posicao].id != id) {
      ++posicao;
    }
    Cartao* ordem = pagina->Secao(u"Ordem");
    auto botoes = std::make_unique<views::View>();
    botoes->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
    botoes
        ->AddChildView(Botao(
            u"Subir", base::BindRepeating(&PaginaDeEspacos::Mover,
                                          base::Unretained(this), id, -1)))
        ->SetEnabled(posicao > 0);
    botoes
        ->AddChildView(Botao(
            u"Descer", base::BindRepeating(&PaginaDeEspacos::Mover,
                                           base::Unretained(this), id, 1)))
        ->SetEnabled(posicao + 1 < todos.size());
    ordem->Adicionar(Linha(u"Posição na lateral",
                           u"A ordem das bolinhas e dos atalhos Ctrl+Alt+1…5.",
                           std::move(botoes)));

    auto* perigo = pagina->AddChildView(std::make_unique<views::View>());
    perigo->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    perigo->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(16, 0, 0, 0));
    auto excluir = std::make_unique<views::MdTextButton>(
        base::BindRepeating(&PaginaDeEspacos::Excluir, base::Unretained(this),
                            id),
        u"Excluir espaço…");
    excluir->SetEnabledTextColors(ui::kColorSysError);
    // Um vermelho leve, sem o contorno (que era azul).
    excluir->SetBgColorIdOverride(ui::kColorSysErrorContainer);
    excluir->SetStrokeColorIdOverride(ui::kColorSysErrorContainer);
    excluir->SetEnabled(todos.size() > 1);
    perigo->AddChildView(std::move(excluir));
    Trocar(std::move(pagina));
  }

  void Novo() {
    EditarEspaco(browser_, std::string(),
                 base::BindOnce(
                     [](BrowserWindowInterface* browser,
                        const std::string& espaco) {
                       espacos::IrPara(browser, espaco);
                     },
                     browser_.get()));
  }

  void TrocarCor(const std::string& cor) {
    std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, aberto_);
    if (!espaco || espaco->cor == cor) {
      return;
    }
    espaco->cor = cor;
    Gravar(*espaco);
  }

  void TrocarContainer(views::Combobox* combobox) {
    std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, aberto_);
    const std::optional<size_t> escolhido = combobox->GetSelectedIndex();
    if (!espaco || !escolhido) {
      return;
    }
    espaco->container =
        *escolhido == 0 ? std::string() : containers_[*escolhido - 1].id;
    Gravar(*espaco);
  }

  void Mover(std::string id, int sentido) {
    const std::vector<espacos::Espaco> todos = espacos::Todos(perfil_);
    for (size_t i = 0; i < todos.size(); ++i) {
      if (todos[i].id == id) {
        if (sentido < 0 && i == 0) {
          return;
        }
        espacos::Mover(perfil_, id, i + sentido);
        return;
      }
    }
  }

  // Como no Zen, pergunta antes: as abas e os fixados do espaço vão junto.
  void Excluir(std::string id) {
    std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, id);
    if (!espaco) {
      return;
    }
    auto dialogo = std::make_unique<views::DialogDelegate>();
    dialogo->SetTitle(u"Excluir o espaço “" +
                      base::UTF8ToUTF16(espaco->nome) + u"”?");
    dialogo->SetModalType(ui::mojom::ModalType::kWindow);
    dialogo->SetShowCloseButton(true);
    dialogo->SetButtonLabel(ui::mojom::DialogButton::kOk, u"Excluir");
    dialogo->SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancelar");
    dialogo->set_fixed_width(380);
    auto aviso = std::make_unique<views::Label>(
        u"As abas dele fecham em todas as janelas, e os favoritos, sites "
        u"fixados e pastas dele são apagados. O container continua.");
    aviso->SetMultiLine(true);
    aviso->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    aviso->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(4, 20, 8, 20)));
    dialogo->SetContentsView(std::move(aviso));
    dialogo->SetAcceptCallback(base::BindOnce(
        [](Profile* perfil, std::string id) { espacos::Excluir(perfil, id); },
        perfil_.get(), id));
    MostrarSobre(constrained_window::CreateBrowserModalDialogViews(
                     std::move(dialogo), JanelaDe(browser_)),
                 browser_);
  }

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<Profile> perfil_;
  PrefChangeRegistrar prefs_;
  std::string aberto_;
  std::vector<containers::Container> containers_;
  raw_ptr<views::Textfield> nome_ = nullptr;
  raw_ptr<views::Textfield> icone_ = nullptr;
  bool gravando_ = false;
  base::WeakPtrFactory<PaginaDeEspacos> weak_factory_{this};
};

BEGIN_METADATA(PaginaDeEspacos)
END_METADATA

// --- Containers --------------------------------------------------------------

std::unique_ptr<views::View> PaginaContainers(BrowserWindowInterface* browser) {
  // O painel dos containers já traz a explicação no alto.
  auto pagina = std::make_unique<Pagina>(u"Containers");
  Cartao* cartao = pagina->Secao(std::u16string());
  cartao->Adicionar(containers::CriarPainel(browser->GetProfile()->GetOriginalProfile()));
  pagina->Rodape(
      u"Histórico, favoritos e senhas continuam do perfil; o container separa "
      u"cookies, logins e dados dos sites.");
  return pagina;
}

// --- Atalhos -----------------------------------------------------------------

// Botão com o atalho da ação; clicar e apertar as teclas troca o atalho.
class BotaoDeAtalho : public views::LabelButton {
  METADATA_HEADER(BotaoDeAtalho, views::LabelButton)

 public:
  BotaoDeAtalho(Profile* perfil, std::string acao, base::RepeatingClosure mudou)
      : views::LabelButton(base::BindRepeating(&BotaoDeAtalho::Capturar,
                                               base::Unretained(this))),
        perfil_(perfil),
        acao_(std::move(acao)),
        mudou_(std::move(mudou)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetMinSize(gfx::Size(124, 28));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    // Cara de tecla: um chip arredondado.
    SetBackground(views::CreateRoundedRectBackground(
        ui::kColorSysSurfaceVariant, 6));
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColor(ui::kColorSysOnSurface);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 6);
    Mostrar();
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if (!capturando_) {
      return views::LabelButton::OnKeyPressed(event);
    }
    const ui::KeyboardCode tecla = event.key_code();
    if (tecla == ui::VKEY_CONTROL || tecla == ui::VKEY_MENU ||
        tecla == ui::VKEY_SHIFT || tecla == ui::VKEY_LWIN ||
        tecla == ui::VKEY_RWIN || tecla == ui::VKEY_COMMAND) {
      return true;  // espera a tecla de verdade
    }
    const int modificadores =
        event.flags() & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                         ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
    capturando_ = false;
    if (tecla == ui::VKEY_ESCAPE && !modificadores) {
      Mostrar();
      return true;
    }
    if ((tecla == ui::VKEY_BACK || tecla == ui::VKEY_DELETE) &&
        !modificadores) {
      espacos::DefinirAtalho(perfil_, acao_, "");
      mudou_.Run();
      return true;
    }
    const bool tecla_de_funcao = tecla >= ui::VKEY_F1 && tecla <= ui::VKEY_F12;
    const std::string texto =
        atalhos::Escrever(ui::Accelerator(tecla, modificadores));
    // Tecla sozinha (ou só com Shift, que é letra maiúscula) atrapalharia a
    // digitação nas páginas: o atalho vale antes delas.
    const bool com_comando =
        modificadores & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                         ui::EF_COMMAND_DOWN);
    if (texto.empty() || (!com_comando && !tecla_de_funcao)) {
      Mostrar();
      return true;
    }
    // O mesmo atalho em outra ação: sai de lá.
    for (const espacos::Atalho& outro : espacos::Atalhos()) {
      if (outro.acao != acao_ &&
          espacos::AtalhoDe(perfil_, outro.acao) == texto) {
        espacos::DefinirAtalho(perfil_, outro.acao, "");
      }
    }
    espacos::DefinirAtalho(perfil_, acao_, texto);
    mudou_.Run();
    return true;
  }

  void OnBlur() override {
    views::LabelButton::OnBlur();
    if (capturando_) {
      capturando_ = false;
      Mostrar();
    }
  }

 private:
  void Capturar() {
    capturando_ = true;
    SetText(u"Aperte as teclas…");
    RequestFocus();
  }

  void Mostrar() {
    SetText(atalhos::Mostrar(espacos::AtalhoDe(perfil_, acao_)));
  }

  raw_ptr<Profile> perfil_;
  std::string acao_;
  base::RepeatingClosure mudou_;
  bool capturando_ = false;
};

BEGIN_METADATA(BotaoDeAtalho)
END_METADATA

// Atalho fixo (do Chromium), com a mesma cara de tecla do BotaoDeAtalho, mas
// sem trocar.
std::unique_ptr<views::View> Tecla(std::u16string texto) {
  auto tecla = std::make_unique<views::Label>(std::move(texto));
  tecla->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  tecla->SetEnabledColor(ui::kColorLabelForegroundSecondary);
  tecla->SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysSurfaceVariant, 6));
  tecla->SetPreferredSize(gfx::Size(124, 28));
  return tecla;
}

class PaginaDeAtalhos : public views::View {
  METADATA_HEADER(PaginaDeAtalhos, views::View)

 public:
  explicit PaginaDeAtalhos(Profile* perfil) : perfil_(perfil) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    Reconstruir();
  }

 private:
  void Reconstruir() {
    RemoveAllChildViews();
    auto pagina = std::make_unique<Pagina>(
        u"Atalhos do teclado",
        u"Clique no atalho e aperte as teclas novas. Esc cancela; Backspace "
        u"tira o atalho. Valem em todas as janelas, e antes das páginas.");
    Cartao* cartao = pagina->Secao(std::u16string());
    for (const espacos::Atalho& atalho : espacos::Atalhos()) {
      cartao->Adicionar(
          Linha(atalho.descricao, std::u16string(),
                std::make_unique<BotaoDeAtalho>(
                    perfil_, atalho.acao,
                    base::BindRepeating(&PaginaDeAtalhos::ReconstruirDepois,
                                        weak_factory_.GetWeakPtr()))));
    }
    Cartao* chromium = pagina->Secao(u"Do Chromium");
    chromium->Adicionar(Linha(
        u"Ctrl+Tab na ordem de uso",
        u"Ctrl+Tab volta pra aba usada por último, e de novo pra anterior; "
        u"desligado, vai pra aba de baixo na lateral.",
        Interruptor(perfil_->GetPrefs()->GetBoolean(prefs::kCtrlTabMru),
                    u"Ctrl+Tab na ordem de uso",
                    base::BindRepeating(
                        [](Profile* perfil, bool ligado) {
                          perfil->GetPrefs()->SetBoolean(prefs::kCtrlTabMru,
                                                         ligado);
                        },
                        perfil_.get()))));
    chromium->Adicionar(Linha(u"Modo de leitura", u"Só o texto da página, "
                              u"num painel ao lado.",
                              Tecla(u"Shift+Alt+R")));
    auto* rodape = pagina->AddChildView(std::make_unique<views::View>());
    rodape
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal))
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
    rodape->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 0, 0));
    rodape->AddChildView(Botao(
        u"Restaurar os padrões",
        base::BindRepeating(&PaginaDeAtalhos::Restaurar,
                            base::Unretained(this))));
    AddChildView(Rolando(std::move(pagina)));
    InvalidateLayout();
  }

  void Restaurar() {
    espacos::RestaurarAtalhos(perfil_);
    ReconstruirDepois();
  }

  // O botão que avisou ainda está na pilha: reconstrói no próximo giro.
  void ReconstruirDepois() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PaginaDeAtalhos::Reconstruir,
                                  weak_factory_.GetWeakPtr()));
  }

  raw_ptr<Profile> perfil_;
  base::WeakPtrFactory<PaginaDeAtalhos> weak_factory_{this};
};

BEGIN_METADATA(PaginaDeAtalhos)
END_METADATA

// O conteúdo do diálogo ocupando tudo e um "X" flutuando no canto de cima,
// por cima da página (no lugar do X do quadro, que reservava uma faixa).
class ComBotaoDeFechar : public views::View {
  METADATA_HEADER(ComBotaoDeFechar, views::View)

 public:
  ComBotaoDeFechar() {
    fechar_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(
            [](views::View* view) {
              if (view->GetWidget()) {
                view->GetWidget()->Close();
              }
            },
            base::Unretained(this)),
        vector_icons::kCloseIcon, 18));
    fechar_->SetTooltipText(u"Fechar");
    fechar_->GetViewAccessibility().SetName(u"Fechar");
    views::InstallCircleHighlightPathGenerator(fechar_);
  }

  // views::View:
  void Layout(PassKey) override {
    for (views::View* filho : children()) {
      if (filho != fechar_) {
        filho->SetBoundsRect(GetLocalBounds());
      }
    }
    const gfx::Size tamanho = fechar_->GetPreferredSize();
    fechar_->SetBounds(width() - tamanho.width() - 14, 14, tamanho.width(),
                       tamanho.height());
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& limites) const override {
    for (views::View* filho : children()) {
      if (filho != fechar_) {
        return filho->GetPreferredSize(limites);
      }
    }
    return gfx::Size();
  }

 private:
  raw_ptr<views::ImageButton> fechar_;
};

BEGIN_METADATA(ComBotaoDeFechar)
END_METADATA

// --- Janela ------------------------------------------------------------------

// Item do menu da esquerda: ícone e nome; o escolhido tem fundo e a pílula
// de destaque à esquerda (NavigationView do Dia).
class ItemDoMenu : public views::Button {
  METADATA_HEADER(ItemDoMenu, views::Button)

 public:
  ItemDoMenu(const gfx::VectorIcon& icone,
             std::u16string nome,
             PressedCallback callback)
      : views::Button(std::move(callback)) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 14),
        12));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    AddChildView(std::make_unique<views::ImageView>(
                     ui::ImageModel::FromVectorIcon(
                         icone, ui::kColorIconSecondary, 16)))
        ->SetCanProcessEventsWithinSubtree(false);
    AddChildView(Titulo(nome, 14, gfx::Font::Weight::NORMAL))
        ->SetCanProcessEventsWithinSubtree(false);
    SetPreferredSize(gfx::Size(kLarguraMenu - 16, kAlturaItemMenu));
    GetViewAccessibility().SetName(nome);
  }

  void SetEscolhido(bool escolhido) {
    if (escolhido_ != escolhido) {
      escolhido_ = escolhido;
      SchedulePaint();
    }
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const bool escuro = Escuro(this);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (escolhido_ || GetState() == STATE_HOVERED ||
        GetState() == STATE_PRESSED) {
      const SkAlpha alfa =
          escolhido_ ? (escuro ? 0x16 : 0x0D) : (escuro ? 0x0C : 0x08);
      flags.setColor(escuro ? SkColorSetA(SK_ColorWHITE, alfa)
                            : SkColorSetA(SK_ColorBLACK, alfa));
      canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), 6, flags);
    }
    if (escolhido_) {
      flags.setColor(GetColorProvider()->GetColor(ui::kColorAccent));
      canvas->DrawRoundRect(gfx::RectF(0, (height() - 16) / 2.0f, 3, 16),
                            1.5f, flags);
    }
  }

 private:
  bool escolhido_ = false;
};

BEGIN_METADATA(ItemDoMenu)
END_METADATA

class JanelaDeConfiguracoes : public views::View {
  METADATA_HEADER(JanelaDeConfiguracoes, views::View)

 public:
  explicit JanelaDeConfiguracoes(BrowserWindowInterface* browser)
      : browser_(browser) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    auto* menu = AddChildView(std::make_unique<views::View>());
    menu->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::TLBR(18, 8, 12, 8), 2));
    menu->SetPreferredSize(gfx::Size(kLarguraMenu, 0));
    menu->AddChildView(
            Titulo(u"Configurações", 15, gfx::Font::Weight::SEMIBOLD))
        ->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, 14, 12, 0)));
    struct Entrada {
      raw_ref<const gfx::VectorIcon> icone;
      const char16_t* nome;
    };
    const Entrada entradas[] = {
        {raw_ref(vector_icons::kSettingsIcon), u"Geral"},
        {raw_ref(icones_menu::kShieldCheckIcon), u"Privacidade"},
        {raw_ref(kBrushFilledIcon), u"Aparência"},
        {raw_ref(kViewListIcon), u"Lateral"},
        {raw_ref(kGridViewIcon), u"Espaços"},
        {raw_ref(vector_icons::kWorkIcon), u"Containers"},
        {raw_ref(vector_icons::kKeyboardIcon), u"Atalhos"},
    };
    for (size_t i = 0; i < std::size(entradas); ++i) {
      itens_.push_back(menu->AddChildView(std::make_unique<ItemDoMenu>(
          *entradas[i].icone, entradas[i].nome,
          base::BindRepeating(&JanelaDeConfiguracoes::SelecionarDepois,
                              base::Unretained(this), static_cast<Aba>(i)))));
    }

    auto* divisa = AddChildView(std::make_unique<views::Separator>());
    divisa->SetOrientation(views::Separator::Orientation::kVertical);
    divisa->SetColorId(ui::kColorSeparator);

    conteudo_ = AddChildView(std::make_unique<views::View>());
    conteudo_->SetLayoutManager(std::make_unique<views::FillLayout>());
    layout->SetFlexForView(conteudo_, 1);
    SetPreferredSize(gfx::Size(kLarguraJanela, kAlturaJanela));
  }

  void Selecionar(Aba aba) {
    const size_t indice = static_cast<size_t>(aba);
    for (size_t i = 0; i < itens_.size(); ++i) {
      itens_[i]->SetEscolhido(i == indice);
    }
    conteudo_->RemoveAllChildViews();
    switch (aba) {
      case Aba::kGeral:
        conteudo_->AddChildView(Rolando(PaginaGeral(browser_)));
        break;
      case Aba::kPrivacidade:
        conteudo_->AddChildView(Rolando(PaginaPrivacidade(browser_)));
        break;
      case Aba::kAparencia:
        conteudo_->AddChildView(Rolando(PaginaAparencia(browser_)));
        break;
      case Aba::kLateral:
        conteudo_->AddChildView(Rolando(PaginaLateral(browser_)));
        break;
      case Aba::kEspacos:
        conteudo_->AddChildView(std::make_unique<PaginaDeEspacos>(browser_));
        break;
      case Aba::kContainers:
        conteudo_->AddChildView(Rolando(PaginaContainers(browser_)));
        break;
      case Aba::kAtalhos:
        conteudo_->AddChildView(
            std::make_unique<PaginaDeAtalhos>(browser_->GetProfile()->GetOriginalProfile()));
        break;
    }
    conteudo_->InvalidateLayout();
    SchedulePaint();
  }

  // Refaz a página Geral (depois de importar) com `frase` na linha de
  // exportar, à vista. Depois da pilha de quem pediu: a linha que pediu é
  // apagada junto com a página.
  void RefazerGeral(std::u16string frase) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&JanelaDeConfiguracoes::RefazerGeralAgora,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(frase)));
  }

 private:
  void RefazerGeralAgora(std::u16string frase) {
    Selecionar(Aba::kGeral);
    if (LinhaDeExportar* linha = AcharLinhaDeExportar(conteudo_)) {
      linha->Escrever(frase);
    }
    // Rola até ela depois de o layout assentar: as descrições acertam a
    // altura pela largura de verdade (TextoDeDescricao) em mais de uma
    // passada, e rolando na hora a frase ficava cortada embaixo.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&JanelaDeConfiguracoes::MostrarLinhaDeExportar,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(100));
  }

  void MostrarLinhaDeExportar() {
    if (LinhaDeExportar* linha = AcharLinhaDeExportar(conteudo_)) {
      linha->ScrollViewToVisible();
    }
  }

  static LinhaDeExportar* AcharLinhaDeExportar(views::View* raiz) {
    if (auto* linha = views::AsViewClass<LinhaDeExportar>(raiz)) {
      return linha;
    }
    for (views::View* filho : raiz->children()) {
      if (LinhaDeExportar* linha = AcharLinhaDeExportar(filho)) {
        return linha;
      }
    }
    return nullptr;
  }

  // O item do menu que pediu ainda está na pilha.
  void SelecionarDepois(Aba aba) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&JanelaDeConfiguracoes::Selecionar,
                                  weak_factory_.GetWeakPtr(), aba));
  }

  raw_ptr<BrowserWindowInterface> browser_;
  std::vector<raw_ptr<ItemDoMenu>> itens_;
  raw_ptr<views::View> conteudo_ = nullptr;
  base::WeakPtrFactory<JanelaDeConfiguracoes> weak_factory_{this};
};

BEGIN_METADATA(JanelaDeConfiguracoes)
END_METADATA

void RefazerDepoisDeImportar(views::View* de_dentro, std::u16string frase) {
  for (views::View* v = de_dentro; v; v = v->parent()) {
    if (auto* janela = views::AsViewClass<JanelaDeConfiguracoes>(v)) {
      janela->RefazerGeral(std::move(frase));
      return;
    }
  }
}

class EditorDeEspaco : public views::DialogDelegate {
 public:
  EditorDeEspaco(Profile* perfil,
                 std::string espaco,
                 base::OnceCallback<void(const std::string&)> pronto)
      : perfil_(perfil),
        id_(std::move(espaco)),
        pronto_(std::move(pronto)) {
    std::optional<espacos::Espaco> atual =
        id_.empty() ? std::nullopt : espacos::Get(perfil_, id_);
    cor_ = atual ? atual->cor : espacos::ProximaCor(perfil_);
    SetTitle(atual ? u"Editar espaço" : u"Novo espaço");
    SetModalType(ui::mojom::ModalType::kWindow);
    SetShowCloseButton(true);
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   atual ? u"Salvar" : u"Criar");
    SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancelar");
    SetAcceptCallback(base::BindOnce(&EditorDeEspaco::Confirmar,
                                     base::Unretained(this)));
    set_fixed_width(420);

    auto conteudo = std::make_unique<views::View>();
    conteudo->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::TLBR(4, 20, 8, 20), 12));

    auto* linha_nome = conteudo->AddChildView(std::make_unique<views::View>());
    auto* layout_nome =
        linha_nome->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
    layout_nome->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    icone_ = linha_nome->AddChildView(std::make_unique<views::Textfield>());
    icone_->SetPlaceholderText(u"＋");
    icone_->SetDefaultWidthInChars(2);
    icone_->SetAccessibleName(u"Emoji do espaço");
    nome_ = linha_nome->AddChildView(std::make_unique<views::Textfield>());
    nome_->SetPlaceholderText(u"Nome do espaço (ex.: Trabalho)");
    nome_->SetDefaultWidthInChars(24);
    nome_->SetAccessibleName(u"Nome do espaço");
    layout_nome->SetFlexForView(nome_, 1);

    conteudo->AddChildView(seletor_de_cor::Criar(
        cor_, base::BindRepeating(
                  [](EditorDeEspaco* editor, const std::string& cor) {
                    editor->cor_ = cor;
                  },
                  base::Unretained(this))));

    containers_ = containers::GetAll(perfil_);
    auto* linha_container =
        conteudo->AddChildView(std::make_unique<views::View>());
    auto* layout_container =
        linha_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
    layout_container->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    linha_container->AddChildView(
        std::make_unique<views::Label>(u"Container"));
    container_ = linha_container->AddChildView(
        std::make_unique<views::Combobox>(ModeloDeContainers(containers_)));
    container_->SetAccessibleName(u"Container do espaço");
    conteudo->AddChildView(Descricao(
        u"As abas novas do espaço abrem no container: os sites ficam logados "
        u"só nele.",
        380));

    if (atual) {
      icone_->SetText(base::UTF8ToUTF16(atual->icone));
      nome_->SetText(base::UTF8ToUTF16(atual->nome));
      container_->SetSelectedIndex(
          IndiceDoContainer(containers_, atual->container));
    } else {
      container_->SetSelectedIndex(0);
    }
    SetInitiallyFocusedView(nome_);
    SetContentsView(std::move(conteudo));
  }

 private:
  void Confirmar() {
    std::string nome = base::UTF16ToUTF8(nome_->GetText());
    if (nome.empty()) {
      nome = "Espaço";
    }
    const std::optional<size_t> escolhido = container_->GetSelectedIndex();
    const std::string container = escolhido && *escolhido > 0
                                      ? containers_[*escolhido - 1].id
                                      : std::string();
    const std::string icone = base::UTF16ToUTF8(icone_->GetText());
    std::string id = id_;
    if (std::optional<espacos::Espaco> atual =
            id.empty() ? std::nullopt : espacos::Get(perfil_, id)) {
      atual->nome = nome;
      atual->icone = icone;
      atual->cor = cor_;
      atual->container = container;
      espacos::Atualizar(perfil_, *atual);
    } else {
      id = espacos::Criar(perfil_, nome, icone, cor_, container).id;
    }
    if (pronto_) {
      std::move(pronto_).Run(id);
    }
  }

  raw_ptr<Profile> perfil_;
  std::string id_;
  base::OnceCallback<void(const std::string&)> pronto_;
  std::string cor_;
  std::vector<containers::Container> containers_;
  raw_ptr<views::Textfield> icone_ = nullptr;
  raw_ptr<views::Textfield> nome_ = nullptr;
  raw_ptr<views::Combobox> container_ = nullptr;
};

}  // namespace

void Mostrar(BrowserWindowInterface* browser, Aba aba) {
  auto& abertas = Abertas();
  if (auto it = abertas.find(browser); it != abertas.end() && it->second) {
    if (auto* janela = views::AsViewClass<JanelaDeConfiguracoes>(
            it->second->GetContentsView()->children().front())) {
      janela->Selecionar(aba);
    }
    it->second->Activate();
    return;
  }

  auto dialogo = std::make_unique<views::DialogDelegate>();
  dialogo->SetTitle(u"Configurações do Norte");
  dialogo->SetShowTitle(false);
  dialogo->SetModalType(ui::mojom::ModalType::kWindow);
  // O "X" do quadro do diálogo, sem título, ocupava uma faixa vazia inteira
  // no alto; o nosso flutua no canto (ComBotaoDeFechar).
  dialogo->SetShowCloseButton(false);
  dialogo->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  dialogo->set_margins(gfx::Insets());
  dialogo->set_fixed_width(kLarguraJanela);

  auto conteudo = std::make_unique<ComBotaoDeFechar>();
  conteudo
      ->AddChildViewAt(std::make_unique<JanelaDeConfiguracoes>(browser), 0)
      ->Selecionar(aba);
  dialogo->SetContentsView(std::move(conteudo));

  // Como os diálogos do Chromium: centralizado no alto da janela.
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::move(dialogo), JanelaDe(browser));
  abertas[browser] = widget->GetWeakPtr();
  MostrarSobre(widget, browser);
}

void EditarEspaco(BrowserWindowInterface* browser,
                  const std::string& espaco,
                  base::OnceCallback<void(const std::string&)> pronto) {
  views::Widget* widget = constrained_window::CreateBrowserModalDialogViews(
      std::make_unique<EditorDeEspaco>(browser->GetProfile()->GetOriginalProfile(), espaco,
                                       std::move(pronto)),
      JanelaDe(browser));
  MostrarSobre(widget, browser);
}

}  // namespace navegador::configuracoes
