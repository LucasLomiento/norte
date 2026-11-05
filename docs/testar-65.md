# Como testar a versão 65: Chromium 157

Os testes automáticos rodaram numa tela virtual (X11, sem aceleração). Este roteiro é o que só dá para ver numa sessão de verdade.

**Antes de instalar, guarde uma cópia do perfil.** Um perfil aberto na 157 não volta a abrir direito na 155. Com o Norte fechado:

```
cp -a ~/.config/norte ~/.config/norte.antes-do-157
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-157.0.8103.38-65-x86_64.pkg.tar.zst
```

Feche o Norte inteiro e abra de novo. Se algo der errado, para voltar:

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-64-x86_64.pkg.tar.zst
rm -rf ~/.config/norte && mv ~/.config/norte.antes-do-157 ~/.config/norte
```

## Os seus dados

| O que fazer | O que deveria acontecer |
|---|---|
| Abrir um site em que você está logado, em cada container | Continua logado |
| Abrir o preenchimento de uma senha salva | A senha está lá (o teste automático não usou o chaveiro) |
| Olhar os espaços, as pastas fixadas e os favoritos | Tudo como antes |
| Abrir um site com dados locais (WhatsApp Web, Discord) | Continua configurado |

## Wayland e Hyprland (não dá para simular)

| O que fazer | O que deveria acontecer |
|---|---|
| Olhar a lateral e a barra com o vidro ligado | Sem faixa escura e sem listras; a lateral com o mesmo tom |
| `Ctrl+S` e encostar na borda esquerda | Some e aparece deslizando; ao sair, recolhe. A saída foi ajustada na atualização |
| Dois monitores, encostar a borda de uma janela | Só a lateral dessa janela abre |
| Arrastar uma aba para fora, para outra janela e para a borda da página | Sai só ao soltar; não trava; divide a tela na borda |
| Trocar de espaço com os botões do mouse e com `Ctrl+Alt+←/→` | Troca na hora, sem aba deslizando |
| Rolar um vídeo em tela cheia por um tempo | A tela não congela |

## Recursos que dependem de algo de fora

| O que fazer | O que deveria acontecer |
|---|---|
| Netflix ou Spotify | Toca (o Widevine baixa no primeiro uso) |
| Extensão do Claude | Funciona, com o cartão e o espaço laranja |
| 1Password | A extensão conversa com o app |
| Compartilhar a tela numa chamada | A tela aparece |
| Tocar música em outra aba | O cartão "tocando agora" aparece no pé da lateral |

## O atualizador

Com a -65 instalada, abra o Norte quando a próxima versão sair: o aviso aparece no pé da lateral. O botão foi testado em contêiner, com `apt-get` e `pacman`, numa versão falsa.
