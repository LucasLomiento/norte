// Sem IA: o navegador não tem as funções de IA do Google (pedido do
// usuário). Desliga as flags delas na criação da FeatureList, juntando à
// --disable-features que já existir.

#ifndef NAVEGADOR_BROWSER_SEM_IA_SEM_IA_H_
#define NAVEGADOR_BROWSER_SEM_IA_SEM_IA_H_

class PrefRegistrySimple;
class PrefService;

namespace base {
class CommandLine;
}  // namespace base

namespace navegador {

// Congelar as abas dos outros espaços (Configurações do Norte → Geral →
// Memória). No Local State: vale ao abrir o navegador, quando a lista de
// recursos é montada (antes das prefs do perfil existirem).
inline constexpr char kCongelarOutrosEspacosPref[] =
    "navegador.congelar_outros_espacos";
// Rolagem automática com o botão do meio (Geral → Rolagem). Também no Local
// State, pelo mesmo motivo.
inline constexpr char kRolagemComOMeioPref[] =
    "navegador.rolagem_com_o_meio";
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

void DesligarIA(base::CommandLine* command_line);

// Flags do Chromium que o Norte liga por padrão e experimentos da lista de
// teste do Chromium que ele desliga (ver as listas no .cc). Uma que a pessoa
// já tenha ligado ou desligado na linha de comando fica como ela pediu.
// `local_state`: as opções do Norte que viram recurso (pode ser nulo).
void LigarRecursos(base::CommandLine* command_line,
                   PrefService* local_state = nullptr);

}  // namespace navegador

#endif  // NAVEGADOR_BROWSER_SEM_IA_SEM_IA_H_
