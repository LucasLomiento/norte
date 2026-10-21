// Sugestões do Norte na barra de endereço:
//   - Comandos: ">" e o nome da ação ("> foco", "> espaço trabalho",
//     "> dividir") mostram as ações do Norte (as dos atalhos, os espaços, os
//     containers e algumas do menu do pé da lateral); Enter roda a primeira.
//     A lista vem do cliente (AutocompleteProviderClient::NavegadorComandos,
//     do lado do Chrome) e quem roda é o NavegadorExecutar.
//   - Calculadora: uma conta ("2+3*4", "(10-2)/3", "15% de 80", "2^10") dá o
//     resultado na lista, calculado aqui (a do Chromium só repete o que o
//     buscador manda). Escolher copia o resultado.
//
// Compilado dentro de //components/omnibox/browser.

#ifndef NAVEGADOR_BROWSER_OMNIBOX_NAVEGADOR_PROVIDER_H_
#define NAVEGADOR_BROWSER_OMNIBOX_NAVEGADOR_PROVIDER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class AutocompleteProviderClient;

class NavegadorProvider : public AutocompleteProvider {
 public:
  explicit NavegadorProvider(AutocompleteProviderClient* client);
  NavegadorProvider(const NavegadorProvider&) = delete;
  NavegadorProvider& operator=(const NavegadorProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // O resultado da conta de `texto` escrito, ou nada se não é uma conta.
  // Decimal com vírgula se a conta usou vírgula, com ponto se usou ponto e,
  // sem nenhum, com vírgula se `virgula_padrao` (português). Público pros
  // testes.
  static std::optional<std::u16string> Calcular(std::u16string_view texto,
                                                bool virgula_padrao);

 private:
  ~NavegadorProvider() override;

  void Comandos(const AutocompleteInput& input, std::u16string_view busca);
  void Calculadora(const AutocompleteInput& input);

  raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // NAVEGADOR_BROWSER_OMNIBOX_NAVEGADOR_PROVIDER_H_
