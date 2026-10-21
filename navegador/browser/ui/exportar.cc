#include "navegador/browser/ui/exportar.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
#include "ui/base/base_window.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace navegador::exportar {

namespace {

// O que vai no arquivo: os espaços e o que é deles, os containers, os
// atalhos e as opções. Não vão a ligação grupo de abas -> espaço
// (navegador.espacos.grupos, só desta máquina) nem o espaço ativo.
constexpr const char* kPrefs[] = {
    "navegador.espacos.lista",
    "navegador.espacos.fixos",
    "navegador.espacos_container",
    "navegador.containers",
    "navegador.atalhos",
    "navegador.barra.cor_do_site",
    "navegador.barra.escondida",
    "navegador.inicio.hora",
    "navegador.inicio.data",
    "navegador.janela.barra_na_tela_cheia",
    "navegador.janela.botoes",
    "navegador.janela.moldura",
    "navegador.lateral.abrir_inteira",
    "navegador.lateral.esconder_depois_ms",
    "navegador.lateral.esconder_fora_ms",
    "navegador.lateral.flutuante.desfoque",
    "navegador.lateral.flutuante.opacidade",
    "navegador.lateral.mostrar_na_borda",
    "navegador.lateral.quadro_de_favoritos",
    "navegador.previa.shift_clique",
    "navegador.atualizacoes.procurar",
    "navegador.bloqueador.avisos_de_cookies",
    "navegador.estilos.css",
    "navegador.estilos.escuro",
};

// As opções que valem pro navegador inteiro (Local State: lidas antes de o
// perfil abrir, ver sem_ia.h), no mesmo arquivo.
constexpr const char* kPrefsLocais[] = {
    "navegador.congelar_outros_espacos",
    "navegador.rolagem_com_o_meio",
};

constexpr int kVersao = 1;

std::string Montar(PrefService* prefs) {
  base::DictValue valores;
  auto juntar = [&valores](PrefService* origem, const char* nome) {
    if (const PrefService::Preference* pref =
            origem ? origem->FindPreference(nome) : nullptr) {
      valores.Set(nome, pref->GetValue()->Clone());
    }
  };
  for (const char* nome : kPrefs) {
    juntar(prefs, nome);
  }
  for (const char* nome : kPrefsLocais) {
    juntar(g_browser_process->local_state(), nome);
  }
  base::DictValue arquivo;
  arquivo.Set("norte", kVersao);
  arquivo.Set("prefs", std::move(valores));
  return base::WriteJsonWithOptions(arquivo,
                                    base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or(std::string());
}

// Aplica o que o arquivo tem e estas versões conhecem. Devolve quantas
// opções entraram, ou nada se o arquivo não é do Norte.
std::optional<int> Aplicar(PrefService* prefs, const std::string& texto) {
  std::optional<base::Value> json =
      base::JSONReader::Read(texto, base::JSON_PARSE_RFC);
  const base::DictValue* arquivo = json ? json->GetIfDict() : nullptr;
  const base::DictValue* valores = arquivo && arquivo->FindInt("norte")
                                       ? arquivo->FindDict("prefs")
                                       : nullptr;
  if (!valores) {
    return std::nullopt;
  }
  int aplicadas = 0;
  auto aplicar = [valores, &aplicadas](PrefService* destino, const char* nome) {
    const base::Value* valor = valores->Find(nome);
    const PrefService::Preference* pref =
        destino ? destino->FindPreference(nome) : nullptr;
    if (valor && pref && valor->type() == pref->GetType()) {
      destino->Set(nome, *valor);
      ++aplicadas;
    }
  };
  for (const char* nome : kPrefs) {
    aplicar(prefs, nome);
  }
  for (const char* nome : kPrefsLocais) {
    aplicar(g_browser_process->local_state(), nome);
  }
  return aplicadas;
}

// Uma janela de arquivo (salvar ou abrir), que se apaga quando termina.
class Arquivo : public ui::SelectFileDialog::Listener {
 public:
  Arquivo(bool salvar, BrowserWindowInterface* browser, Resposta resposta)
      : salvar_(salvar),
        perfil_(browser->GetProfile()->GetOriginalProfile()->GetWeakPtr()),
        resposta_(std::move(resposta)) {
    content::WebContents* aba =
        browser->GetTabStripModel()->GetActiveWebContents();
    dialogo_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(aba));
    ui::SelectFileDialog::FileTypeInfo tipos;
    tipos.extensions = {{FILE_PATH_LITERAL("json")}};
    tipos.include_all_files = true;
    const base::FilePath pasta =
        DownloadPrefs::FromBrowserContext(perfil_.get())->DownloadPath();
    dialogo_->SelectFile(
        salvar ? ui::SelectFileDialog::SELECT_SAVEAS_FILE
               : ui::SelectFileDialog::SELECT_OPEN_FILE,
        salvar ? u"Exportar as configurações do Norte"
               : u"Importar as configurações do Norte",
        salvar ? pasta.Append(FILE_PATH_LITERAL("norte.json")) : pasta, &tipos,
        1, FILE_PATH_LITERAL("json"),
        browser->GetWindow() ? browser->GetWindow()->GetNativeWindow()
                             : gfx::NativeWindow());
  }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& escolhido, int) override {
    const base::FilePath caminho = escolhido.path();
    if (!perfil_) {
      Terminar(false, std::u16string());
      return;
    }
    if (salvar_) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(
              [](base::FilePath caminho, std::string texto) {
                return !texto.empty() && base::WriteFile(caminho, texto);
              },
              caminho, Montar(perfil_->GetPrefs())),
          base::BindOnce(
              [](Arquivo* eu, bool ok) {
                eu->Terminar(ok, ok ? u"Exportado: espaços, fixados, "
                                      u"containers, atalhos e opções."
                                    : u"Não deu pra gravar o arquivo.");
              },
              base::Unretained(this)));
      return;
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            [](base::FilePath caminho) {
              std::string texto;
              return base::ReadFileToString(caminho, &texto) ? texto
                                                             : std::string();
            },
            caminho),
        base::BindOnce(
            [](Arquivo* eu, std::string texto) {
              std::optional<int> aplicadas =
                  eu->perfil_ ? Aplicar(eu->perfil_->GetPrefs(), texto)
                              : std::nullopt;
              eu->Terminar(
                  aplicadas.has_value(),
                  aplicadas
                      ? u"Importado. Reinicie o Norte pra tudo valer nas "
                        u"janelas abertas."
                      : u"Esse arquivo não é das configurações do Norte.");
            },
            base::Unretained(this)));
  }

  void FileSelectionCanceled() override {
    Terminar(false, std::u16string());
  }

 private:
  ~Arquivo() override = default;

  void Terminar(bool ok, const std::u16string& frase) {
    if (resposta_ && !frase.empty()) {
      std::move(resposta_).Run(ok, frase);
    }
    dialogo_->ListenerDestroyed();
    delete this;
  }

  const bool salvar_;
  base::WeakPtr<Profile> perfil_;
  Resposta resposta_;
  scoped_refptr<ui::SelectFileDialog> dialogo_;
};

}  // namespace

void Exportar(BrowserWindowInterface* browser, Resposta resposta) {
  new Arquivo(/*salvar=*/true, browser, std::move(resposta));
}

void Importar(BrowserWindowInterface* browser, Resposta resposta) {
  new Arquivo(/*salvar=*/false, browser, std::move(resposta));
}

}  // namespace navegador::exportar
