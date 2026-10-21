# O que falta confirmar no seu computador

Coisas que só dão pra ver no seu Hyprland com a NVIDIA (ou no KDE do seu amigo): nos testes daqui (tela virtual, sem compositor) elas não aparecem. Em cada uma, o que fazer e o que deveria acontecer. Me conte só o que der diferente.

## No seu Hyprland

| Desde | O que fazer | O que deveria acontecer |
|---|---|---|
| -33, -35 | Usar normal por uns dias | A tela não congela mais. Se congelar, continuar mexendo o mouse: em ~5 s ela pisca e volta (o vigia). Se não voltar, `chrome://gpucrash` destrava e me conte |
| -36 | Ctrl+Shift+S (barra escondida), mostrar a barra com o ponteiro, clicar numa extensão (1Password) e tirar o ponteiro da barra | A barra fica enquanto o pop-up está aberto, e o pop-up não vai pro canto esquerdo |
| -11 | Ctrl+S (lateral recolhida) e encostar o ponteiro na borda esquerda | As abas aparecem junto com a lateral, sem chegar atrasadas |
| -7 | Abrir a lateral pela borda e clicar noutra janela (outro app) | A lateral some depois da folga, sem ficar presa aberta |
| -42 | `du -sh ~/.config/norte/{OptGuideOnDeviceModel,SODA,SODALanguagePacks} ~/.config/norte/Default/annotated_page_contents_db 2>/dev/null` | Mostra o que o Chromium baixou ou gravou pra IA antes da -42. Com o Norte fechado, dá pra apagar o `OptGuideOnDeviceModel` e o `annotated_page_contents_db`. O SODA sai sozinho em uns 30 dias (apagar antes faz baixar de novo, nas versões antigas) |
| -42 | Abrir um vídeo 4K do YouTube e olhar `chrome://media-internals` (a linha `kVideoDecoderName` e `kIsPlatformVideoDecoder`) e o `nvidia-smi dmon -s u` (coluna dec) | Me conte o que aparece: se o vídeo estiver no processador (decoder de software e dec em 0), dá pra tentar o da placa por flag |

## No KDE do seu amigo

| O que fazer | O que deveria acontecer |
|---|---|
| Arrastar uma aba pra fora da janela e soltar | Uma janela nova com a aba, sem "janelas infinitas" (não reproduziu aqui no Plasma 6.7) |
| A versão dele: `pacman -Q navegador` | Da -21 em diante, dá pra trocar o nome do pacote pra `norte` sem quebrar o atualizador dele |

## Testado só aqui (tela virtual)

Isto passou nos testes daqui (conferido ao vivo na -42, em 21/10), mas vale reparar se algo der errado no uso normal:

- Aba descarregada (por falta de memória ou pelo "Descarregar os outros espaços") volta no mesmo container, com o nome dado e ligada ao site fixado.
- Pop-up e DevTools solto fecham junto com a última aba deles (Ctrl+W ou o próprio site fechando), sem deixar janela vazia.
- "Limpar dados" (com "Cookies" marcado) e excluir um container apagam os cookies dos containers.
