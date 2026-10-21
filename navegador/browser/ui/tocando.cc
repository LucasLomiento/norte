#include "navegador/browser/ui/tocando.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "navegador/icones/menu/vector_icons.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace navegador::tocando {

namespace {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionImageType;

constexpr int kCapa = 36;
constexpr int kRaio = 10;
constexpr int kRaioDaCapa = 6;
constexpr int kBotao = 24;
constexpr int kIcone = 12;

// Capa do que toca (a imagem que o site dá pra sessão de mídia) ou, sem ela,
// o ícone do site num quadrado.
class Capa : public views::View {
  METADATA_HEADER(Capa, views::View)

 public:
  Capa() {
    SetPreferredSize(gfx::Size(kCapa, kCapa));
    SetCanProcessEventsWithinSubtree(false);
  }

  void Definir(const gfx::ImageSkia& capa, const gfx::ImageSkia& icone) {
    capa_ = capa;
    icone_ = icone;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::RectF area(GetLocalBounds());
    SkPath recorte = SkPath::RRect(SkRRect::MakeRectXY(
        gfx::RectFToSkRect(area), kRaioDaCapa, kRaioDaCapa));
    canvas->Save();
    canvas->ClipPath(recorte, /*do_anti_alias=*/true);
    if (!capa_.isNull()) {
      // Preenche o quadrado (corta o que sobra, como capa de disco).
      const float escala =
          std::max(static_cast<float>(width()) / capa_.width(),
                   static_cast<float>(height()) / capa_.height());
      const int largura = capa_.width() * escala;
      const int altura = capa_.height() * escala;
      canvas->DrawImageInt(capa_, 0, 0, capa_.width(), capa_.height(),
                           (width() - largura) / 2, (height() - altura) / 2,
                           largura, altura, /*filter=*/true);
    } else {
      cc::PaintFlags fundo;
      fundo.setAntiAlias(true);
      fundo.setColor(SkColorSetA(
          GetColorProvider()->GetColor(kColorTabForegroundActiveFrameActive),
          0x1a));
      canvas->DrawRect(area, fundo);
      if (!icone_.isNull()) {
        canvas->DrawImageInt(icone_, (width() - icone_.width()) / 2,
                             (height() - icone_.height()) / 2);
      }
    }
    canvas->Restore();
  }

 private:
  gfx::ImageSkia capa_;
  gfx::ImageSkia icone_;
};

BEGIN_METADATA(Capa)
END_METADATA

// Andamento da faixa: trilha fina com a parte tocada na cor de destaque.
// Clicar leva até aquele ponto (quando o site deixa).
class Andamento : public views::View {
  METADATA_HEADER(Andamento, views::View)

 public:
  explicit Andamento(base::RepeatingCallback<void(double)> buscar)
      : buscar_(std::move(buscar)) {
    SetPreferredSize(gfx::Size(10, kBotao));
  }

  // Repinta só quando a parte tocada anda um pixel: a fração muda a cada
  // tique, e cada repintura era um quadro da interface (2 por segundo). Sem
  // `pintar` (fora da vista), só guarda: a próxima pintura já sai certa.
  void Definir(std::optional<double> parte, bool pode_buscar,
               bool pintar = true) {
    const int pixels = parte ? static_cast<int>(*parte * width()) : -1;
    parte_ = parte;
    if (!pintar || (pixels == pixels_ && pode_buscar == pode_buscar_)) {
      return;
    }
    pixels_ = pixels;
    pode_buscar_ = pode_buscar;
    SchedulePaint();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (!parte_ || !pode_buscar_ || !event.IsOnlyLeftMouseButton() ||
        width() <= 0) {
      return false;
    }
    const double ponto =
        std::clamp(static_cast<double>(event.x()) / width(), 0.0, 1.0);
    parte_ = ponto;
    SchedulePaint();
    buscar_.Run(ponto);
    return true;
  }

  void OnMouseEntered(const ui::MouseEvent& event) override { SchedulePaint(); }
  void OnMouseExited(const ui::MouseEvent& event) override { SchedulePaint(); }

  void OnPaint(gfx::Canvas* canvas) override {
    if (!parte_) {
      return;
    }
    const ui::ColorProvider* cores = GetColorProvider();
    const float altura = pode_buscar_ && IsMouseHovered() ? 5.0f : 3.0f;
    const gfx::RectF trilha(0, (height() - altura) / 2.0f, width(), altura);
    cc::PaintFlags fundo;
    fundo.setAntiAlias(true);
    fundo.setColor(SkColorSetA(
        cores->GetColor(kColorTabForegroundActiveFrameActive), 0x26));
    canvas->DrawRoundRect(trilha, altura / 2, fundo);
    gfx::RectF feito = trilha;
    feito.set_width(std::max<float>(altura, trilha.width() * *parte_));
    cc::PaintFlags destaque;
    destaque.setAntiAlias(true);
    destaque.setColor(cores->GetColor(ui::kColorAccent));
    canvas->DrawRoundRect(feito, altura / 2, destaque);
  }

