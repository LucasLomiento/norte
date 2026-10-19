// Prévia de link, como o Glance do Zen e o Peek do Arc: Shift+clique num link
// abre a página num cartão por cima da aba (a página de trás fica escura e
// desfocada). Esc, clicar fora ou o "x" fecham; "Abrir como aba" passa a
// página (já carregada, com o histórico dela) pra uma aba nova ao lado, no
// mesmo espaço; "Dividir" faz o mesmo e põe as duas lado a lado.
//
// A página da prévia nasce no container da aba de origem. Links que ela abre
// em aba nova vão pra janela como se viessem da aba de origem.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_PREVIA_H_
#define NAVEGADOR_BROWSER_UI_PREVIA_H_

#include <optional>

#include "content/public/common/referrer.h"
#include "url/gurl.h"
#include "url/origin.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace navegador::previa {

// Abre a prévia de `url` na janela. Devolve false se não deu (janela que não
// é comum, sem aba de origem nela): aí o Chromium segue com a janela nova.
bool Abrir(BrowserWindowInterface* browser,
           content::WebContents* origem,
           const GURL& url,
           const content::Referrer& referrer,
           const std::optional<url::Origin>& iniciador);

}  // namespace navegador::previa

#endif  // NAVEGADOR_BROWSER_UI_PREVIA_H_
