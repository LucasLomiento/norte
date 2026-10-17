#include "navegador/browser/ui/barra.h"

#include <map>
#include <optional>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/non_client_view.h"

namespace navegador::barra {

namespace {

// Refaz o layout da janela: o BrowserView::IsToolbarVisible() consulta
// Escondida(), e o layout esconde ou mostra a barra a partir dele.
void Refazer(BrowserView* view) {
  view->InvalidateLayout();
  if (views::Widget* widget = view->GetWidget()) {
    // A moldura também: os botões da janela somem e voltam com a barra.
    if (widget->non_client_view() && widget->non_client_view()->frame_view()) {
      widget->non_client_view()->frame_view()->InvalidateLayout();
      widget->non_client_view()->frame_view()->SchedulePaint();
    }
    widget->GetRootView()->SchedulePaint();
  }
}

// Se o foco está na barra que vai sumir, passa pra página (senão o views
// passa pro próximo da fila, que pode ser a lateral).
void TirarFocoDaBarra(BrowserView* view) {
  views::FocusManager* foco = view->GetFocusManager();
  if (!foco || !view->toolbar() ||
      !view->toolbar()->Contains(foco->GetFocusedView())) {
    return;
  }
  if (content::WebContents* pagina = view->GetActiveWebContents()) {
    pagina->Focus();
  }
}

bool Ligada(const BrowserView* view) {
  const BrowserWindowInterface* browser = view ? view->browser() : nullptr;
  return browser && browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         browser->GetProfile()->GetPrefs()->GetBoolean(
             espacos::kBarraEscondidaPref);
}

// Faixa do alto da página que mostra a barra escondida (px).
constexpr int kZonaDoTopo = 6;
// Folga abaixo da barra antes de contar que o ponteiro voltou pra página.
constexpr int kFolgaAbaixo = 12;

// A barra escondida de uma janela, mostrada por um tempo: pelo Ctrl+L / nova
// aba (até o foco sair dela) ou pelo ponteiro no alto da página ou saindo da
// janela por cima (até ele voltar pra página e passar a folga, como a lateral
// escondida).
class Espia : public views::FocusChangeListener,
              public views::WidgetObserver,
              public ui::EventHandler,
              public gfx::AnimationDelegate {
 public:
  Espia(BrowserView* view, views::Widget* widget)
      : view_(view), widget_(widget) {
    animacao_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    observacao_.Observe(widget);
    widget->GetFocusManager()->AddFocusChangeListener(this);
    if (aura::Window* janela = widget->GetNativeWindow();
        janela && janela->GetRootWindow()) {
      raiz_ = janela->GetRootWindow();
      raiz_->AddPreTargetHandler(this);
    }
  }
  Espia(const Espia&) = delete;
  Espia& operator=(const Espia&) = delete;
  ~Espia() override {
    if (raiz_) {
      raiz_->RemovePreTargetHandler(this);
    }
    if (views::FocusManager* foco = widget_->GetFocusManager()) {
      foco->RemoveFocusChangeListener(this);
    }
  }

  bool a_vista() const { return espiando_ || pelo_ponteiro_; }
  // Quanto a barra já desceu (0 escondida, 1 inteira), na animação.
  double valor() const { return animacao_.GetCurrentValue(); }

  // Ctrl+L / nova aba: à vista até o foco sair da barra.
  void Espiar() {
    espiando_ = true;
    esconder_.Stop();
    Animar();
  }

  // A pref mudou: começa escondida, sem nada pendente.
  void Zerar() {
    espiando_ = false;
    pelo_ponteiro_ = false;
    esconder_.Stop();
    animacao_.Reset(0);
    fraco_.InvalidateWeakPtrs();
  }

  // gfx::AnimationDelegate: cada quadro refaz o layout (a barra desce por
  // cima da página, que não muda de tamanho).
  void AnimationProgressed(const gfx::Animation* animation) override {
    Refazer(view_);
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    Refazer(view_);
  }

  // views::FocusChangeListener:
  void OnDidChangeFocus(views::View* antes, views::View* agora) override {
    // Sem foco = a janela perdeu a vez (outra janela, um menu): a barra fica,
    // o foco volta pra ela quando a janela voltar.
    if (!espiando_ || !agora || !view_->top_container() ||
        view_->top_container()->Contains(agora)) {
      return;
    }
    // Fora do aviso de mudança de foco: o layout esconde a barra e mexe na
    // visibilidade de views (que mexe no foco).
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Espia::FimDoFoco, fraco_.GetWeakPtr()));
  }

