#include "navegador/browser/sem_ia/sem_ia.h"

#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

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
    "SkillsEnabled,"
    // Ligados no Chromium 155 ou pela lista de teste (conferido em 21/10/2026:
    // sem eles, as páginas viam LanguageModel, Summarizer e
    // document.modelContext, e o perfil gravava o conteúdo das abas).
    "OptimizationGuideModelExecution,"  // IA do Google nos servidores
    "OnDeviceModelBackgroundDownload,"  // Gemini Nano baixado sozinho
    "AIPromptAPI,"         // Gemini Nano pras páginas (LanguageModel)
    "AIPromptAPIForWorkers,"
    "AISummarizationAPI,"  // (Summarizer)
    "AISummarizationAPIForWorkers,"
    "AIWriterAPI,"         // (Writer, Rewriter e Proofreader: por origin
    "AIRewriterAPI,"       // trial, pras páginas inscritas)
    "AIProofreadingAPI,"
    "AIPromptAPIParams,"
    "WebMCP,"              // páginas oferecendo ferramentas a agentes de IA
    "GlicActor,"           // Gemini agindo nas páginas
    "GlicActorUi,"
    "HeadlessLiveCaption,"  // legendas do áudio das abas pro Gemini
    "AnnotatedPageContentExtraction,"  // conteúdo das abas gravado pra IA
    "AnnotatedPageContentPDFTextExtraction,"
    "PDFEmbeddingsGeneration,"
    "AIPageContentTrackedElementsPassword,"
    "PageContentCache,"
    "WebUIOmniboxAimPopup,"  // popup do Modo IA escondido em cada janela
    "WebUIOmniboxDynamicAiModeButton,"
    "WebUIOmniboxAskGAboutThisPage,"
    "StarterPackExpansion,"  // @gemini nas sugestões da barra
    "ApiContextualTasksPrivate,"  // extensão das "tarefas contextuais"
    "PermissionsAIv4,"
    "DevToolsConsoleInsightsTeasers,"  // IA nas ferramentas de desenvolvedor
    "DevToolsAiV2Architecture,"
    "FirstRunFeatureShowcaseGeminiStep,"
    "EnableAiSubscriptionAvatarRing";

// Ligadas por padrão: "<flag>" ou "<flag>:<parâmetro>/<valor>".
//
// Cada nome aqui ou no kDesligadas faz o Chromium pular o experimento
// inteiro da lista de teste que cita o recurso (ShouldSkipExperiment): os
// outros recursos dele voltam ao padrão, e os parâmetros também. Por isso o
// que se trava aqui vai com os parâmetros do estudo. `tools/lista_de_teste.py
// de <Recurso>` mostra o estudo e o que cai junto; `derrubados`, o que estas
// listas derrubam.
constexpr const char* kLigadas[] = {
    // Barras de rolagem finas por cima da página (as "Fluent" do Chromium),
    // que só aparecem rolando, como no Dia e no macOS; no lugar das cinzas
    // com setinhas, que ocupavam a borda de toda página.
    "OverlayScrollbar:mode/overlay",
    // Recolhendo ou abrindo a lateral (Ctrl+S), a página muda de largura a
    // cada quadro: a janela espera o quadro da página no tamanho novo antes
    // de mostrar (no Chromium, só no Mac com ARM). Sem isso o conteúdo
    // chegava 1 ou 2 quadros atrasado e a borda direita tremia (até 120 px
    // numa página pesada).
    "UseDefaultDeadlineWhenAnimatingBounds",

    // Formato do que fica gravado no perfil. A lista de teste pôs os perfis
    // nesses formatos (IndexedDB e localStorage em SQLite, favoritos e sessão
    // também cifrados, buscadores migrados), e ela muda a cada versão do
    // Chromium. Sem os dois primeiros, a primeira abertura apaga o IndexedDB
    // e esvazia o localStorage dos sites, sem volta (testado num perfil de
    // teste em 21/10/2026); sem os buscadores, o Norte cai na partida.
    // Travados com os parâmetros do estudo: sem o "stage", o IndexedDB fica
    // em UseLevelDbOnly, que apaga os bancos em SQLite.
    "IdbSqliteOnDiskRollout:stage/UseSqliteForNewStores",
    "DomStorageSqliteNewDatabases:"
    "DomStorageSqliteNewDatabasesStage/UseSqliteForNewDatabases",
    "EncryptBookmarks:stage/write_both_read_only_clear",
    "EncryptSessionStorage:stage/write_both_read_only_clear",
    "PrepopulatedEnginesMigration",
    "ApplySearchEngineTypeMigration",
    "HistoryDatabaseWriteAheadLogging",
    "DiskCacheBackendExperiment:"
    "backend/sql/DiskCacheBackendResetCacheOnGroupChange/true",
    "HttpCacheInitializeDiskCacheBackendEarly",
    "UsePersistentCacheForCodeCache",
    "PersistentCacheCollectionWalMode",
    "HttpCacheZstdCompression",
    "HttpCacheZstdDecompression",
    // Descarte de abas pelo caminho novo, o único em que o descarte das abas
    // de container (gancho no tab_lifecycle_unit.cc) foi testado.
    "WebContentsDiscard:urgent_discard_ignore_workers/true",
    // Troca de tema sem refazer o CSS das páginas (sem eles, cada troca de
    // aba custava 18% a mais de CPU).
    "ThemeChangeOptimization",
    "ColorIdCssStyleSheetOptimization",
    // Ícones arredondados e menus novos (estudo GlowUp, os 5 juntos): a troca
    // pros ícones do Lucide (navegador/icones/LUGARES) conta com eles.
    "RoundedIcons",
    "MenuSimplification",
    "TabStripDeclutter",
    "ToolbarGlowUp",
    "WebuiRefresh2026",
    // Ctrl+Tab na ordem de uso (opção em Atalhos) e vídeo em janelinha ao
    // trocar de aba (opção em Geral → Mídia).
    "CtrlTabMru",
    "BrowserInitiatedAutomaticPictureInPicture",
    // Só o idioma principal no Accept-Language: menos dados pra impressão
    // digital.
    "ReduceAcceptLanguage",
    "ReduceAcceptLanguageHTTP",
    // Processos e memória no padrão do Chromium. A lista de teste deixava 2
    // processos de reserva, aquecia 5 service workers e a quarentena do
    // alocador ia a 2 GB no navegador, nas páginas e na GPU (com 6 sites,
    // 850 MB e 19 processos contra 780 MB e 16 assim). Sem parâmetro, o
    // estudo cai e valem os padrões: 1 reserva, 2 service workers e 512 KB
    // de quarentena só no navegador.
    "MultipleSpareRPHs",
    "SpeculativeServiceWorkerWarmUp",
    "PartitionAllocSchedulerLoopQuarantine",
    "PartitionAllocWithAdvancedChecks",
    "PartitionAllocLargeEmptySlotSpanRing",
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
    // Componentes de fala baixados sem pedir na primeira abertura de cada
    // pasta de perfil (~0,4 a 0,8 GB: o motor das legendas automáticas e o
    // pacote de idioma) e o ditado do Chromium (o usuário dita pelo voxtype).
    "PreemptiveSodaDownload",
    "Dictation",
    // Ao focar a barra, uma página do Google carregada num processo à parte
    // (~20 MB), que grava cookies do Google sem a pessoa visitar.
    "Prewarm",
    // Processos de página vazios guardados pra reaproveitar, processo
    // "quente" preferido e métricas do WebGPU (experimentos da lista,
    // desligados no Chromium).
    "TransientKeepAlivePolicy",
    "PreferWarmRendererProcess",
    "CollectWebGPUSupportMetrics",
    // Sugestões abrindo só ao soltar o clique na barra: a barra mudava duas
    // vezes (foco ao apertar, sugestões ao soltar). Desligado, abrem no
    // aperto, como no Chromium até 2024.
    "OmniboxShowPopupOnMouseReleased",
};

