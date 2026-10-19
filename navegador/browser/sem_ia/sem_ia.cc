#include "navegador/browser/sem_ia/sem_ia.h"

#include <string>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"

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

}  // namespace

void LigarRecursos(base::CommandLine* command_line) {
  const std::string desligadas =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);
  std::string ligadas =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  const std::string pedidas = desligadas + "," + ligadas;
  for (const char* flag : kLigadas) {
    const std::string_view nome =
        std::string_view(flag).substr(0, std::string_view(flag).find(':'));
    if (pedidas.find(nome) != std::string::npos) {
      continue;
    }
    if (!ligadas.empty()) {
      ligadas += ",";
    }
    ligadas += flag;
  }
  command_line->AppendSwitchASCII(switches::kEnableFeatures, ligadas);
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
