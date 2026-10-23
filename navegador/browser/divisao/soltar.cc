#include "navegador/browser/divisao/soltar.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/web_contents.h"
#include "navegador/browser/divisao/divisao.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_solid_color.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace navegador::divisao {

namespace {

constexpr base::TimeDelta kMovimento = base::Milliseconds(170);
constexpr base::TimeDelta kAparecer = base::Milliseconds(140);
constexpr base::TimeDelta kAbrir = base::Milliseconds(150);
// Como os painéis (kSplitViewContentRoundedCorners e o contorno).
constexpr float kRaioDoPainel = 8;
constexpr float kRaioDoCartao = 12;
constexpr float kOpacidadeDoApagado = 0.32f;
// O cartão fica um pouco pra dentro da metade, como um painel novo.
constexpr int kMargemDoCartao = 6;
constexpr int kIconeDoCartao = 28;
constexpr int kLarguraMaximaDoTexto = 280;

// O lado do retângulo mais perto do ponto (o painel fica dividido em quatro
// triângulos pelas diagonais).
Lado LadoMaisPerto(const gfx::Rect& r, const gfx::Point& p) {
  const float x =
      r.width() > 0 ? static_cast<float>(p.x() - r.x()) / r.width() : 0.5f;
  const float y =
      r.height() > 0 ? static_cast<float>(p.y() - r.y()) / r.height() : 0.5f;
  Lado lado = Lado::kEsquerda;
  float menor = x;
  if (1 - x < menor) {
    menor = 1 - x;
    lado = Lado::kDireita;
  }
  if (y < menor) {
    menor = y;
    lado = Lado::kCima;
  }
  if (1 - y < menor) {
    lado = Lado::kBaixo;
  }
  return lado;
}

gfx::Rect Metade(const gfx::Rect& r, Lado lado) {
  switch (lado) {
    case Lado::kEsquerda:
      return gfx::Rect(r.x(), r.y(), r.width() / 2, r.height());
    case Lado::kDireita:
      return gfx::Rect(r.x() + r.width() / 2, r.y(), r.width() - r.width() / 2,
                       r.height());
    case Lado::kCima:
      return gfx::Rect(r.x(), r.y(), r.width(), r.height() / 2);
    case Lado::kBaixo:
      return gfx::Rect(r.x(), r.y() + r.height() / 2, r.width(),
                       r.height() - r.height() / 2);
  }
  return r;
}

SkColor ComAlfa(SkColor cor, float alfa) {
  return SkColorSetA(cor, static_cast<U8CPU>(
                              std::clamp(alfa, 0.0f, 1.0f) * SkColorGetA(cor)));
}

tabs::TabInterface* AbaDoPainel(MultiContentsView& paginas, int painel) {
  if (painel < 0 ||
      painel >= static_cast<int>(paginas.contents_container_views().size())) {
    return nullptr;
  }
  content::WebContents* const conteudo =
      paginas.contents_container_views()[painel]->contents_view()->web_contents();
  return conteudo ? tabs::TabInterface::MaybeGetFromContents(conteudo)
                  : nullptr;
}

}  // namespace

// O lugar onde a aba entra: vidro fosco (a cor da barra, translúcida, com o
// fundo desfocado pela camada) com a borda na cor do que acontece ao soltar e
// o ícone e o título da aba no meio. Só repinta quando muda de tamanho (o
// deslizar entre dois lugares) ou de conteúdo.
class PreviaDeSoltar::Cartao : public views::View {
  METADATA_HEADER(Cartao, views::View)

