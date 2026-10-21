#include "navegador/browser/ui/aba_nova.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "navegador/icones/menu/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace navegador::aba_nova {

namespace {

// Depois de a aba entrar: a lista já pôs a linha no lugar (a animação de
// entrar começa com a linha sem altura).
constexpr base::TimeDelta kDepoisDeEntrar = base::Milliseconds(120);
constexpr base::TimeDelta kTempoDaPilula = base::Milliseconds(2200);
constexpr int kAlturaDaPilula = 34;
constexpr int kMargemDaPilula = 12;
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

// Some sozinho no fim da animação (ou na hora, sem animação).
void SumirDepois(base::WeakPtr<views::View> vista) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<views::View> vista) {
                       if (vista && vista->parent()) {
                         vista->parent()->RemoveChildViewT(vista.get());
                       }
                     },
                     vista));
}

// O brilho por cima da linha da aba nova, do tamanho dela.
class Brilho : public views::View, public views::ViewObserver {
  METADATA_HEADER(Brilho, views::View)

 public:
  Brilho() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
  }

  // views::View:
  void AddedToWidget() override {
    if (!parent()) {
      return;
    }
    aba_.Observe(parent());
    SetBoundsRect(parent()->GetLocalBounds());
    if (!gfx::Animation::ShouldRenderRichAnimation()) {
      // Sem animação: aparece um instante e some.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SumirDepois, weak_factory_.GetWeakPtr()),
          base::Milliseconds(600));
      return;
    }
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&SumirDepois, weak_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(&SumirDepois, weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(base::Milliseconds(250))
        .SetOpacity(layer(), 1.0f)
        .Then()
        .SetDuration(base::Milliseconds(700))
        .SetOpacity(layer(), 0.0f, gfx::Tween::EASE_OUT);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(
        SkColorSetA(GetColorProvider()->GetColor(ui::kColorAccent), 0x59));
    canvas->DrawRoundRect(GetLocalBounds(), 8, flags);
  }

  // views::ViewObserver (a linha da aba, que ainda pode crescer):
  void OnViewBoundsChanged(views::View* aba) override {
    SetBoundsRect(aba->GetLocalBounds());
  }
  void OnViewIsDeleting(views::View* aba) override { aba_.Reset(); }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> aba_{this};
  base::WeakPtrFactory<views::View> weak_factory_{this};
};

BEGIN_METADATA(Brilho)
END_METADATA

// A pílula no pé da página, com a lateral escondida.
class Pilula : public views::Button {
  METADATA_HEADER(Pilula, views::Button)

 public:
  Pilula(TabStripModel* model, tabs::TabInterface* aba, std::u16string texto)
      : views::Button(
            base::BindRepeating(&Pilula::IrPraAba, base::Unretained(this))),
        model_(model),
        aba_(aba->GetHandle()),
        texto_(std::move(texto)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    GetViewAccessibility().SetName(texto_);
  }

