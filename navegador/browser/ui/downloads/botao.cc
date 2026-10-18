#include "navegador/browser/ui/downloads/botao.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#include "navegador/browser/ui/downloads/ponte.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"

namespace navegador::downloads {

namespace {

constexpr int kTamanho = 28;
constexpr int kIcone = 16;
constexpr float kRaioDoAnel = 11.5f;

class BotaoDeDownloads;

std::map<BrowserWindowInterface*, BotaoDeDownloads*>& Botoes() {
  static base::NoDestructor<std::map<BrowserWindowInterface*,
                                     BotaoDeDownloads*>>
      botoes;
  return *botoes;
}

class BotaoDeDownloads : public views::Button {
  METADATA_HEADER(BotaoDeDownloads, views::Button)

 public:
  explicit BotaoDeDownloads(BrowserWindowInterface* browser)
      : views::Button(base::BindRepeating(&BotaoDeDownloads::Abrir,
                                          base::Unretained(this))),
        browser_(browser) {
    SetPreferredSize(gfx::Size(kTamanho, kTamanho));
    SetTooltipText(u"Downloads");
    GetViewAccessibility().SetName(u"Downloads");
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InkDrop::Get(this)->SetBaseColor(
        kColorTabForegroundInactiveFrameActive);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), 6);
    Botoes()[browser_] = this;
  }

  ~BotaoDeDownloads() override {
    auto it = Botoes().find(browser_);
    if (it != Botoes().end() && it->second == this) {
      Botoes().erase(it);
    }
  }

  // À vista de verdade (com a lateral escondida, ele existe mas fica fora da
  // parte visível dela).
  bool AVista() const {
    return GetWidget() && IsDrawn() && !GetVisibleBounds().IsEmpty() &&
           GetVisibleBounds().width() >= width() / 2;
  }

  void Atualizar(const Progresso& progresso) {
    progresso_ = progresso;
    const bool girando =
        progresso_.quantos > 0 && !progresso_.tamanho_conhecido;
    if (girando && !giro_.IsRunning()) {
      giro_.Start(FROM_HERE, base::Milliseconds(30),
                  base::BindRepeating(&BotaoDeDownloads::Girar,
                                      base::Unretained(this)));
    } else if (!girando) {
      giro_.Stop();
    }
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorProvider* cores = GetColorProvider();
    const SkColor cor = cores->GetColor(kColorTabForegroundInactiveFrameActive);
    const gfx::PointF centro(width() / 2.0f, height() / 2.0f);
    const gfx::ImageSkia icone = gfx::CreateVectorIcon(
        kDownloadIcon, progresso_.quantos > 0 ? kIcone - 4 : kIcone, cor);
    canvas->DrawImageInt(icone, (width() - icone.width()) / 2,
                         (height() - icone.height()) / 2);

    if (progresso_.quantos > 0) {
      const SkColor destaque = cores->GetColor(ui::kColorAccent);
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(2);
      flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
      flags.setColor(SkColorSetA(cor, 0x40));
      canvas->DrawCircle(centro, kRaioDoAnel, flags);
      const SkRect caixa = gfx::RectFToSkRect(
          gfx::RectF(centro.x() - kRaioDoAnel, centro.y() - kRaioDoAnel,
                     2 * kRaioDoAnel, 2 * kRaioDoAnel));
      SkPathBuilder arco;
      if (progresso_.tamanho_conhecido) {
        arco.addArc(caixa, -90,
                    360.0f * std::max(progresso_.porcentagem, 3) / 100);
      } else {
        arco.addArc(caixa, angulo_, 90);
      }
      flags.setColor(destaque);
      canvas->DrawPath(arco.detach(), flags);
    } else if (progresso_.concluido_novo) {
      // Terminou e ninguém abriu ainda: um ponto no canto, como no Dia.
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(cores->GetColor(ui::kColorAccent));
      canvas->DrawCircle(gfx::PointF(width() - 7.0f, 7.0f), 3, flags);
    }
  }

 private:
  void Abrir() {
    if (auto* controle = DownloadToolbarUIController::From(browser_)) {
      controle->InvokeUI();
    } else {
      chrome::ShowDownloads(browser_);
    }
  }

  void Girar() {
    angulo_ = (angulo_ + 12) % 360;
    // Fora de vista (lateral escondida, janela minimizada) não repinta: o
    // relógio é de 30 ms e cada pintura recompõe a janela.
    if (AVista() && GetWidget()->IsVisible() && !GetWidget()->IsMinimized()) {
      SchedulePaint();
    }
  }

  raw_ptr<BrowserWindowInterface> browser_;
  Progresso progresso_;
  int angulo_ = 0;
  base::RepeatingTimer giro_;
};

BEGIN_METADATA(BotaoDeDownloads)
END_METADATA

void Ligar() {
  static bool ligado = false;
  if (ligado) {
    return;
  }
  ligado = true;
  DefinirAncora(base::BindRepeating([](BrowserWindowInterface* browser) {
    auto it = Botoes().find(browser);
    return it != Botoes().end() && it->second->AVista()
               ? static_cast<views::View*>(it->second)
               : nullptr;
  }));
  DefinirAvisoDeProgresso(base::BindRepeating(
      [](BrowserWindowInterface* browser, const Progresso& progresso) {
        if (auto it = Botoes().find(browser); it != Botoes().end()) {
          it->second->Atualizar(progresso);
        }
      }));
}

}  // namespace

std::unique_ptr<views::View> CriarBotao(BrowserWindowInterface* browser) {
  Ligar();
  return std::make_unique<BotaoDeDownloads>(browser);
}

}  // namespace navegador::downloads
