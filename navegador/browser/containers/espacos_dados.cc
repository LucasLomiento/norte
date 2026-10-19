#include "navegador/browser/containers/espacos_dados.h"

#include "base/functional/callback.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/environment.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "build/build_config.h"

namespace navegador::espacos {

namespace {

// A lista de espaços e os atalhos são do perfil normal: na janela anônima
// as prefs são uma cópia em memória por cima dele, e o que se gravasse ali
// sumia ao fechar a janela. Os grupos (de abas desta janela) e o espaço
// ativo ficam no perfil da própria janela.
PrefService* Prefs(Profile* profile) {
  return profile->GetOriginalProfile()->GetPrefs();
}

constexpr char kId[] = "id";
constexpr char kNome[] = "nome";
constexpr char kIcone[] = "icone";
constexpr char kCor[] = "cor";
constexpr char kContainer[] = "container";
constexpr char kTemporario[] = "temporario";

// Rodízio das cores dos espaços novos (nomes da paleta dos containers).
constexpr const char* kCores[] = {"azul",    "roxo",    "verde", "laranja",
                                  "rosa",    "turquesa", "amarelo", "vermelho"};

base::DictValue ParaDict(const Espaco& espaco) {
  base::DictValue dict;
  dict.Set(kId, espaco.id);
  dict.Set(kNome, espaco.nome);
  dict.Set(kIcone, espaco.icone);
  dict.Set(kCor, espaco.cor);
  dict.Set(kContainer, espaco.container);
  if (espaco.temporario) {
    dict.Set(kTemporario, true);
  }
  return dict;
}

std::optional<Espaco> DeValor(const base::Value& valor) {
  const base::DictValue* dict = valor.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  const std::string* id = dict->FindString(kId);
  const std::string* nome = dict->FindString(kNome);
  if (!id || id->empty() || !nome) {
    return std::nullopt;
  }
  const std::string* icone = dict->FindString(kIcone);
  const std::string* cor = dict->FindString(kCor);
  const std::string* container = dict->FindString(kContainer);
  return Espaco{*id,
                *nome,
                icone ? *icone : "",
                cor ? *cor : "azul",
                container ? *container : "",
                dict->FindBool(kTemporario).value_or(false)};
}

// Id fixo: a lista padrão é igual em todo perfil novo e só é gravada quando
// alguém mexe.
base::ListValue ListaPadrao() {
  base::ListValue lista;
  lista.Append(ParaDict({"pessoal", "Pessoal", "", "azul", ""}));
  return lista;
}

std::string NovoId() {
  uint8_t bytes[6];
  base::RandBytes(bytes);
  return "e" + base::HexEncodeLower(bytes);
}

}  // namespace

bool JanelasPeloMouse() {
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<base::Environment> ambiente = base::Environment::Create();
  return !ambiente->HasVar("HYPRLAND_INSTANCE_SIGNATURE") &&
         !ambiente->HasVar("SWAYSOCK") && !ambiente->HasVar("I3SOCK");
#else
  return true;
#endif
}

namespace {

// Botões da janela por padrão, menos num gerenciador por atalho (Hyprland,
// Sway, i3), onde a janela fecha pelo atalho do sistema e eles só ocupam
// espaço. No KDE, no GNOME e no Windows sem eles não dava pra fechar com o
// mouse.
bool BotoesPorPadrao() {
  return JanelasPeloMouse();
}

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kListaPref, ListaPadrao());
  registry->RegisterDictionaryPref(kGruposPref);
  registry->RegisterStringPref(kAtivoPref, "pessoal");
  registry->RegisterDictionaryPref(kAtalhosPref);
  registry->RegisterBooleanPref(kQuadroDeFavoritosPref, true);
  registry->RegisterBooleanPref(kBotoesDaJanelaPref, BotoesPorPadrao());
  registry->RegisterIntegerPref(kMolduraPref, kMolduraPadrao);
  registry->RegisterBooleanPref(kBarraNaCorDoSitePref, true);
  registry->RegisterBooleanPref(kBarraEscondidaPref, false);
  registry->RegisterBooleanPref(kBarraNaTelaCheiaPref, true);
  registry->RegisterIntegerPref(kFlutuanteOpacidadePref,
                                kFlutuanteOpacidadePadrao);
  registry->RegisterIntegerPref(kFlutuanteDesfoquePref,
                                kFlutuanteDesfoquePadrao);
  registry->RegisterIntegerPref(kEsconderDepoisPref, kEsconderDepoisPadrao);
  registry->RegisterIntegerPref(kEsconderForaPref, kEsconderForaPadrao);
  registry->RegisterBooleanPref(kMostrarNaBordaPref, true);
  registry->RegisterBooleanPref(kAbrirInteiraPref, true);
  registry->RegisterBooleanPref(kProcurarAtualizacoesPref, true);
  registry->RegisterBooleanPref(kPreviaPref, true);
  registry->RegisterBooleanPref(kInicioHoraPref, true);
  registry->RegisterBooleanPref(kInicioDataPref, true);
}

