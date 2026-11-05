#!/usr/bin/env python3
"""Reaplica patches/ numa árvore do Chromium que acabou de mudar de versão, um
arquivo de cada vez e com merge de 3 vias: um conflito num arquivo não derruba o
patch inteiro. Usado pelo tools/atualizar-chromium.sh.

  python3 -I tools/reaplicar.py <raiz do repositório> <src do Chromium>

O merge precisa dos objetos da versão anterior, que ficam no git da árvore
enquanto o stash do atualizar-chromium.sh existir.
"""

import glob
import os
import re
import subprocess
import sys

raiz, src = sys.argv[1:3]
limpos, conflitos, falhas = [], [], []
for caminho in sorted(glob.glob(os.path.join(raiz, "patches", "*.patch"))):
    with open(caminho, "rb") as arquivo:
        partes = re.split(rb"(?m)^(?=diff --git )", arquivo.read())
    for parte in partes:
        if not parte.startswith(b"diff --git"):
            continue
        cabecalho = parte.split(b"\n", 1)[0].decode(errors="replace")
        nome = cabecalho.split(" ")[-1].split("/", 1)[-1].strip('"')
        if subprocess.run(["git", "apply", "--index", "-"], cwd=src, input=parte,
                          capture_output=True).returncode == 0:
            limpos.append(nome)
            continue
        r = subprocess.run(["git", "apply", "--3way", "-"], cwd=src, input=parte,
                           capture_output=True)
        saida = (r.stderr + r.stdout).decode(errors="replace")
        if r.returncode == 0:
            limpos.append(nome)
        elif "with conflicts" in saida or "\nU " in saida:
            conflitos.append((nome, os.path.basename(caminho)))
        else:
            falhas.append((nome, os.path.basename(caminho), saida.strip()[:200]))

print(f"limpos: {len(limpos)}  conflitos: {len(conflitos)}  falharam: {len(falhas)}")
for nome, patch in conflitos:
    print(f"CONFLITO {nome}  ({patch})")
for nome, patch, erro in falhas:
    print(f"FALHOU {nome}  ({patch}): {erro}")
