#include "navegador/browser/ui/containers/espaco.h"

#include <map>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "navegador/browser/containers/espacos_dados.h"

namespace navegador::containers {

namespace {

std::map<BrowserWindowInterface*, std::string>& Pendentes() {
  static base::NoDestructor<std::map<BrowserWindowInterface*, std::string>>
      pendentes;
  return *pendentes;
}

}  // namespace

std::optional<std::string> EspacoFocado(BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel()) {
    return std::nullopt;
  }
  if (auto it = Pendentes().find(browser); it != Pendentes().end()) {
    return it->second;
  }
  std::optional<tab_groups::TabGroupId> grupo =
      browser->GetTabStripModel()->GetFocusedGroup();
  if (!grupo) {
    // Janela nova, ainda sem grupo: a primeira aba vai pro espaço em que a
    // janela vai entrar (o último usado), e precisa nascer no container dele.
    if (browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
      return std::nullopt;
    }
    // Janela que perdeu o foco do grupo (o Chromium tira a cada aba inserida
    // sem grupo): o espaço que ela mostra, não o da última janela usada.
    if (std::optional<tab_groups::TabGroupId> visivel =
            espacos::GrupoVisivel(browser->GetTabStripModel())) {
      if (std::optional<std::string> espaco = espacos::EspacoDoGrupo(
              browser->GetProfile(), visivel->ToString())) {
        return espaco;
      }
    }
    return espacos::Ativo(browser->GetProfile());
  }
  return espacos::EspacoDoGrupo(browser->GetProfile(), grupo->ToString());
}

EspacoPendente::EspacoPendente(BrowserWindowInterface* browser,
                               std::string espaco)
    : browser_(browser) {
  Pendentes()[browser_] = std::move(espaco);
}

EspacoPendente::~EspacoPendente() {
  Pendentes().erase(browser_);
}

}  // namespace navegador::containers
