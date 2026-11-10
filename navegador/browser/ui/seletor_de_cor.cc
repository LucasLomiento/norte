#include "navegador/browser/ui/seletor_de_cor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace navegador::seletor_de_cor {

namespace {

constexpr int kAmostra = 22;  // diâmetro da bolinha da paleta
constexpr int kColunas = 9;
constexpr int kAlturaQuadrado = 120;
constexpr int kAlturaMatiz = 12;
constexpr float kRaio = 8;

struct Predefinida {
  const char* cor;
  const char16_t* nome;
};

// Primeira linha: a paleta de sempre (a dos containers); segunda: tons mais
// suaves.
constexpr Predefinida kPaleta[] = {
    {"azul", u"Azul"},          {"turquesa", u"Turquesa"},
    {"verde", u"Verde"},        {"amarelo", u"Amarelo"},
    {"laranja", u"Laranja"},    {"vermelho", u"Vermelho"},
    {"rosa", u"Rosa"},          {"roxo", u"Roxo"},
    {"cinza", u"Cinza"},        {"#6C7BFF", u"Anil"},
    {"#4FC3F7", u"Céu"},        {"#5ED6A8", u"Menta"},
    {"#E6C36A", u"Areia"},      {"#F2A07B", u"Pêssego"},
    {"#F07A8A", u"Coral"},      {"#D78BE0", u"Orquídea"},
    {"#A58BFF", u"Lavanda"},    {"#5C6370", u"Grafite"},
};

struct Hsv {
  float h = 0;  // 0..360
  float s = 1;  // 0..1
  float v = 1;  // 0..1
};

Hsv ParaHsv(SkColor cor) {
  SkScalar hsv[3];
  SkColorToHSV(cor, hsv);
  return {hsv[0], hsv[1], hsv[2]};
}

SkColor DeHsv(const Hsv& cor) {
  const SkScalar hsv[3] = {cor.h, cor.s, cor.v};
  return SkHSVToColor(SK_AlphaOPAQUE, hsv);
}

// Marcador redondo (quadrado e barra de matiz): anel branco com contorno.
// O raio de fora do marcador (com o contorno).
constexpr float kRaioDoMarcador = 8.5f;

void PintarMarcador(gfx::Canvas* canvas, const gfx::PointF& centro) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setColor(SkColorSetA(SK_ColorBLACK, 0x55));
  canvas->DrawCircle(centro, 8, flags);
  flags.setStrokeWidth(2.5f);
  flags.setColor(SK_ColorWHITE);
  canvas->DrawCircle(centro, 6.5f, flags);
}

// Bolinha da paleta.
class Amostra : public views::Button {
  METADATA_HEADER(Amostra, views::Button)

 public:
  Amostra(std::string cor, std::u16string nome, PressedCallback callback)
      : views::Button(std::move(callback)), cor_(std::move(cor)) {
    SetPreferredSize(gfx::Size(kAmostra + 8, kAmostra + 8));
    SetTooltipText(nome);
    GetViewAccessibility().SetName(nome);
    views::InstallCircleHighlightPathGenerator(this);
  }

  const std::string& cor() const { return cor_; }

  void SetMarcada(bool marcada) {
    if (marcada_ != marcada) {
      marcada_ = marcada;
      SchedulePaint();
    }
  }

  // As cores vêm do tema e a pintura fica guardada: sem repintar, o desenho
  // ficava com as cores de antes ao trocar claro/escuro ou a cor do espaço.
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::PointF centro(width() / 2.0f, height() / 2.0f);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (marcada_ || GetState() == STATE_HOVERED) {
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(2);
      const SkColor anel =
          GetColorProvider()->GetColor(ui::kColorLabelForeground);
      flags.setColor(marcada_ ? anel : SkColorSetA(anel, 0x55));
      canvas->DrawCircle(centro, kAmostra / 2.0f + 2.5f, flags);
      flags.setStyle(cc::PaintFlags::kFill_Style);
    }
    flags.setColor(containers::Cor(cor_));
    canvas->DrawCircle(centro, kAmostra / 2.0f - (marcada_ ? 1.5f : 0), flags);
  }

 private:
  std::string cor_;
  bool marcada_ = false;
};

BEGIN_METADATA(Amostra)
END_METADATA

// Saturação (horizontal) e brilho (vertical) no matiz atual.
class QuadradoSV : public views::View {
  METADATA_HEADER(QuadradoSV, views::View)

 public:
  explicit QuadradoSV(base::RepeatingCallback<void(float, float)> mudou)
      : mudou_(std::move(mudou)) {
    GetViewAccessibility().SetName(u"Saturação e brilho");
  }

