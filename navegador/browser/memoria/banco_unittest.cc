#include "navegador/browser/memoria/banco.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace navegador::memoria {
namespace {

class MemoriaBanco : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(pasta_.CreateUniqueTempDir());
    banco_ = std::make_unique<Banco>(Arquivo());
    ASSERT_TRUE(banco_->Abrir());
  }

  base::FilePath Arquivo() const {
    return pasta_.GetPath().AppendASCII("Norte Memoria");
  }

  void Ler(const std::string& url,
           const std::u16string& titulo,
           const std::u16string& texto,
           base::Time quando = base::Time::Now(),
           const std::string& espaco = "pessoal",
           const std::string& container = "") {
    Leitura leitura;
    leitura.url = url;
    leitura.host = GURLHost(url);
    leitura.titulo = titulo;
    leitura.texto = texto;
    leitura.quando = quando;
    leitura.espaco = espaco;
    leitura.container = container;
    banco_->Lembrar(leitura);
  }

  static std::string GURLHost(const std::string& url) {
    const size_t inicio = url.find("://") + 3;
    return url.substr(inicio, url.find('/', inicio) - inicio);
  }

  std::vector<std::string> Achar(const std::u16string& consulta,
                                 Filtro filtro = Filtro()) {
    filtro.consulta = consulta;
    std::vector<std::string> urls;
    for (const Resultado& r : banco_->Buscar(filtro)) {
      urls.push_back(r.url);
    }
    return urls;
  }

  base::ScopedTempDir pasta_;
  std::unique_ptr<Banco> banco_;
};

TEST_F(MemoriaBanco, AchaPeloTextoSemAcentoEPorPrefixo) {
  Ler("https://pt.wikipedia.org/wiki/Kintsugi", u"Kintsugi – Wikipédia",
      u"Kintsugi é a técnica japonesa de consertar cerâmica com ouro.");
  Ler("https://exemplo.com/receita", u"Bolo de cenoura",
      u"Misture a cenoura com o açúcar e asse por 40 minutos.");
  EXPECT_EQ(Achar(u"ceramica"),
            std::vector<std::string>{"https://pt.wikipedia.org/wiki/Kintsugi"});
  EXPECT_EQ(Achar(u"CERÂM japo"),
            std::vector<std::string>{"https://pt.wikipedia.org/wiki/Kintsugi"});
  EXPECT_EQ(Achar(u"acucar"),
            std::vector<std::string>{"https://exemplo.com/receita"});
  // Todas as palavras precisam estar.
  EXPECT_TRUE(Achar(u"ceramica cenoura").empty());
  EXPECT_TRUE(Achar(u"inexistente").empty());
}

TEST_F(MemoriaBanco, TrechoComDestaqueEFrase) {
  Ler("https://a.com/1", u"Um título",
      u"Primeira linha.\nO kintsugi conserta cerâmica com ouro em pó.");
  std::vector<Resultado> r = banco_->Buscar({.consulta = u"ouro"});
  ASSERT_EQ(r.size(), 1u);
  ASSERT_EQ(r[0].trecho.destaques.size(), 1u);
  const auto& [inicio, fim] = r[0].trecho.destaques[0];
  EXPECT_EQ(r[0].trecho.texto.substr(inicio, fim - inicio), u"ouro");
  EXPECT_NE(r[0].trecho.frase.find(u"ouro"), std::u16string::npos);
  EXPECT_EQ(r[0].visitas, 1);
}

TEST_F(MemoriaBanco, TituloValeMaisQueTexto) {
  Ler("https://a.com/texto", u"Outra coisa",
      u"Aqui se fala de vulcões, só de passagem, num texto comprido sobre "
      u"geografia, rios, montanhas, planícies e cidades.");
  Ler("https://a.com/titulo", u"Vulcões", u"Montanhas que soltam lava.");
  const std::vector<std::string> urls = Achar(u"vulcoes");
  ASSERT_EQ(urls.size(), 2u);
  EXPECT_EQ(urls[0], "https://a.com/titulo");
}

