"""O que a lista de experimentos de teste do Chromium liga no Norte (Linux).

Fora da "official build", o Chromium aplica o
testing/variations/fieldtrial_testing_config.json: de cada estudo, o primeiro
experimento que vale pro Linux de mesa liga e desliga os recursos dele, com
os parâmetros. Mas se um só desses recursos estiver na linha de comando
(--enable-features/--disable-features, ou as listas do sem_ia.cc), o
Chromium pula o experimento INTEIRO (ShouldSkipExperiment, em
components/variations/field_trial_config/field_trial_util.cc): os outros
recursos dele voltam ao padrão, e os parâmetros também.

    python3 -I tools/lista_de_teste.py de <Recurso> [...]
        estudo, experimento, parâmetros e os outros recursos que caem junto
    python3 -I tools/lista_de_teste.py derrubados
        estudos que as listas do sem_ia.cc derrubam, e o que cai junto sem
        estar em lista nenhuma
    python3 -I tools/lista_de_teste.py ia
        estudos ligados com cara de IA que o kSemIA não cobre
    python3 -I tools/lista_de_teste.py comparar <json antigo> [<json novo>]
        a cada Chromium novo: estudos novos, que saíram ou mudaram, com os que
        mexem em armazenamento (formato de dados) marcados
"""

import json
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.environ.get(
    "NAVEGADOR_SRC", os.path.expanduser("~/Work/navegador-chromium/src"))
LISTA = os.path.join(SRC, "testing/variations/fieldtrial_testing_config.json")
SEM_IA = os.path.join(RAIZ, "navegador/browser/sem_ia/sem_ia.cc")

# Nomes que costumam mudar o que fica gravado no perfil: estudo novo com um
# destes merece teste de 4 aberturas (gravar, abrir sem, abrir com) antes de
# entrar ou sair.
ARMAZENAMENTO = re.compile(
    r"Sqlite|LevelDb|Idb|IndexedDB|DomStorage|LocalStorage|SessionStorage|"
    r"Encrypt|Migration|Database|DiskCache|HttpCache|PersistentCache|"
    r"CodeCache|Prefs?\b|Sync|Storage|Bookmark|History|Cookie|Partition",
    re.I)

PARECE_IA = re.compile(
    r"^AI|AI[A-Z]|Ai[A-Z]|Aim[A-Z]|Gemini|Glic|Nano|OnDeviceModel|"
    r"ModelExecution|Summariz|PromptAPI|LanguageModel|WriterAPI|RewriterAPI|"
    r"Proofread|Compose|Lens|Embedding|Annotated|PageContent|WebMCP|Actor|"
    r"Contextual|Soda|LiveCaption|Indigo|Skills|Insights|TabOrganization",
)


def carregar(caminho=LISTA):
    """{estudo: experimento escolhido pro Linux de mesa}, como o Chromium."""
    with open(caminho, encoding="utf-8") as f:
        lista = json.load(f)
    escolhidos = {}
    for estudo, configs in sorted(lista.items()):
        for config in configs:
            if "linux" not in config.get("platforms", []):
                continue
            formas = config.get("form_factors", [])
            if formas and "desktop" not in formas:
                continue
            if config.get("is_low_end_device") is True:
                continue
            exp = next((e for e in config.get("experiments", [])
                        if not e.get("hardware_classes")), None)
            if exp:
                escolhidos[estudo] = {
                    "experimento": exp.get("name", ""),
                    "params": exp.get("params", {}),
                    "liga": exp.get("enable_features", []),
                    "desliga": exp.get("disable_features", []),
                }
                break
    return escolhidos


def listas_do_sem_ia():
    """Os nomes do kSemIA, kLigadas e kDesligadas (sem os parâmetros)."""
    with open(SEM_IA, encoding="utf-8") as f:
        texto = f.read()
    sem_comentarios = re.sub(r"//[^\n]*", "", texto)
    listas = {}
    for nome in ("kSemIA", "kLigadas", "kDesligadas"):
        m = re.search(nome + r"\[\]\s*=\s*(\{.*?\}|\".*?\");",
                      sem_comentarios, re.S)
        if not m:
            listas[nome] = []
            continue
        # Literais vizinhos viram um só ("a" "b" -> "ab"), como no C++.
        corpo = re.sub(r'"\s*"', "", m.group(1))
        pedacos = re.findall(r'"([^"]*)"', corpo)
        nomes = []
        for p in ",".join(pedacos).split(","):
            p = p.strip()
            if p:
                nomes.append(p.split(":", 1)[0])
        listas[nome] = nomes
    return listas


