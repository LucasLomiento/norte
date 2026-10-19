// Cartão "tocando agora" no pé da lateral, como os controles de mídia do Zen:
// a mídia que toca numa aba desta janela que não está à vista (outra aba ou
// outro espaço), com a capa (ou o ícone do site), o título, tocar/pausar e
// próxima faixa, e uma linha com o andamento. Clicar no cartão vai até a aba
// (trocando de espaço, se for o caso). Segue a sessão de mídia ativa do
// Chromium (a última que pegou o foco de áudio).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_TOCANDO_H_
#define NAVEGADOR_BROWSER_UI_TOCANDO_H_

#include <memory>

class BrowserWindowInterface;

namespace views {
class View;
}  // namespace views

namespace navegador::tocando {

std::unique_ptr<views::View> CriarCartao(BrowserWindowInterface* browser);

}  // namespace navegador::tocando

#endif  // NAVEGADOR_BROWSER_UI_TOCANDO_H_
