// Rotas dos links que chegam de fora (outro app, o terminal, o "abrir com"):
// abrem no espaço certo, como o "Air Traffic Control" do Arc.
//   - `norte --space=<nome ou id> [endereço]`: no espaço escolhido;
//   - sem isso, o primeiro endereço http(s) cujo site está na lista de um
//     espaço (Editar espaço → Sites) vai pra esse espaço.
// Sem rota, abre no espaço da janela, como antes.
//
// Compilado dentro de //chrome/browser/ui:ui; chamado pelo
// StartupBrowserCreatorImpl::OpenTabsInBrowser.

#ifndef NAVEGADOR_BROWSER_UI_ROTAS_H_
#define NAVEGADOR_BROWSER_UI_ROTAS_H_

#include <vector>

#include "base/memory/weak_ptr.h"

class BrowserWindowInterface;
class GURL;

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace navegador::rotas {

// Leva `browser` pro espaço dos `enderecos`, antes de eles abrirem. Num
// espaço sem abas, a troca cria uma "Nova aba": ela vem de volta, pra
// FecharVazia fechar depois que os links abrirem.
base::WeakPtr<content::WebContents> LevarProEspaco(
    BrowserWindowInterface* browser,
    const base::CommandLine& linha,
    const std::vector<GURL>& enderecos);

// Fecha a aba que o LevarProEspaco criou, se ela continua vazia.
void FecharVazia(BrowserWindowInterface* browser,
                 base::WeakPtr<content::WebContents> vazia);

}  // namespace navegador::rotas

#endif  // NAVEGADOR_BROWSER_UI_ROTAS_H_
