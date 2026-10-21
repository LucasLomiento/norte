// Links limpos: sem os parâmetros que só servem pra rastrear de onde a pessoa
// veio (utm_*, fbclid, gclid...). O Ctrl+Shift+C copia o endereço da aba
// assim, e o menu do link tem "Copiar link limpo" e "Copiar link como
// Markdown" (menu_da_pagina.h). A lista é nossa, curta: só parâmetros que
// nunca mudam a página.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_LINKS_H_
#define NAVEGADOR_BROWSER_UI_LINKS_H_

#include <string>

class BrowserWindowInterface;
class GURL;

namespace navegador::links {

// `url` sem os parâmetros de rastreio (o resto, na mesma ordem, e o
// fragmento ficam).
GURL Limpar(const GURL& url);

// "[texto](endereço)", com o texto e o endereço escapados pro Markdown.
std::u16string ComoMarkdown(const std::u16string& texto, const GURL& url);

// Copia `texto` e mostra o aviso "Link copiado" na janela, como o Chromium
// faz no "Copiar endereço".
void Copiar(BrowserWindowInterface* browser, const std::u16string& texto);

// O Ctrl+Shift+C: o endereço da aba ativa, limpo.
void CopiarEnderecoDaAba(BrowserWindowInterface* browser);

}  // namespace navegador::links

#endif  // NAVEGADOR_BROWSER_UI_LINKS_H_