std::vector<Espaco> Todos(Profile* profile) {
  std::vector<Espaco> resultado;
  for (const base::Value& valor : Prefs(profile)->GetList(kListaPref)) {
    if (std::optional<Espaco> espaco = DeValor(valor)) {
      resultado.push_back(std::move(*espaco));
    }
  }
  if (resultado.empty()) {
    resultado.push_back({"pessoal", "Pessoal", "", "azul", ""});
  }
  return resultado;
}

std::optional<Espaco> Get(Profile* profile, std::string_view id) {
  for (Espaco& espaco : Todos(profile)) {
    if (espaco.id == id) {
      return std::move(espaco);
    }
  }
  return std::nullopt;
}

Espaco Criar(Profile* profile,
             std::string_view nome,
             std::string_view icone,
             std::string_view cor,
             std::string_view container,
             bool temporario) {
  Espaco espaco{NovoId(),          std::string(nome),
                std::string(icone), std::string(cor),
                std::string(container), temporario};
  // A lista padrão só existe em memória: grava ela antes de acrescentar.
  base::ListValue lista;
  for (const Espaco& existente : Todos(profile)) {
    lista.Append(ParaDict(existente));
  }
  lista.Append(ParaDict(espaco));
  Prefs(profile)->SetList(kListaPref, std::move(lista));
  return espaco;
}

void Atualizar(Profile* profile, const Espaco& espaco) {
  base::ListValue lista;
  for (const Espaco& existente : Todos(profile)) {
    lista.Append(ParaDict(existente.id == espaco.id ? espaco : existente));
  }
  Prefs(profile)->SetList(kListaPref, std::move(lista));
}

void Remover(Profile* profile, std::string_view id) {
  std::vector<Espaco> todos = Todos(profile);
  if (todos.size() <= 1) {
    return;
  }
  base::ListValue lista;
  for (const Espaco& existente : todos) {
    if (existente.id != id) {
      lista.Append(ParaDict(existente));
    }
  }
  Prefs(profile)->SetList(kListaPref, std::move(lista));

  // Os grupos que eram dele ficam soltos (a janela religa).
  ScopedDictPrefUpdate grupos(profile->GetPrefs(), kGruposPref);
  std::vector<std::string> soltar;
  for (const auto& [grupo, espaco] : *grupos) {
    if (espaco.is_string() && espaco.GetString() == id) {
      soltar.push_back(grupo);
    }
  }
  for (const std::string& grupo : soltar) {
    grupos->Remove(grupo);
  }
  if (Ativo(profile) == id) {
    DefinirAtivo(profile, Todos(profile).front().id);
  }
}

void Mover(Profile* profile, std::string_view id, size_t posicao) {
  std::vector<Espaco> todos = Todos(profile);
  auto it = std::ranges::find(todos, id, &Espaco::id);
  if (it == todos.end()) {
    return;
  }
  Espaco espaco = std::move(*it);
  todos.erase(it);
  posicao = std::min(posicao, todos.size());
  todos.insert(todos.begin() + posicao, std::move(espaco));
  base::ListValue lista;
  for (const Espaco& existente : todos) {
    lista.Append(ParaDict(existente));
  }
  Prefs(profile)->SetList(kListaPref, std::move(lista));
}

std::string ProximaCor(Profile* profile) {
  return kCores[Todos(profile).size() % std::size(kCores)];
}

std::optional<std::string> EspacoDoGrupo(Profile* profile,
                                         std::string_view grupo) {
  const std::string* id =
      profile->GetPrefs()->GetDict(kGruposPref).FindString(grupo);
  if (!id || !Get(profile, *id)) {
    return std::nullopt;
  }
  return *id;
}

void LigarGrupo(Profile* profile,
                std::string_view grupo,
                std::string_view espaco) {
  ScopedDictPrefUpdate grupos(profile->GetPrefs(), kGruposPref);
  grupos->Set(grupo, espaco);
}

void DesligarGrupo(Profile* profile, std::string_view grupo) {
  ScopedDictPrefUpdate grupos(profile->GetPrefs(), kGruposPref);
  grupos->Remove(grupo);
}

void PodarGrupos(Profile* profile, const std::set<std::string>& vivos) {
  std::vector<std::string> mortos;
  for (const auto [grupo, espaco] : profile->GetPrefs()->GetDict(kGruposPref)) {
    if (!vivos.contains(grupo)) {
      mortos.push_back(grupo);
    }
  }
  if (mortos.empty()) {
    return;
  }
  ScopedDictPrefUpdate grupos(profile->GetPrefs(), kGruposPref);
  for (const std::string& grupo : mortos) {
    grupos->Remove(grupo);
  }
}

bool GrupoTemporario(Profile* profile, std::string_view grupo) {
  const std::optional<std::string> id = EspacoDoGrupo(profile, grupo);
  if (!id) {
    return false;
  }
  const std::optional<Espaco> espaco = Get(profile, *id);
  return espaco && espaco->temporario;
}

std::string Ativo(Profile* profile) {
  const std::string& id = profile->GetPrefs()->GetString(kAtivoPref);
  if (Get(profile, id)) {
    return id;
  }
  return Todos(profile).front().id;
}

