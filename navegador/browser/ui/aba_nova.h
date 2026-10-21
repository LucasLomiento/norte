// Aviso de aba nova aberta por link (clique num link que abre aba nova,
// Ctrl+clique, botão do meio, "abrir link em nova aba"): com a lateral à
// vista, a linha da aba nova brilha na cor de destaque e apaga em menos de
// um segundo; com a lateral escondida (ou a aba fora da parte visível da
// lista), uma pílula no pé da página diz "Aberta numa aba nova" por uns
// dois segundos, e clicar nela vai pra aba. Com "menos movimento" no sistema,
// sem a animação.
//
// Usa o aviso que o Chromium já dá antes de pôr a aba na janela
// (BrowserView::AddOnLinkOpeningFromGestureCallback, feito pra animações
// assim).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_ABA_NOVA_H_
#define NAVEGADOR_BROWSER_UI_ABA_NOVA_H_

class BrowserView;

namespace views {
class View;
}  // namespace views

namespace navegador::aba_nova {

// Põe em `dono` (uma View que vive tanto quanto a janela: a lateral) quem
// acompanha as abas abertas por link na janela `browser_view`.
void Acompanhar(views::View* dono, BrowserView* browser_view);

}  // namespace navegador::aba_nova

#endif  // NAVEGADOR_BROWSER_UI_ABA_NOVA_H_