TEST_F(MemoriaBanco, VoltarAtualizaOTextoEContaVisita) {
  Ler("https://a.com/x", u"Página", u"texto antigo com abacate");
  Ler("https://a.com/x", u"Página", u"texto novo com banana");
  EXPECT_TRUE(Achar(u"abacate").empty());
  std::vector<Resultado> r = banco_->Buscar({.consulta = u"banana"});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].visitas, 2);

  // A releitura (ao sair da aba) não conta visita.
  Leitura releitura;
  releitura.url = "https://a.com/x";
  releitura.host = "a.com";
  releitura.titulo = u"Página";
  releitura.texto = u"texto novo com banana e caju";
  releitura.quando = base::Time::Now();
  releitura.visita = false;
  banco_->Lembrar(releitura);
  r = banco_->Buscar({.consulta = u"caju"});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].visitas, 2);

  banco_->SomarTempo("https://a.com/x", 90);
  r = banco_->Buscar({.consulta = u"caju"});
  EXPECT_EQ(r[0].segundos, 90);
}

TEST_F(MemoriaBanco, FiltrosDeEspacoEData) {
  const base::Time agora = base::Time::Now();
  Ler("https://a.com/velha", u"Velha", u"tema comum", agora - base::Days(20),
      "trabalho");
  Ler("https://a.com/nova", u"Nova", u"tema comum", agora, "pessoal");
  EXPECT_EQ(Achar(u"tema").size(), 2u);
  EXPECT_EQ(Achar(u"tema", {.espaco = "trabalho"}),
            std::vector<std::string>{"https://a.com/velha"});
  EXPECT_EQ(Achar(u"tema", {.desde = agora - base::Days(7)}),
            std::vector<std::string>{"https://a.com/nova"});
}

TEST_F(MemoriaBanco, RecentesEmOrdem) {
  const base::Time agora = base::Time::Now();
  Ler("https://a.com/1", u"Um", u"um", agora - base::Hours(2));
  Ler("https://a.com/2", u"Dois", u"dois", agora - base::Hours(1));
  Ler("https://a.com/3", u"Três", u"três", agora);
  std::vector<Resultado> r = banco_->Buscar(Filtro());
  ASSERT_EQ(r.size(), 3u);
  EXPECT_EQ(r[0].url, "https://a.com/3");
  EXPECT_EQ(r[2].url, "https://a.com/1");
  EXPECT_EQ(r[0].trecho.texto, u"três");
}

TEST_F(MemoriaBanco, Miniatura) {
  Ler("https://a.com/1", u"Um", u"um texto");
  EXPECT_FALSE(banco_->TemMiniatura("https://a.com/1", base::Days(1)));
  banco_->GuardarMiniatura("https://a.com/1", {1, 2, 3}, base::Time::Now());
  // Página que não está no banco: não guarda.
  banco_->GuardarMiniatura("https://a.com/fora", {4}, base::Time::Now());
  EXPECT_TRUE(banco_->TemMiniatura("https://a.com/1", base::Days(1)));
  std::vector<Resultado> r = banco_->Buscar({.consulta = u"texto"});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_TRUE(r[0].tem_miniatura);
  EXPECT_EQ(banco_->Miniatura(r[0].id), (std::vector<uint8_t>{1, 2, 3}));
}

