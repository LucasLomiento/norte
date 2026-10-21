#include "navegador/browser/ui/seguidora.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "ui/base/page_transition_types.h"

namespace navegador::seguidora {

namespace {

// As abas com a opção ligada. Aba fechada fica com o identificador velho,
// que não acha mais ninguém.
std::set<tabs::TabHandle>& Lideres() {
  static base::NoDestructor<std::set<tabs::TabHandle>> lideres;
  return *lideres;
}

// A outra metade da divisão de `aba`, ou nada.
tabs::TabInterface* OutraMetade(tabs::TabInterface* aba) {
  if (!aba || !aba->IsSplit()) {
    return nullptr;
  }
  BrowserWindowInterface* janela = aba->GetBrowserWindowInterface();
  TabStripModel* model = janela ? janela->GetTabStripModel() : nullptr;
  split_tabs::SplitTabData* divisao =
      model ? model->GetSplitData(*aba->GetSplit()) : nullptr;
  if (!divisao) {
    return nullptr;
  }
  for (tabs::TabInterface* outra : divisao->ListTabs()) {
    if (outra != aba) {
      return outra;
    }
  }
  return nullptr;
}

// Troca o clique num link da aba líder por abrir o mesmo endereço na outra
// metade.
class Desvio : public content::NavigationThrottle {
 public:
  explicit Desvio(content::NavigationThrottleRegistry& registry)
      : content::NavigationThrottle(registry) {}

  ThrottleCheckResult WillStartRequest() override {
    content::NavigationHandle* navegacao = navigation_handle();
    tabs::TabInterface* aba =
        tabs::TabInterface::MaybeGetFromContents(navegacao->GetWebContents());
    if (!Ligada(aba) || !navegacao->IsRendererInitiated() ||
        !navegacao->HasUserGesture() || navegacao->IsPost() ||
        navegacao->IsSameDocument() ||
        !ui::PageTransitionCoreTypeIs(navegacao->GetPageTransition(),
                                      ui::PAGE_TRANSITION_LINK) ||
        !navegacao->GetURL().SchemeIsHTTPOrHTTPS()) {
      return PROCEED;
    }
    tabs::TabInterface* outra = OutraMetade(aba);
    if (!outra) {
      return PROCEED;
    }
    content::NavigationController::LoadURLParams parametros(
        navegacao->GetURL());
    parametros.referrer = content::Referrer(navegacao->GetReferrer());
    parametros.transition_type = ui::PAGE_TRANSITION_LINK;
    // Fora daqui: uma navegação não começa outra de dentro do throttle.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](tabs::TabHandle destino,
               content::NavigationController::LoadURLParams parametros) {
              if (tabs::TabInterface* aba = destino.Get()) {
                aba->GetContents()->GetController().LoadURLWithParams(
                    parametros);
              }
            },
            outra->GetHandle(), std::move(parametros)));
    return CANCEL_AND_IGNORE;
  }

  const char* GetNameForLogging() override { return "NavegadorSeguidora"; }
};

}  // namespace

bool Ligada(const tabs::TabInterface* aba) {
  return aba && Lideres().contains(aba->GetHandle());
}

void Alternar(tabs::TabInterface* aba) {
  if (!aba) {
    return;
  }
  const tabs::TabHandle handle = aba->GetHandle();
  if (!Lideres().erase(handle)) {
    Lideres().insert(handle);
  }
}

void TalvezAdicionar(content::NavigationThrottleRegistry& registry) {
  if (!Lideres().empty() &&
      registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(std::make_unique<Desvio>(registry));
  }
}

}  // namespace navegador::seguidora
