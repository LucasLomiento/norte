// A árvore da divisão (arvore.h) dentro do SplitTabVisualData do Chromium:
// com duas abas ele guarda o lado e a proporção; com três ou quatro, a
// árvore vai como texto e o lado é o da raiz.
//
// Compilado dentro do //components/split_tabs, junto da árvore.

#ifndef NAVEGADOR_BROWSER_DIVISAO_VISUAL_H_
#define NAVEGADOR_BROWSER_DIVISAO_VISUAL_H_

#include <vector>

#include "components/split_tabs/split_tab_visual_data.h"
#include "navegador/browser/divisao/arvore.h"

namespace navegador::divisao {

// A árvore de uma divisão de `paineis` abas. Sem árvore guardada (ou com uma
// que não bate com o número de abas): com duas, a do lado e da proporção; com
// mais, todas lado a lado ou empilhadas, conforme o lado.
No ArvoreDe(const split_tabs::SplitTabVisualData& visual, int paineis);

// O SplitTabVisualData dessa árvore (com dois painéis, sem árvore).
split_tabs::SplitTabVisualData VisualDe(const No& arvore);

// O mesmo desenho, arrumado pra `paineis` abas.
split_tabs::SplitTabVisualData Normalizado(
    const split_tabs::SplitTabVisualData& visual,
    int paineis);

// O desenho sem os painéis nas posições `fora` (de uma divisão de
// `paineis`).
split_tabs::SplitTabVisualData SemPaineis(
    const split_tabs::SplitTabVisualData& visual,
    int paineis,
    std::vector<int> fora);

}  // namespace navegador::divisao

#endif  // NAVEGADOR_BROWSER_DIVISAO_VISUAL_H_
