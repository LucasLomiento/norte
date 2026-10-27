// Cartão "Claude está usando o navegador" no pé da lateral, no lugar da tarja
// preta do Chromium (o aviso de que uma extensão está usando o depurador; ver
// navegador/browser/containers/claude.h). Aparece em toda janela enquanto a
// extensão do Claude mexe no navegador, no laranja do Claude, com o container
// em que ele abre abas (dá pra trocar ali) e "Parar", que desconecta ele.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_CLAUDE_H_
#define NAVEGADOR_BROWSER_UI_CLAUDE_H_

#include <memory>
#include <optional>
#include <string>

class BrowserWindowInterface;
class Profile;

namespace views {
class View;
}  // namespace views

namespace navegador::claude {

std::unique_ptr<views::View> CriarCartao(BrowserWindowInterface* browser);

// Grava o container das abas do Claude (nada = o do espaço em que a pessoa
// está; "" = sem container) e passa os espaços do Claude abertos pra ele.
void EscolherContainer(Profile* perfil,
                       const std::optional<std::string>& container);

}  // namespace navegador::claude

#endif  // NAVEGADOR_BROWSER_UI_CLAUDE_H_