  // ui::EventHandler: o ponteiro na janela (antes da página e das views).
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (!Ligada(view_) || !view_->multi_contents_view()) {
      return;
    }
    // Tela cheia de um site (vídeo): a barra nunca aparece ali, e cada
    // passagem pelo alto animava o layout da janela à toa.
    if (content::WebContents* ativa = view_->GetActiveWebContents();
        ativa && ativa->IsFullscreen()) {
      return;
    }
    const gfx::Point ponto = event->root_location();
    // A página (abaixo da barra, quando ela está à vista), na raiz.
    const gfx::Rect pagina =
        view_->multi_contents_view()->ConvertRectToWidget(
            view_->multi_contents_view()->GetLocalBounds());
    const bool na_coluna =
        ponto.x() >= pagina.x() && ponto.x() < pagina.right();
    switch (event->type()) {
      case ui::EventType::kMouseMoved:
      case ui::EventType::kMouseEntered:
        if (!a_vista()) {
          if (na_coluna && ponto.y() <= pagina.y() + kZonaDoTopo) {
            MostrarPeloPonteiro();
          }
        } else if (pelo_ponteiro_) {
          const bool fora = ponto.y() > pagina.y() + kFolgaAbaixo ||
                            ponto.x() < pagina.x() - kFolgaAbaixo;
          if (!fora) {
            esconder_.Stop();
          } else if (!esconder_.IsRunning()) {
            ContarFolga(base::Milliseconds(espacos::EsconderDepoisMs(
                view_->browser()->GetProfile())));
          }
        }
        break;
      case ui::EventType::kMouseExited:
        // Só a saída de verdade (o evento da plataforma, na janela raiz).
        if (event->target() != raiz_.get() ||
            (event->flags() & ui::EF_IS_SYNTHESIZED)) {
          break;
        }
        // Jogado pro alto (além do vão entre as janelas): mostra, com a folga
        // de fora da janela, como a lateral pela borda esquerda.
        if (!a_vista() && na_coluna && ponto.y() <= kZonaDoTopo) {
          MostrarPeloPonteiro();
        }
        if (pelo_ponteiro_) {
          ContarFolga(base::Milliseconds(
              espacos::EsconderForaMs(view_->browser()->GetProfile())));
        }
        break;
      default:
        break;
    }
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Desce ou sobe a barra conforme o estado, nos tempos da lateral escondida
  // (tab_strip_animations.cc: 250 ms pra abrir, 150 ms pra fechar).
  void Animar() {
    if (a_vista()) {
      animacao_.SetSlideDuration(base::Milliseconds(250));
      animacao_.Show();
    } else {
      animacao_.SetSlideDuration(base::Milliseconds(150));
      animacao_.Hide();
    }
    Refazer(view_);
  }

  void MostrarPeloPonteiro() {
    pelo_ponteiro_ = true;
    esconder_.Stop();
    Animar();
  }

  void ContarFolga(base::TimeDelta folga) {
    esconder_.Start(FROM_HERE, folga,
                    base::BindOnce(&Espia::FimDaFolga, base::Unretained(this)));
  }

  void FimDaFolga() {
    // Um menu ou um balão da barra aberto (o pop-up de uma extensão,
    // informações do site, favorito): o ponteiro foi pra ele, e subir a barra
    // levava o balão junto. Espera fechar, como a lateral.
    if (MenuOuBalaoAberto()) {
      ContarFolga(base::Milliseconds(200));
      return;
    }
    pelo_ponteiro_ = false;
    // Digitando na barra (clicou no endereço) ou nos favoritos: fica até o
    // foco sair dela.
    views::FocusManager* foco = view_->GetFocusManager();
    if (foco && view_->top_container() &&
        view_->top_container()->Contains(foco->GetFocusedView())) {
      espiando_ = true;
    }
    Animar();
  }

