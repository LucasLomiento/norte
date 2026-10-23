#include "navegador/browser/ui/containers/containers_ui.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/containers/fixos.h"
#include "navegador/browser/ui/containers/gerenciador.h"
#include "navegador/icones/menu/vector_icons.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace navegador::containers {

namespace {

// "Gerenciar containers…" dos submenus (definida mais abaixo, junto da ponte
// das Configurações).
void GerenciarContainers(content::WebContents* aba);

// "Novo container temporário": um relógio (o temporário some ao fechar a
// última aba dele, temporarios.h).
ui::ImageModel IconeDoTemporario() {
  return ui::ImageModel::FromVectorIcon(icones_menu::kTimerIcon,
                                        ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

// Os ids dos itens dos submenus são únicos no menu inteiro: o Views acha o
// comando pelo id, no primeiro item com ele (MenuModel::
// GetModelAndIndexForCommandId, em profundidade). Com ids repetidos, "Abrir
// em container → X" executava o "Mover para o espaço" de mesmo id, e
// "Recarregar" (1) e "Duplicar" (2) do menu da aba caíam no "Fixar no
// espaço". Faixas daqui: 21000 em diante (os IDC_ do Chromium começam em
// 33000; os comandos do menu da aba são < 100).

// Paleta dos containers do Firefox (os nomes são os do pref).
struct CorNomeada {
  const char* nome;
  SkColor cor;
};
constexpr CorNomeada kCores[] = {
    {"azul", SkColorSetRGB(0x37, 0xAD, 0xFF)},
    {"turquesa", SkColorSetRGB(0x00, 0xC7, 0x9A)},
    {"verde", SkColorSetRGB(0x51, 0xCD, 0x00)},
    {"amarelo", SkColorSetRGB(0xFF, 0xCB, 0x00)},
    {"laranja", SkColorSetRGB(0xFF, 0x9F, 0x00)},
    {"vermelho", SkColorSetRGB(0xFF, 0x61, 0x3D)},
    {"rosa", SkColorSetRGB(0xFF, 0x4B, 0xDA)},
    {"roxo", SkColorSetRGB(0xAF, 0x51, 0xF5)},
    {"cinza", SkColorSetRGB(0x8F, 0x8F, 0x9D)},
};

class BolinhaSource : public gfx::CanvasImageSource {
 public:
  explicit BolinhaSource(SkColor cor)
      : gfx::CanvasImageSource(gfx::Size(16, 16)), cor_(cor) {}

  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(cor_);
    canvas->DrawCircle(gfx::PointF(8, 8), 5, flags);
  }

 private:
  SkColor cor_;
};

std::optional<Container> ContainerDaAba(content::WebContents* contents) {
  if (!contents) {
    return std::nullopt;
  }
  std::optional<std::string> id = IdForWebContents(contents);
  if (!id) {
    return std::nullopt;
  }
  return Get(Profile::FromBrowserContext(contents->GetBrowserContext()), *id);
}

// "Abrir em container" no menu de contexto da aba.
class SubmenuDaAba : public ui::SimpleMenuModel,
                     public ui::SimpleMenuModel::Delegate {
 public:
  SubmenuDaAba(TabStripModel* tab_strip, int index)
      : ui::SimpleMenuModel(this),
        tab_strip_(tab_strip),
        alvo_(tab_strip->GetWebContentsAt(index)->GetWeakPtr()) {
    // Janela anônima não tem containers (como no Firefox).
    if (tab_strip->profile()->IsOffTheRecord()) {
      return;
    }
    std::optional<Container> atual = ContainerDaAba(alvo_.get());
    containers_ = GetAll(tab_strip->profile());
    for (size_t i = 0; i < containers_.size(); ++i) {
      if (atual && atual->id == containers_[i].id) {
        continue;
      }
      AddItemWithIcon(kPrimeiro + i, base::UTF8ToUTF16(containers_[i].name),
                      Bolinha(containers_[i].color));
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithIcon(kTemporario, u"Novo container temporário",
                    IconeDoTemporario());
    if (atual) {
      AddItem(kSemContainer, u"Sem container");
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kGerenciar, u"Gerenciar containers…");
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (!alvo_) {
      return;
    }
    if (command_id == kGerenciar) {
      GerenciarContainers(alvo_.get());
      return;
    }
    const int index = tab_strip_->GetIndexOfWebContents(alvo_.get());
    if (index == TabStripModel::kNoTab) {
      return;
    }
    std::string id;
    if (command_id == kTemporario) {
      id = AddTemporario(tab_strip_->profile()).id;
    } else if (command_id != kSemContainer) {
      const size_t i = command_id - kPrimeiro;
      if (i >= containers_.size()) {
        return;
      }
      id = containers_[i].id;
    }
    const GURL url = alvo_->GetLastCommittedURL().is_empty()
                         ? alvo_->GetVisibleURL()
                         : alvo_->GetLastCommittedURL();
    AbrirEmContainer(tab_strip_, url, id, index, /*ativar=*/true);
    tab_strip_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_USER_GESTURE);
  }

 private:
  static constexpr int kPrimeiro = 23100;
  static constexpr int kSemContainer = 23001;
  static constexpr int kGerenciar = 23002;
  static constexpr int kTemporario = 23003;

  raw_ptr<TabStripModel> tab_strip_;
  // A aba pode fechar com o menu aberto (o menu da aba não fecha junto).
  base::WeakPtr<content::WebContents> alvo_;
  std::vector<Container> containers_;
};

MoverAba& PonteMoverAba() {
  static base::NoDestructor<MoverAba> ponte;
  return *ponte;
}

FixarAba& PonteFixar() {
  static base::NoDestructor<FixarAba> ponte;
  return *ponte;
}

// "Fixar no espaço" no menu de contexto da aba.
class SubmenuFixar : public ui::SimpleMenuModel,
                     public ui::SimpleMenuModel::Delegate {
 public:
  SubmenuFixar(TabStripModel* tab_strip, int index)
      : ui::SimpleMenuModel(this),
        tab_strip_(tab_strip),
        alvo_(tab_strip->GetWebContentsAt(index)->GetWeakPtr()) {
    if (PonteFixar().is_null() || tab_strip->profile()->IsOffTheRecord()) {
      return;  // A janela anônima não tem fixados.
    }
    if (fixos::SiteDaAba(alvo_.get())) {
      AddItem(kDesafixar, u"Desafixar");
      return;
    }
    std::string espaco = espacos::Ativo(tab_strip->profile());
    if (std::optional<tab_groups::TabGroupId> grupo =
            tab_strip->GetTabGroupForTab(index)) {
      espaco = espacos::EspacoDoGrupo(tab_strip->profile(), grupo->ToString())
                   .value_or(espaco);
    }
    AddItem(kSolto, u"Fora das pastas");
    for (const fixos::Entrada& entrada :
         fixos::Do(tab_strip->profile(), espaco)) {
      if (entrada.pasta) {
        AddItemWithIcon(kPrimeiraPasta + pastas_.size(),
                        base::UTF8ToUTF16(entrada.nome),
                        ui::ImageModel::FromVectorIcon(
                            vector_icons::kFolderFlippableIcon, ui::kColorMenuIcon,
                            16));
        pastas_.push_back(entrada.id);
      }
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kNovaPasta, u"Nova pasta…");
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    const int index = tab_strip_->GetIndexOfWebContents(alvo_.get());
    if (index == TabStripModel::kNoTab || PonteFixar().is_null()) {
      return;
    }
    std::string destino;
    if (command_id == kDesafixar) {
      destino = kFixarDesafixar;
    } else if (command_id == kNovaPasta) {
      destino = kFixarNovaPasta;
    } else if (command_id != kSolto) {
      const size_t i = command_id - kPrimeiraPasta;
      if (i >= pastas_.size()) {
        return;
      }
      destino = pastas_[i];
    }
    PonteFixar().Run(tab_strip_, index, destino);
  }

 private:
  static constexpr int kSolto = 22001;
  static constexpr int kNovaPasta = 22002;
  static constexpr int kDesafixar = 22003;
  static constexpr int kPrimeiraPasta = 22100;

  raw_ptr<TabStripModel> tab_strip_;
  base::WeakPtr<content::WebContents> alvo_;
  std::vector<std::string> pastas_;
};

AbrirConfiguracoes& PonteConfiguracoes() {
  static base::NoDestructor<AbrirConfiguracoes> ponte;
  return *ponte;
}

// "Gerenciar containers…": a página Containers das Configurações, na janela
// da aba (antes, um diálogo à parte, com outro desenho).
void GerenciarContainers(content::WebContents* aba) {
  tabs::TabInterface* tab =
      aba ? tabs::TabInterface::MaybeGetFromContents(aba) : nullptr;
  if (tab && tab->GetBrowserWindowInterface() &&
      !PonteConfiguracoes().is_null()) {
    PonteConfiguracoes().Run(tab->GetBrowserWindowInterface(),
                             /*containers=*/true);
  }
}

// "Mover para o espaço" no menu de contexto da aba.
class SubmenuDeEspacos : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  SubmenuDeEspacos(TabStripModel* tab_strip, int index)
      : ui::SimpleMenuModel(this),
        tab_strip_(tab_strip),
        alvo_(tab_strip->GetWebContentsAt(index)->GetWeakPtr()) {
    if (tab_strip->IsTabPinned(index) || PonteMoverAba().is_null() ||
        tab_strip->profile()->IsOffTheRecord()) {
      return;  // A janela anônima não tem espaços.
    }
    std::optional<std::string> atual;
    if (std::optional<tab_groups::TabGroupId> grupo =
            tab_strip->GetTabGroupForTab(index)) {
      atual = espacos::EspacoDoGrupo(tab_strip->profile(), grupo->ToString());
    }
    for (const espacos::Espaco& espaco : espacos::Todos(tab_strip->profile())) {
      if (espaco.id == atual) {
        continue;
      }
      std::u16string nome = base::UTF8ToUTF16(espaco.nome);
      if (!espaco.icone.empty()) {
        nome = base::UTF8ToUTF16(espaco.icone) + u" " + nome;
      }
      AddItemWithIcon(kPrimeiro + espacos_.size(), nome, Bolinha(espaco.cor));
      espacos_.push_back(espaco.id);
    }
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    const size_t i = command_id - kPrimeiro;
    const int index = tab_strip_->GetIndexOfWebContents(alvo_.get());
    if (i >= espacos_.size() || index == TabStripModel::kNoTab ||
        PonteMoverAba().is_null()) {
      return;
    }
    PonteMoverAba().Run(tab_strip_, index, espacos_[i]);
  }

 private:
  static constexpr int kPrimeiro = 21000;

  raw_ptr<TabStripModel> tab_strip_;
  base::WeakPtr<content::WebContents> alvo_;
  std::vector<std::string> espacos_;
};

// "Abrir link em container" no menu de contexto da página.
class SubmenuDoLink : public ui::SimpleMenuModel,
                      public ui::SimpleMenuModel::Delegate {
 public:
  SubmenuDoLink(TabStripModel* tab_strip,
                content::WebContents* origem,
                const GURL& link)
      : ui::SimpleMenuModel(this),
        tab_strip_(tab_strip),
        origem_(origem->GetWeakPtr()),
        link_(link),
        containers_(GetAll(tab_strip->profile())) {
    for (size_t i = 0; i < containers_.size(); ++i) {
      AddItemWithIcon(kPrimeiro + i, base::UTF8ToUTF16(containers_[i].name),
                      Bolinha(containers_[i].color));
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithIcon(kTemporario, u"Novo container temporário",
                    IconeDoTemporario());
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kGerenciar, u"Gerenciar containers…");
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == kGerenciar) {
      if (origem_) {
        GerenciarContainers(origem_.get());
      }
      return;
    }
    std::string id;
    if (command_id == kTemporario) {
      id = AddTemporario(tab_strip_->profile()).id;
    } else {
      const size_t i = command_id - kPrimeiro;
      if (i >= containers_.size()) {
        return;
      }
      id = containers_[i].id;
    }
    int index = tab_strip_->GetIndexOfWebContents(origem_.get());
    if (index == TabStripModel::kNoTab) {
      index = tab_strip_->count() - 1;
    }
    AbrirEmContainer(tab_strip_, link_, id, index, /*ativar=*/false);
  }