 public:
  Cartao() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kRaioDoCartao));
    layer()->SetIsFastRoundedCorner(true);
    layer()->SetBackgroundBlur(24);
    layer()->SetOpacity(0);
    SetCanProcessEventsWithinSubtree(false);
  }

  void Definir(Modo modo,
               const ui::ImageModel& icone,
               const std::u16string& titulo) {
    if (modo == modo_ && titulo == titulo_ && icone == icone_) {
      return;
    }
    modo_ = modo;
    icone_ = icone;
    titulo_ = titulo;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const ui::ColorProvider* cores = GetColorProvider();
    if (!cores) {
      return;
    }
    const gfx::RectF r(GetLocalBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    // Vidro na cor da barra; onde a aba vai entrar, puxado pro destaque.
    const SkColor vidro =
        modo_ == Modo::kJuntar
            ? gfx::Tween::ColorValueBetween(
                  0.16, cores->GetColor(kColorToolbar),
                  cores->GetColor(ui::kColorSysPrimary))
            : cores->GetColor(kColorToolbar);
    flags.setColor(ComAlfa(vidro, 0.76f));
    canvas->DrawRoundRect(r, kRaioDoCartao, flags);

    SkColor borda;
    switch (modo_) {
      case Modo::kJuntar:
        borda = cores->GetColor(ui::kColorSysPrimary);
        break;
      case Modo::kCheia:
        borda = cores->GetColor(ui::kColorSysError);
        break;
      case Modo::kTirar:
      case Modo::kArrastando:
        borda = ComAlfa(cores->GetColor(ui::kColorSysOnSurface), 0.35f);
        break;
    }
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1.5f);
    flags.setColor(borda);
    gfx::RectF contorno = r;
    contorno.Inset(0.75f);
    canvas->DrawRoundRect(contorno, kRaioDoCartao - 0.75f, flags);

    std::u16string texto = titulo_;
    ui::ImageModel icone = icone_;
    if (modo_ == Modo::kCheia) {
      texto = u"Uma divisão tem até 4 abas";
      icone = ui::ImageModel();
    } else if (modo_ == Modo::kTirar) {
      texto = u"Tirar da divisão";
    }

    // Ícone em cima, texto embaixo, no meio; num cartão pequeno, só o que
    // couber.
    const gfx::FontList fonte =
        gfx::FontList().DeriveWithSizeDelta(1).DeriveWithWeight(
            gfx::Font::Weight::MEDIUM);
    const int altura_do_texto = fonte.GetHeight();
    const int largura_do_texto =
        std::min({kLarguraMaximaDoTexto, gfx::GetStringWidth(texto, fonte),
                  width() - 32});
    const bool com_texto = !texto.empty() && largura_do_texto >= 40 &&
                           height() >= altura_do_texto + 24;
    const bool com_icone =
        !icone.IsEmpty() &&
        height() >= kIconeDoCartao + (com_texto ? altura_do_texto + 10 : 0) +
                        24 &&
        width() >= kIconeDoCartao + 24;
    const int altura = (com_icone ? kIconeDoCartao : 0) +
                       (com_icone && com_texto ? 10 : 0) +
                       (com_texto ? altura_do_texto : 0);
    int y = (height() - altura) / 2;
    if (com_icone) {
      const gfx::ImageSkia imagem = icone.Rasterize(cores);
      canvas->DrawImageInt(imagem, 0, 0, imagem.width(), imagem.height(),
                           (width() - kIconeDoCartao) / 2, y, kIconeDoCartao,
                           kIconeDoCartao, /*filter=*/true);
      y += kIconeDoCartao + 10;
    }
    if (com_texto) {
      canvas->DrawStringRectWithFlags(
          gfx::ElideText(texto, fonte, largura_do_texto, gfx::ELIDE_TAIL),
          fonte, cores->GetColor(ui::kColorSysOnSurface),
          gfx::Rect((width() - largura_do_texto) / 2, y, largura_do_texto,
                    altura_do_texto),
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    }
  }

 private:
  Modo modo_ = Modo::kJuntar;
  ui::ImageModel icone_;
  std::u16string titulo_;
};

BEGIN_METADATA(PreviaDeSoltar, Cartao)
END_METADATA

PreviaDeSoltar::PreviaDeSoltar() {
  SetID(kIdSoltar);
  // Os eventos (e o arrasto) passam pra página embaixo.
  SetCanProcessEventsWithinSubtree(false);
  SetVisible(false);
  apagado_ = AddChildView(std::make_unique<views::View>());
  apagado_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  apagado_->layer()->AsSolidColor()->SetColor(SkColors::kBlack);
  apagado_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kRaioDoPainel));
  apagado_->layer()->SetIsFastRoundedCorner(true);
  apagado_->layer()->SetOpacity(0);
  cartao_ = AddChildView(std::make_unique<Cartao>());
  movimento_.SetSlideDuration(kMovimento);
  movimento_.SetTweenType(gfx::Tween::EASE_OUT);
}

PreviaDeSoltar::~PreviaDeSoltar() = default;

