#!/bin/bash
# Cria a chave que assina as versões do Norte. Roda uma vez só, num terminal:
# o gpg pede uma senha na janela do sistema, e essa senha não passa por script
# nenhum. Depois disso, cada publicação pede a senha de novo (o tools/publicar.sh
# não assina sozinho).
#
#   tools/chave-de-assinatura.sh
#
# Grava pacote/releases/norte.asc (a parte pública) e pacote/releases/CHAVE (a
# impressão digital) e envia a parte pública pro servidor de chaves: é de lá que
# o pacman de quem já tem o Norte instalado busca a chave na primeira versão
# assinada (sem isso, o `pacman -Syu` dessas pessoas falha).
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
NOME="Norte releases <LucasLomiento@users.noreply.github.com>"
SERVIDOR=hkps://keyserver.ubuntu.com
cd "$RAIZ/pacote/releases"

if [[ -s CHAVE ]]; then
  echo "Já existe uma chave: $(cat CHAVE)" >&2
  exit 1
fi
gpg --quick-generate-key "$NOME" ed25519 sign never
CHAVE=$(gpg --list-secret-keys --with-colons "$NOME" | awk -F: '/^fpr/{print $10; exit}')
gpg --armor --export "$CHAVE" > norte.asc
echo "$CHAVE" > CHAVE
if ! gpg --keyserver "$SERVIDOR" --send-keys "$CHAVE"; then
  echo "Não deu pra enviar. Tente de novo: gpg --keyserver $SERVIDOR --send-keys $CHAVE" >&2
fi

cat <<FIM

Chave criada: $CHAVE

A parte secreta só existe em ~/.gnupg. Sem ela e sem a senha, não dá pra assinar
versões novas, e quem já usa o Norte teria que trocar de chave à mão. Guarde uma
cópia no seu gerenciador de senhas:

  gpg --armor --export-secret-keys $CHAVE > norte-secreta.asc

Se a chave vazar, publique o certificado de revogação que o gpg já gerou em
~/.gnupg/openpgp-revocs.d/$CHAVE.rev.
FIM
