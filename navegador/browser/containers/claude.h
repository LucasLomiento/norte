// A extensão do Claude (Claude in Chrome) no Norte. Ela mexe nas abas pelo
// depurador do Chromium (chrome.debugger), e o Chromium avisa com uma tarja
// preta em todas as abas ("começou a depurar este navegador"). Pra ela, e só
// pra ela (pelo id da loja: outra extensão não se passa pelo Claude), o aviso
// é o cartão do Norte no pé da lateral (navegador/browser/ui/claude.h), com
// "Parar" e o container das abas que ela abre. As outras extensões que usam o
// depurador continuam com a tarja.
//
// As abas que ela abre (chrome.tabs.create) nascem no container escolhido pro
// Claude (Configurações → Containers ou o cartão), ou no do espaço em que a
// pessoa está.
//
// Sem interface: compilado no //navegador/browser/containers, que o
// debugger_api.cc e o open_tab_helper.cc enxergam.

#ifndef NAVEGADOR_BROWSER_CONTAINERS_CLAUDE_H_
#define NAVEGADOR_BROWSER_CONTAINERS_CLAUDE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace navegador::claude {

// A extensão na Chrome Web Store.
inline constexpr char kIdDaExtensao[] = "fcoeoabgfenejglbffodgkkbkcdhcgfn";
// Container das abas que o Claude abre: vazio = o do espaço em que a pessoa
// está; kSemContainer; ou o id do container.
inline constexpr char kContainerPref[] = "navegador.claude.container";
inline constexpr char kSemContainer[] = "*nenhum*";
// O laranja do Claude: o espaço dele e o cartão.
inline constexpr char kCor[] = "#D97757";

bool EDoClaude(std::string_view extension_id);
// O grupo de abas que a extensão cria (e o espaço temporário dele) se chama
// "Claude".
bool NomeDoClaude(std::string_view nome);

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// nada = o do espaço em que a pessoa está (também se o escolhido foi
// excluído); "" = sem container; senão o id.
std::optional<std::string> Container(Profile* profile);
void DefinirContainer(Profile* profile,
                      const std::optional<std::string>& container);

// open_tab_helper.cc: enquanto vive, a aba que nasce foi pedida pelo Claude.
class AbaDoClaude {
 public:
  AbaDoClaude();
  AbaDoClaude(const AbaDoClaude&) = delete;
  AbaDoClaude& operator=(const AbaDoClaude&) = delete;
  ~AbaDoClaude();
};
bool CriandoAbaDoClaude();

// debugger_api.cc: uma sessão do depurador do Claude começou. `parar`
// desconecta essa sessão (o "Parar" do cartão, como o "Cancelar" da tarja).
// Soltar a inscrição = a sessão acabou.
base::CallbackListSubscription ComecouUso(base::OnceClosure parar);
// O Claude está usando o navegador: do começo da primeira sessão até 5 s
// depois da última (como a tarja, que fica um pouco depois de acabar).
bool EmUso();
base::CallbackListSubscription ObservarUso(base::RepeatingClosure mudou);
// Desconecta todas as sessões.
void Parar();

}  // namespace navegador::claude

#endif  // NAVEGADOR_BROWSER_CONTAINERS_CLAUDE_H_
