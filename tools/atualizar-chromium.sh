#!/bin/bash
# Atualiza a base do Chromium. Tem que virar rotina: cada versão estável traz
# correções de segurança que só chegam a quem usa o Norte quando sai uma versão
# nova dele. A issue "Chromium novo" do repositório avisa quando isso acontece.
#
#   tools/atualizar-chromium.sh <versão>     prepara a árvore e reaplica os patches
#   (resolver os conflitos que ele listar)
#   tools/atualizar-chromium.sh --continuar  regenera os patches e mostra o que conferir
#
# Depois: tools/compilar.sh, testar um perfil antigo (cópia!) com a versão nova e
# empacotar. Mudar de versão principal recompila tudo: horas, em prioridade ociosa.
#
# NUNCA abrir um perfil de verdade antes do "comparar" do fim: a lista de experimentos
# muda a cada versão e já mudou o formato dos dados dos perfis (ver "Experimentos de
# teste do Chromium" no docs/ARQUITETURA.md).
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${NAVEGADOR_SRC:-$HOME/Work/navegador-chromium/src}"
BACKUP="$HOME/.local/share/navegador/backup"
export PATH="$HOME/.local/share/navegador/depot_tools:$PATH"
PRIO=(nice -n 19 ionice -c3)

if [[ "${1:-}" = --continuar ]]; then
  ANTIGA=$(cat "$SRC/.versao-anterior-do-norte" 2>/dev/null || true)
  git -C "$SRC" reset -q
  python3 -I "$RAIZ/tools/marca.py" --src "$SRC"
  "$RAIZ/tools/icones.sh"
  "$RAIZ/tools/fonte.sh" --gerar >/dev/null
  git -C "$RAIZ" status --short patches CHROMIUM_VERSION | head -40
  if [[ -n "$ANTIGA" ]]; then
    echo; echo "=== Experimentos: o que mudou desde a $ANTIGA (olhe os [ARMAZENAMENTO])"
    python3 -I "$RAIZ/tools/lista_de_teste.py" comparar "$BACKUP/antes-de-$ANTIGA/fieldtrial.json" |
      grep -E 'ARMAZENAMENTO|^~' | cut -c1-170 || true
  fi
  echo; echo "=== Recursos de IA ligados que o kSemIA não cobre (acrescentar em sem_ia.cc)"
  python3 -I "$RAIZ/tools/lista_de_teste.py" ia | cut -c1-170
  exit 0
fi

NOVA="${1:?uso: tools/atualizar-chromium.sh <versão> | --continuar}"
ANTIGA=$(cat "$RAIZ/CHROMIUM_VERSION")
[[ "$NOVA" != "$ANTIGA" ]] || { echo "Já está na $NOVA." >&2; exit 1; }
if [[ -n "$(git -C "$RAIZ" status --porcelain -- patches navegador)" ]]; then
  echo "Há mudanças sem commit em patches/ ou navegador/: resolva antes." >&2
  exit 1
fi

# A árvore tem que bater com os patches, senão o trabalho sem commit se perde.
"$RAIZ/tools/fonte.sh" --gerar >/dev/null
if [[ -n "$(git -C "$RAIZ" status --porcelain -- patches)" ]]; then
  echo "A árvore tem mudanças que não estão em patches/: confira e faça o commit." >&2
  exit 1
fi

GUARDAR="$BACKUP/antes-de-$ANTIGA"
mkdir -p "$GUARDAR"
cp "$SRC/testing/variations/fieldtrial_testing_config.json" "$GUARDAR/fieldtrial.json"
git -C "$SRC" diff --binary > "$GUARDAR/arvore.diff"
cp "$(dirname "$SRC")/.gclient" "$GUARDAR/gclient"
echo "$ANTIGA" > "$SRC/.versao-anterior-do-norte"

cd "$SRC"
git stash push -q -m "norte em cima do $ANTIGA"
"${PRIO[@]}" git fetch -q --depth 1 origin "refs/tags/$NOVA:refs/tags/$NOVA"
"${PRIO[@]}" git checkout -q --detach "$NOVA"
cd ..
sed -i "s#refs/tags/$ANTIGA#refs/tags/$NOVA#" .gclient
"${PRIO[@]}" gclient sync --no-history --shallow --nohooks -j 8 --force
"${PRIO[@]}" gclient runhooks
echo "$NOVA" > "$RAIZ/CHROMIUM_VERSION"

python3 -I "$RAIZ/tools/reaplicar.py" "$RAIZ" "$SRC"
cat <<FIM

Agora: resolva os conflitos listados (procure <<<<<<< ours, que é a $NOVA, e
>>>>>>> theirs, que é o Norte em cima da $ANTIGA) e rode:

  tools/atualizar-chromium.sh --continuar
FIM
