# Como testar a versão 50

Lateral escondida não abre sozinha na outra janela. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-50-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Duas janelas do Norte, uma à esquerda e outra à direita, as duas com a lateral escondida. Com o ponteiro na da esquerda, ir pra borda esquerda da da direita | Só a lateral da da direita abre (antes, a da esquerda abria junto) |
| O mesmo com as duas no mesmo monitor e com uma em cada monitor | Igual: só a da direita |
| O caminho contrário (da direita pra borda esquerda da da esquerda) | Só a da esquerda abre |
| Abrir a lateral de uma janela, deixar o ponteiro em cima dela e trocar o foco pra outra janela (atalho do Hyprland) | Ela continua aberta enquanto o ponteiro estiver em cima |