def parametros(exp):
    return "/".join(f"{k}/{v}" for k, v in sorted(exp["params"].items()))


def de(nomes):
    estudos = carregar()
    for nome in nomes:
        achou = False
        for estudo, exp in estudos.items():
            if nome in exp["liga"] or nome in exp["desliga"]:
                achou = True
                lado = "liga" if nome in exp["liga"] else "desliga"
                print(f"{nome}: {lado} em {estudo} [{exp['experimento']}]")
                if exp["params"]:
                    print(f"    parâmetros: {parametros(exp)}")
                outros = [f for f in exp["liga"] if f != nome]
                if outros:
                    print(f"    caem junto (ligados): {', '.join(outros)}")
                outros = [f for f in exp["desliga"] if f != nome]
                if outros:
                    print(f"    caem junto (desligados): {', '.join(outros)}")
        if not achou:
            print(f"{nome}: fora da lista (vale o padrão do Chromium)")


def derrubados():
    estudos = carregar()
    listas = listas_do_sem_ia()
    nossos = set(n for l in listas.values() for n in l)
    for estudo, exp in estudos.items():
        citados = [f for f in exp["liga"] + exp["desliga"] if f in nossos]
        if not citados:
            continue
        perdidos_l = [f for f in exp["liga"] if f not in nossos]
        perdidos_d = [f for f in exp["desliga"] if f not in nossos]
        print(f"{estudo} [{exp['experimento']}] por {', '.join(citados)}")
        if exp["params"]:
            print(f"    parâmetros perdidos: {parametros(exp)}")
        if perdidos_l:
            print(f"    voltam ao padrão (estavam ligados): "
                  f"{', '.join(perdidos_l)}")
        if perdidos_d:
            print(f"    voltam ao padrão (estavam desligados): "
                  f"{', '.join(perdidos_d)}")


def ia():
    estudos = carregar()
    sem_ia = listas_do_sem_ia()["kSemIA"]
    # "GlicActor" já está coberto pelo "Glic" desligado: os recursos que
    # começam com um nome do kSemIA dependem dele.
    def coberto(f):
        return any(f.startswith(n) for n in sem_ia)
    for estudo, exp in estudos.items():
        suspeitos = [f for f in exp["liga"]
                     if PARECE_IA.search(f) and not coberto(f)]
        if suspeitos:
            print(f"{estudo} [{exp['experimento']}]: {', '.join(suspeitos)}")


def comparar(antigo, novo):
    a = carregar(antigo)
    b = carregar(novo)
    def marca(estudo, exp):
        nomes = [estudo] + exp["liga"] + exp["desliga"]
        return " [ARMAZENAMENTO]" if any(ARMAZENAMENTO.search(n)
                                         for n in nomes) else ""
    for estudo in sorted(set(b) - set(a)):
        print(f"+ {estudo}: liga {', '.join(b[estudo]['liga']) or '-'}"
              f"{marca(estudo, b[estudo])}")
    for estudo in sorted(set(a) - set(b)):
        print(f"- {estudo}: ligava {', '.join(a[estudo]['liga']) or '-'}"
              f"{marca(estudo, a[estudo])}")
    for estudo in sorted(set(a) & set(b)):
        if a[estudo] != b[estudo]:
            print(f"~ {estudo}: {json.dumps(a[estudo], ensure_ascii=False)}"
                  f"\n    -> {json.dumps(b[estudo], ensure_ascii=False)}"
                  f"{marca(estudo, b[estudo])}")


def main(args):
    if not args:
        print(__doc__)
        return 1
    if args[0] == "de" and len(args) > 1:
        de(args[1:])
    elif args[0] == "derrubados":
        derrubados()
    elif args[0] == "ia":
        ia()
    elif args[0] == "comparar" and len(args) in (2, 3):
        comparar(args[1], args[2] if len(args) == 3 else LISTA)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