// Os nomes dos recursos de uma lista do --enable-features ou do
// --disable-features ("*Nome<Estudo.Grupo:p/v" -> "Nome").
std::set<std::string, std::less<>> Nomes(const std::string& lista) {
  std::set<std::string, std::less<>> nomes;
  for (std::string_view item : base::SplitStringPiece(
           lista, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (item.starts_with('*')) {
      item.remove_prefix(1);
    }
    nomes.emplace(item.substr(0, item.find_first_of("<:")));
  }
  return nomes;
}

// Junta `flags` à lista do `switch_name` na linha de comando, menos as que
// quem abriu o navegador já citou (ligando ou desligando): essa decide. Pelo
// nome inteiro: "GlicActor" entra mesmo com "GlicActorUi" na linha.
void Juntar(base::CommandLine* command_line,
            const char* switch_name,
            base::span<const char* const> flags) {
  const std::set<std::string, std::less<>> pedidas =
      Nomes(command_line->GetSwitchValueASCII(switches::kDisableFeatures) +
            "," + command_line->GetSwitchValueASCII(switches::kEnableFeatures));
  std::string lista = command_line->GetSwitchValueASCII(switch_name);
  for (const char* flag : flags) {
    const std::string_view nome =
        std::string_view(flag).substr(0, std::string_view(flag).find(':'));
    if (pedidas.contains(nome)) {
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

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kCongelarOutrosEspacosPref, false);
  registry->RegisterBooleanPref(kRolagemComOMeioPref, false);
}

static bool LigadaNoLocalState(PrefService* local_state, const char* pref) {
  return local_state && local_state->FindPreference(pref) &&
         local_state->GetBoolean(pref);
}

void LigarRecursos(base::CommandLine* command_line, PrefService* local_state) {
  Juntar(command_line, switches::kEnableFeatures, kLigadas);
  Juntar(command_line, switches::kDisableFeatures, kDesligadas);
  // Abas dos outros espaços congeladas (paradas, sem rodar script), depois
  // de 15 min sem uso. O Chromium não congela as que tocam som, estão numa
  // chamada, mandam arquivo, seguram um Web Lock ou têm permissão de
  // notificação.
  if (LigadaNoLocalState(local_state, kCongelarOutrosEspacosPref)) {
    constexpr const char* kCongelar[] = {
        "TabGroupsFocusing:TabGroupsFocusFreezing/true",
        "CPUMeasurementInFreezingPolicy:freezing_visible_protection_time/15m",
    };
    Juntar(command_line, switches::kEnableFeatures, kCongelar);
  }
  // Botão do meio fora de link e de campo: rola a página pra onde o
  // ponteiro for (o Chromium só liga no Windows; no Linux o meio também cola
  // a seleção, e nos campos continua colando).
  if (LigadaNoLocalState(local_state, kRolagemComOMeioPref)) {
    constexpr char kBlink[] = "enable-blink-features";
    std::string lista = command_line->GetSwitchValueASCII(kBlink);
    if (!Nomes(lista).contains("MiddleClickAutoscroll")) {
      lista += lista.empty() ? "MiddleClickAutoscroll" : ",MiddleClickAutoscroll";
      command_line->AppendSwitchASCII(kBlink, lista);
    }
  }
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
