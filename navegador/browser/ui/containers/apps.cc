#include "navegador/browser/ui/containers/apps.h"

#include <algorithm>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace navegador::containers {

namespace {

// App (PWA) da janela, fora os isolados; nada se não é janela de app.
std::optional<std::string> AppDaJanela(BrowserWindowInterface* browser) {
  web_app::AppBrowserController* app =
      browser ? web_app::AppBrowserController::From(browser) : nullptr;
  if (!app || web_app::AppBrowserController::IsIsolatedWebApp(browser)) {
    return std::nullopt;
  }
  return app->app_id();
}

// "Container" no menu da janela do app: os containers do perfil (menos o
// atual), "Sem container" e "Gerenciar containers…". Como o "Abrir em
// container" da aba, mas a escolha fica pro app.
class SubmenuDoApp : public ui::SimpleMenuModel,
                     public ui::SimpleMenuModel::Delegate {
 public:
  SubmenuDoApp(BrowserWindowInterface* browser, std::string app_id)
      : ui::SimpleMenuModel(this),
        browser_(browser->GetWeakPtr()),
        app_id_(std::move(app_id)) {
    Profile* profile = browser->GetProfile();
    const std::string atual = ContainerDoApp(profile, app_id_).value_or("");
    for (Container& container : GetAll(profile)) {
      // Temporário some com os dados quando as abas dele fecham: não serve
      // de casa pra um app.
      if (container.temporario || container.id == atual) {
        continue;
      }
      containers_.push_back(std::move(container));
    }
    for (size_t i = 0; i < containers_.size(); ++i) {
      AddItemWithIcon(kPrimeiro + i, base::UTF8ToUTF16(containers_[i].name),
                      Bolinha(containers_[i].color));
    }
    if (!atual.empty()) {
      AddItem(kSemContainer, u"Sem container");
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kGerenciar, u"Gerenciar containers…");
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (!browser_) {
      return;
    }
    if (command_id == kGerenciar) {
      MostrarConfiguracoes(browser_.get(), /*containers=*/true);
      return;
    }
    std::string id;
    if (command_id != kSemContainer) {
      const size_t i = command_id - kPrimeiro;
      if (i >= containers_.size()) {
        return;
      }
      id = containers_[i].id;
    }
    TrocarContainerDoApp(browser_->GetProfile(), app_id_, id);
  }

 private:
  // Únicos no menu inteiro: o Views acha o comando pelo id no primeiro item
  // em profundidade (os do menu da aba são 21000-25199).
  static constexpr int kPrimeiro = 26100;
  static constexpr int kSemContainer = 26001;
  static constexpr int kGerenciar = 26002;

  base::WeakPtr<BrowserWindowInterface> browser_;
  const std::string app_id_;
  std::vector<Container> containers_;
};

}  // namespace

std::vector<AppInstalado> AppsInstalados(Profile* profile) {
  std::vector<AppInstalado> apps;
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    return apps;
  }
  const web_app::WebAppRegistrar& registro = provider->registrar_unsafe();
  for (const webapps::AppId& id : registro.GetAppIds(
           web_app::WebAppFilter::OpensInDedicatedWindow())) {
    if (!registro.AppMatches(id,
                             web_app::WebAppFilter::IsAppSurfaceableToUser()) ||
        registro.AppMatches(id, web_app::WebAppFilter::IsIsolatedApp())) {
      continue;
    }
    const GURL& inicio = registro.GetAppStartUrl(id);
    std::string site(inicio.host());
    if (inicio.path() != "/") {
      site += inicio.path();
    }
    apps.push_back({id, base::UTF8ToUTF16(registro.GetAppShortName(id)),
                    base::UTF8ToUTF16(site)});
  }
  std::ranges::sort(apps, {}, &AppInstalado::nome);
  return apps;
}

std::u16string NomeDoContainerDoApp(Profile* profile,
                                    const std::string& app_id) {
  const std::string id = ContainerDoApp(profile, app_id).value_or("");
  std::optional<Container> container =
      id.empty() ? std::nullopt : Get(profile, id);
  return container ? base::UTF8ToUTF16(container->name) : u"Sem container";
}

void TrocarContainerDoApp(Profile* profile,
                          const std::string& app_id,
                          const std::string& container) {
  DefinirContainerDoApp(profile, app_id, container);
  // As janelas abertas do app passam pro container novo já: as abas delas
  // são reabertas no mesmo endereço (o login muda junto com o container).
  std::vector<base::WeakPtr<BrowserWindowInterface>> janelas;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile &&
            AppDaJanela(browser) == app_id) {
          janelas.push_back(browser->GetWeakPtr());
        }
        return true;
      });
  for (const base::WeakPtr<BrowserWindowInterface>& janela : janelas) {
    if (!janela || !janela->GetTabStripModel()) {
      continue;
    }
    TabStripModel* abas = janela->GetTabStripModel();
    for (int i = abas->count() - 1; i >= 0; --i) {
      if (IdForWebContents(abas->GetWebContentsAt(i)).value_or("") !=
          container) {
        ReabrirEmContainer(abas, i, container);
      }
    }
  }
}

std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDoApp(
    BrowserWindowInterface* browser) {
  std::optional<std::string> app_id = AppDaJanela(browser);
  if (!app_id || browser->GetProfile()->IsOffTheRecord()) {
    return nullptr;
  }
  return std::make_unique<SubmenuDoApp>(browser, std::move(*app_id));
}

}  // namespace navegador::containers
