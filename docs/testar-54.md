# Como testar a versão 54: a Memória

Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-54-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo. A Memória começa vazia: ela guarda o texto das páginas a partir de agora (o que já está no histórico não tem o texto).

## Guardar

| O que fazer | O que deveria acontecer |
|---|---|
| Abrir umas páginas de leitura (um artigo, uma issue do GitHub, uma resposta do Stack Overflow, uma página da Wikipédia) e ficar uns segundos em cada | Elas entram. Nada aparece na tela: é por trás |
| Abrir uma página com campo de senha (um login) | Não entra |
| Abrir o banco, o Gmail, o WhatsApp Web | Não entram (o banco e o e-mail pedem pra não ser guardados; e-mail e conversas ficam de fora de fábrica) |
| Abrir a mesma página numa janela anônima (Ctrl+Shift+N) | Não entra, e lá o Ctrl+Shift+F só explica que a janela anônima fica de fora |
| Ctrl+clique num link (aba de fundo) e não olhar pra ela | Só entra quando você olhar pra ela por uns segundos |

## Buscar

| O que fazer | O que deveria acontecer |
|---|---|
| Ctrl+Shift+F | O painel da Memória desce por cima da página (a de trás escura e desfocada), com a linha do tempo: o que você leu hoje, ontem, com a miniatura de cada página |
| Digitar uma frase ou um termo do meio de um artigo que você leu (sem acento, ou só o começo da palavra: "japo" acha "japonês") | Aparecem as páginas com o trecho em que as palavras estão, marcadas; à direita, a miniatura grande, quando você viu, quantas vezes e por quanto tempo leu |
| Digitar em japonês (Ctrl+Espaço) uma palavra de uma página em japonês | Acha também |
| ↑↓ e Enter | Abre a página numa aba nova já rolada até o trecho, com ele destacado em roxo |
| Enter num resultado que já está aberto numa aba ("Aberta" na pílula) | Vai pra aba e seleciona o trecho |
| Shift+Enter / Ctrl+Enter | Abre na aba atual / numa aba de fundo (o painel fica pra abrir outras) |
| Tab | Troca o período: Tudo, Hoje, 7 dias, 30 dias. "Só em <espaço>" deixa só o que você leu nesse espaço |
| Shift+Delete, ou o botão direito num resultado → Esquecer esta página | A página some da Memória |
| Esc, clicar fora ou trocar de aba | O painel some e o foco volta pra página |

## Na barra de endereço

| O que fazer | O que deveria acontecer |
|---|---|
| Ctrl+L e digitar 3+ letras de algo que só está no texto de uma página lida | Até 3 sugestões com o título da página e o trecho, com as palavras em negrito; escolher abre no trecho |
| A última sugestão, "Buscar “…” na Memória" | Abre o painel com o texto já digitado |
| Shift+Delete numa sugestão da Memória | Esquece a página |

## Controle

| O que fazer | O que deveria acontecer |
|---|---|
| Botão direito numa página → "Não lembrar deste site" | O que estava guardado do site sai, e ele não entra mais (o mesmo item vira "Voltar a lembrar deste site") |
| Ctrl+, → Memória | Ligar/desligar, por quanto tempo guardar (3 meses de fábrica), e-mail e conversas, sites e containers que ficam de fora, quantas páginas e quanto espaço, "Apagar tudo…" (pede um segundo clique) |
| Ctrl+Shift+Del → apagar o histórico da última hora | As mesmas páginas saem da Memória |
| Menu do logo | "Memória", logo abaixo do Histórico, com o atalho |
| Configurações → Geral | A antiga seção "Memória" (congelar as abas dos outros espaços) agora se chama "Desempenho" |
