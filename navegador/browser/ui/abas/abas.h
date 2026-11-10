// Detalhes das abas da lateral no estilo do Dia.

#ifndef NAVEGADOR_BROWSER_UI_ABAS_ABAS_H_
#define NAVEGADOR_BROWSER_UI_ABAS_ABAS_H_

#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class Label;
class View;
}  // namespace views

namespace navegador::abas {

// TabView::OnPaint, antes do fundo da aba: as abas fixadas são "ladrilhos"
// (os favoritos do Dia), com um fundo um pouco mais claro que a lateral.
void PintarLadrilho(gfx::Canvas* canvas, const views::View& aba, bool fixada);

// Renomear no lugar (clique duplo no nome de uma aba da lateral, de um site
// fixado ou de uma pasta): um campo por cima do `rotulo`, filho de `dono`,
// indo até o fim da linha. Enter ou clicar fora grava; Esc cancela.
// `pronto` recebe o texto (sem espaços nas pontas; pode ser vazio) depois do
// evento, já sem o campo, e só se o dono ainda existir.
void EditarNoLugar(views::View* dono,
                   views::Label& rotulo,
                   std::u16string_view inicial,
                   base::OnceCallback<void(std::u16string)> pronto);

// Dá um nome à aba (vazio volta o título da página). As abas
// (chrome/browser/ui/views/tabs) não enxergam o TabUIHelper: a lateral liga a
// ponte (espacos.cc).
// Se `dono` está com o campo de renomear aberto. As teclas que o campo não
// trata (o espaço, por exemplo: a letra entra depois, pelo método de entrada)
// sobem pros pais; quem trata teclas (botões, a aba) deixa passar enquanto
// isso, senão o espaço vira clique e some do nome.
bool Editando(const views::View* dono);

// O ponteiro está mesmo em cima de `view`: o `IsMouseHovered()` e mais a
// janela debaixo do ponteiro ser a dela. No Wayland o Chromium não sabe
// onde as janelas estão, e a posição do ponteiro que ele dá é a de dentro
// da janela em que o ponteiro está, pra qualquer janela que pergunte: com
// duas janelas, a que repintava uma linha achava o ponteiro em cima dela
// (destaque e "x" acesos na janela errada). Pra usar em pintura e em
// conferências fora de um evento de mouse da própria view.
bool PonteiroEmCima(const views::View* view);

using Renomeador =
    base::RepeatingCallback<void(tabs::TabInterface*, const std::u16string&)>;
void DefinirRenomeador(Renomeador renomeador);
void RenomearAba(tabs::TabInterface* aba, const std::u16string& nome);

}  // namespace navegador::abas

#endif  // NAVEGADOR_BROWSER_UI_ABAS_ABAS_H_