  bool MenuOuBalaoAberto() const {
    if (views::MenuController::GetActiveInstance()) {
      return true;
    }
    views::View* topo = view_->top_container();
    if (!topo) {
      return false;
    }
    for (views::Widget* outro :
         views::Widget::GetAllOwnedWidgets(widget_->GetNativeView())) {
      if (!outro->IsVisible() || !outro->widget_delegate()) {
        continue;
      }
      views::BubbleDialogDelegate* balao =
          outro->widget_delegate()->AsBubbleDialogDelegate();
      views::View* ancora = balao ? balao->GetAnchorView() : nullptr;
      if (ancora && topo->Contains(ancora)) {
        return true;
      }
    }
    return false;
  }

  void FimDoFoco() {
    espiando_ = false;
    Animar();
  }

  const raw_ptr<BrowserView> view_;
  const raw_ptr<views::Widget> widget_;
  raw_ptr<aura::Window> raiz_ = nullptr;
  bool espiando_ = false;
  bool pelo_ponteiro_ = false;
  base::OneShotTimer esconder_;
  gfx::SlideAnimation animacao_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver> observacao_{
      this};
  base::WeakPtrFactory<Espia> fraco_{this};
};

std::map<const BrowserView*, std::unique_ptr<Espia>>& Espias() {
  static base::NoDestructor<std::map<const BrowserView*, std::unique_ptr<Espia>>>
      espias;
  return *espias;
}

void Espia::OnWidgetDestroying(views::Widget* widget) {
  // Apaga este objeto.
  Espias().erase(view_);
}

Espia* EspiaDe(BrowserView* view) {
  views::Widget* widget = view->GetWidget();
  // Janela fechando: o layout ainda consulta, mas não há mais o que seguir.
  if (!widget || widget->IsClosed() || !widget->GetFocusManager()) {
    return nullptr;
  }
  std::unique_ptr<Espia>& espia = Espias()[view];
  if (!espia) {
    espia = std::make_unique<Espia>(view, widget);
  }
  return espia.get();
}

}  // namespace

bool Escondida(const BrowserView* view) {
  if (!Ligada(view)) {
    return false;
  }
  // Consultado no layout: a primeira vez cria quem segue o ponteiro.
  Espia* espia = EspiaDe(const_cast<BrowserView*>(view));
  return !espia || (!espia->a_vista() && espia->valor() == 0);
}

std::optional<double> Sobreposta(const BrowserView* view) {
  if (!Ligada(view) || Escondida(view)) {
    return std::nullopt;
  }
  auto it = Espias().find(view);
  return it == Espias().end() ? std::nullopt
                              : std::optional<double>(it->second->valor());
}

void Definir(Profile* perfil, bool escondida) {
  perfil->GetPrefs()->SetBoolean(espacos::kBarraEscondidaPref, escondida);
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile() != perfil) {
      continue;
    }
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
    if (!view) {
      continue;
    }
    if (auto it = Espias().find(view); it != Espias().end()) {
      it->second->Zerar();
    }
    if (escondida) {
      TirarFocoDaBarra(view);
    }
    Refazer(view);
  }
}

void Alternar(BrowserWindowInterface* browser) {
  Profile* perfil = browser->GetProfile();
  Definir(perfil,
          !perfil->GetPrefs()->GetBoolean(espacos::kBarraEscondidaPref));
}

void Espiar(BrowserView* view) {
  views::Widget* widget = view->GetWidget();
  Espia* espia = Ligada(view) ? EspiaDe(view) : nullptr;
  if (!espia) {
    return;
  }
  espia->Espiar();
  // Já: o endereço só aceita foco com a barra à vista (o layout do
  // BrowserView chama o UpdateFocusBehavior).
  view->InvalidateLayout();
  widget->LayoutRootViewIfNecessary();
}

}  // namespace navegador::barra
