// Banco da Memória (memoria.h): um SQLite no perfil ("Norte Memoria") com o
// texto das páginas lidas e um índice FTS5 por cima dele. Roda numa
// sequência de fundo (memoria.cc); nada aqui fala com a interface.
//
// Uma linha por endereço (sem o fragmento e sem os parâmetros de
// rastreio): voltar à página atualiza o texto e conta mais uma visita.
//
// Só depende do //base e do //sql: os testes rodam sem o navegador
// (navegador_memoria_unittests).

#ifndef NAVEGADOR_BROWSER_MEMORIA_BANCO_H_
#define NAVEGADOR_BROWSER_MEMORIA_BANCO_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "navegador/browser/memoria/texto.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace navegador::memoria {

// Uma página lida, como a captura entrega.
struct Leitura {
  // Endereço sem fragmento e sem rastreio (a chave).
  std::string url;
  std::string host;
  std::u16string titulo;
  // Já limpo (texto::Limpar).
  std::u16string texto;
  // Espaço e container da aba (ids; vazio = nenhum).
  std::string espaco;
  std::string container;
  base::Time quando;
  // Conta uma visita. A releitura de quando a pessoa sai da aba (pra pegar
  // o que a página carregou depois) não conta.
  bool visita = true;
};

struct Resultado {
  int64_t id = 0;
  std::string url;
  std::string host;
  std::u16string titulo;
  base::Time primeira;
  base::Time ultima;
  int visitas = 0;
  // Tempo com a página à vista, somado.
  int segundos = 0;
  std::string espaco;
  std::string container;
  bool tem_miniatura = false;
  texto::Trecho trecho;
};

struct Filtro {
  // Vazio: as páginas mais recentes (a linha do tempo).
  std::u16string consulta;
  // Nulo: desde sempre.
  base::Time desde;
  // Vazio: todos os espaços.
  std::string espaco;
  size_t limite = 40;
  size_t tamanho_do_trecho = 180;
};

struct Resumo {
  int64_t paginas = 0;
  int64_t bytes = 0;
  base::Time mais_antiga;
};

class Banco {
 public:
  explicit Banco(base::FilePath arquivo);
  Banco(const Banco&) = delete;
  Banco& operator=(const Banco&) = delete;
  ~Banco();

  // Abre (ou cria) o arquivo. Arquivo estragado é apagado e começa de novo:
  // a memória se refaz com as próximas leituras. Falso se nem assim deu (o
  // resto vira nada).
  bool Abrir();

  void Lembrar(const Leitura& leitura);
  void SomarTempo(const std::string& url, int segundos);
  // JPEG da página à vista. Só fica se a página já está no banco.
  void GuardarMiniatura(const std::string& url,
                        const std::vector<uint8_t>& jpeg,
                        base::Time quando);
  // Já tem miniatura (de menos de `idade`)? Pra não tirar outra a cada volta.
  bool TemMiniatura(const std::string& url, base::TimeDelta idade);

  std::vector<Resultado> Buscar(const Filtro& filtro);
  std::vector<uint8_t> Miniatura(int64_t id);

  void EsquecerPagina(int64_t id);
  void EsquecerEnderecos(const std::vector<std::string>& urls);
  // As vistas pela última vez em [inicio, fim).
  void EsquecerPeriodo(base::Time inicio, base::Time fim);
  // O site e os subdomínios.
  void EsquecerSite(const std::string& host);
  void EsquecerContainer(const std::string& container);
  void EsquecerTudo();

  // Apaga as páginas não vistas desde `antes_de` (nulo: fica tudo) e as
  // miniaturas mais velhas que `miniaturas_antes_de` ou além das
  // `maximo_de_miniaturas` mais novas.
  void Podar(base::Time antes_de,
             base::Time miniaturas_antes_de,
             size_t maximo_de_miniaturas);

  Resumo Resumir();

 private:
  bool CriarTabelas();
  void Apagar(const std::vector<int64_t>& ids);
  std::vector<Resultado> Recentes(const Filtro& filtro);
  void Completar(std::vector<Resultado>& resultados,
                 const std::vector<texto::Termo>& termos,
                 size_t tamanho);

  const base::FilePath arquivo_;
  sql::Database db_;
  sql::MetaTable meta_;
  bool aberto_ = false;
};

}  // namespace navegador::memoria

#endif  // NAVEGADOR_BROWSER_MEMORIA_BANCO_H_
