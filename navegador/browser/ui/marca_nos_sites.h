// O nome do navegador nos sites que escrevem "Chrome" pra todo Chromium: os
// botões da Chrome Web Store ("Usar no Chrome" vira "Usar no Norte"). Vai
// junto com as regras do bloqueador (bloqueador.cc, Escondedor) e roda num
// mundo isolado no começo do documento, valendo mesmo com o bloqueador
// desligado.

#ifndef NAVEGADOR_BROWSER_UI_MARCA_NOS_SITES_H_
#define NAVEGADOR_BROWSER_UI_MARCA_NOS_SITES_H_

#include <string>

class GURL;

namespace navegador::marca_nos_sites {

// O script pra página principal em `url`, ou vazio se o site não é um deles.
std::string JsDoSite(const GURL& url);

}  // namespace navegador::marca_nos_sites

#endif  // NAVEGADOR_BROWSER_UI_MARCA_NOS_SITES_H_
