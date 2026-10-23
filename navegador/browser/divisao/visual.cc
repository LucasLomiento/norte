#include "navegador/browser/divisao/visual.h"

#include <algorithm>
#include <functional>
#include <optional>

namespace navegador::divisao {

No ArvoreDe(const split_tabs::SplitTabVisualData& visual, int paineis) {
  const bool lado_a_lado =
      visual.split_layout() == split_tabs::SplitTabLayout::kSideBySide;
  if (paineis > 2 && !visual.navegador_arvore().empty()) {
    if (std::optional<No> arvore = Ler(visual.navegador_arvore());
        arvore && Paineis(*arvore) == paineis) {
      return *std::move(arvore);
    }
  }
  if (paineis <= 2) {
    return DeDuas(lado_a_lado, visual.split_ratio());
  }
  return lado_a_lado ? Colunas(paineis) : Linhas(paineis);
}

split_tabs::SplitTabVisualData VisualDe(const No& arvore) {
  bool lado_a_lado = true;
  double proporcao = 0.5;
  if (Paineis(arvore) <= 2) {
    if (!EDeDuas(arvore, &lado_a_lado, &proporcao)) {
      lado_a_lado = arvore.tipo != No::Tipo::kColuna;
      proporcao = 0.5;
    }
    return split_tabs::SplitTabVisualData(
        lado_a_lado ? split_tabs::SplitTabLayout::kSideBySide
                    : split_tabs::SplitTabLayout::kStacked,
        proporcao);
  }
  split_tabs::SplitTabVisualData visual(
      arvore.tipo == No::Tipo::kColuna ? split_tabs::SplitTabLayout::kStacked
                                       : split_tabs::SplitTabLayout::kSideBySide,
      0.5);
  visual.set_navegador_arvore(Escrever(arvore));
  return visual;
}

split_tabs::SplitTabVisualData Normalizado(
    const split_tabs::SplitTabVisualData& visual,
    int paineis) {
  return VisualDe(ArvoreDe(visual, paineis));
}

split_tabs::SplitTabVisualData SemPaineis(
    const split_tabs::SplitTabVisualData& visual,
    int paineis,
    std::vector<int> fora) {
  No arvore = ArvoreDe(visual, paineis);
  // De trás pra frente: tirar um painel muda a posição dos que vêm depois.
  std::sort(fora.begin(), fora.end(), std::greater<int>());
  fora.erase(std::unique(fora.begin(), fora.end()), fora.end());
  for (int painel : fora) {
    if (painel >= 0 && painel < Paineis(arvore)) {
      arvore = Remover(arvore, painel);
    }
  }
  return VisualDe(arvore);
}

}  // namespace navegador::divisao
