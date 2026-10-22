// Interface dos containers: a cor na aba, o submenu "Abrir em container" e
// abrir uma aba num container.

#ifndef NAVEGADOR_BROWSER_UI_CONTAINERS_CONTAINERS_UI_H_
#define NAVEGADOR_BROWSER_UI_CONTAINERS_CONTAINERS_UI_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"

class BrowserWindowInterface;
class GURL;
class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Canvas;
}  // namespace gfx

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace navegador::containers {

// Cor de verdade de um nome da paleta (azul, laranja, ...). As mesmas no
// tema claro e no escuro: são vivas o bastante pros dois.
// Também aceita "#RRGGBB" (cor escolhida no seletor de cor dos espaços).
SkColor Cor(std::string_view nome);
// "#RRGGBB".
std::string Hex(SkColor cor);
// Nome da paleta mais perto da cor (pelo matiz), pra quem só tem a paleta
// (a cor do grupo de abas do Chromium).
std::string NomeMaisProximo(std::string_view cor);

// Bolinha da cor do container, pros menus.
ui::ImageModel Bolinha(std::string_view nome);

// TabView::OnPaint: a barrinha colorida na esquerda da aba de container.
void PintarIndicador(gfx::Canvas* canvas,
                     const views::View& aba,
                     const tabs::TabInterface* tab);

// Abre `url` numa aba nova no container `id` (vazio = sem container) logo
// depois de `index`. Devolve onde ela entrou.
int AbrirEmContainer(TabStripModel* tab_strip,
                     const GURL& url,
                     std::string_view id,
                     int index,
                     bool ativar);

// Troca a aba `index` por uma no container `id` (vazio = sem container), no
// mesmo endereço e no mesmo lugar (ativa e fixada continuam), e fecha a
// original. A janela não fecha: a nova entra antes.
void ReabrirEmContainer(TabStripModel* tab_strip,
                        int index,
                        std::string_view id);

// Menu de contexto da aba: "Abrir em container" com os containers do
// perfil (e "Sem container", se a aba está em um). Reabre a aba no
// escolhido e fecha a original.
std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDaAba(TabStripModel* tab_strip,
                                                       int index);

// Menu de contexto da aba: "Mover para o espaço" com os outros espaços do
// perfil. Quem move é a janela (navegador/browser/ui/espacos.cc, que mora
// no //chrome/browser/ui:ui e não pode ser chamado daqui): ela se registra
// com DefinirMoverAba.
using MoverAba = base::RepeatingCallback<
    void(TabStripModel* tab_strip, int index, const std::string& espaco)>;
void DefinirMoverAba(MoverAba mover);
std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDeEspacos(
    TabStripModel* tab_strip,
    int index);

// Menu de contexto da aba: "Fixar no espaço" (solto, numa pasta do espaço
// ou numa pasta nova; ou "Desafixar", se a aba já é de um site fixado). Quem
// fixa é a área dos fixados (navegador/browser/ui/fixos.cc), registrada com
// DefinirFixarAba; `destino` é o id da pasta, vazio (solto) ou um dos dois
// abaixo.
inline constexpr char kFixarNovaPasta[] = "*nova*";
inline constexpr char kFixarDesafixar[] = "*desafixar*";
using FixarAba = base::RepeatingCallback<
    void(TabStripModel* tab_strip, int index, const std::string& destino)>;
void DefinirFixarAba(FixarAba fixar);
// O mesmo "Fixar no espaço", de fora do menu (o atalho).
void FixarAbaNoEspaco(TabStripModel* tab_strip,
                      int index,
                      const std::string& destino);
std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuFixar(TabStripModel* tab_strip,
                                                       int index);

// "Configurações do Navegador…" no menu do logo (o AppMenuModel também não
// enxerga o //chrome/browser/ui:ui): espacos.cc registra quem abre.
// `containers`: direto na página Containers ("Gerenciar containers…").
using AbrirConfiguracoes =
    base::RepeatingCallback<void(BrowserWindowInterface* browser,
                                 bool containers)>;
void DefinirAbrirConfiguracoes(AbrirConfiguracoes abrir);
void MostrarConfiguracoes(BrowserWindowInterface* browser,
                          bool containers = false);

// Menu de contexto de um link: "Abrir link em container". Abre o link numa
// aba nova (em segundo plano, como "abrir link em nova aba") ao lado da aba
// de origem.
std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDoLink(
    TabStripModel* tab_strip,
    content::WebContents* origem,
    const GURL& link);

// Menu do botão "Nova aba": "Nova aba em container".
std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuNovaAba(
    TabStripModel* tab_strip);

}  // namespace navegador::containers

#endif  // NAVEGADOR_BROWSER_UI_CONTAINERS_CONTAINERS_UI_H_