  void Definir(const Hsv& cor) {
    cor_ = cor;
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, kAlturaQuadrado);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::RectF area(GetLocalBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(DeHsv({cor_.h, 1, 1}));
    canvas->DrawRoundRect(area, kRaio, flags);

    // Branco -> transparente (saturação) e transparente -> preto (brilho).
    const SkPoint horizontal[2] = {{area.x(), 0}, {area.right(), 0}};
    const SkColor4f branco[2] = {SkColors::kWhite, {1, 1, 1, 0}};
    flags.setColor(SK_ColorWHITE);
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        horizontal, branco, nullptr, 2, SkTileMode::kClamp));
    canvas->DrawRoundRect(area, kRaio, flags);
    const SkPoint vertical[2] = {{0, area.y()}, {0, area.bottom()}};
    const SkColor4f preto[2] = {{0, 0, 0, 0}, SkColors::kBlack};
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        vertical, preto, nullptr, 2, SkTileMode::kClamp));
    canvas->DrawRoundRect(area, kRaio, flags);

    // O marcador inteiro dentro do quadro: nas beiras (uma cor viva tem
    // brilho 100%, no alto) metade dele ficava cortada.
    PintarMarcador(
        canvas,
        gfx::PointF(std::clamp(area.x() + cor_.s * area.width(),
                               area.x() + kRaioDoMarcador,
                               area.right() - kRaioDoMarcador),
                    std::clamp(area.y() + (1 - cor_.v) * area.height(),
                               area.y() + kRaioDoMarcador,
                               area.bottom() - kRaioDoMarcador)));
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    Mover(event.location());
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    Mover(event.location());
    return true;
  }

 private:
  void Mover(const gfx::Point& ponto) {
    if (width() <= 0 || height() <= 0) {
      return;
    }
    cor_.s = std::clamp(ponto.x() / static_cast<float>(width()), 0.0f, 1.0f);
    cor_.v =
        1 - std::clamp(ponto.y() / static_cast<float>(height()), 0.0f, 1.0f);
    SchedulePaint();
    mudou_.Run(cor_.s, cor_.v);
  }

  Hsv cor_;
  base::RepeatingCallback<void(float, float)> mudou_;
};

BEGIN_METADATA(QuadradoSV)
END_METADATA

class BarraDeMatiz : public views::View {
  METADATA_HEADER(BarraDeMatiz, views::View)

 public:
  explicit BarraDeMatiz(base::RepeatingCallback<void(float)> mudou)
      : mudou_(std::move(mudou)) {
    GetViewAccessibility().SetName(u"Matiz");
  }

  void Definir(float matiz) {
    matiz_ = matiz;
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Espaço pro marcador, que passa da barra.
    return gfx::Size(0, kAlturaMatiz + 6);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::RectF barra(GetLocalBounds());
    barra.Inset(gfx::InsetsF::VH(3, 0));
    std::vector<SkColor4f> cores;
    std::vector<SkScalar> posicoes;
    for (int i = 0; i <= 6; ++i) {
      cores.push_back(SkColor4f::FromColor(DeHsv({i * 60.0f, 1, 1})));
      posicoes.push_back(i / 6.0f);
    }
    const SkPoint pontos[2] = {{barra.x(), 0}, {barra.right(), 0}};
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorWHITE);
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        pontos, cores.data(), posicoes.data(), cores.size(),
        SkTileMode::kClamp));
    canvas->DrawRoundRect(barra, barra.height() / 2, flags);
    PintarMarcador(canvas,
                   gfx::PointF(std::clamp(barra.x() + matiz_ / 360 * barra.width(),
                                          barra.x() + kRaioDoMarcador,
                                          barra.right() - kRaioDoMarcador),
                               barra.CenterPoint().y()));
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    Mover(event.location());
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    Mover(event.location());
    return true;
  }

 private:
  void Mover(const gfx::Point& ponto) {
    if (width() <= 0) {
      return;
    }
    // 360 é o mesmo vermelho do 0: fica em 359,9 pra o marcador não pular.
    matiz_ = std::clamp(ponto.x() / static_cast<float>(width()), 0.0f,
                        0.9997f) *
             360;
    SchedulePaint();
    mudou_.Run(matiz_);
  }

  float matiz_ = 0;
  base::RepeatingCallback<void(float)> mudou_;
};

BEGIN_METADATA(BarraDeMatiz)
END_METADATA

// A cor escolhida, redonda, ao lado do código.
class Previa : public views::View {
  METADATA_HEADER(Previa, views::View)

 public:
  Previa() { SetPreferredSize(gfx::Size(kAmostra + 8, kAmostra + 8)); }

  void Definir(SkColor cor) {
    cor_ = cor;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(cor_);
    canvas->DrawCircle(gfx::PointF(width() / 2.0f, height() / 2.0f),
                       kAmostra / 2.0f, flags);
  }

 private:
  SkColor cor_ = SK_ColorWHITE;
};

BEGIN_METADATA(Previa)
END_METADATA

class Seletor : public views::View, public views::TextfieldController {
  METADATA_HEADER(Seletor, views::View)

