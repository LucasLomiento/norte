# Norte

*[Português](README.pt-BR.md)*

A Chromium-based browser with your tabs in a sidebar that hides itself, **spaces** (each with its own tabs, pinned sites, folders and bookmarks), **containers** (separate cookies and logins inside the same profile, like Firefox containers), an **ad and tracker blocker** and import from Zen. Dark, minimal and with no AI features. Code: [LucasLomiento/norte](https://github.com/LucasLomiento/norte).

Website: [lucaslomiento.github.io/norte](https://lucaslomiento.github.io/norte/en/).

For **Linux x86_64**. Norte's own menus and settings are in Portuguese for now; with `NORTE_IDIOMA=en-US`, everything that comes from Chromium is in English.

## Arch Linux and derivatives

CachyOS, EndeavourOS, Manjaro. Packages are signed; this is the key's fingerprint:

```
@CHAVE@
```

1. Trust the key:

   ```sh
   curl -fsSL https://github.com/LucasLomiento/norte-releases/raw/main/norte.asc | sudo pacman-key --add -
   sudo pacman-key --lsign-key @CHAVE@
   ```

2. Add this to the end of `/etc/pacman.conf`:

   ```ini
   [norte]
   SigLevel = Required DatabaseRequired
   Server = https://github.com/LucasLomiento/norte-releases/releases/latest/download
   ```

3. Install:

   ```sh
   sudo pacman -Sy norte
   ```

Norte then updates with your system (`sudo pacman -Syu`). It also checks for new versions when it opens and shows a notice at the bottom of the sidebar, with an "Atualizar" (update) button.

<!-- aur -->
**From the AUR:** `yay -S norte-bin` (or your AUR helper). The AUR updates it; Norte only tells you a new version is out.
<!-- /aur -->

### Installed before the packages were signed?

If your `pacman.conf` still says `SigLevel = Optional TrustAll`, pacman asks once whether to import the key: answer **Y**, and everything keeps working. Answering no makes `pacman -Sy` stop with an error about the database. To get the full verification, do steps 1 and 2 above.

## Debian 12+, Ubuntu 24.04+ and derivatives

Download the `.deb` from the [latest version](https://github.com/LucasLomiento/norte-releases/releases/latest) and:

```sh
sudo apt install ./norte_*_amd64.deb
```

Norte tells you when a new version is out and installs it from the notice at the bottom of the sidebar. Ubuntu 22.04 and older aren't supported: the browser needs glibc 2.36.

## Checking a download

`SHA256SUMS` lists the checksum of each file and `SHA256SUMS.asc` signs that list:

```sh
curl -fsSL https://github.com/LucasLomiento/norte-releases/raw/main/norte.asc | gpg --import
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
```

## First steps

- **Norte settings:** `Ctrl+,` (or click the space's name at the top of the sidebar).
- **Show or hide the sidebar:** `Ctrl+S`. When hidden, it appears when the pointer touches the left edge.
- **Switch spaces:** `Ctrl+Alt+←/→`, the dots at the bottom of the sidebar, or your mouse's side buttons over it.
- **Coming from Zen:** Settings → Geral → Importar do Zen (containers, spaces, pinned sites, each container's cookies, passwords, bookmarks and history).
- **Netflix and similar (Widevine):** the component downloads by itself the first time.
- **1Password:** the extension talks to the 1Password app out of the box; the package sets up the link and adds Norte to the browsers the app trusts.

## Licenses

Built on [Chromium](https://www.chromium.org/) (BSD license) and its third-party components; the full list is at `chrome://credits`. Not a Google product.
