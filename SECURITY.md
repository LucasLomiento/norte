# Segurança

## Como avisar

Não abra issue pública para falha de segurança. Use o [formulário privado do repositório](https://github.com/LucasLomiento/norte/security/advisories/new): só o mantenedor vê o aviso.

Conte a versão do Norte (Configurações do Norte → Geral → Atualizações), os passos para reproduzir e o que um atacante consegue fazer com a falha.

O Norte é mantido por uma pessoa, então não há prazo garantido de resposta. Depois do conserto, a falha é divulgada com o crédito para quem avisou, se a pessoa quiser.

## Versões cobertas

Só a versão mais recente publicada em [norte-releases](https://github.com/LucasLomiento/norte-releases). Antes de avisar, confira se a falha ainda acontece nela.

## Norte ou Chromium?

- **Acontece só no Norte** (lateral, espaços, containers, bloqueador, atualizador, importador): avise aqui.
- **Também acontece no Chromium puro:** avise o [Chromium](https://www.chromium.org/Home/chromium-security/reporting-security-bugs/).

As correções do Chromium só chegam a quem usa o Norte quando sai uma versão nova do Norte com a base atualizada. A versão usada hoje está em [CHROMIUM_VERSION](CHROMIUM_VERSION).
