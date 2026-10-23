#include "navegador/browser/ui/painel_da_memoria.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/containers/containers.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "navegador/browser/memoria/memoria.h"
#include "navegador/browser/memoria/texto.h"
#include "navegador/browser/ui/configuracoes.h"
#include "navegador/browser/ui/containers/containers_ui.h"
#include "navegador/browser/ui/containers/espaco.h"
#include "navegador/browser/ui/links.h"
#include "navegador/browser/ui/visual.h"
#include "navegador/icones/menu/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace navegador::memoria {

namespace {

// --- Medidas -----------------------------------------------------------------

constexpr int kRaio = 14;
// Folga do cartão até a borda da página.
constexpr int kMargem = 28;
constexpr int kLarguraMaxima = 980;
constexpr int kAlturaMaxima = 700;
constexpr int kCabecalho = 60;
constexpr int kFiltros = 44;
constexpr int kRodape = 36;
// O detalhe do resultado à direita, quando o cartão é largo.
constexpr int kLarguraDoDetalhe = 330;
constexpr int kComDetalhe = 860;
constexpr int kAlturaDaLinha = 106;
constexpr int kAlturaDaLinhaCompacta = 66;
constexpr int kAlturaDoGrupo = 34;
// Miniatura na lista: 16:10, como a guardada.
constexpr int kMiniaturaLargura = 104;
constexpr int kMiniaturaAltura = 65;
constexpr int kMiniaturaCompactaLargura = 72;
constexpr int kMiniaturaCompactaAltura = 45;
constexpr size_t kLimiteDaBusca = 60;
constexpr size_t kLimiteDaLinhaDoTempo = 150;
// O trecho que vem do banco; a lista mostra as duas primeiras linhas, o
// detalhe quase tudo.
constexpr size_t kTamanhoDoTrecho = 420;
// Espera entre uma tecla e a busca (quem digita rápido não busca a cada
// letra).
constexpr base::TimeDelta kEsperaDaBusca = base::Milliseconds(35);

enum class Periodo { kTudo = 0, kHoje = 1, kSemana = 2, kMes = 3 };
constexpr std::array<std::u16string_view, 4> kNomesDosPeriodos = {
    u"Tudo", u"Hoje", u"7 dias", u"30 dias"};

gfx::FontList Fonte(int tamanho,
                    gfx::Font::Weight peso = gfx::Font::Weight::NORMAL) {
  return gfx::FontList()
      .DeriveWithSizeDelta(tamanho - gfx::FontList().GetFontSize())
      .DeriveWithWeight(peso);
}

// Títulos na Bricolage Grotesque (a fonte de marca, que vai no pacote).
gfx::FontList FonteTitulo(int tamanho) {
  return gfx::FontList({"Bricolage Grotesque",
                        gfx::FontList().GetPrimaryFont().GetFontName()},
                       gfx::Font::NORMAL, tamanho,
                       gfx::Font::Weight::SEMIBOLD);
}

bool Escuro(const views::View* view) {
  const ui::ColorProvider* cores = view->GetColorProvider();
  return !cores ||
         color_utils::IsDark(cores->GetColor(ui::kColorSysSurface));
}

SkColor Cor(const views::View* view, ui::ColorId id) {
  return view->GetColorProvider()->GetColor(id);
}

// --- Datas, em português -------------------------------------------------------

constexpr std::array<std::u16string_view, 12> kMeses = {
    u"jan.", u"fev.", u"mar.", u"abr.", u"maio", u"jun.",
    u"jul.", u"ago.", u"set.", u"out.", u"nov.", u"dez."};
constexpr std::array<std::u16string_view, 12> kMesesInteiros = {
    u"janeiro", u"fevereiro", u"março",    u"abril",   u"maio",
    u"junho",   u"julho",     u"agosto",   u"setembro", u"outubro",
    u"novembro", u"dezembro"};
constexpr std::array<std::u16string_view, 7> kDias = {
    u"domingo", u"segunda", u"terça", u"quarta",
    u"quinta",  u"sexta",   u"sábado"};
constexpr std::array<std::u16string_view, 7> kDiasInteiros = {
    u"Domingo",      u"Segunda-feira", u"Terça-feira", u"Quarta-feira",
    u"Quinta-feira", u"Sexta-feira",   u"Sábado"};

// Dias de calendário entre as duas datas (0 = mesmo dia).
int DiasEntre(base::Time antes, base::Time depois) {
  return static_cast<int>(std::lround(
      (depois.LocalMidnight() - antes.LocalMidnight()).InSecondsF() / 86400.0));
}

std::u16string Hora(const base::Time::Exploded& e) {
  return base::UTF8ToUTF16(base::StringPrintf("%d:%02d", e.hour, e.minute));
}

// "agora", "há 5 min", "hoje, 14:32", "ontem, 9:05", "segunda, 14:32",
// "3 de out.", "3 de out. de 2025".
std::u16string Quando(base::Time quando) {
  const base::Time agora = base::Time::Now();
  const base::TimeDelta passou = agora - quando;
  if (passou < base::Minutes(1)) {
    return u"agora";
  }
  if (passou < base::Hours(1)) {
    return u"há " + base::NumberToString16(passou.InMinutes()) + u" min";
  }
  base::Time::Exploded e;
  quando.LocalExplode(&e);
  base::Time::Exploded hoje;
  agora.LocalExplode(&hoje);
  const int dias = DiasEntre(quando, agora);
  if (dias == 0) {
    return u"hoje, " + Hora(e);
  }
  if (dias == 1) {
    return u"ontem, " + Hora(e);
  }
  if (dias < 7) {
    return std::u16string(kDias[e.day_of_week]) + u", " + Hora(e);
  }
  std::u16string data = base::NumberToString16(e.day_of_month) + u" de " +
                        std::u16string(kMeses[e.month - 1]);
  if (e.year != hoje.year) {
    data += u" de " + base::NumberToString16(e.year);
  }
  return data;
}

// Título do grupo na linha do tempo: "Hoje", "Ontem", "Segunda-feira, 20 de
// outubro", "20 de outubro de 2025".
std::u16string Dia(base::Time quando) {
  const base::Time agora = base::Time::Now();
  const int dias = DiasEntre(quando, agora);
  if (dias == 0) {
    return u"Hoje";
  }
  if (dias == 1) {
    return u"Ontem";
  }
  base::Time::Exploded e;
  quando.LocalExplode(&e);
  base::Time::Exploded hoje;
  agora.LocalExplode(&hoje);
  std::u16string data = base::NumberToString16(e.day_of_month) + u" de " +
                        std::u16string(kMesesInteiros[e.month - 1]);
  if (e.year != hoje.year) {
    return data + u" de " + base::NumberToString16(e.year);
  }
  return std::u16string(kDiasInteiros[e.day_of_week]) + u", " + data;
}

// "lida por 6 min", "lida por 1 h 20 min"; vazio com menos de um minuto.
std::u16string TempoDeLeitura(int segundos) {
  const int minutos = segundos / 60;
  if (minutos < 1) {
    return std::u16string();
  }
  if (minutos < 60) {
    return u"lida por " + base::NumberToString16(minutos) + u" min";
  }
  std::u16string texto =
      u"lida por " + base::NumberToString16(minutos / 60) + u" h";
  if (minutos % 60) {
    texto += u" " + base::NumberToString16(minutos % 60) + u" min";
  }
  return texto;
}

// --- Desenho -----------------------------------------------------------------

std::unique_ptr<gfx::RenderText> Texto(std::u16string_view texto,
                                       const gfx::FontList& fonte,
                                       SkColor cor,
                                       const gfx::Rect& area,
                                       size_t linhas,
                                       int entrelinha,
                                       gfx::ElideBehavior corte) {
  std::unique_ptr<gfx::RenderText> render = gfx::RenderText::CreateRenderText();
  render->SetText(texto);
  render->SetFontList(fonte);
  render->SetColor(cor);
  render->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  render->SetCursorEnabled(false);
  if (linhas > 1) {
    render->SetMultiline(true);
    render->SetMaxLines(linhas);
    render->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
    render->SetMinLineHeight(entrelinha);
    // Do alto da área (no meio, um título de uma linha só descia em cima
    // do que vem embaixo dele).
    render->SetVerticalAlignment(gfx::ALIGN_TOP);
  }
  render->SetElideBehavior(corte);
  render->SetDisplayRect(area);
  return render;
}

// Uma linha de texto cortada com "…" (ou no meio, pra endereço).
void Escrever(gfx::Canvas* canvas,
              std::u16string_view texto,
              const gfx::FontList& fonte,
              SkColor cor,
              const gfx::Rect& area,
              gfx::ElideBehavior corte = gfx::ELIDE_TAIL) {
  Texto(texto, fonte, cor, area, 1, 0, corte)->Draw(canvas);
}

// O trecho com as palavras achadas marcadas (cor e peso do acento, com um
// fundo de marca-texto por baixo), em até `linhas` linhas. Devolve a altura
// usada.
int EscreverTrecho(gfx::Canvas* canvas,
                   const views::View* view,
                   const texto::Trecho& trecho,
                   const gfx::Rect& area,
                   const gfx::FontList& fonte,
                   size_t linhas,
                   int entrelinha) {
  if (trecho.texto.empty()) {
    return 0;
  }
  std::unique_ptr<gfx::RenderText> render =
      Texto(trecho.texto, fonte, Cor(view, ui::kColorSysOnSurfaceSecondary),
            area, linhas, entrelinha, gfx::ELIDE_TAIL);
  const SkColor destaque = Cor(view, ui::kColorSysOnSurfacePrimary);
  for (const auto& [inicio, fim] : trecho.destaques) {
    if (inicio < fim && fim <= trecho.texto.size()) {
      render->ApplyColor(destaque, gfx::Range(inicio, fim));
      render->ApplyWeight(gfx::Font::Weight::SEMIBOLD, gfx::Range(inicio, fim));
    }
  }
  // O que não coube foi trocado por "…": marca só o que aparece (senão o
  // fundo de marca-texto caía nas reticências).
  size_t visivel = trecho.texto.size();
  if (const std::u16string_view mostrado = render->GetDisplayText();
      mostrado.size() < trecho.texto.size()) {
    visivel = mostrado.empty() ? 0 : mostrado.size() - 1;
  }
  cc::PaintFlags marca;
  marca.setAntiAlias(true);
  marca.setColor(SkColorSetA(Cor(view, ui::kColorSysPrimary),
                             Escuro(view) ? 0x33 : 0x22));
  for (const auto& [inicio, fim] : trecho.destaques) {
    if (inicio >= fim || fim > visivel) {
      continue;
    }
    for (gfx::Rect pedaco : render->GetSubstringBounds(gfx::Range(inicio, fim))) {
      pedaco.Intersect(area);
      if (pedaco.IsEmpty()) {
        continue;
      }
      gfx::RectF caixa(pedaco);
      caixa.Inset(gfx::InsetsF::VH(1, -2));
      canvas->DrawRoundRect(caixa, 3, marca);
    }
  }
  render->Draw(canvas);
  return std::min(area.height(), render->GetStringSize().height());
}

// Imagem preenchendo o retângulo (corta o que sobra), com os cantos redondos
// e um contorno fino.
void DesenharMiniatura(gfx::Canvas* canvas,
                       const views::View* view,
                       const gfx::Rect& area,
                       const gfx::ImageSkia& miniatura,
                       const gfx::ImageSkia& icone,
                       int raio) {
  const gfx::RectF caixa(area);
  canvas->Save();
  canvas->ClipPath(SkPath::RRect(SkRRect::MakeRectXY(gfx::RectFToSkRect(caixa),
                                                     raio, raio)),
                   /*do_anti_alias=*/true);
  if (!miniatura.isNull()) {
    const float escala =
        std::max(static_cast<float>(area.width()) / miniatura.width(),
                 static_cast<float>(area.height()) / miniatura.height());
    const int largura = std::ceil(miniatura.width() * escala);
    const int altura = std::ceil(miniatura.height() * escala);
    // Alinhada no alto: o começo da página é o que a pessoa reconhece.
    canvas->DrawImageInt(miniatura, 0, 0, miniatura.width(), miniatura.height(),
                         area.x() + (area.width() - largura) / 2, area.y(),
                         largura, altura, /*filter=*/true);
  } else {
    cc::PaintFlags fundo;
    fundo.setColor(Cor(view, ui::kColorSysSurface4));
    canvas->DrawRect(caixa, fundo);
    const gfx::ImageSkia simbolo =
        !icone.isNull()
            ? icone
            : gfx::CreateVectorIcon(icones_menu::kGlobeIcon,
                                    std::min(20, area.height() / 2),
                                    Cor(view, ui::kColorSysOnSurfaceSubtle));
    canvas->DrawImageInt(simbolo,
                         area.x() + (area.width() - simbolo.width()) / 2,
                         area.y() + (area.height() - simbolo.height()) / 2);
  }
  canvas->Restore();
  cc::PaintFlags contorno;
  contorno.setAntiAlias(true);
  contorno.setStyle(cc::PaintFlags::kStroke_Style);
  contorno.setStrokeWidth(1);
  contorno.setColor(Escuro(view) ? SkColorSetA(SK_ColorWHITE, 0x14)
                                 : SkColorSetA(SK_ColorBLACK, 0x14));
  gfx::RectF borda = caixa;
  borda.Inset(0.5f);
  canvas->DrawRoundRect(borda, raio, contorno);
}

// Tecla desenhada ("esc", "↵"): o texto num retângulo de contorno.
int DesenharTecla(gfx::Canvas* canvas,
                  const views::View* view,
                  std::u16string_view texto,
                  int x,
                  int centro_y) {
  const gfx::FontList fonte = Fonte(11, gfx::Font::Weight::MEDIUM);
  const int largura =
      std::max(18, gfx::GetStringWidth(texto, fonte) + 10);
  const gfx::Rect caixa(x, centro_y - 9, largura, 18);
  cc::PaintFlags contorno;
  contorno.setAntiAlias(true);
  contorno.setStyle(cc::PaintFlags::kStroke_Style);
  contorno.setStrokeWidth(1);
  contorno.setColor(Cor(view, ui::kColorSysDivider));
  gfx::RectF borda(caixa);
  borda.Inset(0.5f);
  canvas->DrawRoundRect(borda, 5, contorno);
  canvas->DrawStringRectWithFlags(texto, fonte,
                                  Cor(view, ui::kColorSysOnSurfaceSubtle),
                                  caixa, gfx::Canvas::TEXT_ALIGN_CENTER);
  return largura;
}

// --- Peças --------------------------------------------------------------------

// O que as peças perguntam pro painel.
class Contexto {
 public:
  virtual const Resultado& ResultadoEm(size_t indice) const = 0;
  virtual bool Selecionada(size_t indice) const = 0;
  virtual bool EstaAberta(size_t indice) const = 0;
  // Pedem a imagem se ainda não têm (e repintam quando chega).
  virtual gfx::ImageSkia MiniaturaDe(int64_t id) = 0;
  virtual gfx::ImageSkia IconeDe(const std::string& url) = 0;
  virtual std::u16string NomeDoEspaco(const std::string& id) = 0;
  virtual std::u16string NomeDoContainer(const std::string& id) = 0;
  virtual void Passou(size_t indice) = 0;
  virtual void Apertou(size_t indice, int flags) = 0;

