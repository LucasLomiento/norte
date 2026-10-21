#include "navegador/browser/ui/containers/gerenciador.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
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
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
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

// A lista de containers editável no lugar: no diálogo "Containers" e na aba
// Containers das Configurações do Navegador.
class PainelDeContainers : public views::View,
                           public views::TextfieldController {
  METADATA_HEADER(PainelDeContainers, views::View)

 public:
  explicit PainelDeContainers(Profile* profile) : profile_(profile) {
    views::View* conteudo = this;
    conteudo->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets::TLBR(0, 16, 16, 16),
        10));
    auto* legenda = conteudo->AddChildView(std::make_unique<views::Label>(
        u"Cada container tem cookies, logins e dados dos sites separados. "
        u"Clique na bolinha pra trocar a cor e no nome pra renomear."));
    legenda->SetMultiLine(true);
    legenda->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    legenda->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    legenda->SizeToFit(300);
    lista_ = conteudo->AddChildView(std::make_unique<views::View>());
    lista_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));
    // "+ Novo container" como linha de texto, no estilo do "+ Nova guia" da
    // lateral.
    auto* novo = conteudo->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&PainelDeContainers::Novo,
                            base::Unretained(this)),
        u"Novo container"));
    novo->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            kAddIcon, ui::kColorIconSecondary, 16));
    novo->SetImageLabelSpacing(20);
    novo->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 6)));
    novo->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    views::InkDrop::Get(novo)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InstallRoundRectHighlightPathGenerator(novo, gfx::Insets(), 6);
    Reconstruir();
  }

  // views::TextfieldController: renomeia enquanto digita.
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    for (const Linha& linha : linhas_) {
      if (linha.nome == sender) {
        if (std::optional<Container> c = Get(profile_, linha.id)) {
          c->name = base::UTF16ToUTF8(new_contents);
          Update(profile_, *c);
        }
        return;
      }
    }
  }

 private:
  struct Linha {
    std::string id;
    raw_ptr<views::Textfield> nome;
  };

  void Reconstruir(const std::string& focar = std::string()) {
    linhas_.clear();
    lista_->RemoveAllChildViews();
    for (const Container& c : GetAll(profile_)) {
      auto* linha = lista_->AddChildView(std::make_unique<views::View>());
      auto* layout =
          linha->SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);

      linha->AddChildView(std::make_unique<BotaoCor>(
          c.color, base::BindRepeating(&PainelDeContainers::TrocarCor,
                                       base::Unretained(this), c.id)));
      auto* nome = linha->AddChildView(std::make_unique<views::Textfield>());
      nome->SetText(base::UTF8ToUTF16(c.name));
      nome->SetDefaultWidthInChars(18);
      nome->SetAccessibleName(u"Nome do container");
      nome->set_controller(this);
      // Texto solto, editável no lugar: sem a caixa de formulário (o anel de
      // foco aparece ao clicar).
      nome->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 6)));
      nome->SetBackgroundEnabled(false);
      nome->RemoveHoverEffect();
      layout->SetFlexForView(nome, 1);
      auto* excluir =
          linha->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
              base::BindRepeating(&PainelDeContainers::Excluir,
                                  base::Unretained(this), c.id),
              kDeleteIcon, 16));
      excluir->SetTooltipText(u"Excluir container");
      linhas_.push_back({c.id, nome});
      if (c.id == focar) {
        nome->RequestFocus();
        nome->SelectAll(false);
      }
    }
    PreferredSizeChanged();
    if (GetWidget() && GetWidget()->non_client_view()) {
      GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
    }
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

  // Apaga os dados do container (cookies, armazenamento, cache) e tira da
  // lista. Abas abertas nele continuam abertas, sem login.
  void Excluir(std::string id) {
    ApagarDados(profile_, id);
    Remove(profile_, id);
    Reconstruir();
  }

  raw_ptr<Profile> profile_;
  raw_ptr<views::View> lista_ = nullptr;
  std::vector<Linha> linhas_;
};

BEGIN_METADATA(PainelDeContainers)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CriarPainel(Profile* profile) {
  return std::make_unique<PainelDeContainers>(profile);
}

}  // namespace navegador::containers
