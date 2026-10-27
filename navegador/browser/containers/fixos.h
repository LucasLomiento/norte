// Fixados de cada espaço, como os "pinned tabs" e as pastas do Zen: uma
// lista de sites (soltos ou dentro de pastas) que é do espaço e não da
// janela. Toda janela que está no espaço mostra a mesma lista no alto da
// lateral, e ela sobrevive a fechar o navegador. Clicar num site abre uma
// aba ligada a ele (que sai da lista de abas e aparece no lugar do site);
// fechar a aba não tira o site da lista.
//
// Este arquivo guarda os dados e a ligação aba -> site (que vai junto na
// sessão, pra voltar ligada quando o navegador reabre). A interface fica em
// navegador/browser/ui/fixos.*.

#ifndef NAVEGADOR_BROWSER_CONTAINERS_FIXOS_H_
#define NAVEGADOR_BROWSER_CONTAINERS_FIXOS_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace navegador::fixos {

// { "<id do espaço>": [ site | pasta ] }, na ordem da lateral.
//   site:  { "id", "url", "titulo" }
//   pasta: { "id", "nome", "aberta", "itens": [ site | pasta ] }
inline constexpr char kFixosPref[] = "navegador.espacos.fixos";
// Pastas dentro de pastas: no máximo 5 numa fila (cada uma recua os itens
// dela na lateral).
inline constexpr int kNiveisDePasta = 5;
// Chave no extra_data da aba na sessão: id do site ligado.
inline constexpr char kSessionKey[] = "navegador_fixo";

struct Site {
  std::string id;
  GURL url;
  std::string titulo;
};

// Uma linha do alto da lateral: um site solto ou uma pasta.
struct Entrada {
  Entrada();
  Entrada(const Entrada&);
  Entrada(Entrada&&);
  Entrada& operator=(const Entrada&);
  Entrada& operator=(Entrada&&);
  ~Entrada();

  std::string id;
  bool pasta = false;
  Site site;  // só quando !pasta
  std::string nome;  // pasta
  bool aberta = true;  // pasta
  std::vector<Entrada> itens;  // pasta: sites e pastas
};

// Uma pasta pros menus ("Mover para a pasta"), em profundidade.
struct PastaNoMenu {
  std::string id;
  // "Trabalho › Clientes".
  std::u16string caminho;
  // Quantas pastas tem em volta dela (0 = solta).
  int nivel = 0;
  // Quantas pastas numa fila ela tem, contando ela (1 = sem pastas dentro).
  int altura = 1;
};

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

std::vector<Entrada> Do(Profile* profile, std::string_view espaco);

// A pasta `id` em `entradas`, procurando também dentro das pastas.
const Entrada* PastaPorId(const std::vector<Entrada>& entradas,
                          std::string_view id);
// Os sites de dentro da pasta, inclusive os das pastas de dentro dela.
std::vector<Site> SitesDaPasta(const Entrada& pasta);
// Todas as pastas do espaço, em profundidade.
std::vector<PastaNoMenu> PastasDoEspaco(Profile* profile,
                                        std::string_view espaco);
// Dá pra pôr a pasta (ou o site) `id` dentro da pasta `pasta` (vazia =
// solta)? Pasta não entra nela mesma nem numa de dentro dela, nem passa de
// kNiveisDePasta numa fila.
bool CabeEm(Profile* profile, std::string_view id, std::string_view pasta);

// Favoritos do espaço: os ladrilhos no alto da lateral (como os favoritos do
// Dia, que são do perfil, e os "essentials" do Zen por container). Ficam no
// mesmo dicionário, na chave "favoritos:<espaço>"; as funções por id abaixo
// (Remover, Mover, TrocarEndereco...) valem pra eles também.
std::vector<Site> Favoritos(Profile* profile, std::string_view espaco);
std::string Favoritar(Profile* profile,
                      std::string_view espaco,
                      const GURL& url,
                      std::string_view titulo);
