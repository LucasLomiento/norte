# Como testar a versão 68: revisão geral (dados, Memória, desempenho, visual)

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-157.0.8103.38-68-x86_64.pkg.tar.zst
```

Feche o Norte inteiro e abra de novo.

Tudo abaixo foi testado na tela virtual (Xvfb). O que só dá pra ver no seu Hyprland está no fim.

## Dados e containers

| O que fazer | O que deveria acontecer |
|---|---|
| Configurações (`Ctrl+,`) → Containers, clicar na lixeira de um container | No lugar dela aparece "Excluir e apagar os dados" por 5 s; sem clicar, volta a lixeira |
| Com uma aba aberta num container, excluir esse container | A aba reabre no mesmo endereço, sem a barrinha de cor e sem o login |
| Apagar o nome de um container e clicar fora | Volta o nome de antes (nome vazio não é gravado) |
| Com a tela dividida, botão direito num link do painel da esquerda → Abrir link em container | A aba nova entra depois das abas da divisão, e a divisão continua |

## Memória

| O que fazer | O que deveria acontecer |
|---|---|
| Ler uma página, abrir a Memória (`Ctrl+Shift+F`), esquecer a página (`Shift+Delete`) com a aba dela ainda aberta, esperar uns 20 s e trocar de aba | A página não volta pra Memória sozinha |
| Num site, clicar num link e depois Voltar | A página de trás continua sendo lembrada (antes, depois do Voltar a aba parava de entrar na Memória) |
| Apagar um item na página do histórico (`Ctrl+H`) | Sai da Memória também |

## Lateral

| O que fazer | O que deveria acontecer |
|---|---|
| Ter 7 ou mais espaços | No pé, uma fila de bolinhas em volta do espaço atual, com as das pontas menores; o "+" sempre à vista |
| `>` na barra de endereço | Os comandos ficam juntos, sem a busca do Google no meio |
| `> descarregar` com uma aba de outro espaço numa chamada, ou com texto digitado num formulário | Essa aba fica; as outras são descarregadas |
| Clique duplo no nome de uma aba, digitar e `Enter` | O foco volta pra página (as setas e o espaço rolam ela) |
| Botão direito no logo | "Esconder a lateral" e "Mostrar a lateral ao encostar na borda" (marca e desmarca de verdade) |
| Botão direito no vazio da lateral | Sem "Enviar feedback sobre a barra de abas" |
| Lateral escondida (`Ctrl+S`), com a tela dividida, encostar na borda | As abas da divisão aparecem lado a lado, como com a lateral aberta |
| Barra escondida (`Ctrl+Shift+S`), `Ctrl+L` e `Ctrl+Shift+S` de novo | Sem faixa escura no alto da página |

## Aparência

| O que fazer | O que deveria acontecer |
|---|---|
| Configurações → Aparência → Modo: trocar claro/escuro | O ícone de downloads no pé da lateral muda de cor na hora |
| Configurações no escuro | Os botões dentro dos cartões (Importar, Abrir, os atalhos) aparecem como botões |
| Rolar uma página das Configurações | O "X" fica na margem, sem ficar por cima do conteúdo |
| Barra de endereço: `2.500 * 3`, `1.000 + 2`, `0x10`, `21/10/2026` | 7500, 1002, e nada nas duas últimas |

## Só no seu computador

| O que fazer | O que deveria acontecer |
|---|---|
| Duas janelas do Norte, o ponteiro em cima dos fixados de uma | A outra não acende o destaque (nem o "x") na linha da mesma posição |
| Duas janelas em espaços diferentes, arrastar uma aba de uma pra outra | A que recebe continua mostrando os fixados do espaço dela |
| Usar normalmente uns sites pesados (portais de notícia) | Tudo igual, com os anúncios escondidos como antes (as regras de esconder agora ficam guardadas por processo) |
| Com a extensão do Claude em uso | O "Parar" do cartão aparece inteiro, mesmo com "Container do espaço" escrito ao lado |
