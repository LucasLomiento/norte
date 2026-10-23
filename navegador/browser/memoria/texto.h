// Texto da Memória (memoria.h): o que entra no índice e o que sai na busca.
// Sem IA: é busca de palavras (o FTS5 do SQLite, com o tokenizador
// unicode61, que ignora maiúsculas e acentos). Japonês e chinês não separam
// as palavras com espaço: o ICU separa antes, no índice e na busca.
//
// Só depende do //base (e do ICU por ele): os testes rodam sem o navegador
// (navegador_memoria_unittests).

#ifndef NAVEGADOR_BROWSER_MEMORIA_TEXTO_H_
#define NAVEGADOR_BROWSER_MEMORIA_TEXTO_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace navegador::memoria::texto {

// O texto da página como fica guardado: linhas aparadas, espaços e linhas
// em branco repetidos juntados, sem os caracteres invisíveis, no máximo
// `limite` letras (sem partir um par substituto).
std::u16string Limpar(std::u16string_view bruto, size_t limite);

// A letra é de uma escrita sem espaço entre as palavras (japonês, chinês,
// tailandês).
bool SemEspacos(char16_t c);

// `texto` com as palavras das escritas sem espaço separadas por um espaço
// (o ICU acha onde uma palavra termina), pro tokenizador do FTS5, que só
// separa por espaço e pontuação. O resto fica igual.
std::u16string Segmentar(std::u16string_view texto);

// Minúsculas e sem acento (nas letras latinas, gregas e cirílicas), letra por
// letra: o tamanho não muda, e a posição de cada letra no texto dobrado é a
// mesma do original.
std::u16string Dobrar(std::u16string_view texto);

// Uma palavra buscada, dobrada (Dobrar), pra achar o trecho no texto.
struct Termo {
  std::u16string texto;
  // A palavra inteira ("de", "com", o que está entre aspas); senão, o começo
  // de uma palavra ("japo" acha "japonês").
  bool inteiro = false;
  // Marcar no trecho. As curtas ("de", "com"), quando a busca tem palavras
  // maiores, só filtram: marcadas, acendiam o texto inteiro.
  bool destacar = true;
};

struct Consulta {
  // Pro MATCH do FTS5 ("termo"* "de" "uma frase"); vazio se não sobrou nada
  // que valha buscar.
  std::string fts;
  std::vector<Termo> termos;
};

// O que a pessoa digitou vira a consulta: toda palavra precisa aparecer;
// as de 4 letras ou mais (e a que ainda está sendo digitada, no fim) valem
// como começo de palavra, então "japo" acha "japonês"; as curtas já
// digitadas ("de", "com", "the"), só inteiras. O que está "entre aspas" vale
// como frase. Palavras de uma letra só (fora do japonês e do chinês) ficam
// de fora.
Consulta Montar(std::u16string_view digitado);

// Um pedaço do texto em volta das palavras buscadas.
struct Trecho {
  std::u16string texto;
  // Começo e fim (no `texto` do trecho) de cada palavra achada.
  std::vector<std::pair<size_t, size_t>> destaques;
  // Algumas palavras seguidas do texto original, numa linha só, em volta da
  // melhor ocorrência: vão pro fragmento de texto do endereço (Fragmento),
  // que abre a página com elas à vista e destacadas. Vazio se as palavras
  // não estão no texto (só no título).
  std::u16string frase;
};

// O melhor pedaço de `texto` com cerca de `tamanho` letras: o que tem mais
// palavras diferentes da busca, e com elas mais juntas. As quebras de linha
// viram " · ", e "…" marca o corte. Sem palavra achada, a partir do primeiro
// parágrafo de verdade (o começo do texto costuma ser o menu do site).
Trecho Recortar(std::u16string_view texto,
                const std::vector<Termo>& termos,
                size_t tamanho);

// O mesmo trecho começando umas `antes` letras antes da primeira palavra
// achada (pra caber numa ou duas linhas com ela à vista).
Trecho Encurtar(const Trecho& trecho, size_t antes);

// ":~:text=..." com a `frase` codificada, pra pôr no fim do fragmento do
// endereço (o "#"). Vazio se a frase é vazia.
std::string Fragmento(std::u16string_view frase);

}  // namespace navegador::memoria::texto

#endif  // NAVEGADOR_BROWSER_MEMORIA_TEXTO_H_
