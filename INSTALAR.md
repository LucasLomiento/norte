# Instalar o Norte

O pacote é montado a partir da compilação já feita: o `out/release`
(binário único, abre a janela em ~0,4 s) se existir, senão o `out/dev`
(mesma otimização, só dividido em bibliotecas, ~0,8 s). Ver
[docs/ARQUITETURA.md](docs/ARQUITETURA.md).

```sh
cd ~/Work/github/pessoal/navegador/pacote
makepkg -f                                  # monta o navegador-*.pkg.tar.zst
sudo pacman -U navegador-*.pkg.tar.zst      # instala (pede senha)
```

(O pacote ainda se chama `navegador`, o nome antigo; o programa é o Norte.)

Depois, `norte` no terminal ou "Norte" no menu de apps. Na primeira vez
aparece a tela de boas-vindas, onde dá pra deixar ele como navegador padrão.
Versões novas aparecem num aviso no pé da lateral (e em Configurações do
Norte → Geral → Atualizações), que baixa, instala com a senha do sistema e
reinicia.

## Onde fica cada coisa

- Programa: `/usr/lib/norte`; comando: `/usr/bin/norte` (o `navegador`
  continua, pra quem veio de antes da versão 21).
- Perfil: `~/.config/norte` (separado do Chromium e do Zen).
- Flags extras (uma por linha): `~/.config/norte-flags.conf`. O lançador já
  liga o teclado japonês no Wayland (`--enable-wayland-ime`), as senhas no
  chaveiro do GNOME, o gesto de voltar no touchpad e as extensões do Omarchy
  (Alt+Shift+L copia o endereço, Alt+Shift+D baixa o vídeo). Um
  `--enable-features` ou `--disable-features` no arquivo entra junto com os
  do lançador.
- Erros (aberto pelo menu de apps, sem terminal): `journalctl -t norte`.
- Extensões MV2 (o uBlock Origin completo, carregado sem compactar em
  `chrome://extensions`): `--norte-mv2` no `norte-flags.conf`. Sem garantia:
  uma versão nova do Chromium pode tirar o resto do suporte.
- Políticas (se um dia precisar): `/etc/norte/policies/managed/*.json`.
- Idioma: português por padrão; `NORTE_IDIOMA=en-US norte` troca (os textos
  próprios do Norte continuam em português).

## Menu e perfis

- O menu do navegador fica no logo, no canto de cima da lateral (como no
  Dia); `Alt+F` também abre. Clique direito no logo: recolher/abrir a
  lateral (ou `Ctrl+S`).
- Lateral recolhida some inteira; encostar o mouse na borda esquerda faz ela
  aparecer por cima da página (como a lateral flutuante do Dia).
- Perfis: menu do logo → o submenu com o nome do perfil → outros perfis,
  "Adicionar novo perfil", visitante e "Gerenciar perfis". Cada perfil abre
  na própria janela; ao abrir o navegador, volta o último perfil usado.
- Containers: Configurações do Norte → Containers (criar, renomear, trocar a
  cor, excluir); "Abrir em container" no menu da aba e do link.

## Espaços, fixados e configurações

- **Configurações do Norte**: `Ctrl+,` ou menu do logo → "Configurações do
  Norte…". Páginas: Geral, Privacidade, Aparência, Lateral, Espaços,
  Containers e Atalhos. As configurações do Chromium ficam em Geral → Mais
  configurações.
- **Espaços** (como os workspaces do Zen): cada janela está num espaço e
  mostra só as abas dele; aba nova nasce no container do espaço. Trocar: nome
  do espaço no alto da lateral, botões laterais do mouse com o ponteiro em
  cima da lateral, rolagem de lado na lateral, as bolinhas do pé ou os
  atalhos. Menu da aba → "Mover para o espaço". Clique direito no pé da
  lateral: fechar as abas do espaço, fechar as repetidas, copiar os
  endereços, descarregar os outros espaços.
- **Favoritos, sites fixados e pastas** são do espaço (iguais em toda janela
  nele, e ficam ao fechar o navegador). Menu da aba → "Adicionar aos
  favoritos" (ladrilho no alto) ou "Fixar no espaço" (fora das pastas, numa
  pasta ou numa pasta nova). Clicar abre o site; o "x" fecha só a aba.
  Clique direito num site ou pasta: renomear, voltar ao endereço fixado,
  usar o endereço atual, mover, desafixar, excluir a pasta.
- **Levar pra outra máquina**: Configurações do Norte → Geral → Trazer de
  outro navegador → Exportar/Importar (espaços, fixados, containers, atalhos
  e opções num arquivo).
- **Links de outros apps no espaço certo**: Editar espaço → Sites
  (`github.com, figma.com`): um link desses sites vindo do Discord, do Slack
  ou do terminal abre nesse espaço. `norte --space=<nome> <endereço>` abre
  no espaço escolhido.
- **Comandos na barra**: `>` e o nome da ação (`> foco`, `> trabalho`,
  `> container banco`); Enter roda o primeiro. Uma conta (`2+3*4`) mostra o
  resultado na lista; escolher copia.
- **Estilo por site**: clique direito na página → "Estilo deste site…":
  modo escuro só nele e um CSS próprio, por cima do CSS do site.
- **Container temporário**: "Novo container temporário" nos menus de
  container; some (com os cookies) quando a última aba dele fecha.
- **Aba seguidora**: com a tela dividida, menu da aba → Organizar a
  visualização dividida → "Links desta aba abrem na outra metade".

## Atalhos próprios (padrão; mudam nas Configurações)

- `Ctrl+S`: mostrar/ocultar a lateral; `Ctrl+Shift+S`: a barra de endereço;
  `Ctrl+Alt+C`: as duas juntas (modo foco).
- `Ctrl+Alt+→` / `Ctrl+Alt+←`: próximo / anterior espaço.
- `Ctrl+Alt+1` … `Ctrl+Alt+9`: ir pro espaço 1 a 9.
- `Ctrl+Alt+N`: novo espaço.
- `Ctrl+Alt+V` / `Ctrl+Alt+H`: dividir a tela lado a lado / em cima e
  embaixo (de novo desfaz); `Ctrl+Alt+X` troca os lados.
- `Ctrl+Shift+C`: copiar o endereço da aba, sem os parâmetros de rastreio.
- `Shift`+clique num link: prévia num cartão por cima da aba.
- `Ctrl+,`: Configurações do Norte.
- Sem tecla de fábrica (dá pra escolher em Atalhos): renomear a aba, fixar no
  espaço, pôr nos favoritos, fechar as abas do espaço.

## Netflix

O DRM (Widevine) é baixado sozinho cerca de um minuto depois da primeira
abertura, e o Chromium no Linux só carrega ele na abertura seguinte: na
primeira vez, abrir o navegador, esperar um minuto, fechar e abrir de novo
(testado no pacote em 15/10: depois de reabrir, o Widevine responde). No
Linux o Netflix limita a resolução em qualquer navegador.