bool EFavorito(Profile* profile, std::string_view id);

// "Adicionar aos favoritos" / "Tirar dos favoritos" do menu da aba
// (TabStripModel, CommandTogglePinned). Quem faz é a área dos fixados da
// janela (navegador/browser/ui/fixos.cc), que se registra aqui; devolve
// falso quando não tratou (aí o Chromium fixa a aba do jeito dele).
using AlternarFavoritoDaAba =
    base::RepeatingCallback<bool(content::WebContents* aba)>;
void DefinirAlternarFavorito(AlternarFavoritoDaAba alternar);
bool AlternarFavorito(content::WebContents* aba);

// Site pelo id, em qualquer espaço (`espaco` e `pasta` recebem onde está;
// pasta vazia = solto).
std::optional<Site> SitePorId(Profile* profile,
                              std::string_view id,
                              std::string* espaco = nullptr,
                              std::string* pasta = nullptr);

// Fixa no espaço, solto (`pasta` vazia) ou no fim da pasta. Devolve o id.
std::string Fixar(Profile* profile,
                  std::string_view espaco,
                  const GURL& url,
                  std::string_view titulo,
                  std::string_view pasta);
// Fixa no espaço do lado (`depois` ou antes) de `vizinho`, na lista dele
// (solto, dentro de uma pasta). Devolve o id.
std::string FixarPerto(Profile* profile,
                       std::string_view espaco,
                       const GURL& url,
                       std::string_view titulo,
                       std::string_view vizinho,
                       bool depois);
// Pasta nova no fim dos fixados do espaço, ou no fim da pasta `dentro`.
// Devolve o id (vazio se não coube: kNiveisDePasta).
std::string CriarPasta(Profile* profile,
                       std::string_view espaco,
                       std::string_view nome,
                       std::string_view dentro = {});
void RenomearPasta(Profile* profile, std::string_view id, std::string_view nome);
void AbrirPasta(Profile* profile, std::string_view id, bool aberta);
void RenomearSite(Profile* profile,
                  std::string_view id,
                  std::string_view titulo);
void TrocarEndereco(Profile* profile, std::string_view id, const GURL& url);
// Site ou pasta inteira.
void Remover(Profile* profile, std::string_view id);
// Sobe (-1) ou desce (+1) dentro da lista em que está.
void Mover(Profile* profile, std::string_view id, int sentido);
// Arrastar: põe o site ou a pasta `id` antes (ou `depois`) de `vizinho`, na
// lista dele (solto, dentro de uma pasta ou nos favoritos). Pasta não vai
// pros favoritos, nem pra dentro dela mesma, nem passa de kNiveisDePasta.
// Devolve se mudou.
bool Posicionar(Profile* profile,
                std::string_view id,
                std::string_view vizinho,
                bool depois);
// Leva o site ou a pasta pra dentro de uma pasta (ou pra fora, com `pasta`
// vazia), no fim. Com as mesmas regras do Posicionar.
bool MoverParaPasta(Profile* profile,
                    std::string_view id,
                    std::string_view pasta);
// O espaço foi apagado: os fixados vão junto.
void RemoverEspaco(Profile* profile, std::string_view espaco);

// Aba ligada a um site fixado (nada = aba comum).
std::optional<std::string> SiteDaAba(content::WebContents* contents);
// Liga (ou desliga, com id vazio).
void LigarAba(content::WebContents* contents, std::string_view id);
// browser_tabrestore.cc: a aba volta da sessão ligada ao mesmo site.
void RestaurarDaSessao(content::WebContents* contents,
                       const std::map<std::string, std::string>& extra_data);
// Coleta pro extra_data da aba fechada ("reabrir aba fechada").
void AddExtraData(content::WebContents* contents,
                  std::map<std::string, std::string>& extra_data);

}  // namespace navegador::fixos

#endif  // NAVEGADOR_BROWSER_CONTAINERS_FIXOS_H_