void PreviaDeSoltar::Aparecer(views::View* view, float opacidade) {
  if (view->layer()->GetTargetOpacity() == opacidade) {
    return;
  }
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kAparecer)
      .SetOpacity(view->layer(), opacidade, gfx::Tween::EASE_OUT);
}

void PreviaDeSoltar::Mostrar(Modo modo,
                             const gfx::Rect& painel,
                             const gfx::Rect& novo,
                             const gfx::Rect& origem,
                             const ui::ImageModel& icone,
                             const std::u16string& titulo) {
  gfx::Rect destino = modo == Modo::kJuntar  ? novo
                      : modo == Modo::kCheia ? painel
                                             : origem;
  destino.Inset(kMargemDoCartao);
  cartao_->Definir(modo, icone, titulo);
  if (!mostrando_) {
    // Aparece já no lugar, sem vir deslizando de onde parou da outra vez.
    mostrando_ = true;
    SetVisible(true);
    de_ = para_ = destino;
    movimento_.Reset(1.0);
    cartao_->SetBoundsRect(destino);
  } else if (destino != para_) {
    de_ = cartao_->bounds();
    para_ = destino;
    movimento_.Reset(0.0);
    movimento_.Show();
  }
  modo_ = modo;
  if (origem != origem_) {
    origem_ = origem;
    if (!origem.IsEmpty()) {
      apagado_->SetBoundsRect(origem);
    }
  }
  Aparecer(cartao_, 1.0f);
  Aparecer(apagado_, origem.IsEmpty() ? 0.0f : kOpacidadeDoApagado);
}

void PreviaDeSoltar::Esconder() {
  if (!mostrando_) {
    return;
  }
  mostrando_ = false;
  origem_ = gfx::Rect();
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<PreviaDeSoltar> eu) {
            if (eu && !eu->mostrando_) {
              eu->SetVisible(false);
            }
          },
          fabrica_.GetWeakPtr()))
      .Once()
      .SetDuration(kAparecer)
      .SetOpacity(cartao_->layer(), 0.0f, gfx::Tween::EASE_IN)
      .SetOpacity(apagado_->layer(), 0.0f, gfx::Tween::EASE_IN);
}

void PreviaDeSoltar::AnimationProgressed(const gfx::Animation* animation) {
  cartao_->SetBoundsRect(gfx::Tween::RectValueBetween(
      movimento_.GetCurrentValue(), de_, para_));
}

void PreviaDeSoltar::AnimationEnded(const gfx::Animation* animation) {
  cartao_->SetBoundsRect(para_);
}

BEGIN_METADATA(PreviaDeSoltar)
END_METADATA

AlcaDoPainel::AlcaDoPainel(MultiContentsView& paginas,
                           PreviaDeSoltar& previa,
                           TabStripModel& modelo,
                           int painel)
    : paginas_(paginas), previa_(previa), modelo_(modelo), painel_(painel) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetVisible(false);
  set_context_menu_controller(this);
  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(u"Mover o painel");
  SetTooltipText(
      u"Arraste pra outro painel, ou pra lateral pra tirar da divisão");
  abrir_.SetSlideDuration(kAbrir);
  abrir_.SetTweenType(gfx::Tween::EASE_OUT);
  AtualizarEventos();
}

AlcaDoPainel::~AlcaDoPainel() = default;

void AlcaDoPainel::SetAtiva(bool ativa) {
  if (ativa_ != ativa) {
    ativa_ = ativa;
    SchedulePaint();
  }
}

void AlcaDoPainel::SetPerto(bool perto) {
  if (perto_ == perto) {
    return;
  }
  perto_ = perto;
  AtualizarEventos();
  if (perto_ || arrastando_) {
    abrir_.Show();
  } else {
    abrir_.Hide();
  }
}

void AlcaDoPainel::AtualizarEventos() {
  // Quieta, o clique passa pra página.
  SetCanProcessEventsWithinSubtree(perto_ || arrastando_);
}

tabs::TabInterface* AlcaDoPainel::Aba() const {
  return AbaDoPainel(paginas_.get(), painel_);
}