 public:
  Seletor(const std::string& cor, Mudou mudou) : mudou_(std::move(mudou)) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 10));

    auto* grade = AddChildView(std::make_unique<views::View>());
    auto* tabela =
        grade->SetLayoutManager(std::make_unique<views::TableLayout>());
    for (int coluna = 0; coluna < kColunas; ++coluna) {
      tabela->AddColumn(views::LayoutAlignment::kCenter,
                        views::LayoutAlignment::kCenter,
                        views::TableLayout::kFixedSize,
                        views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
      if (coluna + 1 < kColunas) {
        tabela->AddPaddingColumn(views::TableLayout::kFixedSize, 4);
      }
    }
    const size_t linhas = (std::size(kPaleta) + kColunas - 1) / kColunas;
    for (size_t linha = 0; linha < linhas; ++linha) {
      if (linha > 0) {
        tabela->AddPaddingRow(views::TableLayout::kFixedSize, 4);
      }
      tabela->AddRows(1, views::TableLayout::kFixedSize);
    }
    for (const Predefinida& predefinida : kPaleta) {
      amostras_.push_back(grade->AddChildView(std::make_unique<Amostra>(
          predefinida.cor, predefinida.nome,
          base::BindRepeating(&Seletor::Escolher, base::Unretained(this),
                              std::string(predefinida.cor)))));
    }

    auto* rotulo = AddChildView(std::make_unique<views::Label>(
        u"Cor personalizada", views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY));
    rotulo->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    quadrado_ = AddChildView(std::make_unique<QuadradoSV>(base::BindRepeating(
        [](Seletor* seletor, float s, float v) {
          seletor->hsv_.s = s;
          seletor->hsv_.v = v;
          seletor->DoHsv();
        },
        base::Unretained(this))));
    matiz_ = AddChildView(std::make_unique<BarraDeMatiz>(base::BindRepeating(
        [](Seletor* seletor, float h) {
          seletor->hsv_.h = h;
          seletor->quadrado_->Definir(seletor->hsv_);
          seletor->DoHsv();
        },
        base::Unretained(this))));

    auto* linha = AddChildView(std::make_unique<views::View>());
    auto* layout = linha->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    previa_ = linha->AddChildView(std::make_unique<Previa>());
    codigo_ = linha->AddChildView(std::make_unique<views::Textfield>());
    codigo_->SetDefaultWidthInChars(8);
    codigo_->set_controller(this);
    codigo_->GetViewAccessibility().SetName(u"Código da cor");

    Mostrar(cor, /*mexer_no_codigo=*/true);
  }

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    std::string texto = base::ToUpperASCII(
        base::UTF16ToUTF8(base::TrimWhitespace(new_contents, base::TRIM_ALL)));
    if (!texto.empty() && texto[0] != '#') {
      texto = "#" + texto;
    }
    if (texto.size() != 7 || containers::Hex(containers::Cor(texto)) != texto) {
      return;
    }
    Mostrar(texto, /*mexer_no_codigo=*/false);
    mudou_.Run(cor_);
  }

 private:
  void Escolher(const std::string& cor) {
    Mostrar(cor, /*mexer_no_codigo=*/true);
    mudou_.Run(cor_);
  }

  // A cor livre mudou (quadrado ou barra).
  void DoHsv() {
    cor_ = containers::Hex(DeHsv(hsv_));
    Marcar();
    previa_->Definir(DeHsv(hsv_));
    codigo_->SetText(base::UTF8ToUTF16(cor_));
    mudou_.Run(cor_);
  }

  void Mostrar(const std::string& cor, bool mexer_no_codigo) {
    cor_ = cor;
    const SkColor real = containers::Cor(cor);
    hsv_ = ParaHsv(real);
    quadrado_->Definir(hsv_);
    matiz_->Definir(hsv_.h);
    previa_->Definir(real);
    if (mexer_no_codigo) {
      codigo_->SetText(base::UTF8ToUTF16(containers::Hex(real)));
    }
    Marcar();
  }

  void Marcar() {
    const std::string hex = containers::Hex(containers::Cor(cor_));
    for (Amostra* amostra : amostras_) {
      amostra->SetMarcada(amostra->cor() == cor_ ||
                          containers::Hex(containers::Cor(amostra->cor())) ==
                              hex);
    }
  }

  Mudou mudou_;
  std::string cor_;
  Hsv hsv_;
  std::vector<raw_ptr<Amostra>> amostras_;
  raw_ptr<QuadradoSV> quadrado_ = nullptr;
  raw_ptr<BarraDeMatiz> matiz_ = nullptr;
  raw_ptr<Previa> previa_ = nullptr;
  raw_ptr<views::Textfield> codigo_ = nullptr;
};

BEGIN_METADATA(Seletor)
END_METADATA

}  // namespace

std::unique_ptr<views::View> Criar(const std::string& cor, Mudou mudou) {
  return std::make_unique<Seletor>(cor, std::move(mudou));
}

}  // namespace navegador::seletor_de_cor
