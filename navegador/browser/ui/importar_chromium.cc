#include "navegador/browser/ui/importar_chromium.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/service_access_type.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace navegador::importar_chromium {

namespace {

// O mesmo rótulo de métricas do importador do Firefox (o sql::Database pede
// um; aqui não se mede nada).
inline constexpr sql::Database::Tag kTag{"FirefoxImporter"};

// Até quantas páginas do histórico (as visitadas por último).
constexpr int kMaximoDoHistorico = 50000;

struct Lugar {
  const char16_t* nome;
  const char* pasta;  // relativa à casa
};

constexpr Lugar kLugares[] = {
    {u"Google Chrome", ".config/google-chrome"},
    {u"Google Chrome Beta", ".config/google-chrome-beta"},
    {u"Chromium", ".config/chromium"},
    {u"Brave", ".config/BraveSoftware/Brave-Browser"},
    {u"Vivaldi", ".config/vivaldi"},
    {u"Microsoft Edge", ".config/microsoft-edge"},
    // Flatpak.
    {u"Google Chrome", ".var/app/com.google.Chrome/config/google-chrome"},
    {u"Chromium", ".var/app/org.chromium.Chromium/config/chromium"},
    {u"Brave", ".var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser"},
};

std::vector<Origem> ProcurarAqui() {
  std::vector<Origem> achados;
  const base::FilePath casa = base::GetHomeDir();
  for (const Lugar& lugar : kLugares) {
    const base::FilePath perfil = casa.Append(lugar.pasta).Append("Default");
    if (base::PathExists(perfil.Append("Bookmarks")) ||
        base::PathExists(perfil.Append("History"))) {
      achados.push_back({lugar.nome, perfil});
    }
  }
  return achados;
}

// Um favorito ou uma pasta, como no arquivo Bookmarks.
struct No {
  std::u16string nome;
  GURL url;  // vazio: pasta
  std::vector<No> filhos;
};

void LerNo(const base::DictValue& dict, No& no) {
  if (const std::string* nome = dict.FindString("name")) {
    no.nome = base::UTF8ToUTF16(*nome);
  }
  const std::string* tipo = dict.FindString("type");
  if (tipo && *tipo == "url") {
    if (const std::string* url = dict.FindString("url")) {
      no.url = GURL(*url);
    }
    return;
  }
  if (const base::ListValue* filhos = dict.FindList("children")) {
    for (const base::Value& filho : *filhos) {
      if (const base::DictValue* d = filho.GetIfDict()) {
        No novo;
        LerNo(*d, novo);
        if (novo.url.is_valid() || !novo.filhos.empty()) {
          no.filhos.push_back(std::move(novo));
        }
      }
    }
  }
}

struct Dados {
  // Barra de favoritos e "Outros favoritos" do outro navegador.
  No barra;
  No outros;
  history::URLRows historico;
  std::u16string erro;
};

std::unique_ptr<Dados> Ler(base::FilePath perfil) {
  auto dados = std::make_unique<Dados>();
  std::string texto;
  if (base::ReadFileToString(perfil.Append("Bookmarks"), &texto)) {
    std::optional<base::Value> json =
        base::JSONReader::Read(texto, base::JSON_PARSE_RFC);
    const base::DictValue* raizes =
        json && json->is_dict() ? json->GetDict().FindDict("roots") : nullptr;
    if (raizes) {
      if (const base::DictValue* barra = raizes->FindDict("bookmark_bar")) {
        LerNo(*barra, dados->barra);
      }
      if (const base::DictValue* outros = raizes->FindDict("other")) {
        LerNo(*outros, dados->outros);
      }
    }
  }

  // O History fica travado com o navegador aberto: uma cópia (com o -wal e
  // o -journal, se houver).
  base::ScopedTempDir copia;
  if (base::PathExists(perfil.Append("History")) &&
      copia.CreateUniqueTempDir()) {
    for (const char* nome : {"History", "History-wal", "History-journal"}) {
      if (base::PathExists(perfil.Append(nome))) {
        base::CopyFile(perfil.Append(nome), copia.GetPath().Append(nome));
      }
    }
    sql::Database db(kTag);
    if (db.Open(copia.GetPath().Append("History"))) {
      sql::Statement linha(db.GetUniqueStatement(
          "SELECT url, title, visit_count, typed_count, last_visit_time, "
          "hidden FROM urls ORDER BY last_visit_time DESC LIMIT ?"));
      linha.BindInt(0, kMaximoDoHistorico);
      while (linha.Step()) {
        GURL url(linha.ColumnString(0));
        if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
          continue;
        }
        history::URLRow row(url);
        row.set_title(base::UTF8ToUTF16(linha.ColumnString(1)));
        row.set_visit_count(std::max(1, linha.ColumnInt(2)));
        row.set_typed_count(linha.ColumnInt(3));
        // Microssegundos desde 1601, como o base::Time do Chromium.
        row.set_last_visit(base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(linha.ColumnInt64(4))));
        row.set_hidden(linha.ColumnBool(5));
        dados->historico.push_back(std::move(row));
      }
    }
  }
  if (dados->barra.filhos.empty() && dados->outros.filhos.empty() &&
      dados->historico.empty()) {
    dados->erro = u"Não achei favoritos nem histórico nesse perfil.";
  }
  return dados;
}

