"""Ícones do Material (Chromium) -> Lucide pelo nome (navegador/icones/MAPA).

    python3 -I tools/mapa_icones.py

Acha no src todas as cópias de cada .icon listado no MAPA (o mesmo nome
aparece em várias pastas: components/vector_icons, chrome/app/vector_icons,
ui/views/vector_icons...), gera o .icon do Lucide que falta em
navegador/icones (tools/lucide.py; ":cheio" = preenchido) e regrava a seção
gerada do navegador/icones/LUGARES, que o tools/icones.sh copia pro src. As
linhas escritas à mão no LUGARES (antes da seção) valem primeiro: o caminho
que já está nelas fica fora da seção.

Rodar de novo depois de atualizar o Chromium pega as cópias novas.
"""
import os
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.environ.get("NAVEGADOR_SRC",
                     os.path.expanduser("~/Work/navegador-chromium/src"))
LUCIDE = os.environ.get(
    "LUCIDE",
    os.path.expanduser(
        "~/Work/github/pessoal/resenha/node_modules/@lucide/svelte/dist/icons"))
ICONES = os.path.join(RAIZ, "navegador", "icones")
MARCA = "# --- gerado por tools/mapa_icones.py a partir do MAPA; não editar ---"
# Pastas que o Norte não compila (ou que são nossas).
FORA = ("out/", "third_party/", "navegador/", "ash/", "chromeos/", "ios/",
        "android_webview/", "remoting/", "fuchsia_web/")


def icones_do_src():
    por_nome = {}
    for pasta, subpastas, arquivos in os.walk(SRC):
        rel = os.path.relpath(pasta, SRC)
        rel = "" if rel == "." else rel + "/"
        subpastas[:] = [d for d in subpastas
                        if not (rel + d + "/").startswith(FORA)
                        and not d.startswith(".")]
        for a in arquivos:
            if a.endswith(".icon"):
                por_nome.setdefault(a[:-5], []).append(rel + a)
    return por_nome


def main():
    mapa = []
    for linha in open(os.path.join(ICONES, "MAPA"), encoding="utf-8"):
        linha = linha.split("#", 1)[0].strip()
        if linha:
            material, lucide = linha.split()
            nome, _, modo = lucide.partition(":")
            mapa.append((material, nome, modo == "cheio"))

    lugares = os.path.join(ICONES, "LUGARES")
    texto = open(lugares, encoding="utf-8").read()
    manual = texto.split(MARCA)[0].rstrip("\n") + "\n"
    feitos = {l.split()[1] for l in manual.splitlines()
              if l.strip() and not l.startswith("#")}
    # O que os nossos patches já mudam (a agulha no lugar do logo do
    # Chromium, no 0004) não é trocado por cima.
    for l in open(os.path.join(RAIZ, "patches", "SERIES"), encoding="utf-8"):
        if l.strip() and not l.startswith("#"):
            feitos.update(l.split()[1:])

    por_nome = icones_do_src()
    geradas, sem_copia = [], []
    for material, nome, cheio in mapa:
        arquivo = nome + ("-cheio" if cheio else "")
        destino = os.path.join(ICONES, arquivo + ".icon")
        if not os.path.exists(destino):
            args = ["python3", "-I", os.path.join(RAIZ, "tools", "lucide.py")]
            args += (["--cheio"] if cheio else []) + [LUCIDE, nome]
            saida = subprocess.run(args, check=True, capture_output=True,
                                   text=True).stdout
            with open(destino, "w", encoding="utf-8") as f:
                f.write(saida + "\n")
        copias = [c for c in sorted(por_nome.get(material, []))
                  if c not in feitos]
        if not copias:
            sem_copia.append(material)
        geradas += [f"{arquivo} {c}" for c in copias]

    with open(lugares, "w", encoding="utf-8") as f:
        f.write(manual + MARCA + "\n" + "\n".join(geradas) + "\n")
    print(f"{len(geradas)} lugares em {len(mapa)} nomes")
    if sem_copia:
        print("sem cópia no src (ou só nas linhas à mão):", " ".join(sem_copia),
              file=sys.stderr)


if __name__ == "__main__":
    main()
