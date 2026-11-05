# Inkay Updater

A Wii U app (Aroma) that keeps **Inkay** up to date for **Pretendo Network** and **Protarium Network**, in every Aroma environment on your SD card.

It downloads the latest release straight from the official repositories:

| Network | Repository | File |
|---|---|---|
| Pretendo | [PretendoNetwork/Inkay](https://github.com/PretendoNetwork/Inkay/releases) | `Inkay-pretendo.zip` |
| Protarium | [Protarium-Network/Inkay-GitHub-Release](https://github.com/Protarium-Network/Inkay-GitHub-Release/releases) | `wiiu.zip` |

## Features

- **Every environment in one list**: e.g. `aroma` and `Aroma Protarium`, each with its own network.
- **Shows whether an update is available**: the installed version is read from the plugin itself, so it also works for an Inkay you installed by hand.
- **Detects the network** from the plugin's own description (Pretendo or Protarium), not from file names.
- **Safe install**:
  - The download is checked first: it has to contain exactly one valid plugin (`.wps`) and module (`.wms`).
  - The new files are written next to the old ones and read back.
  - Only then are they swapped in. If anything fails, the old Inkay is put back, so a bad download or a full SD card never leaves you with a broken Inkay.
- **Background music**: an original track by Senne Caelen that loops seamlessly. Press **Y** to turn it on or off.
- **No double Inkay**: another Inkay file under a different name in the same environment (which would load twice) is switched off by renaming it to `.disabled`.

## Install

1. Download `inkay_updater.wuhb` from the [latest release](https://github.com/sennecaelen/Inkay-Updater/releases).
2. Put it in `sd:/wiiu/apps` on your SD card.
3. Start **Inkay Updater** from the Wii U Menu.

Requires a Wii U with [Aroma](https://aroma.foryour.cafe/).

## Usage

| Button | Action |
|---|---|
| Up / Down | Choose an environment |
| Left / Right | Choose the network to install (Pretendo / Protarium) |
| A | Install or update |
| + | Check for updates again |
| Y | Music on/off |
| B (after installing) | Restart the console |
| HOME | Exit |

Each row shows the installed network and version, and the status: *Up to date*, *Update available*, *Not installed*, *Version unknown* or *Switch network*. A new environment without Inkay defaults to Protarium when its name contains "protarium", otherwise to Pretendo.

Restart the console after installing so Aroma loads the new Inkay.

## Files

In each environment the app writes:
- `modules/Inkay-pretendo.wms` and `plugins/Inkay-pretendo.wps`. Both networks use these names, like their own installers do.
- `plugins/config/inkay_updater.json`: what was installed, only used when a plugin has no version inside it.

## Building

With [devkitPro](https://devkitpro.org/) (devkitPPC, wut, wiiu-curl, wiiu-mbedtls):

```bash
make
```

The app ends up in `build/inkay_updater.wuhb`. GitHub Actions builds every push, and pushing to the `release` branch publishes the version in `inkay/version.hh` as a release.

## Credits

- Based on [Roséverse Installer](https://github.com/Project-Rose/Roseverse-Installer) by Project Rosé & contributors (MIT). Its eg2 framework (rendering, input, network) is used unchanged apart from a small download fix.
- [Pretendo Network](https://pretendo.network/) and [Protarium Network](https://github.com/Protarium-Network) for Inkay.
- [minimp3](https://github.com/lieff/minimp3) (CC0) for playing the music.
- Music: original composition by Senne Caelen.
- Third-party licenses are in `licenses/`.
