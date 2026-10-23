// Divisão de tela com até 4 abas (como no Zen): juntar uma aba à página de
// outra, de qualquer lado; tirar uma aba da divisão sem desfazer as outras;
// trocar de lugar; arranjos prontos. Usado pelo arrastar da lateral pra
// página (MultiContentsView), pelos menus da divisão e pelos atalhos
// (Ctrl+Alt+V/H/G).
//
// O desenho fica numa árvore de linhas e colunas (arvore.h) dentro do
// SplitTabVisualData; o TabStripModel guarda as abas na ordem dos painéis
// (NavegadorRefazerDivisao, NavegadorDesenharDivisao).
//
// Compilado dentro do //chrome/browser/ui.

#ifndef NAVEGADOR_BROWSER_DIVISAO_DIVISAO_H_
#define NAVEGADOR_BROWSER_DIVISAO_DIVISAO_H_

#include <vector>

#include "components/split_tabs/split_tab_id.h"
#include "navegador/browser/divisao/arvore.h"

class TabStripModel;

namespace tabs {
class TabInterface;
}

namespace navegador::divisao {

enum class Arranjo { kLadoALado, kEmpilhadas, kGrade };

// A árvore da divisão em que `aba` está (vazia/painel único se não está).
No ArvoreDaAba(TabStripModel* modelo, tabs::TabInterface* aba);

// Posição da aba entre os painéis da divisão dela, ou -1.
int PosicaoNaDivisao(TabStripModel* modelo, tabs::TabInterface* aba);

// Dá pra pôr `aba` do lado da `alvo` (na divisão dela ou numa nova)? Não dá
// com a divisão cheia, com a aba sendo a última do espaço dela (o espaço
// sumiria) ou com as duas a mesma aba.
bool PodeJuntar(TabStripModel* modelo,
                tabs::TabInterface* aba,
                tabs::TabInterface* alvo);

// Põe `aba` do `lado` da página da `alvo`: cria a divisão, entra na que a
// `alvo` já está ou, se as duas já estão juntas, troca a `aba` de lugar.
// Sai da divisão em que estava antes. Ativa a `aba`.
bool Juntar(TabStripModel* modelo,
            tabs::TabInterface* aba,
            tabs::TabInterface* alvo,
            Lado lado);

// Põe a aba no fim da divisão (à direita ou embaixo, conforme a divisão),
// com todos os painéis do mesmo tamanho. Ativa a aba.
bool Acrescentar(TabStripModel* modelo,
                 tabs::TabInterface* aba,
                 split_tabs::SplitTabId divisao);

// Troca duas abas da mesma divisão de lugar.
bool Trocar(TabStripModel* modelo, tabs::TabInterface* a, tabs::TabInterface* b);

// Tira a aba da divisão; com 3 ou 4, as outras continuam divididas.
void Tirar(TabStripModel* modelo, tabs::TabInterface* aba);

// Arranjo pronto pra divisão.
void Arrumar(TabStripModel* modelo,
             split_tabs::SplitTabId divisao,
             Arranjo arranjo);
// O arranjo que a divisão tem agora, se for um dos prontos.
bool TemArranjo(TabStripModel* modelo,
                split_tabs::SplitTabId divisao,
                Arranjo arranjo);

// Todos os painéis do mesmo tamanho.
void Igualar(TabStripModel* modelo, split_tabs::SplitTabId divisao);

// Divide as `abas` (2 a 4, na ordem) com o arranjo; as que estavam em outras
// divisões saem delas. A ativa continua ativa (ou a primeira, se ela não
// entra).
bool DividirAbas(TabStripModel* modelo,
                 const std::vector<tabs::TabInterface*>& abas,
                 Arranjo arranjo);

}  // namespace navegador::divisao

#endif  // NAVEGADOR_BROWSER_DIVISAO_DIVISAO_H_
