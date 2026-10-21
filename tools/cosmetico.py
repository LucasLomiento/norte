"""Extrai as regras de esconder elementos de listas no formato do Adblock Plus.

    python3 -I tools/cosmetico.py lista1.txt [lista2.txt ...] \
        [--avisos lista-de-avisos.txt ...] > cosmetico.txt

As listas depois de --avisos (a EasyList Cookie: os avisos de cookies) saem
com as letras em minúscula (g, s, e): o navegador aplica só com a opção
"Esconder avisos de cookies" ligada.

Saída (uma regra por linha, campos separados por tab):
  G  seletor              esconder em todo site
  S  dominios  seletor    esconder só nesses sites (",", "~dominio" exclui)
  E  dominios  seletor    exceção (#@#): não esconder nesses sites
  J  dominios  caminho  valor
                          scriptlet "set" do uBlock (##+js(set, a.b, valor)):
                          define a variável que um script barrado criaria,
                          pro resto do site não quebrar

Ficam de fora as regras com sintaxe estendida do uBlock/AdGuard/ABP
(:has-text, :-abp-*, :style, scriptlets...), que não são CSS: um seletor
inválido no meio de um bloco faria o navegador ignorar o bloco inteiro.
"""
import re
import sys

# Pseudo-classes que não são CSS de verdade (as de verdade, como :has e
# :not, o Chromium entende).
ESTENDIDAS = re.compile(
    r":-abp-|:has-text\(|:contains\(|:style\(|:matches-css|:xpath\(|"
    r":upward\(|:remove\(|:watch-attr\(|:min-text-length\(|:matches-attr\(|"
    r":matches-path\(|:matches-media\(|:others\(|:if\(|:if-not\(|"
    r":nth-ancestor\(|:-uBO|:remove-attr\(|:remove-class\(|\{")


# Valores do "set" que o navegador sabe criar (bloqueador.cc).
VALORES = {"undefined", "false", "true", "null", "noopFunc", "trueFunc",
           "falseFunc", "emptyObj", "emptyArr", "{}", "[]", "''", "emptyStr",
           "yes", "no"}


def scriptlet(dominios, corpo):
    """'set, a.b, valor' -> ("J", dominios, "a.b", "valor"), ou None."""
    if not dominios or not corpo.endswith(")"):
        return None
    args = [a.strip() for a in corpo[:-1].split(",")]
    if len(args) != 3 or args[0] not in ("set", "set-constant"):
        return None
    caminho, valor = args[1], args[2]
    if not re.fullmatch(r"[A-Za-z_$][\w$]*(\.[A-Za-z_$][\w$]*)*", caminho):
        return None
    if valor not in VALORES and not re.fullmatch(r"-?\d{1,5}", valor):
        return None
    return ("J", dominios, caminho, valor)


def main():
    vistas = set()
    avisos = False
    for caminho in sys.argv[1:]:
        if caminho == "--avisos":
            avisos = True
            continue
        with open(caminho, encoding="utf-8", errors="replace") as lista:
            for linha in lista:
                linha = linha.strip()
                if not linha or linha.startswith("!") or linha.startswith("["):
                    continue
                excecao = "#@#" in linha
                separador = "#@#" if excecao else "##"
                if separador not in linha:
                    continue  # regra de rede, ou #?#, #$#, #%# (não é CSS)
                dominios, seletor = linha.split(separador, 1)
                if seletor.startswith("+js(") and not excecao:
                    if dominios and not re.fullmatch(r"[a-z0-9.,~*_-]+",
                                                     dominios):
                        continue
                    regra = scriptlet(dominios, seletor[4:])
                    if regra and regra not in vistas:
                        vistas.add(regra)
                        print("\t".join(regra))
                    continue
                if not seletor or seletor.startswith("+js") or \
                        ESTENDIDAS.search(seletor) or "\t" in seletor:
                    continue
                if dominios and not re.fullmatch(r"[a-z0-9.,~*_-]+", dominios):
                    continue
                if excecao:
                    regra = ("E", dominios, seletor)
                elif dominios:
                    regra = ("S", dominios, seletor)
                else:
                    regra = ("G", seletor)
                if avisos:
                    regra = (regra[0].lower(),) + regra[1:]
                if regra not in vistas:
                    vistas.add(regra)
                    print("\t".join(regra))


if __name__ == "__main__":
    main()
