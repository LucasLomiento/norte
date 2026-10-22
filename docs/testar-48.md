# Como testar a versão 48

Container dos apps instalados (PWA). Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-48-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Numa aba de um container (Trabalho, por exemplo), entrar num site que dá pra instalar como app e instalar (ícone de instalar na barra) | A janela do app abre logada como na aba (no mesmo container) |
| Fechar o app e abrir de novo (pelo lançador do Omarchy ou pelo Norte) | Abre no mesmo container, mesmo que a aba atual do Norte seja de outro |
| Na janela do app, menu "⋮" → Container | Mostra o container atual ao lado; o submenu lista os outros, "Sem container" e "Gerenciar containers…" |
| Escolher outro container no submenu | A janela do app recarrega no mesmo endereço, agora no container novo (com o login de lá) |
| Configurações do Norte (Ctrl+,) → Containers → Apps instalados | O app aparece com o container dele; trocar ali vale igual ao menu (e a janela aberta acompanha) |
| App que já estava instalado antes desta versão | Abre como antes (sem container) até você escolher um |
