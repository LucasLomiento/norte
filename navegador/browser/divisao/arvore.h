// Desenho da divisão de tela com mais de duas abas (até kMaximoDePaineis):
// uma árvore de linhas (painéis lado a lado) e colunas (um em cima do
// outro), com a fração do espaço de cada filho. As folhas são os painéis, na
// ordem das abas dentro da divisão (em profundidade: da esquerda pra direita
// e de cima pra baixo).
//
// Com duas abas o Chromium guarda só o lado (split_tabs::SplitTabLayout) e a
// proporção, e a árvore fica vazia: DeDuas() monta a equivalente. Com três ou
// quatro, a árvore vai como texto no SplitTabVisualData (e na sessão):
// "L" é um painel e "H(5000 L,5000 V(5000 L,5000 L))" é uma linha com um
// painel à esquerda e uma coluna de dois à direita (frações em décimos de
// milésimo, sem vírgula decimal pra não depender da língua).
//
// Só C++ puro: compilado dentro do //components/split_tabs, que o modelo de
// abas (chrome/browser/ui/tabs) e a tela (chrome/browser/ui/views) já usam.

#ifndef NAVEGADOR_BROWSER_DIVISAO_ARVORE_H_
#define NAVEGADOR_BROWSER_DIVISAO_ARVORE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace navegador::divisao {

inline constexpr int kMaximoDePaineis = 4;

struct No {
  enum class Tipo { kPainel, kLinha, kColuna };

  No();
  explicit No(Tipo tipo);
  No(const No&);
  No(No&&);
  No& operator=(const No&);
  No& operator=(No&&);
  ~No();

  bool EPainel() const { return tipo == Tipo::kPainel; }

  Tipo tipo = Tipo::kPainel;
  // Linha ou coluna: dois filhos ou mais e a fração de cada um (somam 1).
  std::vector<No> filhos;
  std::vector<double> fracoes;
  // Só no meio das operações: a posição que o painel tinha antes (-1: novo).
  int origem = -1;
};

// Lado de um painel, pra pôr outro encostado nele.
enum class Lado { kEsquerda, kDireita, kCima, kBaixo };

struct Retangulo {
  int x = 0;
  int y = 0;
  int largura = 0;
  int altura = 0;

  int direita() const { return x + largura; }
  int baixo() const { return y + altura; }
  bool Contem(int px, int py) const {
    return px >= x && px < direita() && py >= y && py < baixo();
  }
};

// A fresta entre dois filhos vizinhos de uma linha ou coluna, que se arrasta
// pra mudar o tamanho dos dois.
struct Divisor {
  Divisor();
  Divisor(const Divisor&);
  Divisor& operator=(const Divisor&);
  ~Divisor();

  Retangulo area;
  // Fresta de uma linha (em pé: arrasta na horizontal)?
  bool em_pe = true;
  // O nó dono: os índices dos filhos, a partir da raiz.
  std::vector<int> caminho;
  // Entre o filho `indice` e o `indice + 1`.
  int indice = 0;
};

struct Desenho {
  Desenho();
  Desenho(const Desenho&);
  Desenho& operator=(const Desenho&);
  ~Desenho();

  // Na ordem das abas.
  std::vector<Retangulo> paineis;
  std::vector<Divisor> divisores;
};

// Texto <-> árvore. Ler() dá nullopt pra texto que não é uma árvore válida
// (com mais de kMaximoDePaineis painéis também).
std::optional<No> Ler(std::string_view texto);
std::string Escrever(const No& raiz);

int Paineis(const No& raiz);

// A árvore de uma divisão de duas (o lado e a proporção do Chromium).
No DeDuas(bool lado_a_lado, double proporcao);
// Se a árvore é de dois painéis, o lado e a proporção equivalentes.
bool EDeDuas(const No& raiz, bool* lado_a_lado, double* proporcao);

// Arranjos prontos com `n` painéis (2 a kMaximoDePaineis).
No Colunas(int n);  // todos lado a lado
No Linhas(int n);   // um em cima do outro
// Grade: com 3, um à esquerda e dois empilhados à direita; com 4, dois em
// cima e dois embaixo.
No Grade(int n);

// Um painel novo do `lado` do painel `alvo`, que cede metade do espaço dele.
// `posicao_nova` recebe a posição do painel novo na ordem das abas.
No Inserir(const No& raiz, int alvo, Lado lado, int* posicao_nova);

// Tira o painel; o espaço dele se divide entre os irmãos, na proporção de
// cada um.
No Remover(const No& raiz, int painel);

// Leva o painel `de` pro `lado` do painel `alvo`. `ordem` recebe, pra cada
// posição nova, a posição antiga do painel (pra reordenar as abas).
No Mover(const No& raiz, int de, int alvo, Lado lado, std::vector<int>* ordem);

// Todos os filhos de cada linha e coluna com o mesmo tamanho.
No Igualar(const No& raiz);
// Só os filhos do nó em `caminho`.
No IgualarNo(const No& raiz, const std::vector<int>& caminho);

// O painel que fica no lugar do `painel` quando ele sai (o irmão mais perto,
// na posição de antes de sair).
int Vizinho(const No& raiz, int painel);

// Retângulos dos painéis e das frestas dentro de `area`, com `espaco` px
// entre os painéis.
Desenho Calcular(const No& raiz, const Retangulo& area, int espaco);

// O divisor `indice` do nó em `caminho` arrastado `delta` px a partir de
// `inicial` (a árvore do começo do arrasto), com a `area` e o `espaco` do
// Calcular(). Nenhum painel fica menor que `minimo` px (nem que metade do
// que os dois filhos somam). Perto de os dois ficarem iguais, gruda.
No Redimensionar(const No& inicial,
                 const Retangulo& area,
                 int espaco,
                 const std::vector<int>& caminho,
                 int indice,
                 int delta,
                 int minimo);

// Painel (posição) embaixo do ponto, ou -1.
int PainelEm(const Desenho& desenho, int x, int y);

}  // namespace navegador::divisao

#endif  // NAVEGADOR_BROWSER_DIVISAO_ARVORE_H_
