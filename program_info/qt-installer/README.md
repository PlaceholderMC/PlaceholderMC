# Qt Installer Framework files

This ~~is~~ will be the primary way we distribute our binaries on Windows. See it's docs [here](https://doc.qt.io/qtinstallerframework/index.html)


## Quick tutorial

The scripts for the installer (and thus the installer itself) currently only support Windows. Binary downloads for it can be found:

- [Here](https://download.qt.io/official_releases/qt-installer-framework/) for MSVC 2019
- In [MSYS2's](https://www.msys2.org/) repositories for MINGW

Once you have it installed

- Place a `.zip` release artifact at `packages\org.primslauncher.PrismLauncher\data\prismlauncher.zip`
- Place a vcredist exe at `packages\com.microsoft.VCRedist\data\vc_redist.x64.exe` (or `vc_redist.arm64.exe for ARM)
- Run `\path\to\qtif\bin\binarycreator.exe --offline-only -c config\config.xml -p packages prismlauncher-setup.exe`

The resulting exe may be launched as normal by clicking on it or from the CLI -- the latter is very useful for debugging with the `--verbose` flag

## TODO

- [ ] Add start menu icon
- [ ] Register file associations
- [ ] Download QtIF in CI (easy for MSYS2, will need to probably download and verify manually for MSVC)
- [ ] Download correct vcredist exe in CI
- [ ] Test on ARM
- [ ] Support macOS/Linux?
- [ ] Setup remote repository to replace current updater? ([docs](https://doc.qt.io/qtinstallerframework/ifw-updates.html))
