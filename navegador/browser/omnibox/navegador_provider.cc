#include "navegador/browser/omnibox/navegador_provider.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

constexpr size_t kMaximoDeComandos = 8;
// Acima da busca do que foi digitado (~1300): Enter roda o primeiro.
constexpr int kRelevanciaDoPrimeiro = 1550;
// Abaixo da busca: a conta aparece na lista, e Enter continua buscando.
constexpr int kRelevanciaDaConta = 1250;
// Memória: abaixo da busca e dos endereços do histórico que batem com o que
// foi digitado; Enter continua fazendo o que fazia.
constexpr int kRelevanciaDaMemoria = 1080;
constexpr size_t kMaximoDaMemoria = 3;
// Menos que isso digitado não vai pra Memória (acharia meio mundo).
constexpr size_t kMinimoDaMemoria = 3;

// A sugestão escolhida roda o comando em vez de navegar.
class AcaoDoNorte : public OmniboxAction {
 public:
  AcaoDoNorte(std::string id, const std::u16string& titulo)
      : OmniboxAction(LabelStrings(titulo, titulo, std::u16string(), titulo),
                      GURL()),
        id_(std::move(id)) {}

  void Execute(ExecutionContext& context) const override {
    context.client_->NavegadorExecutar(id_);
  }

  // O logo do Norte (o chrome_product_cr23.svg da barra é a agulha).
  const gfx::VectorIcon& GetVectorIcon() const override {
    return omnibox::kChromeProductIcon;
  }

  // Não é um pedal do Chromium (OmniboxPedal::FromAction faria o cast).
  OmniboxActionId ActionId() const override { return OmniboxActionId::UNKNOWN; }

 private:
  ~AcaoDoNorte() override = default;

  const std::string id_;
};

// Minúsculas sem acento, pra "espaco" achar "Espaço".
std::u16string Dobrar(std::u16string_view texto) {
  static constexpr std::pair<std::u16string_view, char16_t> kAcentos[] = {
      {u"áàâãäÁÀÂÃÄ", u'a'}, {u"éèêëÉÈÊË", u'e'}, {u"íìîïÍÌÎÏ", u'i'},
      {u"óòôõöÓÒÔÕÖ", u'o'}, {u"úùûüÚÙÛÜ", u'u'}, {u"çÇ", u'c'},
      {u"ñÑ", u'n'},
  };
  std::u16string dobrado;
  dobrado.reserve(texto.size());
  for (char16_t c : texto) {
    for (const auto& [letras, base] : kAcentos) {
      if (letras.find(c) != std::u16string_view::npos) {
        c = base;
        break;
      }
    }
    dobrado.push_back(base::ToLowerASCII(c));
  }
  return dobrado;
}

// Toda palavra da busca começa alguma palavra do texto?
bool Acha(const std::u16string& texto,
          const std::vector<std::u16string>& palavras) {
  for (const std::u16string& palavra : palavras) {
    bool achou = false;
    for (size_t pos = texto.find(palavra); pos != std::u16string::npos;
         pos = texto.find(palavra, pos + 1)) {
      if (pos == 0 || !base::IsAsciiAlphaNumeric(texto[pos - 1])) {
        achou = true;
        break;
      }
    }
    if (!achou) {
      return false;
    }
  }
  return true;
}

// Conta com + - * / ^ %, parênteses, "x" e "×" como vezes, "÷" como
// dividir, vírgula ou ponto decimal e "15% de 80".
class Conta {
 public:
  explicit Conta(std::u16string_view texto) : texto_(texto) {}

  std::optional<double> Avaliar() {
    std::optional<double> valor = Soma();
    Espacos();
    if (!valor || pos_ != texto_.size() || !std::isfinite(*valor)) {
      return std::nullopt;
    }
    return valor;
  }

  bool usou_virgula() const { return usou_virgula_; }
  bool usou_ponto() const { return usou_ponto_; }
  int operadores() const { return operadores_; }
  int menos() const { return menos_; }
  bool usou_parenteses() const { return usou_parenteses_; }

 private:
  char16_t Atual() const {
    return pos_ < texto_.size() ? texto_[pos_] : u'\0';
  }

  void Espacos() {
    while (pos_ < texto_.size() &&
           (texto_[pos_] == u' ' || texto_[pos_] == u' ')) {
      ++pos_;
    }
  }

  bool Palavra(std::u16string_view palavra) {
    if (texto_.substr(pos_, palavra.size()) != palavra) {
      return false;
    }
    const size_t fim = pos_ + palavra.size();
    if (fim < texto_.size() && base::IsAsciiAlpha(texto_[fim])) {
      return false;
    }
    pos_ = fim;
    return true;
  }

