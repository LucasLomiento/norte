// Atualizador: ao abrir o navegador (o primeiro processo depois de fechar
// todas as janelas) e depois a cada 6 h, procura a última versão nas
// Releases do GitHub (LucasLomiento/norte-releases) e, se for mais nova que a
// instalada,
// mostra um aviso no pé da lateral. "Atualizar" (no aviso ou nas
// Configurações) baixa o pacote novo, instala com o pacman (pela janela de
// senha do sistema, pkexec) e oferece reiniciar.
//
// Também percebe a versão nova instalada por fora (sudo pacman -Syu) com o
// navegador aberto: o VERSAO da pasta muda, e o aviso pede pra reiniciar (o
// processo aberto segue no binário antigo até lá, mesmo abrindo outra
// janela).
//
// A versão instalada vem do arquivo VERSAO ao lado do executável, que o
// pacote grava ("155.0.8059.79-9"); sem ele (compilação de desenvolvimento)
// não procura nada.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_ATUALIZADOR_H_
#define NAVEGADOR_BROWSER_UI_ATUALIZADOR_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class View;
}  // namespace views

namespace navegador::atualizador {

// Releases públicas (só os pacotes; o código fica no repositório privado).
// (Era navegador-releases: o GitHub redireciona o nome antigo, então as
// versões anteriores ao Norte continuam achando as novas.)
inline constexpr char kRepositorio[] = "LucasLomiento/norte-releases";

// Em que pé está a atualização (uma só pro processo, em todas as janelas).
enum class Etapa {
  kNada,        // Sem versão nova (ou ainda sem procurar).
  kDisponivel,  // Versão nova no GitHub.
  kBaixando,    // Baixando o pacote.
  kInstalando,  // O pacman, com a senha na janela do sistema.
  kInstalada,   // Instalada (por nós ou por fora): falta reiniciar.
  kErro,        // Não deu pra baixar ou instalar.
};

struct Andamento {
  Etapa etapa = Etapa::kNada;
  // A versão nova ("155.0.8059.79-39"), fora do kNada.
  std::string nova;
  // Baixando: quanto já veio, de 0 a 100 (-1 sem o tamanho do pacote).
  int porcento = -1;
  // kErro: o que deu errado, pra mostrar.
  std::u16string erro;
  // A página da versão nova nas Releases ("O que mudou"), fora do kNada.
  std::string pagina;
};

// Aviso no pé da lateral (escondido enquanto não há versão nova). A primeira
// janela do processo também dispara a procura, se estiver ligada.
std::unique_ptr<views::View> CriarAviso(BrowserWindowInterface* browser);

// "Procurar agora" das Configurações: `resposta` recebe uma frase. Achando
// versão nova, a etapa passa a kDisponivel (e avisa quem segue o AoMudar).
void ProcurarAgora(Profile* perfil,
                   base::OnceCallback<void(const std::u16string&)> resposta);

// Versão instalada, pra mostrar ("" fora do pacote). É a do processo aberto:
// a instalada por fora depois de abrir aparece como kInstalada.
void LerVersaoInstalada(base::OnceCallback<void(const std::string&)> resposta);

Andamento Agora();

// `mudou` roda a cada mudança do Andamento (etapa, porcentagem...).
base::CallbackListSubscription AoMudar(base::RepeatingClosure mudou);

// Baixa e instala a versão nova (kDisponivel ou kErro; nas outras etapas não
// faz nada).
void Atualizar();

// Fecha e abre de novo o navegador inteiro (todas as janelas, as abas
// voltam), já no binário novo.
void Reiniciar();

}  // namespace navegador::atualizador

#endif  // NAVEGADOR_BROWSER_UI_ATUALIZADOR_H_
