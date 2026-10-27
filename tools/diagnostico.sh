#!/bin/bash
# Diagnóstico do Norte pesando ou travando. Rodar com o Norte aberto, de
# preferência na hora em que ele está pesado:
#
#   bash diagnostico.sh            # só olha (CPU, memória, registro)
#   bash diagnostico.sh --pilha    # travado de vez: fecha o Norte gerando um
#                                  # core dump e guarda onde ele estava preso
#
# Grava ~/norte-diagnostico-<data>.txt: é esse arquivo que vai pro
# desenvolvedor. Não lê senhas, cookies nem as páginas abertas; o registro do
# Norte (journal) pode ter endereços de sites.
set -u
saida=~/norte-diagnostico-$(date +%Y%m%d-%H%M%S).txt

principal() {
  ps -eo pid,args --sort=start_time |
    awk '$2 ~ "/usr/lib/norte/norte" && $0 !~ "--type=" {print $1; exit}'
}

tipo() {
  local t
  t=$(tr '\0' ' ' < "/proc/$1/cmdline" 2>/dev/null |
        grep -o -- '--type=[a-z-]*' | head -1)
  echo "${t:-principal}"
}

# CPU de cada processo do Norte em 5 s, pelo /proc (sem depender do top).
cpu_dos_processos() {
  local antes depois p
  declare -A antes
  for p in $(pgrep -f '^/usr/lib/norte/norte'); do
    antes[$p]=$(awk '{print $14+$15}' "/proc/$p/stat" 2>/dev/null)
  done
  sleep 5
  for p in "${!antes[@]}"; do
    depois=$(awk '{print $14+$15}' "/proc/$p/stat" 2>/dev/null) || continue
    awk -v a="${antes[$p]}" -v d="$depois" -v p="$p" \
        -v r="$(awk '/VmRSS/ {print $2}' "/proc/$p/status" 2>/dev/null)" \
        -v t="$(tipo "$p")" \
        'BEGIN {printf "%6.1f%% %7.0f MB  %-28s pid %s\n", (d-a)/5, r/1024, t, p}'
  done | sort -rn
}

{
  echo "== Norte e sistema ($(date))"
  echo "Versão: $(cat /usr/lib/norte/VERSAO 2>/dev/null)"
  echo "Kernel: $(uname -r)  Sessão: ${XDG_CURRENT_DESKTOP:-?} ${XDG_SESSION_TYPE:-?}"
  lspci -k 2>/dev/null | grep -A2 -E 'VGA|3D' | grep -E 'VGA|3D|driver'
  echo
  echo "== Memória do sistema"
  free -m
  echo
  pid=$(principal)
  if [ -z "$pid" ]; then
    echo "O Norte não está aberto."
  else
    echo "== Processos do Norte (CPU medida em 5 s)"
    cpu_dos_processos
    echo
    echo "Processos: $(pgrep -fc '^/usr/lib/norte/norte')  Páginas (renderer): $(pgrep -fc '^/usr/lib/norte/norte.*--type=renderer')"
    awk '/VmRSS/ {printf "Principal: %.0f MB\n", $2/1024}' "/proc/$pid/status"
    echo "Aberto há: $(ps -o etime= -p "$pid")"
    echo "Linhas de execução do principal: $(ls "/proc/$pid/task" | wc -l)"
    echo "Estado do principal: $(awk '/State/ {print $2, $3}' "/proc/$pid/status")"
    echo
  fi
  echo "== Mais pesados do sistema"
  top -b -n1 -o %CPU 2>/dev/null | head -20
  echo
  echo "== Registro do Norte desde que o PC ligou: mensagens mais repetidas"
  journalctl --user -b -t norte --no-pager -o cat 2>/dev/null |
    sed -E 's/^\[[0-9]+:[0-9]+:[0-9]+\/[0-9.]+:([A-Z]+):([^]]+)\] /\1 \2 /' |
    cut -c1-150 | sed -E 's/-?[0-9]+(\.[0-9]+)? ms/N ms/' |
    sort | uniq -c | sort -rn | head -25
  echo
  echo "== Registro do Norte: últimas 300 linhas (sem o 'Frame latency')"
  journalctl --user -b -t norte --no-pager 2>/dev/null |
    grep -v "Frame latency" | tail -300 | cut -c1-300
} > "$saida" 2>&1

if [ "${1:-}" = "--pilha" ] && [ -n "${pid:-}" ]; then
  echo
  echo "O Norte (processo $pid) vai ser fechado à força pra mostrar onde ele"
  echo "estava preso. As abas voltam na próxima vez que ele abrir."
  read -r -p "Continuar? [s/N] " resposta
  if [ "$resposta" = "s" ] || [ "$resposta" = "S" ]; then
    kill -ABRT "$pid"
    for _ in $(seq 30); do
      coredumpctl info "$pid" >/dev/null 2>&1 && break
      sleep 1
    done
    {
      echo
      echo "== Onde o processo principal estava (core dump)"
      coredumpctl info "$pid" --no-pager 2>&1 | head -400
    } >> "$saida"
  fi
fi

echo "Pronto: $saida"