void AlcaDoPainel::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void AlcaDoPainel::OnPaint(gfx::Canvas* canvas) {
  const ui::ColorProvider* cores = GetColorProvider();
  if (!cores) {
    return;
  }
  const float t = arrastando_ ? 1.0f : abrir_.GetCurrentValue();
  const SkColor acento = cores->GetColor(ui::kColorSysPrimary);
  const SkColor barra = cores->GetColor(kColorToolbar);

  // Quieta: um risquinho (na cor de destaque no painel ativo). Aberta: uma
  // cápsula de vidro com "•••".
  const float largura = 26 + 22 * t;
  const float altura = 5 + 13 * t;
  const gfx::RectF capsula((width() - largura) / 2, 6, largura, altura);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const SkColor fundo_quieto = ativa_ ? acento : barra;
  flags.setColor(ComAlfa(
      gfx::Tween::ColorValueBetween(t, fundo_quieto, barra), 0.7f + 0.22f * t));
  canvas->DrawRoundRect(capsula, altura / 2, flags);
  // Um fio claro, pra aparecer também em página escura.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setColor(ComAlfa(SK_ColorWHITE, 0.14f + 0.1f * t));
  gfx::RectF fio = capsula;
  fio.Inset(0.5f);
  canvas->DrawRoundRect(fio, altura / 2 - 0.5f, flags);

  if (t <= 0) {
    return;
  }
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(ComAlfa(ativa_ || arrastando_
                             ? acento
                             : cores->GetColor(ui::kColorSysOnSurface),
                         t * 0.95f));
  const float raio = 1.0f + 1.25f * t;
  for (int i = -1; i <= 1; ++i) {
    canvas->DrawCircle(
        gfx::PointF(capsula.CenterPoint().x() + i * 7.0f * t,
                    capsula.CenterPoint().y()),
        raio, flags);
  }
}

bool AlcaDoPainel::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton()) {
    return false;
  }
  inicio_ = event.location();
  cancelado_ = false;
  tabs::TabInterface* const aba = Aba();
  aba_ = aba ? aba->GetHandle() : tabs::TabHandle();
  return aba != nullptr;
}

bool AlcaDoPainel::OnMouseDragged(const ui::MouseEvent& event) {
  if (cancelado_) {
    return true;
  }
  if (!arrastando_) {
    if (!views::View::ExceededDragThreshold(event.location() - inicio_)) {
      return true;
    }
    arrastando_ = true;
    ultimo_.reset();
    AtualizarEventos();
    if (GetWidget()) {
      monitor_ = views::EventMonitor::CreateWindowMonitor(
          this, GetWidget()->GetNativeWindow(), {ui::EventType::kKeyPressed});
      GetWidget()->SetCursor(ui::Cursor(ui::mojom::CursorType::kGrabbing));
    }
    SchedulePaint();
  }
  gfx::Point ponto = event.location();
  views::View::ConvertPointToTarget(this, &paginas_.get(), &ponto);
  Atualizar(ponto);
  return true;
}

void AlcaDoPainel::OnMouseReleased(const ui::MouseEvent& event) {
  if (arrastando_) {
    Terminar(/*soltar=*/true);
    return;
  }
  if (cancelado_) {
    cancelado_ = false;
    return;
  }
  // Um clique: o painel fica ativo.
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    if (tabs::TabInterface* const aba = aba_.Get()) {
      if (const int indice = modelo_->GetIndexOfTab(aba);
          indice != TabStripModel::kNoTab) {
        modelo_->ActivateTabAt(indice);
      }
    }
  }
}

void AlcaDoPainel::OnMouseCaptureLost() {
  if (arrastando_) {
    Terminar(/*soltar=*/false);
  }
}

ui::Cursor AlcaDoPainel::GetCursor(const ui::MouseEvent& event) {
  return ui::Cursor(arrastando_ ? ui::mojom::CursorType::kGrabbing
                                : ui::mojom::CursorType::kGrab);
}

void AlcaDoPainel::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  tabs::TabInterface* const aba = Aba();
  if (!aba || !aba->IsSplit() || !GetWidget()) {
    return;
  }
  const int indice = modelo_->GetIndexOfTab(aba);
  if (indice == TabStripModel::kNoTab) {
    return;
  }
  // O menu do painel (o mesmo que a barrinha do Chromium no canto abria).
  menu_ = std::make_unique<SplitTabMenuModel>(
      &modelo_.get(), SplitTabMenuModel::MenuSource::kMiniToolbar, indice);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void AlcaDoPainel::OnEvent(const ui::Event& event) {
  if (arrastando_ && event.IsKeyEvent() &&
      event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE) {
    cancelado_ = true;
    Terminar(/*soltar=*/false);
  }
}

