#include "navegador/browser/memoria/banco.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace navegador::memoria {

namespace {

constexpr int kVersao = 1;
// Candidatos que a busca do FTS5 entrega pra ordenar aqui (relevância do
// texto, mais o quanto a página é recente e lida).
constexpr int kCandidatos = 300;
// Apagar em lotes: uma transação gigante segura o arquivo travado.
constexpr size_t kLote = 400;

// O tokenizador: sem maiúsculas e sem acento ("cafe" acha "Café"); as marcas
// por cima das letras (tailandês, devanágari) ficam dentro da palavra.
constexpr char kCriarBusca[] =
    "CREATE VIRTUAL TABLE IF NOT EXISTS busca USING fts5("
    "titulo, texto, content='', contentless_delete=1, "
    "tokenize=\"unicode61 remove_diacritics 2 categories 'L* N* Co M*'\")";

// O título vale mais que o texto.
constexpr char kRelevancia[] = "bm25(busca, 4.0, 1.0)";

// Na linha do tempo (sem busca) o trecho é o começo da página: não precisa
// ler o texto inteiro (até 120 mil letras por página, 150 páginas por vez).
constexpr int kComecoDoTexto = 4000;
// Na tabela de metadados: tem página apagada com as palavras ainda no
// índice (`Limpar`).
constexpr char kChaveSujo[] = "sujo";
// Depois de zerado por estar estragado, o arquivo é aberto de novo no
// próximo uso; mais que isso numa sessão é o disco, e a Memória para.
constexpr int kMaximoDeReaberturas = 3;

// Arquivo estragado de verdade. O `sql::IsErrorCatastrophic` conta também o
// erro genérico (SQLITE_ERROR), que é o que uma consulta malformada do FTS5
// devolve: uma busca estranha zerava a Memória inteira.
bool Estragado(int erro) {
  const sql::SqliteResultCode codigo = sql::ToSqliteResultCode(erro & 0xFF);
  return codigo == sql::SqliteResultCode::kCorrupt ||
         codigo == sql::SqliteResultCode::kNotADatabase;
}

}  // namespace

Banco::Banco(base::FilePath arquivo)
    : arquivo_(std::move(arquivo)),
      // O FTS5 é uma tabela virtual: o Chromium só deixa usar nos bancos que
      // pedem (patch no sql/database).
      db_(sql::DatabaseOptions().set_cache_size(128)
              .set_enable_virtual_tables_discouraged(true),
          "PageContentStore") {}

Banco::~Banco() = default;

bool Banco::Abrir() {
  if (aberto_) {
    return true;
  }
  db_.set_error_callback(
      base::BindRepeating(&Banco::Erro, base::Unretained(this)));
  for (int tentativa = 0; tentativa < 2; ++tentativa) {
    if (db_.Open(arquivo_)) {
      mais_novo_ = false;
      if (CriarTabelas()) {
        aberto_ = true;
        // Apagou e o Norte fechou antes de limpar: fica pra próxima poda.
        int sujo = 0;
        sujo_ = meta_.GetValue(kChaveSujo, &sujo) && sujo != 0;
        return true;
      }
      if (mais_novo_) {
        // De uma versão mais nova do Norte (voltou pra uma antiga): não dá
        // pra ler, mas também não é pra apagar o que a pessoa guardou.
        db_.Close();
        LOG(ERROR) << "Memória: o arquivo é de uma versão mais nova";
        return false;
      }
    }
    db_.Close();
    sql::Database::Delete(arquivo_);
  }
  LOG(ERROR) << "Memória: não deu pra abrir " << arquivo_;
  return false;
}

