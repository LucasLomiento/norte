#include "navegador/browser/ui/containers/gerenciador.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace navegador::containers {

namespace {

// Ordem do rodízio de cores (clicar na bolinha passa pra próxima).
constexpr const char* kPaleta[] = {"azul",    "turquesa", "verde",
                                   "amarelo", "laranja",  "vermelho",
                                   "rosa",    "roxo",     "cinza"};

const char* ProximaCor(const std::string& atual) {
  for (size_t i = 0; i < std::size(kPaleta); ++i) {
    if (atual == kPaleta[i]) {
      return kPaleta[(i + 1) % std::size(kPaleta)];
    }
  }
  return kPaleta[0];
}

// Bolinha da cor do container; clicar troca a cor.
class BotaoCor : public views::Button {
  METADATA_HEADER(BotaoCor, views::Button)

 public:
  BotaoCor(std::string cor, PressedCallback ao_clicar)
      : views::Button(std::move(ao_clicar)), cor_(std::move(cor)) {
    SetPreferredSize(gfx::Size(28, 28));
    SetTooltipText(u"Trocar a cor");
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::PointF centro(width() / 2.0f, height() / 2.0f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    // Com o mouse em cima, um anel fino em volta: dá pra clicar.
    if (IsMouseHovered() || HasFocus()) {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1.5f);
      flags.setColor(SkColorSetA(Cor(cor_), 0x80));
      canvas->DrawCircle(centro, 11, flags);
      flags.setStyle(cc::PaintFlags::kFill_Style);
    }
    flags.setColor(Cor(cor_));
    canvas->DrawCircle(centro, 7, flags);
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    SchedulePaint();
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    SchedulePaint();
  }

 private:
  std::string cor_;
};

BEGIN_METADATA(BotaoCor)
END_METADATA

// O nome do container, editável no lugar. Saindo do campo vazio, volta o
// último nome gravado.
class CampoDoNome : public views::Textfield {
  METADATA_HEADER(CampoDoNome, views::Textfield)

 public:
  explicit CampoDoNome(base::RepeatingCallback<std::u16string()> gravado)
      : gravado_(std::move(gravado)) {}

  void OnBlur() override {
    views::Textfield::OnBlur();
    if (base::TrimWhitespace(GetText(), base::TRIM_ALL).empty()) {
      SetText(gravado_.Run());
    }
  }

