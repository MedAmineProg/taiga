# Taiga

[![](https://img.shields.io/github/license/MedAmineProg/taiga)](LICENSE)
[![](https://img.shields.io/github/v/release/MedAmineProg/taiga?include_prereleases)](https://github.com/MedAmineProg/taiga/releases)
[![](https://img.shields.io/github/actions/workflow/status/MedAmineProg/taiga/build.yml?branch=develop&label=build)](https://github.com/MedAmineProg/taiga/actions/workflows/build.yml)

[Taiga](https://taiga.moe) is an open-source desktop anime tracker. It detects the anime you watch on your computer and keeps your progress in sync with [AniList](https://anilist.co), [Kitsu](https://kitsu.app) and [MyAnimeList](https://myanimelist.net). It also manages your anime library, tells you when new episodes air and shows what you've been watching.

This is a fork of [erengy/taiga](https://github.com/erengy/taiga), built on the Taiga 2.0 rewrite (Qt 6, C++23). It adds the features below and runs on Windows, Linux and macOS.

> [!WARNING]
> Taiga 2.0 is in alpha. Back up your list on your service before trying it.

## Download

Test builds for Windows are on the [Releases](https://github.com/MedAmineProg/taiga/releases) page:

1. Download `Taiga-<version>-windows-x64.zip`.
2. Extract it anywhere and run `Taiga.exe`.

Taiga is portable: settings, the anime database and logs are kept in the `data` folder next to `Taiga.exe`, so deleting the folder removes everything. Windows SmartScreen may warn that the app is unrecognized, since builds aren't signed. Choose **More info** → **Run anyway**.

On Linux and macOS, [build it from source](#building).

## Features

### Tracking and sync

- **Automatic detection:** Taiga recognizes the episode you're playing and updates your list after you've watched it.
  - **Windows:** supported media players and web browsers, through [Anisthesia](https://github.com/erengy/anisthesia).
  - **Linux:** players that support MPRIS (VLC, mpv with the MPRIS plugin, Celluloid, Haruna, SMPlayer, Totem…), plus video files opened by players without MPRIS support, such as plain mpv.
  - **macOS:** video files opened in IINA, mpv, VLC, QuickTime Player, Movist, Infuse or Kodi.
- **Main service:** your list lives on one service (AniList, Kitsu or MyAnimeList). Changes are queued and sent in the background, even after a restart.
- **Mirror to other services** (*Settings › Accounts*): every change to your list can also be sent to the other services you log in to, so all of them stay up to date.
  - **Copy my list** sends your whole list to a service once.
  - Anime are matched across services automatically, and a status line shows what's waiting to be sent or couldn't be.
- **Reliable syncing:** failed requests are retried with increasing delays, and rate limits from the services are respected.
- **Undo:** each list change shows a notification, such as *"Dandadan · Episode 7 watched"*, with an **Undo** button.

### Home

- **Greeting:** a summary of what's new, such as episodes ready to watch or airing in the next 24 hours.
- **Continue watching:** the anime you're watching, with your progress, and badges for new episodes that have aired (**NEW**) or are in your library folders (**READY**).
- **Airing this week:** upcoming episodes of the anime you're watching or planning to watch.

### Airing calendar

- **Week view:** upcoming episodes for the week, with previous and next week buttons.
- **Filters:** show only the anime you're watching, or hide the ones you plan to watch.
- **Notify me:** get a notification when an episode of an anime you're watching airs.

### Statistics

- **Overview:** time watched, episodes, mean score and your current streak.
- **Activity:** a heatmap of the episodes you watched each day over the last year.
- **Scores, top genres and top studios:** based on your list and watch history.

### Everywhere else

- **Command palette:** press <kbd>Ctrl</kbd>+<kbd>K</kbd> to search anime, pages and actions.
- **Discord Rich Presence** (*Settings › Sharing › Discord*): shows the anime you're watching, the episode, its cover, the time elapsed and a *View anime* link on your Discord profile.
- **Secure accounts:** access tokens and passwords are kept in the system's credential store (Windows Credential Manager, macOS Keychain, or Secret Service or KWallet on Linux), not in plain text.
- **Dark and light themes:** the theme follows your system and switches live.
- **Library:** scans your anime folders to find episodes you have and play the next one.

## Building

Requirements:

- **CMake** 3.21 or later, and **Ninja**.
- **Qt** 6.10 or later, with the Qt SVG module.
- **A C++23 compiler:**
  - **Windows:** Visual Studio 2022 (MSVC).
  - **Linux:** GCC 15.
  - **macOS:** Homebrew LLVM. Apple's Clang lacks some C++23 library features.

Clone with submodules, then configure and build:

```sh
git clone --recursive https://github.com/MedAmineProg/taiga.git
cd taiga
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The executable is written to `bin/`.

**Platform notes:**

- **Windows:** run the commands from a *Developer PowerShell for VS 2022*, and point CMake to Qt with `-DCMAKE_PREFIX_PATH=C:/Qt/6.10.x/msvc2022_64`.
- **Linux:** install the OpenGL, EGL, xkbcommon and libsecret development packages. For Ubuntu, see the Linux job in [`build.yml`](.github/workflows/build.yml).
- **macOS:** run `brew install llvm ninja`, then set `CC`, `CXX` and `LDFLAGS` to Homebrew LLVM as in the macOS job in [`build.yml`](.github/workflows/build.yml).

**Tests:** add `-DTAIGA_BUILD_TESTS=ON` and run `ctest --test-dir build`.

### Making a release

On GitHub, go to **Actions › Release › Run workflow** and enter a version, such as `2.0.0-alpha.1`. Pushing a `v*` tag works too. The workflow builds, tests and packages Taiga for Windows. It then publishes the package as a pre-release.

## Related projects

- [Anime relations](https://github.com/erengy/anime-relations) (episode redirections)
- [Anisthesia](https://github.com/erengy/anisthesia) (media detection library)
- [Anitomy](https://github.com/erengy/anitomy) (anime video filename parser)

## License

Taiga is licensed under [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.html). It was created by [Eren Okka](https://github.com/erengy) and contributors.
