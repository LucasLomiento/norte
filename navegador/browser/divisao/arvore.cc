#include "navegador/browser/divisao/arvore.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace navegador::divisao {

namespace {

// Quanto perto de "os dois do mesmo tamanho" o divisor gruda, em px (o
// Chromium usa 15 com duas abas).
constexpr int kGrude = 15;
// Mais fundo que isto o texto não é uma árvore nossa (com 4 painéis a
// árvore tem no máximo 3 níveis).
constexpr int kProfundidadeMaxima = 8;

No::Tipo EixoDoLado(Lado lado) {
  return lado == Lado::kEsquerda || lado == Lado::kDireita ? No::Tipo::kLinha
                                                           : No::Tipo::kColuna;
}

bool LadoAntes(Lado lado) {
  return lado == Lado::kEsquerda || lado == Lado::kCima;
}

// Dá a cada folha a posição dela (em profundidade) como origem.
void Numerar(No& no, int* contador) {
  if (no.EPainel()) {
    no.origem = (*contador)++;
    return;
  }
  for (No& filho : no.filhos) {
    Numerar(filho, contador);
  }
}

void Numerar(No& raiz) {
  int contador = 0;
  Numerar(raiz, &contador);
}

void Limpar(No& no) {
  no.origem = -1;
  for (No& filho : no.filhos) {
    Limpar(filho);
  }
}

void Origens(const No& no, std::vector<int>* origens) {
  if (no.EPainel()) {
    origens->push_back(no.origem);
    return;
  }
  for (const No& filho : no.filhos) {
    Origens(filho, origens);
  }
}

bool Achar(const No& no, int origem, std::vector<int>* caminho) {
  if (no.EPainel()) {
    return no.origem == origem;
  }
  for (size_t k = 0; k < no.filhos.size(); ++k) {
    caminho->push_back(static_cast<int>(k));
    if (Achar(no.filhos[k], origem, caminho)) {
      return true;
    }
    caminho->pop_back();
  }
  return false;
}

No* NoEm(No& raiz, const std::vector<int>& caminho) {
  No* no = &raiz;
  for (int k : caminho) {
    if (no->EPainel() || k < 0 || k >= static_cast<int>(no->filhos.size())) {
      return nullptr;
    }
    no = &no->filhos[k];
  }
  return no;
}

void FracoesIguais(No& no) {
  no.fracoes.assign(no.filhos.size(), 1.0 / no.filhos.size());
}

// Frações somando 1 (as negativas ou faltando viram iguais).
void AjustarFracoes(No& no) {
  if (no.fracoes.size() != no.filhos.size()) {
    FracoesIguais(no);
    return;
  }
  double soma = 0;
  for (double& f : no.fracoes) {
    if (!(f > 0)) {
      f = 0;
    }
    soma += f;
  }
  if (!(soma > 0)) {
    FracoesIguais(no);
    return;
  }
  for (double& f : no.fracoes) {
    f /= soma;
  }
}

// Linha dentro de linha vira uma linha só; nó de um filho vira o filho.
void Normalizar(No& no) {
  if (no.EPainel()) {
    no.filhos.clear();
    no.fracoes.clear();
    return;
  }
  AjustarFracoes(no);
  for (No& filho : no.filhos) {
    Normalizar(filho);
  }
  std::vector<No> filhos;
  std::vector<double> fracoes;
  for (size_t k = 0; k < no.filhos.size(); ++k) {
    No& filho = no.filhos[k];
    if (!filho.EPainel() && filho.tipo == no.tipo) {
      for (size_t j = 0; j < filho.filhos.size(); ++j) {
        filhos.push_back(std::move(filho.filhos[j]));
        fracoes.push_back(no.fracoes[k] * filho.fracoes[j]);
      }
    } else {
      filhos.push_back(std::move(filho));
      fracoes.push_back(no.fracoes[k]);
    }
  }
  no.filhos = std::move(filhos);
  no.fracoes = std::move(fracoes);
  AjustarFracoes(no);
  if (no.filhos.size() == 1) {
    No unico = std::move(no.filhos[0]);
    no = std::move(unico);
  } else if (no.filhos.empty()) {
    no = No();
  }
}

void PrimeiraOuUltima(const No& no, bool ultima, int* origem) {
  if (no.EPainel()) {
    *origem = no.origem;
    return;
  }
  PrimeiraOuUltima(ultima ? no.filhos.back() : no.filhos.front(), ultima,
                   origem);
}

// Tira a folha `origem`; o espaço dela vai pros irmãos.
void RemoverPorOrigem(No& raiz, int origem) {
  std::vector<int> caminho;
  if (!Achar(raiz, origem, &caminho) || caminho.empty()) {
    return;
  }
  const int i = caminho.back();
  caminho.pop_back();
  No* pai = NoEm(raiz, caminho);
  pai->filhos.erase(pai->filhos.begin() + i);
  pai->fracoes.erase(pai->fracoes.begin() + i);
  AjustarFracoes(*pai);
  Normalizar(raiz);
}

// Folha nova (com `origem_nova`) do `lado` da folha `origem_alvo`.
void InserirPorOrigem(No& raiz, int origem_alvo, Lado lado, int origem_nova) {
  std::vector<int> caminho;
  if (!Achar(raiz, origem_alvo, &caminho)) {
    return;
  }
  const No::Tipo eixo = EixoDoLado(lado);
  const bool antes = LadoAntes(lado);
  No novo;
  novo.origem = origem_nova;
  if (!caminho.empty()) {
    const int i = caminho.back();
    std::vector<int> caminho_do_pai(caminho.begin(), caminho.end() - 1);
    No* pai = NoEm(raiz, caminho_do_pai);
    if (pai->tipo == eixo) {
      const double metade = pai->fracoes[i] / 2;
      pai->fracoes[i] = metade;
      const int em = antes ? i : i + 1;
      pai->filhos.insert(pai->filhos.begin() + em, std::move(novo));
      pai->fracoes.insert(pai->fracoes.begin() + em, metade);
      return;
    }
  }
  No* folha = NoEm(raiz, caminho);
  No velho = std::move(*folha);
  No grupo(eixo);
  if (antes) {
    grupo.filhos.push_back(std::move(novo));
    grupo.filhos.push_back(std::move(velho));
  } else {
    grupo.filhos.push_back(std::move(velho));
    grupo.filhos.push_back(std::move(novo));
  }
  grupo.fracoes = {0.5, 0.5};
  *folha = std::move(grupo);
}

// Tamanho de cada filho (no eixo do nó) num espaço de `disponivel` px: a
// soma dá certinho `disponivel`.
std::vector<int> Tamanhos(const No& no, int disponivel) {
  std::vector<int> tamanhos;
  double acumulado = 0;
  int inicio = 0;
  for (size_t k = 0; k < no.filhos.size(); ++k) {
    acumulado += no.fracoes[k];
    const int fim = k + 1 == no.filhos.size()
                        ? disponivel
                        : static_cast<int>(std::lround(acumulado * disponivel));
    tamanhos.push_back(std::max(0, fim - inicio));
    inicio = std::max(inicio, fim);
  }
  return tamanhos;
}

Retangulo Filho(const Retangulo& area,
                bool linha,
                int posicao,
                int tamanho) {
  return linha ? Retangulo{posicao, area.y, tamanho, area.altura}
               : Retangulo{area.x, posicao, area.largura, tamanho};
}

void CalcularNo(const No& no,
                const Retangulo& area,
                int espaco,
                std::vector<int>* caminho,
                int* contador,
                Desenho* desenho) {
  if (no.EPainel()) {
    if (*contador < static_cast<int>(desenho->paineis.size())) {
      desenho->paineis[*contador] = area;
    }
    ++*contador;
    return;
  }
  const bool linha = no.tipo == No::Tipo::kLinha;
  const int n = static_cast<int>(no.filhos.size());
  const int total = linha ? area.largura : area.altura;
  const int disponivel = std::max(0, total - espaco * (n - 1));
  const std::vector<int> tamanhos = Tamanhos(no, disponivel);
  int posicao = linha ? area.x : area.y;
  for (int k = 0; k < n; ++k) {
    caminho->push_back(k);
    CalcularNo(no.filhos[k], Filho(area, linha, posicao, tamanhos[k]), espaco,
               caminho, contador, desenho);
    caminho->pop_back();
    posicao += tamanhos[k];
    if (k + 1 < n) {
      Divisor divisor;
      divisor.area = Filho(area, linha, posicao, espaco);
      divisor.em_pe = linha;
      divisor.caminho = *caminho;
      divisor.indice = k;
      desenho->divisores.push_back(std::move(divisor));
      posicao += espaco;
    }
  }
}

// Retângulo do nó em `caminho`.
Retangulo AreaDoNo(const No& raiz,
                   const Retangulo& area,
                   int espaco,
                   const std::vector<int>& caminho) {
  const No* no = &raiz;
  Retangulo atual = area;
  for (int k : caminho) {
    if (no->EPainel() || k < 0 || k >= static_cast<int>(no->filhos.size())) {
      break;
    }
    const bool linha = no->tipo == No::Tipo::kLinha;
    const int n = static_cast<int>(no->filhos.size());
    const int total = linha ? atual.largura : atual.altura;
    const int disponivel = std::max(0, total - espaco * (n - 1));
    const std::vector<int> tamanhos = Tamanhos(*no, disponivel);
    int posicao = linha ? atual.x : atual.y;
    for (int j = 0; j < k; ++j) {
      posicao += tamanhos[j] + espaco;
    }
    atual = Filho(atual, linha, posicao, tamanhos[k]);
    no = &no->filhos[k];
  }
  return atual;
}

// O menor tamanho que o nó aguenta no eixo `linha` (largura) ou na altura.
int Minimo(const No& no, bool linha, int minimo, int espaco) {
  if (no.EPainel()) {
    return minimo;
  }
  const bool mesmo_eixo = (no.tipo == No::Tipo::kLinha) == linha;
  int resultado = 0;
  for (const No& filho : no.filhos) {
    const int m = Minimo(filho, linha, minimo, espaco);
    resultado = mesmo_eixo ? resultado + m : std::max(resultado, m);
  }
  if (mesmo_eixo) {
    resultado += espaco * (static_cast<int>(no.filhos.size()) - 1);
  }
  return resultado;
}

struct Leitor {
  std::string_view texto;
  size_t i = 0;
  bool ok = true;

