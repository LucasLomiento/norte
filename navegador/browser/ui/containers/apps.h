// Container dos apps instalados (PWA): o submenu "Container" no menu da
// janela do app (web_app_menu_model.cc) e a lista nas Configurações do Norte
// (página Containers). Compilado dentro do
// //chrome/browser/ui/web_applications:impl, que tem o AppBrowserController
// e o registro dos apps (e é dependência do //chrome/browser/ui:ui).
//
// A escolha é do app (navegador/browser/containers/containers.h,
// ContainerDoApp): toda aba nova numa janela dele nasce nesse container.

#ifndef NAVEGADOR_BROWSER_UI_CONTAINERS_APPS_H_
#define NAVEGADOR_BROWSER_UI_CONTAINERS_APPS_H_

#include <memory>
#include <string>
#include <vector>

class BrowserWindowInterface;
class Profile;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace navegador::containers {

struct AppInstalado {
  std::string id;
  std::u16string nome;
  // Endereço de início sem o esquema, pra diferenciar apps de mesmo nome.
  std::u16string site;
};

// Apps instalados que abrem em janela própria, por nome. Os isolados (IWA)
// ficam de fora: têm partição própria.
std::vector<AppInstalado> AppsInstalados(Profile* profile);

// Nome do container em que o app abre ("Sem container" se nenhum).
std::u16string NomeDoContainerDoApp(Profile* profile,
                                    const std::string& app_id);

// Grava o container do app ("" = sem container) e reabre nele as abas das
// janelas abertas do app, no mesmo endereço.
void TrocarContainerDoApp(Profile* profile,
                          const std::string& app_id,
                          const std::string& container);

// Submenu "Container" do menu da janela do app; nullptr fora de janela de
// app (PWA) ou na janela anônima.
std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDoApp(
    BrowserWindowInterface* browser);

}  // namespace navegador::containers

#endif  // NAVEGADOR_BROWSER_UI_CONTAINERS_APPS_H_
