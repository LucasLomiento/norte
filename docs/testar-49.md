# Como testar a versão 49

Lateral escondida não abre sozinha em outra janela. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-49-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Uma janela do Norte no DP-2 (direita) e outra no DP-1 (esquerda), as duas com a lateral escondida. Com o ponteiro na janela do DP-2, ir pra janela do DP-1 e encostar na borda esquerda dela | Só a lateral da janela do DP-1 abre (antes, a do DP-2 abria junto) |
| O mesmo, várias vezes, rápido e devagar | A do DP-2 nunca abre sozinha |
| Duas janelas no mesmo monitor, lado a lado: sair da da direita pela esquerda, entrando na outra | A da direita não abre |
| Uma janela encostada na esquerda do DP-1 (o canto de verdade da tela): jogar o ponteiro pra esquerda, além da borda | A lateral abre, como antes (um tiquinho depois: 0,1 s) |
| Encostar na borda esquerda de qualquer janela com o ponteiro dentro dela | Abre na hora, como sempre |
