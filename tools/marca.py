#!/usr/bin/env python3
"""Troca "Chromium" por "Norte" (o nome do produto) nos textos da interface, sem perder as
traduções.

O grit identifica cada tradução (.xtb) por um hash do texto em inglês da
mensagem (.grd). Trocar o texto muda o hash e a tradução some (cai pro
inglês). Então, pra cada mensagem dos arquivos de INTEIROS
(chrome/app/chromium_strings.grd e os dos componentes):

  1. calcula o id antigo (texto original) e o novo (texto com Norte);
  2. em cada chromium_strings_*.xtb, renomeia a tradução do id antigo pro
     novo e troca "Chromium" no texto traduzido.

Idempotente: se o .grd já não tem "Chromium" nas mensagens, não faz nada.
Roda pelo tools/fonte.sh, depois dos patches (os arquivos gerados aqui não
entram em patches/: são muitos e mudam a cada versão).

  python3 -I tools/marca.py [--src ~/Work/navegador-chromium/src]
"""

import argparse
import glob
import os
import re
import sys

ANTIGO = "Chromium"
NOVO = "Norte"
# Nomes que não são o produto: ficam como estão.
PRESERVAR = ("Chromium OS", "ChromiumOS", "Chromium Authors", "chromium.org",
             "Chromium Embedded", "Chromium Projects",
             # "feito graças ao projeto de código aberto Chromium" (Sobre).
             'Chromium<ph name="END_LINK_CHROMIUM"')


def trocar(texto):
    marcas = {}
    for i, p in enumerate(PRESERVAR):
        chave = f"\0{i}\0"
        marcas[chave] = p
        texto = texto.replace(p, chave)
    texto = texto.replace(ANTIGO, NOVO)
    for chave, p in marcas.items():
        texto = texto.replace(chave, p)
    return texto


