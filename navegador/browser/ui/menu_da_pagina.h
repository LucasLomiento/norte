// Itens do Norte no menu do botão direito das páginas: "Copiar link limpo" e
// "Copiar link como Markdown" (num link) e "Desligar o bloqueador neste
// site" (na página; antes, só pelas informações do site, como "Anúncios
// invasivos"). Os comandos são nossos e quem atende é um observador do menu,
// como o do corretor ortográfico.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_MENU_DA_PAGINA_H_
#define NAVEGADOR_BROWSER_UI_MENU_DA_PAGINA_H_

#include <memory>

class RenderViewContextMenuObserver;
class RenderViewContextMenuProxy;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace navegador::menu_da_pagina {

// Numa faixa sem comandos do Chromium (chrome_command_ids.h, entre os do
// modo tablet e os do preenchimento automático).
inline constexpr int kCopiarLinkLimpo = 52700;
inline constexpr int kCopiarLinkComoMarkdown = 52701;
inline constexpr int kBloqueadorNoSite = 52702;

// O observador do menu de `pagina`: põe os itens no InitMenu e atende os
// comandos.
std::unique_ptr<RenderViewContextMenuObserver> Criar(
    RenderViewContextMenuProxy* menu,
    content::WebContents* pagina);

// Leva os itens pros lugares (eles entram no fim): os do link logo depois do
// "Copiar endereço do link"; o do bloqueador antes do "Ver código-fonte".
void Arrumar(ui::SimpleMenuModel& menu);

}  // namespace navegador::menu_da_pagina

#endif  // NAVEGADOR_BROWSER_UI_MENU_DA_PAGINA_H_
