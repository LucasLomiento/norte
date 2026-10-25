// Arrastar uma aba da lateral pra cima da página (como no Zen): a aba não
// vira janela solta; a página mostra onde ela vai entrar (a metade do painel
// do lado mais perto do ponteiro) e, ao soltar, divide a tela com a página
// que estava à vista antes do clique (o clique já ativa a aba arrastada, e a
// de antes volta pra tela enquanto a aba passa por cima dela).
//
// O MultiContentsView tem uma PreviaDeSoltar (por cima das páginas, sem
// eventos), um AlvoDaDivisao, que o BrowserView dá pro TabDragController
// como alvo quando o ponteiro está em cima das páginas, e uma AlcaDoPainel
// no alto de cada painel da divisão.
//
// Desempenho: nada aqui pinta a cada movimento do mouse. A prévia só muda
// quando o lugar de soltar muda (de painel ou de lado), e aí desliza um
// cartão do tamanho da metade; o escurecido do painel arrastado é uma camada
// de cor (a opacidade anima no compositor). No Hyprland com a janela de
// vidro, pintar a janela inteira a cada movimento obrigava o compositor a
// refazer o desfoque da janela toda.
//
// Arrasto do sistema (Wayland sem xdg-toplevel-drag, como o Hyprland): saindo
// da janela, a aba vira um arrastar-e-soltar do sistema, e em cima da página
// quem recebe os eventos é a página. O AlvoDaDivisao lê a posição no cliente
// de arrastar da janela (aura::client::DragDropClient, relativa à janela
// inteira: certa pra qualquer painel) e avisa o TabDragController, como a
// lateral avisa; e, saindo da janela por cima da página, larga a prévia (senão
// soltar fora dela dividia a tela).
//
// Compilado dentro do //chrome/browser/ui.

#ifndef NAVEGADOR_BROWSER_DIVISAO_SOLTAR_H_
#define NAVEGADOR_BROWSER_DIVISAO_SOLTAR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "components/tabs/public/tab_interface.h"
#include "navegador/browser/divisao/arvore.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"

class MultiContentsView;
class TabStripModel;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
}  // namespace views

namespace navegador::divisao {

// Id (views::View::SetID) da camada: o layout do MultiContentsView estica ela
// por cima das páginas, como a prévia de link.
inline constexpr int kIdSoltar = 0x6E73;

class PreviaDeSoltar : public views::View, public gfx::AnimationDelegate {
  METADATA_HEADER(PreviaDeSoltar, views::View)

 public:
  enum class Modo {
    // A aba entra na metade `novo` do `painel`.
    kJuntar,
    // Não cabe: a divisão do `painel` já tem 4 abas.
    kCheia,
    // O painel `origem` (arrastado pela alça) sai da divisão.
    kTirar,
    // O painel `origem` está sendo arrastado, ainda sem lugar: `titulo` é a
    // dica de onde soltar.
    kArrastando,
  };

  PreviaDeSoltar();
  PreviaDeSoltar(const PreviaDeSoltar&) = delete;
  PreviaDeSoltar& operator=(const PreviaDeSoltar&) = delete;
  ~PreviaDeSoltar() override;

  // Retângulos nas coordenadas desta camada. `origem`: o painel arrastado
  // pela alça (vazio quando a aba vem da lateral), que fica apagado. Repetir
  // o que já está à vista não custa nada.
  void Mostrar(Modo modo,
               const gfx::Rect& painel,
               const gfx::Rect& novo,
               const gfx::Rect& origem,
               const ui::ImageModel& icone,
               const std::u16string& titulo);
  void Esconder();

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  class Cartao;

  void Aparecer(views::View* view, float opacidade);