def ids_em_ordem(src, grd):
    sys.path.insert(0, os.path.join(src, "tools", "grit"))
    from grit import grd_reader  # noqa: E402
    from grit.node import message  # noqa: E402

    # Toda variável dos <if expr> vira False: só queremos os ids, de todas
    # as mensagens (o percurso abaixo passa por todos os ramos).
    # As do .grd e as das partes (.grdp) que ele inclui.
    nomes = set()
    for arquivo in partes(grd):
        texto = open(arquivo, encoding="utf-8").read()
        for expr in re.findall(r'expr="([^"]*)"', texto):
            nomes.update(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", expr))
    nomes -= {"and", "or", "not", "True", "False", "in", "pp_if",
              "pp_ifdef", "is_linux", "is_win", "is_macosx", "is_android",
              "is_ios", "is_fuchsia", "is_bsd", "is_posix", "is_chromeos",
              "lang"}
    defines = {n: False for n in nomes}
    raiz = grd_reader.Parse(grd, debug=False, defines=defines)
    # Em ordem, não por nome: o mesmo IDS_ aparece em ramos diferentes
    # (Mac e o resto, por exemplo) com textos diferentes.
    return [node.GetCliques()[0].GetMessage().GetId()
            for node in raiz.Preorder()
            if isinstance(node, message.MessageNode)]


# Mensagens fora do chromium_strings.grd que dizem "Chrome" querendo dizer o
# navegador. Só essas mudam (o arquivo tem "Chrome Web Store" e afins).
ESPECIFICAS = {
    "chrome/app/generated_resources.grd": (
        "chrome/app/resources/generated_resources_*.xtb",
        {"IDS_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL",
         "IDS_SETTINGS_SIDE_PANEL_ALIGNMENT_CHROME_PANELS"},
        "Chrome",
    ),
}


def partes(grd):
    """O .grd e as partes (.grdp) que ele inclui."""
    texto = open(grd, encoding="utf-8").read()
    return [grd] + [os.path.join(os.path.dirname(grd), parte)
                    for parte in re.findall(r'<part file="([^"]+)"', texto)]


def trocar_arquivo(src, grd_rel, xtb_glob, trocar_mensagem, antigo):
    """Troca o texto das mensagens escolhidas e leva as traduções junto."""
    grd = os.path.join(src, grd_rel)

    def nas_mensagens(m):
        nome = re.search(r'name="([^"]+)"', m.group(1))
        if not trocar_mensagem(nome.group(1) if nome else ""):
            return m.group(0)
        return m.group(1) + m.group(2).replace(antigo, NOVO) + m.group(3)

    mudados = {}
    for arquivo in partes(grd):
        original = open(arquivo, encoding="utf-8").read()
        novo = re.sub(r"(<message\b[^>]*>)(.*?)(</message>)", nas_mensagens,
                      original, flags=re.S)
        if novo != original:
            mudados[arquivo] = novo
    if not mudados:
        return 0, 0

    ids_antigos = ids_em_ordem(src, grd)
    for arquivo, novo in mudados.items():
        open(arquivo, "w", encoding="utf-8").write(novo)
    ids_novos = ids_em_ordem(src, grd)
    assert len(ids_antigos) == len(ids_novos)
    mapa = {a: n for a, n in zip(ids_antigos, ids_novos) if a != n}

    padrao = re.compile(r'<translation id="(\d+)">(.*?)</translation>', re.S)
    total = 0
    for xtb in sorted(glob.glob(os.path.join(src, xtb_glob))):
        conteudo = open(xtb, encoding="utf-8").read()

        def renomear(m):
            nonlocal total
            if m.group(1) not in mapa:
                return m.group(0)
            total += 1
            texto = m.group(2).replace(antigo, NOVO)
            return f'<translation id="{mapa[m.group(1)]}">{texto}</translation>'

        novo_xtb = padrao.sub(renomear, conteudo)
        if novo_xtb != conteudo:
            open(xtb, "w", encoding="utf-8").write(novo_xtb)
    return len(mapa), total


# Arquivos de texto com a marca inteira (toda mensagem com "Chromium" muda).
INTEIROS = (
    ("chrome/app/chromium_strings.grd",
     "chrome/app/resources/chromium_strings_*.xtb"),
    # "O Chromium não foi encerrado corretamente" e afins.
    ("components/components_chromium_strings.grd",
     "components/strings/components_chromium_strings_*.xtb"),
    # "O Chromium não vai salvar" (nova aba anônima), "Reiniciar o Chromium"
    # (aba que travou), avisos de certificado, senhas, preenchimento.
    ("components/components_strings.grd",
     "components/strings/components_strings_*.xtb"),
)


# Uma palavra só em português: "aba", como no resto do Norte (o Chromium em
# pt-BR diz "guia": "+ Nova guia" na lateral, "Fechar guia" no menu). Só
# nas traduções pt-BR: o inglês não muda, então os ids também não. Fica o
# "guia" de manual: com artigo masculino antes ("o guia", "um guia") e o
# "Guia de privacidade" e o "Guia de configuração".
PT_BR = ("chrome/app/resources/generated_resources_pt-BR.xtb",
         "components/strings/components_strings_pt-BR.xtb",
         "chrome/app/resources/chromium_strings_pt-BR.xtb")
MASCULINO = re.compile(
    r"\b(?:o|os|um|uns|este|estes|esse|esses|do|dos|no|nos|ao|aos|pelo|"
    r"pelos|neste|deste|desse|nesse)\s+$", re.I)
DE_MANUAL = re.compile(r"\s+de\s+(?:privacidade|configura)", re.I)
GUIA = re.compile(r"(?<![\w-])([Gg])uia(s?)(?![\w-])")
# Textos inteiros trocados (pt-BR): o título do balão de downloads encostava
# no X; o "Você e o Google" sem conta do Google; e o "Anúncios invasivos" do
# Chromium, que no Norte é o bloqueador (em todo site, não só nos abusivos).
TROCAS_PT = {
    "Histórico de downloads recentes": "Downloads recentes",
    "Você e o Google": "Você",
    "Anúncios invasivos": "Bloqueador de anúncios",
    "Neste site, há exibição de anúncios invasivos ou enganosos":
        "O Norte bloqueia anúncios e rastreadores em todo site",
    "No site, há exibição de anúncios invasivos ou enganosos":
        "O Norte bloqueia anúncios e rastreadores em todo site",
    "Bloquear a exibição de anúncios invasivos ou enganosos":
        "Bloquear anúncios e rastreadores",
    "Bloquear se o site mostrar anúncios invasivos ou enganosos":
        "Bloquear anúncios e rastreadores",
    "Os sites geralmente mostram anúncios para oferecer conteúdo ou serviços "
    "sem custo financeiro. No entanto, alguns sites têm anúncios invasivos ou "
    "enganosos.":
        "O bloqueador do Norte tira anúncios e rastreadores de todo site (com "
        "as listas EasyList e EasyPrivacy). Aqui dá pra liberar um site.",
    "A publicidade é bloqueada em sites que mostram anúncios invasivos ou "
    "enganosos":
        "Anúncios e rastreadores bloqueados em todo site",
}


def aba_no_texto(texto):
    def trocar_guia(m):
        if MASCULINO.search(texto[:m.start()]) or DE_MANUAL.match(
                texto, m.end()):
            return m.group(0)
        return ("A" if m.group(1) == "G" else "a") + "ba" + m.group(2)
    return GUIA.sub(trocar_guia, texto)


def palavras_pt(src):
    padrao = re.compile(r'(<translation id="\d+">)(.*?)(</translation>)', re.S)
    total = 0
    for rel in PT_BR:
        caminho = os.path.join(src, rel)
        if not os.path.exists(caminho):
            continue
        conteudo = open(caminho, encoding="utf-8").read()

        def na_traducao(m):
            nonlocal total
            texto = TROCAS_PT.get(m.group(2), m.group(2))
            # Só o texto, fora das marcas (<ph name="..."/>).
            pedacos = re.split(r"(<[^>]+>)", texto)
            novo = "".join(p if p.startswith("<") else aba_no_texto(p)
                           for p in pedacos)
            if novo != m.group(2):
                total += 1
            return m.group(1) + novo + m.group(3)

        novo = padrao.sub(na_traducao, conteudo)
        if novo != conteudo:
            open(caminho, "w", encoding="utf-8").write(novo)
    print(f"marca: pt-BR: {total} traduções com \"aba\"")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.expanduser(
        "~/Work/navegador-chromium/src"))
    args = ap.parse_args()

    for grd_rel, xtb_glob in INTEIROS:
        trocar_inteiro(args.src, grd_rel, xtb_glob)
    especificas(args.src)
    palavras_pt(args.src)


