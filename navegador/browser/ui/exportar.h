// Exportar e importar o jeito do Norte num arquivo JSON: espaços, sites
// fixados e favoritos, containers, atalhos e as opções de aparência e da
// lateral. Sem conta do Google não há sincronização: assim dá pra levar pra
// outra máquina ou guardar (num repositório de dotfiles, por exemplo). Fica
// de fora a ligação dos grupos de abas com os espaços (só vale nesta
// máquina) e o que é do Chromium (favoritos, senhas, histórico).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_EXPORTAR_H_
#define NAVEGADOR_BROWSER_UI_EXPORTAR_H_

#include <string>

#include "base/functional/callback.h"

class BrowserWindowInterface;

namespace navegador::exportar {

// `ok`: gravou ou aplicou o arquivo. `frase`: o que mostrar.
using Resposta =
    base::OnceCallback<void(bool ok, const std::u16string& frase)>;

// Pergunta onde salvar e grava. `resposta` recebe uma frase pra mostrar (ou
// nada, se a pessoa cancelou).
void Exportar(BrowserWindowInterface* browser, Resposta resposta);

// Pergunta o arquivo e aplica (substitui os espaços, fixados e containers
// deste perfil). `resposta` recebe uma frase.
void Importar(BrowserWindowInterface* browser, Resposta resposta);

}  // namespace navegador::exportar

#endif  // NAVEGADOR_BROWSER_UI_EXPORTAR_H_
