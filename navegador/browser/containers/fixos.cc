#include "navegador/browser/containers/fixos.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"

namespace navegador::fixos {

namespace {

// O que a pessoa escolhe é do perfil normal: na janela anônima as prefs são
// uma cópia em memória por cima dele, e o que se gravasse ali sumia ao
// fechar a janela.
PrefService* Prefs(Profile* profile) {
  return profile->GetOriginalProfile()->GetPrefs();
}

constexpr char kId[] = "id";
constexpr char kUrl[] = "url";
constexpr char kTitulo[] = "titulo";
constexpr char kNome[] = "nome";
constexpr char kAberta[] = "aberta";
constexpr char kItens[] = "itens";

constexpr char kPrefixoFavoritos[] = "favoritos:";

std::string ChaveDosFavoritos(std::string_view espaco) {
  return std::string(kPrefixoFavoritos) + std::string(espaco);
}

// Espaço de uma chave do dicionário ("favoritos:<espaço>" ou "<espaço>").
std::string EspacoDaChave(std::string_view chave) {
  if (chave.starts_with(kPrefixoFavoritos)) {
    chave.remove_prefix(std::char_traits<char>::length(kPrefixoFavoritos));
  }
  return std::string(chave);
}

AlternarFavoritoDaAba& PonteFavorito() {
  static base::NoDestructor<AlternarFavoritoDaAba> ponte;
  return *ponte;
}

// Endereço usado como chave do SupportsUserData.
constexpr char kChaveDaAba[] = "navegador_fixo";

class LigacaoDaAba : public base::SupportsUserData::Data {
 public:
  explicit LigacaoDaAba(std::string id) : id_(std::move(id)) {}
  const std::string& id() const { return id_; }