  bool Pegar(char c) {
    if (i < texto.size() && texto[i] == c) {
      ++i;
      return true;
    }
    return false;
  }

  bool Numero(int* valor) {
    const size_t inicio = i;
    int v = 0;
    while (i < texto.size() && texto[i] >= '0' && texto[i] <= '9' &&
           i - inicio < 6) {
      v = v * 10 + (texto[i] - '0');
      ++i;
    }
    *valor = v;
    return i > inicio;
  }

  No LerNo(int profundidade) {
    No no;
    if (!ok || profundidade > kProfundidadeMaxima) {
      ok = false;
      return no;
    }
    if (Pegar('L')) {
      return no;
    }
    if (Pegar('H')) {
      no.tipo = No::Tipo::kLinha;
    } else if (Pegar('V')) {
      no.tipo = No::Tipo::kColuna;
    } else {
      ok = false;
      return no;
    }
    if (!Pegar('(')) {
      ok = false;
      return no;
    }
    while (ok) {
      int fracao = 0;
      if (!Numero(&fracao) || !Pegar(' ')) {
        ok = false;
        break;
      }
      no.filhos.push_back(LerNo(profundidade + 1));
      no.fracoes.push_back(fracao / 10000.0);
      if (no.filhos.size() > kMaximoDePaineis) {
        ok = false;
        break;
      }
      if (Pegar(',')) {
        continue;
      }
      if (!Pegar(')')) {
        ok = false;
      }
      break;
    }
    if (no.filhos.size() < 2) {
      ok = false;
    }
    return no;
  }
};

void Escrever(const No& no, std::string* texto) {
  if (no.EPainel()) {
    *texto += 'L';
    return;
  }
  *texto += no.tipo == No::Tipo::kLinha ? "H(" : "V(";
  for (size_t k = 0; k < no.filhos.size(); ++k) {
    if (k) {
      *texto += ',';
    }
    const int fracao = static_cast<int>(std::lround(no.fracoes[k] * 10000));
    *texto += std::to_string(std::clamp(fracao, 0, 10000));
    *texto += ' ';
    Escrever(no.filhos[k], texto);
  }
  *texto += ')';
}

No Iguais(No::Tipo tipo, int n) {
  No grupo(tipo);
  for (int k = 0; k < n; ++k) {
    grupo.filhos.emplace_back();
  }
  FracoesIguais(grupo);
  return grupo;
}

void IgualarTudo(No& no) {
  if (no.EPainel()) {
    return;
  }
  FracoesIguais(no);
  for (No& filho : no.filhos) {
    IgualarTudo(filho);
  }
}

}  // namespace

