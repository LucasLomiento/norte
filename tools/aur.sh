#!/bin/bash
# Gera o norte-bin do AUR para uma versão já publicada e, com --enviar, manda pro
# AUR. O tools/publicar.sh chama isto com --enviar a cada versão.
#
#   tools/aur.sh <versão> [--enviar] [pasta de saída]
#
# Antes da primeira vez: criar a conta em https://aur.archlinux.org, colocar lá a
# chave pública de SSH e rodar `ssh aur@aur.archlinux.org help` pra aceitar o host.
# Sem isso, o --enviar só avisa e sai sem erro.
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
VERSAO="${1:?uso: tools/aur.sh <versão> [--enviar] [pasta]}"
shift
ENVIAR=
SAIDA=
for argumento in "$@"; do
  case "$argumento" in
    --enviar) ENVIAR=1 ;;
    *) SAIDA="$argumento" ;;
  esac
done
TRAB=$(mktemp -d)
trap 'rm -rf "$TRAB"' EXIT
REMOTO=ssh://aur@aur.archlinux.org/norte-bin.git

if [[ -n "$ENVIAR" ]]; then
  if ! GIT_SSH_COMMAND='ssh -o BatchMode=yes -o ConnectTimeout=10' \
       git ls-remote "$REMOTO" >/dev/null 2>&1; then
    echo "AUR: sem acesso ao $REMOTO (ver o começo de tools/aur.sh). Pulando." >&2
    exit 0
  fi
  git clone -q "$REMOTO" "$TRAB/aur"
  PASTA="$TRAB/aur"
else
  PASTA="${SAIDA:-$TRAB/aur}"
  mkdir -p "$PASTA"
fi

ARQUIVO="navegador-$VERSAO-x86_64.pkg.tar.zst"
ENDERECO="https://github.com/LucasLomiento/norte-releases/releases/download/v$VERSAO/$ARQUIVO"
if [[ -f "$RAIZ/pacote/$ARQUIVO" ]]; then
  SHA=$(sha256sum "$RAIZ/pacote/$ARQUIVO" | cut -d' ' -f1)
else
  curl -fsSL -o "$TRAB/$ARQUIVO" "$ENDERECO"
  SHA=$(sha256sum "$TRAB/$ARQUIVO" | cut -d' ' -f1)
fi
cp "$RAIZ/pacote/norte.install" "$PASTA/norte.install"
SHA_INSTALL=$(sha256sum "$PASTA/norte.install" | cut -d' ' -f1)
sed -e "s/@VERSAO@/$VERSAO/" -e "s/@SHA256@/$SHA/" -e "s/@SHA256_INSTALL@/$SHA_INSTALL/" \
  "$RAIZ/pacote/aur/PKGBUILD.modelo" > "$PASTA/PKGBUILD"
(cd "$PASTA" && makepkg --printsrcinfo > .SRCINFO)

if [[ -n "$ENVIAR" ]]; then
  git -C "$PASTA" add PKGBUILD .SRCINFO norte.install
  if git -C "$PASTA" diff --cached --quiet; then
    echo "AUR: já está em $VERSAO."
  else
    git -C "$PASTA" commit -q -m "$VERSAO"
    git -C "$PASTA" push -q origin HEAD:master
    echo "AUR: norte-bin em $VERSAO: https://aur.archlinux.org/packages/norte-bin"
  fi
else
  echo "Gerado em $PASTA"
fi
