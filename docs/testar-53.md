# Como testar a versão 53

Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-53-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Numa nova aba, clicar fora da barra, depois apertar o botão do mouse na barra e segurar | Tudo muda de uma vez, ainda segurando: o G vai pra esquerda (aparecendo devagar), as sugestões abrem e o "Pesquise no Google ou digite o URL" continua lá. Sem anel azul |
| Soltar o botão | Nada mais se mexe |
| Comparar as bordas da caixa antes e depois do clique | A caixa não cresce: as sugestões descem por baixo, com as mesmas bordas da barra |
| Olhar o G da barra e os ícones das sugestões | Na mesma coluna; o texto também alinhado |
| Digitar algo e apagar tudo | O texto de exemplo volta, com a barra ainda focada |
| Numa página aberta, clicar na barra mexendo um pouquinho o mouse antes de soltar | O endereço inteiro fica selecionado (antes podia selecionar só um pedaço) |
| Arrastar de verdade pelo endereço (mais que uns 8 px) | Seleciona o trecho arrastado, como antes |
| Esc com as sugestões abertas | As sugestões fecham, a barra continua focada (aí sim com o anel), sem o G pular |
