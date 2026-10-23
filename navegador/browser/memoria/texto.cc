#include "navegador/browser/memoria/texto.h"

#include <algorithm>
#include <cstdint>
#include <set>

#include "base/i18n/break_iterator.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utf16.h"

namespace navegador::memoria::texto {

namespace {

// Ocorrências de uma palavra que o trecho olha, no máximo (texto enorme com
// a palavra em toda linha).
constexpr size_t kMaximoDeAchados = 400;

bool Invisivel(char16_t c) {
  return c == 0x00AD || (c >= 0x200B && c <= 0x200D) || c == 0x2060 ||
         c == 0xFEFF;
}

bool Quebra(char16_t c) {
  return c == u'\n' || c == u'\r' || c == 0x2028 || c == 0x2029;
}

bool Branco(char16_t c) {
  return c == u' ' || c == u'\t' || c == u'\f' || c == u'\v' || c == 0x00A0 ||
         c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F ||
         c == 0x205F || c == 0x3000;
}

// Letra que faz parte de uma palavra pro tokenizador do FTS5 (as categorias
// "L* N* Co M*" do banco.cc: letras, números, uso privado e as marcas que vão
// por cima das letras, que o tailandês usa no meio da palavra); o resto
// separa.
bool DePalavra(char16_t c) {
  if (c < 0x80) {
    return base::IsAsciiAlphaNumeric(c);
  }
  if (U16_IS_SURROGATE(c)) {
    return true;
  }
  return (U_GET_GC_MASK(c) &
          (U_GC_L_MASK | U_GC_N_MASK | U_GC_CO_MASK | U_GC_M_MASK)) != 0;
}

char16_t DobrarLetra(char16_t c) {
  if (c < 0x80) {
    return base::ToLowerASCII(c);
  }
  if (U16_IS_SURROGATE(c)) {
    return c;
  }
  const UChar32 minuscula = u_tolower(c);
  if (minuscula > 0xFFFF) {
    return c;
  }
  // Só as escritas com acento como letra à parte (latina, grega, cirílica):
  // no japonês a decomposição tiraria o dakuten (が → か).
  if (minuscula >= 0x0530) {
    return static_cast<char16_t>(minuscula);
  }
  static const icu::Normalizer2* const nfd = [] {
    UErrorCode erro = U_ZERO_ERROR;
    const icu::Normalizer2* normalizador =
        icu::Normalizer2::getNFDInstance(erro);
    return U_SUCCESS(erro) ? normalizador : nullptr;
  }();
  icu::UnicodeString decomposta;
  if (nfd && nfd->getDecomposition(minuscula, decomposta) &&
      decomposta.length() > 0 && !U16_IS_SURROGATE(decomposta.charAt(0))) {
    return decomposta.charAt(0);
  }
  return static_cast<char16_t>(minuscula);
}

// As palavras de `texto` como o tokenizador do FTS5 separa (as escritas sem
// espaço já segmentadas).
std::vector<std::u16string> Palavras(std::u16string_view texto) {
  std::vector<std::u16string> palavras;
  std::u16string atual;
  for (char16_t c : Segmentar(texto)) {
    if (DePalavra(c)) {
      atual.push_back(c);
    } else if (!atual.empty()) {
      palavras.push_back(std::move(atual));
      atual.clear();
    }
  }
  if (!atual.empty()) {
    palavras.push_back(std::move(atual));
  }
  return palavras;
}

bool SoHiragana(std::u16string_view palavra) {
  return std::ranges::all_of(
      palavra, [](char16_t c) { return c >= 0x3040 && c <= 0x309F; });
}

// Vale buscar a palavra? Uma letra só acha meio mundo (fora o ideograma
// japonês ou chinês, que já é uma palavra); partícula japonesa sozinha
// (の, を) também.
bool ValeBuscar(std::u16string_view palavra) {
  if (palavra.empty()) {
    return false;
  }
  if (SemEspacos(palavra.front())) {
    return palavra.size() > 1 || !SoHiragana(palavra);
  }
  return palavra.size() > 1;
}

std::string Aspas(std::u16string_view texto) {
  std::string escrito = "\"";
  for (char c : base::UTF16ToUTF8(texto)) {
    if (c == '"') {
      escrito += "\"\"";
    } else {
      escrito.push_back(c);
    }
  }
  escrito += "\"";
  return escrito;
}

struct Achado {
  size_t inicio;
  size_t fim;
  size_t termo;
};

// Onde os termos marcáveis aparecem no texto dobrado: como começo de
// palavra (a busca do FTS5 é por prefixo), estendido até o fim dela, ou a
// palavra inteira; nas escritas sem espaço, em qualquer lugar.
std::vector<Achado> Achar(std::u16string_view dobrado,
                          const std::vector<Termo>& termos) {
  std::vector<Achado> achados;
  for (size_t t = 0; t < termos.size(); ++t) {
    const std::u16string& termo = termos[t].texto;
    if (termo.empty() || !termos[t].destacar) {
      continue;
    }
    const bool sem_espacos = SemEspacos(termo.front());
    size_t vezes = 0;
    for (size_t pos = dobrado.find(termo);
         pos != std::u16string_view::npos && vezes < kMaximoDeAchados;
         pos = dobrado.find(termo, pos + 1)) {
      if (!sem_espacos && pos > 0 && DePalavra(dobrado[pos - 1])) {
        continue;
      }
      size_t fim = pos + termo.size();
      if (!sem_espacos && termos[t].inteiro && fim < dobrado.size() &&
          DePalavra(dobrado[fim])) {
        continue;
      }
      while (!sem_espacos && fim < dobrado.size() && DePalavra(dobrado[fim])) {
        ++fim;
      }
      achados.push_back({pos, fim, t});
      ++vezes;
    }
  }
  std::ranges::sort(achados, {}, &Achado::inicio);
  return achados;
}

size_t ComecoDaLinha(std::u16string_view texto, size_t pos) {
  const size_t quebra = texto.rfind(u'\n', pos == 0 ? 0 : pos - 1);
  return quebra == std::u16string_view::npos || pos == 0 ? 0 : quebra + 1;
}

size_t FimDaLinha(std::u16string_view texto, size_t pos) {
  const size_t quebra = texto.find(u'\n', pos);
  return quebra == std::u16string_view::npos ? texto.size() : quebra;
}

// Algumas palavras seguidas em volta do achado (e até `ate`, onde termina a
// última palavra buscada perto dele na mesma linha), sem sair da linha: o que
// o fragmento de texto procura na página e destaca.
std::u16string Frase(std::u16string_view texto,
                     const Achado& achado,
                     size_t ate) {
  const size_t linha = ComecoDaLinha(texto, achado.inicio);
  const size_t fim_da_linha = FimDaLinha(texto, achado.inicio);
  if (SemEspacos(texto[achado.inicio])) {
    // Sem espaços pra contar palavras: do achado até a pontuação seguinte
    // (ou umas 16 letras), começando nele (onde a palavra começa).
    size_t fim = achado.fim;
    while (fim < fim_da_linha && fim - achado.inicio < 16 &&
           DePalavra(texto[fim])) {
      ++fim;
    }
    return std::u16string(texto.substr(achado.inicio, fim - achado.inicio));
  }
  // Até 3 palavras antes e 4 depois.
  size_t inicio = achado.inicio;
  for (int palavras = 0; palavras < 3; ++palavras) {
    size_t i = inicio;
    while (i > linha && texto[i - 1] == u' ') {
      --i;
    }
    if (i == linha) {
      break;
    }
    while (i > linha && texto[i - 1] != u' ') {
      --i;
    }
    inicio = i;
  }
  size_t fim = std::max(achado.fim, ate);
  for (int palavras = 0; palavras < 4 && fim - inicio < 140; ++palavras) {
    size_t i = fim;
    while (i < fim_da_linha && texto[i] == u' ') {
      ++i;
    }
    if (i == fim_da_linha) {
      break;
    }
    while (i < fim_da_linha && texto[i] != u' ') {
      ++i;
    }
    fim = i;
  }
  std::u16string frase(texto.substr(inicio, fim - inicio));
  // Pontuação solta nas pontas atrapalha o encaixe ("(foo" com o parêntese
  // num elemento à parte).
  base::TrimString(frase, u" ,;:.!?()[]{}\"'«»“”‘’…-–—", &frase);
  return frase;
}

}  // namespace

std::u16string Limpar(std::u16string_view bruto, size_t limite) {
  std::u16string limpo;
  limpo.reserve(std::min(bruto.size(), limite));
  bool espaco = false;
  bool quebra = false;
  for (char16_t c : bruto) {
    if (limpo.size() >= limite) {
      break;
    }
    if (Invisivel(c)) {
      continue;
    }
    if (Quebra(c)) {
      quebra = !limpo.empty();
      espaco = false;
      continue;
    }
    if (Branco(c)) {
      espaco = !limpo.empty() && !quebra;
      continue;
    }
    if (c < 0x20 || c == 0x7F) {
      continue;
    }
    if (quebra) {
      limpo.push_back(u'\n');
    } else if (espaco) {
      limpo.push_back(u' ');
    }
    quebra = false;
    espaco = false;
    limpo.push_back(c);
  }
  if (!limpo.empty() && U16_IS_LEAD(limpo.back())) {
    limpo.pop_back();
  }
  return limpo;
}

bool SemEspacos(char16_t c) {
  return (c >= 0x3005 && c <= 0x3007) ||  // 々 〆 〇
         (c >= 0x3040 && c <= 0x30FF) ||  // hiragana e katakana
         (c >= 0x31F0 && c <= 0x31FF) ||  // katakana fonético
         (c >= 0x3400 && c <= 0x4DBF) ||  // ideogramas (extensão A)
         (c >= 0x4E00 && c <= 0x9FFF) ||  // ideogramas
         (c >= 0xF900 && c <= 0xFAFF) ||  // ideogramas de compatibilidade
         (c >= 0xFF66 && c <= 0xFF9F) ||  // katakana de meia largura
         (c >= 0x0E00 && c <= 0x0EFF) ||  // tailandês e laosiano
         (c >= 0x1000 && c <= 0x109F) ||  // birmanês
         (c >= 0x1780 && c <= 0x17FF);    // khmer
}

std::u16string Segmentar(std::u16string_view texto) {
  if (std::ranges::none_of(texto, SemEspacos)) {
    return std::u16string(texto);
  }
  base::i18n::BreakIterator palavras(texto,
                                     base::i18n::BreakIterator::BREAK_WORD);
  if (!palavras.Init()) {
    return std::u16string(texto);
  }
  std::u16string segmentado;
  segmentado.reserve(texto.size() + texto.size() / 3);
  while (palavras.Advance()) {
    std::u16string_view pedaco = palavras.GetString();
    if (pedaco.empty()) {
      continue;
    }
    // Espaço na divisa entre uma palavra sem espaços e o que vem colado nela
    // ("Chromeで検索" → "Chrome で 検索").
    if (!segmentado.empty() &&
        (SemEspacos(segmentado.back()) || SemEspacos(pedaco.front())) &&
        DePalavra(segmentado.back()) && DePalavra(pedaco.front())) {
      segmentado.push_back(u' ');
    }
    segmentado.append(pedaco);
  }
  return segmentado;
}

std::u16string Dobrar(std::u16string_view texto) {
  std::u16string dobrado(texto.size(), u'\0');
  for (size_t i = 0; i < texto.size(); ++i) {
    dobrado[i] = DobrarLetra(texto[i]);
  }
  return dobrado;
}

Consulta Montar(std::u16string_view digitado) {
  // Separa as "frases entre aspas" do resto; aspas sem par valem como
  // palavras soltas.
  struct Parte {
    std::u16string texto;
    bool frase;
  };
  std::vector<Parte> partes;
  std::u16string atual;
  bool dentro = false;
  for (char16_t c : digitado) {
    if (c == u'"' || c == u'“' || c == u'”') {
      partes.push_back({std::move(atual), dentro});
      atual.clear();
      dentro = !dentro;
      continue;
    }
    atual.push_back(c);
  }
  partes.push_back({std::move(atual), false});

  // A última palavra, se o texto termina nela, ainda está sendo digitada:
  // vale como começo de palavra mesmo curta.
  const bool digitando =
      !digitado.empty() && DePalavra(digitado.back()) && !dentro;
  const std::vector<std::u16string> ultimas = Palavras(partes.back().texto);
  const std::u16string ultima =
      digitando && !partes.back().frase && !ultimas.empty() ? ultimas.back()
                                                            : u"";
  // Palavra "curta": até 3 letras (fora as escritas sem espaço).
  auto curta = [](std::u16string_view palavra) {
    return !SemEspacos(palavra.front()) && palavra.size() <= 3;
  };
  bool tem_longa = false;
  for (const Parte& parte : partes) {
    for (const std::u16string& palavra : Palavras(parte.texto)) {
      tem_longa |= !curta(palavra);
    }
  }

  Consulta consulta;
  std::set<std::u16string> vistos;
  std::vector<std::string> pedacos;
  for (size_t p = 0; p < partes.size(); ++p) {
    const Parte& parte = partes[p];
    std::vector<std::u16string> palavras = Palavras(parte.texto);
    if (parte.frase && palavras.size() > 1) {
      pedacos.push_back(Aspas(base::JoinString(palavras, u" ")));
      for (const std::u16string& palavra : palavras) {
        if (vistos.insert(Dobrar(palavra)).second) {
          consulta.termos.push_back(
              {.texto = Dobrar(palavra), .inteiro = true});
        }
      }
      continue;
    }
    for (size_t i = 0; i < palavras.size(); ++i) {
      const std::u16string& palavra = palavras[i];
      if (!ValeBuscar(palavra) && !(parte.frase && palavras.size() == 1)) {
        continue;
      }
      const std::u16string dobrada = Dobrar(palavra);
      if (!vistos.insert(dobrada).second) {
        continue;
      }
      const bool sendo_digitada = p + 1 == partes.size() &&
                                  i + 1 == palavras.size() && !ultima.empty();
      const bool inteiro =
          parte.frase || (curta(palavra) && !sendo_digitada);
      pedacos.push_back(Aspas(palavra) + (inteiro ? "" : "*"));
      consulta.termos.push_back(
          {.texto = dobrada,
           .inteiro = inteiro,
           .destacar = parte.frase || sendo_digitada || !curta(palavra) ||
                       !tem_longa});
    }
  }
  consulta.fts = base::JoinString(pedacos, " ");
  return consulta;
}

Trecho Recortar(std::u16string_view texto,
                const std::vector<Termo>& termos,
                size_t tamanho) {
  Trecho trecho;
  if (texto.empty() || tamanho == 0) {
    return trecho;
  }
  const std::u16string dobrado = Dobrar(texto);
  const std::vector<Achado> achados = Achar(dobrado, termos);

  // A janela com mais termos diferentes e, empatando, com eles mais juntos
  // (o pedaço em que as palavras aparecem perto uma da outra é o que a
  // pessoa lembra), começando um quarto antes do achado que abre ela.
  const size_t antes = tamanho / 4;
  size_t melhor = 0;
  int64_t melhor_nota = -1;
  for (size_t a = 0; a < achados.size(); ++a) {
    const size_t fim = achados[a].inicio + tamanho - antes;
    std::set<size_t> diferentes;
    size_t junto = 0;
    for (size_t b = a; b < achados.size() && achados[b].fim <= fim; ++b) {
      if (diferentes.insert(achados[b].termo).second) {
        junto = achados[b].fim - achados[a].inicio;
      }
    }
    const int64_t nota = static_cast<int64_t>(diferentes.size()) * 1000000 -
                         static_cast<int64_t>(junto);
    if (nota > melhor_nota) {
      melhor_nota = nota;
      melhor = a;
    }
  }

  size_t inicio = 0;
  if (achados.empty()) {
    // Sem palavra achada (linha do tempo): o primeiro parágrafo de verdade,
    // não o menu do site no começo.
    for (size_t linha = 0; linha < texto.size();) {
      const size_t fim_da_linha = FimDaLinha(texto, linha);
      if (fim_da_linha - linha >= 60) {
        inicio = linha;
        break;
      }
      linha = fim_da_linha + 1;
    }
  } else {
    const Achado& ancora = achados[melhor];
    // A frase leva junto as outras palavras buscadas que estão perto, na
    // mesma linha ("a piscina reabre ... troca do filtro").
    const size_t fim_da_linha = FimDaLinha(texto, ancora.inicio);
    size_t ate = ancora.fim;
    for (size_t b = melhor + 1; b < achados.size() &&
                                achados[b].inicio < fim_da_linha &&
                                achados[b].fim - ancora.inicio <= 110;
         ++b) {
      ate = std::max(ate, achados[b].fim);
    }
    trecho.frase = Frase(texto, ancora, ate);
    inicio = ancora.inicio > antes ? ancora.inicio - antes : 0;
    const size_t linha = ComecoDaLinha(texto, ancora.inicio);
    if (linha >= inicio) {
      inicio = linha;
    } else {
      // Não começa no meio de uma palavra.
      while (inicio < ancora.inicio && inicio > 0 &&
             DePalavra(texto[inicio - 1]) && !SemEspacos(texto[inicio])) {
        ++inicio;
      }
      while (inicio < ancora.inicio && texto[inicio] == u' ') {
        ++inicio;
      }
    }
  }
  size_t fim = std::min(texto.size(), inicio + tamanho);
  if (fim < texto.size() && !SemEspacos(texto[fim - 1])) {
    // Termina numa palavra inteira (se der).
    const size_t espaco = texto.find_last_of(u" \n", fim);
    if (espaco != std::u16string_view::npos && espaco > inicio + tamanho / 2) {
      fim = espaco;
    }
  }
  while (fim > inicio && (texto[fim - 1] == u' ' || texto[fim - 1] == u'\n')) {
    --fim;
  }

  // Monta o texto do trecho, com as quebras de linha virando " · ", e onde
  // cada letra da janela foi parar nele.
  std::vector<size_t> onde(fim - inicio + 1, 0);
  const bool reticencias_antes = inicio > 0 && texto[inicio - 1] != u'\n';
  if (reticencias_antes) {
    trecho.texto = u"…";
  }
  for (size_t i = inicio; i < fim; ++i) {
    onde[i - inicio] = trecho.texto.size();
    if (texto[i] == u'\n') {
      trecho.texto += u" · ";
    } else {
      trecho.texto.push_back(texto[i]);
    }
  }
  onde[fim - inicio] = trecho.texto.size();
  if (fim < texto.size() && texto[fim] != u'\n') {
    trecho.texto += u"…";
  }
  for (const Achado& achado : achados) {
    if (achado.inicio < inicio || achado.inicio >= fim) {
      continue;
    }
    trecho.destaques.emplace_back(onde[achado.inicio - inicio],
                                  onde[std::min(achado.fim, fim) - inicio]);
  }
  return trecho;
}

Trecho Encurtar(const Trecho& trecho, size_t antes) {
  if (trecho.destaques.empty() ||
      trecho.destaques.front().first <= antes + 1) {
    return trecho;
  }
  const size_t primeiro = trecho.destaques.front().first;
  size_t corte = primeiro - antes;
  // Começa numa palavra inteira (se der).
  while (corte < primeiro && trecho.texto[corte - 1] != u' ' &&
         !SemEspacos(trecho.texto[corte])) {
    ++corte;
  }
  if (corte >= primeiro) {
    corte = primeiro - antes;
  }
  Trecho curto;
  curto.texto = u"…" + trecho.texto.substr(corte);
  curto.frase = trecho.frase;
  for (const auto& [inicio, fim] : trecho.destaques) {
    if (inicio >= corte) {
      curto.destaques.emplace_back(inicio - corte + 1, fim - corte + 1);
    }
  }
  return curto;
}

std::string Fragmento(std::u16string_view frase) {
  if (frase.empty()) {
    return std::string();
  }
  // Fora letras, números e . _ ~, tudo codificado: o "-", a "," e o "&" são
  // sintaxe do fragmento de texto.
  std::string fragmento = ":~:text=";
  for (const unsigned char c : base::UTF16ToUTF8(frase)) {
    if (base::IsAsciiAlphaNumeric(c) || c == '.' || c == '_' || c == '~') {
      fragmento.push_back(static_cast<char>(c));
    } else {
      base::StringAppendF(&fragmento, "%%%02X", c);
    }
  }
  return fragmento;
}

}  // namespace navegador::memoria::texto