// Arquivo estragado no meio do uso: zera, e o próximo uso abre de novo (a
// memória volta a se encher com as próximas leituras).
void Banco::Erro(int erro, sql::Statement* comando) {
  DLOG(ERROR) << "Memória: erro " << erro << ": " << db_.GetErrorMessage();
  // Abrindo, quem cuida é o `Abrir` (apaga o arquivo e tenta de novo).
  if (!aberto_ || !Estragado(erro)) {
    return;
  }
  LOG(ERROR) << "Memória: banco estragado (" << erro << "), zerando";
  aberto_ = false;
  // Zerar também pode dar erro: sem o aviso, não volta pra cá.
  db_.reset_error_callback();
  db_.RazeAndPoison();
}

bool Banco::Pronto() {
  if (aberto_) {
    return true;
  }
  if (mais_novo_ || reaberturas_ >= kMaximoDeReaberturas) {
    return false;
  }
  ++reaberturas_;
  db_.Close();
  return Abrir();
}

bool Banco::CriarTabelas() {
  sql::Transaction transacao(&db_);
  if (!transacao.Begin() || !meta_.Init(&db_, kVersao, kVersao)) {
    return false;
  }
  if (meta_.GetCompatibleVersionNumber() > kVersao) {
    mais_novo_ = true;
    return false;
  }
  // O texto no fim da linha: ler os outros campos não passa pelas páginas
  // de transbordo dele.
  return db_.Execute(
             "CREATE TABLE IF NOT EXISTS paginas("
             "id INTEGER PRIMARY KEY,"
             "url TEXT NOT NULL UNIQUE,"
             "host TEXT NOT NULL,"
             "titulo TEXT NOT NULL,"
             "primeira INTEGER NOT NULL,"
             "ultima INTEGER NOT NULL,"
             "visitas INTEGER NOT NULL,"
             "segundos INTEGER NOT NULL,"
             "espaco TEXT NOT NULL,"
             "container TEXT NOT NULL,"
             "texto TEXT NOT NULL)") &&
         db_.Execute(
             "CREATE INDEX IF NOT EXISTS paginas_ultima ON paginas(ultima)") &&
         db_.Execute(
             "CREATE INDEX IF NOT EXISTS paginas_host ON paginas(host)") &&
         // Excluir um container apaga as páginas lidas nele: sem o índice,
         // cada exclusão (e cada container temporário que fecha) varria a
         // tabela inteira, com o texto das páginas.
         db_.Execute(
             "CREATE INDEX IF NOT EXISTS paginas_container "
             "ON paginas(container)") &&
         db_.Execute(kCriarBusca) &&
         db_.Execute(
             "CREATE TABLE IF NOT EXISTS miniaturas("
             "pagina INTEGER PRIMARY KEY,"
             "quando INTEGER NOT NULL,"
             "imagem BLOB NOT NULL)") &&
         transacao.Commit();
}

