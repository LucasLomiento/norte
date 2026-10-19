// Espaço da janela (id do espaço, ver espacos_dados.h), pros containers.
// Separado do containers_ui pra o browser_navigator poder usar sem puxar a
// interface.

#ifndef NAVEGADOR_BROWSER_UI_CONTAINERS_ESPACO_H_
#define NAVEGADOR_BROWSER_UI_CONTAINERS_ESPACO_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"
#include "url/origin.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace navegador::containers {

// Espaço das abas novas da janela: o pendente (ver abaixo), se houver; senão
// o do grupo focado; numa janela nova, ainda sem grupo, o último usado.
std::optional<std::string> EspacoFocado(BrowserWindowInterface* browser);

// Enquanto vive, as abas novas da janela são do `espaco` (a janela criando a
// primeira aba de um espaço que ainda não tem grupo nela).
class EspacoPendente {
 public:
  EspacoPendente(BrowserWindowInterface* browser, std::string espaco);
  EspacoPendente(const EspacoPendente&) = delete;
  EspacoPendente& operator=(const EspacoPendente&) = delete;
  ~EspacoPendente();

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

// Prévia de link (Shift+clique, ver navegador/browser/ui/previa.h). A
// interface registra quem abre; o browser_navigator pergunta antes de abrir
// a janela nova. Devolve false quando a prévia não abriu (aí segue o normal).
using AbridorDePrevia = base::RepeatingCallback<bool(
    BrowserWindowInterface* browser,
    content::WebContents* origem,
    const GURL& url,
    const content::Referrer& referrer,
    const std::optional<url::Origin>& iniciador)>;
void DefinirAbridorDePrevia(AbridorDePrevia abridor);
bool AbrirPrevia(BrowserWindowInterface* browser,
                 content::WebContents* origem,
                 const GURL& url,
                 const content::Referrer& referrer,
                 const std::optional<url::Origin>& iniciador);

}  // namespace navegador::containers

#endif  // NAVEGADOR_BROWSER_UI_CONTAINERS_ESPACO_H_