 private:
  static constexpr int kPrimeiro = 24100;
  static constexpr int kGerenciar = 24002;
  static constexpr int kTemporario = 24003;

  raw_ptr<TabStripModel> tab_strip_;
  base::WeakPtr<content::WebContents> origem_;
  GURL link_;
  std::vector<Container> containers_;
};

// "Nova aba em container" no menu do botão "Nova aba".
class SubmenuNovaAba : public ui::SimpleMenuModel,
                       public ui::SimpleMenuModel::Delegate {
 public:
  explicit SubmenuNovaAba(TabStripModel* tab_strip)
      : ui::SimpleMenuModel(this),
        tab_strip_(tab_strip),
        containers_(GetAll(tab_strip->profile())) {
    if (tab_strip->profile()->IsOffTheRecord()) {
      return;
    }
    for (size_t i = 0; i < containers_.size(); ++i) {
      AddItemWithIcon(kPrimeiro + i, base::UTF8ToUTF16(containers_[i].name),
                      Bolinha(containers_[i].color));
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithIcon(kTemporario, u"Novo container temporário",
                    IconeDoTemporario());
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kGerenciar, u"Gerenciar containers…");
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == kGerenciar) {
      GerenciarContainers(tab_strip_->GetActiveWebContents());
      return;
    }
    std::string id;
    if (command_id == kTemporario) {
      id = AddTemporario(tab_strip_->profile()).id;
    } else {
      const size_t i = command_id - kPrimeiro;
      if (i >= containers_.size()) {
        return;
      }
      id = containers_[i].id;
    }
    // No fim do espaço da janela (sem grupo, a aba nasceria fora dele).
    int index = tab_strip_->count() - 1;
    if (std::optional<tab_groups::TabGroupId> grupo =
            tab_strip_->GetFocusedGroup();
        grupo && tab_strip_->group_model()->ContainsTabGroup(*grupo)) {
      index = tab_strip_->group_model()->GetTabGroup(*grupo)->ListTabs().end() -
              1;
    }
    AbrirEmContainer(tab_strip_, GURL("chrome://newtab/"), id, index,
                     /*ativar=*/true);
  }

 private:
  static constexpr int kPrimeiro = 25100;
  static constexpr int kGerenciar = 25002;
  static constexpr int kTemporario = 25003;

  raw_ptr<TabStripModel> tab_strip_;
  std::vector<Container> containers_;
};

}  // namespace

