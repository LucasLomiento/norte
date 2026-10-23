// Medidas do visual do Dia aplicadas no Chromium.
//
// Medidas tiradas do Dia rodando, ao lado do nosso. O
// Chromium pergunta as medidas dele pra GetLayoutConstant/GetLayoutInsets
// (chrome/browser/ui/layout_constants.cc); o gancho lá consulta este arquivo
// primeiro, e o que não está aqui fica como o Chromium faz.
//
// Compilado dentro do alvo //chrome/browser/ui:layout_constants (e não num
// alvo próprio) porque precisa do enum dele, e ele precisa da gente.

#ifndef NAVEGADOR_BROWSER_UI_VISUAL_H_
#define NAVEGADOR_BROWSER_UI_VISUAL_H_

#include <optional>

#include "chrome/browser/ui/layout_constants.h"
#include "ui/gfx/geometry/insets.h"

namespace navegador::visual {

// Cartão do conteúdo: a barra de navegação e a página juntas, com margem até
// a borda da janela (medido no Dia rodando: margem ~6). Cantos retos: os
// arredondados do Dia (~8) deixavam um dente escuro entre a lateral e a
// página.
inline constexpr int kCartaoMargem = 6;
inline constexpr int kCartaoRaio = 0;

// Folga entre as abas da lateral: no Dia, uma aba (destaque de 30) a cada 35.
inline constexpr int kEspacoEntreAbas = 5;

// Lateral recolhida: só a borda onde se encosta o mouse pra ela aparecer
// por cima (a lateral flutuante do Dia).
inline constexpr int kLateralRecolhida = 3;
// Abaixo disto (lateral estreita), a área dos fixados some.
inline constexpr int kLarguraMinimaDosFixos = 100;

// Id (views::View::SetID) da área dos fixados do espaço
// (navegador/browser/ui/fixos.h) dentro do TabStripView: o layout dele põe
// a área entre os favoritos e as abas.
inline constexpr int kIdAreaDeFixos = 0x6E66;
// Id da camada da prévia de link (navegador/browser/ui/previa.h), filha do
// MultiContentsView: o layout dele estica ela por cima das páginas.
inline constexpr int kIdPrevia = 0x6E70;
// Id do painel da Memória (navegador/browser/ui/painel_da_memoria.h), também filho do
// MultiContentsView e esticado por cima das páginas.
inline constexpr int kIdMemoria = 0x6E6D;

std::optional<int> LayoutConstantOverride(LayoutConstant constant);
std::optional<gfx::Insets> LayoutInsetsOverride(LayoutInset inset);

}  // namespace navegador::visual

#endif  // NAVEGADOR_BROWSER_UI_VISUAL_H_
