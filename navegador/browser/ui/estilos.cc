#include "navegador/browser/ui/estilos.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "url/gurl.h"

namespace navegador::estilos {

namespace {

// As do perfil normal: a anônima lê as mesmas (e não grava nada).
PrefService* Prefs(Profile* perfil) {
  return perfil->GetOriginalProfile()->GetPrefs();
}

}  // namespace

std::string Chave(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || !url.has_host()) {
    return std::string();
  }
  std::string host = base::ToLowerASCII(url.host());
  if (host.starts_with("www.")) {
    host.erase(0, 4);
  }
  return host;
}

std::string CssDoSite(Profile* perfil, const GURL& url) {
  const std::string chave = Chave(url);
  if (chave.empty()) {
    return std::string();
  }
  const std::string* css =
      Prefs(perfil)->GetDict(espacos::kEstilosCssPref).FindString(chave);
  return css ? *css : std::string();
}

bool EscuroNoSite(Profile* perfil, const GURL& url) {
  const std::string chave = Chave(url);
  return !chave.empty() && Prefs(perfil)
                               ->GetDict(espacos::kEstilosEscuroPref)
                               .FindBool(chave)
                               .value_or(false);
}

void Definir(Profile* perfil,
             const std::string& chave,
             const std::string& css,
             bool escuro) {
  if (chave.empty()) {
    return;
  }
  {
    ScopedDictPrefUpdate estilos(Prefs(perfil), espacos::kEstilosCssPref);
    if (base::TrimWhitespaceASCII(css, base::TRIM_ALL).empty()) {
      estilos->Remove(chave);
    } else {
      estilos->Set(chave, css);
    }
  }
  ScopedDictPrefUpdate escuros(Prefs(perfil), espacos::kEstilosEscuroPref);
  if (escuro) {
    escuros->Set(chave, true);
  } else {
    escuros->Remove(chave);
  }
}

}  // namespace navegador::estilos
