#!/bin/bash
# Prepara a árvore do Chromium com as nossas mudanças:
#   1. liga navegador/ dentro dela como src/navegador
#   2. aplica patches/*.patch em ordem (pula os que já estão aplicados)
#   3. troca "Chromium" por "Norte" nos textos (tools/marca.py)
#   4. põe os ícones do navegador (tools/icones.sh)
#
#   tools/fonte.sh            -> prepara
#   tools/fonte.sh --gerar    -> regrava patches/ a partir do que mudou no src
#                                (um patch por área, ver patches/SERIES)
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${NAVEGADOR_SRC:-$HOME/Work/navegador-chromium/src}"

if [ ! -d "$SRC/.git" ]; then
  echo "Sem árvore do Chromium em $SRC (rode ~/Work/navegador-chromium/sync.sh)" >&2
  exit 1
fi

ln -sfn "$RAIZ/navegador" "$SRC/navegador"

if [ "${1:-}" = "--gerar" ]; then
  # Cada linha de patches/SERIES: <nome>.patch <caminhos...>
  # O que mudou no src e não está em nenhuma linha vai pra 9999-resto.patch.
  cd "$SRC"
  usados=()
  while read -r nome caminhos; do
    [ -z "$nome" ] || [[ "$nome" = \#* ]] && continue
    # shellcheck disable=SC2086
    git diff --binary -- $caminhos > "$RAIZ/patches/$nome"
    [ -s "$RAIZ/patches/$nome" ] || rm -f "$RAIZ/patches/$nome"
    # shellcheck disable=SC2206
    usados+=($caminhos)
  done < "$RAIZ/patches/SERIES"
  excl=()
  for c in "${usados[@]}"; do excl+=(":(exclude)$c"); done
  # Os ícones do Lucide são copiados pelo tools/icones.sh.
  while read -r _nome destino; do
    case "$_nome" in ''|\#*) continue ;; esac
    excl+=(":(exclude)$destino")
  done < "$RAIZ/navegador/icones/LUGARES"
  # E os .svg inteiros das páginas internas (tools/icones_webui.py), com o
  # "_old" do lado.
  while read -r alvo _nome; do
    case "$alvo" in ''|\#*|*'#'*) continue ;; esac
    excl+=(":(exclude)$alvo" ":(exclude)${alvo%.svg}_old.svg")
  done < "$RAIZ/navegador/icones/WEBUI"
  # Os textos com o nome do produto (e o "aba" no lugar de "guia") são
  # gerados pelo tools/marca.py; os favicons, pelo tools/icones.sh.
  git diff --binary -- . ':(exclude)navegador' \
    ':(exclude)chrome/app/chromium_strings.grd' \
    ':(exclude)chrome/app/resources/chromium_strings_*.xtb' \
    ':(exclude)chrome/app/settings_chromium_strings.grdp' \
    ':(exclude)chrome/app/settings_strings.grdp' \
    ':(exclude)components/components_chromium_strings.grd' \
    ':(exclude)components/strings/components_chromium_strings_*.xtb' \
    ':(exclude)components/components_strings.grd' \
    ':(exclude)components/*_strings.grdp' \
    ':(exclude)components/strings/components_strings_*.xtb' \
    ':(exclude)chrome/app/generated_resources.grd' \
    ':(exclude)chrome/app/resources/generated_resources_*.xtb' \
    ':(exclude)chrome/app/theme/chromium/*product_logo*' \
    ':(exclude)chrome/app/theme/chromium/linux/*product_logo*' \
    ':(exclude)chrome/app/theme/default_*_percent/chromium/*product_logo*' \
    ':(exclude)chrome/app/theme/default_*_percent/chromium/linux/*product_logo*' \
    ':(exclude)ui/webui/resources/images/chrome_logo_dark.svg' \
    ':(exclude)chrome/app/theme/default_*_percent/common/favicon_ntp.png' \
    ':(exclude)chrome/app/theme/default_*_percent/common/favicon_settings.png' \
    ':(exclude)chrome/app/theme/default_*_percent/common/favicon_downloads.png' \
    ':(exclude)components/resources/default_*_percent/favicon_history.png' \
    ':(exclude)extensions/browser/resources/default_*_percent/favicon_extensions.png' \
    ':(exclude)ui/resources/default_*_percent/common/default_favicon*.png' \
    "${excl[@]}" > "$RAIZ/patches/9999-resto.patch"
  [ -s "$RAIZ/patches/9999-resto.patch" ] || rm -f "$RAIZ/patches/9999-resto.patch"
  ls -1 "$RAIZ/patches"
  exit 0
fi

cd "$SRC"
for p in "$RAIZ"/patches/*.patch; do
  [ -e "$p" ] || continue
  if git apply --check "$p" 2>/dev/null; then
    git apply "$p" && echo "aplicado: $(basename "$p")"
  elif git apply --check --reverse "$p" 2>/dev/null; then
    echo "já estava: $(basename "$p")"
  else
    echo "CONFLITO: $(basename "$p")" >&2
    exit 1
  fi
done

python3 -I "$RAIZ/tools/marca.py" --src "$SRC"
"$RAIZ/tools/icones.sh"