SkColor Cor(std::string_view nome) {
  for (const CorNomeada& c : kCores) {
    if (nome == c.nome) {
      return c.cor;
    }
  }
  // Cor escolhida no seletor: "#RRGGBB".
  uint32_t rgb = 0;
  if (nome.size() == 7 && nome[0] == '#' &&
      base::HexStringToUInt(nome.substr(1), &rgb)) {
    return SkColorSetA(static_cast<SkColor>(rgb), SK_AlphaOPAQUE);
  }
  return kCores[0].cor;
}

std::string Hex(SkColor cor) {
  return base::StringPrintf("#%02X%02X%02X", SkColorGetR(cor),
                            SkColorGetG(cor), SkColorGetB(cor));
}

std::string NomeMaisProximo(std::string_view cor) {
  for (const CorNomeada& c : kCores) {
    if (cor == c.nome) {
      return c.nome;
    }
  }
  // Pelo matiz (o cinza só pra cores sem saturação), que é o que a cor do
  // grupo do Chromium carrega.
  const SkColor alvo = Cor(cor);
  color_utils::HSL hsl_alvo;
  color_utils::SkColorToHSL(alvo, &hsl_alvo);
  if (hsl_alvo.s < 0.15) {
    return "cinza";
  }
  std::string melhor = kCores[0].nome;
  double menor = 10;
  for (const CorNomeada& c : kCores) {
    if (std::string_view(c.nome) == "cinza") {
      continue;
    }
    color_utils::HSL hsl;
    color_utils::SkColorToHSL(c.cor, &hsl);
    double d = std::abs(hsl.h - hsl_alvo.h);
    d = std::min(d, 1 - d);
    if (d < menor) {
      menor = d;
      melhor = c.nome;
    }
  }
  return melhor;
}

