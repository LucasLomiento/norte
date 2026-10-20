#include "navegador/browser/sem_ia/sem_ia.h"

#include <string>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/span.h"

namespace navegador {

namespace {

// Nomes das flags (o texto do BASE_FEATURE, sem o "k").
constexpr char kSemIA[] =
    "AimEnabled,"          // "Modo IA" na barra de endereço
    "Aim3pEntrypoint,"     // entrada do Modo IA com outra busca
    "NtpComposebox,"       // caixa de IA na nova aba
    "Compose,"             // "Me ajude a escrever"
    "LensOverlay,"         // Google Lens com IA por cima da página
    "Glic,"                // Gemini no Chrome
    "GlicRollout,"
    "GlicSelectionPrompt,"  // sugestão do Gemini ao selecionar texto
    "HistoryEmbeddings,"   // busca no histórico com IA
    "TabOrganization,"     // "organizar guias" com IA
    "AutofillAi,"          // preenchimento automático com IA
    "AutofillAiWithDataSchema,"
    "PermissionsAIv1,"     // pedidos de permissão avaliados por IA
    "PermissionsAIv3,"
    "PermissionsAIP92,"
    "ContextualTasks,"     // tarefas com IA no painel lateral
    "ContextualCueing,"    // sugestões de IA pela página
    "ContextualCueingV2,"
    "GoogleSearchAiModeWorkspace,"
    "AiModeOmniboxEntryPoint,"
    "Indigo,"
    "SkillsEnabled";

// Ligadas por padrão: "<flag>" ou "<flag>:<parâmetro>/<valor>".
constexpr const char* kLigadas[] = {
    // Barras de rolagem finas por cima da página (as "Fluent" do Chromium),
    // que só aparecem rolando, como no Dia e no macOS; no lugar das cinzas
    // com setinhas, que ocupavam a borda de toda página.
    "OverlayScrollbar:mode/overlay",
};

// Desligadas por padrão. Fora da "official build" o Chromium aplica a lista
// de experimentos de teste (testing/variations/fieldtrial_testing_config.json,
// ~680 recursos em teste). Ela continua valendo: os perfis já foram migrados
// por alguns desses recursos, e desligar a lista inteira derruba o navegador
// na partida (a lista de buscadores "voltando" de uma migração dá
// NOTREACHED). Aqui saem só os que deram problema.
constexpr const char* kDesligadas[] = {
    // Voltar, avançar, recarregar e "Início" desenhados por uma página WebUI
    // dentro da barra: com a barra escondida (Ctrl+Shift+S), ao descer por
    // cima da página eles não apareciam no Wayland.
    "WebUIBackForwardButton",
    "WebUIReloadButton",
    "WebUIHomeButton",
    "WebUISplitTabsButton",
    // Quadros pedidos no ritmo das respostas de quadro do Wayland (desligado
    // no Chromium). Sem resposta do compositor (NVIDIA 615 + Hyprland), a
    // janela parava de pedir quadros e a tela congelava.
    "WaylandExternalBeginFrameSource",
    // Balões de "conheça este recurso" e propagandas do Chrome, vários de
    // coisas que o Norte não tem (login do Google, enviar aba pro celular,
    // o Chrome no celular).
    "IPH_AutofillAtMemory",
    "IPH_BookmarkBarSimplified",
    "IPH_DesktopSharedHighlighting",
    "IPH_ExtensionsZeroStatePromo",
    "IPH_GMCSaveVideoFrame",
    "IPH_PasswordsSavePrimingPromo",
    "IPH_PasswordsSaveRecoveryPromo",
    "IPH_PriceTrackingPageActionIconLabelFeature",
    "IPH_ReadingModeLineFocus",
    "IPH_ReadingModePageActionLabel",
    "IPH_ReadingModeSidePanel",
    "IPH_SendTabToSelfOmnibox",
    "IPH_SendTabToSelfTutorial",
    "IPH_SignInBenefits",
    "IPH_TabSearchComboButton",
    "MobilePromoOnDesktopWithQRCodeWave1",
    "EnableManagementPromotionBanner",
};

// Junta `flags` à lista do `switch_name` na linha de comando, menos as que
// quem abriu o navegador já citou (ligando ou desligando): essa decide.
void Juntar(base::CommandLine* command_line,
            const char* switch_name,
            base::span<const char* const> flags) {
  const std::string pedidas =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures) + "," +
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string lista = command_line->GetSwitchValueASCII(switch_name);
  for (const char* flag : flags) {
    const std::string_view nome =
        std::string_view(flag).substr(0, std::string_view(flag).find(':'));
    if (pedidas.find(nome) != std::string::npos) {
      continue;
    }
    if (!lista.empty()) {
      lista += ",";
    }
    lista += flag;
  }
  command_line->AppendSwitchASCII(switch_name, lista);
}

}  // namespace

void LigarRecursos(base::CommandLine* command_line) {
  Juntar(command_line, switches::kEnableFeatures, kLigadas);
  Juntar(command_line, switches::kDisableFeatures, kDesligadas);
}

void DesligarIA(base::CommandLine* command_line) {
  std::string desligadas =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  if (!desligadas.empty()) {
    desligadas += ",";
  }
  desligadas += kSemIA;
  command_line->AppendSwitchASCII(switches::kDisableFeatures, desligadas);
}

}  // namespace navegador