No::No() = default;
No::No(Tipo tipo) : tipo(tipo) {}
No::No(const No&) = default;
No::No(No&&) = default;
No& No::operator=(const No&) = default;
No& No::operator=(No&&) = default;
No::~No() = default;

Divisor::Divisor() = default;
Divisor::Divisor(const Divisor&) = default;
Divisor& Divisor::operator=(const Divisor&) = default;
Divisor::~Divisor() = default;

Desenho::Desenho() = default;
Desenho::Desenho(const Desenho&) = default;
Desenho& Desenho::operator=(const Desenho&) = default;
Desenho::~Desenho() = default;

std::optional<No> Ler(std::string_view texto) {
  Leitor leitor{texto};
  No raiz = leitor.LerNo(0);
  if (!leitor.ok || leitor.i != texto.size()) {
    return std::nullopt;
  }
  Normalizar(raiz);
  const int n = Paineis(raiz);
  if (n < 1 || n > kMaximoDePaineis) {
    return std::nullopt;
  }
  return raiz;
}

std::string Escrever(const No& raiz) {
  std::string texto;
  Escrever(raiz, &texto);
  return texto;
}

int Paineis(const No& raiz) {
  if (raiz.EPainel()) {
    return 1;
  }
  int n = 0;
  for (const No& filho : raiz.filhos) {
    n += Paineis(filho);
  }
  return n;
}

