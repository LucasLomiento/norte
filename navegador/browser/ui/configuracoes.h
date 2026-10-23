// Configurações do Navegador: a janela das coisas que são nossas (geral,
// espaços, containers e atalhos), aberta pelo menu do logo, pelo menu do
// nome do espaço ou pelo atalho (Ctrl+, por padrão). As do Chromium
// continuam em chrome://settings.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_CONFIGURACOES_H_
#define NAVEGADOR_BROWSER_UI_CONFIGURACOES_H_

#include <string>

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace navegador::configuracoes {

// Na ordem do menu da janela.
enum class Aba {
  kGeral = 0,
  kPrivacidade = 1,
  kMemoria = 2,
  kAparencia = 3,
  kLateral = 4,
  kEspacos = 5,
  kContainers = 6,
  kAtalhos = 7,
};

// Abre (ou traz pra frente) a janela de configurações na aba pedida.
void Mostrar(BrowserWindowInterface* browser, Aba aba = Aba::kGeral);

// Editor de um espaço: nome, emoji, cor e container. Com `espaco` vazio,
// cria um novo. `pronto` recebe o id quando a pessoa confirma.
void EditarEspaco(BrowserWindowInterface* browser,
                  const std::string& espaco,
                  base::OnceCallback<void(const std::string&)> pronto);

// "Estilo deste site…" (menu da página): o CSS próprio e o modo escuro do
// site da `aba` (estilos.h). Salvar recarrega a aba.
void EditarEstilo(BrowserWindowInterface* browser, content::WebContents* aba);

}  // namespace navegador::configuracoes

#endif  // NAVEGADOR_BROWSER_UI_CONFIGURACOES_H_