  std::optional<double> Soma() {
    std::optional<double> valor = Produto();
    while (valor) {
      Espacos();
      const char16_t c = Atual();
      if (c != u'+' && c != u'-' && c != u'−') {
        break;
      }
      ++pos_;
      ++operadores_;
      if (c != u'+') {
        ++menos_;
      }
      std::optional<double> outro = Produto();
      if (!outro) {
        return std::nullopt;
      }
      valor = c == u'+' ? *valor + *outro : *valor - *outro;
    }
    return valor;
  }

  std::optional<double> Produto() {
    std::optional<double> valor = Potencia();
    while (valor) {
      Espacos();
      const char16_t c = Atual();
      if (c != u'*' && c != u'/' && c != u'x' && c != u'×' &&
          c != u'÷') {
        break;
      }
      ++pos_;
      ++operadores_;
      std::optional<double> outro = Potencia();
      if (!outro) {
        return std::nullopt;
      }
      if (c == u'/' || c == u'÷') {
        if (*outro == 0) {
          return std::nullopt;
        }
        valor = *valor / *outro;
      } else {
        valor = *valor * *outro;
      }
    }
    return valor;
  }

  std::optional<double> Potencia() {
    std::optional<double> base = Unario();
    Espacos();
    if (base && Atual() == u'^') {
      ++pos_;
      ++operadores_;
      std::optional<double> expoente = Potencia();
      if (!expoente) {
        return std::nullopt;
      }
      return std::pow(*base, *expoente);
    }
    return base;
  }

  std::optional<double> Unario() {
    Espacos();
    if (Atual() == u'-' || Atual() == u'−') {
      ++pos_;
      std::optional<double> valor = Unario();
      return valor ? std::optional<double>(-*valor) : std::nullopt;
    }
    if (Atual() == u'+') {
      ++pos_;
      return Unario();
    }
    return Percentual();
  }

  std::optional<double> Percentual() {
    std::optional<double> valor = Primario();
    Espacos();
    if (!valor || Atual() != u'%') {
      return valor;
    }
    ++pos_;
    ++operadores_;
    const size_t antes = pos_;
    Espacos();
    if (Palavra(u"de")) {
      std::optional<double> total = Unario();
      if (!total) {
        return std::nullopt;
      }
      return *valor / 100 * *total;
    }
    pos_ = antes;
    return *valor / 100;
  }

  std::optional<double> Primario() {
    Espacos();
    if (Atual() == u'(') {
      ++pos_;
      std::optional<double> valor = Soma();
      Espacos();
      if (!valor || Atual() != u')') {
        return std::nullopt;
      }
      ++pos_;
      usou_parenteses_ = true;
      return valor;
    }
    return Numero();
  }

  std::optional<double> Numero() {
    std::string digitos;
    bool separador = false;
    while (pos_ < texto_.size()) {
      const char16_t c = texto_[pos_];
      if (base::IsAsciiDigit(c)) {
        digitos.push_back(static_cast<char>(c));
      } else if ((c == u'.' || c == u',') && !separador) {
        separador = true;
        usou_virgula_ |= c == u',';
        usou_ponto_ |= c == u'.';
        digitos.push_back('.');
      } else {
        break;
      }
      ++pos_;
    }
    double valor = 0;
    if (digitos.empty() || digitos == "." ||
        !base::StringToDouble(digitos, &valor)) {
      return std::nullopt;
    }
    return valor;
  }

  const std::u16string_view texto_;
  size_t pos_ = 0;
  bool usou_virgula_ = false;
  bool usou_ponto_ = false;
  bool usou_parenteses_ = false;
  int operadores_ = 0;
  int menos_ = 0;
};

}  // namespace

NavegadorProvider::NavegadorProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_NAVEGADOR),
      client_(client) {}

NavegadorProvider::~NavegadorProvider() = default;

// static
std::optional<std::u16string> NavegadorProvider::Calcular(
    std::u16string_view texto,
    bool virgula_padrao) {
  texto = base::TrimWhitespace(texto, base::TRIM_ALL);
  // Começar com "=" também vale ("=2+2").
  if (texto.starts_with(u"=")) {
    texto.remove_prefix(1);
  }
  if (texto.empty() || texto.size() > 200 ||
      (!base::IsAsciiDigit(texto.back()) && texto.back() != u')' &&
       texto.back() != u'%')) {
    return std::nullopt;
  }
  Conta conta(texto);
  std::optional<double> valor = conta.Avaliar();
  // Sem operador não é conta; só traços (2026-10-21, 11-98765-4321) é data
  // ou telefone.
  if (!valor || conta.operadores() == 0 ||
      (conta.menos() == conta.operadores() && conta.menos() >= 2 &&
       !conta.usou_parenteses() &&
       texto.find(u' ') == std::u16string_view::npos)) {
    return std::nullopt;
  }
  // -0 vira 0.
  if (*valor == 0) {
    valor = 0;
  }
  std::string escrito = base::StringPrintf("%.12g", *valor);
  if (conta.usou_virgula() || (virgula_padrao && !conta.usou_ponto())) {
    std::ranges::replace(escrito, '.', ',');
  }
  return base::UTF8ToUTF16(escrito);
}

void NavegadorProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  matches_.clear();
  if (input.IsZeroSuggest() ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }
  const std::u16string_view texto =
      base::TrimWhitespace(input.text(), base::TRIM_LEADING);
  if (texto.starts_with(u">")) {
    Comandos(input, texto.substr(1));
    return;
  }
  Calculadora(input);
  Memoria(input);
}

void NavegadorProvider::Stop(AutocompleteStopReason stop_reason) {
  AutocompleteProvider::Stop(stop_reason);
  ++pedido_;
}

void NavegadorProvider::DeleteMatch(const AutocompleteMatch& match) {
  if (match.type != AutocompleteMatchType::HISTORY_BODY) {
    return;
  }
  client_->NavegadorEsquecer(match.destination_url);
  std::erase_if(matches_, [&](const AutocompleteMatch& m) {
    return m.destination_url == match.destination_url;
  });
}

void NavegadorProvider::Memoria(const AutocompleteInput& input) {
  // Endereço digitado é pra ir pro site; e a classificação síncrona (Enter
  // antes das sugestões chegarem) não espera.
  if (input.omit_asynchronous_matches() ||
      input.type() == metrics::OmniboxInputType::URL) {
    return;
  }
  const std::u16string texto(base::TrimWhitespace(input.text(), base::TRIM_ALL));
  if (texto.size() < kMinimoDaMemoria) {
    return;
  }
  done_ = false;
  client_->NavegadorLembrar(
      texto, base::BindOnce(&NavegadorProvider::Lembrou,
                            weak_factory_.GetWeakPtr(), ++pedido_, texto));
}

void NavegadorProvider::Lembrou(int pedido,
                                std::u16string texto,
                                std::vector<NavegadorLembranca> lembrancas) {
  if (pedido != pedido_) {
    return;
  }
  done_ = true;
  for (size_t i = 0; i < lembrancas.size() && i < kMaximoDaMemoria; ++i) {
    const NavegadorLembranca& lembranca = lembrancas[i];
    AutocompleteMatch match(this, kRelevanciaDaMemoria - static_cast<int>(i),
                            /*deletable=*/true,
                            AutocompleteMatchType::HISTORY_BODY);
    match.destination_url = lembranca.url;
    GURL::Replacements sem_trecho;
    sem_trecho.ClearRef();
    match.fill_into_edit = base::UTF8ToUTF16(
        lembranca.url.ReplaceComponents(sem_trecho).spec());
    // A barra troca as duas (AggregateNewMatches): aparece o título e, ao
    // lado, o trecho com as palavras achadas em negrito.
    match.description = lembranca.titulo.empty()
                            ? base::UTF8ToUTF16(lembranca.url.host())
                            : lembranca.titulo;
    match.description_class = {{0, ACMatchClassification::NONE}};
    match.contents = lembranca.trecho;
    size_t pos = 0;
    for (const auto& [inicio, fim] : lembranca.destaques) {
      if (inicio < pos || fim <= inicio || fim > match.contents.size()) {
        continue;
      }
      match.contents_class.emplace_back(pos, ACMatchClassification::DIM);
      match.contents_class.emplace_back(inicio, ACMatchClassification::MATCH);
      pos = fim;
    }
    match.contents_class.emplace_back(pos, ACMatchClassification::DIM);
    // Sem pedaços vazios (dois na mesma posição) e começando do zero.
    ACMatchClassifications limpas;
    for (const ACMatchClassification& c : match.contents_class) {
      if (!limpas.empty() && limpas.back().offset == c.offset) {
        limpas.back() = c;
      } else if (c.offset < match.contents.size() || limpas.empty()) {
        limpas.push_back(c);
      }
    }
    match.contents_class = std::move(limpas);
    match.transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    matches_.push_back(std::move(match));
  }
  // Embaixo das páginas: o painel da Memória com o texto (mais resultados,
  // miniaturas, filtros).
  if (!lembrancas.empty()) {
    const std::u16string titulo = u"Buscar “" + texto + u"” na Memória";
    AutocompleteMatch match(
        this,
        kRelevanciaDaMemoria - static_cast<int>(kMaximoDaMemoria) - 1,
        /*deletable=*/false, AutocompleteMatchType::PEDAL);
    match.takeover_action = base::MakeRefCounted<AcaoDoNorte>(
        "memoria:" + base::UTF16ToUTF8(texto), titulo);
    match.contents = titulo;
    match.contents_class = {{0, ACMatchClassification::NONE}};
    match.fill_into_edit = texto;
    match.destination_url = GURL("chrome://norte/memoria");
    match.transition = ui::PAGE_TRANSITION_GENERATED;
    match.suggest_type = omnibox::TYPE_NATIVE_CHROME;
    matches_.push_back(std::move(match));
  }
  NotifyListeners(!lembrancas.empty());
}