void Banco::Lembrar(const Leitura& leitura) {
  if (!Pronto() || leitura.url.empty()) {
    return;
  }
  sql::Transaction transacao(&db_);
  if (!transacao.Begin()) {
    return;
  }
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id, titulo, texto FROM paginas WHERE url = ?"));
  achar.BindString(0, leitura.url);
  if (achar.Step()) {
    const int64_t id = achar.ColumnInt64(0);
    const bool mudou = achar.ColumnString16(1) != leitura.titulo ||
                       achar.ColumnString16(2) != leitura.texto;
    achar.Reset(/*clear_bound_vars=*/true);
    sql::Statement atualizar(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE paginas SET host = ?, titulo = ?, ultima = ?, "
        "visitas = visitas + ?, espaco = ?, container = ?, texto = ? "
        "WHERE id = ?"));
    atualizar.BindString(0, leitura.host);
    atualizar.BindString16(1, leitura.titulo);
    atualizar.BindTime(2, leitura.quando);
    atualizar.BindInt(3, leitura.visita ? 1 : 0);
    atualizar.BindString(4, leitura.espaco);
    atualizar.BindString(5, leitura.container);
    atualizar.BindString16(6, leitura.texto);
    atualizar.BindInt64(7, id);
    if (!atualizar.Run()) {
      return;
    }
    if (mudou) {
      sql::Statement tirar(db_.GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM busca WHERE rowid = ?"));
      tirar.BindInt64(0, id);
      sql::Statement por(db_.GetCachedStatement(
          SQL_FROM_HERE,
          "INSERT INTO busca(rowid, titulo, texto) VALUES(?, ?, ?)"));
      por.BindInt64(0, id);
      por.BindString16(1, texto::Segmentar(leitura.titulo));
      por.BindString16(2, texto::Segmentar(leitura.texto));
      if (!tirar.Run() || !por.Run()) {
        return;
      }
    }
    std::ignore = transacao.Commit();
    return;
  }
  achar.Reset(/*clear_bound_vars=*/true);
  // A releitura (ao sair da aba) só atualiza o que já está guardado. Se a
  // página saiu daqui no meio (a pessoa mandou esquecer, apagou o histórico
  // ou tudo), ela não volta sem uma visita nova.
  if (!leitura.visita) {
    return;
  }
  sql::Statement inserir(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO paginas(url, host, titulo, primeira, ultima, visitas, "
      "segundos, espaco, container, texto) "
      "VALUES(?, ?, ?, ?, ?, ?, 0, ?, ?, ?)"));
  inserir.BindString(0, leitura.url);
  inserir.BindString(1, leitura.host);
  inserir.BindString16(2, leitura.titulo);
  inserir.BindTime(3, leitura.quando);
  inserir.BindTime(4, leitura.quando);
  inserir.BindInt(5, 1);
  inserir.BindString(6, leitura.espaco);
  inserir.BindString(7, leitura.container);
  inserir.BindString16(8, leitura.texto);
  if (!inserir.Run()) {
    return;
  }
  const int64_t id = db_.GetLastInsertRowId();
  sql::Statement por(db_.GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO busca(rowid, titulo, texto) VALUES(?, ?, ?)"));
  por.BindInt64(0, id);
  por.BindString16(1, texto::Segmentar(leitura.titulo));
  por.BindString16(2, texto::Segmentar(leitura.texto));
  if (por.Run()) {
    std::ignore = transacao.Commit();
  }
}

void Banco::SomarTempo(const std::string& url, int segundos) {
  if (!Pronto() || segundos <= 0) {
    return;
  }
  sql::Statement somar(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE paginas SET segundos = segundos + ? WHERE url = ?"));
  somar.BindInt(0, segundos);
  somar.BindString(1, url);
  std::ignore = somar.Run();
}

void Banco::GuardarMiniatura(const std::string& url,
                             const std::vector<uint8_t>& jpeg,
                             base::Time quando) {
  if (!Pronto() || jpeg.empty()) {
    return;
  }
  sql::Statement guardar(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO miniaturas(pagina, quando, imagem) "
      "SELECT id, ?, ? FROM paginas WHERE url = ?"));
  guardar.BindTime(0, quando);
  guardar.BindBlob(1, base::span(jpeg));
  guardar.BindString(2, url);
  std::ignore = guardar.Run();
}

bool Banco::TemMiniatura(const std::string& url, base::TimeDelta idade) {
  if (!Pronto()) {
    return false;
  }
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m.quando FROM miniaturas m JOIN paginas p ON p.id = m.pagina "
      "WHERE p.url = ?"));
  achar.BindString(0, url);
  return achar.Step() &&
         base::Time::Now() - achar.ColumnTime(0) < idade;
}