TEST_F(MemoriaBanco, Esquecer) {
  const base::Time agora = base::Time::Now();
  Ler("https://a.com/1", u"Um", u"palavra", agora - base::Days(3));
  Ler("https://sub.b.com/2", u"Dois", u"palavra", agora, "pessoal", "banco");
  Ler("https://b.com/3", u"Três", u"palavra", agora);
  Ler("https://xb.com/4", u"Quatro", u"palavra", agora);
  Ler("https://c.com/5", u"Cinco", u"palavra", agora, "pessoal", "banco");
  ASSERT_EQ(Achar(u"palavra").size(), 5u);

  banco_->EsquecerSite("b.com");
  EXPECT_EQ(Achar(u"palavra").size(), 3u);  // xb.com fica.

  banco_->EsquecerContainer("banco");
  EXPECT_EQ(Achar(u"palavra").size(), 2u);

  banco_->EsquecerPeriodo(agora - base::Days(4), agora - base::Days(2));
  EXPECT_EQ(Achar(u"palavra"), std::vector<std::string>{"https://xb.com/4"});

  banco_->EsquecerEnderecos({"https://xb.com/4"});
  EXPECT_TRUE(Achar(u"palavra").empty());
  EXPECT_EQ(banco_->Resumir().paginas, 0);
}

TEST_F(MemoriaBanco, EsquecerTudoEResumo) {
  Ler("https://a.com/1", u"Um", u"palavra");
  Ler("https://a.com/2", u"Dois", u"palavra");
  banco_->GuardarMiniatura("https://a.com/1", {1}, base::Time::Now());
  Resumo resumo = banco_->Resumir();
  EXPECT_EQ(resumo.paginas, 2);
  EXPECT_GT(resumo.bytes, 0);
  EXPECT_FALSE(resumo.mais_antiga.is_null());
  banco_->EsquecerTudo();
  EXPECT_TRUE(Achar(u"palavra").empty());
  EXPECT_EQ(banco_->Resumir().paginas, 0);
  // Continua funcionando depois.
  Ler("https://a.com/3", u"Três", u"palavra");
  EXPECT_EQ(Achar(u"palavra").size(), 1u);
}

TEST_F(MemoriaBanco, Podar) {
  const base::Time agora = base::Time::Now();
  Ler("https://a.com/velha", u"Velha", u"palavra", agora - base::Days(100));
  Ler("https://a.com/nova", u"Nova", u"palavra", agora);
  Ler("https://a.com/outra", u"Outra", u"palavra", agora);
  banco_->GuardarMiniatura("https://a.com/nova", {1}, agora - base::Days(40));
  banco_->GuardarMiniatura("https://a.com/outra", {2}, agora);
  banco_->Podar(agora - base::Days(90), agora - base::Days(30), 10);
  EXPECT_EQ(Achar(u"palavra").size(), 2u);
  EXPECT_FALSE(banco_->TemMiniatura("https://a.com/nova", base::Days(365)));
  EXPECT_TRUE(banco_->TemMiniatura("https://a.com/outra", base::Days(1)));
  // Sem prazo pras páginas, só o teto das miniaturas.
  banco_->GuardarMiniatura("https://a.com/nova", {1}, agora - base::Hours(1));
  banco_->Podar(base::Time(), agora - base::Days(30), 1);
  EXPECT_EQ(Achar(u"palavra").size(), 2u);
  EXPECT_FALSE(banco_->TemMiniatura("https://a.com/nova", base::Days(365)));
  EXPECT_TRUE(banco_->TemMiniatura("https://a.com/outra", base::Days(1)));
}

TEST_F(MemoriaBanco, Japones) {
  Ler("https://ja.example/1", u"日本語の勉強",
      u"私は毎日日本語を勉強しています。漢字は難しいです。");
  EXPECT_EQ(Achar(u"勉強").size(), 1u);
  EXPECT_EQ(Achar(u"漢字").size(), 1u);
  EXPECT_EQ(Achar(u"日本語").size(), 1u);
  EXPECT_TRUE(Achar(u"中国語").empty());
  std::vector<Resultado> r = banco_->Buscar({.consulta = u"漢字"});
  ASSERT_EQ(r.size(), 1u);
  ASSERT_FALSE(r[0].trecho.destaques.empty());
  EXPECT_TRUE(r[0].trecho.frase.starts_with(u"漢字"));
}

