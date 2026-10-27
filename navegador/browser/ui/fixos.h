// Área dos fixados do espaço no alto da lateral (dados em
// navegador/browser/containers/fixos.h): os sites e as pastas do espaço
// atual da janela, entre os favoritos e as abas, com uma linha embaixo como
// no Zen. As pastas têm a cara dos grupos do Dia (nome em cinza e seta; os
// sites recuados).
//
// Clicar num site abre (ou ativa) a aba ligada a ele, que sai da lista de
// abas e aparece no lugar do site. O "x" fecha a aba; o site continua.
//
// Compilado dentro de //chrome/browser/ui:ui (precisa do Navigate, da sessão
// e dos favicons).

#ifndef NAVEGADOR_BROWSER_UI_FIXOS_H_
#define NAVEGADOR_BROWSER_UI_FIXOS_H_

class BrowserWindowInterface;
class TabDragTarget;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace navegador::fixos {

// Põe a área dentro do `tab_strip_view` (o TabStripView da lateral, cujo
// layout acha a área pelo id visual::kIdAreaDeFixos).
void AdicionarArea(views::View* tab_strip_view, BrowserWindowInterface* browser);

// Alvo do arrasto de abas (BrowserView::GetTabDragTarget): uma aba comum da
// lista desta janela, arrastada pra cima da área, é fixada onde for solta
// (do lado de um site, dentro de uma pasta, nos favoritos ou no fim). Nulo
// fora da área ou pra outro arrasto (espaço, divisão, aba de outra janela).
// Sem nada fixado, o arrasto mostra o lugar de soltar no alto da lateral.
TabDragTarget* AlvoDosFixos(BrowserWindowInterface* browser,
                            const gfx::Point& ponto_na_tela);

}  // namespace navegador::fixos

#endif  // NAVEGADOR_BROWSER_UI_FIXOS_H_