std::vector<Resultado> Banco::Buscar(const Filtro& filtro) {
  if (!Pronto()) {
    return {};
  }
  // Busca que já ficou pra trás (a pessoa digitou outra letra): nem roda.
  if (filtro.pedido && filtro.pedido->load(std::memory_order_relaxed) !=
                           filtro.numero_do_pedido) {
    return {};
  }
  if (filtro.consulta.empty()) {
    return Recentes(filtro);
  }
  const texto::Consulta consulta = texto::Montar(filtro.consulta);
  if (consulta.fts.empty()) {
    return {};
  }
  // CROSS JOIN: a busca do índice por fora, a tabela das páginas por dentro
  // (o SQLite não troca a ordem).
  const std::string sql = base::StrCat(
      {"SELECT p.id, p.url, p.host, p.titulo, p.primeira, p.ultima, "
       "p.visitas, p.segundos, p.espaco, p.container, ",
       kRelevancia,
       " FROM busca CROSS JOIN paginas p ON p.id = busca.rowid "
       "WHERE busca MATCH ? AND p.ultima >= ? AND (? = '' OR p.espaco = ?) "
       "ORDER BY ",
       kRelevancia, " LIMIT ", base::NumberToString(kCandidatos)});
  sql::Statement buscar(db_.GetUniqueStatement(sql));
  buscar.BindString(0, consulta.fts);
  buscar.BindTime(1, filtro.desde);
  buscar.BindString(2, filtro.espaco);
  buscar.BindString(3, filtro.espaco);

  struct Candidato {
    Resultado resultado;
    double peso;
  };
  std::vector<Candidato> candidatos;
  const base::Time agora = base::Time::Now();
  while (buscar.Step()) {
    Resultado r;
    r.id = buscar.ColumnInt64(0);
    r.url = buscar.ColumnString(1);
    r.host = buscar.ColumnString(2);
    r.titulo = buscar.ColumnString16(3);
    r.primeira = buscar.ColumnTime(4);
    r.ultima = buscar.ColumnTime(5);
    r.visitas = buscar.ColumnInt(6);
    r.segundos = buscar.ColumnInt(7);
    r.espaco = buscar.ColumnString(8);
    r.container = buscar.ColumnString(9);
    // O bm25 do FTS5 é negativo (mais negativo, mais relevante).
    const double texto = std::max(0.0, -buscar.ColumnDouble(10));
    const double dias =
        std::max(0.0, (agora - r.ultima).InSecondsF() / 86400.0);
    const double recente = 1.0 / (1.0 + dias / 30.0);
    const double peso = texto * (0.55 + 0.45 * recente) *
                        (1.0 + 0.15 * std::log1p(r.visitas)) *
                        (1.0 + 0.1 * std::log1p(r.segundos / 30.0));
    candidatos.push_back({std::move(r), peso});
  }
  std::ranges::stable_sort(candidatos, std::ranges::greater(),
                           &Candidato::peso);
  if (candidatos.size() > filtro.limite) {
    candidatos.resize(filtro.limite);
  }
  std::vector<Resultado> resultados;
  resultados.reserve(candidatos.size());
  for (Candidato& candidato : candidatos) {
    resultados.push_back(std::move(candidato.resultado));
  }
  Completar(resultados, consulta.termos, filtro.tamanho_do_trecho);
  return resultados;
}

std::vector<Resultado> Banco::Recentes(const Filtro& filtro) {
  sql::Statement recentes(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, url, host, titulo, primeira, ultima, visitas, segundos, "
      "espaco, container FROM paginas "
      "WHERE ultima >= ? AND (? = '' OR espaco = ?) "
      "ORDER BY ultima DESC LIMIT ?"));
  recentes.BindTime(0, filtro.desde);
  recentes.BindString(1, filtro.espaco);
  recentes.BindString(2, filtro.espaco);
  recentes.BindInt64(3, static_cast<int64_t>(filtro.limite));
  std::vector<Resultado> resultados;
  while (recentes.Step()) {
    Resultado r;
    r.id = recentes.ColumnInt64(0);
    r.url = recentes.ColumnString(1);
    r.host = recentes.ColumnString(2);
    r.titulo = recentes.ColumnString16(3);
    r.primeira = recentes.ColumnTime(4);
    r.ultima = recentes.ColumnTime(5);
    r.visitas = recentes.ColumnInt(6);
    r.segundos = recentes.ColumnInt(7);
    r.espaco = recentes.ColumnString(8);
    r.container = recentes.ColumnString(9);
    resultados.push_back(std::move(r));
  }
  Completar(resultados, {}, filtro.tamanho_do_trecho);
  return resultados;
}