 protected:
  virtual ~Contexto() = default;
};

// Um resultado na lista: miniatura, título, site e quando, e o trecho com as
// palavras achadas (na linha do tempo, sem o trecho: o começo do texto da
// página costuma ser o menu do site).
class Linha : public views::Button {
  METADATA_HEADER(Linha, views::Button)

 public:
  Linha(Contexto* contexto, size_t indice, bool compacta)
      : contexto_(contexto), indice_(indice), compacta_(compacta) {
    SetCallback(base::BindRepeating(
        [](Linha* linha, const ui::Event& evento) {
          linha->contexto_->Apertou(linha->indice_, evento.flags());
        },
        base::Unretained(this)));
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    // O foco fica no campo de busca (as setas andam na lista por ele).
    SetFocusBehavior(FocusBehavior::NEVER);
    const Resultado& r = contexto_->ResultadoEm(indice_);
    GetViewAccessibility().SetName(r.titulo.empty()
                                       ? base::UTF8ToUTF16(r.host)
                                       : r.titulo + u", " +
                                             base::UTF8ToUTF16(r.host));
    SetTooltipText(base::UTF8ToUTF16(r.url));
  }

  size_t indice() const { return indice_; }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& disponivel) const override {
    return gfx::Size(0, compacta_ ? kAlturaDaLinhaCompacta : kAlturaDaLinha);
  }

