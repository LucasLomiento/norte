"""Ícones do Lucide nas páginas internas (WebUI), pelo navegador/icones/WEBUI.

    python3 -I tools/icones_webui.py

Duas formas de linha no WEBUI:
  <arquivo>#<id> <lucide>[:cheio]  troca cada <g id="id"> do conjunto de
      ícones (cr-iconset) por um <g> com o desenho do Lucide em traço; o
      viewBox de 24 vai no próprio <g> (o cr-iconset copia o <g> inteiro e
      usa o viewBox dele), e a cor pelo style, pra seguir o
      --iron-icon-fill-color das páginas que pintam ícone por ele.
  <arquivo .svg> <lucide>[:cheio]  troca o .svg inteiro (e o "_old" do lado):
      são máscaras (--cr-icon-image, os da barra de endereço), a cor vem do
      tema; o width/height do original fica.

Só regrava o que mudou (mexer na data recompila os recursos). Roda pelo
tools/icones.sh; os .svg inteiros ficam fora dos patches (tools/fonte.sh).
"""
import os
import re
import sys
from xml.sax.saxutils import quoteattr

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lucide  # noqa: E402  (tools/lucide.py)

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.environ.get("NAVEGADOR_SRC",
                     os.path.expanduser("~/Work/navegador-chromium/src"))
LUCIDE = os.environ.get(
    "LUCIDE",
    os.path.expanduser(
        "~/Work/github/pessoal/resenha/node_modules/@lucide/svelte/dist/icons"))
TRACO = 2


def desenho(nome):
    partes = []
    for tipo, attrs in lucide.ler_icone(LUCIDE, nome):
        a = " ".join(f"{k}={quoteattr(str(v))}" for k, v in attrs.items()
                     if k != "key")
        partes.append(f"<{tipo} {a}></{tipo}>")
    return "".join(partes)


def grupo(id_, nome, cheio):
    cor = "var(--iron-icon-fill-color, currentColor)"
    estilo = (f"fill: {cor if cheio else 'none'}; stroke: {cor}; "
              f"stroke-width: {TRACO}; stroke-linecap: round; "
              "stroke-linejoin: round")
    return (f'<g id="{id_}" viewBox="0 0 24 24" style="{estilo}">'
            f"<!-- navegador: \"{nome}\" do Lucide (licença ISC) -->"
            f"{desenho(nome)}</g>")


def trocar_grupos(texto, id_, novo):
    """Troca cada <g id="id_">...</g> (com <g> aninhados) por `novo`."""
    abre = f'<g id="{id_}"'
    saida, i = [], 0
    while True:
        j = texto.find(abre, i)
        if j < 0:
            saida.append(texto[i:])
            return "".join(saida)
        nivel, k = 0, j
        for m in re.finditer(r"<g[\s>]|</g>", texto[j:]):
            nivel += -1 if m.group() == "</g>" else 1
            if nivel == 0:
                k = j + m.end()
                break
        saida += [texto[i:j], novo]
        i = k


def svg_inteiro(original, nome, cheio):
    m = re.search(r"<svg\b[^>]*>", original)
    raiz = m.group() if m else ""
    tam = ""
    for atributo in ("width", "height"):
        v = re.search(rf'\b{atributo}="([^"]*)"', raiz)
        tam += f' {atributo}="{v.group(1) if v else 24}"'
    preenche = "#000" if cheio else "none"
    return ('<svg xmlns="http://www.w3.org/2000/svg"' + tam +
            f' viewBox="0 0 24 24" fill="{preenche}" stroke="#000"'
            f' stroke-width="{TRACO}" stroke-linecap="round"'
            ' stroke-linejoin="round">'
            f"<!-- navegador: \"{nome}\" do Lucide (licença ISC) -->"
            f"{desenho(nome)}</svg>\n")


def gravar(caminho, texto):
    with open(caminho, encoding="utf-8") as f:
        if f.read() == texto:
            return False
    with open(caminho, "w", encoding="utf-8") as f:
        f.write(texto)
    return True


def main():
    grupos, inteiros = {}, []
    for linha in open(os.path.join(RAIZ, "navegador", "icones", "WEBUI"),
                      encoding="utf-8"):
        linha = linha.split("#", 1)[0] if linha.startswith("#") else linha
        linha = linha.strip()
        if not linha:
            continue
        alvo, lucide_ = linha.split()
        nome, _, modo = lucide_.partition(":")
        if "#" in alvo:
            arquivo, id_ = alvo.split("#", 1)
            grupos.setdefault(arquivo, []).append((id_, nome, modo == "cheio"))
        else:
            inteiros.append((alvo, nome, modo == "cheio"))

    mudados = 0
    for arquivo, lista in grupos.items():
        caminho = os.path.join(SRC, arquivo)
        texto = open(caminho, encoding="utf-8").read()
        for id_, nome, cheio in lista:
            texto = trocar_grupos(texto, id_, grupo(id_, nome, cheio))
        mudados += gravar(caminho, texto)
    for alvo, nome, cheio in inteiros:
        base = os.path.join(SRC, alvo)
        for caminho in (base, base[:-4] + "_old.svg"):
            if os.path.exists(caminho):
                original = open(caminho, encoding="utf-8").read()
                mudados += gravar(caminho, svg_inteiro(original, nome, cheio))
    print(f"icones_webui: {mudados} arquivos mudados")


if __name__ == "__main__":
    main()
