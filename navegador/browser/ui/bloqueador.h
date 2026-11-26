// Bloqueador de anúncios e rastreadores: o filtro de subrecursos do próprio
// Chromium (components/subresource_filter, o que o Chrome liga só em sites
// abusivos), ligado em todo site http(s) com as regras de rede da EasyList,
// EasyPrivacy e EasyList Portuguese (tools/listas.sh, que vão no pacote em
// /usr/lib/navegador/bloqueador). O filtro roda no processo de cada página
// com as regras indexadas e mapeadas em memória: barra a requisição antes de
// ela sair, então a página carrega menos coisa.
//
// O filtro só barra requisições; o espaço do anúncio barrado (às vezes com
// uma animação de "carregando" que nunca termina) some com as regras de
// esconder elementos das mesmas listas, injetadas como CSS (AcompanharAba).
//
// Liga e desliga pela configuração "Anúncios" do Chromium: o padrão do perfil
// (Configurações do Navegador → Geral) e as exceções por site (clicando no
// ícone do site na barra de endereço).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_UI_BLOQUEADOR_H_
#define NAVEGADOR_BROWSER_UI_BLOQUEADOR_H_

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace navegador::bloqueador {

// Ao abrir o navegador: indexa as regras do pacote (uma vez por versão) e
// as entrega às páginas. No lugar das listas que o Chrome baixa do Google.
void CarregarListas();

// Cada aba (TabFeatures): esconde os elementos de anúncio da página principal
// com as regras cosméticas das listas (CSS de origem do usuário, que a página
// não vê nem sobrescreve).
void AcompanharAba(content::WebContents* contents);

// Ligado no perfil: o padrão de "Anúncios" é bloquear.
bool Ligado(Profile* perfil);
void Ligar(Profile* perfil, bool ligado);

}  // namespace navegador::bloqueador

#endif  // NAVEGADOR_BROWSER_UI_BLOQUEADOR_H_
