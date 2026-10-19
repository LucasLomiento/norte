# Como testar a versão 31

O jeito de usar: faça cada teste **antes** na versão que você tem agora (a -30) e veja o problema. Depois instale a -31 e repita para ver que sumiu.

Instalar a -31 (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-31-x86_64.pkg.tar.zst
```

Depois de instalar, feche o Norte inteiro e abra de novo.

---

## 1. Novo na -31: antes na -30 e depois na -31

### 1.1 Janela anônima sem espaços, containers nem fixados

Abra uma janela anônima com **Ctrl+Shift+N**.

| O que fazer | Antes (-30) | Depois (-31) |
|---|---|---|
| Olhar o alto da lateral | Nome de um espaço ("Pessoal"); clicar abre a lista de espaços | Diz **"Anônima"** e não abre nada |
| Olhar o pé da lateral | Bolinhas dos espaços e o "+" | Só o botão de downloads |
| Olhar acima das abas | Quadro tracejado dos favoritos e os fixados do espaço | Nada: só as abas |
| Abrir umas abas (Ctrl+T) | Abas dentro do espaço | Abas soltas, como num Chromium comum |
| **Ctrl+Alt+→** | Troca de espaço (abre uma aba no outro espaço) | Não faz nada de espaço (a tecla vai pra página) |
| Clique direito numa aba | "Mover para o espaço", "Fixar no espaço", "Adicionar aos favoritos" | Nenhum dos três; aparece o **"Fixar"** normal |
| Fechar todas as abas (Ctrl+W até a última) | A janela não fecha: abre uma "Nova guia" | **A janela anônima fecha** |
| Botões voltar/avançar do mouse com o ponteiro na lateral | Trocam de espaço | Voltam/avançam a página, como sempre |

O mesmo vale para a janela de **Visitante** (menu do logo → perfil → Visitante): lá o cabeçalho diz "Visitante".

Nada da janela anônima vai mais para as preferências do perfil (espaços, grupos, fixados).

### 1.2 "Norte" no lugar de "Chromium" em mais textos

| O que fazer | Antes (-30) | Depois (-31) |
|---|---|---|
| Abrir uma janela anônima e ler a página da nova aba | "O **Chromium** não vai salvar:" | "O **Norte** não vai salvar:" |
| Abrir https://untrusted-root.badssl.com/ e clicar em "Avançado" | O texto fala do Chromium | Fala do Norte |

No total, 42 textos trocaram de "Chromium" para "Norte": aba que travou, avisos de certificado, gerenciador de senhas, preenchimento de cartão e endereço, escolha do buscador. Todos com a tradução em português mantida.

### 1.3 Favoritos sem repetição

1. Abra o mesmo site em **duas** abas (por exemplo, `https://github.com` duas vezes).
2. Na primeira, clique direito → **Adicionar aos favoritos**.
3. Na segunda, a mesma coisa.

| | Antes (-30) | Depois (-31) |
|---|---|---|
| Ladrilhos no alto da lateral | **Dois** ladrilhos iguais do GitHub | **Um** só (a segunda aba continua aba comum) |

O mesmo acontecia sem você fazer nada quando uma extensão fixa abas (o Claude in Chrome, ou qualquer extensão que use `chrome.tabs.update({pinned: true})`). Cada vez virava um favorito novo. No teste de estresse, isso deu 352 ladrilhos para 7 sites.

### 1.4 Fixados e favoritos rolam, e a lista de abas não some

Para ver o problema sem precisar de 30 fixados, deixe a janela **baixinha** (meia tela na vertical, ou bem pequena se for flutuante).

1. Fixe uns 10 a 15 sites (clique direito na aba → Fixar no espaço → Fora das pastas), ou abra uma pasta com muitos sites.
2. Tenha também várias abas abertas no espaço.

| | Antes (-30) | Depois (-31) |
|---|---|---|
| Lista de abas | **Some**: os fixados ocupam a lateral inteira | Continua embaixo (com pelo menos 2/5 da altura), rolando sozinha |
| Últimos fixados | Cortados, não dá pra clicar | A área dos fixados **rola com a rodinha** |
| Clicar num fixado depois de rolar | — | Abre normalmente |

---

## 2. Já estão na versão que você tem (-28 a -30)

Para estes você não tem mais o "antes", porque já está na -30. Se quiser ver o bug antigo, instale a -27, teste e volte para a -31:

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-27-x86_64.pkg.tar.zst
```

| Teste | Na -27 (bug) | Agora |
|---|---|---|
| Espaço com **uma aba só** → clique direito → Mover para o espaço → outro | O navegador **fecha inteiro** (crash) | A aba vai e a janela vai junto para o outro espaço |
| Espaço com 3 abas → clique direito na **primeira** → Abrir em container → X | **Crash** (ou a aba é movida de espaço) | A aba reabre no mesmo lugar, no container X |
| Clique direito numa aba → **Recarregar** | Fixava a aba no espaço em vez de recarregar | Recarrega |
| Clique direito → **Duplicar** | Abria "Nova pasta…" | Duplica |
| Aba num container → clicar num link que abre em aba nova (`target=_blank`, por exemplo um link externo no Gmail/Slack) | A aba nova abria **fora** do container, com a conta da partição normal | A aba nova fica no mesmo container (com a barrinha colorida) |
| Excluir um container que não foi usado desde que o Norte abriu | Os dados (logins, cookies) ficavam no disco | São apagados |
| Configurações do Chromium → Limpar dados de navegação → cookies, todo o período | Os containers continuavam logados | Os containers também são limpos |
| Login com Google/GitHub num popup que fecha sozinho no fim | Aparecia uma "Nova guia" sobrando na janela | Não aparece |
| **Shift+Alt+W** | Fechava todas as abas do espaço, sem perguntar | Não faz nada |
| Deslizar dois dedos de lado no touchpad em cima da lateral | Pulava dois espaços | Troca um espaço por gesto |
| Deixar um cronômetro/upload num espaço e ficar 6 minutos noutro | A aba congelava (o cronômetro parava) | Continua rodando |
| Barra escondida (Ctrl+Shift+S) → **Ctrl+K** | Nada aparecia, e o que se digitava ia pra página | A barra desce, pronta para pesquisar |
| Barra escondida → ponteiro no alto → abrir o 1Password ou o cadeado do site | A barra subia e o balão pulava junto | A barra fica até o balão fechar |
| Configurações → Atalhos → tentar gravar **Shift+A** (-30) | Aceitava, e o "A" maiúsculo parava de funcionar nas páginas | Recusa (precisa de Ctrl, Alt ou Super) |
| Clique duplo sem querer no nome de uma aba e clicar fora | O título congelava (o contador de não lidas do Gmail parava) | Nada muda |
| Ctrl+Shift+T depois de fechar a aba de um site fixado pelo "x" | Não reabria | Reabre, ligada ao fixado |
