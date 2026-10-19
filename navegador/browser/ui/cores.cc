#include "navegador/browser/ui/cores.h"

#include <map>

#include "base/environment.h"
#include "base/no_destructor.h"
#include "build/build_config.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"

namespace navegador::cores {

bool Vidro() {
  static const bool vidro = [] {
#if BUILDFLAG(IS_LINUX)
    std::unique_ptr<base::Environment> ambiente = base::Environment::Create();
    if (std::optional<std::string> forcado =
            ambiente->GetVar("NAVEGADOR_VIDRO")) {
      return *forcado != "0";
    }
    return ambiente->HasVar("HYPRLAND_INSTANCE_SIGNATURE");
#else
    return false;
#endif
  }();
  return vidro;
}


namespace {

// Paleta do Dia: claro e escuro lado a lado. ARGB, alfa primeiro.
struct Paleta {
  SkColor fundo;          // barra lateral e moldura (SolidBackgroundFillColorBase)
  SkColor barra;          // barra de navegação (no Dia, da cor da página)
  SkColor pagina;         // fundo da página antes de carregar
  SkColor texto;          // TextFillColorPrimary
  SkColor texto_inativo;  // abas que não estão selecionadas
  SkColor aba_ativa;      // linha da aba ativa
  SkColor aba_hover;      // SubtleFillColorSecondary
  SkColor aba_selecionada;  // várias abas selecionadas
  SkColor divisor;        // DividerStrokeColorDefault
  SkColor endereco;       // barra de endereço parada (no Dia, texto solto)
  SkColor endereco_hover; // com o mouse em cima vira caixa
  SkColor destaque;       // AccentFillColorDefault
};

constexpr Paleta kClaro = {
    .fundo = SkColorSetRGB(0xF3, 0xF3, 0xF3),
    .barra = SK_ColorWHITE,
    .pagina = SK_ColorWHITE,
    .texto = SkColorSetARGB(0xE4, 0x00, 0x00, 0x00),
    .texto_inativo = SkColorSetARGB(0xB8, 0x00, 0x00, 0x00),
    .aba_ativa = SkColorSetARGB(0xCC, 0xFF, 0xFF, 0xFF),
    .aba_hover = SkColorSetARGB(0x0F, 0x00, 0x00, 0x00),
    .aba_selecionada = SkColorSetARGB(0x12, 0x00, 0x00, 0x00),
    .divisor = SkColorSetARGB(0x0F, 0x00, 0x00, 0x00),
    .endereco = SK_ColorWHITE,
    .endereco_hover = SkColorSetRGB(0xF1, 0xF1, 0xF1),
    .destaque = SkColorSetRGB(0x00, 0x5F, 0xB8),
};

// Escuro: o "Noite elétrica" do Resenha (resenha/app/src/renderer/app.css):
// grafite neutro, quase preto, sem tinta de cor; um acento só, o azul
// elétrico. A lateral fica direto no fundo da janela e só o conteúdo é um
// painel.
namespace noite {
constexpr SkColor kFundo = SkColorSetRGB(0x09, 0x09, 0x0B);    // --bg-canvas
constexpr SkColor kPainel = SkColorSetRGB(0x11, 0x11, 0x13);   // --bg-panel
constexpr SkColor kElevado = SkColorSetRGB(0x18, 0x18, 0x1B);  // --bg-raised
constexpr SkColor kSobre = SkColorSetRGB(0x22, 0x22, 0x26);    // --bg-overlay
constexpr SkColor kCampo = SkColorSetRGB(0x0C, 0x0C, 0x0E);    // --bg-input
constexpr SkColor kTexto = SkColorSetRGB(0xF4, 0xF4, 0xF5);    // --fg
constexpr SkColor kTexto2 = SkColorSetRGB(0xAA, 0xAA, 0xB3);   // --fg-2
constexpr SkColor kTexto3 = SkColorSetRGB(0x88, 0x88, 0x92);   // --fg-3
constexpr SkColor kTexto4 = SkColorSetRGB(0x52, 0x52, 0x5B);   // --fg-4
// Camadas de interação brancas e translúcidas (--hover, --selected, --press).
constexpr SkColor kHover = SkColorSetARGB(0x0B, 0xFF, 0xFF, 0xFF);
constexpr SkColor kSelecionado = SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPressionado = SkColorSetARGB(0x1C, 0xFF, 0xFF, 0xFF);
constexpr SkColor kLinha = SkColorSetARGB(0x12, 0xFF, 0xFF, 0xFF);      // --line
constexpr SkColor kLinhaForte = SkColorSetARGB(0x1F, 0xFF, 0xFF, 0xFF); // --line-strong
// Azul elétrico: preenchimento com texto branco, e como texto/ícone.
constexpr SkColor kAcento = SkColorSetRGB(0x2B, 0x6C, 0xF5);         // --accent
constexpr SkColor kAcentoHover = SkColorSetRGB(0x35, 0x77, 0xF7);    // --accent-hover
constexpr SkColor kAcentoTexto = SkColorSetRGB(0x7A, 0xAA, 0xFF);    // --accent-fg
constexpr SkColor kAcentoMarca = SkColorSetRGB(0x4F, 0x8C, 0xFF);    // --brand-2
constexpr SkColor kVermelho = SkColorSetRGB(0xFF, 0x5A, 0x6B);       // --red
}  // namespace noite

constexpr Paleta kEscuro = {
    .fundo = noite::kFundo,
    .barra = noite::kPainel,
    .pagina = noite::kPainel,
    .texto = noite::kTexto,
    .texto_inativo = noite::kTexto2,
    .aba_ativa = noite::kSelecionado,
    .aba_hover = noite::kHover,
    .aba_selecionada = noite::kSelecionado,
    .divisor = noite::kLinha,
    .endereco = noite::kPainel,
    .endereco_hover = noite::kElevado,
    .destaque = noite::kAcentoMarca,
};

// O resto da interface do Chromium (menus, balões, diálogos, páginas
// internas como chrome://settings) sai das cores "de sistema" do Material;
// recalculadas aqui, no fim, todas as que dependem delas mudam junto.
void PintarSistemaEscuro(ui::ColorMixer& mixer) {
  using namespace noite;
  mixer[ui::kColorSysBase] = {kPainel};
  mixer[ui::kColorSysBaseContainer] = {kElevado};
  mixer[ui::kColorSysBaseContainerElevated] = {kSobre};
  mixer[ui::kColorSysSurface] = {kElevado};
  mixer[ui::kColorSysSurface1] = {kElevado};
  mixer[ui::kColorSysSurface2] = {kElevado};
  mixer[ui::kColorSysSurface3] = {SkColorSetRGB(0x1D, 0x1D, 0x21)};
  mixer[ui::kColorSysSurface4] = {kSobre};
  mixer[ui::kColorSysSurface5] = {SkColorSetRGB(0x26, 0x26, 0x2B)};
  mixer[ui::kColorSysSurfaceVariant] = {kSobre};
  mixer[ui::kColorSysSurfaceSection] = {kElevado};
  mixer[ui::kColorSysBaseContainerOnSurface] = {kSobre};
  mixer[ui::kColorSysNeutralContainer] = {kSobre};
  mixer[ui::kColorSysHeader] = {kFundo};
  mixer[ui::kColorSysHeaderInactive] = {kFundo};
  mixer[ui::kColorSysHeaderContainer] = {kPainel};
  mixer[ui::kColorSysHeaderContainerInactive] = {kPainel};
  mixer[ui::kColorSysOmniboxContainer] = {kElevado};

  mixer[ui::kColorSysOnSurface] = {kTexto};
  mixer[ui::kColorSysOnSurfaceVariant] = {kTexto2};
  mixer[ui::kColorSysOnSurfaceSecondary] = {kTexto2};
  mixer[ui::kColorSysOnSurfaceSubtle] = {kTexto3};
  mixer[ui::kColorSysOnSurfacePrimary] = {kAcentoTexto};
  mixer[ui::kColorSysOnSurfacePrimaryInactive] = {kTexto3};

  mixer[ui::kColorSysPrimary] = {kAcento};
  mixer[ui::kColorSysOnPrimary] = {SK_ColorWHITE};
  mixer[ui::kColorSysPrimaryContainer] = {
      color_utils::AlphaBlend(kAcentoMarca, kElevado, 0.18f)};
  mixer[ui::kColorSysOnPrimaryContainer] = {kAcentoTexto};
  mixer[ui::kColorSysTonalContainer] = {
      color_utils::AlphaBlend(kAcentoMarca, kElevado, 0.16f)};
  mixer[ui::kColorSysOnTonalContainer] = {SkColorSetRGB(0xD6, 0xE4, 0xFF)};
  mixer[ui::kColorSysBaseTonalContainer] = {
      color_utils::AlphaBlend(kAcentoMarca, kPainel, 0.12f)};
  mixer[ui::kColorSysOnBaseTonalContainer] = {SkColorSetRGB(0xD6, 0xE4, 0xFF)};
  mixer[ui::kColorSysTonalOutline] = {
      color_utils::AlphaBlend(kAcentoMarca, kElevado, 0.45f)};
  mixer[ui::kColorSysInversePrimary] = {kAcento};

  mixer[ui::kColorSysOutline] = {color_utils::AlphaBlend(SK_ColorWHITE, kElevado, 0.14f)};
  mixer[ui::kColorSysNeutralOutline] = {color_utils::AlphaBlend(SK_ColorWHITE, kElevado, 0.12f)};
  mixer[ui::kColorSysDivider] = {color_utils::AlphaBlend(SK_ColorWHITE, kElevado, 0.07f)};
  mixer[ui::kColorSysOnHeaderDivider] = {kLinha};
  mixer[ui::kColorSysOnHeaderPrimary] = {kTexto2};

  mixer[ui::kColorSysStateHover] = {kHover};
  mixer[ui::kColorSysStateHoverOnSubtle] = {kHover};
  mixer[ui::kColorSysStateHoverOnProminent] = {SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF)};
  mixer[ui::kColorSysStatePressed] = {kPressionado};
  mixer[ui::kColorSysStateRippleNeutralOnSubtle] = {kPressionado};
  mixer[ui::kColorSysStateRippleNeutralOnProminent] = {kPressionado};
  mixer[ui::kColorSysStateRipplePrimary] = {SkColorSetA(kAcentoMarca, 0x3D)};
  mixer[ui::kColorSysStateFocusRing] = {kAcentoMarca};
  mixer[ui::kColorSysStateFocusHighlight] = {SkColorSetA(kAcentoMarca, 0x24)};
  mixer[ui::kColorSysStateTextHighlight] = {SkColorSetA(kAcentoMarca, 0x5A)};
  mixer[ui::kColorSysStateOnTextHighlight] = {kTexto};
  mixer[ui::kColorSysStateDisabled] = {SkColorSetA(kTexto, 0x61)};
  mixer[ui::kColorSysStateDisabledContainer] = {SkColorSetA(kTexto, 0x14)};
  mixer[ui::kColorSysStateScrim] = {SkColorSetARGB(0xB3, 0x04, 0x04, 0x06)};
  mixer[ui::kColorSysStateHeaderHover] = {kHover};
  mixer[ui::kColorSysStateOnHeaderHover] = {kHover};

