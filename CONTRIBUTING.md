# Contribuir com o Norte

Bugs, sugestões e pull requests são bem-vindos, em português, inglês ou espanhol.

*Issues and pull requests are welcome in Portuguese, English or Spanish.*

## Por onde começar

- **Bug:** abra uma [issue de bug](https://github.com/LucasLomiento/norte/issues/new?template=bug.yml).
- **Sugestão:** abra uma [issue de sugestão](https://github.com/LucasLomiento/norte/issues/new?template=sugestao.yml).
- **Dúvida ou ideia ainda solta:** use as [Discussions](https://github.com/LucasLomiento/norte/discussions).
- **Falha de segurança:** não abra nada público, siga o [SECURITY.md](SECURITY.md).

Conserto pequeno, texto e tradução podem ir direto para um PR. Para recurso novo ou mudança grande, combine numa issue antes de escrever o código: compilar o Norte leva horas, e uma decisão de produto pode deixar o PR de fora.

## Como uma mudança entra

1. Faça um fork e crie uma branch a partir da `main`.
2. Abra o PR para a `main` e preencha o modelo. Um assunto por PR.
3. O mantenedor revisa. Ninguém faz push direto na `main`.
4. Commit novo depois de uma aprovação cancela a aprovação, e o PR volta para revisão.
5. O PR entra por squash: vira um commit só, com o título do PR. Escreva um título que descreva a mudança.

Ao enviar uma contribuição, você concorda que ela fica sob a mesma licença do projeto ([BSD-3-Clause](LICENSE)).

## O que guia as decisões

- **Sem IA embutida, sem conta Google e sem estatísticas de uso.** PR que traga qualquer um desses não entra.
- **Mexer o mínimo no Chromium.** Sai versão nova a cada 4 semanas, com correções de segurança, e cada patch é um conflito em potencial. Prefira arquivo novo em `navegador/` com um gancho pequeno no Chromium.
- **Nada copiado de produto fechado.** Dá para se inspirar no comportamento de outro navegador. Código, ícones, imagens e fontes de terceiros só entram com licença compatível e citados no README.

## Preparar o ambiente

O passo a passo está em [Compilar, no README](README.md#compilar). Compilar do zero leva algumas horas e pede uns 16 GB de RAM livres. Mudança só em `site/` ou `docs/` não precisa de nada disso.

Os scripts procuram o Chromium em `~/Work/navegador-chromium/src`. Usar esse caminho evita configurar qualquer coisa; a maioria dos scripts aceita outro pela variável `NAVEGADOR_SRC`.

## Onde fica cada coisa

| Pasta | O que tem |
|---|---|
| `navegador/` | Código novo do Norte, ligado dentro da árvore do Chromium |
| `patches/` | Mudanças em arquivos do Chromium, um patch por área (`patches/SERIES`) |
| `tools/` | Scripts para compilar, testar, empacotar e publicar |
| `docs/` | [ARQUITETURA.md](docs/ARQUITETURA.md) explica como cada parte funciona |
| `pacote/` | Pacote do pacman |
| `site/` | Site do projeto, em português, inglês e espanhol |

Os patches são gerados, não editados à mão: mude a árvore do Chromium e rode `tools/fonte.sh --gerar`. Arquivo que não está em `patches/SERIES` cai no `9999-resto.patch`, então acrescente-o na linha da área certa.

## Testar

- **Sintaxe:** `python3 -I tools/checar.py` confere os `.cc` mudados sem esperar a compilação inteira. Precisa de `gn gen out/dev --export-compile-commands`.
- **Compilar:** `tools/compilar.sh` compila o `out/dev` em prioridade baixa.
- **Na sua sessão:** `tools/testar.sh abrir` abre o Norte compilado com um perfil de teste. Esse script usa o `hyprctl`, então só funciona no Hyprland.
- **Sem tocar na sua sessão:** `tools/xt.py` move o mouse e digita num servidor X virtual (Xvfb).
- **Site:** `node site/build.mjs` gera a página em `site/dist`.

Diga no PR onde você testou. Muita coisa muda entre X11, KDE, Hyprland e NVIDIA, e ninguém consegue testar tudo sozinho. Se mudou como algo funciona, atualize o `docs/ARQUITETURA.md`.

## Conduta

Participar daqui é aceitar o [Código de Conduta](CODE_OF_CONDUCT.md).