int Copiar(bookmarks::BookmarkModel* modelo,
           const bookmarks::BookmarkNode* pasta,
           const No& no) {
  int total = 0;
  for (const No& filho : no.filhos) {
    if (filho.url.is_valid()) {
      modelo->AddURL(pasta, pasta->children().size(), filho.nome, filho.url);
      ++total;
    } else {
      const bookmarks::BookmarkNode* nova =
          modelo->AddFolder(pasta, pasta->children().size(), filho.nome);
      total += Copiar(modelo, nova, filho);
    }
  }
  return total;
}

}  // namespace

void Procurar(base::OnceCallback<void(std::vector<Origem>)> resposta) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ProcurarAqui), std::move(resposta));
}

void Importar(Profile* perfil,
              const Origem& origem,
              base::OnceCallback<void(const Resumo&)> pronto) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&Ler, origem.perfil),
      base::BindOnce(
          [](base::WeakPtr<Profile> perfil, std::u16string nome,
             base::OnceCallback<void(const Resumo&)> pronto,
             std::unique_ptr<Dados> dados) {
            Resumo resumo;
            if (!perfil) {
              return;
            }
            if (!dados->erro.empty()) {
              resumo.erro = dados->erro;
              std::move(pronto).Run(resumo);
              return;
            }
            bookmarks::BookmarkModel* modelo =
                BookmarkModelFactory::GetForBrowserContext(perfil.get());
            if (modelo && modelo->loaded() &&
                (!dados->barra.filhos.empty() ||
                 !dados->outros.filhos.empty())) {
              const bookmarks::BookmarkNode* outros = modelo->other_node();
              const bookmarks::BookmarkNode* pasta = modelo->AddFolder(
                  outros, outros->children().size(), u"Do " + nome);
              resumo.favoritos += Copiar(modelo, pasta, dados->barra);
              resumo.favoritos += Copiar(modelo, pasta, dados->outros);
            }
            if (history::HistoryService* historico =
                    HistoryServiceFactory::GetForProfile(
                        perfil.get(), ServiceAccessType::EXPLICIT_ACCESS);
                historico && !dados->historico.empty()) {
              // O mais perto que há de "importado de outro navegador".
              historico->AddPagesWithDetails(dados->historico,
                                             history::SOURCE_FIREFOX_IMPORTED);
              resumo.historico = dados->historico.size();
            }
            resumo.ok = true;
            std::move(pronto).Run(resumo);
          },
          perfil->GetWeakPtr(), origem.nome, std::move(pronto)));
}

}  // namespace navegador::importar_chromium
