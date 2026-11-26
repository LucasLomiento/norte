# Como testar a versão 69: Chromium 158

Os testes automáticos rodaram numa tela virtual (X11, sem aceleração), comparando a -68 e a -69 no mesmo perfil:

- os dados de um site gravados pela -68 (localStorage, cookie, IndexedDB e Cache Storage) abrem iguais na -69;
- 50 passos com captura de tela (claro e escuro): trocar de espaço nos dois sentidos, botões do mouse, menus do logo, da aba e do "+ Nova aba", `Ctrl+S`, a borda, a barra escondida, o modo foco, a aba nova, as Configurações, a Memória, pastas e subpastas, abrir e fechar fixados, arrastar aba pros fixados e a tela dividida. Tudo igual à -68;
- o `Ctrl+S` gravado a 60 quadros por segundo, recolhendo e abrindo: a mesma curva, sem faixa na borda direita.

Única diferença achada: trocando de espaço pelo teclado com o mouse parado em cima da lateral, a -68 mostrava a dica do item embaixo do mouse; a -69 não mostra (ou mostra "New tab"). Mexendo o mouse, a dica certa aparece.

Este roteiro é o que só dá para ver numa sessão de verdade.

**Antes de instalar, guarde uma cópia do perfil.** Um perfil aberto na 158 não volta a abrir direito na 157. Com o Norte fechado:

```
rm -rf ~/.config/norte.antes-do-157
cp -a ~/.config/norte ~/.config/norte.antes-do-158
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-158.0.8127.88-69-x86_64.pkg.tar.zst
```

(A primeira linha apaga a cópia guardada antes do 157, de 21/10, que não serve mais.)

Feche o Norte inteiro e abra de novo. Se algo der errado, para voltar:

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-157.0.8103.38-68-x86_64.pkg.tar.zst
rm -rf ~/.config/norte && mv ~/.config/norte.antes-do-158 ~/.config/norte
```

## Os seus dados

| O que fazer | O que deveria acontecer |
|---|---|
| Abrir um site em que você está logado, em cada container | Continua logado |
| Abrir o preenchimento de uma senha salva | A senha está lá (o teste automático não usou o chaveiro) |
| Olhar os espaços, as pastas fixadas e os favoritos | Tudo como antes |
| Abrir um site com dados locais (WhatsApp Web, Discord) | Continua configurado |
| `Ctrl+Shift+F` e buscar algo que você leu | A Memória acha |

## Wayland e Hyprland (não dá para simular)

| O que fazer | O que deveria acontecer |
|---|---|
| `Ctrl+S` várias vezes, olhando a borda direita da página | Desliza sem tremer. No 158 o recurso que segurava isso sumiu; agora é uma mudança nossa no código |
| Abrir um vídeo em janelinha (PiP) por cima de um balão ou menu | Não some nem pisca. O Chromium corrigiu isso no 158 e a nossa correção saiu |
| Olhar a lateral e a barra com o vidro ligado | Sem faixa escura e sem listras |
| Arrastar uma aba para fora, para outra janela e para a borda da página | Sai só ao soltar; não trava; divide a tela na borda |
| Trocar de espaço com os botões do mouse e com `Ctrl+Alt+←/→` | Troca na hora, sem aba deslizando |
| Rolar um vídeo em tela cheia por um tempo | A tela não congela |

## Recursos que dependem de algo de fora

| O que fazer | O que deveria acontecer |
|---|---|
| Netflix ou Spotify | Toca |
| Extensão do Claude | Funciona, com o cartão e o espaço laranja |
| 1Password | A extensão conversa com o app |
| Compartilhar a tela numa chamada | A tela aparece |
| Abrir um site com anúncio | O bloqueador esconde (as abas novas e as que voltam da memória continuam sendo acompanhadas: no 158 isso mudou de lugar no Chromium) |
| Espaço temporário: abrir, usar e fechar | Some com os dados dele |
