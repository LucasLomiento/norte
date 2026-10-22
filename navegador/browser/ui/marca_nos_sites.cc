#include "navegador/browser/ui/marca_nos_sites.h"

#include "url/gurl.h"

namespace navegador::marca_nos_sites {

namespace {

// Roda num mundo isolado (o mesmo DOM, sem depender do site), no começo do
// documento, antes dos scripts da página; chegando com a página já montada,
// passa primeiro pelo que já está nela. A loja monta os botões aos poucos e
// troca de página sem recarregar: o observador pega cada texto que entra. Só troca texto que é exatamente um dos rótulos (nada nas
// descrições das extensões), e o aria-label/title desses botões. O próprio
// texto trocado não está na lista: não volta a ser trocado.
constexpr char kLojaDoChrome[] = R"(
(() => {
  const trocas = new Map([
    ['Usar no Chrome', 'Usar no Norte'],
    ['Adicionar ao Chrome', 'Adicionar ao Norte'],
    ['Remover do Chrome', 'Remover do Norte'],
    ['Add to Chrome', 'Add to Norte'],
    ['Remove from Chrome', 'Remove from Norte'],
    ['Agregar a Chrome', 'Agregar a Norte'],
    ['Añadir a Chrome', 'Añadir a Norte'],
    ['Quitar de Chrome', 'Quitar de Norte'],
    ['Eliminar de Chrome', 'Eliminar de Norte'],
  ]);
  const atributos = ['aria-label', 'title'];
  const trocarTexto = (texto) => {
    const valor = texto.nodeValue;
    const novo = trocas.get(valor.trim());
    if (novo) {
      texto.nodeValue = valor.replace(valor.trim(), novo);
    }
  };
  const trocarAtributos = (el) => {
    for (const nome of atributos) {
      const valor = el.getAttribute(nome);
      const novo = valor && trocas.get(valor.trim());
      if (novo) {
        el.setAttribute(nome, novo);
      }
    }
  };
  const trocar = (no) => {
    if (no.nodeType === Node.TEXT_NODE) {
      trocarTexto(no);
      return;
    }
    if (no.nodeType !== Node.ELEMENT_NODE) {
      return;
    }
    trocarAtributos(no);
    const andar = document.createTreeWalker(no, NodeFilter.SHOW_TEXT);
    for (let t = andar.nextNode(); t; t = andar.nextNode()) {
      trocarTexto(t);
    }
    for (const el of no.querySelectorAll('[aria-label], [title]')) {
      trocarAtributos(el);
    }
  };
  if (document.documentElement) {
    trocar(document.documentElement);
  }
  new MutationObserver((mudancas) => {
    for (const m of mudancas) {
      if (m.type === 'childList') {
        m.addedNodes.forEach(trocar);
      } else if (m.type === 'characterData') {
        trocarTexto(m.target);
      } else {
        trocarAtributos(m.target);
      }
    }
  }).observe(document, {
    subtree: true, childList: true, characterData: true,
    attributes: true, attributeFilter: atributos,
  });
})();
)";

}  // namespace

std::string JsDoSite(const GURL& url) {
  if (url.SchemeIs(url::kHttpsScheme) &&
      url.host() == "chromewebstore.google.com") {
    return kLojaDoChrome;
  }
  return std::string();
}

}  // namespace navegador::marca_nos_sites
