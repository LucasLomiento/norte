// Sugestões do Norte na barra de endereço:
//   - Comandos: ">" e o nome da ação ("> foco", "> espaço trabalho",
//     "> dividir") mostram as ações do Norte (as dos atalhos, os espaços, os
//     containers e algumas do menu do pé da lateral); Enter roda a primeira.
//     A lista vem do cliente (AutocompleteProviderClient::NavegadorComandos,
//     do lado do Chrome) e quem roda é o NavegadorExecutar.
//   - Calculadora: uma conta ("2+3*4", "(10-2)/3", "15% de 80", "2^10") dá o
//     resultado na lista, calculado aqui (a do Chromium só repete o que o
//     buscador manda). Escolher copia o resultado.
//   - Memória: o que se digita acha também o texto das páginas já lidas
//     (navegador/browser/memoria/memoria.h, pelo cliente:
//     NavegadorLembrar). Até 3, com o trecho e as palavras em negrito;
//     escolher abre a página no trecho. Shift+Delete esquece a página. E,
//     embaixo delas, "Buscar … na Memória", que abre o painel com o texto.
//
// Compilado dentro de //components/omnibox/browser.

#ifndef NAVEGADOR_BROWSER_OMNIBOX_NAVEGADOR_PROVIDER_H_
#define NAVEGADOR_BROWSER_OMNIBOX_NAVEGADOR_PROVIDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"

class AutocompleteInput;
class AutocompleteProviderClient;

class NavegadorProvider : public AutocompleteProvider {
 public:
  explicit NavegadorProvider(AutocompleteProviderClient* client);
  NavegadorProvider(const NavegadorProvider&) = delete;
  NavegadorProvider& operator=(const NavegadorProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

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
  void Memoria(const AutocompleteInput& input);
  void Lembrou(int pedido,
               std::u16string texto,
               std::vector<NavegadorLembranca> lembrancas);

  raw_ptr<AutocompleteProviderClient> client_;
  // Conta as buscas na Memória: resposta de uma busca velha (o texto mudou,
  // a barra parou) não entra.
  int pedido_ = 0;
  base::WeakPtrFactory<NavegadorProvider> weak_factory_{this};
};

#endif  // NAVEGADOR_BROWSER_OMNIBOX_NAVEGADOR_PROVIDER_H_