No DeDuas(bool lado_a_lado, double proporcao) {
  No grupo(lado_a_lado ? No::Tipo::kLinha : No::Tipo::kColuna);
  grupo.filhos.emplace_back();
  grupo.filhos.emplace_back();
  const double p = std::clamp(proporcao, 0.05, 0.95);
  grupo.fracoes = {p, 1 - p};
  return grupo;
}

bool EDeDuas(const No& raiz, bool* lado_a_lado, double* proporcao) {
  if (raiz.EPainel() || raiz.filhos.size() != 2 ||
      !raiz.filhos[0].EPainel() || !raiz.filhos[1].EPainel()) {
    return false;
  }
  *lado_a_lado = raiz.tipo == No::Tipo::kLinha;
  *proporcao = raiz.fracoes[0];
  return true;
}

No Colunas(int n) {
  return Iguais(No::Tipo::kLinha, std::clamp(n, 2, kMaximoDePaineis));
}

No Linhas(int n) {
  return Iguais(No::Tipo::kColuna, std::clamp(n, 2, kMaximoDePaineis));
}

No Grade(int n) {
  n = std::clamp(n, 2, kMaximoDePaineis);
  if (n == 2) {
    return Colunas(2);
  }
  if (n == 3) {
    No linha(No::Tipo::kLinha);
    linha.filhos.emplace_back();
    linha.filhos.push_back(Linhas(2));
    FracoesIguais(linha);
    return linha;
  }
  No coluna(No::Tipo::kColuna);
  coluna.filhos.push_back(Colunas(2));
  coluna.filhos.push_back(Colunas(2));
  FracoesIguais(coluna);
  return coluna;
}

No Inserir(const No& raiz, int alvo, Lado lado, int* posicao_nova) {
  No r = raiz;
  Numerar(r);
  InserirPorOrigem(r, alvo, lado, -1);
  Normalizar(r);
  std::vector<int> origens;
  Origens(r, &origens);
  if (posicao_nova) {
    *posicao_nova = static_cast<int>(
        std::find(origens.begin(), origens.end(), -1) - origens.begin());
  }
  Limpar(r);
  return r;
}

