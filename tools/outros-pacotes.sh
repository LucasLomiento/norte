#!/bin/bash
# Monta o .deb (Debian 12+, Ubuntu 24.04+: o binário pede glibc 2.36) a partir
# do pacote do pacman já pronto, sem compilar nada. Pesa na CPU: rode com
# `chrt --idle 0 nice -n 19`, como a compilação.
#
#   tools/outros-pacotes.sh [pacote/navegador-<versão>-x86_64.pkg.tar.zst]
#
# Gera em pacote/: norte_<versão>_amd64.deb.
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
PKG="${1:-$(ls -t "$RAIZ"/pacote/navegador-*-x86_64.pkg.tar.zst | head -1)}"
VERSAO=$(basename "$PKG" | sed -E 's/^navegador-(.*)-x86_64\.pkg\.tar\.zst$/\1/')
SAIDA="${SAIDA:-$RAIZ/pacote}"
echo "Pacote: $(basename "$PKG") ($VERSAO)"

TRAB=$(mktemp -d)
trap 'rm -rf "$TRAB"' EXIT
mkdir "$TRAB/raiz"
bsdtar -xpf "$PKG" -C "$TRAB/raiz" --exclude '.PKGINFO' --exclude '.MTREE' \
  --exclude '.BUILDINFO' --exclude '.INSTALL'
# O que só existe por causa do nome antigo não vai pra quem está chegando agora.
rm -rf "$TRAB/raiz/usr/bin/navegador" "$TRAB/raiz/usr/lib/navegador" \
  "$TRAB/raiz/usr/share/applications/navegador.desktop"

fazer_deb() {
  local d="$TRAB/deb" c="$TRAB/controle" saida="$SAIDA/norte_${VERSAO}_amd64.deb"
  cp -a "$TRAB/raiz" "$d"
  mkdir -p "$c" "$d/usr/share/doc" "$d/usr/share/norte"
  echo deb > "$d/usr/lib/norte/FORMATO"
  chmod 4755 "$d/usr/lib/norte/chrome-sandbox"
  mv "$d/usr/share/licenses/navegador" "$d/usr/share/doc/norte"
  rmdir "$d/usr/share/licenses"
  install -m644 "$RAIZ/pacote/deb/apparmor-norte" "$d/usr/share/norte/apparmor-norte"
  install -m755 "$RAIZ/pacote/deb/postinst" "$RAIZ/pacote/deb/postrm" "$c/"
  cat > "$c/control" <<CONTROL
Package: norte
Version: $VERSAO
Architecture: amd64
Maintainer: Lucas Lomiento <LucasLomiento@users.noreply.github.com>
Installed-Size: $(du -sk "$d" | cut -f1)
Depends: ca-certificates, fonts-liberation, libasound2t64 | libasound2, libatk-bridge2.0-0t64 | libatk-bridge2.0-0, libatk1.0-0t64 | libatk1.0-0, libatspi2.0-0t64 | libatspi2.0-0, libc6 (>= 2.36), libcairo2, libcups2t64 | libcups2, libdbus-1-3, libdrm2, libexpat1, libgbm1, libglib2.0-0t64 | libglib2.0-0, libgtk-3-0t64 | libgtk-3-0, libnspr4, libnss3, libpango-1.0-0, libudev1, libvulkan1, libx11-6, libxcb1, libxcomposite1, libxdamage1, libxext6, libxfixes3, libxkbcommon0, libxrandr2, xdg-utils
Recommends: gnome-keyring, pipewire
Provides: www-browser
Section: web
Priority: optional
Homepage: https://lucaslomiento.github.io/norte/
Description: Chromium-based browser with a sidebar, spaces and containers
 Norte keeps your tabs in a sidebar that gets out of the way, gives every part
 of your life its own space, keeps accounts apart with containers and blocks
 ads and trackers out of the box. No AI features and no Google account.
CONTROL
  printf '2.0\n' > "$TRAB/debian-binary"
  tar -C "$c" --owner=0 --group=0 --numeric-owner --sort=name -cJf "$TRAB/control.tar.xz" .
  (cd "$d" && XZ_OPT='-6 -T4' tar --owner=0 --group=0 --numeric-owner --sort=name \
    -cJf "$TRAB/data.tar.xz" ./usr ./etc)
  rm -f "$saida"
  (cd "$TRAB" && ar rc "$saida" debian-binary control.tar.xz data.tar.xz)
  echo "Gerado: $saida ($(du -h "$saida" | cut -f1))"
}

fazer_deb