 private:
  std::string id_;
};

std::string NovoId(std::string_view prefixo) {
  uint8_t bytes[6];
  base::RandBytes(bytes);
  return std::string(prefixo) + base::HexEncodeLower(bytes);
}

std::optional<Site> SiteDe(const base::Value& valor) {
  const base::DictValue* dict = valor.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  const std::string* id = dict->FindString(kId);
  const std::string* url = dict->FindString(kUrl);
  if (!id || id->empty() || !url || dict->FindList(kItens)) {
    return std::nullopt;
  }
  const std::string* titulo = dict->FindString(kTitulo);
  return Site{*id, GURL(*url), titulo ? *titulo : std::string()};
}

base::DictValue DictDoSite(const Site& site) {
  base::DictValue dict;
  dict.Set(kId, site.id);
  dict.Set(kUrl, site.url.spec());
  dict.Set(kTitulo, site.titulo);
  return dict;
}

// Onde um id está: a lista que o contém, a posição nela, a pasta em volta
// (vazia = solto) e quantas pastas tem em volta.
struct Lugar {
  raw_ptr<base::ListValue> lista = nullptr;
  size_t indice = 0;
  std::string pasta;
  int nivel = 0;
};

std::optional<Lugar> AcharNaLista(base::ListValue& lista,
                                  std::string_view id,
                                  const std::string& pasta,
                                  int nivel) {
  for (size_t i = 0; i < lista.size(); ++i) {
    base::DictValue* dict = lista[i].GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* atual = dict->FindString(kId);
    if (atual && *atual == id) {
      return Lugar{&lista, i, pasta, nivel};
    }
    base::ListValue* itens = dict->FindList(kItens);
    if (itens && atual) {
      if (std::optional<Lugar> dentro =
              AcharNaLista(*itens, id, *atual, nivel + 1)) {
        return dentro;
      }
    }
  }
  return std::nullopt;
}

std::optional<Lugar> Achar(base::DictValue& todos,
                           std::string_view id,
                           std::string* espaco = nullptr,
                           std::string* pasta = nullptr) {
  for (auto [chave, valor] : todos) {
    base::ListValue* lista = valor.GetIfList();
    if (!lista) {
      continue;
    }
    if (std::optional<Lugar> lugar = AcharNaLista(*lista, id, "", 0)) {
      if (espaco) {
        *espaco = EspacoDaChave(chave);
      }
      if (pasta) {
        *pasta = lugar->pasta;
      }
      return lugar;
    }
  }
  return std::nullopt;
}

bool EPasta(const base::Value& valor) {
  return valor.is_dict() && valor.GetDict().FindList(kItens);
}

// Quantas pastas numa fila: 0 pra um site, 1 pra pasta sem pasta dentro.
int Altura(const base::Value& valor) {
  if (!EPasta(valor)) {
    return 0;
  }
  int dentro = 0;
  for (const base::Value& item : *valor.GetDict().FindList(kItens)) {
    dentro = std::max(dentro, Altura(item));
  }
  return 1 + dentro;
}

// `id` está dentro da pasta `valor` (em qualquer nível)?
bool Contem(const base::Value& valor, std::string_view id) {
  if (!EPasta(valor)) {
    return false;
  }
  for (const base::Value& item : *valor.GetDict().FindList(kItens)) {
    const std::string* item_id =
        item.is_dict() ? item.GetDict().FindString(kId) : nullptr;
    if ((item_id && *item_id == id) || Contem(item, id)) {
      return true;
    }
  }
  return false;
}

// Pode `valor` ir pra uma lista com `nivel` pastas em volta? Pasta não entra
// nela mesma nem numa de dentro dela (`pasta_de_destino`) nem passa do limite.
bool Cabe(const base::Value& valor,
          std::string_view id,
          std::string_view pasta_de_destino,
          int nivel) {
  if (!EPasta(valor)) {
    return true;
  }
  if (!pasta_de_destino.empty() &&
      (pasta_de_destino == id || Contem(valor, pasta_de_destino))) {
    return false;
  }
  return nivel + Altura(valor) <= kNiveisDePasta;
}

std::optional<Entrada> EntradaDe(const base::Value& valor) {
  const base::DictValue* dict = valor.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  if (const base::ListValue* itens = dict->FindList(kItens)) {
    const std::string* id = dict->FindString(kId);
    if (!id || id->empty()) {
      return std::nullopt;
    }
    Entrada pasta;
    pasta.id = *id;
    pasta.pasta = true;
    const std::string* nome = dict->FindString(kNome);
    pasta.nome = nome ? *nome : std::string();
    pasta.aberta = dict->FindBool(kAberta).value_or(true);
    for (const base::Value& item : *itens) {
      if (std::optional<Entrada> dentro = EntradaDe(item)) {
        pasta.itens.push_back(std::move(*dentro));
      }
    }
    return pasta;
  }
  std::optional<Site> site = SiteDe(valor);
  if (!site) {
    return std::nullopt;
  }
  Entrada solto;
  solto.id = site->id;
  solto.site = std::move(*site);
  return solto;
}

void JuntarSites(const Entrada& entrada, std::vector<Site>& sites) {
  if (!entrada.pasta) {
    sites.push_back(entrada.site);
    return;
  }
  for (const Entrada& item : entrada.itens) {
    JuntarSites(item, sites);
  }
}

int AlturaDa(const Entrada& entrada) {
  if (!entrada.pasta) {
    return 0;
  }
  int dentro = 0;
  for (const Entrada& item : entrada.itens) {
    dentro = std::max(dentro, AlturaDa(item));
  }
  return 1 + dentro;
}

void JuntarPastas(const std::vector<Entrada>& entradas,
                  const std::u16string& caminho,
                  int nivel,
                  std::vector<PastaNoMenu>& pastas) {
  for (const Entrada& entrada : entradas) {
    if (!entrada.pasta) {
      continue;
    }
    const std::u16string nome = base::UTF8ToUTF16(entrada.nome);
    const std::u16string meu =
        caminho.empty() ? nome : caminho + u" \u203A " + nome;
    pastas.push_back({entrada.id, meu, nivel, AlturaDa(entrada)});
    JuntarPastas(entrada.itens, meu, nivel + 1, pastas);
  }
}

base::ListValue& ListaDoEspaco(base::DictValue& todos, std::string_view espaco) {
  if (!todos.FindList(espaco)) {
    todos.Set(espaco, base::ListValue());
  }
  return *todos.FindList(espaco);
}

}  // namespace

Entrada::Entrada() = default;
Entrada::Entrada(const Entrada&) = default;
Entrada::Entrada(Entrada&&) = default;
Entrada& Entrada::operator=(const Entrada&) = default;
Entrada& Entrada::operator=(Entrada&&) = default;
Entrada::~Entrada() = default;

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kFixosPref);
}

