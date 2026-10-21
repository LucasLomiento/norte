#!/bin/bash
# Compila o navegador no menor peso possível pro resto do PC:
# prioridade ociosa de CPU e de disco, e poucos processos ao mesmo tempo
# (30 GB de RAM: cada compilação do Blink chega a 2 GB).
#   tools/compilar.sh            -> out/dev, alvo chrome (JOBS=8, teto de 22 GB de RAM)
#   JOBS=12 tools/compilar.sh    -> mais rápido, mais pesado
# Com um jogo aberto, a compilação congela até ele fechar.
#   tools/compilar.sh out/dev chrome chrome_sandbox
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${NAVEGADOR_SRC:-$HOME/Work/navegador-chromium/src}"
OUT="${1:-out/dev}"; shift || true
ALVOS=("${@:-chrome}")
JOBS="${JOBS:-8}"
export PATH="$HOME/.local/share/navegador/depot_tools:$PATH"
cd "$SRC"
if [ ! -f "$OUT/args.gn" ]; then
  mkdir -p "$OUT"
  cp "$RAIZ/tools/args-dev.gn" "$OUT/args.gn"
  gn gen "$OUT"
fi
# Num escopo do systemd com teto de memória: se a RAM apertar, quem morre é
# a compilação, não os apps abertos do usuário.
MEMORIA="${MEMORIA:-22G}"
UNIDADE="norte-compilar-$$"
systemd-run --user --scope --quiet --collect --unit="$UNIDADE" \
  -p MemoryHigh="$MEMORIA" -p MemoryMax="$MEMORIA" \
  chrt --idle 0 nice -n 19 ionice -c3 \
  autoninja -j "$JOBS" -C "$OUT" "${ALVOS[@]}" &
COMPILACAO=$!

# Jogo aberto (Proton/Wine, gamescope ou um jogo da biblioteca da Steam): a
# compilação fica congelada (systemctl freeze) até ele fechar, pra CPU e o
# disco ficarem pro jogo.
# (Os colchetes impedem o padrão de casar com uma linha de comando que o
# contenha escrito.)
jogando() {
  pgrep -f '\.ex[e]( |$)|steamapps/[c]ommon/' >/dev/null ||
    pgrep -x 'gamescop[e]|gamescope-w[l]' >/dev/null
}
(
  congelada=
  while kill -0 "$COMPILACAO" 2>/dev/null; do
    if jogando; then
      if [[ -z "$congelada" ]] &&
         systemctl --user freeze "$UNIDADE.scope" 2>/dev/null; then
        congelada=1
        echo "[compilar] jogo aberto: compilação pausada" >&2
      fi
    elif [[ -n "$congelada" ]]; then
      systemctl --user thaw "$UNIDADE.scope" 2>/dev/null
      congelada=
      echo "[compilar] jogo fechado: compilação continua" >&2
    fi
    sleep 5
  done
) &
VIGIA=$!
codigo=0
wait "$COMPILACAO" || codigo=$?
kill "$VIGIA" 2>/dev/null || true
exit "$codigo"
