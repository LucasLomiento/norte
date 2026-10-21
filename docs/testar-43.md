# Como testar a versão 43

A -43 traz o resto das funções da pesquisa: comandos e calculadora na barra, links de outros apps no espaço certo, estilo por site, container temporário, aba seguidora, notificações em container e o aviso da aba ativa fora de vista. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-43-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

---

## 1. Comandos na barra

| O que fazer | O que deveria acontecer |
|---|---|
| Ctrl+L e digitar `>` | A lista das ações do Norte (lateral, barra, espaços, dividir, containers...), com a agulha do Norte |
| `> foco` e Enter | O modo foco liga (lateral e barra somem); Ctrl+Alt+C volta |
| `> trabalho` (ou o nome de um espaço seu) e Enter | A janela vai pro espaço |
| `> container banco` e Enter | Uma aba nova no container Banco |
| Digitar `>` e qualquer coisa | Nada disso vai pras sugestões do Google (só a busca do que foi digitado aparece, se você escolher) |

## 2. Calculadora

| O que fazer | O que deveria acontecer |
|---|---|
| Digitar `2+3*4` na barra | Uma linha "2+3*4 = 14" na lista; escolher ela copia o 14 (aviso "Copiado") |
| `15% de 80`, `2^10`, `(10-2)/3`, `2,5*4` | 12, 1024, 2,66666666667, 10 |
| Uma data (`2026-10-21`) ou telefone | Sem conta |

## 3. Links de outros apps no espaço certo

| O que fazer | O que deveria acontecer |
|---|---|
| Configurações do Norte → Espaços → editar um espaço → Sites: `github.com` | — |
| Clicar num link do GitHub no Discord, no Slack ou no terminal | Abre nesse espaço, e a janela vai pra ele |
| No terminal: `norte --space=Pessoal https://github.com` | Abre no Pessoal, mesmo com a regra |

## 4. Estilo por site

| O que fazer | O que deveria acontecer |
|---|---|
| Clique direito numa página → "Estilo deste site…" | O editor do site |
| Marcar "Modo escuro neste site" e Salvar | O site fica escuro (o automático do Chromium); outros sites continuam como eram |
| Escrever um CSS (ex.: `body { font-size: 18px }`) e Salvar | Vale em todas as páginas do site, por cima do CSS dele |

## 5. Containers e divisão

| O que fazer | O que deveria acontecer |
|---|---|
| Menu da aba → Abrir em container → "Novo container temporário" | A aba reabre num container cinza "Temporário 1" |
| Fechar a última aba dele | O container some (e os cookies dele) |
| Dividir a tela (Ctrl+Alt+V com duas abas), menu da aba → Organizar a visualização dividida → "Links desta aba abrem na outra metade" | Clicar num link de uma metade abre na outra |
| WhatsApp Web, Gmail ou Calendar num container, com notificação liberada | As notificações aparecem, e clicar nelas leva pro site (antes, num container, o Chromium negava a notificação) |

## 6. Lateral

| O que fazer | O que deveria acontecer |
|---|---|
| Com muitas abas, rolar a lista até a ativa sair de vista | Um aviso no alto (ou embaixo) mostra a ativa; clicar rola até ela |
