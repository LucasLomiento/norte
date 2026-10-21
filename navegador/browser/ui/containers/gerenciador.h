// Janela "Containers": criar, renomear, trocar a cor e excluir.

#ifndef NAVEGADOR_BROWSER_UI_CONTAINERS_GERENCIADOR_H_
#define NAVEGADOR_BROWSER_UI_CONTAINERS_GERENCIADOR_H_

#include <memory>

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace navegador::containers {

// A lista de containers editável no lugar (criar, renomear, cor, excluir),
// pra pôr dentro de outra janela (as Configurações do Navegador).
std::unique_ptr<views::View> CriarPainel(Profile* profile);

}  // namespace navegador::containers

#endif  // NAVEGADOR_BROWSER_UI_CONTAINERS_GERENCIADOR_H_
