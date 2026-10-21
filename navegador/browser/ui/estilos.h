// Estilos por site (fase 1): um CSS próprio e o modo escuro automático do
// Blink, cada um só no site escolhido. Clique direito na página → "Estilo
// deste site…" (o editor fica nas Configurações, configuracoes.h). O CSS
// entra no começo de cada página do site, como folha do autor depois das do
// site (bloqueador.cc manda junto com as regras do bloqueador); o modo
// escuro vai pelas preferências da página
// (ChromeContentBrowserClient::OverrideWebPreferences).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_ESTILOS_H_
#define NAVEGADOR_BROWSER_UI_ESTILOS_H_

#include <string>

class GURL;
class Profile;

namespace navegador::estilos {

// A chave do site: o host, sem "www." (vazio fora de http/https).
std::string Chave(const GURL& url);

// O CSS que a pessoa escreveu pro site de `url` (vazio se nenhum).
std::string CssDoSite(Profile* perfil, const GURL& url);

// O modo escuro do Blink está ligado no site de `url`?
bool EscuroNoSite(Profile* perfil, const GURL& url);

// Grava o estilo do site `chave` (CSS vazio e sem modo escuro: apaga).
void Definir(Profile* perfil,
             const std::string& chave,
             const std::string& css,
             bool escuro);

}  // namespace navegador::estilos

#endif  // NAVEGADOR_BROWSER_UI_ESTILOS_H_