std::vector<Entrada> Do(Profile* profile, std::string_view espaco) {
  std::vector<Entrada> resultado;
  const base::ListValue* lista =
      Prefs(profile)->GetDict(kFixosPref).FindList(espaco);
  if (!lista) {
    return resultado;
  }
  for (const base::Value& valor : *lista) {
    if (std::optional<Entrada> entrada = EntradaDe(valor)) {
      resultado.push_back(std::move(*entrada));
    }
  }
  return resultado;
}

const Entrada* PastaPorId(const std::vector<Entrada>& entradas,
                          std::string_view id) {
  for (const Entrada& entrada : entradas) {
    if (!entrada.pasta) {
      continue;
    }
    if (entrada.id == id) {
      return &entrada;
    }
    if (const Entrada* dentro = PastaPorId(entrada.itens, id)) {
      return dentro;
    }
  }
  return nullptr;
}

std::vector<Site> SitesDaPasta(const Entrada& pasta) {
  std::vector<Site> sites;
  JuntarSites(pasta, sites);
  return sites;
}

std::vector<PastaNoMenu> PastasDoEspaco(Profile* profile,
                                        std::string_view espaco) {
  std::vector<PastaNoMenu> pastas;
  JuntarPastas(Do(profile, espaco), std::u16string(), 0, pastas);
  return pastas;
}

bool CabeEm(Profile* profile, std::string_view id, std::string_view pasta) {
  base::DictValue todos = Prefs(profile)->GetDict(kFixosPref).Clone();
  std::optional<Lugar> origem = Achar(todos, id);
  if (!origem) {
    return false;
  }
  int nivel = 0;
  if (!pasta.empty()) {
    std::optional<Lugar> destino = Achar(todos, pasta);
    if (!destino || !EPasta((*destino->lista)[destino->indice])) {
      return false;
    }
    nivel = destino->nivel + 1;
  }
  return Cabe((*origem->lista)[origem->indice], id, pasta, nivel);
}

std::vector<Site> Favoritos(Profile* profile, std::string_view espaco) {
  std::vector<Site> resultado;
  if (const base::ListValue* lista =
          Prefs(profile)->GetDict(kFixosPref).FindList(
              ChaveDosFavoritos(espaco))) {
    for (const base::Value& valor : *lista) {
      if (std::optional<Site> site = SiteDe(valor)) {
        resultado.push_back(std::move(*site));
      }
    }
  }
  return resultado;
}

std::string Favoritar(Profile* profile,
                      std::string_view espaco,
                      const GURL& url,
                      std::string_view titulo) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  base::ListValue& lista =
      ListaDoEspaco(update.Get(), ChaveDosFavoritos(espaco));
  // O endereço já está nos favoritos do espaço: é ele. Uma extensão que fixa
  // abas (ou a mesma página fixada de novo) criava um ladrilho repetido a
  // cada vez.
  for (const base::Value& valor : lista) {
    if (std::optional<Site> existente = SiteDe(valor);
        existente && existente->url == url) {
      return existente->id;
    }
  }
  const Site site{NovoId("f"), url, std::string(titulo)};
  lista.Append(DictDoSite(site));
  return site.id;
}

bool EFavorito(Profile* profile, std::string_view id) {
  const base::ListValue* lista = nullptr;
  for (auto [chave, valor] : Prefs(profile)->GetDict(kFixosPref)) {
    if (!chave.starts_with(kPrefixoFavoritos) || !(lista = valor.GetIfList())) {
      continue;
    }
    for (const base::Value& item : *lista) {
      if (std::optional<Site> site = SiteDe(item); site && site->id == id) {
        return true;
      }
    }
  }
  return false;
}

void DefinirAlternarFavorito(AlternarFavoritoDaAba alternar) {
  PonteFavorito() = std::move(alternar);
}

bool AlternarFavorito(content::WebContents* aba) {
  return !PonteFavorito().is_null() && PonteFavorito().Run(aba);
}

std::optional<Site> SitePorId(Profile* profile,
                              std::string_view id,
                              std::string* espaco,
                              std::string* pasta) {
  // Busca numa cópia: Achar precisa de acesso de escrita.
  base::DictValue todos = Prefs(profile)->GetDict(kFixosPref).Clone();
  std::optional<Lugar> lugar = Achar(todos, id, espaco, pasta);
  if (!lugar) {
    return std::nullopt;
  }
  return SiteDe((*lugar->lista)[lugar->indice]);
}

