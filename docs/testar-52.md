# Como testar a versão 52

Leva a 50 (lateral) e a 51 (ícones), que não foram publicadas: ver docs/testar-50.md e docs/testar-51.md. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-52-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Abrir uma extensão na Chrome Web Store (ex.: chromewebstore.google.com, procurar "uBlock Origin Lite") | O botão diz "Usar no Norte" (em inglês, "Add to Norte") |
| Instalar a extensão e voltar na página dela | "Remover do Norte" |
| Andar pela loja (outra extensão, a busca) sem recarregar | Os botões continuam dizendo Norte |
| Abrir o UOL e outros sites de notícia que você usa | Abrem como antes (a barra de cotações do UOL funcionando); se algum site ficar esquisito, me avisa qual |