  // O painel arrastado pela alça, apagado (camada de cor preta).
  raw_ptr<views::View> apagado_ = nullptr;
  // O lugar onde a aba entra: vidro fosco com o ícone e o título.
  raw_ptr<Cartao> cartao_ = nullptr;
  bool mostrando_ = false;
  Modo modo_ = Modo::kJuntar;
  gfx::Rect de_;
  gfx::Rect para_;
  gfx::Rect origem_;
  gfx::SlideAnimation movimento_{this};
  base::WeakPtrFactory<PreviaDeSoltar> fabrica_{this};
};

// A alça no meio do alto de cada painel da divisão. Quieta, é um risquinho
// que não pega o mouse (o clique passa pra página); com o ponteiro chegando
// perto (o MultiContentsView acompanha o mouse por cima das páginas), vira
// uma cápsula com "•••" que dá pra pegar. Arrastar leva o painel pra um lado
// de outro painel (muda de lugar) ou, soltando na lateral, tira ele da
// divisão; Esc cancela. Um clique ativa o painel; o botão direito abre o menu
// da divisão daquele painel (fechar, tirar, arranjos).
class AlcaDoPainel : public views::View,
                     public gfx::AnimationDelegate,
                     public views::ContextMenuController,
                     public ui::EventObserver {
  METADATA_HEADER(AlcaDoPainel, views::View)

 public:
  static constexpr int kLargura = 72;
  static constexpr int kAltura = 26;

  // `painel`: a posição em MultiContentsView::contents_container_views().
  AlcaDoPainel(MultiContentsView& paginas,
               PreviaDeSoltar& previa,
               TabStripModel& modelo,
               int painel);
  AlcaDoPainel(const AlcaDoPainel&) = delete;
  AlcaDoPainel& operator=(const AlcaDoPainel&) = delete;
  ~AlcaDoPainel() override;

  // O painel dela é o ativo (os pontinhos ficam na cor de destaque).
  void SetAtiva(bool ativa);
  // O ponteiro está perto: a alça abre (e passa a pegar o mouse).
  void SetPerto(bool perto);
  bool arrastando() const { return arrastando_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::mojom::MenuSourceType source_type)
      override;

  // ui::EventObserver (o Esc durante o arrasto):
  void OnEvent(const ui::Event& event) override;

 private:
  struct Destino {
    tabs::TabHandle alvo;
    Lado lado = Lado::kDireita;
    bool tirar = false;

    bool operator==(const Destino&) const = default;
  };

  tabs::TabInterface* Aba() const;
  void AtualizarEventos();
  // `ponto` nas coordenadas do MultiContentsView.
  void Atualizar(const gfx::Point& ponto);
  void Terminar(bool soltar);

  const raw_ref<MultiContentsView> paginas_;
  const raw_ref<PreviaDeSoltar> previa_;
  const raw_ref<TabStripModel> modelo_;
  const int painel_;
  bool ativa_ = false;
  bool perto_ = false;
  bool arrastando_ = false;
  // Cancelado com Esc: o resto do arrasto (até soltar o botão) não conta.
  bool cancelado_ = false;
  gfx::Point inicio_;
  tabs::TabHandle aba_;
  // O destino da última atualização (nullopt: sem lugar), pra não refazer a
  // prévia a cada movimento.
  std::optional<std::optional<Destino>> ultimo_;
  gfx::SlideAnimation abrir_{this};
  std::unique_ptr<views::EventMonitor> monitor_;
  std::unique_ptr<ui::SimpleMenuModel> menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::WeakPtrFactory<AlcaDoPainel> fabrica_{this};
};

// Acompanha o mouse por cima da janela (sem pegar os eventos: a página
// continua recebendo tudo) e abre a alça do painel quando o ponteiro chega
// perto do meio do alto dele. Ligado só com a divisão à vista.
class VigiaDasAlcas : public ui::EventObserver {
 public:
  VigiaDasAlcas(views::View& paginas,
                const std::vector<raw_ptr<AlcaDoPainel>>& alcas);
  VigiaDasAlcas(const VigiaDasAlcas&) = delete;
  VigiaDasAlcas& operator=(const VigiaDasAlcas&) = delete;
  ~VigiaDasAlcas() override;

  void Ligar(bool ligado);

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

 private:
  void Fechar();

  const raw_ref<views::View> paginas_;
  const raw_ref<const std::vector<raw_ptr<AlcaDoPainel>>> alcas_;
  std::unique_ptr<views::EventMonitor> monitor_;
};

class AlvoDaDivisao : public TabDragTarget {
 public:
  AlvoDaDivisao(MultiContentsView& paginas,
                PreviaDeSoltar& previa,
                TabStripModel& modelo);
  AlvoDaDivisao(const AlvoDaDivisao&) = delete;
  AlvoDaDivisao& operator=(const AlvoDaDivisao&) = delete;
  ~AlvoDaDivisao() override;

  // TabDragTarget:
  void OnTabDragEntered() override;
  TabDragContext* OnTabDragUpdated(TabDragTarget::DragController& controller,
                                   const gfx::Point& point_in_screen) override;
  void OnTabDragExited(const gfx::Point& point_in_screen) override;
  void OnTabDragEnded() override;
  bool CanDropTab() override;
  void HandleTabDrop(TabDragTarget::DragController& controller) override;
  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) override;

  // Passa a acompanhar o arrasto do sistema na janela das páginas (sem custo
  // fora dele). Repetir não faz nada. Falso se não deu (sem janela).
  bool AcompanharArrastoDoSistema();
  // A página avisou que o arrasto do sistema saiu dela (BrowserView::
  // PreHandleDragExit): pode ter ido pra outro painel ou pra fora da janela.
  void ArrastoSaiuDaPagina();

 private:
  class Sistema;

  struct Escolha {
    tabs::TabHandle alvo;
    Lado lado = Lado::kDireita;
    bool pode = false;

    bool operator==(const Escolha&) const = default;
  };

  void Limpar();
  // Volta a arrastada pra tela, se a gente tinha trocado pela de antes.
  void DevolverAtivacao();

  const raw_ref<MultiContentsView> paginas_;
  const raw_ref<PreviaDeSoltar> previa_;
  const raw_ref<TabStripModel> modelo_;
  std::optional<Escolha> escolha_;
  // A aba arrastada, quando a gente ativou a de antes do clique no lugar
  // dela.
  tabs::TabHandle trocou_de_;
  bool trocou_ = false;
  // O TabDragController está com este alvo (entre o Entered e o Exited).
  bool dentro_ = false;
  std::unique_ptr<Sistema> sistema_;
  base::OnceClosureList ao_destruir_;
  base::WeakPtrFactory<AlvoDaDivisao> fabrica_{this};
};

}  // namespace navegador::divisao

#endif  // NAVEGADOR_BROWSER_DIVISAO_SOLTAR_H_
