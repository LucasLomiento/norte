#include "navegador/browser/ui/menu_da_pagina.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/ui/bloqueador.h"
#include "navegador/browser/ui/links.h"
#include "navegador/icones/menu/vector_icons.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace navegador::menu_da_pagina {

namespace {

ui::ImageModel Icone(const gfx::VectorIcon& icone) {
  return ui::ImageModel::FromVectorIcon(icone, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

class ItensDoNorte : public RenderViewContextMenuObserver {
 public:
  ItensDoNorte(RenderViewContextMenuProxy* menu, content::WebContents* pagina)
      : menu_(menu), pagina_(pagina->GetWeakPtr()) {}

  // RenderViewContextMenuObserver:
  void InitMenu(const content::ContextMenuParams& params) override {
    link_ = params.link_url;
    texto_do_link_ = params.link_text;
    if (link_.is_valid() && link_.SchemeIsHTTPOrHTTPS()) {
      menu_->AddMenuItemWithIcon(kCopiarLinkLimpo, u"Copiar link limpo",
                                 Icone(icones_menu::kCopyIcon));
      menu_->AddMenuItemWithIcon(kCopiarLinkComoMarkdown,
                                 u"Copiar link como Markdown",
                                 Icone(icones_menu::kLinkIcon));
    }
    // Na página mesmo (não num link, numa seleção, numa imagem ou num
    // campo), com o bloqueador ligado no perfil.
    site_ = params.page_url;
    Profile* perfil = Perfil();
    if (params.link_url.is_empty() && params.selection_text.empty() &&
        params.media_type == blink::mojom::ContextMenuDataMediaType::kNone &&
        !params.is_editable && site_.SchemeIsHTTPOrHTTPS() && perfil &&
        bloqueador::Ligado(perfil)) {
      const bool desligado = DesligadoNoSite();
      menu_->AddMenuItemWithIcon(
          kBloqueadorNoSite,
          desligado ? u"Ligar o bloqueador neste site"
                    : u"Desligar o bloqueador neste site",
          Icone(desligado ? icones_menu::kShieldCheckIcon
                          : icones_menu::kShieldOffIcon));
    }
  }

  bool IsCommandIdSupported(int command_id) override {
    return command_id == kCopiarLinkLimpo ||
           command_id == kCopiarLinkComoMarkdown ||
           command_id == kBloqueadorNoSite;
  }

  bool IsCommandIdEnabled(int command_id) override { return !!pagina_; }

  void ExecuteCommand(int command_id) override {
    if (!pagina_) {
      return;
    }
    switch (command_id) {
      case kCopiarLinkLimpo:
        links::Copiar(Janela(), base::UTF8ToUTF16(links::Limpar(link_).spec()));
        break;
      case kCopiarLinkComoMarkdown:
        links::Copiar(Janela(), links::ComoMarkdown(texto_do_link_,
                                                    links::Limpar(link_)));
        break;
      case kBloqueadorNoSite:
        TrocarBloqueadorNoSite();
        break;
    }
  }

 private:
  Profile* Perfil() const {
    return pagina_ ? Profile::FromBrowserContext(pagina_->GetBrowserContext())
                   : nullptr;
  }

  BrowserWindowInterface* Janela() const {
    tabs::TabInterface* aba =
        pagina_ ? tabs::TabInterface::MaybeGetFromContents(pagina_.get())
                : nullptr;
    return aba ? aba->GetBrowserWindowInterface() : nullptr;
  }

  // O bloqueador é o filtro de anúncios do Chromium ligado em todo site
  // (ver bloqueador.h): desligado num site = a exceção "permitir anúncios".
  bool DesligadoNoSite() const {
    Profile* perfil = Perfil();
    return perfil && HostContentSettingsMapFactory::GetForProfile(perfil)
                             ->GetContentSetting(site_, site_,
                                                 ContentSettingsType::ADS) ==
                         CONTENT_SETTING_ALLOW;
  }

  void TrocarBloqueadorNoSite() {
    Profile* perfil = Perfil();
    if (!perfil) {
      return;
    }
    HostContentSettingsMapFactory::GetForProfile(perfil)
        ->SetContentSettingDefaultScope(site_, GURL(), ContentSettingsType::ADS,
                                        DesligadoNoSite()
                                            ? CONTENT_SETTING_DEFAULT
                                            : CONTENT_SETTING_ALLOW);
    // As regras valem a partir do carregamento da página.
    pagina_->GetController().Reload(content::ReloadType::NORMAL,
                                    /*check_for_repost=*/true);
  }

  raw_ptr<RenderViewContextMenuProxy> menu_;
  base::WeakPtr<content::WebContents> pagina_;
  GURL link_;
  std::u16string texto_do_link_;
  GURL site_;
};

// Tira o item `id` de onde ele está e põe logo depois de `antes` (ou antes
// de `depois`), com o rótulo e o ícone.
void Mover(ui::SimpleMenuModel& menu,
           int id,
           std::optional<int> antes,
           std::optional<int> depois) {
  std::optional<size_t> de = menu.GetIndexOfCommandId(id);
  if (!de) {
    return;
  }
  const std::u16string rotulo = menu.GetLabelAt(*de);
  const ui::ImageModel icone = menu.GetIconAt(*de);
  std::optional<size_t> alvo;
  if (antes) {
    if (std::optional<size_t> i = menu.GetIndexOfCommandId(*antes)) {
      alvo = *i + 1;
    }
  } else if (depois) {
    alvo = menu.GetIndexOfCommandId(*depois);
  }
  if (!alvo) {
    return;
  }
  menu.RemoveItemAt(*de);
  if (*alvo > *de) {
    --*alvo;
  }
  menu.InsertItemAt(*alvo, id, rotulo);
  menu.SetIcon(*alvo, icone);
}

}  // namespace

std::unique_ptr<RenderViewContextMenuObserver> Criar(
    RenderViewContextMenuProxy* menu,
    content::WebContents* pagina) {
  return std::make_unique<ItensDoNorte>(menu, pagina);
}

void Arrumar(ui::SimpleMenuModel& menu) {
  Mover(menu, kCopiarLinkComoMarkdown, IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
        std::nullopt);
  Mover(menu, kCopiarLinkLimpo, IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
        std::nullopt);
  Mover(menu, kBloqueadorNoSite, std::nullopt, IDC_VIEW_SOURCE);
  // Sem o "Ver código-fonte" (páginas sem ele), fica no fim, onde entrou.
}

}  // namespace navegador::menu_da_pagina
