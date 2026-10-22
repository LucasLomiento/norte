"""Converte ícones do Lucide (https://lucide.dev, licença ISC) pro formato
.icon do Chromium (ui/gfx/vector_icon_types.h).

    python3 -I tools/lucide.py <pasta dos ícones> <nome> [traço] > arquivo.icon
    python3 -I tools/lucide.py --cheio <pasta dos ícones> <nome> [traço] > arquivo.icon
        (preenchido e com o contorno: estrela de favorito, fixado, curtido)
    python3 -I tools/lucide.py --svg <cor> <pasta dos ícones> <nome> > arquivo.svg
        (o SVG, pra virar PNG com o rsvg-convert: favicons das páginas
        internas, tools/icones.sh)

<pasta dos ícones>: os .svelte do pacote @lucide/svelte (dist/icons), que
trazem o desenho em JSON ({"node": [["path", {"d": ...}], ...]}). Traço em
unidades da grade de 24 (padrão 2,1: dá ~1,75 px com o ícone em 20 px, a
espessura do Resenha).

Só gera comandos absolutos (o .icon não tem as variantes relativas de todos):
caminhos SVG (M L H V C S Q T A Z, maiúsculas e minúsculas), line, circle,
ellipse, rect (com rx), polyline e polygon.
"""
import json
import re
import sys

NUMERO = re.compile(r"[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?")


def fmt(v):
    v = round(v, 3)
    if v == int(v):
        return str(int(v))
    return ("%.3f" % v).rstrip("0").rstrip(".") + "f"


def ler_icone(pasta, nome):
    with open(f"{pasta}/{nome}.svelte", encoding="utf-8") as f:
        texto = f.read()
    m = re.search(r"const iconData = (\{.*?\});", texto, re.S)
    if not m:
        raise SystemExit(f"sem iconData em {nome}")
    return json.loads(m.group(1))["node"]


class Leitor:
    """Lê um 'd' caractere a caractere (as bandeiras do arco podem vir
    grudadas: "a2 2 0 011 1" é 0, 1 e depois o 1 do x)."""

    def __init__(self, d):
        self.d = d
        self.i = 0

    def pular(self):
        while self.i < len(self.d) and self.d[self.i] in " ,\t\n":
            self.i += 1

    def fim(self):
        self.pular()
        return self.i >= len(self.d)

    def comando(self):
        self.pular()
        if self.i < len(self.d) and self.d[self.i].isalpha():
            self.i += 1
            return self.d[self.i - 1]
        return None

    def numero(self):
        self.pular()
        m = NUMERO.match(self.d, self.i)
        if not m:
            raise ValueError(f"esperava número em {self.d[self.i:self.i + 12]!r}")
        self.i = m.end()
        return float(m.group(0))

    def bandeira(self):
        self.pular()
        c = self.d[self.i]
        if c not in "01":
            raise ValueError(f"bandeira de arco inválida em {self.d[self.i:]!r}")
        self.i += 1
        return int(c)


def caminho(d):
    """Um 'd' do SVG em comandos .icon absolutos."""
    saida = []
    ler = Leitor(d)
    cmd = None
    x = y = 0.0
    inicio = (0.0, 0.0)
    ultimo_ctrl = None  # (tipo, x, y) do último ponto de controle
    while not ler.fim():
        novo = ler.comando()
        if novo:
            cmd = novo
            if cmd in "Zz":
                saida.append("CLOSE")
                x, y = inicio
                ultimo_ctrl = None
                continue
        if cmd is None:
            raise ValueError("caminho sem comando")
        rel = cmd.islower()
        c = cmd.upper()
        dx, dy = (x, y) if rel else (0.0, 0.0)
        if c == "M":
            x, y = ler.numero() + dx, ler.numero() + dy
            saida.append("MOVE_TO, %s, %s" % (fmt(x), fmt(y)))
            inicio = (x, y)
            cmd = "l" if rel else "L"  # pares seguintes são linhas
            ultimo_ctrl = None
        elif c == "L":
            x, y = ler.numero() + dx, ler.numero() + dy
            saida.append("LINE_TO, %s, %s" % (fmt(x), fmt(y)))
            ultimo_ctrl = None
        elif c == "H":
            x = ler.numero() + dx
            saida.append("LINE_TO, %s, %s" % (fmt(x), fmt(y)))
            ultimo_ctrl = None
        elif c == "V":
            y = ler.numero() + dy
            saida.append("LINE_TO, %s, %s" % (fmt(x), fmt(y)))
            ultimo_ctrl = None
        elif c == "A":
            rx, ry, rot = ler.numero(), ler.numero(), ler.numero()
            grande, horario = ler.bandeira(), ler.bandeira()
            x, y = ler.numero() + dx, ler.numero() + dy
            saida.append("ARC_TO, %s, %s, %s, %d, %d, %s, %s" % (
                fmt(rx), fmt(ry), fmt(rot), grande, horario, fmt(x), fmt(y)))
            ultimo_ctrl = None
        elif c in "CS":
            if c == "C":
                x1, y1 = ler.numero() + dx, ler.numero() + dy
            elif ultimo_ctrl and ultimo_ctrl[0] == "C":
                x1, y1 = 2 * x - ultimo_ctrl[1], 2 * y - ultimo_ctrl[2]
            else:
                x1, y1 = x, y
            x2, y2 = ler.numero() + dx, ler.numero() + dy
            nx, ny = ler.numero() + dx, ler.numero() + dy
            saida.append("CUBIC_TO, %s, %s, %s, %s, %s, %s" % tuple(
                fmt(v) for v in (x1, y1, x2, y2, nx, ny)))
            ultimo_ctrl = ("C", x2, y2)
            x, y = nx, ny
        elif c in "QT":
            if c == "Q":
                x1, y1 = ler.numero() + dx, ler.numero() + dy
            elif ultimo_ctrl and ultimo_ctrl[0] == "Q":
                x1, y1 = 2 * x - ultimo_ctrl[1], 2 * y - ultimo_ctrl[2]
            else:
                x1, y1 = x, y
            nx, ny = ler.numero() + dx, ler.numero() + dy
            saida.append("QUADRATIC_TO, %s, %s, %s, %s" % tuple(
                fmt(v) for v in (x1, y1, nx, ny)))
            ultimo_ctrl = ("Q", x1, y1)
            x, y = nx, ny
        else:
            raise ValueError(f"comando {cmd} não suportado")
    return saida


