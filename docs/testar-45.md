# Como testar a versão 45

Sem as sugestões do Google na barra. Instalar (num terminal):

```
sudo pacman -U ~/Work/github/pessoal/navegador/pacote/navegador-155.0.8059.79-45-x86_64.pkg.tar.zst
```

Depois feche o Norte inteiro e abra de novo.

| O que fazer | O que deveria acontecer |
|---|---|
| Digitar "receita de bolo" na barra | Só a linha "receita de bolo – Pesquisa Google" e o que tiver no seu histórico e nos favoritos; nada de "receita de bolo de chocolate", "de cenoura" |
| Clicar na barra sem digitar nada | Nenhuma pesquisa sugerida pelo Google |
| Configurações do Norte (Ctrl+,) → Privacidade | "Barra de endereço → Sugestões do buscador", desligado |
| Ligar o interruptor e digitar de novo | As sugestões do Google voltam na hora; desligar tira na hora |
| `> foco` e `2+3*4` na barra | Os comandos e a calculadora continuam |

Se as sugestões continuarem aparecendo logo depois de instalar, a opção estava gravada como ligada no seu perfil (ligada na mão em algum momento): é só desligar em Privacidade.
