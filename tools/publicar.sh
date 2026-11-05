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
# Cada release leva o pacote, o .deb e o banco do pacman, com dois nomes:
# norte.db (seção [norte] do pacman.conf, a de agora) e navegador.db (a
# [navegador] de quem instalou antes do Norte). O Server
# ".../releases/latest/download" sempre aponta pra versão mais nova. O
# atualizador do Norte lê a mesma release pela API.
#
# Tudo vai assinado com a chave de pacote/releases/CHAVE (criada pelo
# tools/chave-de-assinatura.sh): o gpg pede a senha na janela do sistema. O
# pacote e os bancos têm um .sig ao lado, e um SHA256SUMS assinado cobre o resto.
# A chave tem que estar no servidor de chaves, porque é de lá que o pacman de quem
# instalou antes das assinaturas (SigLevel = Optional TrustAll) busca ela: sem isso
# o `pacman -Syu` dessas pessoas para com "invalid or corrupted database".
#
# Ensaio: com PUBLICAR_ENSAIO=<pasta>, faz tudo (assina inclusive) e deixa os arquivos
# da release e os READMEs na pasta, sem falar com o GitHub, o AUR ou o servidor de
# chaves. PUBLICAR_PACOTE=<arquivo> escolhe o pacote.
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
REPO=LucasLomiento/norte-releases
NOTAS="${1:-}"
ENSAIO="${PUBLICAR_ENSAIO:-}"

# (sed, não head: com o pipefail, o head fechando o cano mata o ls com SIGPIPE e
# derruba o script, dependendo de quantos pacotes há na pasta.)
PKG="${PUBLICAR_PACOTE:-$(ls -t "$RAIZ"/pacote/navegador-*-x86_64.pkg.tar.zst | sed -n 1p)}"
VERSAO=$(basename "$PKG" | sed -E 's/^navegador-(.*)-x86_64\.pkg\.tar\.zst$/\1/')
TAG="v$VERSAO"
echo "Pacote: $(basename "$PKG") -> $REPO $TAG"

CHAVE=$(cat "$RAIZ/pacote/releases/CHAVE" 2>/dev/null || true)
if [[ -z "$CHAVE" ]]; then
  echo "Sem chave de assinatura: rode tools/chave-de-assinatura.sh." >&2
  exit 1
fi
if ! gpg --list-secret-keys "$CHAVE" >/dev/null 2>&1; then
  echo "A parte secreta da chave $CHAVE não está neste computador." >&2
  exit 1
fi
# O servidor é um conjunto de máquinas e uma chave nova demora pra chegar em todas:
# uma resposta só pode dar certo por sorte. O pacman faz as duas consultas.
if [[ -z "$ENSAIO" ]]; then
  for _ in {1..8}; do
    for pedido in "op=get&search=0x$CHAVE" "op=index&options=mr&fingerprint=on&search=0x$CHAVE"; do
      if ! curl -fsS -o /dev/null "https://keyserver.ubuntu.com/pks/lookup?$pedido"; then
        echo "A chave $CHAVE ainda não está em todo o keyserver.ubuntu.com: publicar agora" >&2
        echo "poderia quebrar o pacman de quem já instalou. Reenvie e espere uns minutos:" >&2
        echo "  gpg --keyserver hkps://keyserver.ubuntu.com --send-keys $CHAVE" >&2
        exit 1
      fi
    done
  done
fi

if [[ -z "$ENSAIO" ]] && gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
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
if [[ -n "$ENSAIO" ]]; then
  mkdir -p "$TRAB/repo"; git -C "$TRAB/repo" init -q -b main
else
  if ! gh repo view "$REPO" >/dev/null 2>&1; then
    gh repo create "$REPO" --public \
      --description "Norte (Chromium com espaços, containers e lateral): versões pro Linux"
  fi
  gh repo clone "$REPO" "$TRAB/repo" -- -q 2>/dev/null || {
    mkdir -p "$TRAB/repo"; git -C "$TRAB/repo" init -q -b main
    git -C "$TRAB/repo" remote add origin "https://github.com/$REPO.git"
  }
fi
cp "$RAIZ/pacote/releases/norte.asc" "$TRAB/repo/"
# Os READMEs: a chave no lugar do @CHAVE@ e o AUR só quando já der pra instalar por lá.
AUR=
[[ "$(curl -fsS 'https://aur.archlinux.org/rpc/v5/info?arg[]=norte-bin' || true)" == *'"resultcount":1'* ]] && AUR=1
for leia in README.md README.pt-BR.md; do
  sed "s/@CHAVE@/$CHAVE/g" "$RAIZ/pacote/releases/$leia" > "$TRAB/repo/$leia"
  [[ -n "$AUR" ]] || sed -i '/^<!-- aur -->$/,/^<!-- \/aur -->$/d' "$TRAB/repo/$leia"
