// Espaços na janela (o modelo de dados fica em
// navegador/browser/containers/espacos_dados.h), como os workspaces do Zen:
// a janela está sempre num espaço e mostra só as abas dele; aba nova nasce
// no espaço (e no container dele); ativar uma aba de outro espaço troca de
// espaço.
//
// Cada espaço, numa janela, é um grupo de abas focado
// (features::kTabGroupsFocusing). A faixa no pé da lateral mostra o espaço
// atual, uma bolinha por espaço e o "+". Troca de espaço: atalhos
// (configuráveis), bolinhas, rodinha na faixa, rolagem horizontal ou
// Ctrl+rodinha em cima da lateral, e os botões laterais do mouse (voltar e
// avançar) com o ponteiro na lateral.
//
// Compilado dentro de //chrome/browser/ui:ui (precisa do
// BrowserWindowInterface, dos comandos e dos grupos; a lateral, que chama
// isto, mora lá).

#ifndef NAVEGADOR_BROWSER_UI_ESPACOS_H_
#define NAVEGADOR_BROWSER_UI_ESPACOS_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "third_party/skia/include/core/SkColor.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class View;
}  // namespace views

namespace navegador::espacos {

// Põe no fim de `lateral` (a VerticalTabStripRegionView, de FlexLayout
// vertical) um espaçador que empurra pra baixo e a faixa de espaços, e liga
// o controle de espaços da janela.
void AdicionarFaixa(views::View* lateral, BrowserWindowInterface* browser);

// Nome do espaço atual pro cabeçalho da lateral (entre o logo e a seta),
// como o nome do perfil no Dia: clicar abre a lista de espaços. Chamar
// depois do AdicionarFaixa.
std::unique_ptr<views::View> CriarTitulo(BrowserWindowInterface* browser);

// Cor do espaço em que a janela está, pra tingir a moldura e a lateral
// (gancho no BrowserWidget::GetColorProviderKey). Nada = sem espaço focado.
std::optional<SkColor> CorDaJanela(BrowserWindowInterface* browser);

// Leva a janela pro espaço (cria a primeira aba dele se ainda não tiver).
void IrPara(BrowserWindowInterface* browser, const std::string& espaco);

// Próximo/anterior, dando a volta.
void Girar(BrowserWindowInterface* browser, bool pra_frente);

// Abre o editor pra criar um espaço novo e leva a janela pra ele.
void NovoEspaco(BrowserWindowInterface* browser);

// Fecha as abas do espaço que a janela mostra, como o "Limpar" do Arc e do
// Zen. Ficam as ligadas a sites fixados; sem abas, o espaço fica com uma
// nova. Ctrl+Shift+T traz de volta.
void FecharAbasDoEspaco(BrowserWindowInterface* browser);

// Roda uma ação na janela: as dos atalhos (Atalhos() em espacos_dados.h,
// ex. "modo_foco") e as dos comandos da barra (comandos.h). Falso se a
// janela não conhece a ação.
bool ExecutarAcao(BrowserWindowInterface* browser, std::string_view acao);

// Move a aba `index` da janela pro espaço (no fim das abas dele).
void MoverAbaPara(BrowserWindowInterface* browser,
                  int index,
                  const std::string& espaco);

// Apaga o espaço: fecha as abas dele em todas as janelas do perfil (as que
// estavam nele vão pro espaço seguinte) e tira da lista.
void Excluir(Profile* profile, const std::string& espaco);

}  // namespace navegador::espacos

#endif  // NAVEGADOR_BROWSER_UI_ESPACOS_H_