 private:
  base::RepeatingCallback<void(double)> buscar_;
  std::optional<double> parte_;
  // Até onde a parte tocada foi pintada, em pixels (-1: sem andamento).
  int pixels_ = -1;
  bool pode_buscar_ = false;
};

BEGIN_METADATA(Andamento)
END_METADATA

class Cartao : public views::Button,
               public media_session::mojom::MediaControllerObserver,
               public media_session::mojom::MediaControllerImageObserver,
               public TabStripModelObserver {
  METADATA_HEADER(Cartao, views::Button)

 public:
  explicit Cartao(BrowserWindowInterface* browser)
      : views::Button(
            base::BindRepeating(&Cartao::IrParaAba, base::Unretained(this))),
        browser_(browser) {
    SetVisible(false);
    SetTooltipText(u"Ir para a aba");
    GetViewAccessibility().SetName(u"Tocando agora");
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 8, 6, 8));
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), kRaio);

    // Em cima a capa e os textos; embaixo o andamento e os botões.
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets::TLBR(8, 8, 4, 6),
        2));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    auto* linha = AddChildView(std::make_unique<views::View>());
    linha->SetCanProcessEventsWithinSubtree(false);
    auto* linha_layout =
        linha->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 9));
    linha_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    capa_ = linha->AddChildView(std::make_unique<Capa>());
    auto* textos = linha->AddChildView(std::make_unique<views::View>());
    textos->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 1));
    linha_layout->SetFlexForView(textos, 1);
    const gfx::FontList base;
    titulo_ = textos->AddChildView(std::make_unique<views::Label>());
    titulo_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    titulo_->SetElideBehavior(gfx::FADE_TAIL);
    titulo_->SetFontList(base.DeriveWithSizeDelta(12 - base.GetFontSize())
                             .DeriveWithWeight(gfx::Font::Weight::SEMIBOLD));
    detalhe_ = textos->AddChildView(std::make_unique<views::Label>());
    detalhe_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    detalhe_->SetElideBehavior(gfx::FADE_TAIL);
    detalhe_->SetFontList(base.DeriveWithSizeDelta(11 - base.GetFontSize()));

    auto* controles = AddChildView(std::make_unique<views::View>());
    auto* controles_layout =
        controles->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 2));
    controles_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    andamento_ = controles->AddChildView(std::make_unique<Andamento>(
        base::BindRepeating(&Cartao::Buscar, base::Unretained(this))));
    andamento_->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 6));
    controles_layout->SetFlexForView(andamento_, 1);
    tocar_ = controles->AddChildView(CriarBotao(
        base::BindRepeating(&Cartao::TocarOuPausar, base::Unretained(this))));
    proxima_ = controles->AddChildView(CriarBotao(
        base::BindRepeating(&Cartao::Proxima, base::Unretained(this))));
    proxima_->SetTooltipText(u"Próxima");
    proxima_->GetViewAccessibility().SetName(u"Próxima");

    // A sessão de mídia ativa (a última que pegou o foco de áudio).
    mojo::Remote<media_session::mojom::MediaControllerManager> gerente;
    content::GetMediaSessionService().BindMediaControllerManager(
        gerente.BindNewPipeAndPassReceiver());
    gerente->CreateActiveMediaController(
        controlador_.BindNewPipeAndPassReceiver());
    controlador_->AddObserver(observador_.BindNewPipeAndPassRemote());
    controlador_->ObserveImages(MediaSessionImageType::kArtwork, kCapa,
                                kCapa * 3,
                                observador_de_capa_.BindNewPipeAndPassRemote());

    browser_->GetTabStripModel()->AddObserver(this);
  }

  Cartao(const Cartao&) = delete;
  Cartao& operator=(const Cartao&) = delete;
  ~Cartao() override = default;

  // views::Button:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    const ui::ColorProvider* cores = GetColorProvider();
    const SkColor texto = cores->GetColor(kColorTabForegroundActiveFrameActive);
    const bool em_cima =
        GetState() == STATE_HOVERED || GetState() == STATE_PRESSED;
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(texto, em_cima ? 0x24 : 0x14));
    const gfx::RectF area(GetLocalBounds());
    canvas->DrawRoundRect(area, kRaio, flags);
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    const ui::ColorProvider* cores = GetColorProvider();
    titulo_->SetEnabledColor(
        cores->GetColor(kColorTabForegroundActiveFrameActive));
    detalhe_->SetEnabledColor(
        cores->GetColor(kColorTabForegroundInactiveFrameActive));
    for (views::ImageButton* botao : {tocar_.get(), proxima_.get()}) {
      views::InkDrop::Get(botao)->SetBaseColor(
          cores->GetColor(kColorTabForegroundActiveFrameActive));
    }
    AtualizarIcones();
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override {
    info_ = std::move(info);
    Atualizar();
  }

  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override {
    metadados_ = metadata;
    Atualizar();
  }

  void MediaSessionActionsChanged(
      const std::vector<MediaSessionAction>& actions) override {
    tem_proxima_ = std::ranges::find(actions, MediaSessionAction::kNextTrack) !=
                   actions.end();
    pode_buscar_ = std::ranges::find(actions, MediaSessionAction::kSeekTo) !=
                   actions.end();
    Atualizar();
  }

  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {
    sessao_ = request_id;
    // A sessão nova manda os dados dela logo depois.
    metadados_.reset();
    posicao_.reset();
    capa_da_sessao_ = gfx::ImageSkia();
    Atualizar();
  }

  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {
    posicao_ = position;
    Atualizar();
  }

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(MediaSessionImageType type,
                                   const SkBitmap& bitmap) override {
    if (type != MediaSessionImageType::kArtwork) {
      return;
    }
    capa_da_sessao_ = bitmap.drawsNothing()
                          ? gfx::ImageSkia()
                          : gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    Atualizar();
  }

  void MediaControllerChapterImageChanged(int chapter_index,
                                          const SkBitmap& bitmap) override {}

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    Atualizar();
  }

  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override {
    // Título e ícone da aba (usados quando o site não dá metadados).
    if (tab && tab->GetContents() == WebContentsDaSessao()) {
      Atualizar();
    }
  }

 private:
  std::unique_ptr<views::ImageButton> CriarBotao(
      views::Button::PressedCallback apertou) {
    auto botao = std::make_unique<views::ImageButton>(std::move(apertou));
    botao->SetPreferredSize(gfx::Size(kBotao, kBotao));
    botao->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    botao->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    views::InkDrop::Get(botao.get())
        ->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InstallCircleHighlightPathGenerator(botao.get());
    return botao;
  }

  void AtualizarIcones() {
    if (!GetColorProvider()) {
      return;
    }
    const bool tocando =
        info_ && info_->playback_state == MediaPlaybackState::kPlaying;
    tocar_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            tocando ? icones_menu::kPausarIcon : icones_menu::kTocarIcon,
            kColorTabForegroundActiveFrameActive, kIcone));
    tocar_->SetTooltipText(tocando ? u"Pausar" : u"Tocar");
    tocar_->GetViewAccessibility().SetName(tocando ? u"Pausar" : u"Tocar");
    proxima_->SetImageModel(views::Button::STATE_NORMAL,
                            ui::ImageModel::FromVectorIcon(
                                icones_menu::kProximaIcon,
                                kColorTabForegroundActiveFrameActive, kIcone));
  }

  content::WebContents* WebContentsDaSessao() const {
    return sessao_
               ? content::MediaSession::GetWebContentsFromRequestId(*sessao_)
               : nullptr;
  }

  // A aba da sessão, se ela é desta janela.
  tabs::TabInterface* AbaDaSessao() const {
    content::WebContents* contents = WebContentsDaSessao();
    tabs::TabInterface* aba =
        contents ? tabs::TabInterface::MaybeGetFromContents(contents) : nullptr;
    return aba && aba->GetBrowserWindowInterface() == browser_ ? aba : nullptr;
  }

  // A aba já está na frente (ativa, ou do lado da ativa na tela dividida).
  bool AVista(const tabs::TabInterface* aba) const {
    const tabs::TabInterface* ativa =
        browser_->GetTabStripModel()->GetActiveTab();
    if (aba == ativa) {
      return true;
    }
    return ativa && aba->GetSplit().has_value() &&
           aba->GetSplit() == ativa->GetSplit();
  }

  std::optional<double> ParteTocada() const {
    if (!posicao_) {
      return std::nullopt;
    }
    const base::TimeDelta duracao = posicao_->duration();
    if (duracao.is_inf() || duracao <= base::TimeDelta()) {
      return std::nullopt;
    }
    return std::clamp(posicao_->GetPosition() / duracao, 0.0, 1.0);
  }

  void Atualizar() {
    tabs::TabInterface* aba = AbaDaSessao();
    const bool mostrar =
        aba && info_ && info_->is_controllable &&
        info_->state !=
            media_session::mojom::MediaSessionInfo::SessionState::kInactive &&
        !AVista(aba);
    if (mostrar) {
      TabUIHelper* ajudante = TabUIHelper::From(aba);
      std::u16string titulo;
      std::u16string detalhe;
      if (metadados_) {
        titulo = metadados_->title;
        detalhe = !metadados_->artist.empty() ? metadados_->artist
                                              : metadados_->source_title;
      }
      if (titulo.empty() && ajudante) {
        titulo = ajudante->GetTitle();
      }
      if (detalhe.empty()) {
        detalhe =
            base::UTF8ToUTF16(aba->GetContents()->GetLastCommittedURL().host());
      }
      titulo_->SetText(titulo);
      detalhe_->SetText(detalhe);
      gfx::ImageSkia icone;
      if (ajudante && GetColorProvider()) {
        icone = ajudante->GetFavicon().Rasterize(GetColorProvider());
      }
      capa_->Definir(capa_da_sessao_, icone);
      proxima_->SetVisible(tem_proxima_);
      AtualizarIcones();
    }

    const bool tocando =
        mostrar && info_->playback_state == MediaPlaybackState::kPlaying;
    if (tocando && ParteTocada() && !relogio_.IsRunning()) {
      relogio_.Start(FROM_HERE, IntervaloDoAndamento(),
                     base::BindRepeating(&Cartao::AtualizarAndamento,
                                         base::Unretained(this)));
    } else if (!tocando || !ParteTocada()) {
      relogio_.Stop();
    }
    AtualizarAndamento();

    if (mostrar != GetVisible()) {
      SetVisible(mostrar);
      PreferredSizeChanged();
    }
    SchedulePaint();
  }

  void IrParaAba() {
    tabs::TabInterface* aba = AbaDaSessao();
    if (!aba) {
      return;
    }
    TabStripModel* modelo = browser_->GetTabStripModel();
    const int indice = modelo->GetIndexOfTab(aba);
    if (indice != TabStripModel::kNoTab) {
      modelo->ActivateTabAt(indice);
    }
  }

  void TocarOuPausar() {
    if (info_ && info_->playback_state == MediaPlaybackState::kPlaying) {
      controlador_->Suspend();
    } else {
      controlador_->Resume();
    }
  }

  void Proxima() { controlador_->NextTrack(); }

  // O tempo pra linha andar um pixel (entre 0,5 e 10 s): numa música de 3
  // min, ~1 s; num vídeo de 1 h, 10 s.
  base::TimeDelta IntervaloDoAndamento() const {
    if (!posicao_ || andamento_->width() <= 0) {
      return base::Milliseconds(500);
    }
    return std::clamp(posicao_->duration() / andamento_->width(),
                      base::Milliseconds(500), base::Seconds(10));
  }

  // Fora da vista (lateral recolhida ou escondida, janela minimizada), o
  // tique não pinta.
  bool AVista() const {
    return GetWidget() && !GetWidget()->IsMinimized() && IsDrawn() &&
           !GetVisibleBounds().IsEmpty() &&
           GetVisibleBounds().width() >= width() / 2;
  }

  void AtualizarAndamento() {
    andamento_->Definir(ParteTocada(), pode_buscar_, AVista());
  }

  void Buscar(double parte) {
    if (posicao_ && pode_buscar_) {
      controlador_->SeekTo(posicao_->duration() * parte);
    }
  }

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<Capa> capa_ = nullptr;
  raw_ptr<views::Label> titulo_ = nullptr;
  raw_ptr<views::Label> detalhe_ = nullptr;
  raw_ptr<Andamento> andamento_ = nullptr;
  raw_ptr<views::ImageButton> tocar_ = nullptr;
  raw_ptr<views::ImageButton> proxima_ = nullptr;

  mojo::Remote<media_session::mojom::MediaController> controlador_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver> observador_{
      this};
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      observador_de_capa_{this};

  std::optional<base::UnguessableToken> sessao_;
  media_session::mojom::MediaSessionInfoPtr info_;
  std::optional<media_session::MediaMetadata> metadados_;
  std::optional<media_session::MediaPosition> posicao_;
  gfx::ImageSkia capa_da_sessao_;
  bool tem_proxima_ = false;
  bool pode_buscar_ = false;
  base::RepeatingTimer relogio_;
};

BEGIN_METADATA(Cartao)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CriarCartao(BrowserWindowInterface* browser) {
  return std::make_unique<Cartao>(browser);
}

}  // namespace navegador::tocando