void AlcaDoPainel::Atualizar(const gfx::Point& ponto) {
  tabs::TabInterface* const aba = aba_.Get();
  if (!aba || painel_ >= paginas_->NavegadorPaineis()) {
    cancelado_ = true;
    Terminar(/*soltar=*/false);
    return;
  }

  // Pra onde vai: a lateral (à esquerda das páginas), outro painel ou nada.
  std::optional<Destino> destino;
  int painel = -1;
  if (ponto.x() < 0) {
    destino = Destino{tabs::TabHandle(), Lado::kDireita, /*tirar=*/true};
  } else {
    painel = paginas_->NavegadorPainelEm(ponto);
    tabs::TabInterface* const alvo =
        painel != painel_ ? AbaDoPainel(paginas_.get(), painel) : nullptr;
    if (alvo) {
      destino = Destino{
          alvo->GetHandle(),
          LadoMaisPerto(paginas_->contents_container_views()[painel]->bounds(),
                        ponto),
          /*tirar=*/false};
    }
  }
  // A cada movimento do mouse só se o lugar mudou.
  if (ultimo_.has_value() && *ultimo_ == destino) {
    return;
  }
  ultimo_ = destino;

  const gfx::Rect origem =
      paginas_->contents_container_views()[painel_]->bounds();
  TabUIHelper* const helper = TabUIHelper::From(aba);
  const ui::ImageModel icone =
      helper ? helper->GetFavicon() : ui::ImageModel();
  if (!destino) {
    previa_->Mostrar(PreviaDeSoltar::Modo::kArrastando, origem, origem, origem,
                     ui::ImageModel(),
                     u"Solte em outro painel, ou na lateral pra tirar");
  } else if (destino->tirar) {
    previa_->Mostrar(PreviaDeSoltar::Modo::kTirar, origem, origem, origem,
                     icone, std::u16string());
  } else {
    const gfx::Rect retangulo =
        paginas_->contents_container_views()[painel]->bounds();
    previa_->Mostrar(PreviaDeSoltar::Modo::kJuntar, retangulo,
                     Metade(retangulo, destino->lado), origem, icone,
                     helper ? helper->GetTitle() : std::u16string());
  }
}

void AlcaDoPainel::Terminar(bool soltar) {
  arrastando_ = false;
  monitor_.reset();
  previa_->Esconder();
  AtualizarEventos();
  if (!perto_) {
    abrir_.Hide();
  }
  SchedulePaint();
  const std::optional<std::optional<Destino>> ultimo = std::move(ultimo_);
  ultimo_.reset();
  if (!soltar || !ultimo || !ultimo->has_value()) {
    return;
  }
  // Fora daqui: mexer na divisão reordena os painéis (e esta alça) no meio
  // do evento do mouse.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AlcaDoPainel> eu, tabs::TabHandle aba,
             Destino destino) {
            if (!eu || !aba.Get()) {
              return;
            }
            if (destino.tirar) {
              Tirar(&eu->modelo_.get(), aba.Get());
            } else if (tabs::TabInterface* const alvo = destino.alvo.Get()) {
              Juntar(&eu->modelo_.get(), aba.Get(), alvo, destino.lado);
            }
          },
          fabrica_.GetWeakPtr(), aba_, **ultimo));
}

BEGIN_METADATA(AlcaDoPainel)
END_METADATA

VigiaDasAlcas::VigiaDasAlcas(views::View& paginas,
                             const std::vector<raw_ptr<AlcaDoPainel>>& alcas)
    : paginas_(paginas), alcas_(alcas) {}

VigiaDasAlcas::~VigiaDasAlcas() = default;

void VigiaDasAlcas::Ligar(bool ligado) {
  if (ligado == !!monitor_) {
    return;
  }
  if (!ligado) {
    monitor_.reset();
    Fechar();
    return;
  }
  if (!paginas_->GetWidget()) {
    return;
  }
  monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, paginas_->GetWidget()->GetNativeWindow(),
      {ui::EventType::kMouseMoved, ui::EventType::kMouseExited});
}

void VigiaDasAlcas::Fechar() {
  for (AlcaDoPainel* alca : *alcas_) {
    alca->SetPerto(false);
  }
}

