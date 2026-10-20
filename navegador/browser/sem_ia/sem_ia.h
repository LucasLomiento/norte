// Sem IA: o navegador não tem as funções de IA do Google (pedido do
// usuário). Desliga as flags delas na criação da FeatureList, juntando à
// --disable-features que já existir.

#ifndef NAVEGADOR_BROWSER_SEM_IA_SEM_IA_H_
#define NAVEGADOR_BROWSER_SEM_IA_SEM_IA_H_

namespace base {
class CommandLine;
}  // namespace base

namespace navegador {

void DesligarIA(base::CommandLine* command_line);

// Flags do Chromium que o Norte liga por padrão e experimentos da lista de
// teste do Chromium que ele desliga (ver as listas no .cc). Uma que a pessoa
// já tenha ligado ou desligado na linha de comando fica como ela pediu.
void LigarRecursos(base::CommandLine* command_line);

}  // namespace navegador

#endif  // NAVEGADOR_BROWSER_SEM_IA_SEM_IA_H_