  void OnMouseMoved(const ui::MouseEvent& evento) override {
    views::Button::OnMouseMoved(evento);
    // Só o ponteiro andando de verdade escolhe (a lista rolando por baixo
    // dele, não).
    if (evento.location() != ultimo_ponto_) {
      ultimo_ponto_ = evento.location();
      contexto_->Passou(indice_);
    }
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const Resultado& r = contexto_->ResultadoEm(indice_);
    const bool escuro = Escuro(this);
    gfx::Rect caixa = GetLocalBounds();
    caixa.Inset(gfx::Insets::VH(2, 8));

    if (contexto_->Selecionada(indice_) || GetState() == STATE_HOVERED) {
      cc::PaintFlags fundo;
      fundo.setAntiAlias(true);
      const bool selecionada = contexto_->Selecionada(indice_);
      fundo.setColor(escuro ? SkColorSetA(SK_ColorWHITE,
                                          selecionada ? 0x12 : 0x08)
                            : SkColorSetA(SK_ColorBLACK,
                                          selecionada ? 0x0B : 0x05));
      canvas->DrawRoundRect(gfx::RectF(caixa), 10, fundo);
      if (selecionada) {
        fundo.setColor(Cor(this, ui::kColorSysPrimary));
        canvas->DrawRoundRect(
            gfx::RectF(caixa.x(), caixa.CenterPoint().y() - 12, 3, 24), 1.5f,
            fundo);
      }
    }

    const int largura_miniatura =
        compacta_ ? kMiniaturaCompactaLargura : kMiniaturaLargura;
    const int altura_miniatura =
        compacta_ ? kMiniaturaCompactaAltura : kMiniaturaAltura;
    const gfx::Rect miniatura(caixa.x() + 12,
                              caixa.y() + (caixa.height() - altura_miniatura) / 2,
                              largura_miniatura, altura_miniatura);
    const gfx::ImageSkia icone = contexto_->IconeDe(r.url);
    DesenharMiniatura(canvas, this, miniatura,
                      r.tem_miniatura ? contexto_->MiniaturaDe(r.id)
                                      : gfx::ImageSkia(),
                      icone, 7);

    const int x = miniatura.right() + 14;
    int largura = caixa.right() - 14 - x;
    int y = caixa.y() + (compacta_ ? 12 : 11);

    // Já aberta numa aba: a pílula no canto.
    if (contexto_->EstaAberta(indice_)) {
      const gfx::FontList fonte = Fonte(11, gfx::Font::Weight::MEDIUM);
      const std::u16string aberta = u"Aberta";
      const int pilula = gfx::GetStringWidth(aberta, fonte) + 14;
      const gfx::Rect lugar(caixa.right() - 12 - pilula, y, pilula, 18);
      cc::PaintFlags fundo;
      fundo.setAntiAlias(true);
      fundo.setColor(Cor(this, ui::kColorSysTonalContainer));
      canvas->DrawRoundRect(gfx::RectF(lugar), 9, fundo);
      canvas->DrawStringRectWithFlags(
          aberta, fonte, Cor(this, ui::kColorSysOnTonalContainer), lugar,
          gfx::Canvas::TEXT_ALIGN_CENTER);
      largura -= pilula + 8;
    }

    const std::u16string titulo =
        r.titulo.empty() ? base::UTF8ToUTF16(r.host) : r.titulo;
    Escrever(canvas, titulo, Fonte(14, gfx::Font::Weight::SEMIBOLD),
             Cor(this, ui::kColorSysOnSurface), gfx::Rect(x, y, largura, 19));
    y += 21;

    // Ícone do site, site, quando e visitas.
    if (!icone.isNull()) {
      canvas->DrawImageInt(icone, 0, 0, icone.width(), icone.height(), x,
                           y + 2, 13, 13, /*filter=*/true);
    }
    const int depois_do_icone = icone.isNull() ? 0 : 19;
    std::u16string meta =
        base::UTF8ToUTF16(r.host) + u"  ·  " + Quando(r.ultima);
    if (r.visitas > 1) {
      meta += u"  ·  " + base::NumberToString16(r.visitas) + u" visitas";
    }
    Escrever(canvas, meta, Fonte(12), Cor(this, ui::kColorSysOnSurfaceSubtle),
             gfx::Rect(x + depois_do_icone, y, largura - depois_do_icone, 17));
    y += 22;

    if (!compacta_) {
      // Na lista cabem duas linhas: o trecho começa perto da primeira
      // palavra achada.
      EscreverTrecho(canvas, this, texto::Encurtar(r.trecho, 28),
                     gfx::Rect(x, y, caixa.right() - 14 - x,
                               caixa.bottom() - 6 - y),
                     Fonte(13), 2, 18);
    }
  }

 private:
  raw_ptr<Contexto> contexto_;
  const size_t indice_;
  const bool compacta_;
  gfx::Point ultimo_ponto_;
};

BEGIN_METADATA(Linha)
END_METADATA

// Título de um dia na linha do tempo.
class Grupo : public views::View {
  METADATA_HEADER(Grupo, views::View)

 public:
  explicit Grupo(std::u16string titulo) : titulo_(std::move(titulo)) {
    SetCanProcessEventsWithinSubtree(false);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& disponivel) const override {
    return gfx::Size(0, kAlturaDoGrupo);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    Escrever(canvas, titulo_, Fonte(12, gfx::Font::Weight::SEMIBOLD),
             Cor(this, ui::kColorSysOnSurfaceSubtle),
             gfx::Rect(22, height() - 22, width() - 44, 18));
  }

 private:
  const std::u16string titulo_;
};

BEGIN_METADATA(Grupo)
END_METADATA

// Quando não há o que mostrar: símbolo, título e uma explicação (e às vezes
// um botão).
class Vazio : public views::View {
  METADATA_HEADER(Vazio, views::View)

 public:
  Vazio(const gfx::VectorIcon& icone,
        std::u16string titulo,
        std::u16string texto,
        std::u16string botao = std::u16string(),
        views::Button::PressedCallback apertou = {})
      : icone_(icone), titulo_(std::move(titulo)), texto_(std::move(texto)) {
    if (!botao.empty()) {
      botao_ = AddChildView(
          std::make_unique<views::MdTextButton>(std::move(apertou), botao));
      botao_->SetStyle(ui::ButtonStyle::kTonal);
    }
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& disponivel) const override {
    return gfx::Size(0, 300);
  }

  void Layout(PassKey) override {
    if (botao_) {
      const gfx::Size tamanho = botao_->GetPreferredSize();
      botao_->SetBounds((width() - tamanho.width()) / 2, AlturaDoTexto() + 18,
                        tamanho.width(), tamanho.height());
    }
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const int centro = width() / 2;
    // O símbolo num círculo suave.
    cc::PaintFlags circulo;
    circulo.setAntiAlias(true);
    circulo.setColor(Cor(this, ui::kColorSysSurface4));
    canvas->DrawCircle(gfx::PointF(centro, 72), 30, circulo);
    const gfx::ImageSkia simbolo = gfx::CreateVectorIcon(
        *icone_, 26, Cor(this, ui::kColorSysOnSurfaceSecondary));
    canvas->DrawImageInt(simbolo, centro - 13, 72 - 13);
    canvas->DrawStringRectWithFlags(
        titulo_, FonteTitulo(18), Cor(this, ui::kColorSysOnSurface),
        gfx::Rect(24, 118, width() - 48, 26), gfx::Canvas::TEXT_ALIGN_CENTER);
    const int largura = std::min(width() - 64, 440);
    std::unique_ptr<gfx::RenderText> texto =
        Texto(texto_, Fonte(13), Cor(this, ui::kColorSysOnSurfaceSecondary),
              gfx::Rect(centro - largura / 2, 152, largura, 80), 4, 19,
              gfx::ELIDE_TAIL);
    texto->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    texto->Draw(canvas);
  }

 private:
  int AlturaDoTexto() const { return 152 + 4 * 19; }

  const raw_ref<const gfx::VectorIcon> icone_;
  const std::u16string titulo_;
  const std::u16string texto_;
  raw_ptr<views::MdTextButton> botao_ = nullptr;
};

BEGIN_METADATA(Vazio)
END_METADATA

// Pílula de filtro (período, "Só este espaço").
class Ficha : public views::Button {
  METADATA_HEADER(Ficha, views::Button)

 public:
  Ficha(std::u16string texto, PressedCallback apertou, SkColor bolinha = 0)
      : views::Button(std::move(apertou)),
        texto_(std::move(texto)),
        bolinha_(bolinha) {
    SetFocusBehavior(FocusBehavior::NEVER);
    GetViewAccessibility().SetName(texto_);
  }

  void SetEscolhida(bool escolhida) {
    if (escolhida_ != escolhida) {
      escolhida_ = escolhida;
      SchedulePaint();
    }
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& disponivel) const override {
    return gfx::Size(gfx::GetStringWidth(texto_, Fonte(12, gfx::Font::Weight::MEDIUM)) +
                         24 + (bolinha_ ? 14 : 0),
                     26);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::RectF caixa(GetLocalBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (escolhida_) {
      flags.setColor(Cor(this, ui::kColorSysTonalContainer));
      canvas->DrawRoundRect(caixa, 13, flags);
    } else {
      if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
        flags.setColor(Cor(this, ui::kColorSysStateHover));
        canvas->DrawRoundRect(caixa, 13, flags);
      }
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(1);
      flags.setColor(Cor(this, ui::kColorSysDivider));
      gfx::RectF borda = caixa;
      borda.Inset(0.5f);
      canvas->DrawRoundRect(borda, 12.5f, flags);
    }
    int x = 12;
    if (bolinha_) {
      cc::PaintFlags cor;
      cor.setAntiAlias(true);
      cor.setColor(bolinha_);
      canvas->DrawCircle(gfx::PointF(x + 4, height() / 2.0f), 4, cor);
      x += 14;
    }
    canvas->DrawStringRectWithFlags(
        texto_, Fonte(12, gfx::Font::Weight::MEDIUM),
        Cor(this, escolhida_ ? ui::kColorSysOnTonalContainer
                             : ui::kColorSysOnSurfaceSecondary),
        gfx::Rect(x, 0, width() - x - 12, height()), 0);
  }

 private:
  const std::u16string texto_;
  const SkColor bolinha_;
  bool escolhida_ = false;
};

BEGIN_METADATA(Ficha)
END_METADATA

// O resultado escolhido, em detalhe, à direita: miniatura grande, título
// inteiro, endereço, quando e quanto foi lida, o trecho maior e as ações.
class Detalhe : public views::View {
  METADATA_HEADER(Detalhe, views::View)

