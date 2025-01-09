# Third-party libraries

This folder has third-party or otherwise external libraries needed for other parts to work.

## filesystem

Gulrak's implementation of C++17 std::filesystem for C++11 /C++14/C++17/C++20 on Windows, macOS, Linux and FreeBSD.

See [github repo](https://github.com/gulrak/filesystem).

MIT licensed.

## gamemode

A performance optimization daemon.

See [github repo](https://github.com/FeralInteractive/gamemode).

BSD-3-Clause licensed

## cmark

The C reference implementation of CommonMark, a standardized Markdown spec.

See [github_repo](https://github.com/commonmark/cmark).

BSD2 licensed.

## javacheck

Simple Java tool that prints the JVM details - version and platform bitness.

Do what you want with it. It is so trivial that noone cares.

## launcher

Java launcher part for Minecraft.

It does the following:

- Waits for a launch script on stdin.
- Consumes the launch script you feed it.
- Proceeds with launch when it gets the `launcher` command.

If "abort" is sent, the process will exit.

This means the process is essentially idle until the final command is sent. You can, for example, attach a profiler before you send it.

The `standard` and `legacy` launchers are available.

- `standard` can handle launching any Minecraft version, at the cost of some extra features `legacy` enables (custom window icon and title).
- `legacy` is intended for use with Minecraft versions < 1.6 and is deprecated.

Example (some parts have been censored):

```text
mod legacyjavafixer-1.0
mainClass net.minecraft.launchwrapper.Launch
param --username
param CENSORED
param --version
param Prism Launcher
param --gameDir
param /home/peterix/minecraft/FTB/17ForgeTest/minecraft
param --assetsDir
param /home/peterix/minecraft/mmc5/assets
param --assetIndex
param 1.7.10
param --uuid
param CENSORED
param --accessToken
param CENSORED
param --userProperties
param {}
param --userType
param mojang
param --tweakClass
param cpw.mods.fml.common.launcher.FMLTweaker
windowTitle Prism Launcher: 172ForgeTest
windowParams 854x480
userName CENSORED
sessionId token:CENSORED:CENSORED
launcher standard
```

Available under `GPL-3.0-only` (with classpath exception), sublicensed from its original `Apache-2.0` codebase

## libnbtplusplus

libnbt++ is a free C++ library for Minecraft's file format Named Binary Tag (NBT). It can read and write compressed and uncompressed NBT files and provides a code interface for working with NBT data.

See [github repo](https://github.com/ljfa-ag/libnbtplusplus).

Available either under LGPL version 3 or later.

## LocalPeer

Library for making only one instance of the application run at all times.

BSD licensed, derived from [QtSingleApplication](https://github.com/qtproject/qt-solutions/tree/master/qtsingleapplication).

Changes are made to make the code more generic and useful in less usual conditions.

## murmur2

Canonical implementation of the murmur2 hash, taken from [SMHasher](https://github.com/aappleby/smhasher).

Public domain (the author disclaimed the copyright).

## qt-qrcodegenerator

A simple library for generating QR codes

See [github repo](https://github.com/nayuki/QR-Code-generator).

MIT

## quazip

A zip manipulation library.

LGPL 2.1 with linking exception.

## rainbow

Color functions extracted from [KGuiAddons](https://inqlude.org/libraries/kguiaddons.html). Used for adaptive text coloring.

Available either under LGPL version 2.1 or later.

## systeminfo

A Prism Launcher-specific library for probing system information.

Apache 2.0

## tomlplusplus

A TOML language parser. Used by Forge 1.14+ to store mod metadata.

See [github repo](https://github.com/marzer/tomlplusplus).

Licenced under the MIT licence.

## qdcss

A quick and dirty css parser, used by NilLoader to store mod metadata.

Translated (and heavily trimmed down) from [the original Java code](https://github.com/unascribed/NilLoader/blob/trunk/src/main/java/nilloader/api/lib/qdcss/QDCSS.java) from NilLoader

Licensed under LGPL version 3.
