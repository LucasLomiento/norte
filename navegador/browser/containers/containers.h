// Containers: identidades separadas dentro do mesmo perfil, como os
// containers do Firefox. Cada container é uma StoragePartition própria
// (cookies, localStorage, IndexedDB, cache e service workers separados);
// histórico, favoritos e senhas continuam sendo do perfil.
//
// Uma aba de container nasce com um SiteInstance de partição fixa, e o
// content/ mantém essa partição em qualquer navegação da aba (o mesmo
// mecanismo do <webview>). Por isso o container de uma aba se descobre pela
// própria aba, sem tabela paralela.

#ifndef NAVEGADOR_BROWSER_CONTAINERS_CONTAINERS_H_
#define NAVEGADOR_BROWSER_CONTAINERS_CONTAINERS_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/storage_partition_config.h"

class GURL;
class Profile;

namespace content {
class SiteInstance;
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace navegador::containers {

// Domínio das partições dos containers ([a-z]*, regra do content/).
inline constexpr char kPartitionDomain[] = "navegador";
// Lista de containers do perfil.
inline constexpr char kContainersPref[] = "navegador.containers";
// Chave no extra_data da aba na sessão.
inline constexpr char kSessionExtraDataKey[] = "navegador_container";
// Antigo: container por grupo de abas (hoje é do espaço, espacos_dados.h).
inline constexpr char kEspacosPref[] = "navegador.espacos_container";

struct Container {
  std::string id;
  std::string name;
  // Nome da cor da paleta (azul, laranja, verde, rosa, roxo, amarelo,
  // vermelho, cinza), não um hexadecimal: a cor de verdade vem do tema.
  std::string color;
  // Nome do ícone (pessoa, maleta, cifrao, carrinho, ...).
  std::string icon;
  // Temporário: some (com os dados) quando a última aba dele fecha
  // (containers_ui.h).
  bool temporario = false;
};

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

std::vector<Container> GetAll(Profile* profile);
std::optional<Container> Get(Profile* profile, std::string_view id);
// Cria com um id novo e devolve o container criado.
Container Add(Profile* profile,
              std::string_view name,
              std::string_view color,
              std::string_view icon);
// Um container temporário novo ("Temporário 1", "Temporário 2"...), em
// cinza.
Container AddTemporario(Profile* profile);
// Atualiza nome, cor e ícone do container com o mesmo id.
void Update(Profile* profile, const Container& container);
// Tira da lista. Os dados ficam até alguém chamar ApagarDados.
void Remove(Profile* profile, std::string_view id);

// Apaga tudo o que o container guardou (cookies, armazenamento, cache HTTP
// e de código), mesmo sem aba dele aberta nesta sessão. Abas abertas nele
// perdem o login. Roda em segundo plano.
void ApagarDados(Profile* profile, std::string_view id);

// Container padrão do espaço (id do espaço, ver espacos_dados.h).
std::optional<std::string> ContainerDoEspaco(Profile* profile,
                                             std::string_view espaco);
// Vazio tira o container do espaço.
void DefinirContainerDoEspaco(Profile* profile,
                              std::string_view espaco,
                              std::string_view container);

content::StoragePartitionConfig PartitionConfig(Profile* profile,
                                                std::string_view id);

// Container da aba, ou nada se a aba é da partição normal do perfil.
std::optional<std::string> IdForWebContents(content::WebContents* contents);
std::optional<std::string> IdForSiteInstance(content::SiteInstance* instance);

// SiteInstance de uma aba nova dentro do container `id`.
scoped_refptr<content::SiteInstance> CreateSiteInstance(Profile* profile,
                                                        const GURL& url,
                                                        std::string_view id);

// --- Ganchos no Chromium -------------------------------------------------

// browser_navigator.cc (CreateTargetContents), só quando não há opener.
// Com aba de origem (Ctrl+clique, "abrir em nova aba"), herda o container
// dela (ou nenhum). Sem origem (Ctrl+T, botão "Nova aba"), usa o container
// do espaço focado, se tiver. Devolve nullptr quando não é de container.
scoped_refptr<content::SiteInstance> SiteInstanceForNewTab(
    Profile* profile,
    content::WebContents* source_contents,
    std::optional<std::string> espaco,
    const GURL& url);

// browser_tabrestore.cc (CreateRestoredTab): recria a aba no container que
// estava salvo na sessão. Devolve nullptr quando não havia container ou ele
// não existe mais.
scoped_refptr<content::SiteInstance> SiteInstanceForRestoredTab(
    Profile* profile,
    const std::map<std::string, std::string>& extra_data);

// tab_lifecycle_unit.cc (FinishDiscard): a aba descartada pra economizar
// memória ganha uma WebContents nova. Ela nasce no container da antiga
// (nullptr = partição normal) e leva o nome dado e o site fixado.
scoped_refptr<content::SiteInstance> SiteInstanceDaAba(
    Profile* profile,
    content::WebContents* contents);
void CopiarDadosDaAba(content::WebContents* de, content::WebContents* para);

// Coleta do extra_data (sessão e "reabrir aba fechada").
void AddExtraData(content::WebContents* contents,
                  std::map<std::string, std::string>& extra_data);

}  // namespace navegador::containers

#endif  // NAVEGADOR_BROWSER_CONTAINERS_CONTAINERS_H_
