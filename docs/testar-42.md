# Como testar a versão 42

A -42 junta a primeira leva da pesquisa de melhorias: proteger os dados dos sites, tirar o resto da IA, gastar menos memória e CPU, um monte de acabamento visual e funções novas. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-42-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro (todas as janelas, inclusive as escondidas noutros workspaces) e abra de novo. Pra conferir que é a nova: `ls -la /proc/$(pgrep -of /usr/lib/norte/norte)/exe` não pode dizer "(deleted)".

---

## 1. Os sites continuam logados

O Chromium guardava os dados dos sites (IndexedDB, localStorage) num formato experimental que podia sumir numa atualização. Agora o formato fica travado.

| O que fazer | O que deveria acontecer |
|---|---|
| Abrir WhatsApp Web, Discord e um site em que você está logado | Continuam logados, com as conversas |

## 2. Sem IA

| O que fazer | O que deveria acontecer |
|---|---|
| Digitar `@` na barra de endereço | Os atalhos de busca (`@favoritos`, `@histórico`...) sem o `@gemini` |
| No console de qualquer página (F12): `typeof LanguageModel` | `"undefined"` (antes, `"function"`) |
| `du -sh ~/.config/norte/{OptGuideOnDeviceModel,SODA,SODALanguagePacks} ~/.config/norte/Default/annotated_page_contents_db 2>/dev/null` | O que já tinha sido baixado ou gravado antes da -42. Com o Norte fechado, dá pra apagar o `OptGuideOnDeviceModel` e o `annotated_page_contents_db` |

## 3. Mais leve

| O que fazer | O que deveria acontecer |
|---|---|
| Usar normal | Menos processos e menos memória (no teste daqui, com os mesmos 6 sites abertos na -41 e na -42: 18 → 17 processos, ~1.060 → ~930 MB) |
| Passar o mouse nas abas, ou deixar um chat (WhatsApp, Discord) mudando o título noutro espaço | A janela não é redesenhada inteira a cada mudança (não dá pra ver, mas o vidro atrás gasta menos) |
| Configurações do Norte → Geral → Memória → "Congelar as abas dos outros espaços" (desligado de fábrica) | Ligado (depois de reiniciar), as abas dos outros espaços param de rodar depois de 15 min; música, chamada e sites com notificação continuam |

## 4. Visual

| O que olhar | O que mudou |
|---|---|
| Ctrl+S e encostar na borda: a lateral por cima da página | A opacidade é a da opção (90%), não ~68%; e tem sombra no lado direito |
| Trocar de aba entre sites de cores diferentes | A barra já troca de cor junto com a aba, sem 1 quadro com a cor da anterior |
| Trocar de espaço | A cor da lateral esmaece, em vez do corte seco |
| Passar o mouse nos botões da barra num site claro e no X das abas | O hover aparece |
| Botões secundários ("Cancelar", "Nova pasta…"), interruptores desligados | Cinza, não azulados; o interruptor desligado se vê |
| Menus (logo, aba, página) | Ícones do Lucide no lugar dos do Google (os do menu do logo, do menu da aba e outros) |
| "Nova guia" | Virou "Nova aba" em todo lugar (o Chromium em português dizia "guia") |
| Sugestões da barra | Sem a barrinha azul, a linha escolhida é uma pílula |
| Shift+clique num link (prévia) | Nasce do ponto do clique, sem clarão branco, e fecha esmaecendo |
| Ctrl+Shift+S, mostrar a barra com o ponteiro | Uma sombra curta separa a barra da página |
| As bolinhas dos espaços, no pé | Cor cheia e crescem com o mouse em cima |
| Favoritos (ladrilhos) | O ativo se destaca mais que o hover; um ponto embaixo nos que estão abertos; encolhem um pouco ao apertar |
| Modo claro (Configurações → Aparência → Modo) | Menus e interruptores neutros, sem tingir com a cor do espaço |
| Gesto de voltar no touchpad | Setas no grafite e no azul do Norte |

## 5. Funções novas

| O que fazer | O que deveria acontecer |
|---|---|
| Clique direito num link | "Copiar link limpo" (sem utm, fbclid...) e "Copiar link como Markdown" |
| Ctrl+Shift+C | Copia o endereço da aba sem os parâmetros de rastreio |
| Clique direito na página | "Desligar o bloqueador neste site" (e "Ligar" de novo) |
| Configurações do Norte → Privacidade (página nova) | Bloqueador, avisos de cookies, cookies de terceiros, HTTPS, DNS seguro, IP local |
| Abrir um site com aviso de cookies (um europeu, por exemplo) | O aviso não aparece. Se a página travar a rolagem, desligue o bloqueador nela e me conte qual |
| Clicar no ícone do site (fim da barra) | As informações do site abrem do lado do ícone, não lá no começo |
| Configurações → Geral → Trazer de outro navegador | Importar do Chrome, Brave ou Vivaldi (favoritos e histórico); Exportar/Importar as configurações do Norte num arquivo |
| Configurações → Geral → Mídia | Escolher o vídeo em janelinha ao trocar de aba (perguntar, sempre, nunca) |
| Configurações → Atalhos | "Ctrl+Tab na ordem de uso", modo foco (Ctrl+Alt+C esconde a lateral e a barra juntas), espaços 6–9, e ações sem tecla de fábrica (renomear a aba, fixar, favoritar, fechar o espaço) |
| Clique direito no pé da lateral | Fechar as repetidas, copiar os endereços das abas, descarregar os outros espaços |
| Clique direito num site fixado que mudou de página | "Voltar ao endereço fixado" |
| Passar o mouse na linha entre os fixados e as abas | "Limpar" fecha as abas do espaço (os fixados ficam) |
| Com a lateral e a barra escondidas, abrir uma página lenta | Uma linha fina pulsa no alto da página enquanto carrega |
| Um site com zoom (Ctrl+=) | A lupa do zoom fica acesa na barra parada |
| Desfazer uma divisão aberta pelo atalho sem escolher a outra aba | A aba "escolha uma aba" fecha junto |
| Versão nova disponível | O aviso tem "O que mudou"; e a procura roda a cada 6 h com o Norte aberto |
| Configurações → Geral → Mouse → "Rolar com o botão do meio" (desligado) | Ligado e reiniciado: clicar com o meio na página e mover rola |

## 6. Pra mim saber

- Erros do Norte aberto pelo menu: `journalctl -t norte` (antes, se perdiam).
- Se algo quebrar, a -41 continua instalável com o mesmo comando, trocando o `-42`.