std::string Fixar(Profile* profile,
                  std::string_view espaco,
                  const GURL& url,
                  std::string_view titulo,
                  std::string_view pasta) {
  const Site site{NovoId("s"), url, std::string(titulo)};
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  base::ListValue* destino = &ListaDoEspaco(update.Get(), espaco);
  if (!pasta.empty()) {
    if (std::optional<Lugar> lugar = Achar(update.Get(), pasta)) {
      if (base::ListValue* itens =
              (*lugar->lista)[lugar->indice].GetDict().FindList(kItens)) {
        destino = itens;
      }
    }
  }
  destino->Append(DictDoSite(site));
  return site.id;
}

std::string FixarPerto(Profile* profile,
                       std::string_view espaco,
                       const GURL& url,
                       std::string_view titulo,
                       std::string_view vizinho,
                       bool depois) {
  const Site site{NovoId("s"), url, std::string(titulo)};
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  if (std::optional<Lugar> lugar = Achar(update.Get(), vizinho)) {
    lugar->lista->Insert(
        lugar->lista->begin() + lugar->indice + (depois ? 1 : 0),
        base::Value(DictDoSite(site)));
  } else {
    ListaDoEspaco(update.Get(), espaco).Append(DictDoSite(site));
  }
  return site.id;
}

std::string CriarPasta(Profile* profile,
                       std::string_view espaco,
                       std::string_view nome,
                       std::string_view dentro) {
  const std::string id = NovoId("p");
  base::DictValue pasta;
  pasta.Set(kId, id);
  pasta.Set(kNome, nome);
  pasta.Set(kAberta, true);
  pasta.Set(kItens, base::ListValue());
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  base::ListValue* destino = &ListaDoEspaco(update.Get(), espaco);
  if (!dentro.empty()) {
    std::optional<Lugar> lugar = Achar(update.Get(), dentro);
    base::Value* de_fora =
        lugar ? &(*lugar->lista)[lugar->indice] : nullptr;
    if (!de_fora || !EPasta(*de_fora) ||
        lugar->nivel + 2 > kNiveisDePasta) {
      return std::string();
    }
    de_fora->GetDict().Set(kAberta, true);
    destino = de_fora->GetDict().FindList(kItens);
  }
  destino->Append(std::move(pasta));
  return id;
}

void RenomearPasta(Profile* profile,
                   std::string_view id,
                   std::string_view nome) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  if (std::optional<Lugar> lugar = Achar(update.Get(), id)) {
    (*lugar->lista)[lugar->indice].GetDict().Set(kNome, nome);
  }
}

void AbrirPasta(Profile* profile, std::string_view id, bool aberta) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  if (std::optional<Lugar> lugar = Achar(update.Get(), id)) {
    (*lugar->lista)[lugar->indice].GetDict().Set(kAberta, aberta);
  }
}

void RenomearSite(Profile* profile,
                  std::string_view id,
                  std::string_view titulo) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  if (std::optional<Lugar> lugar = Achar(update.Get(), id)) {
    (*lugar->lista)[lugar->indice].GetDict().Set(kTitulo, titulo);
  }
}

void TrocarEndereco(Profile* profile, std::string_view id, const GURL& url) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  if (std::optional<Lugar> lugar = Achar(update.Get(), id)) {
    (*lugar->lista)[lugar->indice].GetDict().Set(kUrl, url.spec());
  }
}

void Remover(Profile* profile, std::string_view id) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  if (std::optional<Lugar> lugar = Achar(update.Get(), id)) {
    lugar->lista->erase(lugar->lista->begin() + lugar->indice);
  }
}

void Mover(Profile* profile, std::string_view id, int sentido) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  std::optional<Lugar> lugar = Achar(update.Get(), id);
  if (!lugar) {
    return;
  }
  const int destino = static_cast<int>(lugar->indice) + sentido;
  if (destino < 0 || destino >= static_cast<int>(lugar->lista->size())) {
    return;
  }
  base::Value valor = std::move((*lugar->lista)[lugar->indice]);
  lugar->lista->erase(lugar->lista->begin() + lugar->indice);
  lugar->lista->Insert(lugar->lista->begin() + destino, std::move(valor));
}