TEST_F(MemoriaBanco, ConsultaEstranhaNaoQuebra) {
  Ler("https://a.com/1", u"Um", u"NEAR AND OR NOT \"aspas\" * ( ) ^ :");
  for (const char16_t* consulta :
       {u"\"", u"\"\"", u"*", u"NEAR(", u"a:b", u"^x", u"( ) AND", u"OR",
        u"col:texto", u"-x", u"\u0000x"}) {
    banco_->Buscar({.consulta = consulta});
  }
  EXPECT_EQ(Achar(u"aspas").size(), 1u);
  EXPECT_EQ(Achar(u"NEAR").size(), 1u);
}

TEST_F(MemoriaBanco, ArquivoEstragadoComecaDeNovo) {
  Ler("https://a.com/1", u"Um", u"palavra");
  banco_.reset();
  ASSERT_TRUE(base::WriteFile(Arquivo(), "isto não é um banco SQLite"));
  banco_ = std::make_unique<Banco>(Arquivo());
  ASSERT_TRUE(banco_->Abrir());
  EXPECT_TRUE(Achar(u"palavra").empty());
  Ler("https://a.com/2", u"Dois", u"palavra");
  EXPECT_EQ(Achar(u"palavra").size(), 1u);
}

TEST_F(MemoriaBanco, ReabreComOQueTinha) {
  Ler("https://a.com/1", u"Um", u"palavra guardada");
  banco_.reset();
  banco_ = std::make_unique<Banco>(Arquivo());
  ASSERT_TRUE(banco_->Abrir());
  EXPECT_EQ(Achar(u"guardada").size(), 1u);
}


// Rodar à mão: --gtest_also_run_disabled_tests --gtest_filter=*Desempenho*
TEST_F(MemoriaBanco, DISABLED_Desempenho) {
  // 5 mil páginas de ~12 mil letras com palavras sorteadas de um vocabulário
  // de 20 mil (o tamanho de um ano de leitura).
  std::vector<std::u16string> vocabulario;
  for (int i = 0; i < 20000; ++i) {
    std::u16string palavra;
    const int letras = 3 + base::RandIntInclusive(0, 8);
    for (int j = 0; j < letras; ++j) {
      palavra.push_back(u'a' + base::RandIntInclusive(0, 25));
    }
    vocabulario.push_back(palavra);
  }
  base::ElapsedTimer gravar;
  for (int p = 0; p < 5000; ++p) {
    std::u16string texto;
    while (texto.size() < 12000) {
      texto += vocabulario[base::RandIntInclusive(0, 19999)];
      texto += base::RandIntInclusive(0, 12) == 0 ? u"\n" : u" ";
    }
    Ler("https://site" + base::NumberToString(p % 300) + ".com/p" +
            base::NumberToString(p),
        vocabulario[base::RandIntInclusive(0, 19999)] + u" " +
            vocabulario[base::RandIntInclusive(0, 19999)],
        texto, base::Time::Now() - base::Minutes(p));
  }
  LOG(ERROR) << "gravar 5000: " << gravar.Elapsed();
  LOG(ERROR) << "arquivo: " << banco_->Resumir().bytes / 1024 / 1024 << " MB";
  for (const std::u16string& consulta :
       {vocabulario[7], vocabulario[7].substr(0, 2),
        vocabulario[9] + u" " + vocabulario[10],
        vocabulario[3].substr(0, 3) + u" " + vocabulario[4].substr(0, 3)}) {
    base::ElapsedTimer buscar;
    std::vector<Resultado> r = banco_->Buscar({.consulta = consulta});
    LOG(ERROR) << "buscar '" << consulta << "': " << r.size() << " em "
               << buscar.Elapsed();
  }
  base::ElapsedTimer recentes;
  banco_->Buscar(Filtro());
  LOG(ERROR) << "recentes: " << recentes.Elapsed();
}

}  // namespace
}  // namespace navegador::memoria