void VigiaDasAlcas::OnEvent(const ui::Event& event) {
  if (event.type() == ui::EventType::kMouseExited || !event.IsLocatedEvent()) {
    Fechar();
    return;
  }
  gfx::Point ponto =
      gfx::ToFlooredPoint(event.AsLocatedEvent()->root_location_f());
  views::View::ConvertPointFromWidget(&paginas_.get(), &ponto);
  for (AlcaDoPainel* alca : *alcas_) {
    if (!alca->GetVisible()) {
      continue;
    }
    // Perto: o meio do alto do painel, um pouco além da alça.
    gfx::Rect perto = alca->bounds();
    perto.Outset(gfx::Outsets::TLBR(0, 56, 30, 56));
    alca->SetPerto(alca->arrastando() || perto.Contains(ponto));
  }
}

AlvoDaDivisao::AlvoDaDivisao(MultiContentsView& paginas,
                             PreviaDeSoltar& previa,
                             TabStripModel& modelo)
    : paginas_(paginas), previa_(previa), modelo_(modelo) {}

AlvoDaDivisao::~AlvoDaDivisao() {
  ao_destruir_.Notify();
}

void AlvoDaDivisao::OnTabDragEntered() {}

TabDragContext* AlvoDaDivisao::OnTabDragUpdated(
    TabDragTarget::DragController& controller,
    const gfx::Point& point_in_screen) {
  const DragSessionData& dados = controller.GetSessionData();
  TabDragContext* const anexado =
      const_cast<TabDragContext*>(controller.GetAttachedContext());
  const bool mesma_janela =
      anexado && anexado->GetTabStripModel() == &modelo_.get();

  // Uma aba solta, ou a aba pega de uma divisão (o Chromium arrasta a
  // divisão inteira; em cima da página, só a aba pega muda de lugar ou sai
  // da divisão). Não um espaço inteiro.
  content::WebContents* const conteudo =
      dados.tab_drag_data_.empty() ||
              dados.source_view_index_ >= dados.tab_drag_data_.size()
          ? nullptr
          : dados.source_view_drag_data()->contents.get();
  tabs::TabInterface* const aba =
      conteudo ? tabs::TabInterface::MaybeGetFromContents(conteudo) : nullptr;
  bool da_divisao = false;
  if (aba && aba->IsSplit() && mesma_janela &&
      dados.num_dragging_tabs() ==
          static_cast<int>(
              modelo_->GetSplitData(*aba->GetSplit())->ListTabs().size())) {
    da_divisao = std::ranges::all_of(
        dados.tab_drag_data_, [&](const TabDragData& dado) {
          tabs::TabInterface* const outra =
              dado.contents
                  ? tabs::TabInterface::MaybeGetFromContents(dado.contents)
                  : nullptr;
          return outra && outra->GetSplit() == aba->GetSplit();
        });
  }
  if (!aba || (dados.num_dragging_tabs() != 1 && !da_divisao) ||
      !dados.dragging_groups.empty() ||
      dados.group_header_drag_data_.has_value()) {
    Limpar();
    return nullptr;
  }

  // O clique ativou a aba arrastada; quem estava à vista antes volta pra
  // tela, e é com ela que a aba divide (se for da mesma divisão, ela já está
  // à vista).
  if (mesma_janela && !trocou_ && aba->IsActivated()) {
    tabs::TabInterface* const antes = controller.NavegadorAtivaAntes();
    if (antes && antes != aba &&
        !(antes->IsSplit() && antes->GetSplit() == aba->GetSplit()) &&
        modelo_->GetIndexOfTab(antes) != TabStripModel::kNoTab) {
      trocou_ = true;
      trocou_de_ = aba->GetHandle();
      modelo_->ActivateTabAt(modelo_->GetIndexOfTab(antes));
    }
  }

  gfx::Point ponto = point_in_screen;
  views::View::ConvertPointFromScreen(&paginas_.get(), &ponto);
  const int painel = paginas_->NavegadorPainelEm(ponto);
  tabs::TabInterface* const alvo = AbaDoPainel(paginas_.get(), painel);
  if (!alvo || alvo == aba) {
    Limpar();
    return mesma_janela ? anexado : nullptr;
  }

  const gfx::Rect retangulo =
      paginas_->contents_container_views()[painel]->bounds();
  const Lado lado = LadoMaisPerto(retangulo, ponto);
  const bool pode =
      mesma_janela
          ? PodeJuntar(&modelo_.get(), aba, alvo)
          : (!alvo->IsSplit() ||
             modelo_->GetSplitData(*alvo->GetSplit())->ListTabs().size() <
                 static_cast<size_t>(kMaximoDePaineis));
  const Escolha escolha{alvo->GetHandle(), lado, pode};
  // A cada movimento do mouse só se o lugar mudou.
  if (escolha_ == escolha) {
    return mesma_janela ? anexado : nullptr;
  }
  escolha_ = escolha;
  TabUIHelper* const helper = TabUIHelper::From(aba);
  previa_->Mostrar(
      pode ? PreviaDeSoltar::Modo::kJuntar : PreviaDeSoltar::Modo::kCheia,
      retangulo, Metade(retangulo, lado), gfx::Rect(),
      helper ? helper->GetFavicon() : ui::ImageModel(),
      helper ? helper->GetTitle() : std::u16string());
  return mesma_janela ? anexado : nullptr;
}

