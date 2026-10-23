# Como testar a versão 55

Leva a 54 (Memória, ver docs/testar-54.md), que não foi publicada. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-55-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Com a barra escondida (Ctrl+Shift+S), fechar o Norte à força (`pkill -9 -f /usr/lib/norte/norte`) e abrir de novo | Abre, com o balão "Restaurar páginas?" no canto de cima; "Restaurar" traz as abas |
| O mesmo com a barra à vista | Igual (já funcionava) |
| Arrastar uma aba da lateral pra fora da janela (janela nova) e de volta, algumas vezes, inclusive abrindo links de outros apps no meio | Sem queda; as abas ficam no espaço |
