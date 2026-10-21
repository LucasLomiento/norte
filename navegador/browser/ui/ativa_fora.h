// A aba ativa fora de vista: com muitas abas, rolando a lista da lateral, a
// ativa sai da tela e nada mostra onde ela está. Um aviso preso no alto (ou
// embaixo) da lista mostra o ícone e o nome dela, com a seta pro lado em
// que ela ficou; clicar rola até ela.
//
// Compilado dentro de //chrome/browser/ui:ui; chamado pelo TabStripView
// (só na lateral vertical).

#ifndef NAVEGADOR_BROWSER_UI_ATIVA_FORA_H_
#define NAVEGADOR_BROWSER_UI_ATIVA_FORA_H_

class BrowserWindowInterface;

namespace views {
class ScrollView;
class View;
}  // namespace views

namespace navegador::ativa_fora {

// Põe o aviso em `dono` (o TabStripView), acompanhando `lista` (a rolagem
// das abas soltas, filha de `dono`).
void Acompanhar(views::View* dono,
                views::ScrollView* lista,
                BrowserWindowInterface* browser);

}  // namespace navegador::ativa_fora

#endif  // NAVEGADOR_BROWSER_UI_ATIVA_FORA_H_
