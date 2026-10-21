// Comandos do Norte na barra de endereço (">" e o nome da ação, ver
// navegador/browser/omnibox/navegador_provider.h): a lista que a barra
// mostra e quem roda o escolhido. A lista junta as ações dos atalhos
// (menos "ir pro espaço N", que vira um por espaço, pelo nome), os
// containers e algumas do menu do pé da lateral.
//
// Compilado dentro de //chrome/browser/ui:ui; chamado pelo
// ChromeAutocompleteProviderClient.

#ifndef NAVEGADOR_BROWSER_UI_COMANDOS_H_
#define NAVEGADOR_BROWSER_UI_COMANDOS_H_

#include <string>
#include <vector>

#include "components/omnibox/browser/autocomplete_provider_client.h"

class Profile;

namespace navegador::comandos {

std::vector<NavegadorComando> Lista(Profile* perfil);

// Roda na janela ativa do perfil (a da barra). "copiar:<texto>" copia (a
// conta da calculadora).
void Executar(Profile* perfil, const std::string& id);

}  // namespace navegador::comandos

#endif  // NAVEGADOR_BROWSER_UI_COMANDOS_H_
