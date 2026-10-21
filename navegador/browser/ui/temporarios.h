// Containers temporários ("Novo container temporário" nos menus de
// container): quando a última aba de um deles fecha, o container some com os
// dados (cookies, armazenamento, cache). Fechar o Norte com uma aba
// temporária aberta não apaga nada: a sessão restaurada volta nele. Se a
// sessão não voltar, a limpeza de 1 min depois de abrir apaga os temporários
// que ficaram sem aba.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_TEMPORARIOS_H_
#define NAVEGADOR_BROWSER_UI_TEMPORARIOS_H_

namespace content {
class WebContents;
}  // namespace content

namespace navegador::temporarios {

// tab_helpers.cc: acompanha toda aba (e, na primeira de cada perfil, marca
// a limpeza da partida).
void AcompanharAba(content::WebContents* contents);

}  // namespace navegador::temporarios

#endif  // NAVEGADOR_BROWSER_UI_TEMPORARIOS_H_
