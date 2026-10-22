# Como testar a versão 47

Lateral escondida sem a faixa na borda. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-47-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Com a moldura em 0 (Configurações do Norte → Aparência), esconder a lateral (Ctrl+S) | A página vai até a beira da janela, sem a faixa escura de 3 px na esquerda |
| Encostar o ponteiro na beira esquerda da janela | A lateral aparece por cima da página, como antes |
| Ctrl+S de novo (abrir) e de novo (esconder) | A página estica e encolhe numa direção só (com a moldura acima de 3, ela voltava uns pixels no fim de esconder e no começo de abrir) |
| Moldura em 3 ou mais | Igual a antes: a mesma margem nos quatro lados |