 public:
  Detalhe(Contexto* contexto,
          views::Button::PressedCallback abrir,
          views::Button::PressedCallback aqui,
          views::Button::PressedCallback esquecer)
      : contexto_(contexto) {
    abrir_ = AddChildView(
        std::make_unique<views::MdTextButton>(std::move(abrir), u"Abrir no trecho"));
    abrir_->SetStyle(ui::ButtonStyle::kProminent);
    abrir_->SetFocusBehavior(FocusBehavior::NEVER);
    aqui_ = AddChildView(
        std::make_unique<views::MdTextButton>(std::move(aqui), u"Nesta aba"));
    aqui_->SetStyle(ui::ButtonStyle::kTonal);
    aqui_->SetFocusBehavior(FocusBehavior::NEVER);
    // Só o ícone (a lixeira): os três com texto não cabem lado a lado.
    esquecer_ = AddChildView(
        std::make_unique<views::MdTextButton>(std::move(esquecer)));
    esquecer_->SetStyle(ui::ButtonStyle::kTonal);
    esquecer_->SetFocusBehavior(FocusBehavior::NEVER);
    esquecer_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(icones_menu::kTrashIcon,
                                       ui::kColorSysOnSurface, 16));
    esquecer_->SetTooltipText(u"Esquecer esta página (Shift+Delete)");
    esquecer_->GetViewAccessibility().SetName(u"Esquecer esta página");
    Mostrar(std::nullopt);
  }

  void Mostrar(std::optional<size_t> indice) {
    indice_ = indice;
    abrir_->SetVisible(indice.has_value());
    aqui_->SetVisible(indice.has_value());
    esquecer_->SetVisible(indice.has_value());
    SchedulePaint();
  }

  void Layout(PassKey) override {
    const int y = height() - 18 - 32;
    int x = 18;
    for (views::MdTextButton* botao : {abrir_.get(), aqui_.get()}) {
      const gfx::Size tamanho = botao->GetPreferredSize();
      botao->SetBounds(x, y, tamanho.width(), 32);
      x += tamanho.width() + 8;
    }
    esquecer_->SetBounds(width() - 18 - 36, y, 36, 32);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // Separado da lista por um fio.
    cc::PaintFlags fio;
    fio.setColor(Cor(this, ui::kColorSysDivider));
    canvas->DrawRect(gfx::RectF(0, 0, 1, height()), fio);
    if (!indice_) {
      return;
    }
    const Resultado& r = contexto_->ResultadoEm(*indice_);
    const int largura = width() - 36;
    int y = 18;
    const int altura_miniatura = largura * 10 / 16;
    const gfx::ImageSkia icone = contexto_->IconeDe(r.url);
    DesenharMiniatura(canvas, this, gfx::Rect(18, y, largura, altura_miniatura),
                      r.tem_miniatura ? contexto_->MiniaturaDe(r.id)
                                      : gfx::ImageSkia(),
                      icone, 10);
    y += altura_miniatura + 14;

    std::unique_ptr<gfx::RenderText> titulo = Texto(
        r.titulo.empty() ? base::UTF8ToUTF16(r.host) : r.titulo,
        Fonte(15, gfx::Font::Weight::SEMIBOLD),
        Cor(this, ui::kColorSysOnSurface), gfx::Rect(18, y, largura, 60), 3,
        20, gfx::ELIDE_TAIL);
    titulo->Draw(canvas);
    y += static_cast<int>(std::min<size_t>(3, titulo->GetNumLines())) * 20 + 4;
    Escrever(canvas, base::UTF8ToUTF16(r.url), Fonte(12),
             Cor(this, ui::kColorSysOnSurfaceSubtle),
             gfx::Rect(18, y, largura, 17), gfx::ELIDE_MIDDLE);
    y += 24;

    // Quando e quanto.
    std::vector<std::u16string> linhas;
    linhas.push_back(u"Vista por último " + Quando(r.ultima));
    std::u16string segunda;
    if (DiasEntre(r.primeira, r.ultima) > 0) {
      segunda = u"Primeira vez " + Quando(r.primeira);
    }
    if (r.visitas > 1) {
      segunda += (segunda.empty() ? u"" : u"  ·  ") +
                 base::NumberToString16(r.visitas) + u" visitas";
    }
    if (std::u16string lida = TempoDeLeitura(r.segundos); !lida.empty()) {
      segunda += (segunda.empty() ? u"" : u"  ·  ") + lida;
    }
    if (!segunda.empty()) {
      linhas.push_back(std::move(segunda));
    }
    std::u16string onde;
    if (std::u16string espaco = contexto_->NomeDoEspaco(r.espaco);
        !espaco.empty()) {
      onde = u"Espaço " + espaco;
    }
    if (std::u16string container = contexto_->NomeDoContainer(r.container);
        !container.empty()) {
      onde += (onde.empty() ? u"Container " : u"  ·  container ") + container;
    }
    if (!onde.empty()) {
      linhas.push_back(std::move(onde));
    }
    for (const std::u16string& linha : linhas) {
      Escrever(canvas, linha, Fonte(12),
               Cor(this, ui::kColorSysOnSurfaceSecondary),
               gfx::Rect(18, y, largura, 17));
      y += 19;
    }
    y += 10;

    // O trecho maior, até os botões.
    const int fim = height() - 18 - 32 - 14;
    if (fim - y >= 19) {
      EscreverTrecho(canvas, this, r.trecho, gfx::Rect(18, y, largura, fim - y),
                     Fonte(13), static_cast<size_t>((fim - y) / 19), 19);
    }
  }

 private:
  raw_ptr<Contexto> contexto_;
  std::optional<size_t> indice_;
  raw_ptr<views::MdTextButton> abrir_ = nullptr;
  raw_ptr<views::MdTextButton> aqui_ = nullptr;
  raw_ptr<views::MdTextButton> esquecer_ = nullptr;
};

BEGIN_METADATA(Detalhe)
END_METADATA

// O campo de busca: o Tab troca o período (vai pro HandleKeyEvent do
// painel), em vez de levar o foco pra fora, como o FocusManager faria.
class CampoDeBusca : public views::Textfield {
  METADATA_HEADER(CampoDeBusca, views::Textfield)

 public:
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& tecla) override {
    return (tecla.key_code() == ui::VKEY_TAB && !tecla.IsControlDown()) ||
           views::Textfield::SkipDefaultKeyEventProcessing(tecla);
  }
};

BEGIN_METADATA(CampoDeBusca)
END_METADATA

// A tecla e o que ela faz, no rodapé.
struct Dica {
  std::u16string_view tecla;
  std::u16string_view faz;
};
constexpr Dica kDicas[] = {
    {u"↑↓", u"escolher"},     {u"↵", u"abrir no trecho"},
    {u"⇧↵", u"nesta aba"},    {u"Ctrl ↵", u"em segundo plano"},
    {u"⇧Del", u"esquecer"},   {u"Tab", u"período"},
};

class Rodape : public views::View {
  METADATA_HEADER(Rodape, views::View)

 public:
  Rodape() { SetCanProcessEventsWithinSubtree(false); }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags fio;
    fio.setColor(Cor(this, ui::kColorSysDivider));
    canvas->DrawRect(gfx::RectF(0, 0, width(), 1), fio);
    const int centro = height() / 2;
    // À direita: onde fica guardado.
    const gfx::FontList fonte = Fonte(12);
    const std::u16string local = u"Só neste computador · sem IA";
    const int largura_local = gfx::GetStringWidth(local, fonte);
    const int x_local = width() - 18 - largura_local;
    const gfx::ImageSkia cadeado = gfx::CreateVectorIcon(
        icones_menu::kLockIcon, 13, Cor(this, ui::kColorSysOnSurfaceSubtle));
    canvas->DrawImageInt(cadeado, x_local - 19, centro - 7);
    canvas->DrawStringRectWithFlags(
        local, fonte, Cor(this, ui::kColorSysOnSurfaceSubtle),
        gfx::Rect(x_local, 0, largura_local, height()), 0);
    // À esquerda, as teclas (as que cabem antes do texto da direita).
    int x = 18;
    for (const Dica& dica : kDicas) {
      const int largura_faz = gfx::GetStringWidth(dica.faz, fonte);
      if (x + 40 + largura_faz > x_local - 40) {
        break;
      }
      x += DesenharTecla(canvas, this, dica.tecla, x, centro) + 6;
      canvas->DrawStringRectWithFlags(
          dica.faz, fonte, Cor(this, ui::kColorSysOnSurfaceSubtle),
          gfx::Rect(x, 0, largura_faz, height()), 0);
      x += largura_faz + 16;
    }
  }
};

BEGIN_METADATA(Rodape)
END_METADATA

// --- O painel -------------------------------------------------------------------

class Painel;

std::map<BrowserWindowInterface*, raw_ptr<Painel>>& Paineis() {
  static base::NoDestructor<std::map<BrowserWindowInterface*, raw_ptr<Painel>>>
      paineis;
  return *paineis;
}

// Comandos do menu do botão direito num resultado.
enum Comando {
  kAbrirNoTrecho = 1,
  kAbrirAqui,
  kAbrirAoFundo,
  kCopiarEndereco,
  kEsquecerPagina,
  kNaoLembrarDoSite,
};

