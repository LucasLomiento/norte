// Memória: o Norte lembra do texto das páginas que você lê, pra você achar
// de novo pelo que lembra (uma frase, um termo), não pelo endereço. A busca
// fica na barra de endereço e no painel da Memória (Ctrl+Shift+F,
// navegador/browser/ui/painel_da_memoria.h), e abre a página no trecho,
// destacado.
//
// Tudo fica só neste computador, num banco do perfil (banco.h); sem IA, é
// busca de palavras. Fica de fora: janela anônima e de visitante, apps
// (PWA), páginas com campo de senha à vista, páginas que pedem pra não ser
// guardadas (Cache-Control: no-store, como bancos e e-mails), a busca do
// buscador, endereços desta máquina, containers temporários e o que a pessoa
// tirar (sites, containers, e-mail e conversas). Apagar o histórico apaga as
// mesmas páginas daqui.
//
// A página entra depois de ficar 3 s à vista com o carregamento terminado
// (uma aba aberta em segundo plano entra quando a pessoa olha pra ela), e é
// relida ao sair da aba, pra pegar o que carregou depois (comentários, feed).
//
// Compilado dentro de //chrome/browser/ui:ui.

#ifndef NAVEGADOR_BROWSER_MEMORIA_MEMORIA_H_
#define NAVEGADOR_BROWSER_MEMORIA_MEMORIA_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "navegador/browser/memoria/banco.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace navegador::memoria {

// Cada aba (TabHelpers): lê a página e guarda na memória do perfil.
void AcompanharAba(content::WebContents* contents);

// Guardando páginas novas (Configurações do Norte → Memória). Desligada, a
// busca continua no que já foi guardado.
bool Ligada(Profile* perfil);

// A chave de uma página: o endereço sem o fragmento e sem os parâmetros de
// rastreio. Vazio se não é página que a Memória guarda (fora do http(s)).
std::string Chave(const GURL& url);

// O endereço do resultado com o fragmento de texto: abre a página com o
// trecho achado à vista e destacado.
GURL EnderecoNoTrecho(const Resultado& resultado);

// Na thread da interface, quando a busca termina.
void Buscar(Profile* perfil,
            Filtro filtro,
            base::OnceCallback<void(std::vector<Resultado>)> pronto);
// A miniatura (já decodificada, fora da thread da interface); vazia se não
// tem.
void Miniatura(Profile* perfil,
               int64_t id,
               base::OnceCallback<void(const gfx::ImageSkia&)> pronto);
void Resumir(Profile* perfil, base::OnceCallback<void(Resumo)> pronto);

void Esquecer(Profile* perfil, int64_t id);
void EsquecerEndereco(Profile* perfil, const GURL& url);
void EsquecerTudo(Profile* perfil, base::OnceClosure pronto);

// Sites que ficam de fora (Configurações e o menu da página): pôr na lista
// também apaga o que já estava guardado deles.
bool SiteDeFora(Profile* perfil, const GURL& url);
void DeixarDeFora(Profile* perfil, const std::string& host, bool fora);
// "github.com, figma.com" (como a pessoa escreve nas Configurações).
std::string SitesDeFora(Profile* perfil);
void DefinirSitesDeFora(Profile* perfil, const std::string& sites);

bool ContainerDeFora(Profile* perfil, const std::string& container);
void DeixarContainerDeFora(Profile* perfil,
                           const std::string& container,
                           bool fora);

}  // namespace navegador::memoria

#endif  // NAVEGADOR_BROWSER_MEMORIA_MEMORIA_H_