def elemento(tipo, a):
    f = lambda k: float(a.get(k, 0))
    if tipo == "path":
        return caminho(a["d"])
    if tipo == "line":
        return ["MOVE_TO, %s, %s" % (fmt(f("x1")), fmt(f("y1"))),
                "LINE_TO, %s, %s" % (fmt(f("x2")), fmt(f("y2")))]
    if tipo == "circle":
        return ["CIRCLE, %s, %s, %s" % (fmt(f("cx")), fmt(f("cy")), fmt(f("r")))]
    if tipo == "ellipse":
        return ["OVAL, %s, %s, %s, %s" % (fmt(f("cx")), fmt(f("cy")),
                                          fmt(f("rx")), fmt(f("ry")))]
    if tipo == "rect":
        r = float(a.get("rx", a.get("ry", 0)))
        return ["ROUND_RECT, %s, %s, %s, %s, %s" % (
            fmt(f("x")), fmt(f("y")), fmt(f("width")), fmt(f("height")), fmt(r))]
    if tipo in ("polyline", "polygon"):
        pts = [float(v) for v in NUMERO.findall(a["points"])]
        cmds = ["MOVE_TO, %s, %s" % (fmt(pts[0]), fmt(pts[1]))]
        for j in range(2, len(pts), 2):
            cmds.append("LINE_TO, %s, %s" % (fmt(pts[j]), fmt(pts[j + 1])))
        if tipo == "polygon":
            cmds.append("CLOSE")
        return cmds
    raise ValueError(f"elemento {tipo} não suportado")


def svg(pasta, nome, cor, traco=2.0):
    """O ícone como SVG, no traço do Lucide e na cor pedida."""
    from xml.sax.saxutils import quoteattr
    elementos = []
    for tipo, attrs in ler_icone(pasta, nome):
        atributos = " ".join(f"{k}={quoteattr(str(v))}"
                             for k, v in attrs.items() if k != "key")
        elementos.append(f"<{tipo} {atributos}/>")
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" '
            f'viewBox="0 0 24 24" fill="none" stroke={quoteattr(cor)} '
            f'stroke-width="{traco}" stroke-linecap="round" '
            'stroke-linejoin="round">' + "".join(elementos) + "</svg>")


def main():
    if sys.argv[1] == "--svg":
        print(svg(sys.argv[3], sys.argv[4], sys.argv[2]))
        return
    cheio = sys.argv[1] == "--cheio"
    args = sys.argv[2:] if cheio else sys.argv[1:]
    pasta, nome = args[0], args[1]
    traco = float(args[2]) if len(args) > 2 else 2.1
    linhas = [
        "// Copyright 2026 The Chromium Authors",
        "// Use of this source code is governed by a BSD-style license that can be",
        "// found in the LICENSE file.",
        "",
        f"// navegador: \"{nome}\"{' preenchido' if cheio else ''} do Lucide "
        "(https://lucide.dev, licença ISC),",
        "// convertido por tools/lucide.py.",
        "",
        "CANVAS_DIMENSIONS, 24,",
    ]
    for n, (tipo, attrs) in enumerate(ler_icone(pasta, nome)):
        if n:
            linhas.append("NEW_PATH,")
        # Preenchido: o mesmo desenho sem traço (o .icon preenche por padrão)
        # e o contorno por cima, pra manter os cantos redondos do Lucide.
        if cheio:
            linhas += [c + "," for c in elemento(tipo, attrs)]
            linhas.append("NEW_PATH,")
        linhas.append(f"STROKE, {fmt(traco)},")
        linhas += [c + "," for c in elemento(tipo, attrs)]
    print("\n".join(linhas))


if __name__ == "__main__":
    main()
