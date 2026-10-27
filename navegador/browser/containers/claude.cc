#include "navegador/browser/containers/claude.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "navegador/browser/containers/containers.h"

namespace navegador::claude {

namespace {

// Quantas AbaDoClaude estão vivas (Navigate pode criar outra aba dentro).
int g_criando = 0;

// Como a tarja do Chromium: fica um pouco depois da última sessão (a
// extensão desliga e liga o depurador entre um passo e outro).
constexpr base::TimeDelta kFolgaDepoisDoUso = base::Seconds(5);

struct Uso {
  Uso() {
    paradas.set_removal_callback(base::BindRepeating(&Uso::SessaoSaiu));
  }

  static void SessaoSaiu();

  // Uma por sessão do depurador.
  base::OnceClosureList paradas;
  base::RepeatingClosureList observadores;
  bool em_uso = false;
  base::OneShotTimer folga;
};

Uso& Estado() {
  static base::NoDestructor<Uso> uso;
  return *uso;
}

void Uso::SessaoSaiu() {
  Uso& uso = Estado();
  if (!uso.paradas.empty() || !uso.em_uso) {
    return;
  }
  uso.folga.Start(FROM_HERE, kFolgaDepoisDoUso, base::BindOnce([] {
                    Uso& uso = Estado();
                    if (uso.paradas.empty()) {
                      uso.em_uso = false;
                      uso.observadores.Notify();
                    }
                  }));
}

}  // namespace

bool EDoClaude(std::string_view extension_id) {
  return extension_id == kIdDaExtensao;
}

bool NomeDoClaude(std::string_view nome) {
  return base::StartsWith(nome, "Claude", base::CompareCase::INSENSITIVE_ASCII);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kContainerPref, std::string());
}

std::optional<std::string> Container(Profile* profile) {
  Profile* original = profile->GetOriginalProfile();
  const std::string& valor = original->GetPrefs()->GetString(kContainerPref);
  if (valor.empty()) {
    return std::nullopt;
  }
  if (valor == kSemContainer) {
    return std::string();
  }
  if (!containers::Get(original, valor)) {
    return std::nullopt;
  }
  return valor;
}

void DefinirContainer(Profile* profile,
                      const std::optional<std::string>& container) {
  profile->GetOriginalProfile()->GetPrefs()->SetString(
      kContainerPref, !container               ? std::string()
                      : container->empty() ? std::string(kSemContainer)
                                           : *container);
}

AbaDoClaude::AbaDoClaude() {
  ++g_criando;
}

AbaDoClaude::~AbaDoClaude() {
  --g_criando;
}

bool CriandoAbaDoClaude() {
  return g_criando > 0;
}

base::CallbackListSubscription ComecouUso(base::OnceClosure parar) {
  Uso& uso = Estado();
  uso.folga.Stop();
  base::CallbackListSubscription inscricao = uso.paradas.Add(std::move(parar));
  if (!uso.em_uso) {
    uso.em_uso = true;
    uso.observadores.Notify();
  }
  return inscricao;
}

bool EmUso() {
  return Estado().em_uso;
}

base::CallbackListSubscription ObservarUso(base::RepeatingClosure mudou) {
  return Estado().observadores.Add(std::move(mudou));
}

void Parar() {
  // Cada sessão se desconecta e solta a inscrição dela. Parado pela pessoa,
  // o cartão some na hora (sem a folga).
  Uso& uso = Estado();
  uso.paradas.Notify();
  if (uso.paradas.empty() && uso.em_uso) {
    uso.folga.Stop();
    uso.em_uso = false;
    uso.observadores.Notify();
  }
}

}  // namespace navegador::claude