ui::ImageModel Bolinha(std::string_view nome) {
  return ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<BolinhaSource>(Cor(nome)));
}

void PintarIndicador(gfx::Canvas* canvas,
                     const views::View& aba,
                     const tabs::TabInterface* tab) {
  if (!tab) {
    return;
  }
  std::optional<Container> container = ContainerDaAba(tab->GetContents());
  if (!container) {
    return;
  }
  // Barrinha de 3 na borda esquerda, com metade da altura da aba.
  const gfx::Rect bounds = aba.GetLocalBounds();
  const float altura = bounds.height() / 2.0f;
  const gfx::RectF barra(2, (bounds.height() - altura) / 2.0f, 3, altura);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(Cor(container->color));
  canvas->DrawRoundRect(barra, 1.5f, flags);
}

int AbrirEmContainer(TabStripModel* tab_strip,
                     const GURL& url,
                     std::string_view id,
                     int index,
                     bool ativar) {
  Profile* profile = tab_strip->profile();
  scoped_refptr<content::SiteInstance> site_instance =
      id.empty() ? content::SiteInstance::CreateForURL(profile, url)
                 : CreateSiteInstance(profile, url, id);
  content::WebContents::CreateParams params(profile, site_instance);
  params.initially_hidden = !ativar;
  // No grupo (espaço) da aba de referência: inserir sem grupo no meio de um
  // grupo dá CHECK no TabCollection::ToDirectIndex, e no fim dele tiraria a
  // janela do espaço.
  std::optional<tab_groups::TabGroupId> grupo;
  if (index >= 0 && index < tab_strip->count()) {
    grupo = tab_strip->GetTabGroupForTab(index);
  }
  const int inserida = tab_strip->InsertWebContentsAt(
      index + 1, content::WebContents::Create(params),
      ativar ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE, grupo);
  content::NavigationController::LoadURLParams load(url);
  load.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  tab_strip->GetWebContentsAt(inserida)->GetController().LoadURLWithParams(
      load);
  return inserida;
}

