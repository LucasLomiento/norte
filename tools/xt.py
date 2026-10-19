#!/usr/bin/env python3
"""Mouse e teclado de verdade num servidor X (Xvfb) via XTest, sem tocar na
sessão Wayland do usuário.

  DISPLAY=:77 xt.py move X Y
  DISPLAY=:77 xt.py click X Y [1|2|3|8|9] [shift+ctrl...]  (8 = voltar, 9 = avançar)
  DISPLAY=:77 xt.py scroll X Y up|down|left|right [n]
  DISPLAY=:77 xt.py key ctrl+comma ctrl+alt+Right
  DISPLAY=:77 xt.py type "texto"
"""

import ctypes
import sys
import time

x11 = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XStringToKeysym.restype = ctypes.c_ulong
x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
x11.XKeysymToKeycode.restype = ctypes.c_ubyte
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                      ctypes.c_int, ctypes.c_int,
                                      ctypes.c_ulong]
xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                      ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                   ctypes.c_int, ctypes.c_ulong]

d = x11.XOpenDisplay(None)
if not d:
    sys.exit("sem display")

NOMES = {"ctrl": "Control_L", "shift": "Shift_L", "alt": "Alt_L",
         "super": "Super_L", "enter": "Return", "esc": "Escape",
         "comma": "comma", "space": "space", "tab": "Tab",
         "backspace": "BackSpace", "del": "Delete"}


def flush():
    x11.XSync(d, 0)


def mover(x, y):
    xtst.XTestFakeMotionEvent(d, -1, x, y, 0)
    flush()
    time.sleep(0.08)


def botao(b):
    xtst.XTestFakeButtonEvent(d, b, 1, 0)
    flush()
    time.sleep(0.05)
    xtst.XTestFakeButtonEvent(d, b, 0, 0)
    flush()
    time.sleep(0.1)


def keycode(nome):
    sym = x11.XStringToKeysym(NOMES.get(nome.lower(), nome).encode())
    if not sym:
        sys.exit(f"tecla desconhecida: {nome}")
    return x11.XKeysymToKeycode(d, sym)


def combo(texto):
    codigos = [keycode(t) for t in texto.split("+")]
    for c in codigos:
        xtst.XTestFakeKeyEvent(d, c, 1, 0)
        flush()
        time.sleep(0.03)
    for c in reversed(codigos):
        xtst.XTestFakeKeyEvent(d, c, 0, 0)
        flush()
        time.sleep(0.03)
    time.sleep(0.15)


args = sys.argv[1:]
cmd = args[0]
if cmd == "move":
    mover(int(args[1]), int(args[2]))
elif cmd == "click":
    mover(int(args[1]), int(args[2]))
    # Teclas seguradas durante o clique (ex.: "shift" ou "ctrl+shift").
    segurar = [keycode(t) for t in args[4].split("+")] if len(args) > 4 else []
    for c in segurar:
        xtst.XTestFakeKeyEvent(d, c, 1, 0)
        flush()
        time.sleep(0.03)
    botao(int(args[3]) if len(args) > 3 else 1)
    for c in reversed(segurar):
        xtst.XTestFakeKeyEvent(d, c, 0, 0)
        flush()
        time.sleep(0.03)
elif cmd == "scroll":
    mover(int(args[1]), int(args[2]))
    b = {"up": 4, "down": 5, "left": 6, "right": 7}[args[3]]
    for _ in range(int(args[4]) if len(args) > 4 else 1):
        botao(b)
elif cmd == "key":
    for c in args[1:]:
        combo(c)
elif cmd == "type":
    ESPECIAIS = {" ": "space", ".": "period", "/": "slash", "-": "minus",
                 ":": "shift+semicolon", "_": "shift+minus", "=": "equal",
                 "?": "shift+slash"}
    for ch in args[1]:
        if ch in ESPECIAIS:
            combo(ESPECIAIS[ch])
        elif ch.isupper():
            combo("shift+" + ch)
        else:
            combo(ch)
else:
    sys.exit(__doc__)