void AlvoDaDivisao::OnTabDragExited(const gfx::Point& point_in_screen) {
  Limpar();
  DevolverAtivacao();
}

void AlvoDaDivisao::OnTabDragEnded() {
  Limpar();
  trocou_ = false;
}

bool AlvoDaDivisao::CanDropTab() {
  return escolha_.has_value() && escolha_->pode && escolha_->alvo.Get();
}

void AlvoDaDivisao::HandleTabDrop(TabDragTarget::DragController& controller) {
  if (!CanDropTab()) {
    return;
  }
  const Escolha escolha = *escolha_;
  Limpar();
  trocou_ = false;

  const DragSessionData& dados = controller.GetSessionData();
  content::WebContents* const conteudo =
      dados.source_view_drag_data()->contents.get();
  tabs::TabInterface* aba =
      conteudo ? tabs::TabInterface::MaybeGetFromContents(conteudo) : nullptr;
  tabs::TabInterface* const alvo = escolha.alvo.Get();
  if (!aba || !alvo) {
    return;
  }

  TabDragContext* const anexado =
      const_cast<TabDragContext*>(controller.GetAttachedContext());
  if (!anexado || anexado->GetTabStripModel() != &modelo_.get()) {
    // Veio de outra janela (arrastada numa janela própria): entra nesta,
    // depois da divisão (ou da aba) do alvo.
    std::unique_ptr<tabs::TabModel> solta =
        controller.DetachTabAtForInsertion(dados.source_view_index_);
    if (!solta) {
      return;
    }
    aba = solta.get();
    int indice = modelo_->GetIndexOfTab(alvo) + 1;
    if (alvo->IsSplit()) {
      indice = static_cast<int>(
          modelo_->GetSplitData(*alvo->GetSplit())->GetIndexRange().end());
    }
    modelo_->InsertDetachedTabAt(
        indice, std::move(solta),
        alvo->IsPinned() ? AddTabTypes::ADD_PINNED : AddTabTypes::ADD_NONE,
        alvo->GetGroup());
  }

  // Depois de o arrasto acabar: o TabDragController ainda arruma a lateral
  // (as abas arrastadas) quando esta função volta.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<AlvoDaDivisao> eu, tabs::TabHandle aba,
             tabs::TabHandle alvo, Lado lado) {
            if (!eu || !aba.Get() || !alvo.Get()) {
              return;
            }
            Juntar(&eu->modelo_.get(), aba.Get(), alvo.Get(), lado);
          },
          fabrica_.GetWeakPtr(), aba->GetHandle(), alvo->GetHandle(),
          escolha.lado));
}

base::CallbackListSubscription AlvoDaDivisao::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return ao_destruir_.Add(std::move(callback));
}

void AlvoDaDivisao::Limpar() {
  previa_->Esconder();
  escolha_.reset();
}

void AlvoDaDivisao::DevolverAtivacao() {
  if (!trocou_) {
    return;
  }
  trocou_ = false;
  tabs::TabInterface* const aba = trocou_de_.Get();
  if (aba && modelo_->GetIndexOfTab(aba) != TabStripModel::kNoTab &&
      !aba->IsActivated()) {
    modelo_->ActivateTabAt(modelo_->GetIndexOfTab(aba));
  }
}

}  // namespace navegador::divisao
