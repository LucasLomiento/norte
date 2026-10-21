#include "navegador/browser/ui/rotas.h"

#include <optional>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/ui/espacos.h"
#include "url/gurl.h"

namespace navegador::rotas {

namespace {

constexpr char kSwitchEspaco[] = "space";

// O espaço de `--space=`: pelo id ou pelo nome (sem diferença de maiúscula).
std::optional<std::string> EspacoPedido(Profile* perfil,
                                        const std::string& pedido) {
  if (pedido.empty()) {
    return std::nullopt;
  }
  for (const espacos::Espaco& espaco : espacos::Todos(perfil)) {
    if (espaco.id == pedido ||
        base::EqualsCaseInsensitiveASCII(espaco.nome, pedido)) {
      return espaco.id;
    }
  }
  return std::nullopt;
}

}  // namespace

base::WeakPtr<content::WebContents> LevarProEspaco(
    BrowserWindowInterface* browser,
    const base::CommandLine& linha,
    const std::vector<GURL>& enderecos) {
  if (!browser || browser->GetProfile()->IsOffTheRecord() ||
      browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
    return nullptr;
  }
  Profile* perfil = browser->GetProfile()->GetOriginalProfile();
  std::optional<std::string> espaco =
      EspacoPedido(perfil, linha.GetSwitchValueUTF8(kSwitchEspaco));
  for (size_t i = 0; !espaco && i < enderecos.size(); ++i) {
    espaco = espacos::EspacoDoSite(perfil, enderecos[i]);
  }
  if (!espaco) {
    return nullptr;
  }
  TabStripModel* model = browser->GetTabStripModel();
  std::set<content::WebContents*> antes;
  for (int i = 0; i < model->count(); ++i) {
    antes.insert(model->GetWebContentsAt(i));
  }
  espacos::IrPara(browser, *espaco);
  for (int i = 0; i < model->count(); ++i) {
    content::WebContents* aba = model->GetWebContentsAt(i);
    if (!antes.contains(aba)) {
      return aba->GetWeakPtr();
    }
  }
  return nullptr;
}

void FecharVazia(BrowserWindowInterface* browser,
                 base::WeakPtr<content::WebContents> vazia) {
  if (!browser || !vazia) {
    return;
  }
  TabStripModel* model = browser->GetTabStripModel();
  const int indice = model->GetIndexOfWebContents(vazia.get());
  // Só se continua sendo a nova aba sem histórico e os links entraram.
  if (indice == TabStripModel::kNoTab || model->count() <= 1 ||
      vazia->GetController().GetEntryCount() > 1 ||
      vazia->GetVisibleURL() != GURL(chrome::kChromeUINewTabURL)) {
    return;
  }
  model->CloseWebContentsAt(indice, TabCloseTypes::CLOSE_NONE);
}

}  // namespace navegador::rotas
