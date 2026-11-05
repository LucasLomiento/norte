# Norte

*[English](README.md)*

Navegador baseado no Chromium, com as abas numa lateral que se esconde, **espaços** (cada um com as suas abas, sites fixados, pastas e favoritos), **containers** (cookies e logins separados dentro do mesmo perfil, como os do Firefox), **bloqueador de anúncios e rastreadores** e importação do Zen. Escuro, minimalista e sem recursos de IA. Código: [LucasLomiento/norte](https://github.com/LucasLomiento/norte).

Site: [lucaslomiento.github.io/norte](https://lucaslomiento.github.io/norte/).

Para **Linux x86_64**.

> Até a versão 20 o Norte se chamava **Navegador**. Quem já tinha instalado não precisa fazer nada: a atualização troca o nome sozinha e leva o perfil junto (logins, espaços, containers e extensões, de `~/.config/navegador` pra `~/.config/norte`). A seção `[navegador]` antiga do `pacman.conf` continua funcionando.

## Arch Linux e derivados

CachyOS, EndeavourOS, Manjaro. Os pacotes são assinados; esta é a impressão digital da chave:

```
@CHAVE@
```

1. Confie na chave:

   ```sh
   curl -fsSL https://github.com/LucasLomiento/norte-releases/raw/main/norte.asc | sudo pacman-key --add -
   sudo pacman-key --lsign-key @CHAVE@
   ```

2. Acrescente ao fim do `/etc/pacman.conf`:

   ```ini
   [norte]
   SigLevel = Required DatabaseRequired
   Server = https://github.com/LucasLomiento/norte-releases/releases/latest/download
   ```

3. Instale:

   ```sh
   sudo pacman -Sy norte
   ```

Depois disso ele atualiza junto com o sistema (`sudo pacman -Syu`). O próprio Norte também procura versão nova quando abre e mostra um aviso no pé da lateral, com o botão "Atualizar" (Configurações → Geral → Atualizações).

<!-- aur -->
**Pelo AUR:** `yay -S norte-bin` (ou o seu ajudante de AUR). Quem atualiza é o AUR; o Norte só avisa que saiu versão nova.
<!-- /aur -->

### Instalou antes de os pacotes serem assinados?

Se o seu `pacman.conf` ainda tem `SigLevel = Optional TrustAll`, o pacman pergunta uma vez se pode importar a chave: responda **S**, e tudo continua funcionando. Se responder não, o `pacman -Sy` para com um erro sobre o banco de dados. Para a verificação completa, faça os passos 1 e 2 acima.

## Debian 12+, Ubuntu 24.04+ e derivados

Baixe o `.deb` da [última versão](https://github.com/LucasLomiento/norte-releases/releases/latest) e:

```sh
sudo apt install ./norte_*_amd64.deb
```

O Norte avisa quando sai versão nova e instala pelo aviso no pé da lateral. Ubuntu 22.04 e anteriores não têm suporte: o navegador pede glibc 2.36.

## Conferir um download

O `SHA256SUMS` lista a soma de cada arquivo e o `SHA256SUMS.asc` assina essa lista:

```sh
curl -fsSL https://github.com/LucasLomiento/norte-releases/raw/main/norte.asc | gpg --import
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
```

## Primeiros passos

- **Configurações do Norte:** `Ctrl+,` (ou clique no nome do espaço, no alto da lateral).
- **Mostrar/ocultar a lateral:** `Ctrl+S`. Escondida, ela aparece ao encostar o ponteiro na borda esquerda.
- **Trocar de espaço:** `Ctrl+Alt+←/→`, as bolinhas no pé da lateral, ou os botões laterais do mouse em cima dela.
- **Vindo do Zen:** Configurações → Geral → Importar do Zen (containers, espaços, sites fixados, cookies de cada container, senhas, favoritos e histórico).
- **Netflix e afins (Widevine):** o componente é baixado sozinho no primeiro uso.
- **1Password:** a extensão já conversa com o app do 1Password; o pacote instala a ligação e põe o Norte na lista de navegadores em que o app confia.

## Licenças

Feito sobre o [Chromium](https://www.chromium.org/) (licença BSD) e os componentes de terceiros dele; a lista completa está em `chrome://credits`. Não é um produto do Google.
