#!/bin/bash
# Publica o pacote mais novo de pacote/ nas Releases públicas
# (LucasLomiento/norte-releases, que antes do Norte se chamava
# navegador-releases: o GitHub redireciona o nome antigo): só os pacotes e o
# repositório do pacman; o código fica neste repositório. Depois refaz o site
# (site/).
#
#   tools/publicar.sh "notas da versão (markdown)"
#
# As notas são em português. Antes de publicar, pôr a tradução delas em
# site/notas/en/<tag>.md e site/notas/es/<tag>.md, com commit e push, pra o
# site já sair com elas.
#
# Cada release leva o pacote e o banco do pacman, com dois nomes: norte.db
# (seção [norte] do pacman.conf, a de agora) e navegador.db (a [navegador] de
# quem instalou antes do Norte). O Server ".../releases/latest/download"
# sempre aponta pra versão mais nova. O atualizador do Norte lê a mesma
# release pela API.
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
REPO=LucasLomiento/norte-releases
NOTAS="${1:-}"

PKG=$(ls -t "$RAIZ"/pacote/navegador-*-x86_64.pkg.tar.zst | head -1)
VERSAO=$(basename "$PKG" | sed -E 's/^navegador-(.*)-x86_64\.pkg\.tar\.zst$/\1/')
TAG="v$VERSAO"
echo "Pacote: $(basename "$PKG") -> $REPO $TAG"

if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  echo "A release $TAG já existe." >&2
  exit 1
fi

# As páginas em inglês e espanhol do site mostram a nota traduzida
# (site/notas/<idioma>/<tag>.md, já enviada pro GitHub antes de publicar);
# sem ela, a nota aparece em português, com um aviso.
for idioma in en es; do
  [[ -f "$RAIZ/site/notas/$idioma/$TAG.md" ]] ||
    echo "Aviso: falta site/notas/$idioma/$TAG.md (a nota vai aparecer em português no site em $idioma)." >&2
done

# O repositório (público) e o README dele.
TRAB=$(mktemp -d)
trap 'rm -rf "$TRAB"' EXIT
if ! gh repo view "$REPO" >/dev/null 2>&1; then
  gh repo create "$REPO" --public \
    --description "Norte (Chromium com espaços, containers e lateral): versões pro Arch Linux"
fi
gh repo clone "$REPO" "$TRAB/repo" -- -q 2>/dev/null || {
  mkdir -p "$TRAB/repo"; git -C "$TRAB/repo" init -q -b main
  git -C "$TRAB/repo" remote add origin "https://github.com/$REPO.git"
}
cp "$RAIZ/pacote/releases/README.md" "$TRAB/repo/README.md"
if [[ -n "$(git -C "$TRAB/repo" status --porcelain)" ]]; then
  git -C "$TRAB/repo" add README.md
  git -C "$TRAB/repo" commit -q -m "README: como instalar e atualizar"
  git -C "$TRAB/repo" push -q -u origin HEAD:main
fi

# Repositório do pacman só com esta versão, com os dois nomes.
mkdir "$TRAB/pub"
cp "$PKG" "$TRAB/pub/"
(cd "$TRAB/pub" && repo-add -q norte.db.tar.gz "$(basename "$PKG")" >/dev/null &&
  rm -f norte.db norte.files &&
  cp norte.db.tar.gz norte.db && cp norte.files.tar.gz norte.files &&
  for ext in db files db.tar.gz files.tar.gz; do
    cp "norte.$ext" "navegador.$ext"
  done)

[[ -z "$NOTAS" ]] && NOTAS="Navegador $VERSAO."
NOTAS="$NOTAS

**Instalar ou atualizar:** veja o [README](https://github.com/$REPO#readme) (Arch e derivados: repositório do pacman ou o .pkg.tar.zst)."
# O instalador do Windows da mesma versão (tools/windows.sh pacote), se houver.
ARQUIVOS=("$TRAB/pub/$(basename "$PKG")")
for nome in norte navegador; do
  for ext in db files db.tar.gz files.tar.gz; do
    ARQUIVOS+=("$TRAB/pub/$nome.$ext")
  done
done
WINDOWS="$RAIZ/pacote/navegador-$VERSAO-windows-x64.exe"
[[ -f "$WINDOWS" ]] && ARQUIVOS+=("$WINDOWS")
gh release create "$TAG" --repo "$REPO" --title "Norte $VERSAO" \
  --notes "$NOTAS" --latest "${ARQUIVOS[@]}"
echo "Publicado: https://github.com/$REPO/releases/tag/$TAG"
# O site (GitHub Pages) lê as releases: refaz com a versão nova.
gh workflow run site.yml --repo LucasLomiento/norte --ref main >/dev/null 2>&1 &&
  echo "Site: https://lucaslomiento.github.io/norte/ (atualizando)" ||
  echo "Aviso: não deu pra disparar o site (gh workflow run site.yml)." >&2
