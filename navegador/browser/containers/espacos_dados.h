// Espaços: conjuntos de abas com nome, ícone, cor e container próprios, como
// os workspaces do Zen e os perfis-na-mesma-janela do Dia. São do perfil (a
// lista vale pra todas as janelas); cada janela está num espaço por vez e
// mostra só as abas dele.
//
// Na janela, cada espaço é um grupo de abas focado (o modo "focar grupo" do
// Chromium já esconde as abas dos outros grupos). Este arquivo guarda só os
// dados: a lista de espaços, qual grupo de qual janela é de qual espaço, o
// último espaço usado e os atalhos configuráveis. A interface fica em
// navegador/browser/ui/espacos.*.

#ifndef NAVEGADOR_BROWSER_CONTAINERS_ESPACOS_DADOS_H_
#define NAVEGADOR_BROWSER_CONTAINERS_ESPACOS_DADOS_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/tab_groups/tab_group_id.h"

class Profile;
class TabStripModel;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace navegador::espacos {

// [{ "id", "nome", "icone", "cor", "container" }], na ordem da faixa.
inline constexpr char kListaPref[] = "navegador.espacos.lista";
// { "<token do grupo de abas>": "<id do espaço>" }. O token do grupo se
// mantém quando a sessão é restaurada.
inline constexpr char kGruposPref[] = "navegador.espacos.grupos";
// Último espaço usado: janela nova (ou aba solta) vai pra ele.
inline constexpr char kAtivoPref[] = "navegador.espacos.ativo";
// { "<ação>": "Ctrl+Alt+Right" }: só os atalhos trocados pela pessoa
// ("" = sem atalho); o resto usa o padrão de Atalhos().
inline constexpr char kAtalhosPref[] = "navegador.atalhos";
// Quadro tracejado no alto da lateral quando o espaço não tem favoritos.
inline constexpr char kQuadroDeFavoritosPref[] =
    "navegador.lateral.quadro_de_favoritos";
// Botões da janela (fechar, minimizar, maximizar) no canto da barra. Num
// gerenciador de janelas por atalho (Hyprland) sobram: desligado por padrão.
inline constexpr char kBotoesDaJanelaPref[] = "navegador.janela.botoes";
// Espessura da moldura em volta do cartão (barra + página), em px.
inline constexpr char kMolduraPref[] = "navegador.janela.moldura";
// Tela cheia da janela (F11, Super+F do Hyprland) com a barra e as abas à
// vista; desligado, some tudo como no Chromium. A tela cheia de um site
// (vídeo) esconde tudo sempre.
inline constexpr char kBarraNaTelaCheiaPref[] =
    "navegador.janela.barra_na_tela_cheia";
// Barra de navegação na cor do topo da página (como o Dia).
inline constexpr char kBarraNaCorDoSitePref[] = "navegador.barra.cor_do_site";
// Barra de navegação escondida (Ctrl+Shift+S): o Ctrl+L mostra enquanto se
// digita o endereço (navegador/browser/ui/barra.h).
inline constexpr char kBarraEscondidaPref[] = "navegador.barra.escondida";
// Lateral escondida que aparece por cima da página (como o modo compacto do
// Zen): opacidade do fundo (0-100), desfoque do que está atrás (px), tempo
// até esconder depois que o ponteiro sai (ms) e se aparece ao encostar o
// ponteiro na borda esquerda.
inline constexpr char kFlutuanteOpacidadePref[] =
    "navegador.lateral.flutuante.opacidade";
inline constexpr char kFlutuanteDesfoquePref[] =
    "navegador.lateral.flutuante.desfoque";
inline constexpr char kEsconderDepoisPref[] =
    "navegador.lateral.esconder_depois_ms";
// O mesmo, quando o ponteiro sai da janela (ou a janela perde o foco, que no
// Hyprland vai com o ponteiro); também o tempo que ela fica aberta quando o
// ponteiro sai pela borda esquerda da janela.
inline constexpr char kEsconderForaPref[] =
    "navegador.lateral.esconder_fora_ms";
inline constexpr char kMostrarNaBordaPref[] = "navegador.lateral.mostrar_na_borda";
// Passando o ponteiro rápido pela borda, a lateral abre inteira (e só depois
// conta o tempo pra esconder); desligado, abre um pedaço e volta.
inline constexpr char kAbrirInteiraPref[] = "navegador.lateral.abrir_inteira";
// Procurar versão nova nas Releases do GitHub ao abrir o navegador.
inline constexpr char kProcurarAtualizacoesPref[] =
    "navegador.atualizacoes.procurar";
// Nova aba: hora e data no centro (os atalhos embaixo usam as prefs do
// próprio Chromium; ver navegador/browser/ui/inicio.h).
inline constexpr char kInicioHoraPref[] = "navegador.inicio.hora";
inline constexpr char kInicioDataPref[] = "navegador.inicio.data";

struct Espaco {
  std::string id;
  std::string nome;
  // Emoji (ou vazio: a faixa mostra a primeira letra do nome).
  std::string icone;
  // Nome da cor da paleta dos containers (azul, roxo, verde...).
  std::string cor;
  // Id do container das abas novas do espaço; vazio = perfil normal.
  std::string container;
  // Espaço de um grupo criado por extensão (o do Claude): some quando o
  // grupo fecha, não vira o espaço ativo e a última aba não fica.
  bool temporario = false;
};

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Nunca vazia: sem nada salvo, vem um espaço "Pessoal" sem container.
std::vector<Espaco> Todos(Profile* profile);
std::optional<Espaco> Get(Profile* profile, std::string_view id);
Espaco Criar(Profile* profile,
             std::string_view nome,
             std::string_view icone,
             std::string_view cor,
             std::string_view container,
             bool temporario = false);
void Atualizar(Profile* profile, const Espaco& espaco);
// Não apaga o último espaço (a lista nunca fica vazia).
void Remover(Profile* profile, std::string_view id);
// Muda de posição na faixa (índice na lista nova).
void Mover(Profile* profile, std::string_view id, size_t posicao);

// Cor da próxima criação, em rodízio.
std::string ProximaCor(Profile* profile);

// Ligação grupo de abas (TabGroupId::ToString()) -> espaço.
std::optional<std::string> EspacoDoGrupo(Profile* profile,
                                         std::string_view grupo);
void LigarGrupo(Profile* profile,
                std::string_view grupo,
                std::string_view espaco);
void DesligarGrupo(Profile* profile, std::string_view grupo);
// Esquece os grupos que não estão em `vivos` (grupos de janelas fechadas:
// sem isso a pref só crescia). Um grupo que volte depois é religado pelo nome.
void PodarGrupos(Profile* profile, const std::set<std::string>& vivos);
// O grupo é de um espaço temporário.
bool GrupoTemporario(Profile* profile, std::string_view grupo);

std::string Ativo(Profile* profile);
void DefinirAtivo(Profile* profile, std::string_view espaco);

// --- Atalhos ---------------------------------------------------------------

struct Atalho {
  // Chave na pref (ex.: "proximo_espaco").
  std::string acao;
  // Texto pra tela de configurações.
  std::u16string descricao;
  // Padrão, no formato "Ctrl+Alt+Right" ("" = sem atalho).
  std::string padrao;
};

// Ações com atalho configurável, na ordem da tela de configurações.
const std::vector<Atalho>& Atalhos();
// Atalho em uso pra ação (o da pessoa ou o padrão).
std::string AtalhoDe(Profile* profile, std::string_view acao);
// "" tira o atalho; igual ao padrão volta a seguir o padrão.
void DefinirAtalho(Profile* profile,
                   std::string_view acao,
                   std::string_view atalho);
void RestaurarAtalhos(Profile* profile);

// --- Geral -----------------------------------------------------------------

bool MostrarQuadroDeFavoritos(Profile* profile);
bool MostrarBotoesDaJanela(Profile* profile);
// Janelas movidas e fechadas pelo mouse (KDE, GNOME, Windows), e não por um
// gerenciador por atalho (Hyprland, Sway, i3).
bool JanelasPeloMouse();
int Moldura(const Profile* profile);
bool MostrarNaBorda(const Profile* profile);
bool AbrirInteira(const Profile* profile);
int EsconderDepoisMs(const Profile* profile);
int EsconderForaMs(const Profile* profile);
// 0-100.
int FlutuanteOpacidade(const Profile* profile);
// px.
int FlutuanteDesfoque(const Profile* profile);

// Padrões (também usados quando não há perfil à mão).
inline constexpr int kMolduraPadrao = 3;
inline constexpr int kFlutuanteOpacidadePadrao = 90;
inline constexpr int kFlutuanteDesfoquePadrao = 20;
// Como o Zen (zen.view.compact.sidebar-keep-hover.duration = 150).
inline constexpr int kEsconderDepoisPadrao = 150;
inline constexpr int kEsconderForaPadrao = 500;

// O grupo do espaço que a janela mostra, pra lateral usar quando o modelo
// está sem grupo focado (no meio de um arrasto, entre uma troca e outra): ela
// continua só com o espaço da janela, sem os outros em grupos com cabeçalho.
// Sem resposta (nullopt), a lateral mostra tudo, como o Chromium. Ligado pela
// faixa dos espaços (navegador/browser/ui/espacos.cc).
using GrupoVisivelFn = base::RepeatingCallback<
    std::optional<tab_groups::TabGroupId>(const TabStripModel*)>;
void DefinirGrupoVisivel(GrupoVisivelFn fn);
std::optional<tab_groups::TabGroupId> GrupoVisivel(const TabStripModel* model);

}  // namespace navegador::espacos

#endif  // NAVEGADOR_BROWSER_CONTAINERS_ESPACOS_DADOS_H_