void NavegadorProvider::Comandos(const AutocompleteInput& input,
                                 std::u16string_view busca) {
  const std::u16string dobrada =
      Dobrar(base::TrimWhitespace(busca, base::TRIM_ALL));
  const std::vector<std::u16string> palavras = base::SplitString(
      dobrada, u" ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Os que começam com a busca primeiro; depois os outros que acham todas
  // as palavras, na ordem da lista.
  std::vector<NavegadorComando> primeiros;
  std::vector<NavegadorComando> outros;
  for (NavegadorComando& comando : client_->NavegadorComandos()) {
    const std::u16string titulo = Dobrar(comando.titulo);
    if (!Acha(titulo + u" " + Dobrar(comando.palavras), palavras)) {
      continue;
    }
    (!dobrada.empty() && titulo.starts_with(dobrada) ? primeiros : outros)
        .push_back(std::move(comando));
  }
  primeiros.insert(primeiros.end(), std::make_move_iterator(outros.begin()),
                   std::make_move_iterator(outros.end()));

  for (size_t i = 0; i < primeiros.size() && i < kMaximoDeComandos; ++i) {
    const NavegadorComando& comando = primeiros[i];
    AutocompleteMatch match(this, kRelevanciaDoPrimeiro - static_cast<int>(i),
                            /*deletable=*/false, AutocompleteMatchType::PEDAL);
    match.takeover_action =
        base::MakeRefCounted<AcaoDoNorte>(comando.id, comando.titulo);
    match.contents = comando.titulo;
    match.contents_class = {{0, ACMatchClassification::NONE}};
    // O que foi digitado fica no campo; Enter roda o primeiro. O endereço
    // nunca é aberto (a ação toma o lugar), mas precisa ser válido e único:
    // sem ele o Enter buscava o texto, e iguais se juntariam.
    match.fill_into_edit = input.text();
    match.destination_url = GURL("chrome://norte/" + comando.id);
    match.transition = ui::PAGE_TRANSITION_GENERATED;
    match.suggest_type = omnibox::TYPE_NATIVE_CHROME;
    match.allowed_to_be_default_match = i == 0;
    matches_.push_back(std::move(match));
  }
}

void NavegadorProvider::Calculadora(const AutocompleteInput& input) {
  const std::u16string conta(
      base::TrimWhitespace(input.text(), base::TRIM_ALL));
  std::optional<std::u16string> resultado =
      Calcular(conta, client_->GetApplicationLocale().starts_with("pt"));
  if (!resultado) {
    return;
  }
  AutocompleteMatch match(this, kRelevanciaDaConta, /*deletable=*/false,
                          AutocompleteMatchType::CALCULATOR);
  match.contents = conta + u" = " + *resultado;
  match.contents_class = {{0, ACMatchClassification::NONE}};
  match.description = u"Escolher copia o resultado";
  match.description_class = {{0, ACMatchClassification::DIM}};
  match.fill_into_edit = *resultado;
  match.takeover_action = base::MakeRefCounted<AcaoDoNorte>(
      "copiar:" + base::UTF16ToUTF8(*resultado), u"Copiar " + *resultado);
  // O endereço de busca da conta, como o da resposta do buscador: com as
  // duas na lista, elas se juntam numa só (e a ação de copiar fica).
  GURL busca("chrome://norte/calculadora");
  if (TemplateURLService* servico = client_->GetTemplateURLService()) {
    if (const TemplateURL* buscador = servico->GetDefaultSearchProvider()) {
      const GURL url(buscador->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(conta),
          servico->search_terms_data()));
      if (url.is_valid()) {
        busca = url;
        // Com o buscador, o endereço "limpo" da comparação é o mesmo da
        // resposta dele (AutocompleteMatch::GURLToStrippedGURL).
        match.keyword = buscador->keyword();
      }
    }
  }
  match.destination_url = busca;
  match.transition = ui::PAGE_TRANSITION_GENERATED;
  matches_.push_back(std::move(match));
}