def trocar_inteiro(src, grd_rel, xtb_glob):
    grd = os.path.join(src, grd_rel)
    # O .grd e as partes (.grdp) que ele inclui: as frases de Configurações
    # ("Sobre o Chromium"...) moram no settings_chromium_strings.grdp, e as
    # traduções delas no mesmo .xtb.
    arquivos = partes(grd)

    # Só o conteúdo das <message> muda (não comentários nem atributos).
    def nas_mensagens(m):
        return m.group(1) + trocar(m.group(2)) + m.group(3)

    mudados = {}
    for arquivo in arquivos:
        original = open(arquivo, encoding="utf-8").read()
        novo = re.sub(r"(<message\b[^>]*>)(.*?)(</message>)", nas_mensagens,
                      original, flags=re.S)
        if novo != original:
            mudados[arquivo] = novo
    if not mudados:
        print(f"marca: {grd_rel}: nada a trocar (já aplicado)")
        return

    ids_antigos = ids_em_ordem(src, grd)
    for arquivo, novo in mudados.items():
        open(arquivo, "w", encoding="utf-8").write(novo)
    ids_novos = ids_em_ordem(src, grd)

    assert len(ids_antigos) == len(ids_novos)
    mapa = {antigo: novo_id
            for antigo, novo_id in zip(ids_antigos, ids_novos)
            if novo_id != antigo}

    padrao = re.compile(r'<translation id="(\d+)">(.*?)</translation>', re.S)
    total = 0
    for xtb in sorted(glob.glob(os.path.join(src, xtb_glob))):
        conteudo = open(xtb, encoding="utf-8").read()

        def renomear(m):
            nonlocal total
            antigo = m.group(1)
            if antigo not in mapa:
                return m.group(0)
            total += 1
            return f'<translation id="{mapa[antigo]}">{trocar(m.group(2))}</translation>'

        open(xtb, "w", encoding="utf-8").write(padrao.sub(renomear, conteudo))

    print(f"marca: {grd_rel}: {len(mapa)} mensagens com id novo, "
          f"{total} traduções movidas")


def especificas(src):
    for grd_rel, (xtb_glob, nomes, antigo) in ESPECIFICAS.items():
        n, t = trocar_arquivo(src, grd_rel, xtb_glob, lambda nome: nome in nomes,
                              antigo)
        if n:
            print(f"marca: {grd_rel}: {n} mensagens, {t} traduções")


if __name__ == "__main__":
    main()
