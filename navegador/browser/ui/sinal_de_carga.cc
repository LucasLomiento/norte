#include "navegador/browser/ui/sinal_de_carga.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/ui/barra.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

namespace navegador::sinal_de_carga {

namespace {

constexpr int kAltura = 2;
constexpr base::TimeDelta kEspera = base::Milliseconds(300);

// A linha no alto da área das páginas, por cima delas.
class Linha : public views::View {
  METADATA_HEADER(Linha, views::View)

 public:
  Linha() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetCanProcessEventsWithinSubtree(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    SetVisible(false);
  }

  void Mostrar() {
    if (GetVisible()) {
      return;
    }
    SetVisible(true);
    layer()->SetOpacity(1.0f);
    // Pulsa no compositor (sem pintar a interface a cada quadro); com "menos
    // movimento" no sistema, fica parada.
    if (gfx::Animation::ShouldRenderRichAnimation()) {
      views::AnimationBuilder()
          .Repeatedly()
          .SetDuration(base::Milliseconds(700))
          .SetOpacity(layer(), 0.3f, gfx::Tween::EASE_IN_OUT)
          .Then()
          .SetDuration(base::Milliseconds(700))
          .SetOpacity(layer(), 1.0f, gfx::Tween::EASE_IN_OUT);
    }
  }

  void Esconder() {
    if (!GetVisible()) {
      return;
    }
    layer()->GetAnimator()->AbortAllAnimations();
    SetVisible(false);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawColor(GetColorProvider()->GetColor(ui::kColorAccent));
  }
};

BEGIN_METADATA(Linha)
END_METADATA

// Invisível, na lateral: segue a aba ativa e o modo (lateral recolhida e
// barra escondida), e mostra ou esconde a Linha.
class Vigia : public views::View,
              public TabStripModelObserver,
              public content::WebContentsObserver,
              public views::ViewObserver {
  METADATA_HEADER(Vigia, views::View)

 public:
  explicit Vigia(BrowserView* browser_view)
      : browser_view_(browser_view),
        model_(browser_view->browser()->GetTabStripModel()) {
    SetVisible(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    model_->AddObserver(this);
    Observe(model_->GetActiveWebContents());
    preferencias_.Init(browser_view->browser()->GetProfile()->GetPrefs());
    preferencias_.Add(
        espacos::kBarraEscondidaPref,
        base::BindRepeating(&Vigia::Atualizar, base::Unretained(this)));
    if (auto* lateral = tabs::VerticalTabStripStateController::From(
            browser_view->browser())) {
      recolhida_ = lateral->RegisterOnCollapseChanged(base::BindRepeating(
          [](Vigia* vigia, tabs::VerticalTabStripCollapseState) {
            vigia->Atualizar();
          },
          base::Unretained(this)));
    }
  }

  // views::View: a área das páginas já existe quando a lateral entra na
  // janela.
  void AddedToWidget() override {
    views::View* area = browser_view_->contents_container();
    if (linha_ || !area) {
      return;
    }
    linha_ = area->AddChildView(std::make_unique<Linha>());
    area_.Observe(area);
    Posicionar();
  }

  // views::ViewObserver (a área das páginas):
  void OnViewBoundsChanged(views::View* area) override { Posicionar(); }
  void OnViewIsDeleting(views::View* area) override {
    area_.Reset();
    linha_ = nullptr;
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (selection.active_tab_changed()) {
      Observe(model_->GetActiveWebContents());
      Atualizar();
    }
  }

  // content::WebContentsObserver:
  void DidStartLoading() override { Atualizar(); }
  void DidStopLoading() override { Atualizar(); }

 private:
  void Posicionar() {
    if (linha_ && linha_->parent()) {
      linha_->SetBounds(0, 0, linha_->parent()->width(), kAltura);
    }
  }

  bool ModoSemSinais() const {
    auto* lateral =
        tabs::VerticalTabStripStateController::From(browser_view_->browser());
    return lateral && lateral->IsCollapsed() && barra::Escondida(browser_view_);
  }

  void Atualizar() {
    if (!linha_) {
      return;
    }
    const bool carregando = web_contents() && web_contents()->IsLoading();
    if (carregando && ModoSemSinais()) {
      if (!linha_->GetVisible() && !espera_.IsRunning()) {
        espera_.Start(FROM_HERE, kEspera,
                      base::BindOnce(&Vigia::Mostrar, base::Unretained(this)));
      }
      return;
    }
    espera_.Stop();
    linha_->Esconder();
  }

  void Mostrar() {
    if (linha_ && web_contents() && web_contents()->IsLoading() &&
        ModoSemSinais()) {
      linha_->Mostrar();
    }
  }

  const raw_ptr<BrowserView> browser_view_;
  const raw_ptr<TabStripModel> model_;
  raw_ptr<Linha> linha_ = nullptr;
  base::OneShotTimer espera_;
  PrefChangeRegistrar preferencias_;
  base::CallbackListSubscription recolhida_;
  base::ScopedObservation<views::View, views::ViewObserver> area_{this};
};

BEGIN_METADATA(Vigia)
END_METADATA

}  // namespace

void Acompanhar(views::View* dono, BrowserView* browser_view) {
  dono->AddChildView(std::make_unique<Vigia>(browser_view));
}

}  // namespace navegador::sinal_de_carga