class Painel : public views::View,
               public Contexto,
               public views::TextfieldController,
               public views::ContextMenuController,
               public ui::SimpleMenuModel::Delegate,
               public TabStripModelObserver,
               public views::WidgetObserver {
  METADATA_HEADER(Painel, views::View)

 public:
  Painel(BrowserWindowInterface* browser, BrowserView* janela)
      : browser_(browser),
        perfil_(browser->GetProfile()),
        anonima_(browser->GetProfile()->IsOffTheRecord()),
        espaco_(containers::EspacoFocado(browser).value_or(std::string())) {
    SetID(visual::kIdMemoria);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBackgroundBlur(8);

    // Cabeçalho: lupa, campo e o estado da busca.
    lupa_ = AddChildView(std::make_unique<views::View>());
    lupa_->SetCanProcessEventsWithinSubtree(false);
    campo_ = AddChildView(std::make_unique<CampoDeBusca>());
    campo_->SetController(this);
    campo_->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    campo_->SetBackgroundEnabled(false);
    campo_->SetFontList(Fonte(18));
    campo_->SetPlaceholderText(anonima_
                                   ? u"A Memória não funciona na janela anônima"
                                   : u"O que você lembra de ter lido?");
    campo_->SetTextColorId(ui::kColorSysOnSurface);
    campo_->SetPlaceholderTextColorId(ui::kColorSysOnSurfaceSubtle);
    campo_->GetViewAccessibility().SetName(u"Buscar na Memória");
    campo_->SetReadOnly(anonima_);

    // Filtros: o período e "Só este espaço".
    for (size_t i = 0; i < kNomesDosPeriodos.size(); ++i) {
      periodos_.push_back(AddChildView(std::make_unique<Ficha>(
          std::u16string(kNomesDosPeriodos[i]),
          base::BindRepeating(&Painel::EscolherPeriodo, base::Unretained(this),
                              static_cast<Periodo>(i)))));
    }
    periodos_[0]->SetEscolhida(true);
    if (!anonima_ && !espaco_.empty()) {
      std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, espaco_);
      if (espaco) {
        so_este_ = AddChildView(std::make_unique<Ficha>(
            u"Só em " + base::UTF8ToUTF16(espaco->nome),
            base::BindRepeating(&Painel::AlternarEspaco,
                                base::Unretained(this)),
            containers::Cor(espaco->cor)));
      }
    }
    configurar_ = AddChildView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(&Painel::AbrirConfiguracoes,
                            base::Unretained(this)),
        u"Configurar"));
    configurar_->SetStyle(ui::ButtonStyle::kText);
    configurar_->SetFocusBehavior(FocusBehavior::NEVER);
    configurar_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(icones_menu::kSettings2Icon,
                                       ui::kColorSysOnSurfaceSecondary, 14));

    rolagem_ = AddChildView(std::make_unique<views::ScrollView>());
    rolagem_->SetBackgroundColor(std::nullopt);
    rolagem_->SetDrawOverflowIndicator(false);
    lista_ = rolagem_->SetContents(std::make_unique<views::View>());
    lista_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(6, 0)));

    detalhe_ = AddChildView(std::make_unique<Detalhe>(
        this,
        base::BindRepeating(&Painel::AbrirEscolhida, base::Unretained(this),
                            WindowOpenDisposition::NEW_FOREGROUND_TAB),
        base::BindRepeating(&Painel::AbrirEscolhida, base::Unretained(this),
                            WindowOpenDisposition::CURRENT_TAB),
        base::BindRepeating(&Painel::EsquecerEscolhida,
                            base::Unretained(this))));
    rodape_ = AddChildView(std::make_unique<Rodape>());

    browser_->GetTabStripModel()->AddObserver(this);
    janela_widget_.Observe(janela->GetWidget());
    Paineis()[browser_] = this;
  }

  ~Painel() override {
    if (auto it = Paineis().find(browser_);
        it != Paineis().end() && it->second == this) {
      Paineis().erase(it);
    }
    if (!janela_fechando_) {
      browser_->GetTabStripModel()->RemoveObserver(this);
    }
  }

  // Abre (primeira vez) ou volta pro campo, com `consulta`.
  void Focar(const std::u16string& consulta) {
    if (!consulta.empty() || primeira_vez_) {
      campo_->SetText(consulta);
    }
    campo_->SelectAll(/*reversed=*/false);
    campo_->RequestFocus();
    if (primeira_vez_) {
      primeira_vez_ = false;
      Aparecer();
      if (!anonima_) {
        Resumir(perfil_, base::BindOnce(&Painel::Resumiu,
                                        weak_factory_.GetWeakPtr()));
      }
      Buscar();
    }
  }

  bool fechando() const { return fechando_; }

  // Some esmaecendo e sai da janela (fora da pilha de quem pediu).
  void Fechar() {
    if (fechando_) {
      return;
    }
    fechando_ = true;
    SetCanProcessEventsWithinSubtree(false);
    // O foco volta pra página antes (senão fica em lugar nenhum).
    if (!janela_fechando_) {
      if (content::WebContents* ativa =
              browser_->GetTabStripModel()->GetActiveWebContents()) {
        ativa->Focus();
      }
    }
    const base::TimeDelta tempo =
        gfx::Animation::RichAnimationDuration(base::Milliseconds(110));
    if (tempo.is_zero() || janela_fechando_) {
      Tirar();
      return;
    }
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&Painel::Tirar, weak_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(&Painel::Tirar, weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(tempo)
        .SetOpacity(layer(), 0.0f, gfx::Tween::EASE_IN);
  }

  // --- Contexto ---------------------------------------------------------------

  const Resultado& ResultadoEm(size_t indice) const override {
    return resultados_[indice];
  }

  bool Selecionada(size_t indice) const override {
    return selecionada_ == indice;
  }

  bool EstaAberta(size_t indice) const override {
    return abertas_.contains(resultados_[indice].url);
  }

  gfx::ImageSkia MiniaturaDe(int64_t id) override {
    if (auto it = miniaturas_.find(id); it != miniaturas_.end()) {
      return it->second;
    }
    if (pedidas_.insert(id).second) {
      Miniatura(perfil_, id,
                base::BindOnce(&Painel::ChegouMiniatura,
                               weak_factory_.GetWeakPtr(), id));
    }
    return gfx::ImageSkia();
  }

  gfx::ImageSkia IconeDe(const std::string& url) override {
    if (auto it = icones_.find(url); it != icones_.end()) {
      return it->second;
    }
    favicon::FaviconService* servico = FaviconServiceFactory::GetForProfile(
        perfil_, ServiceAccessType::EXPLICIT_ACCESS);
    if (servico && icones_pedidos_.insert(url).second) {
      servico->GetFaviconImageForPageURL(
          GURL(url),
          base::BindOnce(&Painel::ChegouIcone, weak_factory_.GetWeakPtr(),
                         url),
          &icones_tracker_);
    }
    return gfx::ImageSkia();
  }

  std::u16string NomeDoEspaco(const std::string& id) override {
    if (id.empty()) {
      return std::u16string();
    }
    std::optional<espacos::Espaco> espaco = espacos::Get(perfil_, id);
    return espaco ? base::UTF8ToUTF16(espaco->nome) : std::u16string();
  }

  std::u16string NomeDoContainer(const std::string& id) override {
    if (id.empty()) {
      return std::u16string();
    }
    std::optional<containers::Container> container =
        containers::Get(perfil_, id);
    return container ? base::UTF8ToUTF16(container->name) : std::u16string();
  }

  void Passou(size_t indice) override { Selecionar(indice, /*rolar=*/false); }

  void Apertou(size_t indice, int flags) override {
    Selecionar(indice, /*rolar=*/false);
    if (flags & ui::EF_MIDDLE_MOUSE_BUTTON ||
        flags & ui::EF_CONTROL_DOWN) {
      Abrir(indice, WindowOpenDisposition::NEW_BACKGROUND_TAB);
    } else if (flags & ui::EF_SHIFT_DOWN) {
      Abrir(indice, WindowOpenDisposition::CURRENT_TAB);
    } else {
      Abrir(indice, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    }
  }

  // --- views::View --------------------------------------------------------------

  // O cartão: no alto da página, centrado, com folga dos lados.
  gfx::Rect Cartao() const {
    const gfx::Rect area = GetLocalBounds();
    const int largura =
        std::min(kLarguraMaxima, std::max(320, area.width() - 2 * kMargem));
    const int cima = std::clamp(area.height() * 7 / 100, 16, 72);
    int altura =
        std::min(kAlturaMaxima, std::max(260, area.height() - cima - 24));
    // Na janela anônima é só o aviso.
    if (anonima_) {
      altura = std::min(altura, kCabecalho + 330);
    }
    return gfx::Rect(area.x() + (area.width() - largura) / 2, area.y() + cima,
                     largura, altura);
  }

  void Layout(PassKey) override {
    const gfx::Rect cartao = Cartao();
    int y = cartao.y();
    lupa_->SetBounds(cartao.x() + 22, y + (kCabecalho - 20) / 2, 20, 20);
    // O estado ("12 páginas") e o "esc" ficam pintados à direita.
    campo_->SetBounds(cartao.x() + 54, y + 10,
                      cartao.width() - 54 - 150, kCabecalho - 20);
    y += kCabecalho;

    if (anonima_) {
      // Sem filtros, detalhe nem rodapé: só o aviso embaixo do campo.
      for (views::View* fora :
           std::initializer_list<views::View*>{configurar_, detalhe_, rodape_}) {
        fora->SetVisible(false);
      }
      for (Ficha* ficha : periodos_) {
        ficha->SetVisible(false);
      }
      rolagem_->ClipHeightTo(0, cartao.bottom() - y - 1);
      rolagem_->SetBounds(cartao.x(), y + 1, cartao.width(),
                          cartao.bottom() - y - 1);
      return;
    }
    int x = cartao.x() + 18;
    const int meio = y + (kFiltros - 26) / 2;
    for (Ficha* ficha : periodos_) {
      const int largura = ficha->GetPreferredSize().width();
      ficha->SetBounds(x, meio, largura, 26);
      x += largura + 6;
    }
    if (so_este_) {
      x += 10;
      const int largura = so_este_->GetPreferredSize().width();
      so_este_->SetBounds(x, meio, largura, 26);
    }
    const gfx::Size configurar = configurar_->GetPreferredSize();
    configurar_->SetBounds(cartao.right() - 12 - configurar.width(),
                           y + (kFiltros - configurar.height()) / 2,
                           configurar.width(), configurar.height());
    y += kFiltros;

    const int corpo = cartao.bottom() - kRodape - y;
    // Sem resultados, o aviso ocupa o cartão inteiro.
    const bool com_detalhe =
        cartao.width() >= kComDetalhe && !resultados_.empty();
    const int largura_lista =
        cartao.width() - (com_detalhe ? kLarguraDoDetalhe : 0);
    // Sem o limite de altura a ScrollView não dimensiona a lista.
    rolagem_->ClipHeightTo(0, corpo - 1);
    rolagem_->SetBounds(cartao.x(), y + 1, largura_lista, corpo - 1);
    detalhe_->SetVisible(com_detalhe);
    detalhe_->SetBounds(cartao.x() + largura_lista, y + 1, kLarguraDoDetalhe,
                        corpo - 1);
    rodape_->SetBounds(cartao.x(), cartao.bottom() - kRodape, cartao.width(),
                       kRodape);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    // A página atrás, escurecida (e desfocada pela camada).
    canvas->DrawColor(SkColorSetARGB(0x66, 0x00, 0x00, 0x00));

    const gfx::RectF cartao(Cartao());
    cc::PaintFlags sombra;
    sombra.setAntiAlias(true);
    sombra.setColor(SK_ColorTRANSPARENT);
    sombra.setLooper(gfx::CreateShadowDrawLooper(
        {gfx::ShadowValue(gfx::Vector2d(0, 16), 56,
                          SkColorSetA(SK_ColorBLACK, 0x70)),
         gfx::ShadowValue(gfx::Vector2d(0, 2), 10,
                          SkColorSetA(SK_ColorBLACK, 0x40))}));
    canvas->DrawRoundRect(cartao, kRaio, sombra);
    cc::PaintFlags fundo;
    fundo.setAntiAlias(true);
    fundo.setColor(Cor(this, ui::kColorSysSurface));
    canvas->DrawRoundRect(cartao, kRaio, fundo);
    cc::PaintFlags borda;
    borda.setAntiAlias(true);
    borda.setStyle(cc::PaintFlags::kStroke_Style);
    borda.setStrokeWidth(1);
    borda.setColor(Cor(this, ui::kColorSysDivider));
    gfx::RectF contorno = cartao;
    contorno.Inset(0.5f);
    canvas->DrawRoundRect(contorno, kRaio, borda);

    // A lupa e, à direita do campo, o estado e o "esc".
    const gfx::Rect cabecalho(cartao.x(), cartao.y(), cartao.width(),
                              kCabecalho);
    canvas->DrawImageInt(
        gfx::CreateVectorIcon(icones_menu::kTextSearchIcon, 20,
                              Cor(this, ui::kColorSysOnSurfaceSecondary)),
        lupa_->x(), lupa_->y());
    const int x_esc = cabecalho.right() - 18 - 30;
    DesenharTecla(canvas, this, u"esc", x_esc, cabecalho.CenterPoint().y());
    if (!estado_.empty()) {
      const gfx::FontList fonte = Fonte(12);
      const int largura = gfx::GetStringWidth(estado_, fonte);
      canvas->DrawStringRectWithFlags(
          estado_, fonte, Cor(this, ui::kColorSysOnSurfaceSubtle),
          gfx::Rect(x_esc - 12 - largura, cabecalho.y(), largura,
                    cabecalho.height()),
          0);
    }
    // Fios entre o cabeçalho, os filtros e a lista.
    cc::PaintFlags fio;
    fio.setColor(Cor(this, ui::kColorSysDivider));
    canvas->DrawRect(gfx::RectF(cartao.x(), cabecalho.bottom(), cartao.width(),
                                1),
                     fio);
    if (!anonima_) {
      canvas->DrawRect(gfx::RectF(cartao.x(), cabecalho.bottom() + kFiltros,
                                  cartao.width(), 1),
                       fio);
    }
  }

  bool OnMousePressed(const ui::MouseEvent& evento) override {
    // Clicar fora do cartão fecha (como a prévia de link).
    if (!Cartao().Contains(evento.location())) {
      Fechar();
      return true;
    }
    return false;
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }

  // --- views::TextfieldController ---------------------------------------------

  void ContentsChanged(views::Textfield* campo,
                       const std::u16string& texto) override {
    espera_.Start(FROM_HERE, kEsperaDaBusca,
                  base::BindOnce(&Painel::Buscar, base::Unretained(this)));
  }

  bool HandleKeyEvent(views::Textfield* campo,
                      const ui::KeyEvent& tecla) override {
    if (tecla.type() != ui::EventType::kKeyPressed) {
      return false;
    }
    const bool shift = tecla.IsShiftDown();
    const bool ctrl = tecla.IsControlDown();
    switch (tecla.key_code()) {
      case ui::VKEY_ESCAPE:
        Fechar();
        return true;
      case ui::VKEY_DOWN:
        Mover(1);
        return true;
      case ui::VKEY_UP:
        Mover(-1);
        return true;
      case ui::VKEY_NEXT:
        Mover(5);
        return true;
      case ui::VKEY_PRIOR:
        Mover(-5);
        return true;
      case ui::VKEY_RETURN:
        if (selecionada_) {
          Abrir(*selecionada_,
                ctrl ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                : shift ? WindowOpenDisposition::CURRENT_TAB
                        : WindowOpenDisposition::NEW_FOREGROUND_TAB);
        }
        return true;
      case ui::VKEY_DELETE:
        if (shift && selecionada_) {
          EsquecerEscolhida(tecla);
          return true;
        }
        return false;
      case ui::VKEY_TAB:
        if (!ctrl) {
          const int atual = static_cast<int>(periodo_);
          EscolherPeriodo(static_cast<Periodo>((atual + (shift ? 3 : 1)) % 4));
          return true;
        }
        return false;
      default:
        return false;
    }
  }

  // --- views::ContextMenuController (botão direito num resultado) -----------

  void ShowContextMenuForViewImpl(
      views::View* fonte,
      const gfx::Point& ponto,
      ui::mojom::MenuSourceType tipo) override {
    auto* linha = views::AsViewClass<Linha>(fonte);
    if (!linha) {
      return;
    }
    menu_indice_ = linha->indice();
    Selecionar(linha->indice(), /*rolar=*/false);
    const Resultado& r = resultados_[linha->indice()];
    menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_->AddItemWithIcon(kAbrirNoTrecho, u"Abrir no trecho",
                           ui::ImageModel::FromVectorIcon(
                               icones_menu::kTextSearchIcon,
                               ui::kColorMenuIcon, 16));
    menu_->AddItemWithIcon(kAbrirAoFundo, u"Abrir numa aba de fundo",
                           ui::ImageModel::FromVectorIcon(
                               icones_menu::kSquareArrowOutUpRightIcon,
                               ui::kColorMenuIcon, 16));
    menu_->AddItem(kAbrirAqui, u"Abrir nesta aba");
    menu_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_->AddItemWithIcon(kCopiarEndereco, u"Copiar o endereço",
                           ui::ImageModel::FromVectorIcon(
                               icones_menu::kLinkIcon, ui::kColorMenuIcon, 16));
    menu_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_->AddItemWithIcon(kEsquecerPagina, u"Esquecer esta página",
                           ui::ImageModel::FromVectorIcon(
                               icones_menu::kTrashIcon, ui::kColorMenuIcon,
                               16));
    menu_->AddItemWithIcon(
        kNaoLembrarDoSite,
        u"Não lembrar mais de " + base::UTF8ToUTF16(r.host),
        ui::ImageModel::FromVectorIcon(icones_menu::kEyeOffIcon,
                                       ui::kColorMenuIcon, 16));
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_.get(), views::MenuRunner::CONTEXT_MENU);
    menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(ponto, gfx::Size()),
                            views::MenuAnchorPosition::kTopLeft, tipo);
  }

  // --- ui::SimpleMenuModel::Delegate --------------------------------------------

  void ExecuteCommand(int comando, int flags) override {
    if (!menu_indice_ || *menu_indice_ >= resultados_.size()) {
      return;
    }
    const size_t indice = *menu_indice_;
    switch (comando) {
      case kAbrirNoTrecho:
        Abrir(indice, WindowOpenDisposition::NEW_FOREGROUND_TAB);
        break;
      case kAbrirAqui:
        Abrir(indice, WindowOpenDisposition::CURRENT_TAB);
        break;
      case kAbrirAoFundo:
        Abrir(indice, WindowOpenDisposition::NEW_BACKGROUND_TAB);
        break;
      case kCopiarEndereco:
        links::Copiar(browser_, base::UTF8ToUTF16(resultados_[indice].url));
        break;
      case kEsquecerPagina:
        Esquecer(indice);
        break;
      case kNaoLembrarDoSite: {
        const std::string host = resultados_[indice].host;
        DeixarDeFora(perfil_, host, /*fora=*/true);
        // As páginas do site somem já (o banco apaga em segundo plano).
        TirarDaLista([&host](const Resultado& r) { return r.host == host; });
        break;
      }
    }
  }

  // --- TabStripModelObserver: trocou de aba (ou de espaço), fecha -------------

  void OnTabStripModelChanged(
      TabStripModel* modelo,
      const TabStripModelChange& mudanca,
      const TabStripSelectionChange& selecao) override {
    if (selecao.active_tab_changed() && !segurar_) {
      Fechar();
    }
  }

  // --- views::WidgetObserver: a janela está fechando --------------------------

  void OnWidgetDestroying(views::Widget* widget) override {
    janela_fechando_ = true;
    janela_widget_.Reset();
    browser_->GetTabStripModel()->RemoveObserver(this);
    Paineis().erase(browser_);
  }

 private:
  void Aparecer() {
    // Cresce um pouco a partir do centro do cartão, aparecendo.
    const gfx::Point centro = Cartao().CenterPoint();
    gfx::Transform inicio;
    inicio.Translate(centro.x(), centro.y());
    inicio.Scale(0.97, 0.97);
    inicio.Translate(-centro.x(), -centro.y());
    layer()->SetTransform(inicio);
    layer()->SetOpacity(0.0f);
    views::AnimationBuilder()
        .Once()
        .SetDuration(
            gfx::Animation::RichAnimationDuration(base::Milliseconds(170)))
        .SetOpacity(layer(), 1.0f, gfx::Tween::EASE_OUT)
        .SetTransform(layer(), gfx::Transform(), gfx::Tween::FAST_OUT_SLOW_IN);
  }

  void Tirar() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<Painel> painel) {
                         if (!painel || !painel->parent()) {
                           return;
                         }
                         views::View* pai = painel->parent();
                         pai->RemoveChildViewT(painel.get());
                         pai->InvalidateLayout();
                       },
                       weak_factory_.GetWeakPtr()));
  }

  // Tira da lista os resultados que `sai` aponta (esquecidos), mantendo a
  // escolha perto de onde estava.
  template <typename Sai>
  void TirarDaLista(Sai sai) {
    const size_t antes = selecionada_.value_or(0);
    std::erase_if(resultados_, sai);
    if (resultados_.empty()) {
      selecionada_.reset();
    } else {
      selecionada_ = std::min(antes, resultados_.size() - 1);
    }
    Montar();
  }

  void Resumiu(Resumo resumo) {
    total_ = resumo.paginas;
    if (total_ == 0 && resultados_.empty()) {
      Montar();
    }
  }

  void EscolherPeriodo(Periodo periodo) {
    periodo_ = periodo;
    for (size_t i = 0; i < periodos_.size(); ++i) {
      periodos_[i]->SetEscolhida(static_cast<Periodo>(i) == periodo);
    }
    Buscar();
  }

  void AlternarEspaco() {
    so_este_espaco_ = !so_este_espaco_;
    so_este_->SetEscolhida(so_este_espaco_);
    Buscar();
  }

  void AbrirConfiguracoes() {
    BrowserWindowInterface* browser = browser_;
    Fechar();
    configuracoes::Mostrar(browser, configuracoes::Aba::kMemoria);
  }

  void Buscar() {
    espera_.Stop();
    if (anonima_ || fechando_) {
      Montar();
      return;
    }
    Filtro filtro;
    filtro.consulta = std::u16string(
        base::TrimWhitespace(campo_->GetText(), base::TRIM_ALL));
    // Só o que não dá pra buscar ("a", pontuação): fica a linha do tempo.
    curta_ = !filtro.consulta.empty() &&
             texto::Montar(filtro.consulta).fts.empty();
    if (curta_) {
      filtro.consulta.clear();
    }
    const base::Time agora = base::Time::Now();
    switch (periodo_) {
      case Periodo::kTudo:
        break;
      case Periodo::kHoje:
        filtro.desde = agora.LocalMidnight();
        break;
      case Periodo::kSemana:
        filtro.desde = agora - base::Days(7);
        break;
      case Periodo::kMes:
        filtro.desde = agora - base::Days(30);
        break;
    }
    if (so_este_espaco_) {
      filtro.espaco = espaco_;
    }
    filtro.limite =
        filtro.consulta.empty() ? kLimiteDaLinhaDoTempo : kLimiteDaBusca;
    filtro.tamanho_do_trecho = kTamanhoDoTrecho;
    consulta_feita_ = filtro.consulta;
    memoria::Buscar(perfil_, std::move(filtro),
                    base::BindOnce(&Painel::Chegou, weak_factory_.GetWeakPtr(),
                                   ++pedido_));
  }

  void Chegou(int pedido, std::vector<Resultado> resultados) {
    if (pedido != pedido_ || fechando_) {
      return;
    }
    // A escolha fica no mesmo resultado, se ele continua na lista.
    std::optional<int64_t> antes;
    if (selecionada_ && *selecionada_ < resultados_.size()) {
      antes = resultados_[*selecionada_].id;
    }
    resultados_ = std::move(resultados);
    selecionada_.reset();
    if (!resultados_.empty()) {
      selecionada_ = 0;
      for (size_t i = 0; antes && i < resultados_.size(); ++i) {
        if (resultados_[i].id == *antes) {
          selecionada_ = i;
          break;
        }
      }
    }
    // Abertas em alguma janela do perfil: a pílula "Aberta" e o Enter vai
    // pra aba.
    abertas_.clear();
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* janela) {
          if (janela->GetProfile() == perfil_) {
            TabStripModel* modelo = janela->GetTabStripModel();
            for (int i = 0; i < modelo->count(); ++i) {
              abertas_.insert(
                  Chave(modelo->GetWebContentsAt(i)->GetLastCommittedURL()));
            }
          }
          return true;
        });
    Montar();
    if (selecionada_) {
      Selecionar(*selecionada_, /*rolar=*/true);
    }
  }

  // Refaz a lista (e o estado no cabeçalho) com os resultados.
  void Montar() {
    lista_->RemoveAllChildViews();
    linhas_.clear();
    const bool linha_do_tempo = consulta_feita_.empty();
    if (anonima_) {
      estado_.clear();
      lista_->AddChildView(std::make_unique<Vazio>(
          icones_menu::kHatGlassesIcon, u"Fora da Memória",
          u"Nada desta janela é guardado. A busca no que você leu fica nas "
          u"janelas normais."));
    } else if (resultados_.empty()) {
      estado_.clear();
      if (total_ == 0 && linha_do_tempo && periodo_ == Periodo::kTudo &&
          !so_este_espaco_) {
        lista_->AddChildView(std::make_unique<Vazio>(
            icones_menu::kTextSearchIcon, u"A Memória começa agora",
            u"O Norte guarda o texto das páginas que você lê (só neste "
            u"computador). Depois, é só buscar aqui, ou na barra de "
            u"endereço, uma frase ou um termo que você lembra."));
      } else if (linha_do_tempo) {
        lista_->AddChildView(std::make_unique<Vazio>(
            icones_menu::kClockIcon, u"Nada neste período",
            u"Nenhuma página lida aqui ainda. Troque o período ou o espaço.",
            u"Ver tudo",
            base::BindRepeating(&Painel::VerTudo, base::Unretained(this))));
      } else {
        lista_->AddChildView(std::make_unique<Vazio>(
            icones_menu::kTextSearchIcon,
            u"Nada com “" + consulta_feita_ + u"”",
            periodo_ != Periodo::kTudo || so_este_espaco_
                ? u"Não está neste período ou neste espaço."
                : u"Tente menos palavras, ou o começo delas: “japo” acha "
                  u"“japonês”.",
            periodo_ != Periodo::kTudo || so_este_espaco_ ? u"Buscar em tudo"
                                                          : u"",
            base::BindRepeating(&Painel::VerTudo, base::Unretained(this))));
      }
    } else {
      if (linha_do_tempo) {
        estado_ = curta_ ? u"Continue digitando…" : u"Linha do tempo";
      } else {
        estado_ = resultados_.size() >= kLimiteDaBusca
                      ? u"Mais de " + base::NumberToString16(kLimiteDaBusca) +
                            u" páginas"
                      : resultados_.size() == 1
                            ? u"1 página"
                            : base::NumberToString16(resultados_.size()) +
                                  u" páginas";
      }
      std::optional<int> dia_anterior;
      for (size_t i = 0; i < resultados_.size(); ++i) {
        if (linha_do_tempo) {
          const int dia = DiasEntre(resultados_[i].ultima, base::Time::Now());
          if (dia != dia_anterior) {
            dia_anterior = dia;
            lista_->AddChildView(
                std::make_unique<Grupo>(Dia(resultados_[i].ultima)));
          }
        }
        Linha* linha = lista_->AddChildView(
            std::make_unique<Linha>(this, i, linha_do_tempo));
        linha->set_context_menu_controller(this);
        linhas_.push_back(linha);
      }
    }
    detalhe_->Mostrar(selecionada_);
    InvalidateLayout();
    SchedulePaint();
  }

  void VerTudo() {
    so_este_espaco_ = false;
    if (so_este_) {
      so_este_->SetEscolhida(false);
    }
    EscolherPeriodo(Periodo::kTudo);
  }

  void Selecionar(size_t indice, bool rolar) {
    if (indice >= resultados_.size()) {
      return;
    }
    const std::optional<size_t> antes = selecionada_;
    selecionada_ = indice;
    if (antes && *antes < linhas_.size()) {
      linhas_[*antes]->SchedulePaint();
    }
    if (indice < linhas_.size()) {
      linhas_[indice]->SchedulePaint();
      if (rolar) {
        linhas_[indice]->ScrollViewToVisible();
      }
    }
    if (antes != selecionada_) {
      detalhe_->Mostrar(selecionada_);
    }
  }

  void Mover(int passos) {
    if (resultados_.empty()) {
      return;
    }
    const int atual = selecionada_ ? static_cast<int>(*selecionada_) : -1;
    const int novo = std::clamp(atual + passos, 0,
                                static_cast<int>(resultados_.size()) - 1);
    Selecionar(static_cast<size_t>(novo), /*rolar=*/true);
  }

  void Abrir(size_t indice, WindowOpenDisposition onde) {
    if (indice >= resultados_.size()) {
      return;
    }
    const Resultado resultado = resultados_[indice];
    BrowserWindowInterface* browser = browser_;
    if (onde == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
      // O painel fica: dá pra abrir vários. A aba nova não troca a ativa.
      segurar_ = true;
      AbrirResultado(browser, resultado, onde);
      segurar_ = false;
      abertas_.insert(resultado.url);
      if (indice < linhas_.size()) {
        linhas_[indice]->SchedulePaint();
      }
      return;
    }
    Fechar();
    AbrirResultado(browser, resultado, onde);
  }

  void AbrirEscolhida(WindowOpenDisposition onde, const ui::Event& evento) {
    if (selecionada_) {
      Abrir(*selecionada_, onde);
    }
  }

  void Esquecer(size_t indice) {
    if (indice >= resultados_.size()) {
      return;
    }
    memoria::Esquecer(perfil_, resultados_[indice].id);
    const int64_t id = resultados_[indice].id;
    TirarDaLista([id](const Resultado& r) { return r.id == id; });
    if (total_ > 0) {
      --total_;
    }
  }

  void EsquecerEscolhida(const ui::Event& evento) {
    if (selecionada_) {
      Esquecer(*selecionada_);
    }
  }

  void ChegouMiniatura(int64_t id, const gfx::ImageSkia& imagem) {
    miniaturas_[id] = imagem;
    RepintarCom([id](const Resultado& r) { return r.id == id; });
  }

  void ChegouIcone(const std::string& url,
                   const favicon_base::FaviconImageResult& resultado) {
    icones_[url] = resultado.image.AsImageSkia();
    RepintarCom([&url](const Resultado& r) { return r.url == url; });
  }

  template <typename Qual>
  void RepintarCom(Qual qual) {
    for (Linha* linha : linhas_) {
      if (qual(resultados_[linha->indice()])) {
        linha->SchedulePaint();
      }
    }
    if (selecionada_ && *selecionada_ < resultados_.size() &&
        qual(resultados_[*selecionada_])) {
      detalhe_->SchedulePaint();
    }
  }

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<Profile> perfil_;
  const bool anonima_;
  // O espaço da janela quando o painel abriu (o "Só em ...").
  const std::string espaco_;

  raw_ptr<views::View> lupa_ = nullptr;
  raw_ptr<views::Textfield> campo_ = nullptr;
  std::vector<raw_ptr<Ficha>> periodos_;
  raw_ptr<Ficha> so_este_ = nullptr;
  raw_ptr<views::MdTextButton> configurar_ = nullptr;
  raw_ptr<views::ScrollView> rolagem_ = nullptr;
  raw_ptr<views::View> lista_ = nullptr;
  raw_ptr<Detalhe> detalhe_ = nullptr;
  raw_ptr<Rodape> rodape_ = nullptr;
  std::vector<raw_ptr<Linha>> linhas_;

  Periodo periodo_ = Periodo::kTudo;
  bool so_este_espaco_ = false;
  bool primeira_vez_ = true;
  bool fechando_ = false;
  bool janela_fechando_ = false;
  // Abrindo numa aba de fundo: a troca do modelo não fecha o painel.
  bool segurar_ = false;
  // O texto tinha só o que não dá pra buscar: mostrando a linha do tempo.
  bool curta_ = false;
  int pedido_ = 0;
  int64_t total_ = -1;
  std::u16string consulta_feita_;
  std::u16string estado_;
  std::vector<Resultado> resultados_;
  std::optional<size_t> selecionada_;
  std::set<std::string> abertas_;
  std::map<int64_t, gfx::ImageSkia> miniaturas_;
  std::set<int64_t> pedidas_;
  std::map<std::string, gfx::ImageSkia> icones_;
  std::set<std::string> icones_pedidos_;
  base::CancelableTaskTracker icones_tracker_;
  base::OneShotTimer espera_;

  std::unique_ptr<ui::SimpleMenuModel> menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::optional<size_t> menu_indice_;

  base::ScopedObservation<views::Widget, views::WidgetObserver> janela_widget_{
      this};
  base::WeakPtrFactory<Painel> weak_factory_{this};
};

