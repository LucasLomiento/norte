# Norte

*[Português](README.pt-BR.md)*

**Website:** [lucaslomiento.github.io/norte](https://lucaslomiento.github.io/norte/en/)

Norte is a Linux browser built on Chromium, with your tabs in a sidebar, **spaces**, **containers** and a built-in **ad blocker**. Dark, minimal and with no AI features. (Until version 20 it was called Navegador, which is why the code and the package still use that name in places.)

![Norte](docs/imagens/navegador.png)

> **Language:** Norte's own menus and settings are in Portuguese for now. Run it with `NORTE_IDIOMA=en-US` and everything that comes from Chromium is in English.

## Features

- **Tabs in a sidebar.** Hide it with `Ctrl+S`; it comes back over the page when the pointer touches the left edge.
- **Spaces.** Each space has its own tabs, pinned sites, folders, bookmarks and color. Switch with the name at the top of the sidebar, the side buttons of your mouse over it, or `Ctrl+Alt+←/→`.
- **Containers.** Separate cookies and logins inside the same profile, like Firefox containers. A space can open its tabs in a container.
- **Multiple profiles**, each in its own window.
- **Ad and tracker blocker.** Uses EasyList, EasyPrivacy, EasyList Portuguese and uBlock Origin's fixes. It blocks requests, hides ad slots and repairs sites that depend on the blocked scripts. You can allow a site from its icon in the address bar.
- **Customizable new tab**, with the time, the date and shortcuts: most visited, your own list or nothing.
- **Address bar in the site's color**, sampled from the top of the open page.
- **Import from Zen:** containers, spaces, pinned sites, each container's cookies, passwords, bookmarks and history.
- **Updates itself:** a notice at the bottom of the sidebar tells you when a new version is out.
- **No AI features, no Google account and no usage statistics.**

## Install

Versions are published in [LucasLomiento/norte-releases](https://github.com/LucasLomiento/norte-releases), signed with a GPG key.

- **Arch Linux and derivatives** (CachyOS, EndeavourOS, Manjaro): the signed pacman repository, which updates with the rest of your system. [Step by step](https://github.com/LucasLomiento/norte-releases#readme).
- **Debian 12+, Ubuntu 24.04+ and derivatives:** the `.deb` from the [latest version](https://github.com/LucasLomiento/norte-releases/releases/latest). Norte itself tells you about new versions and installs them.
- **Windows:** no release yet. The code is prepared (see "Build"), but it hasn't been tested.

## Build

This repository only holds what belongs to Norte: the changes to Chromium's files (`patches/`), the new code (`navegador/`) and the scripts (`tools/`). Chromium is downloaded separately, at the version in `CHROMIUM_VERSION`.

1. Install [depot_tools](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html) and fetch the Chromium tag from `CHROMIUM_VERSION` with `gclient`. Use a `src` solution with `"managed": False`.
2. `tools/fonte.sh` links `navegador/` into the tree, applies the patches, swaps the product name in the strings and adds the icons.
3. **Linux:** `tools/compilar.sh out/release chrome chrome_sandbox chrome_crashpad_handler` uses the arguments in `tools/args-release.gn`. Then `tools/listas.sh` builds the blocker's lists and `makepkg` in `pacote/` builds the package. `tools/outros-pacotes.sh` turns that package into a `.deb`.
4. **Windows** (experimental, untested; cross-compiled from Linux):
   - `tools/vm-ferramentas-windows.sh subir` creates a virtual machine that installs Visual Studio and the Windows SDK and generates Microsoft's toolchain package.
   - Add `"win"` to the `target_os` in `.gclient` and run `gclient sync`.
   - Then `tools/windows.sh ferramentas`, `tools/windows.sh gerar`, `tools/windows.sh compilar` and `tools/windows.sh pacote`.

Building Chromium from scratch takes a few hours and about 16 GB of free RAM. `tools/compilar.sh` runs at low priority.

How each part works (in Portuguese): [docs/ARQUITETURA.md](docs/ARQUITETURA.md).

## Contributing

Bug reports, suggestions and pull requests are welcome, in English, Portuguese or Spanish: see [CONTRIBUTING.md](CONTRIBUTING.md). Every change goes through a pull request with review. Security issues go through the private channel in [SECURITY.md](SECURITY.md).

## Licenses

- **Norte's code:** BSD-3-Clause ([LICENSE](LICENSE)), the same as Chromium.
- **Chromium:** BSD and the licenses of its components (`chrome://credits`). Norte is not a Google product.
- **Geist and Bricolage Grotesque fonts:** SIL Open Font License 1.1 (`navegador/app/fontes`).
- **[Lucide](https://lucide.dev) icons:** ISC (`navegador/icones`).
- **Blocker lists**, downloaded at packaging time: EasyList under GPL v3 or CC BY-SA 3.0, and uBlock Origin under GPL v3.