  mixer[ui::kColorSysError] = {kVermelho};
  mixer[ui::kColorSysShadow] = {SK_ColorBLACK};

  // Avisos rápidos ("Link copiado"): o Material usa a superfície invertida
  // (um balão claro no escuro); aqui ficam no cinza elevado, como os menus.
  mixer[ui::kColorToastBackground] = {kSobre};
  mixer[ui::kColorToastBackgroundProminent] = {kSobre};
  mixer[ui::kColorToastForeground] = {kTexto};
  mixer[ui::kColorToastButton] = {kAcentoTexto};
}

// Cor da página na barra de navegação, com texto e ícones no contraste.
class Barra : public ui::ColorProviderKey::InitializerSupplier {
 public:
  explicit Barra(SkColor cor) : cor_(SkColorSetA(cor, SK_AlphaOPAQUE)) {}

  // Só a janela do navegador chama isto (pela chave); o trabalho de verdade
  // é o Pintar, chamado no fim do mixer do navegador, depois da paleta.
  void AddColorMixers(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const override {}

  void Pintar(ui::ColorMixer& mixer) const {
    const bool escura = color_utils::IsDark(cor_);
    const SkColor texto =
        escura ? SK_ColorWHITE : SkColorSetARGB(0xE4, 0x00, 0x00, 0x00);
    mixer[kColorToolbar] = {cor_};
    mixer[kColorWebContentsBackground] = {cor_};
    mixer[kColorLocationBarBackground] = {cor_};
    mixer[kColorLocationBarBackgroundHovered] = {
        color_utils::BlendTowardMaxContrast(cor_, 0x16)};
    mixer[kColorToolbarText] = {texto};
    mixer[kColorOmniboxText] = {texto};
    mixer[kColorOmniboxTextDimmed] = {SkColorSetA(texto, 0x99)};
    mixer[kColorToolbarButtonIcon] = {texto};
    mixer[kColorToolbarButtonIconHovered] = {texto};
    mixer[kColorToolbarButtonIconPressed] = {texto};
    mixer[kColorToolbarButtonIconInactive] = {SkColorSetA(texto, 0x5C)};
    mixer[kColorToolbarInkDrop] = {texto};
    mixer[kColorToolbarContentAreaSeparator] = {SkColorSetA(texto, 0x1A)};

    // Ícone do site sem a bolinha de fundo do Chromium (no Dia é só o
    // ícone).
    mixer[kColorOmniboxIconBackground] = {SK_ColorTRANSPARENT};
    mixer[kColorOmniboxIconForeground] = {texto};
    mixer[kColorOmniboxIconHover] = {SkColorSetA(texto, 0x1A)};
    mixer[kColorOmniboxIconPressed] = {SkColorSetA(texto, 0x29)};
    mixer[kColorOmniboxActionIcon] = {SkColorSetA(texto, 0xB3)};
    mixer[kColorOmniboxActionIconHover] = {SkColorSetA(texto, 0x1A)};

    // Endereço focado e sugestões no mesmo tom da barra: sem isso, num site
    // escuro com o sistema claro a caixa ficava branca com o texto branco.
    const SkColor caixa =
        escura ? color_utils::BlendTowardMaxContrast(cor_, 0x1C)
               : SK_ColorWHITE;
    const SkColor fraco = SkColorSetA(texto, 0x99);
    const SkColor link = escura ? SkColorSetRGB(0x8A, 0xB4, 0xF8)
                                : SkColorSetRGB(0x0B, 0x57, 0xD0);
    mixer[kColorOmniboxResultsBackground] = {caixa};
    mixer[kColorOmniboxResultsBackgroundIph] = {caixa};
    mixer[kColorOmniboxResultsBackgroundHovered] = {
        color_utils::BlendTowardMaxContrast(caixa, 0x10)};
    mixer[kColorOmniboxResultsBackgroundSelected] = {
        color_utils::BlendTowardMaxContrast(caixa, 0x1C)};
    mixer[kColorOmniboxResultsBackgroundHoverOverlay] = {
        SkColorSetA(texto, 0x10)};
    mixer[kColorOmniboxResultsTextSelected] = {texto};
    mixer[kColorOmniboxResultsTextDimmed] = {fraco};
    mixer[kColorOmniboxResultsTextDimmedSelected] = {fraco};
    mixer[kColorOmniboxResultsTextSecondary] = {fraco};
    mixer[kColorOmniboxResultsTextSecondarySelected] = {fraco};
    mixer[kColorOmniboxResultsTextAnswer] = {link};
    mixer[kColorOmniboxResultsIcon] = {SkColorSetA(texto, 0xB3)};
    mixer[kColorOmniboxResultsIconSelected] = {SkColorSetA(texto, 0xB3)};
    mixer[kColorOmniboxResultsIconHovered] = {SkColorSetA(texto, 0x21)};
    mixer[kColorOmniboxResultsUrl] = {link};
    mixer[kColorOmniboxResultsUrlSelected] = {link};
  }

 private:
  SkColor cor_;
};

std::map<SkColor, Barra*>& Barras() {
  static base::NoDestructor<std::map<SkColor, Barra*>> barras;
  return *barras;
}

const Barra* BarraDe(const ui::ColorProviderKey::InitializerSupplier* s) {
  if (!s) {
    return nullptr;
  }
  for (const auto& [cor, barra] : Barras()) {
    if (barra == s) {
      return barra;
    }
  }
  return nullptr;
}

}  // namespace

ui::ColorProviderKey::InitializerSupplier* FornecedorDaBarra(SkColor cor) {
  cor = SkColorSetA(cor, SK_AlphaOPAQUE);
  Barra*& barra = Barras()[cor];
  if (!barra) {
    barra = new Barra(cor);  // Vive até o fim do processo (ver cores.h).
  }
  return barra;
}

void AddColorMixer(ui::ColorProvider* provider,
                   const ui::ColorProviderKey& key) {
  // Janelas de app (PWA) seguem a cor do app; a barra na cor da página é
  // nossa e entra no fim.
  const Barra* barra = BarraDe(key.app_controller.get());
  if (key.app_controller && !barra) {
    return;
  }
  const Paleta& p = key.color_mode == ui::ColorProviderKey::ColorMode::kDark
                        ? kEscuro
                        : kClaro;
  ui::ColorMixer& mixer = provider->AddMixer();

  // Moldura = fundo da barra lateral. Igual com a janela focada ou não.
  // A cor escolhida no "Personalizar" do navegador (key.user_color) tinge o
  // fundo; sem escolha, fica neutro. Com o vidro, translúcida.
  const bool escuro = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  SkColor fundo = p.fundo;
  if (key.user_color &&
      key.user_color_source == ui::ColorProviderKey::UserColorSource::kAccent) {
    // Bem de leve: no escuro, o grafite com muita cor do espaço ficava
    // pesado (o Resenha só tinge com o acento em fio, ícone ou barra).
    fundo = color_utils::AlphaBlend(*key.user_color, p.fundo,
                                    escuro ? 0.12f : 0.14f);
  }
  const SkColor moldura =
      Vidro() ? SkColorSetA(fundo, escuro ? kVidroAlfaEscuro : kVidroAlfaClaro)
             : fundo;
  mixer[ui::kColorFrameActive] = {moldura};
  mixer[ui::kColorFrameInactive] = {moldura};

  // Cartão: barra de navegação e página.
  mixer[kColorToolbar] = {p.barra};
  mixer[kColorWebContentsBackground] = {p.pagina};
  mixer[kColorToolbarButtonIcon] = {p.texto};
  mixer[kColorToolbarSeparator] = {SK_ColorTRANSPARENT};
  // A linha fina entre a barra e a página.
  mixer[kColorToolbarContentAreaSeparator] = {p.divisor};
  mixer[kColorToolbarTopSeparatorFrameActive] = {SK_ColorTRANSPARENT};
  mixer[kColorToolbarTopSeparatorFrameInactive] = {SK_ColorTRANSPARENT};

  // Barra de endereço.
  mixer[kColorLocationBarBackground] = {p.endereco};
  mixer[kColorLocationBarBackgroundHovered] = {p.endereco_hover};
  mixer[kColorOmniboxText] = {p.texto};

  // Abas da lateral.
  mixer[kColorTabBackgroundActiveFrameActive] = {p.aba_ativa};
  mixer[kColorTabBackgroundActiveFrameInactive] = {p.aba_ativa};
  // Inativas sem fundo nenhum (só o hover), como no Dia.
  mixer[kColorTabBackgroundInactiveFrameActive] = {SK_ColorTRANSPARENT};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {SK_ColorTRANSPARENT};
  mixer[kColorTabBackgroundInactiveHoverFrameActive] = {p.aba_hover};
  mixer[kColorTabBackgroundInactiveHoverFrameInactive] = {p.aba_hover};
  mixer[kColorTabBackgroundSelectedFrameActive] = {p.aba_selecionada};
  mixer[kColorTabBackgroundSelectedFrameInactive] = {p.aba_selecionada};
  mixer[kColorTabForegroundActiveFrameActive] = {p.texto};
  mixer[kColorTabForegroundActiveFrameInactive] = {p.texto};
  mixer[kColorTabForegroundInactiveFrameActive] = {p.texto_inativo};
  mixer[kColorTabForegroundInactiveFrameInactive] = {p.texto_inativo};
  mixer[kColorTabDividerFrameActive] = {p.divisor};
  mixer[kColorTabDividerFrameInactive] = {p.divisor};
  mixer[kColorVerticalTabStripShadow] = {SK_ColorTRANSPARENT};
  // Borda fina dos ladrilhos (abas fixadas), como os favoritos do Dia.
  mixer[kColorVerticalTabPinnedOutline] = {p.divisor};

  // "+ Nova aba" e os botões do topo da lateral: sem fundo, texto discreto
  // (o realce do hover vem do ink drop).
  mixer[kColorNewTabButtonCRBackgroundFrameActive] = {SK_ColorTRANSPARENT};
  mixer[kColorNewTabButtonCRBackgroundFrameInactive] = {SK_ColorTRANSPARENT};
  mixer[kColorTabSearchButtonCRForegroundFrameActive] = {p.texto_inativo};
  mixer[kColorTabSearchButtonCRForegroundFrameInactive] = {p.texto_inativo};

  // Destaque (barra de carregamento, foco, botões principais).
  mixer[ui::kColorAccent] = {p.destaque};

  if (escuro) {
    PintarSistemaEscuro(mixer);
    // Links e botões de texto no azul claro (o preenchido fica no escuro).
    mixer[ui::kColorLinkForegroundDefault] = {noite::kAcentoTexto};
    mixer[ui::kColorButtonForeground] = {noite::kTexto};
    // Menus e balões no elevado, com a seleção cinza (não azul) como no
    // Resenha.
    mixer[ui::kColorMenuBackground] = {noite::kElevado};
    mixer[ui::kColorMenuItemBackgroundHighlighted] = {noite::kSobre};
    mixer[ui::kColorMenuSeparator] = {noite::kLinha};
    mixer[ui::kColorBubbleBackground] = {noite::kElevado};
    mixer[ui::kColorDialogBackground] = {noite::kElevado};
    // Cor do espaço na moldura, mais discreta (grafite sujo de cor fica
    // pesado).
  }

  if (barra) {
    barra->Pintar(mixer);
  }
}

}  // namespace navegador::cores