BEGIN_METADATA(Painel)
END_METADATA

// Na aba em que a página já está aberta: seleciona o trecho (e a página rola
// até ele), pelo mundo isolado do Norte.
void IrAoTrecho(content::WebContents* aba, const std::u16string& frase) {
  aba->Focus();
  if (frase.empty()) {
    return;
  }
  std::optional<std::string> literal = base::WriteJson(base::Value(frase));
  if (!literal) {
    return;
  }
  const std::u16string js =
      u"(() => { const s = getSelection(); if (s) s.removeAllRanges(); "
      u"window.find(" +
      base::UTF8ToUTF16(*literal) +
      u", false, false, true, false, true, false); })()";
  aba->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      js, base::NullCallback(), ISOLATED_WORLD_ID_NAVEGADOR);
}

}  // namespace

void AbrirPainel(BrowserWindowInterface* browser,
                 const std::u16string& consulta) {
  if (auto it = Paineis().find(browser); it != Paineis().end()) {
    if (!it->second->fechando()) {
      it->second->Focar(consulta);
      return;
    }
    // Ainda sumindo: um novo toma o lugar (o velho sai sozinho).
    Paineis().erase(it);
  }
  BrowserView* janela = BrowserView::GetBrowserViewForBrowser(browser);
  if (!janela || !janela->GetWidget() || !janela->contents_container()) {
    return;
  }
  views::View* area = janela->contents_container();
  Painel* painel =
      area->AddChildView(std::make_unique<Painel>(browser, janela));
  area->InvalidateLayout();
  painel->Focar(consulta);
}

