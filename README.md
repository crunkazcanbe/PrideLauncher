# Pride Launcher

> ## ⚠️ Beta - not fully working yet
> Pride Launcher is in **early beta**. The Pride modpack's website and pack server it is built for are still under
> construction, so one-click installs and pack updates will not work for everyone yet. Expect rough edges and please
> report bugs in the issues.

A fork of **[Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) 11.0.3** made for the Pride modpack, a
large Minecraft 1.12.2 pack (Lite and Heavy editions). It keeps everything Prism does and adds a home screen and
updater for the pack. **Not affiliated with or endorsed by the Prism Launcher project.**

## What it adds
- **Pride home window:** dusk artwork, a block-style wordmark and big **Install / Update / Play** buttons for the pack.
- **Pack updater:** reads a pack manifest, downloads only changed files (SHA-1 verified, delta updates) and keeps the
  player's own config edits.
- **Self-configuring first run:** finds a suitable Java automatically, picks RAM for the edition, applies the theme and language.
- Own app id and data folder, so it can sit next to a normal Prism Launcher install.
- Fix: the Java setup wizard page no longer has "Prism Launcher" hard-coded.

The pack manifest URL defaults to a placeholder (`https://pride.example.com/pack/manifest.json`) and can be pointed at
any server with an environment variable (see `launcher/pride/PackUpdater.cpp`).

## Building
Same as Prism Launcher: see the [Prism build instructions](https://prismlauncher.org/wiki/development/build-instructions/).
```
cmake --preset linux
cmake --build --preset linux
```
Prism's own API keys are removed, as their fork policy asks. Add your own (Microsoft client ID, CurseForge key) in
`CMakeLists.txt` / `buildconfig` if you build it for real use.

## License and credits
GPL-3.0, like Prism Launcher (see [`LICENSE`](LICENSE) and [`COPYING.md`](COPYING.md)). All of Prism Launcher's code
and its contributors' copyrights are kept; the Pride changes are in one commit on top of 11.0.3.

Built on [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) by the Prism Launcher contributors (itself a
fork of PolyMC / MultiMC).

Made with [Claude Code](https://claude.com/claude-code) and [Blockbench](https://www.blockbench.net).
