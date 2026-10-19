#include "navegador/browser/containers/fixos.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
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

// Onde um id está: a lista que o contém e a posição nela.
struct Lugar {
  raw_ptr<base::ListValue> lista = nullptr;
  size_t indice = 0;
};

std::optional<Lugar> Achar(base::DictValue& todos,
                           std::string_view id,
                           std::string* espaco = nullptr,
                           std::string* pasta = nullptr) {
  for (auto [chave, valor] : todos) {
    base::ListValue* lista = valor.GetIfList();
    if (!lista) {
      continue;
    }
    for (size_t i = 0; i < lista->size(); ++i) {
      base::DictValue* dict = (*lista)[i].GetIfDict();
      if (!dict) {
        continue;
      }
      const std::string* atual = dict->FindString(kId);
      if (atual && *atual == id) {
        if (espaco) {
          *espaco = EspacoDaChave(chave);
        }
        if (pasta) {
          pasta->clear();
        }
        return Lugar{lista, i};
      }
      if (base::ListValue* itens = dict->FindList(kItens)) {
        for (size_t j = 0; j < itens->size(); ++j) {
          const base::DictValue* item = (*itens)[j].GetIfDict();
          const std::string* item_id = item ? item->FindString(kId) : nullptr;
          if (item_id && *item_id == id) {
            if (espaco) {
              *espaco = EspacoDaChave(chave);
            }
            if (pasta && atual) {
              *pasta = *atual;
            }
            return Lugar{itens, j};
          }
        }
      }
    }
  }
  return std::nullopt;
}

base::ListValue& ListaDoEspaco(base::DictValue& todos, std::string_view espaco) {
  if (!todos.FindList(espaco)) {
    todos.Set(espaco, base::ListValue());
  }
  return *todos.FindList(espaco);
}

}  // namespace

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
    const base::DictValue* dict = valor.GetIfDict();
    if (!dict) {
      continue;
    }
    if (const base::ListValue* itens = dict->FindList(kItens)) {
      const std::string* id = dict->FindString(kId);
      if (!id || id->empty()) {
        continue;
      }
      Entrada pasta;
      pasta.id = *id;
      pasta.pasta = true;
      const std::string* nome = dict->FindString(kNome);
      pasta.nome = nome ? *nome : std::string();
      pasta.aberta = dict->FindBool(kAberta).value_or(true);
      for (const base::Value& item : *itens) {
        if (std::optional<Site> site = SiteDe(item)) {
          pasta.itens.push_back(std::move(*site));
        }
      }
      resultado.push_back(std::move(pasta));
    } else if (std::optional<Site> site = SiteDe(valor)) {
      Entrada solto;
      solto.id = site->id;
      solto.site = std::move(*site);
      resultado.push_back(std::move(solto));
    }
  }
  return resultado;
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

std::string CriarPasta(Profile* profile,
                       std::string_view espaco,
                       std::string_view nome) {
  const std::string id = NovoId("p");
  base::DictValue pasta;
  pasta.Set(kId, id);
  pasta.Set(kNome, nome);
  pasta.Set(kAberta, true);
  pasta.Set(kItens, base::ListValue());
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  ListaDoEspaco(update.Get(), espaco).Append(std::move(pasta));
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

void Posicionar(Profile* profile,
                std::string_view id,
                std::string_view vizinho,
                bool depois) {
  if (id == vizinho) {
    return;
  }
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  std::string espaco;
  std::optional<Lugar> origem = Achar(update.Get(), id, &espaco);
  std::string pasta_do_vizinho;
  std::optional<Lugar> alvo =
      Achar(update.Get(), vizinho, nullptr, &pasta_do_vizinho);
  if (!origem || !alvo) {
    return;
  }
  const bool e_pasta =
      (*origem->lista)[origem->indice].GetDict().FindList(kItens) != nullptr;
  std::string ancora(vizinho);
  if (e_pasta) {
    if (!pasta_do_vizinho.empty()) {
      if (pasta_do_vizinho == id) {
        return;  // pra dentro dela mesma
      }
      ancora = pasta_do_vizinho;
    }
    if (alvo->lista == update->FindList(ChaveDosFavoritos(espaco))) {
      return;
    }
  }
  base::Value valor = std::move((*origem->lista)[origem->indice]);
  origem->lista->erase(origem->lista->begin() + origem->indice);
  // Os índices mudaram com a retirada: procura o vizinho de novo.
  std::optional<Lugar> lugar = Achar(update.Get(), ancora);
  if (!lugar) {
    origem->lista->Insert(origem->lista->begin() + origem->indice,
                          std::move(valor));
    return;
  }
  lugar->lista->Insert(lugar->lista->begin() + lugar->indice + (depois ? 1 : 0),
                       std::move(valor));
}

void MoverParaPasta(Profile* profile,
                    std::string_view id,
                    std::string_view pasta) {
  ScopedDictPrefUpdate update(Prefs(profile), kFixosPref);
  std::string espaco;
  std::optional<Lugar> lugar = Achar(update.Get(), id, &espaco);
  if (!lugar || !SiteDe((*lugar->lista)[lugar->indice])) {
    return;
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