No Remover(const No& raiz, int painel) {
  No r = raiz;
  Numerar(r);
  RemoverPorOrigem(r, painel);
  Limpar(r);
  return r;
}

No Mover(const No& raiz, int de, int alvo, Lado lado, std::vector<int>* ordem) {
  No r = raiz;
  Numerar(r);
  const int n = Paineis(r);
  if (de != alvo && de >= 0 && de < n && alvo >= 0 && alvo < n) {
    RemoverPorOrigem(r, de);
    InserirPorOrigem(r, alvo, lado, de);
    Normalizar(r);
  }
  if (ordem) {
    ordem->clear();
    Origens(r, ordem);
  }
  Limpar(r);
  return r;
}

No Igualar(const No& raiz) {
  No r = raiz;
  IgualarTudo(r);
  return r;
}

No IgualarNo(const No& raiz, const std::vector<int>& caminho) {
  No r = raiz;
  No* no = NoEm(r, caminho);
  if (no && !no->EPainel()) {
    FracoesIguais(*no);
  }
  return r;
}

int Vizinho(const No& raiz, int painel) {
  No r = raiz;
  Numerar(r);
  std::vector<int> caminho;
  if (!Achar(r, painel, &caminho) || caminho.empty()) {
    return painel;
  }
  const int i = caminho.back();
  caminho.pop_back();
  const No* pai = NoEm(r, caminho);
  int vizinho = painel;
  if (i > 0) {
    PrimeiraOuUltima(pai->filhos[i - 1], /*ultima=*/true, &vizinho);
  } else {
    PrimeiraOuUltima(pai->filhos[i + 1], /*ultima=*/false, &vizinho);
  }
  return vizinho;
}

Desenho Calcular(const No& raiz, const Retangulo& area, int espaco) {
  Desenho desenho;
  desenho.paineis.resize(Paineis(raiz));
  std::vector<int> caminho;
  int contador = 0;
  CalcularNo(raiz, area, std::max(0, espaco), &caminho, &contador, &desenho);
  return desenho;
}

No Redimensionar(const No& inicial,
                 const Retangulo& area,
                 int espaco,
                 const std::vector<int>& caminho,
                 int indice,
                 int delta,
                 int minimo) {
  No r = inicial;
  No* no = NoEm(r, caminho);
  if (!no || no->EPainel() || indice < 0 ||
      indice + 1 >= static_cast<int>(no->filhos.size())) {
    return r;
  }
  espaco = std::max(0, espaco);
  const Retangulo area_do_no = AreaDoNo(r, area, espaco, caminho);
  const bool linha = no->tipo == No::Tipo::kLinha;
  const int n = static_cast<int>(no->filhos.size());
  const int disponivel = std::max(
      0, (linha ? area_do_no.largura : area_do_no.altura) - espaco * (n - 1));
  if (disponivel <= 0) {
    return r;
  }
  const std::vector<int> tamanhos = Tamanhos(*no, disponivel);
  const int soma = tamanhos[indice] + tamanhos[indice + 1];
  if (soma <= 0) {
    return r;
  }
  const int min_a =
      std::min(Minimo(no->filhos[indice], linha, minimo, espaco), soma / 2);
  const int min_b =
      std::min(Minimo(no->filhos[indice + 1], linha, minimo, espaco), soma / 2);
  int a = std::clamp(tamanhos[indice] + delta, min_a, soma - min_b);
  if (std::abs(a - soma / 2) < kGrude) {
    a = soma / 2;
  }
  const double par = no->fracoes[indice] + no->fracoes[indice + 1];
  no->fracoes[indice] = par * a / soma;
  no->fracoes[indice + 1] = par - no->fracoes[indice];
  return r;
}

int PainelEm(const Desenho& desenho, int x, int y) {
  for (size_t k = 0; k < desenho.paineis.size(); ++k) {
    if (desenho.paineis[k].Contem(x, y)) {
      return static_cast<int>(k);
    }
  }
  return -1;
}

}  // namespace navegador::divisao