bool Posicionar(Profile* profile,
                std::string_view id,
                std::string_view vizinho,
                bool depois) {
  if (id == vizinho) {
    return false;
  }
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  std::string espaco;
  std::optional<Lugar> origem = Achar(update.Get(), id, &espaco);
  std::optional<Lugar> alvo = Achar(update.Get(), vizinho);
  if (!origem || !alvo) {
    return false;
  }
  const base::Value& arrastado = (*origem->lista)[origem->indice];
  if (EPasta(arrastado) &&
      (alvo->lista == update->FindList(ChaveDosFavoritos(espaco)) ||
       !Cabe(arrastado, id, alvo->pasta, alvo->nivel) ||
       Contem(arrastado, vizinho))) {
    return false;
  }
  base::Value valor = std::move((*origem->lista)[origem->indice]);
  origem->lista->erase(origem->lista->begin() + origem->indice);
  // Os índices (e as listas de dentro das pastas) mudaram com a retirada:
  // procura o vizinho de novo.
  std::optional<Lugar> lugar = Achar(update.Get(), vizinho);
  if (!lugar) {
    // A retirada só mexe nos itens da lista de origem, não nela.
    origem->lista->Insert(origem->lista->begin() + origem->indice,
                          std::move(valor));
    return false;
  }
  lugar->lista->Insert(lugar->lista->begin() + lugar->indice + (depois ? 1 : 0),
                       std::move(valor));
  return true;
}

bool MoverParaPasta(Profile* profile,
                    std::string_view id,
                    std::string_view pasta) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  std::string espaco;
  std::optional<Lugar> lugar = Achar(update.Get(), id, &espaco);
  if (!lugar) {
    return false;
  }
  int nivel = 0;
  if (!pasta.empty()) {
    std::optional<Lugar> destino = Achar(update.Get(), pasta);
    if (!destino || !EPasta((*destino->lista)[destino->indice])) {
      return false;
    }
    nivel = destino->nivel + 1;
  }
  if (!Cabe((*lugar->lista)[lugar->indice], id, pasta, nivel)) {
    return false;
  }
  base::Value valor = std::move((*lugar->lista)[lugar->indice]);
  lugar->lista->erase(lugar->lista->begin() + lugar->indice);
  base::ListValue* destino = &ListaDoEspaco(update.Get(), espaco);
  if (!pasta.empty()) {
    if (std::optional<Lugar> da_pasta = Achar(update.Get(), pasta)) {
      if (base::ListValue* itens = (*da_pasta->lista)[da_pasta->indice]
                                       .GetDict()
                                       .FindList(kItens)) {
        destino = itens;
      }
    }
  }
  destino->Append(std::move(valor));
  return true;
}

void RemoverEspaco(Profile* profile, std::string_view espaco) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  update->Remove(espaco);
  update->Remove(ChaveDosFavoritos(espaco));
}

std::optional<std::string> SiteDaAba(content::WebContents* contents) {
  if (!contents) {
    return std::nullopt;
  }
  auto* ligacao =
      static_cast<LigacaoDaAba*>(contents->GetUserData(&kChaveDaAba));
  if (!ligacao) {
    return std::nullopt;
  }
  return ligacao->id();
}

void LigarAba(content::WebContents* contents, std::string_view id) {
  if (!contents) {
    return;
  }
  if (id.empty()) {
    contents->RemoveUserData(&kChaveDaAba);
  } else {
    contents->SetUserData(&kChaveDaAba,
                          std::make_unique<LigacaoDaAba>(std::string(id)));
  }
}

void RestaurarDaSessao(content::WebContents* contents,
                       const std::map<std::string, std::string>& extra_data) {
  auto it = extra_data.find(kSessionKey);
  if (it != extra_data.end() && !it->second.empty()) {
    LigarAba(contents, it->second);
  }
}

void AddExtraData(content::WebContents* contents,
                  std::map<std::string, std::string>& extra_data) {
  if (std::optional<std::string> site = SiteDaAba(contents)) {
    extra_data[kSessionKey] = *site;
  }
}

}  // namespace navegador::fixos
