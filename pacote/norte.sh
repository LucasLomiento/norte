#!/bin/bash
# Lançador do Norte. Flags extras do usuário (uma por linha, # comenta) em
# ~/.config/norte-flags.conf, como o chromium do Arch faz (o
# navegador-flags.conf, do nome antigo, ainda vale).
XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
FLAGS=()
for arquivo in "$XDG_CONFIG_HOME/norte-flags.conf" \
               "$XDG_CONFIG_HOME/navegador-flags.conf"; do
  [[ -f "$arquivo" ]] || continue
  while read -r linha; do
    [[ -z "$linha" || "$linha" = \#* ]] && continue
    FLAGS+=($linha)
  done < "$arquivo"
  break
done
# Interface em português (os textos próprios do Norte são em português).
# Pra trocar: NORTE_IDIOMA=en-US no ambiente.
IDIOMA="${NORTE_IDIOMA:-${NAVEGADOR_IDIOMA:-pt-BR}}"
export LANGUAGE="${IDIOMA/-/_}"

# Quem eu sou pro sistema (definir como padrão, app_id no Wayland). O Chromium
# lê o CHROME_DESKTOP antes do nome embutido, e todo app Electron (Resenha,
# Vesktop, Slack) põe o dele no ambiente: um Norte aberto por um link clicado
# no Resenha se achava "resenha.desktop" e, em 17/10/2026, o "definir como
# padrão" deixou o Resenha como navegador do sistema. O chromium do Arch faz igual.
export CHROME_DESKTOP=norte.desktop

# Integração com o sistema (as mesmas do chromium-flags.conf do Omarchy):
# - método de entrada no Wayland (fcitx5: japonês com Mozc);
# - senhas no chaveiro do GNOME (sem isto, no Hyprland, iriam em texto puro);
# - voltar/avançar com dois dedos no touchpad.
SISTEMA=(
  --ozone-platform-hint=auto
  --enable-wayland-ime --wayland-text-input-version=3
  --password-store=gnome-libsecret
  --enable-features=TouchpadOverscrollHistoryNavigation
)
# Extensões que o Omarchy põe no Chromium (Alt+Shift+L copia o endereço,
# Alt+Shift+D baixa o vídeo, WhatsApp no tema do sistema), se existirem.
OMARCHY=/usr/share/omarchy/default/chromium/extensions
EXTENSOES=()
for e in copy-url yt-dlp whatsapp-slim; do
  [[ -f "$OMARCHY/$e/manifest.json" ]] && EXTENSOES+=("$OMARCHY/$e")
done
if ((${#EXTENSOES[@]})); then
  SISTEMA+=("--load-extension=$(IFS=,; echo "${EXTENSOES[*]}")")
fi

exec /usr/lib/norte/norte "${SISTEMA[@]}" --lang="$IDIOMA" \
  "${FLAGS[@]}" "$@"