void Banco::Completar(std::vector<Resultado>& resultados,
                      const std::vector<texto::Termo>& termos,
                      size_t tamanho) {
  // Com busca, o trecho pode estar em qualquer lugar do texto; sem (a linha
  // do tempo), é o começo dele.
  sql::Statement ler(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT CASE WHEN ? THEN p.texto ELSE substr(p.texto, 1, ?) END, "
      "m.pagina IS NOT NULL FROM paginas p "
      "LEFT JOIN miniaturas m ON m.pagina = p.id WHERE p.id = ?"));
  for (Resultado& r : resultados) {
    ler.BindBool(0, !termos.empty());
    ler.BindInt(1, kComecoDoTexto);
    ler.BindInt64(2, r.id);
    if (ler.Step()) {
      r.trecho = texto::Recortar(ler.ColumnString16(0), termos, tamanho);
      r.tem_miniatura = ler.ColumnBool(1);
    }
    ler.Reset(/*clear_bound_vars=*/true);
  }
}

std::vector<uint8_t> Banco::Miniatura(int64_t id) {
  if (!Pronto()) {
    return {};
  }
  sql::Statement ler(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT imagem FROM miniaturas WHERE pagina = ?"));
  ler.BindInt64(0, id);
  return ler.Step() ? ler.ColumnBlobAsVector(0) : std::vector<uint8_t>();
}

void Banco::Apagar(const std::vector<int64_t>& ids) {
  if (!ids.empty() && !sujo_) {
    sujo_ = true;
    std::ignore = meta_.SetValue(kChaveSujo, 1);
  }
  for (size_t comeco = 0; comeco < ids.size(); comeco += kLote) {
    sql::Transaction transacao(&db_);
    if (!transacao.Begin()) {
      return;
    }
    const size_t fim = std::min(ids.size(), comeco + kLote);
    for (size_t i = comeco; i < fim; ++i) {
      sql::Statement busca(db_.GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM busca WHERE rowid = ?"));
      busca.BindInt64(0, ids[i]);
      sql::Statement miniatura(db_.GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM miniaturas WHERE pagina = ?"));
      miniatura.BindInt64(0, ids[i]);
      sql::Statement pagina(db_.GetCachedStatement(
          SQL_FROM_HERE, "DELETE FROM paginas WHERE id = ?"));
      pagina.BindInt64(0, ids[i]);
      if (!busca.Run() || !miniatura.Run() || !pagina.Run()) {
        return;
      }
    }
    if (!transacao.Commit()) {
      return;
    }
  }
}

void Banco::EsquecerPagina(int64_t id) {
  if (Pronto()) {
    Apagar({id});
  }
}

void Banco::EsquecerEnderecos(const std::vector<std::string>& urls) {
  if (!Pronto()) {
    return;
  }
  std::vector<int64_t> ids;
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM paginas WHERE url = ?"));
  for (const std::string& url : urls) {
    achar.BindString(0, url);
    if (achar.Step()) {
      ids.push_back(achar.ColumnInt64(0));
    }
    achar.Reset(/*clear_bound_vars=*/true);
  }
  Apagar(ids);
}

void Banco::EsquecerEnderecosNoPeriodo(const std::vector<std::string>& urls,
                                       base::Time inicio,
                                       base::Time fim) {
  if (!Pronto()) {
    return;
  }
  std::vector<int64_t> ids;
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id FROM paginas WHERE url = ? AND ultima >= ? AND ultima < ?"));
  for (const std::string& url : urls) {
    achar.BindString(0, url);
    achar.BindTime(1, inicio);
    achar.BindTime(2, fim.is_null() ? base::Time::Max() : fim);
    if (achar.Step()) {
      ids.push_back(achar.ColumnInt64(0));
    }
    achar.Reset(/*clear_bound_vars=*/true);
  }
  Apagar(ids);
}

void Banco::EsquecerPeriodo(base::Time inicio, base::Time fim) {
  if (!Pronto()) {
    return;
  }
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id FROM paginas WHERE ultima >= ? AND ultima < ?"));
  achar.BindTime(0, inicio);
  achar.BindTime(1, fim.is_null() ? base::Time::Max() : fim);
  std::vector<int64_t> ids;
  while (achar.Step()) {
    ids.push_back(achar.ColumnInt64(0));
  }
  Apagar(ids);
}

void Banco::EsquecerSite(const std::string& host) {
  if (!Pronto() || host.empty()) {
    return;
  }
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id FROM paginas WHERE host = ? OR "
      "(length(host) > length(?) AND substr(host, -length(?) - 1) = ?)"));
  achar.BindString(0, host);
  achar.BindString(1, host);
  achar.BindString(2, host);
  achar.BindString(3, "." + host);
  std::vector<int64_t> ids;
  while (achar.Step()) {
    ids.push_back(achar.ColumnInt64(0));
  }
  Apagar(ids);
}

void Banco::EsquecerContainer(const std::string& container) {
  if (!Pronto() || container.empty()) {
    return;
  }
  sql::Statement achar(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM paginas WHERE container = ?"));
  achar.BindString(0, container);
  std::vector<int64_t> ids;
  while (achar.Step()) {
    ids.push_back(achar.ColumnInt64(0));
  }
  Apagar(ids);
}

void Banco::EsquecerTudo() {
  if (!Pronto()) {
    return;
  }
  sujo_ = false;
  {
    sql::Transaction transacao(&db_);
    if (!transacao.Begin() ||
        !db_.Execute("INSERT INTO busca(busca) VALUES('delete-all')") ||
        !db_.Execute("DELETE FROM miniaturas") ||
        !db_.Execute("DELETE FROM paginas") || !transacao.Commit()) {
      return;
    }
  }
  // O arquivo volta ao tamanho de vazio (o apagado já foi zerado: o
  // SQLite do Chromium usa secure_delete).
  std::ignore = db_.Execute("VACUUM");
}

void Banco::Podar(base::Time antes_de,
                  base::Time miniaturas_antes_de,
                  size_t maximo_de_miniaturas) {
  if (!Pronto()) {
    return;
  }
  if (!antes_de.is_null()) {
    EsquecerPeriodo(base::Time(), antes_de);
  }
  sql::Statement velhas(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM miniaturas WHERE quando < ?"));
  velhas.BindTime(0, miniaturas_antes_de);
  std::ignore = velhas.Run();
  sql::Statement demais(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM miniaturas WHERE pagina NOT IN ("
      "SELECT pagina FROM miniaturas ORDER BY quando DESC LIMIT ?)"));
  demais.BindInt64(0, static_cast<int64_t>(maximo_de_miniaturas));
  std::ignore = demais.Run();
  Limpar();
}

void Banco::Limpar() {
  if (!aberto_ || !sujo_) {
    return;
  }
  sujo_ = false;
  // O índice é "sem conteúdo": apagar uma página só marca a linha como
  // apagada, e as palavras dela continuam no arquivo até as partes do índice
  // se juntarem. O 'optimize' junta tudo agora, e o que foi apagado sai de
  // verdade (o SQLite do Chromium zera as páginas livres: secure_delete).
  if (db_.Execute("INSERT INTO busca(busca) VALUES('optimize')")) {
    std::ignore = meta_.SetValue(kChaveSujo, 0);
  }
}

Resumo Banco::Resumir() {
  Resumo resumo;
  if (!Pronto()) {
    return resumo;
  }
  sql::Statement contar(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*), MIN(primeira) FROM paginas"));
  if (contar.Step()) {
    resumo.paginas = contar.ColumnInt64(0);
    if (resumo.paginas > 0) {
      resumo.mais_antiga = contar.ColumnTime(1);
    }
  }
  resumo.bytes = base::GetFileSize(arquivo_).value_or(0);
  return resumo;
}

}  // namespace navegador::memoria