  void Mostrar() {
    if (!parent()) {
      return;
    }
    // No meio, embaixo: no canto esquerdo fica o endereço do link sob o
    // ponteiro (o Ctrl+clique deixa o ponteiro em cima do link).
    const int largura =
        std::min(12 + kIcone + 8 + gfx::GetStringWidth(texto_, gfx::FontList()) +
                     14,
                 parent()->width() - 2 * kMargemDaPilula);
    SetBounds((parent()->width() - largura) / 2,
              parent()->height() - kAlturaDaPilula - 2 * kMargemDaPilula,
              largura, kAlturaDaPilula);
    tempo_.Start(FROM_HERE, kTempoDaPilula,
                 base::BindOnce(&Pilula::Esconder, base::Unretained(this)));
    if (!gfx::Animation::ShouldRenderRichAnimation()) {
      return;
    }
    layer()->SetOpacity(0.0f);
    views::AnimationBuilder()
        .Once()
        .SetDuration(base::Milliseconds(150))
        .SetOpacity(layer(), 1.0f, gfx::Tween::EASE_OUT);
  }

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // As cores dos avisos do Chromium ("Link copiado"): as das abas seguem a
    // cor do site e sumiam numa página clara.
    const ui::ColorProvider* cores = GetColorProvider();
    const SkColor texto = cores->GetColor(ui::kColorToastForeground);
    cc::PaintFlags fundo;
    fundo.setAntiAlias(true);
    fundo.setColor(cores->GetColor(ui::kColorToastBackground));
    canvas->DrawRoundRect(GetLocalBounds(), height() / 2, fundo);
    const int meio = (height() - kIcone) / 2;
    canvas->DrawImageInt(
        gfx::CreateVectorIcon(icones_menu::kSquareArrowOutUpRightIcon, kIcone,
                              SkColorSetA(texto, 0xB3)),
        12, meio);
    canvas->DrawStringRectWithFlags(
        texto_, gfx::FontList(), texto,
        gfx::Rect(12 + kIcone + 8, 0, width() - (12 + kIcone + 8) - 12,
                  height()),
        gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

 private:
  void IrPraAba() {
    tabs::TabInterface* aba = aba_.Get();
    if (model_ && aba) {
      const int indice = model_->GetIndexOfTab(aba);
      if (indice != TabStripModel::kNoTab) {
        model_->ActivateTabAt(indice);
      }
    }
    Esconder();
  }

  void Esconder() {
    tempo_.Stop();
    if (!gfx::Animation::ShouldRenderRichAnimation()) {
      SumirDepois(weak_factory_.GetWeakPtr());
      return;
    }
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&SumirDepois, weak_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(&SumirDepois, weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(base::Milliseconds(250))
        .SetOpacity(layer(), 0.0f, gfx::Tween::EASE_IN);
  }

  // A janela vive mais que a pílula (filha da área das páginas dela).
  const raw_ptr<TabStripModel> model_;
  const tabs::TabHandle aba_;
  const std::u16string texto_;
  base::OneShotTimer tempo_;
  base::WeakPtrFactory<views::View> weak_factory_{this};
};

BEGIN_METADATA(Pilula)
END_METADATA

// Invisível, na lateral: espera o aviso de link abrindo e a aba entrar.
class Vigia : public views::View, public TabStripModelObserver {
  METADATA_HEADER(Vigia, views::View)

 public:
  explicit Vigia(BrowserView* browser_view)
      : browser_view_(browser_view),
        model_(browser_view->browser()->GetTabStripModel()) {
    SetVisible(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    model_->AddObserver(this);
    inscricao_ = browser_view->AddOnLinkOpeningFromGestureCallback(
        base::BindRepeating(&Vigia::LinkAbrindo, base::Unretained(this)));
  }

  ~Vigia() override { model_->RemoveObserver(this); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (!esperando_ || change.type() != TabStripModelChange::kInserted) {
      return;
    }
    const bool em_segundo_plano =
        *esperando_ == WindowOpenDisposition::NEW_BACKGROUND_TAB;
    esperando_.reset();
    for (const auto& inserida : change.GetInsert()->contents) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&Vigia::Avisar, weak_factory_.GetWeakPtr(),
                         inserida.tab->GetHandle(), em_segundo_plano),
          kDepoisDeEntrar);
    }
  }

 private:
  void LinkAbrindo(WindowOpenDisposition disposicao) {
    if (disposicao != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
        disposicao != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
      return;
    }
    // A aba entra logo depois, na mesma chamada (Navigate); se não entrar
    // (aba já aberta, janela nova), o aviso esquece.
    esperando_ = disposicao;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Vigia::Esquecer, weak_factory_.GetWeakPtr()));
  }

  void Esquecer() { esperando_.reset(); }

  void Avisar(tabs::TabHandle handle, bool em_segundo_plano) {
    tabs::TabInterface* aba = handle.Get();
    if (!aba || model_->GetIndexOfTab(aba) == TabStripModel::kNoTab) {
      return;
    }
    auto* lateral =
        tabs::VerticalTabStripStateController::From(browser_view_->browser());
    const bool lateral_a_vista = lateral && !lateral->IsCollapsed();
    // A linha da aba, se está à vista na lista.
    TabView* linha = lateral_a_vista && parent() ? AchaAba(parent(), aba)
                                                 : nullptr;
    if (linha && linha->IsDrawn() &&
        linha->GetVisibleBounds().height() >= linha->height() / 2) {
      linha->AddChildView(std::make_unique<Brilho>());
      return;
    }
    views::View* area = browser_view_->contents_container();
    if (!area) {
      return;
    }
    std::u16string texto = em_segundo_plano ? u"Aberta numa aba nova"
                                            : u"Você está numa aba nova";
    const GURL endereco = aba->GetContents()->GetVisibleURL();
    if (endereco.SchemeIsHTTPOrHTTPS() && endereco.has_host()) {
      texto += u" · " + base::UTF8ToUTF16(endereco.host());
    }
    auto* pilula =
        area->AddChildView(std::make_unique<Pilula>(model_, aba, texto));
    pilula->Mostrar();
  }

  const raw_ptr<BrowserView> browser_view_;
  const raw_ptr<TabStripModel> model_;
  std::optional<WindowOpenDisposition> esperando_;
  base::CallbackListSubscription inscricao_;
  base::WeakPtrFactory<Vigia> weak_factory_{this};
};

BEGIN_METADATA(Vigia)
END_METADATA

}  // namespace

void Acompanhar(views::View* dono, BrowserView* browser_view) {
  dono->AddChildView(std::make_unique<Vigia>(browser_view));
}

}  // namespace navegador::aba_nova
