#include "navegador/browser/ui/temporarios.h"

#include <optional>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "navegador/browser/containers/containers.h"

namespace navegador::temporarios {

namespace {

constexpr base::TimeDelta kLimpezaDaPartida = base::Minutes(1);

// Alguma aba, em qualquer janela do perfil, está no container `id`?
bool TemAba(Profile* perfil, const std::string& id) {
  bool tem = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* janela) {
        if (janela->GetProfile()->GetOriginalProfile() != perfil) {
          return true;
        }
        TabStripModel* model = janela->GetTabStripModel();
        for (int i = 0; i < model->count(); ++i) {
          if (containers::IdForWebContents(model->GetWebContentsAt(i)) == id) {
            tem = true;
            return false;
          }
        }
        return true;
      });
  return tem;
}

void ApagarSeSemAba(base::WeakPtr<Profile> perfil, std::string id) {
  if (!perfil || browser_shutdown::HasShutdownStarted()) {
    return;
  }
  std::optional<containers::Container> container =
      containers::Get(perfil.get(), id);
  if (!container || !container->temporario || TemAba(perfil.get(), id)) {
    return;
  }
  containers::ApagarDados(perfil.get(), id);
  containers::Remove(perfil.get(), id);
}

void LimparSobras(base::WeakPtr<Profile> perfil) {
  if (!perfil) {
    return;
  }
  for (const containers::Container& container :
       containers::GetAll(perfil.get())) {
    if (container.temporario) {
      ApagarSeSemAba(perfil, container.id);
    }
  }
}

// Em cada aba: quando ela some, confere se o container temporário dela
// ficou sem aba.
class Vigia : public content::WebContentsObserver,
              public content::WebContentsUserData<Vigia> {
 public:
  ~Vigia() override = default;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    if (!id_) {
      return;
    }
    Profile* perfil =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetOriginalProfile();
    // Depois: agora a aba ainda está na lista da janela.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ApagarSeSemAba, perfil->GetWeakPtr(), *id_));
  }

 private:
  friend class content::WebContentsUserData<Vigia>;

  explicit Vigia(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<Vigia>(*contents),
        id_(containers::IdForWebContents(contents)) {}

  // O container não muda: abrir noutro cria outra aba.
  const std::optional<std::string> id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(Vigia);

}  // namespace

void AcompanharAba(content::WebContents* contents) {
  Profile* perfil = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (perfil->IsOffTheRecord()) {
    return;
  }
  static base::NoDestructor<std::set<const void*>> perfis_vistos;
  if (perfis_vistos->insert(perfil).second) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&LimparSobras, perfil->GetWeakPtr()),
        kLimpezaDaPartida);
  }
  Vigia::CreateForWebContents(contents);
}

}  // namespace navegador::temporarios
