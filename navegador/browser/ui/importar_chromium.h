// Trazer de um navegador feito do Chromium (Chrome, Chromium, Brave,
// Vivaldi, Edge): favoritos e histórico. O importador do Chromium no Linux só
// conhece o Firefox e o arquivo de favoritos. Lê de uma cópia do perfil, então
// funciona com o outro navegador aberto. Senhas e cookies ficam de fora: o
// Chrome cifra os dois com a chave dele no chaveiro do sistema.
//
// Os favoritos entram numa pasta "Do <navegador>" em "Outros favoritos"; o
// histórico, como visitas importadas (sem apagar nada daqui).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_IMPORTAR_CHROMIUM_H_
#define NAVEGADOR_BROWSER_UI_IMPORTAR_CHROMIUM_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

class Profile;

namespace navegador::importar_chromium {

struct Origem {
  std::u16string nome;    // "Google Chrome"
  base::FilePath perfil;  // a pasta Default do navegador
};

// Os navegadores achados nesta máquina (os perfis Default deles, também os
// do Flatpak), procurados numa tarefa de fundo.
void Procurar(base::OnceCallback<void(std::vector<Origem>)> resposta);

struct Resumo {
  bool ok = false;
  std::u16string erro;
  int favoritos = 0;
  int historico = 0;
};

void Importar(Profile* perfil,
              const Origem& origem,
              base::OnceCallback<void(const Resumo&)> pronto);

}  // namespace navegador::importar_chromium

#endif  // NAVEGADOR_BROWSER_UI_IMPORTAR_CHROMIUM_H_
