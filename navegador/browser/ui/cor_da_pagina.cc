#include "navegador/browser/ui/cor_da_pagina.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_service.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "navegador/browser/containers/espacos_dados.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/widget/widget.h"

namespace navegador::cor_da_pagina {

namespace {

std::map<const BrowserView*, std::optional<SkColor>>& Cores() {
  static base::NoDestructor<std::map<const BrowserView*, std::optional<SkColor>>>
      cores;
  return *cores;
}

// Cor dominante da faixa: o "balde" de cor (5 bits por canal) com mais
// pixels, se ele tiver pelo menos 40% da faixa; senão, a média de tudo.
std::optional<SkColor> CorDominante(const SkBitmap& bitmap) {
  std::map<uint32_t, int> baldes;
  std::map<uint32_t, std::array<uint64_t, 3>> somas;
  uint64_t r = 0, g = 0, b = 0;
  int total = 0;
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor c = bitmap.getColor(x, y);
      if (SkColorGetA(c) < 0x80) {
        continue;
      }
      const uint32_t balde = ((SkColorGetR(c) >> 3) << 10) |
                             ((SkColorGetG(c) >> 3) << 5) | (SkColorGetB(c) >> 3);
      ++baldes[balde];
      auto& soma = somas[balde];
      soma[0] += SkColorGetR(c);
      soma[1] += SkColorGetG(c);
      soma[2] += SkColorGetB(c);
      r += SkColorGetR(c);
      g += SkColorGetG(c);
      b += SkColorGetB(c);
      ++total;
    }
  }
  if (total == 0) {
    return std::nullopt;
  }
  const auto maior = std::max_element(
      baldes.begin(), baldes.end(),
      [](const auto& a, const auto& b) { return a.second < b.second; });
  if (maior->second * 10 >= total * 4) {
    const auto& soma = somas[maior->first];
    const int n = maior->second;
    return SkColorSetRGB(soma[0] / n, soma[1] / n, soma[2] / n);
  }
  return SkColorSetRGB(r / total, g / total, b / total);
}

// Diferença visível entre duas cores (evita recalcular o tema por ruído).
bool Diferentes(std::optional<SkColor> a, std::optional<SkColor> b) {
  if (a.has_value() != b.has_value()) {
    return true;
  }
  if (!a) {
    return false;
  }
  const auto canal = [](SkColor c, int deslocamento) {
    return static_cast<int>((c >> deslocamento) & 0xFF);
  };
  int d = 0;
  for (int deslocamento : {16, 8, 0}) {
    d += std::abs(canal(*a, deslocamento) - canal(*b, deslocamento));
  }
  return d > 6;
}

// 5 bits por canal: tons quase iguais viram a mesma cor (e reaproveitam o
// mesmo fornecedor de cores, ver LimparFornecedoresSePreciso).
SkColor Arredondar(SkColor cor) {
  const auto canal = [](U8CPU v) { return std::min<U8CPU>(255, (v + 4) & ~7u); };
  return SkColorSetRGB(canal(SkColorGetR(cor)), canal(SkColorGetG(cor)),
                       canal(SkColorGetB(cor)));
}

// Cada cor da barra vira uma chave de cores da janela, e o Chromium guarda
// um ColorProvider inteiro (milhares de cores) por chave, sem limite, até o
// tema mudar: e a chave leva também a cor do espaço e o modo (cada aba pede
// o claro e o escuro). Visitando muitos sites, isso só crescia. Passando de
// kMaximoDeFornecedores criados desde a última limpeza, limpa o cache: as
// janelas pedem o fornecedor de novo na próxima pintura
// (Widget::GetColorProvider não guarda ponteiro), e as cores continuam as
// mesmas.
constexpr size_t kMaximoDeFornecedores = 64;

void LimparFornecedoresSePreciso() {
  static size_t na_ultima_limpeza = 0;
  ui::ColorProviderManager& gerente = ui::ColorProviderManager::Get();
  if (gerente.num_providers_initialized() - na_ultima_limpeza <=
      kMaximoDeFornecedores) {
    return;
  }
  gerente.ResetColorProviderCache();
  na_ultima_limpeza = gerente.num_providers_initialized();
}

