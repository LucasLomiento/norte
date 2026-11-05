# Como testar a versão 66: correções depois do Chromium 157

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-157.0.8103.38-66-x86_64.pkg.tar.zst
```

Feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Trocar de espaço e voltar várias vezes (atalho, botões do mouse, menu do título, bolinhas) | O nome no alto, a cor, as pastas fixadas e as abas acompanham sempre |
| Ativar uma aba de outro espaço pela busca de abas (`Ctrl+Shift+A`) | A janela vai pro espaço dela, com o nome certo |
| `Ctrl+S` para recolher e abrir | A página estica junto com a lateral, sem pular nem ficar cortada |
| Num espaço sem nada fixado, arrastar uma aba pela lateral | Não aparece "Solte aqui pra fixar" |
| Fixar pelo botão direito | Aparece o fixado e a linha embaixo dele |
| Arrastar um fixado aberto até a lista de abas | Uma linha mostra o lugar; ao soltar, vira aba comum ali, ativa se já estava |
| Arrastar um fixado fechado até a lista | Vira aba de fundo naquele lugar; a página à vista não muda |
| Arrastar um favorito (ladrilho) até a lista | Igual ao fixado |
| Arrastar uma pasta até a lista | Não sai: pasta continua nos fixados |
| Arrastar uma aba comum até a área dos fixados | Vira fixado no lugar marcado |
