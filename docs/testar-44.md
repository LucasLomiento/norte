# Como testar a versão 44

Correção do "Nova aba" dobrado e o aviso de aba nova aberta por link. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-44-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Deixar uma aba só na lateral (uma "Nova aba"), Ctrl+T, Ctrl+W | O nome da aba fica normal (antes aparecia dobrado, com uma setinha depois do X) |
| Ctrl+clique num link, ou o botão do meio | A linha da aba nova acende em azul na lateral e apaga em menos de um segundo |
| Clicar num link que abre aba nova sozinho (um `target=_blank`) | A aba nova entra ativa e acende do mesmo jeito |
| Ctrl+S (lateral escondida) e Ctrl+clique num link | Uma pílula escura no pé da página: "Aberta numa aba nova · site" por uns dois segundos; clicar nela vai pra aba |
| Muitas abas, rolar a lista até a ativa sair de vista | O aviso da aba ativa continua aparecendo no alto ou embaixo, e clicar rola até ela |
