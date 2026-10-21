// Sinal de carregamento com a lateral recolhida (Ctrl+S) e a barra escondida
// (Ctrl+Shift+S): uma linha fina, pulsando, no alto da página da aba ativa.
// Os dois sinais de sempre (o giro do ícone da aba na lateral e o recarregar
// virando "x" na barra) ficam fora da tela nesse modo. Aparece só depois de
// 300 ms de carga (as rápidas não piscam nada).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_SINAL_DE_CARGA_H_
#define NAVEGADOR_BROWSER_UI_SINAL_DE_CARGA_H_

class BrowserView;

namespace views {
class View;
}  // namespace views

namespace navegador::sinal_de_carga {

// Põe dentro de `dono` (uma View que vive tanto quanto a janela: a lateral)
// quem cuida do sinal da janela `browser_view`.
void Acompanhar(views::View* dono, BrowserView* browser_view);

}  // namespace navegador::sinal_de_carga

#endif  // NAVEGADOR_BROWSER_UI_SINAL_DE_CARGA_H_