void AbrirResultado(BrowserWindowInterface* browser,
                    const Resultado& resultado,
                    WindowOpenDisposition onde) {
  Profile* perfil = browser->GetProfile();
  // Já aberta numa aba (de qualquer janela do perfil): vai pra ela.
  if (onde != WindowOpenDisposition::CURRENT_TAB) {
    BrowserWindowInterface* achada = nullptr;
    int indice = TabStripModel::kNoTab;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* janela) {
          if (janela->GetProfile() != perfil) {
            return true;
          }
          TabStripModel* modelo = janela->GetTabStripModel();
          for (int i = 0; i < modelo->count(); ++i) {
            if (Chave(modelo->GetWebContentsAt(i)->GetLastCommittedURL()) ==
                resultado.url) {
              achada = janela;
              indice = i;
              return false;
            }
          }
          return true;
        });
    if (achada) {
      if (onde == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
        return;
      }
      if (achada != browser) {
        achada->GetWindow()->Activate();
      }
      TabStripModel* modelo = achada->GetTabStripModel();
      modelo->ActivateTabAt(indice);
      IrAoTrecho(modelo->GetWebContentsAt(indice), resultado.trecho.frase);
      return;
    }
  }
  const GURL url = EnderecoNoTrecho(resultado);
  TabStripModel* modelo = browser->GetTabStripModel();
  if (onde == WindowOpenDisposition::CURRENT_TAB) {
    if (content::WebContents* ativa = modelo->GetActiveWebContents()) {
      content::NavigationController::LoadURLParams carregar(url);
      carregar.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
      ativa->GetController().LoadURLWithParams(carregar);
      ativa->Focus();
    }
    return;
  }
  // Uma aba nova logo depois da atual (no espaço dela), no container em que
  // a página foi lida, onde está o login (se ele ainda existe).
  std::string container = resultado.container;
  if (!container.empty() && !containers::Get(perfil, container)) {
    container.clear();
  }
  containers::AbrirEmContainer(
      modelo, url, container, modelo->active_index(),
      /*ativar=*/onde != WindowOpenDisposition::NEW_BACKGROUND_TAB);
}

}  // namespace navegador::memoria
