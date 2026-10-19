# Como testar a versão 33

Faça cada teste **antes**, na versão que você tem agora (a -32), e depois instale a -33 e repita.

Instalar a -33 (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-33-x86_64.pkg.tar.zst
```

Depois de instalar, feche o Norte inteiro e abra de novo. Pra voltar pra -32, é o mesmo comando com `-32` no nome.

---

## 1. Tela congelada se destrava sozinha

O bug: depois da atualização do driver da NVIDIA (615), a tela do Norte às vezes congela. O vídeo para, redimensionar não redesenha e a lateral não abre, mas o navegador continua vivo (o clique dá play no vídeo).

O que mudou: um vigia percebe quando a janela fica parada esperando o Hyprland enquanto você mexe nela. Depois de 5 s sem resposta, ele reinicia a parte gráfica do Norte, como o `chrome://gpucrash` que destravou da outra vez.

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| Quando congelar, continuar mexendo o mouse ou clicando na janela | Fica congelada até você abrir o `chrome://gpucrash` | Em uns 5 s a tela pisca e volta, com as abas e os vídeos no lugar |
| Usar normal, com a tela desligada ou a janela noutro workspace | Nada | Nada (o vigia só age com você mexendo na janela) |

Não dá pra provocar o congelamento de propósito. Se acontecer, me conte se voltou sozinho.

O vigia age no máximo uma vez a cada 5 min: o Chromium desliga a aceleração da GPU se ela cair 3 vezes seguidas. Se congelar duas vezes em menos de 5 min, o `chrome://gpucrash` continua valendo.

Teste opcional, pra descobrir a causa: abrir o Norte por um terminal com o "explicit sync" desligado e usar assim por um tempo.

```
NORTE_SEM_SYNCOBJ=1 norte
```

Se nunca congelar assim, a culpa é desse mecanismo (o driver novo com o Hyprland).

## 2. "Tocando agora" no pé da lateral

Abra uma música ou um vídeo (YouTube, Spotify no navegador, SoundCloud) e vá pra outra aba, ou pra outro espaço.

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| Olhar o pé da lateral | Nada (só um botão de mídia no canto da barra) | Um cartão com a capa, o título, o artista e o andamento |
| Clicar em ⏸ / ▶ no cartão | — | Pausa e volta a tocar, sem sair da aba em que você está |
| Clicar em ⏭ (playlist do YouTube, Spotify) | — | Próxima faixa (o botão só aparece quando o site tem próxima) |
| Clicar na linha do andamento | — | Pula pra aquele ponto (quando o site deixa) |
| Clicar no cartão, fora dos botões | — | Vai até a aba, trocando de espaço se precisar |
| Voltar pra aba que está tocando | — | O cartão some (a mídia já está na sua frente) |
| Olhar o canto direito da barra com mídia tocando | Botão de mídia do Chromium | Não aparece mais (o cartão faz o papel dele) |

## 3. Prévia de link (Shift + clique)

É o Glance do Zen ou o Peek do Arc.

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| **Shift + clique** num link | Abre uma janela nova | Abre a página num cartão por cima da aba; a de trás fica escura e desfocada. No alto do cartão, o ícone, o título e o endereço do site |
| **Esc**, clicar fora do cartão ou o **x** | — | Fecha a prévia e você continua na página de antes |
| Botão **Abrir como aba** (o do meio) | — | Vira uma aba ao lado, no mesmo espaço, sem recarregar (o histórico da prévia vai junto) |
| Botão **Dividir** (o primeiro) | — | Vira aba e divide a tela com a página de antes |
| Ctrl + clique num link dentro da prévia | — | Abre em aba nova e fecha a prévia |
| Shift + clique num link de uma aba de container | — | A prévia abre no mesmo container (logada como a aba) |
| Configurações (Ctrl+,) → Geral → Links → desligar "Prévia com Shift + clique" | — | Shift + clique volta a abrir janela nova |

## 4. Copiar o endereço (Ctrl+Shift+C)

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| **Ctrl+Shift+C** numa página | Abre o DevTools no modo inspecionar | Copia o endereço da aba e mostra "Link copiado" num aviso escuro |
| Configurações → Atalhos | — | "Copiar o endereço da aba" na lista, pra trocar a tecla se quiser |

O DevTools continua no **F12** e no **Ctrl+Shift+I**.

## 5. Troca de espaço animada

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| Trocar de espaço (Ctrl+Alt+→/←, bolinhas, deslizar no touchpad) | A lista troca de uma vez | A lista entra deslizando de lado (da direita indo pra frente, da esquerda voltando) e aparecendo, em ~0,2 s |

## 6. Barras de rolagem finas

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| Abrir uma página comprida (Wikipédia) e olhar a borda direita | Barra cinza com setinhas, sempre à vista, ocupando uma faixa da página | Nada parado; rolando aparece uma barra fina, e com o ponteiro em cima ela engrossa |

## 7. Fechar as abas do espaço

| O que fazer | Antes (-32) | Depois (-33) |
|---|---|---|
| Clicar no nome do espaço (alto da lateral) → **Fechar as abas deste espaço** | Não existia | Fecha as abas do espaço; os sites fixados e os favoritos ficam, e o espaço fica com uma aba nova |
| Logo depois, **Ctrl+Shift+T** | — | As abas voltam, uma por vez |
| Os outros espaços | — | Não são mexidos |

Também está no clique direito nas bolinhas do pé da lateral.