// Invisível, fora do layout: só existe pra viver junto com a janela.
// Como o Dia (ColorSampleControllerV2 no código dele): amostra os pixels do
// topo da página aberta e usa a cor dominante na barra de navegação.
class Observador : public views::View,
                   public TabStripModelObserver,
                   public content::WebContentsObserver {
  METADATA_HEADER(Observador, views::View)

 public:
  explicit Observador(BrowserView* browser_view)
      : browser_view_(browser_view),
        model_(browser_view->browser()->GetTabStripModel()) {
    SetVisible(false);
    SetProperty(views::kViewIgnoredByLayoutKey, true);
    model_->AddObserver(this);
    Observe(model_->GetActiveWebContents());
  }

  ~Observador() override { Cores().erase(browser_view_); }

  // views::View:
  void AddedToWidget() override {
    Amostrar();
    Agendar();
  }
  void RemovedFromWidget() override {
    relogio_.Stop();
    aviso_.Stop();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (selection.active_tab_changed()) {
      Observe(model_->GetActiveWebContents());
      ++geracao_;
      Acordar();
    }
  }

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override { Acordar(); }
  void DidStopLoading() override { Acordar(); }
  // A cor de fundo e o theme-color podem mudar a cada quadro (transição ou
  // animação do fundo do body): cada aviso era uma cópia da tela e um
  // ThemeChanged da janela inteira. Espera os avisos pararem.
  void DidChangeThemeColor() override { AcordarDepois(); }
  void OnBackgroundColorChanged() override { AcordarDepois(); }
  // O vídeo parou (pausa, fim): a página volta a poder ser amostrada.
  void MediaStoppedPlaying(const MediaPlayerInfo& info,
                           const content::MediaPlayerId& id,
                           MediaStoppedReason reason) override {
    if (info.has_video) {
      Acordar();
    }
  }
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    if (!entered_fullscreen) {
      Acordar();
    }
  }

 private:
  bool Ligada() const {
    return browser_view_->browser()->GetProfile()->GetPrefs()->GetBoolean(
        espacos::kBarraNaCorDoSitePref);
  }

  // A página muda sem avisar (rolar, cabeçalho fixo): amostra de novo de
  // tempos em tempos. Cada amostra pede uma cópia da tela ao compositor
  // (que recompõe a janela): quando a cor não muda, o intervalo cresce até
  // 6 s; algo acontecendo na aba volta pra 1,5 s. Desligada (Configurações
  // → Aparência), não amostra nada.
  void Agendar() {
    // Desligada, só confere de vez em quando se foi ligada (sem cópia).
    relogio_.Start(FROM_HERE, Ligada() ? intervalo_ : kIntervaloMaximo,
                   base::BindOnce(&Observador::Tique, base::Unretained(this)));
  }

  // Tela cheia sem a barra: a de um site, ou a da janela com a opção "barra
  // e abas na tela cheia" desligada (ligada, a barra continua à vista e
  // precisa seguir a página).
  bool TelaCheia() {
    content::WebContents* contents = web_contents();
    if (contents && contents->IsFullscreen()) {
      return true;
    }
    return browser_view_->IsFullscreen() &&
           !browser_view_->browser()->GetProfile()->GetPrefs()->GetBoolean(
               espacos::kBarraNaTelaCheiaPref);
  }

  // Cada cópia passa um quadro da página pelo compositor de novo: com vídeo
  // tocando, isso perdia quadros do vídeo (travadinhas a cada amostra,
  // medido no Xvfb: 4 a 6 quadros perdidos em 20 s, nenhum sem a cópia).
  // Enquanto toca, a barra fica com a cor que já tem.
  bool TocandoVideo() {
    content::WebContents* contents = web_contents();
    return contents && contents->GetCurrentlyPlayingVideoCount() > 0;
  }

  void Tique() {
    // Janela de fundo: a barra dela quase não aparece; espera.
    if (Ligada() && GetWidget() && GetWidget()->IsActive() && !TelaCheia() &&
        !TocandoVideo()) {
      Amostrar(/*periodica=*/true);
    }
    intervalo_ = std::min(intervalo_ * 2, kIntervaloMaximo);
    Agendar();
  }

  // Algo mudou na aba (troca, carregamento, cor do tema): uma amostra na
  // hora, mesmo com vídeo tocando (é um quadro só, junto da mudança).
  void Acordar() {
    intervalo_ = kIntervaloMinimo;
    if (!TelaCheia()) {
      Amostrar();
    }
    Agendar();
  }

  void AcordarDepois() {
    // Reinicia a cada aviso: só amostra quando a página para de mudar.
    aviso_.Start(FROM_HERE, kEsperaDoAviso,
                 base::BindOnce(&Observador::Acordar, base::Unretained(this)));
  }

  // Copia uma faixa de 6 de altura no topo da página, reduzida pra 64x2.
  // `periodica`: veio do relógio, não de uma mudança na aba (ver AoAmostrar).
  void Amostrar(bool periodica = false) {
    if (!Ligada()) {
      Definir(std::nullopt);
      return;
    }
    content::WebContents* contents = web_contents();
    content::RenderWidgetHostView* view =
        contents ? contents->GetRenderWidgetHostView() : nullptr;
    if (!view || !view->IsSurfaceAvailableForCopy()) {
      std::optional<SkColor> reserva = Reserva();
      Definir(reserva ? std::optional<SkColor>(Arredondar(*reserva))
                      : std::nullopt);
      return;
    }
    const gfx::Size tamanho = view->GetVisibleViewportSize();
    if (tamanho.IsEmpty()) {
      return;
    }
    view->CopyFromSurface(
        gfx::Rect(0, 0, tamanho.width(), std::min(6, tamanho.height())),
        gfx::Size(64, 2), base::Seconds(1),
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&Observador::AoAmostrar,
                           weak_factory_.GetWeakPtr(), geracao_, periodica)));
  }

  void AoAmostrar(int geracao,
                  bool periodica,
                  const content::CopyFromSurfaceResult& resultado) {
    if (geracao != geracao_) {
      return;  // Trocou de aba no meio.
    }
    std::optional<SkColor> cor;
    if (resultado.has_value() && !resultado->bitmap.drawsNothing()) {
      cor = CorDominante(resultado->bitmap);
    }
    if (!cor) {
      cor = Reserva();
    }
    if (cor) {
      cor = Arredondar(*cor);
    }
    // Cada cor nova recolore a janela inteira. Numa amostra do relógio (a
    // página mudou sozinha: carrossel, cabeçalho animado), só troca quando
    // duas seguidas concordam; em troca de aba ou carregamento, na hora.
    if (periodica) {
      auto atual = Cores().find(browser_view_);
      if (atual != Cores().end() && Diferentes(atual->second, cor) &&
          (!candidata_ || Diferentes(*candidata_, cor))) {
        candidata_ = cor;
        intervalo_ = kIntervaloMinimo;
        Agendar();
        return;
      }
    }
    candidata_.reset();
    // Mudou: a página está se mexendo, amostra mais vezes.
    auto it = Cores().find(browser_view_);
    if (it == Cores().end() || Diferentes(it->second, cor)) {
      intervalo_ = kIntervaloMinimo;
      Agendar();
    }
    Definir(cor);
  }

  // Sem pixels (aba ainda não pintou): theme-color do site, ou o fundo.
  std::optional<SkColor> Reserva() {
    content::WebContents* contents = web_contents();
    if (!contents) {
      return std::nullopt;
    }
    std::optional<SkColor> cor = contents->GetThemeColor();
    if (!cor) {
      cor = contents->GetBackgroundColor();
    }
    if (cor && SkColorGetA(*cor) < 0x80) {
      cor.reset();  // Transparente não é cor de página.
    }
    return cor;
  }

  void Definir(std::optional<SkColor> cor) {
    auto& cores = Cores();
    auto it = cores.find(browser_view_);
    if (it != cores.end() && !Diferentes(it->second, cor)) {
      return;
    }
    cores[browser_view_] = cor;
    LimparFornecedoresSePreciso();
    if (views::Widget* widget = GetWidget()) {
      widget->ThemeChanged();
    }
  }

  raw_ptr<BrowserView> browser_view_;
  raw_ptr<TabStripModel> model_;
  int geracao_ = 0;
  // Cor vista numa amostra do relógio, esperando a próxima confirmar.
  std::optional<std::optional<SkColor>> candidata_;
  static constexpr base::TimeDelta kIntervaloMinimo = base::Milliseconds(1500);
  static constexpr base::TimeDelta kIntervaloMaximo = base::Seconds(6);
  base::TimeDelta intervalo_ = kIntervaloMinimo;
  base::OneShotTimer relogio_;
  static constexpr base::TimeDelta kEsperaDoAviso = base::Milliseconds(250);
  base::OneShotTimer aviso_;
  base::WeakPtrFactory<Observador> weak_factory_{this};
};

BEGIN_METADATA(Observador)
END_METADATA

}  // namespace

std::optional<SkColor> CorDaBarra(const BrowserView* browser_view) {
  // Desligado nas Configurações do Navegador (Aparência): barra neutra.
  if (browser_view && browser_view->browser() &&
      !browser_view->browser()->GetProfile()->GetPrefs()->GetBoolean(
          navegador::espacos::kBarraNaCorDoSitePref)) {
    return std::nullopt;
  }
  auto& cores = Cores();
  auto it = cores.find(browser_view);
  return it == cores.end() ? std::nullopt : it->second;
}

void Acompanhar(views::View* dono, BrowserView* browser_view) {
  dono->AddChildView(std::make_unique<Observador>(browser_view));
}

}  // namespace navegador::cor_da_pagina
