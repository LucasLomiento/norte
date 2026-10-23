// Painel da Memória (o motor fica em navegador/browser/memoria/memoria.h):
// Ctrl+Shift+F (configurável), "> memória" na barra ou o menu do logo abrem,
// por cima da página, a busca em tudo o que você já leu. Digite o que lembra
// (uma frase, um termo, uma palavra em japonês) e aparecem as páginas com o
// trecho achado e as palavras destacadas, a miniatura da página como você
// viu e quando foi. Sem nada digitado, a linha do tempo: o que você leu
// hoje, ontem, na semana.
//
// Enter abre a página já rolada até o trecho e com ele destacado (o
// fragmento de texto); se ela já está aberta numa aba, vai pra aba e marca o
// trecho. Shift+Enter abre na aba atual, Ctrl+Enter numa aba de fundo (o
// painel fica), Shift+Delete esquece a página, Tab troca o período.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_PAINEL_DA_MEMORIA_H_
#define NAVEGADOR_BROWSER_UI_PAINEL_DA_MEMORIA_H_

#include <string>

#include "navegador/browser/memoria/banco.h"
#include "ui/base/window_open_disposition.h"

class BrowserWindowInterface;

namespace navegador::memoria {

// Abre o painel na janela (ou só devolve o foco pro campo, se já está
// aberto), com `consulta` no campo.
void AbrirPainel(BrowserWindowInterface* browser,
                 const std::u16string& consulta = std::u16string());

// Abre um resultado como o Enter do painel: a aba em que a página já está
// aberta (de qualquer janela do perfil), marcando o trecho; senão uma aba
// nova logo depois da atual, no container em que ela foi lida, já no
// trecho. Com CURRENT_TAB, na aba atual.
void AbrirResultado(BrowserWindowInterface* browser,
                    const Resultado& resultado,
                    WindowOpenDisposition onde);

}  // namespace navegador::memoria

#endif  // NAVEGADOR_BROWSER_UI_PAINEL_DA_MEMORIA_H_