done

# O .deb sai do pacote do pacman.
DEB="$RAIZ/pacote/norte_${VERSAO}_amd64.deb"
[[ -f "$DEB" ]] || SAIDA="$(dirname "$DEB")" chrt --idle 0 nice -n 19 "$RAIZ/tools/outros-pacotes.sh" "$PKG"

# Repositório do pacman só com esta versão, com os dois nomes. O `repo-add
# --sign` chama o gpg, que pede a senha.
mkdir "$TRAB/pub"
cp "$PKG" "$DEB" "$RAIZ/pacote/releases/norte.asc" "$TRAB/pub/"
(cd "$TRAB/pub" &&
  gpg --detach-sign --no-armor -u "$CHAVE" "$(basename "$PKG")" &&
  repo-add -q --sign --key "$CHAVE" norte.db.tar.gz "$(basename "$PKG")" >/dev/null &&
  for ext in db files; do
    rm -f "norte.$ext" "norte.$ext.sig"
    cp "norte.$ext.tar.gz" "norte.$ext"
    cp "norte.$ext.tar.gz.sig" "norte.$ext.sig"
    for nome in "$ext" "$ext.sig" "$ext.tar.gz" "$ext.tar.gz.sig"; do
      cp "norte.$nome" "navegador.$nome"
    done
  done &&
  sha256sum "$(basename "$PKG")" "$(basename "$DEB")" norte.asc > SHA256SUMS &&
  gpg --armor --detach-sign -u "$CHAVE" --output SHA256SUMS.asc SHA256SUMS)

[[ -z "$NOTAS" ]] && NOTAS="Navegador $VERSAO."
NOTAS="$NOTAS

**Instalar ou atualizar:** veja o [README](https://github.com/$REPO#readme)."
# O instalador do Windows da mesma versão (tools/windows.sh pacote), se houver.
ARQUIVOS=("$TRAB/pub/$(basename "$PKG")" "$TRAB/pub/$(basename "$PKG").sig"
          "$TRAB/pub/$(basename "$DEB")" "$TRAB/pub/norte.asc"
          "$TRAB/pub/SHA256SUMS" "$TRAB/pub/SHA256SUMS.asc")
for nome in norte navegador; do
  for ext in db db.sig files files.sig db.tar.gz db.tar.gz.sig files.tar.gz files.tar.gz.sig; do
    ARQUIVOS+=("$TRAB/pub/$nome.$ext")
  done
done
WINDOWS="$RAIZ/pacote/navegador-$VERSAO-windows-x64.exe"
[[ -f "$WINDOWS" ]] && ARQUIVOS+=("$WINDOWS")
if [[ -n "$ENSAIO" ]]; then
  mkdir -p "$ENSAIO"
  cp "${ARQUIVOS[@]}" "$TRAB/repo/README.md" "$TRAB/repo/README.pt-BR.md" "$ENSAIO/"
  echo "Ensaio em $ENSAIO: $(ls "$ENSAIO" | wc -l) arquivos. Nada foi publicado."
  exit 0
fi
# Só agora, com tudo assinado: o README diz que as versões são assinadas, e sem
# a senha certa o script para antes de mudar qualquer coisa no GitHub.
if [[ -n "$(git -C "$TRAB/repo" status --porcelain)" ]]; then
  git -C "$TRAB/repo" add README.md README.pt-BR.md norte.asc
  git -C "$TRAB/repo" commit -q -m "README: como instalar e atualizar"
  git -C "$TRAB/repo" push -q -u origin HEAD:main
fi
gh release create "$TAG" --repo "$REPO" --title "Norte $VERSAO" \
  --notes "$NOTAS" --latest "${ARQUIVOS[@]}"
echo "Publicado: https://github.com/$REPO/releases/tag/$TAG"
# O AUR, se a conta já estiver ligada (ver tools/aur.sh).
"$RAIZ/tools/aur.sh" "$VERSAO" --enviar ||
  echo "Aviso: não deu pra atualizar o AUR (tools/aur.sh $VERSAO --enviar)." >&2
# O site (GitHub Pages) lê as releases: refaz com a versão nova.
gh workflow run site.yml --repo LucasLomiento/norte --ref main >/dev/null 2>&1 &&
  echo "Site: https://lucaslomiento.github.io/norte/ (atualizando)" ||
  echo "Aviso: não deu pra disparar o site (gh workflow run site.yml)." >&2
