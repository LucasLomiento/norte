#include "navegador/browser/divisao/divisao.h"

#include <algorithm>
#include <optional>

#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "navegador/browser/divisao/visual.h"

namespace navegador::divisao {

namespace {

std::vector<tabs::TabInterface*> Abas(TabStripModel* modelo,
                                      split_tabs::SplitTabId divisao) {
  return modelo->GetSplitData(divisao)->ListTabs();
}

No Arvore(TabStripModel* modelo, split_tabs::SplitTabId divisao) {
  split_tabs::SplitTabData* dados = modelo->GetSplitData(divisao);
  return ArvoreDe(*dados->visual_data(),
                  static_cast<int>(dados->ListTabs().size()));
}

int Posicao(const std::vector<tabs::TabInterface*>& abas,
            const tabs::TabInterface* aba) {
  const auto it = std::ranges::find(abas, aba);
  return it == abas.end() ? -1 : static_cast<int>(it - abas.begin());
}

// A aba sair do espaço (grupo) dela pro da `alvo` deixaria o espaço vazio
// (e o Chromium apaga grupo vazio).
bool EsvaziaOEspaco(TabStripModel* modelo,
                    const tabs::TabInterface* aba,
                    const tabs::TabInterface* alvo) {
  const std::optional<tab_groups::TabGroupId> grupo = aba->GetGroup();
  if (!grupo || grupo == alvo->GetGroup() || !modelo->group_model()) {
    return false;
  }
  const TabGroup* tab_group = modelo->group_model()->GetTabGroup(*grupo);
  return tab_group && tab_group->tab_count() <= 1;
}

No DoArranjo(Arranjo arranjo, int paineis) {
  switch (arranjo) {
    case Arranjo::kLadoALado:
      return Colunas(paineis);
    case Arranjo::kEmpilhadas:
      return Linhas(paineis);
    case Arranjo::kGrade:
      return Grade(paineis);
  }
  return Colunas(paineis);
}

void Ativar(TabStripModel* modelo, tabs::TabInterface* aba) {
  const int indice = modelo->GetIndexOfTab(aba);
  if (indice != TabStripModel::kNoTab) {
    modelo->ActivateTabAt(indice);
  }
}

// Tira a aba da divisão. Devolve a vizinha dela na divisão (a que ocupa o
// lugar), ou nada.
tabs::TabInterface* TirarDaDivisao(TabStripModel* modelo,
                                   tabs::TabInterface* aba) {
  if (!aba->IsSplit()) {
    return nullptr;
  }
  const split_tabs::SplitTabId divisao = *aba->GetSplit();
  std::vector<tabs::TabInterface*> abas = Abas(modelo, divisao);
  const int posicao = Posicao(abas, aba);
  const No arvore = Arvore(modelo, divisao);
  const int vizinha = Vizinho(arvore, posicao);
  tabs::TabInterface* const quem_fica =
      vizinha >= 0 && vizinha < static_cast<int>(abas.size()) &&
              vizinha != posicao
          ? abas[vizinha]
          : nullptr;
  if (abas.size() <= 2u) {
    modelo->RemoveSplit(divisao);
    return quem_fica;
  }
  abas.erase(abas.begin() + posicao);
  modelo->NavegadorRefazerDivisao(divisao, abas,
                                  VisualDe(Remover(arvore, posicao)));
  return quem_fica;
}

}  // namespace

No ArvoreDaAba(TabStripModel* modelo, tabs::TabInterface* aba) {
  if (!modelo || !aba || !aba->IsSplit()) {
    return No();
  }
  return Arvore(modelo, *aba->GetSplit());
}

int PosicaoNaDivisao(TabStripModel* modelo, tabs::TabInterface* aba) {
  if (!modelo || !aba || !aba->IsSplit()) {
    return -1;
  }
  return Posicao(Abas(modelo, *aba->GetSplit()), aba);
}

bool PodeJuntar(TabStripModel* modelo,
                tabs::TabInterface* aba,
                tabs::TabInterface* alvo) {
  if (!modelo || !aba || !alvo || aba == alvo ||
      modelo->GetIndexOfTab(aba) == TabStripModel::kNoTab ||
      modelo->GetIndexOfTab(alvo) == TabStripModel::kNoTab ||
      EsvaziaOEspaco(modelo, aba, alvo)) {
    return false;
  }
  if (!alvo->IsSplit() || aba->GetSplit() == alvo->GetSplit()) {
    return true;
  }
  return Abas(modelo, *alvo->GetSplit()).size() <
         static_cast<size_t>(kMaximoDePaineis);
}

bool Juntar(TabStripModel* modelo,
            tabs::TabInterface* aba,
            tabs::TabInterface* alvo,
            Lado lado) {
  if (!PodeJuntar(modelo, aba, alvo)) {
    return false;
  }

  if (alvo->IsSplit()) {
    const split_tabs::SplitTabId divisao = *alvo->GetSplit();
    if (aba->GetSplit() == divisao) {
      // As duas já estão juntas: a aba muda de lugar.
      const std::vector<tabs::TabInterface*> abas = Abas(modelo, divisao);
      std::vector<int> ordem;
      const No nova = Mover(Arvore(modelo, divisao), Posicao(abas, aba),
                            Posicao(abas, alvo), lado, &ordem);
      std::vector<tabs::TabInterface*> novas;
      for (int antiga : ordem) {
        novas.push_back(abas[antiga]);
      }
      modelo->NavegadorRefazerDivisao(divisao, novas, VisualDe(nova));
    } else {
      if (aba->IsSplit()) {
        TirarDaDivisao(modelo, aba);
      }
      if (!modelo->ContainsSplit(divisao)) {
        return false;
      }
      std::vector<tabs::TabInterface*> abas = Abas(modelo, divisao);
      int posicao_nova = 0;
      const No nova = Inserir(Arvore(modelo, divisao), Posicao(abas, alvo),
                              lado, &posicao_nova);
      abas.insert(abas.begin() + posicao_nova, aba);
      modelo->NavegadorRefazerDivisao(divisao, abas, VisualDe(nova));
    }
  } else {
    if (aba->IsSplit()) {
      TirarDaDivisao(modelo, aba);
    }
    const bool antes = lado == Lado::kEsquerda || lado == Lado::kCima;
    const bool lado_a_lado = lado == Lado::kEsquerda || lado == Lado::kDireita;
    // A alvo na frente: a divisão fica no grupo (espaço) e na fixação dela.
    const split_tabs::SplitTabId divisao = modelo->AddToNewSplit(
        {modelo->GetIndexOfTab(alvo), modelo->GetIndexOfTab(aba)},
        split_tabs::SplitTabVisualData(
            lado_a_lado ? split_tabs::SplitTabLayout::kSideBySide
                        : split_tabs::SplitTabLayout::kStacked,
            0.5),
        split_tabs::SplitTabCreatedSource::kDragAndDropTab);
    if ((Abas(modelo, divisao)[0] == aba) != antes) {
      modelo->ReverseTabsInSplit(divisao);
    }
  }
  Ativar(modelo, aba);
  return true;
}

bool Acrescentar(TabStripModel* modelo,
                 tabs::TabInterface* aba,
                 split_tabs::SplitTabId divisao) {
  if (!modelo || !aba || !modelo->ContainsSplit(divisao) ||
      aba->GetSplit() == divisao) {
    return false;
  }
  std::vector<tabs::TabInterface*> abas = Abas(modelo, divisao);
  if (abas.size() >= static_cast<size_t>(kMaximoDePaineis) ||
      !PodeJuntar(modelo, aba, abas.back())) {
    return false;
  }
  if (aba->IsSplit()) {
    TirarDaDivisao(modelo, aba);
    if (!modelo->ContainsSplit(divisao)) {
      return false;
    }
    abas = Abas(modelo, divisao);
  }
  const No arvore = Arvore(modelo, divisao);
  int posicao_nova = 0;
  const No nova = navegador::divisao::Igualar(Inserir(
      arvore, static_cast<int>(abas.size()) - 1,
      arvore.tipo == No::Tipo::kColuna ? Lado::kBaixo : Lado::kDireita,
      &posicao_nova));
  abas.insert(abas.begin() + posicao_nova, aba);
  modelo->NavegadorRefazerDivisao(divisao, abas, VisualDe(nova));
  Ativar(modelo, aba);
  return true;
}

bool Trocar(TabStripModel* modelo,
            tabs::TabInterface* a,
            tabs::TabInterface* b) {
  if (!modelo || !a || !b || a == b || !a->IsSplit() ||
      a->GetSplit() != b->GetSplit()) {
    return false;
  }
  const split_tabs::SplitTabId divisao = *a->GetSplit();
  std::vector<tabs::TabInterface*> abas = Abas(modelo, divisao);
  std::swap(abas[Posicao(abas, a)], abas[Posicao(abas, b)]);
  modelo->NavegadorRefazerDivisao(divisao, abas,
                                  *modelo->GetSplitData(divisao)->visual_data());
  return true;
}

void Tirar(TabStripModel* modelo, tabs::TabInterface* aba) {
  if (!modelo || !aba || !aba->IsSplit()) {
    return;
  }
  const bool ativa = aba->IsActivated();
  tabs::TabInterface* const quem_fica = TirarDaDivisao(modelo, aba);
  // A que saiu some da tela; as outras continuam à vista.
  if (ativa && quem_fica) {
    Ativar(modelo, quem_fica);
  }
}

void Arrumar(TabStripModel* modelo,
             split_tabs::SplitTabId divisao,
             Arranjo arranjo) {
  if (!modelo || !modelo->ContainsSplit(divisao)) {
    return;
  }
  const int paineis = static_cast<int>(Abas(modelo, divisao).size());
  modelo->NavegadorDesenharDivisao(divisao,
                                   VisualDe(DoArranjo(arranjo, paineis)));
}

bool TemArranjo(TabStripModel* modelo,
                split_tabs::SplitTabId divisao,
                Arranjo arranjo) {
  if (!modelo || !modelo->ContainsSplit(divisao)) {
    return false;
  }
  const int paineis = static_cast<int>(Abas(modelo, divisao).size());
  return Escrever(Igualar(Arvore(modelo, divisao))) ==
         Escrever(DoArranjo(arranjo, paineis));
}

void Igualar(TabStripModel* modelo, split_tabs::SplitTabId divisao) {
  if (!modelo || !modelo->ContainsSplit(divisao)) {
    return;
  }
  modelo->NavegadorDesenharDivisao(
      divisao, VisualDe(navegador::divisao::Igualar(Arvore(modelo, divisao))));
}

bool DividirAbas(TabStripModel* modelo,
                 const std::vector<tabs::TabInterface*>& abas,
                 Arranjo arranjo) {
  const int n = static_cast<int>(abas.size());
  if (!modelo || n < 2 || n > kMaximoDePaineis) {
    return false;
  }
  // Fica ativa a que já estava, se ela entra; senão, a primeira.
  tabs::TabInterface* const ativa = std::ranges::contains(
                                        abas, modelo->GetActiveTab())
                                        ? modelo->GetActiveTab()
                                        : abas[0];
  for (tabs::TabInterface* aba : abas) {
    if (!aba || modelo->GetIndexOfTab(aba) == TabStripModel::kNoTab ||
        (aba != abas[0] && EsvaziaOEspaco(modelo, aba, abas[0]))) {
      return false;
    }
  }
  // Já são exatamente estas, na mesma divisão: só o arranjo e a ordem.
  if (abas[0]->IsSplit() &&
      Abas(modelo, *abas[0]->GetSplit()).size() == abas.size() &&
      std::ranges::all_of(abas, [&](tabs::TabInterface* aba) {
        return aba->GetSplit() == abas[0]->GetSplit();
      })) {
    modelo->NavegadorRefazerDivisao(*abas[0]->GetSplit(), abas,
                                    VisualDe(DoArranjo(arranjo, n)));
    Ativar(modelo, ativa);
    return true;
  }
  for (tabs::TabInterface* aba : abas) {
    if (aba->IsSplit()) {
      modelo->RemoveSplit(*aba->GetSplit());
    }
  }
  std::vector<int> indices;
  for (tabs::TabInterface* aba : abas) {
    indices.push_back(modelo->GetIndexOfTab(aba));
  }
  const split_tabs::SplitTabId divisao = modelo->AddToNewSplit(
      indices, VisualDe(DoArranjo(arranjo, n)),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);
  // O AddToNewSplit põe na ordem da lateral; a pedida é a de `abas`.
  modelo->NavegadorRefazerDivisao(divisao, abas,
                                  VisualDe(DoArranjo(arranjo, n)));
  Ativar(modelo, ativa);
  return true;
}

}  // namespace navegador::divisao
