// Aba seguidora: com a tela dividida, os links clicados numa metade abrem na
// outra (ler a lista de um lado e as páginas do outro, como no Arc). Liga e
// desliga pela aba, no menu da divisão ("Links desta aba abrem na outra
// metade"). Só o clique comum num link http(s): Ctrl+clique, botão do meio,
// formulários e o que a página faz sozinha continuam como sempre. Não fica
// gravado: vale até fechar a aba.
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_SEGUIDORA_H_
#define NAVEGADOR_BROWSER_UI_SEGUIDORA_H_

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace navegador::seguidora {

// Os links da `aba` vão pra outra metade da divisão dela?
bool Ligada(const tabs::TabInterface* aba);

// Liga ou desliga na `aba`.
void Alternar(tabs::TabInterface* aba);

// Põe o desvio nas navegações (chrome_content_browser_client_navigation_
// throttles.cc).
void TalvezAdicionar(content::NavigationThrottleRegistry& registry);

}  // namespace navegador::seguidora

#endif  // NAVEGADOR_BROWSER_UI_SEGUIDORA_H_
