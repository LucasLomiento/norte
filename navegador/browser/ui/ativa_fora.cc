#include "navegador/browser/ui/ativa_fora.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"

namespace navegador::ativa_fora {

namespace {

constexpr int kAltura = 30;
constexpr int kMargem = 4;
constexpr int kIcone = 16;

TabView* AchaAba(views::View* raiz, const tabs::TabInterface* aba) {
  if (auto* tab_view = views::AsViewClass<TabView>(raiz);
      tab_view && tab_view->GetTabInterface() == aba) {
    return tab_view;
  }
  for (views::View* filho : raiz->children()) {
    if (TabView* achou = AchaAba(filho, aba)) {
      return achou;
    }
  }
  return nullptr;
}

class Aviso : public views::Button, public TabStripModelObserver {
  METADATA_HEADER(Aviso, views::Button)

 public:
  Aviso(views::ScrollView* lista, BrowserWindowInterface* browser)
      : views::Button(base::BindRepeating(&Aviso::IrPraAtiva,
                                          base::Unretained(this))),
        lista_(lista),
        model_(browser->GetTabStripModel()),
        vigia_da_lista_(this) {
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    SetVisible(false);
    model_->AddObserver(this);
    lista_observada_.Observe(lista_);
    rolagem_ = lista_->AddContentsScrolledCallback(
        base::BindRepeating(&Aviso::Atualizar, base::Unretained(this)));
  }

  ~Aviso() override { model_->RemoveObserver(this); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    Atualizar();
  }
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override {
    if (GetVisible() && tab == model_->GetActiveTab()) {
      SchedulePaint();
    }
  }

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    tabs::TabInterface* ativa = model_->GetActiveTab();
    TabUIHelper* ajudante = ativa ? TabUIHelper::From(ativa) : nullptr;
    if (!ajudante) {
      return;
    }
    const ui::ColorProvider* cores = GetColorProvider();
    const SkColor texto = cores->GetColor(kColorTabForegroundActiveFrameActive);
    cc::PaintFlags fundo;
    fundo.setAntiAlias(true);
    fundo.setColor(cores->GetColor(kColorTabBackgroundActiveFrameActive));
    canvas->DrawRoundRect(GetLocalBounds(), 8, fundo);

    const int meio = (height() - kIcone) / 2;
    gfx::ImageSkia favicon = ajudante->GetFavicon().Rasterize(cores);
    if (!favicon.isNull()) {
      canvas->DrawImageInt(favicon, 0, 0, favicon.width(), favicon.height(),
                           8, meio, kIcone, kIcone, /*filter=*/true);
    }
    // A seta pro lado em que a aba ficou.
    canvas->DrawImageInt(
        gfx::CreateVectorIcon(em_cima_ ? vector_icons::kKeyboardArrowUpIcon
                                       : vector_icons::kKeyboardArrowDownIcon,
                              kIcone, SkColorSetA(texto, 0x99)),
        width() - kIcone - 8, meio);
    const int x = 8 + kIcone + 8;
    canvas->DrawStringRectWithFlags(
        ajudante->GetTitle(), gfx::FontList(), texto,
        gfx::Rect(x, 0, width() - x - kIcone - 16, height()),
        gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

 private:
  void Atualizar() {
    tabs::TabInterface* ativa = model_->GetActiveTab();
    views::View* conteudo = lista_ ? lista_->contents() : nullptr;
    TabView* aba = ativa && conteudo ? AchaAba(conteudo, ativa) : nullptr;
    // Segue a própria aba: na animação de abrir e fechar abas ela anda sem
    // a lista rolar nem mudar de tamanho, e sem isso o aviso ficava preso
    // por cima da primeira aba (com uma aba só, a "Nova aba" saía dobrada).
    if (aba_observada_.GetSource() != aba) {
      aba_observada_.Reset();
      if (aba) {
        aba_observada_.Observe(aba);
      }
    }
    if (!aba || !aba->GetVisible() || !lista_->GetVisible()) {
      SetVisible(false);
      return;
    }
    const gfx::Rect na_lista =
        views::View::ConvertRectToTarget(aba, conteudo, aba->GetLocalBounds());
    const gfx::Rect visivel = lista_->GetVisibleRect();
    // Aba ainda sem tamanho (entrando) ou lista de uma linha só: nada.
    if (na_lista.IsEmpty() || visivel.height() < 2 * kAltura) {
      SetVisible(false);
      return;
    }
    if (na_lista.bottom() <= visivel.y()) {
      em_cima_ = true;
    } else if (na_lista.y() >= visivel.bottom()) {
      em_cima_ = false;
    } else {
      SetVisible(false);
      return;
    }
    const gfx::Rect area = lista_->bounds();
    SetBounds(area.x() + kMargem,
              em_cima_ ? area.y() + kMargem
                       : area.bottom() - kAltura - kMargem,
              area.width() - 2 * kMargem, kAltura);
    GetViewAccessibility().SetName(u"Aba ativa: " +
                                   TabUIHelper::From(ativa)->GetTitle());
    SetVisible(true);
    SchedulePaint();
  }

  void IrPraAtiva() {
    tabs::TabInterface* ativa = model_->GetActiveTab();
    if (TabView* aba = ativa && lista_ && lista_->contents()
                           ? AchaAba(lista_->contents(), ativa)
                           : nullptr) {
      aba->ScrollViewToVisible();
    }
  }

  // A lista ou a aba ativa mudam de lugar ou somem (o Button já é um
  // ViewObserver de si mesmo).
  class VigiaDaLista : public views::ViewObserver {
   public:
    explicit VigiaDaLista(Aviso* aviso) : aviso_(aviso) {}
    void OnViewBoundsChanged(views::View* vista) override {
      aviso_->Atualizar();
    }
    void OnViewIsDeleting(views::View* vista) override {
      aviso_->Apagada(vista);
    }

   private:
    raw_ptr<Aviso> aviso_;
  };

  void Apagada(views::View* vista) {
    if (vista == aba_observada_.GetSource()) {
      aba_observada_.Reset();
      return;
    }
    aba_observada_.Reset();
    lista_observada_.Reset();
    rolagem_ = {};
    lista_ = nullptr;
  }

  raw_ptr<views::ScrollView> lista_;
  const raw_ptr<TabStripModel> model_;
  bool em_cima_ = true;
  base::CallbackListSubscription rolagem_;
  VigiaDaLista vigia_da_lista_;
  base::ScopedObservation<views::View, views::ViewObserver> lista_observada_{
      &vigia_da_lista_};
  base::ScopedObservation<views::View, views::ViewObserver> aba_observada_{
      &vigia_da_lista_};
};

BEGIN_METADATA(Aviso)
END_METADATA

}  // namespace

void Acompanhar(views::View* dono,
                views::ScrollView* lista,
                BrowserWindowInterface* browser) {
  if (dono && lista && browser) {
    dono->AddChildView(std::make_unique<Aviso>(lista, browser));
  }
}

}  // namespace navegador::ativa_fora
