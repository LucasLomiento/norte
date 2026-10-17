#include "navegador/browser/ui/abas/abas.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"

namespace navegador::abas {

namespace {

// O campo do "renomear no lugar". Some sozinho ao terminar.
class EditorNoLugar : public views::Textfield,
                      public views::TextfieldController {
  METADATA_HEADER(EditorNoLugar, views::Textfield)

 public:
  EditorNoLugar(views::Label* rotulo,
                std::u16string_view inicial,
                base::OnceCallback<void(std::u16string)> pronto)
      : rotulo_(rotulo),
        pronto_(std::move(pronto)),
        inicial_(base::TrimWhitespace(inicial, base::TRIM_ALL)) {
    observacao_.Observe(rotulo);
    SetFontList(rotulo->font_list());
    set_controller(this);
    SetText(std::u16string(inicial));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 5)));
    GetViewAccessibility().SetName(u"Nome");
    // Os layouts das linhas (abas, fixados, pastas) não mexem nele.
    SetProperty(views::kViewIgnoredByLayoutKey, true);
  }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& evento) override {
    if (evento.type() != ui::EventType::kKeyPressed) {
      return false;
    }
    if (evento.key_code() == ui::VKEY_RETURN) {
      Terminar(/*gravar=*/true);
      return true;
    }
    if (evento.key_code() == ui::VKEY_ESCAPE) {
      Terminar(/*gravar=*/false);
      return true;
    }
    return false;
  }

  // Do começo do rótulo (um pouco antes, pelo recuo do texto) até o fim da
  // linha, cobrindo o "x". A linha pode ainda não ter tamanho (acabou de ser
  // refeita): aí espera o rótulo ganhar o lugar dele.
  void Posicionar() {
    views::View* dono = parent();
    if (!dono || !rotulo_ || dono->height() <= 4 || rotulo_->width() <= 0) {
      return;
    }
    gfx::Rect area = rotulo_->bounds();
    if (rotulo_->parent() != dono) {
      area = views::View::ConvertRectToTarget(rotulo_->parent(), dono, area);
    }
    const int altura = std::min(dono->height() - 4, 26);
    const int x = std::max(0, area.x() - 5);
    SetBounds(x, (dono->height() - altura) / 2,
              std::max(area.width() + 10, dono->width() - x - 4), altura);
  }


  // views::Textfield:
  void OnBlur() override {
    views::Textfield::OnBlur();
    Terminar(/*gravar=*/true);
  }

 private:
  // Fora do evento: gravar pode refazer a linha (e apagar este campo junto).
  void Terminar(bool gravar) {
    if (terminado_) {
      return;
    }
    terminado_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<EditorNoLugar> editor, bool gravar) {
              if (!editor || !editor->parent()) {
                return;
              }
              auto pronto = std::move(editor->pronto_);
              std::u16string texto(
                  base::TrimWhitespace(editor->GetText(), base::TRIM_ALL));
              // Sem mudança não grava: o nome da página continua valendo
              // (um clique duplo sem querer congelava o título da aba).
              gravar = gravar && texto != editor->inicial_;
              editor->parent()->RemoveChildViewT(editor.get());
              if (gravar && pronto) {
                std::move(pronto).Run(std::move(texto));
              }
            },
            weak_factory_.GetWeakPtr(), gravar));
  }

  // Segue o rótulo (o Textfield já é um ViewObserver, daí a classe à parte).
  class Observador : public views::ViewObserver {
   public:
    explicit Observador(EditorNoLugar* editor) : editor_(editor) {}
    void OnViewBoundsChanged(views::View* view) override {
      editor_->Posicionar();
    }
    void OnViewIsDeleting(views::View* view) override {
      editor_->observacao_.Reset();
      editor_->rotulo_ = nullptr;
    }

   private:
    raw_ptr<EditorNoLugar> editor_;
  };

  raw_ptr<views::View> rotulo_;
  Observador observador_{this};
  base::ScopedObservation<views::View, views::ViewObserver> observacao_{
      &observador_};
  base::OnceCallback<void(std::u16string)> pronto_;
  const std::u16string inicial_;
  bool terminado_ = false;
  base::WeakPtrFactory<EditorNoLugar> weak_factory_{this};
};

BEGIN_METADATA(EditorNoLugar)
END_METADATA

Renomeador& PonteRenomear() {
  static base::NoDestructor<Renomeador> ponte;
  return *ponte;
}

}  // namespace

void EditarNoLugar(views::View* dono,
                   views::Label& rotulo,
                   std::u16string_view inicial,
                   base::OnceCallback<void(std::u16string)> pronto) {
  if (!dono || !dono->GetWidget() || Editando(dono)) {
    return;
  }
  auto* campo = dono->AddChildView(
      std::make_unique<EditorNoLugar>(&rotulo, inicial, std::move(pronto)));
  campo->Posicionar();
  campo->SelectAll(/*reversed=*/false);
  campo->RequestFocus();
}

bool Editando(const views::View* dono) {
  if (!dono) {
    return false;
  }
  for (const views::View* filho : dono->children()) {
    if (views::IsViewClass<EditorNoLugar>(filho)) {
      return true;
    }
  }
  return false;
}

void DefinirRenomeador(Renomeador renomeador) {
  PonteRenomear() = std::move(renomeador);
}

void RenomearAba(tabs::TabInterface* aba, const std::u16string& nome) {
  if (aba && !PonteRenomear().is_null()) {
    PonteRenomear().Run(aba, nome);
  }
}

void PintarLadrilho(gfx::Canvas* canvas, const views::View& aba, bool fixada) {
  const ui::ColorProvider* cores = aba.GetColorProvider();
  if (!fixada || !cores) {
    return;
  }
  // Claro: branco translúcido (mais claro que a lateral). Escuro: um véu de
  // branco bem leve.
  const bool escuro =
      color_utils::IsDark(cores->GetColor(ui::kColorFrameActive));
  const SkColor fundo = escuro ? SkColorSetARGB(0x10, 0xFF, 0xFF, 0xFF)
                               : SkColorSetARGB(0x99, 0xFF, 0xFF, 0xFF);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(fundo);
  canvas->DrawRoundRect(
      gfx::RectF(aba.GetLocalBounds()),
      GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius), flags);
}

}  // namespace navegador::abas
