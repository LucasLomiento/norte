#include "navegador/browser/memoria/texto.h"

#include <string>

#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace navegador::memoria::texto {
namespace {

std::u16string Destacado(const Trecho& trecho, size_t i) {
  const auto& [inicio, fim] = trecho.destaques[i];
  return trecho.texto.substr(inicio, fim - inicio);
}

TEST(MemoriaTexto, LimparJuntaEspacosELinhas) {
  EXPECT_EQ(Limpar(u"  Olá \t mundo \r\n\n\n   segunda  linha ​",
                   1000),
            u"Olá mundo\nsegunda linha");
  EXPECT_EQ(Limpar(u"\n\n  \n", 1000), u"");
  EXPECT_EQ(Limpar(u"abcdef", 3), u"abc");
  // Não parte o par substituto (😀 = 2 unidades).
  EXPECT_EQ(Limpar(u"ab😀", 3), u"ab");
}

TEST(MemoriaTexto, DobrarTiraAcentoEMaiuscula) {
  EXPECT_EQ(Dobrar(u"Ação Café ÉLAN Über"), u"acao cafe elan uber");
  // O tamanho não muda; o japonês fica (o dakuten é parte da letra).
  EXPECT_EQ(Dobrar(u"がく"), u"がく");
  EXPECT_EQ(Dobrar(u"ΆΘΗΝΑ").size(), 5u);
}

TEST(MemoriaTexto, SegmentarSoMexeNoQueNaoTemEspaco) {
  EXPECT_EQ(Segmentar(u"cerâmica japonesa"), u"cerâmica japonesa");
  const std::u16string japones = Segmentar(u"日本語を勉強しています");
  EXPECT_NE(japones.find(u' '), std::u16string::npos);
  EXPECT_NE(japones.find(u"勉強"), std::u16string::npos);
  EXPECT_EQ(Segmentar(u"Chromeで検索"), u"Chrome で 検索");
}

TEST(MemoriaTexto, MontarPalavrasComoPrefixo) {
  Consulta c = Montar(u"Cerâmica japo");
  EXPECT_EQ(c.fts, "\"Cerâmica\"* \"japo\"*");
  ASSERT_EQ(c.termos.size(), 2u);
  EXPECT_EQ(c.termos[0].texto, u"ceramica");
  EXPECT_EQ(c.termos[1].texto, u"japo");
}

TEST(MemoriaTexto, MontarIgnoraLetraSoltaEPontuacao) {
  EXPECT_EQ(Montar(u"a casa, e o gato!").fts, "\"casa\"* \"gato\"*");
  EXPECT_EQ(Montar(u"  ").fts, "");
  EXPECT_EQ(Montar(u"a").fts, "");
  EXPECT_EQ(Montar(u"e-mail").fts, "\"mail\"*");
}

TEST(MemoriaTexto, MontarFrasesEntreAspas) {
  Consulta c = Montar(u"\"arte japonesa\" ouro");
  EXPECT_EQ(c.fts, "\"arte japonesa\" \"ouro\"*");
  EXPECT_EQ(c.termos.size(), 3u);
  // Aspas sem par: palavras soltas.
  EXPECT_EQ(Montar(u"\"arte japonesa").fts, "\"arte\"* \"japonesa\"*");
}

TEST(MemoriaTexto, MontarJapones) {
  EXPECT_EQ(Montar(u"勉強").fts, "\"勉強\"*");
  // Partícula sozinha não entra; o ideograma sozinho entra.
  EXPECT_EQ(Montar(u"の").fts, "");
  EXPECT_EQ(Montar(u"猫").fts, "\"猫\"*");
}

TEST(MemoriaTexto, RecortarAchaAPalavraEDestaca) {
  const std::u16string texto =
      u"Kintsugi\nO kintsugi é a técnica japonesa de consertar cerâmica "
      u"quebrada com laca e ouro em pó, deixando as marcas à vista.\nOutra "
      u"linha qualquer sem nada.";
  Trecho t = Recortar(texto, Montar(u"ceramica ouro").termos, 80);
  ASSERT_EQ(t.destaques.size(), 2u);
  EXPECT_EQ(Destacado(t, 0), u"cerâmica");
  EXPECT_EQ(Destacado(t, 1), u"ouro");
  EXPECT_TRUE(t.texto.starts_with(u"…"));
  EXPECT_NE(t.frase.find(u"cerâmica"), std::u16string::npos);
  EXPECT_EQ(t.frase.find(u'\n'), std::u16string::npos);
}

TEST(MemoriaTexto, RecortarPrefixoDestacaAPalavraInteira) {
  Trecho t = Recortar(u"uma arte japonesa antiga", Montar(u"japo").termos, 100);
  ASSERT_EQ(t.destaques.size(), 1u);
  EXPECT_EQ(Destacado(t, 0), u"japonesa");
  // Só como começo de palavra: "japo" no meio de outra não conta.
  EXPECT_TRUE(
      Recortar(u"nipojaponês", Montar(u"japo").termos, 100).destaques.empty());
}

TEST(MemoriaTexto, RecortarComecaNaLinhaEQuebraViraPonto) {
  // Começa na linha do achado (sem o pedaço da de cima) e segue nas de
  // baixo, com " · " no lugar da quebra.
  Trecho t = Recortar(u"Título\nprimeira frase\nsegunda", Montar(u"primeira").termos,
                      200);
  EXPECT_EQ(t.texto, u"primeira frase · segunda");
  ASSERT_EQ(t.destaques.size(), 1u);
  EXPECT_EQ(Destacado(t, 0), u"primeira");
}

TEST(MemoriaTexto, RecortarSemAchadoDaOComeco) {
  Trecho t = Recortar(u"começo do texto e mais coisas", {{.texto = u"zzz"}}, 10);
  EXPECT_TRUE(t.destaques.empty());
  EXPECT_TRUE(t.frase.empty());
  EXPECT_TRUE(t.texto.starts_with(u"começo"));
  EXPECT_TRUE(t.texto.ends_with(u"…"));
}

TEST(MemoriaTexto, RecortarPrefereAJanelaComMaisTermos) {
  std::u16string texto = u"ouro aqui sozinho. ";
  for (int i = 0; i < 20; ++i) {
    texto += u"enchimento de linguiça pra afastar. ";
  }
  texto += u"cerâmica com ouro no meio.";
  Trecho t = Recortar(texto, Montar(u"ouro ceramica").termos, 60);
  EXPECT_EQ(t.destaques.size(), 2u);
}

TEST(MemoriaTexto, RecortarJapones) {
  const std::u16string texto = u"私は毎日日本語を勉強しています。";
  Trecho t = Recortar(texto, Montar(u"勉強").termos, 100);
  ASSERT_EQ(t.destaques.size(), 1u);
  EXPECT_EQ(Destacado(t, 0), u"勉強");
  EXPECT_TRUE(t.frase.starts_with(u"勉強"));
}

TEST(MemoriaTexto, FragmentoCodificaASintaxe) {
  EXPECT_EQ(Fragmento(u"técnica-japonesa, ouro & prata"),
            ":~:text=t%C3%A9cnica%2Djaponesa%2C%20ouro%20%26%20prata");
  EXPECT_EQ(Fragmento(u""), "");
}


// Textos e buscas sorteados (com emoji, japonês, quebras e espaços
// estranhos): nada sai do lugar nem quebra.
TEST(MemoriaTexto, SorteadosNaoQuebram) {
  const std::u16string pedacos[] = {
      u"casa", u"Café", u"ÉLAN", u" ", u"\n", u"\n\n", u"😀", u"日本語",
      u"を", u"勉強", u"-", u",", u"\u00a0", u"\u200b", u"ação", u"x",
      u"\"", u"é", u"\t", u"ภาษาไทย"};
  for (int rodada = 0; rodada < 2000; ++rodada) {
    std::u16string bruto;
    const int n = base::RandIntInclusive(0, 60);
    for (int i = 0; i < n; ++i) {
      bruto += pedacos[base::RandIntInclusive(0, std::size(pedacos) - 1)];
    }
    const std::u16string texto = Limpar(bruto, 1 + base::RandIntInclusive(0, 300));
    std::u16string busca;
    for (int i = 0; i < base::RandIntInclusive(0, 4); ++i) {
      busca += pedacos[base::RandIntInclusive(0, std::size(pedacos) - 1)];
      busca += u" ";
    }
    const Consulta consulta = Montar(busca);
    const Trecho trecho =
        Recortar(texto, consulta.termos, base::RandIntInclusive(1, 200));
    for (const auto& [inicio, fim] : trecho.destaques) {
      ASSERT_LE(inicio, fim);
      ASSERT_LE(fim, trecho.texto.size());
    }
    ASSERT_EQ(Dobrar(texto).size(), texto.size());
    Fragmento(trecho.frase);
    Segmentar(texto);
  }
}


TEST(MemoriaTexto, RecortarPrefereAsPalavrasJuntas) {
  // "ouro" aparece cedo, longe de "cerâmica"; mais adiante as duas vêm
  // juntas: o trecho é o de lá.
  const std::u16string texto =
      u"O ouro foi achado no rio, segundo a lenda dos garimpeiros antigos da "
      u"região, que contavam histórias longas. Depois, a técnica de consertar "
      u"cerâmica quebrada com laca e ouro ficou famosa.";
  Trecho t = Recortar(texto, Montar(u"ouro ceramica").termos, 400);
  ASSERT_GE(t.destaques.size(), 2u);
  EXPECT_NE(t.frase.find(u"cerâmica"), std::u16string::npos);
}

TEST(MemoriaTexto, RecortarSemBuscaPulaOMenu) {
  Trecho t = Recortar(u"Revista\nCultura Artes Design\nNo Japão do século XV, "
                      u"um xogum mandou consertar uma tigela de chá chinesa.",
                      {}, 300);
  EXPECT_TRUE(t.texto.starts_with(u"No Japão"));
}

TEST(MemoriaTexto, EncurtarComecaPertoDaPalavra) {
  Trecho longo = Recortar(
      u"um texto bem comprido antes da palavra pra empurrar ela pra frente "
      u"bastante, e então finalmente a palavra kintsugi aparece aqui.",
      Montar(u"kintsugi").termos, 400);
  Trecho curto = Encurtar(longo, 12);
  ASSERT_EQ(curto.destaques.size(), 1u);
  EXPECT_TRUE(curto.texto.starts_with(u"…"));
  const auto& [inicio, fim] = curto.destaques[0];
  EXPECT_EQ(curto.texto.substr(inicio, fim - inicio), u"kintsugi");
  EXPECT_LE(inicio, 16u);
  // Já perto do começo: igual.
  Trecho perto = Recortar(u"kintsugi é bonito", Montar(u"kintsugi").termos, 400);
  EXPECT_EQ(Encurtar(perto, 12).texto, perto.texto);
}


TEST(MemoriaTexto, FraseLevaAsPalavrasPerto) {
  Trecho t = Recortar(
      u"Parágrafo antes.\nFicou decidido que a piscina reabre em março, depois "
      u"da troca do filtro de areia pela empresa Aquaviva.\nOutro parágrafo.",
      Montar(u"piscina filtro").termos, 400);
  EXPECT_NE(t.frase.find(u"piscina"), std::u16string::npos);
  EXPECT_NE(t.frase.find(u"filtro"), std::u16string::npos);
  EXPECT_EQ(t.frase.find(u'\n'), std::u16string::npos);
}


TEST(MemoriaTexto, PalavrasCurtasSoInteiras) {
  // "de" já digitado: só a palavra inteira, e sem marcar no trecho (com
  // palavras maiores na busca); a última, ainda sendo digitada, é começo.
  Consulta c = Montar(u"grampos de met");
  EXPECT_EQ(c.fts, "\"grampos\"* \"de\" \"met\"*");
  ASSERT_EQ(c.termos.size(), 3u);
  EXPECT_TRUE(c.termos[1].inteiro);
  EXPECT_FALSE(c.termos[1].destacar);
  EXPECT_FALSE(c.termos[2].inteiro);
  // Com espaço no fim, a última também já foi digitada.
  EXPECT_EQ(Montar(u"de com ").fts, "\"de\" \"com\"");
  // Só curtas: marcam.
  EXPECT_TRUE(Montar(u"de com").termos[0].destacar);

  Trecho t = Recortar(
      u"Ficou decidido depois: presa por grampos de metal, feia e torta.",
      Montar(u"grampos de metal").termos, 200);
  ASSERT_EQ(t.destaques.size(), 2u);
  const auto& [a, b] = t.destaques[0];
  EXPECT_EQ(t.texto.substr(a, b - a), u"grampos");
  const auto& [c2, d] = t.destaques[1];
  EXPECT_EQ(t.texto.substr(c2, d - c2), u"metal");
}

}  // namespace
}  // namespace navegador::memoria::texto
