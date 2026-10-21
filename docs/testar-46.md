# Como testar a versão 46

Botões das permissões com vídeo em janelinha. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-46-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Pôr um vídeo do YouTube em janelinha (o botão de janelinha do player, ou trocar de aba com ele tocando) | A janelinha do vídeo abre |
| Com ela aberta, ir em https://permission.site e clicar em "Notifications" | Aparece a janelinha "permission.site quer mostrar notificações" |
| Clicar em "Bloquear" | A janelinha de permissão fecha (antes, só o X fechava) |
| Repetir com "Permitir" (ou com "Location", "Camera") | Fecha também |
| Sem vídeo em janelinha | Igual, como já era |