 private:
  base::RepeatingCallback<std::u16string()> gravado_;
};

BEGIN_METADATA(CampoDoNome)
END_METADATA

// A lista de containers editável no lugar, na página Containers das
// Configurações do Norte: uma linha por container (bolinha da cor, nome,
// lixeira), separadas por um fio como as outras linhas de cartão.
class PainelDeContainers : public views::View,
                           public views::TextfieldController {
  METADATA_HEADER(PainelDeContainers, views::View)

 public:
  explicit PainelDeContainers(Profile* profile) : profile_(profile) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    lista_ = AddChildView(std::make_unique<views::View>());
    lista_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    // "+ Novo container" como linha de texto, no estilo do "+ Nova aba" da
    // lateral.
    auto* novo = AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&PainelDeContainers::Novo,
                            base::Unretained(this)),
        u"Novo container"));
    novo->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            kAddIcon, ui::kColorIconSecondary, 16));
    novo->SetImageLabelSpacing(16);
    novo->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 18)));
    novo->SetMinSize(gfx::Size(0, kAlturaDaLinha));
    novo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    novo->SetLabelStyle(views::style::STYLE_BODY_2);
    views::InkDrop::Get(novo)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(novo)->SetBaseColor(ui::kColorSysOnSurface);
    views::InstallRoundRectHighlightPathGenerator(novo, gfx::Insets(3), 9);
    Reconstruir();
  }

  // views::TextfieldController: renomeia enquanto digita. Nome vazio não é
  // gravado (o container ficava sem nome nos menus).
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    const std::u16string_view nome =
        base::TrimWhitespace(new_contents, base::TRIM_ALL);
    if (nome.empty()) {
      return;
    }
    for (const Linha& linha : linhas_) {
      if (linha.nome == sender) {
        if (std::optional<Container> c = Get(profile_, linha.id)) {
          c->name = base::UTF16ToUTF8(nome);
          Update(profile_, *c);
        }
        return;
      }
    }
  }

 private:
  static constexpr int kAlturaDaLinha = 44;

  struct Linha {
    std::string id;
    raw_ptr<views::Textfield> nome;
    raw_ptr<views::View> lixeira;
    raw_ptr<views::View> confirmar;
  };

  static gfx::FontList FonteDoNome() {
    return gfx::FontList().DeriveWithSizeDelta(14 -
                                               gfx::FontList().GetFontSize());
  }

  static std::u16string NomeGravado(Profile* profile, const std::string& id) {
    std::optional<Container> c = Get(profile, id);
    return c ? base::UTF8ToUTF16(c->name) : std::u16string();
  }

  void Reconstruir(const std::string& focar = std::string()) {
    desistir_.Stop();
    // Os ponteiros das linhas morrem com as views.
    linhas_.clear();
    lista_->RemoveAllChildViews();
    for (const Container& c : GetAll(profile_)) {
      auto* linha = lista_->AddChildView(std::make_unique<views::View>());
      auto* layout =
          linha->SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets::TLBR(0, 12, 0, 10), 6));
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      layout->set_minimum_cross_axis_size(kAlturaDaLinha);

      linha->AddChildView(std::make_unique<BotaoCor>(
          c.color, base::BindRepeating(&PainelDeContainers::TrocarCor,
                                       base::Unretained(this), c.id)));
      auto* nome = linha->AddChildView(std::make_unique<CampoDoNome>(
          base::BindRepeating(&PainelDeContainers::NomeGravado,
                              base::Unretained(profile_.get()), c.id)));
      nome->SetText(base::UTF8ToUTF16(c.name));
      nome->SetDefaultWidthInChars(18);
      nome->SetAccessibleName(u"Nome do container");
      nome->set_controller(this);
      nome->SetFontList(FonteDoNome());
      // Texto solto, editável no lugar: sem a caixa de formulário (o anel de
      // foco aparece ao clicar).
      nome->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(6, 6)));
      nome->SetBackgroundEnabled(false);
      nome->RemoveHoverEffect();
      layout->SetFlexForView(nome, 1);

      auto* lixeira =
          linha->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
              base::BindRepeating(&PainelDeContainers::PedirExclusao,
                                  base::Unretained(this), c.id),
              kDeleteIcon, 16));
      lixeira->SetTooltipText(u"Excluir container");
      // Excluir apaga os logins do container e não tem volta: a lixeira só
      // pergunta, e este botão (no lugar dela por uns segundos) confirma.
      auto confirmar = std::make_unique<views::MdTextButton>(
          base::BindRepeating(&PainelDeContainers::Excluir,
                              base::Unretained(this), c.id),
          u"Excluir e apagar os dados");
      confirmar->SetEnabledTextColors(ui::kColorSysError);
      confirmar->SetBgColorIdOverride(ui::kColorSysErrorContainer);
      confirmar->SetStrokeColorIdOverride(ui::kColorSysErrorContainer);
      confirmar->SetVisible(false);
      views::View* botao = linha->AddChildView(std::move(confirmar));
      linhas_.push_back({c.id, nome, lixeira, botao});
      lista_->AddChildView(std::make_unique<views::Separator>())
          ->SetColorId(ui::kColorSeparator);
      if (c.id == focar) {
        nome->RequestFocus();
        nome->SelectAll(false);
      }
    }
    PreferredSizeChanged();
  }

  void Novo() {
    const std::vector<Container> todos = GetAll(profile_);
    const std::string cor = todos.empty() ? kPaleta[0]
                                          : ProximaCor(todos.back().color);
    Reconstruir(Add(profile_, "Novo container", cor, "").id);
  }

  void TrocarCor(std::string id) {
    if (std::optional<Container> c = Get(profile_, id)) {
      c->color = ProximaCor(c->color);
      Update(profile_, *c);
      Reconstruir();
    }
  }

  // Primeiro clique: a lixeira da linha vira o botão de confirmar, que some
  // sozinho depois de uns segundos (ou ao pedir a exclusão de outra linha).
  void PedirExclusao(std::string id) {
    for (const Linha& linha : linhas_) {
      const bool esta = linha.id == id;
      linha.lixeira->SetVisible(!esta);
      linha.confirmar->SetVisible(esta);
    }
    if (id.empty()) {
      desistir_.Stop();
    } else {
      desistir_.Start(FROM_HERE, base::Seconds(5),
                      base::BindOnce(&PainelDeContainers::PedirExclusao,
                                     base::Unretained(this), std::string()));
    }
  }

  // Apaga os dados do container (cookies, armazenamento, cache) e tira da
  // lista. As abas abertas nele reabrem fora de container (sem o login).
  void Excluir(std::string id) {
    TirarAbasDoContainer(profile_, id);
    ApagarDados(profile_, id);
    Remove(profile_, id);
    Reconstruir();
  }

  raw_ptr<Profile> profile_;
  raw_ptr<views::View> lista_ = nullptr;
  std::vector<Linha> linhas_;
  base::OneShotTimer desistir_;
};

BEGIN_METADATA(PainelDeContainers)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CriarPainel(Profile* profile) {
  return std::make_unique<PainelDeContainers>(profile);
}

}  // namespace navegador::containers
