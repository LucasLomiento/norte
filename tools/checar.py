#!/usr/bin/env python3
"""Checagem rápida (clang -fsyntax-only) dos arquivos que o navegador muda,
sem esperar a compilação inteira chegar neles.

Lista: os .cc em navegador/ e os .cc do Chromium tocados pelos patches
(git diff no src). Usa out/release/compile_commands.json (gn gen
--export-compile-commands), ou o do out/dev. Roda em paralelo, em prioridade baixa.

  python3 -I tools/checar.py [arquivo.cc ...]
"""

import concurrent.futures
import json
import os
import shlex
import subprocess
import sys

SRC = os.path.expanduser("~/Work/navegador-chromium/src")
# O out/release é o que se compila pro pacote; o out/dev fica pra trás.
# CHECAR_OUT=out/dev pra usar outro.
OUT = os.path.join(SRC, os.environ.get(
    "CHECAR_OUT",
    "out/release" if os.path.exists(
        os.path.join(SRC, "out/release/compile_commands.json")) else "out/dev"))


def arquivos_mudados():
    diff = subprocess.run(["git", "diff", "--name-only"], cwd=SRC,
                          capture_output=True, text=True, check=True).stdout
    mudados = [f for f in diff.split() if f.endswith(".cc")]
    nossos = subprocess.run(["find", "-L", "navegador", "-name", "*.cc"],
                            cwd=SRC, capture_output=True, text=True,
                            check=True).stdout.split()
    return sorted(set(mudados + nossos))


def checar(entrada):
    args = shlex.split(entrada["command"])
    # Troca a saída por só checar a sintaxe.
    if "-o" in args:
        i = args.index("-o")
        del args[i:i + 2]
    if "-MF" in args:
        i = args.index("-MF")
        del args[i:i + 2]
    args = [a for a in args if not a.startswith("-MD")]
    args.append("-fsyntax-only")
    r = subprocess.run(["nice", "-n", "19"] + args, cwd=entrada["directory"],
                       capture_output=True, text=True)
    return entrada["file"], r.returncode, r.stderr


def main():
    cc = json.load(open(os.path.join(OUT, "compile_commands.json")))
    por_arquivo = {os.path.normpath(os.path.join(e["directory"], e["file"])): e
                   for e in cc}
    alvos = sys.argv[1:] or arquivos_mudados()
    entradas = []
    for a in alvos:
        caminho = os.path.normpath(os.path.join(SRC, a))
        if caminho in por_arquivo:
            entradas.append(por_arquivo[caminho])
        else:
            print(f"  (sem comando de compilação: {a})")
    erros = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
        for arquivo, rc, stderr in ex.map(checar, entradas):
            nome = arquivo.replace("../../", "")
            if rc == 0:
                print(f"ok    {nome}")
            else:
                erros += 1
                print(f"ERRO  {nome}")
                print("\n".join(l for l in stderr.splitlines()
                                if "error:" in l)[:4000])
    sys.exit(1 if erros else 0)


if __name__ == "__main__":
    main()