void ReabrirEmContainer(TabStripModel* tab_strip,
                        int index,
                        std::string_view id) {
  content::WebContents* original = tab_strip->GetWebContentsAt(index);
  if (!original) {
    return;
  }
  const GURL url = original->GetLastCommittedURL().is_empty()
                       ? original->GetVisibleURL()
                       : original->GetLastCommittedURL();
  const int nova = AbrirEmContainer(tab_strip, url, id, index,
                                    /*ativar=*/tab_strip->active_index() ==
                                        index);
  if (tab_strip->IsTabPinned(index)) {
    tab_strip->SetTabPinned(nova, true);
  }
  tab_strip->CloseWebContentsAt(tab_strip->GetIndexOfWebContents(original),
                                TabCloseTypes::CLOSE_USER_GESTURE);
}

std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDaAba(TabStripModel* tab_strip,
                                                       int index) {
  return std::make_unique<SubmenuDaAba>(tab_strip, index);
}

void DefinirMoverAba(MoverAba mover) {
  PonteMoverAba() = std::move(mover);
}

void DefinirFixarAba(FixarAba fixar) {
  PonteFixar() = std::move(fixar);
}

void FixarAbaNoEspaco(TabStripModel* tab_strip,
                      int index,
                      const std::string& destino) {
  if (!PonteFixar().is_null() && !tab_strip->profile()->IsOffTheRecord()) {
    PonteFixar().Run(tab_strip, index, destino);
  }
}

std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuFixar(TabStripModel* tab_strip,
                                                       int index) {
  return std::make_unique<SubmenuFixar>(tab_strip, index);
}

namespace {

ExecutarAcaoDaJanela& PonteDasAcoes() {
  static base::NoDestructor<ExecutarAcaoDaJanela> ponte;
  return *ponte;
}

LerAtalhoDaAcao& PonteDosAtalhos() {
  static base::NoDestructor<LerAtalhoDaAcao> ponte;
  return *ponte;
}

}  // namespace

void DefinirAtalhoDaAcao(LerAtalhoDaAcao ler) {
  PonteDosAtalhos() = std::move(ler);
}

std::optional<ui::Accelerator> AtalhoDaAcao(Profile* profile,
                                            const std::string& acao) {
  if (PonteDosAtalhos().is_null()) {
    return std::nullopt;
  }
  return PonteDosAtalhos().Run(profile, acao);
}

void DefinirExecutarAcao(ExecutarAcaoDaJanela executar) {
  PonteDasAcoes() = std::move(executar);
}

void ExecutarAcao(BrowserWindowInterface* browser, const std::string& acao) {
  if (!PonteDasAcoes().is_null()) {
    PonteDasAcoes().Run(browser, acao);
  }
}

void DefinirAbrirConfiguracoes(AbrirConfiguracoes abrir) {
  PonteConfiguracoes() = std::move(abrir);
}

void MostrarConfiguracoes(BrowserWindowInterface* browser, bool containers) {
  if (!PonteConfiguracoes().is_null()) {
    PonteConfiguracoes().Run(browser, containers);
  }
}

std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDeEspacos(
    TabStripModel* tab_strip,
    int index) {
  return std::make_unique<SubmenuDeEspacos>(tab_strip, index);
}

std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuDoLink(
    TabStripModel* tab_strip,
    content::WebContents* origem,
    const GURL& link) {
  return std::make_unique<SubmenuDoLink>(tab_strip, origem, link);
}

std::unique_ptr<ui::SimpleMenuModel> CriarSubmenuNovaAba(
    TabStripModel* tab_strip) {
  return std::make_unique<SubmenuNovaAba>(tab_strip);
}

}  // namespace navegador::containers