void DefinirAtivo(Profile* profile, std::string_view espaco) {
  profile->GetPrefs()->SetString(kAtivoPref, std::string(espaco));
}

const std::vector<Atalho>& Atalhos() {
  // Padrões: os do Zen pra troca de espaço (Ctrl+Alt+setas) e o do Arc pra
  // mostrar/ocultar a lateral (Ctrl+S, no lugar do "Salvar página").
  static const base::NoDestructor<std::vector<Atalho>> atalhos(
      std::vector<Atalho>{
          {"alternar_lateral", u"Mostrar ou ocultar a lateral", "Ctrl+S"},
          {"alternar_barra", u"Mostrar ou ocultar a barra de endereço",
           "Ctrl+Shift+S"},
          // O do Dia e do Zen (o "inspecionar" do DevTools segue no
          // Ctrl+Shift+I e no F12).
          {"copiar_endereco", u"Copiar o endereço da aba", "Ctrl+Shift+C"},
          {"proximo_espaco", u"Próximo espaço", "Ctrl+Alt+Right"},
          {"espaco_anterior", u"Espaço anterior", "Ctrl+Alt+Left"},
          {"novo_espaco", u"Novo espaço", "Ctrl+Alt+N"},
          {"espaco_1", u"Ir pro espaço 1", "Ctrl+Alt+1"},
          {"espaco_2", u"Ir pro espaço 2", "Ctrl+Alt+2"},
          {"espaco_3", u"Ir pro espaço 3", "Ctrl+Alt+3"},
          {"espaco_4", u"Ir pro espaço 4", "Ctrl+Alt+4"},
          {"espaco_5", u"Ir pro espaço 5", "Ctrl+Alt+5"},
          // Divisão de tela: os do Zen.
          {"dividir_lado", u"Dividir a tela lado a lado", "Ctrl+Alt+V"},
          {"dividir_cima", u"Dividir a tela em cima e embaixo", "Ctrl+Alt+H"},
          {"trocar_lados", u"Trocar as abas da divisão de lado", "Ctrl+Alt+X"},
          {"configuracoes", u"Configurações do Norte", "Ctrl+Comma"},
          {"containers", u"Gerenciar containers", ""},
      });
  return *atalhos;
}

std::string AtalhoDe(Profile* profile, std::string_view acao) {
  if (const std::string* escolhido =
          Prefs(profile)->GetDict(kAtalhosPref).FindString(acao)) {
    return *escolhido;
  }
  for (const Atalho& atalho : Atalhos()) {
    if (atalho.acao == acao) {
      return atalho.padrao;
    }
  }
  return std::string();
}

bool AcaoDeEspaco(std::string_view acao) {
  return acao == "proximo_espaco" || acao == "espaco_anterior" ||
         acao == "novo_espaco" || acao.starts_with("espaco_");
}

void DefinirAtalho(Profile* profile,
                   std::string_view acao,
                   std::string_view atalho) {
  ScopedDictPrefUpdate atalhos(Prefs(profile), kAtalhosPref);
  for (const Atalho& conhecido : Atalhos()) {
    if (conhecido.acao == acao && conhecido.padrao == atalho) {
      atalhos->Remove(acao);
      return;
    }
  }
  atalhos->Set(acao, atalho);
}

void RestaurarAtalhos(Profile* profile) {
  Prefs(profile)->ClearPref(kAtalhosPref);
}

bool MostrarQuadroDeFavoritos(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(kQuadroDeFavoritosPref);
}

bool MostrarBotoesDaJanela(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(kBotoesDaJanelaPref);
}

int Moldura(const Profile* profile) {
  return std::clamp(profile->GetPrefs()->GetInteger(kMolduraPref), 0, 16);
}

bool MostrarNaBorda(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(kMostrarNaBordaPref);
}

bool AbrirInteira(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(kAbrirInteiraPref);
}

int EsconderDepoisMs(const Profile* profile) {
  return std::clamp(profile->GetPrefs()->GetInteger(kEsconderDepoisPref), 0,
                    3000);
}

int EsconderForaMs(const Profile* profile) {
  return std::clamp(profile->GetPrefs()->GetInteger(kEsconderForaPref), 0,
                    3000);
}

int FlutuanteOpacidade(const Profile* profile) {
  return std::clamp(profile->GetPrefs()->GetInteger(kFlutuanteOpacidadePref),
                    20, 100);
}

int FlutuanteDesfoque(const Profile* profile) {
  return std::clamp(profile->GetPrefs()->GetInteger(kFlutuanteDesfoquePref), 0,
                    60);
}

namespace {

GrupoVisivelFn& FnDoGrupoVisivel() {
  static base::NoDestructor<GrupoVisivelFn> fn;
  return *fn;
}

}  // namespace

void DefinirGrupoVisivel(GrupoVisivelFn fn) {
  FnDoGrupoVisivel() = std::move(fn);
}

std::optional<tab_groups::TabGroupId> GrupoVisivel(const TabStripModel* model) {
  return FnDoGrupoVisivel() ? FnDoGrupoVisivel().Run(model) : std::nullopt;
}

}  // namespace navegador::espacos
