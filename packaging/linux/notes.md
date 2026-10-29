> **This is an unofficial Linux port of the XR vessels, maintained by racerx2. It is not made or supported by Doug Beachy. For help or bug reports, file an issue in this repository ([racerx2/XRVessels](https://github.com/racerx2/XRVessels/issues)) or contact racerx2. Please do not file issues in the official [dbeachy1/XRVessels](https://github.com/dbeachy1/XRVessels) repository or contact dbeachy1 for support.**

Native Linux builds of the XR1 Deltaglider, XR2 Ravenstar and XR5 Vanguard for the Linux/Vulkan edition of Orbiter ([racerx2/orbiter-2027-linux](https://github.com/racerx2/orbiter-2027-linux)). Ported from [dbeachy1/XRVessels](https://github.com/dbeachy1/XRVessels) version 2.0 with Doug Beachy's blessing; the source is this repository at @COMMIT@.

**Version @VER@**: Doug Beachy's version 2.0 is the base, and the port has its own `-linux-1.x` version line. This is not a new official XR release.

### Changes
@CHANGES@

### Needs
@ORBITER@ for Linux or newer ([releases](https://github.com/racerx2/orbiter-2027-linux/releases)). The modules are built the way that Orbiter release is built (AlmaLinux 9, GCC 14, Qt 6.8, against its SDK), so they run on every distribution it runs on.

### Downloads
- **DeltaGliderXR1-@VER@-x64.zip**: the complete XR1 (module, meshes, textures, configs, scenarios, manual).
- **XR5Vanguard-@VER@-x64.zip**: the complete XR5, with the XR payload meshes and the XR5-ER payloads.
- **XR2Ravenstar-@VER@-so-x64.zip**: the XR2 Linux module only. Install Doug Beachy's official XR2 Ravenstar 2.0 package first, then unzip this over it. The XR2 mesh and textures are copyright Steve Tyler (Coolhand) and are distributed only by Doug, so they are not included.

Each zip has a `.sha256` file, the GPL v3 licence (`Doc/XRVessels-LICENSE.txt`) and a readme (`Doc/<vessel>-Linux.txt`).

### Install
Unzip into your Linux Orbiter folder (the one with the Orbiter program, `Modules/` and `Config/`), keeping the folder structure. The zips have the same layout as Doug's packages, with `Modules/<vessel>.so` in place of the Windows `.dll`.

### Checked before release
@CHECKS@

### Credits
From the XR Flight Operations Manual (included in the XR1 and XR5 zips, with the full credits and licence notices):
- **XR vessels (all)**: Douglas Beachy: concept, systems design, C++ code, 2D graphics, dialogs, documentation.
- **DG-XR1**: Don Gallagher (3D mesh tweaks, hover doors, SCRAM doors). Based on the DeltaGlider-S mesh and virtual cockpit by Frying Tiger, and on Martin Schweiger's DeltaGlider sample vessel (MIT licence).
- **XR5 Vanguard**: Russell Hicks (3D modeling, internal cockpit textures, model design, cargo module models, some animation code), PennyBlack (ship textures), Carmen A (fuel, LOX and H2O cargo modules), Tom Fisher (AIA cargo module), Don Gallagher (early prototype meshes).
- **XR2 Ravenstar**: Steve "Coolhand" Tyler (concept, model design, 3D modeling, textures, cargo module models, Lee and Kara astronaut models, turbopack models, custom main/hover/RCS/SCRAM sound effects).
- **Voice callouts (all vessels)**: Sally Beaumont.
- **Astronaut meshes**: Greg Burch.
- **Orbiter**: Dr. Martin Schweiger.
- **Linux port**: racerx2.

### Licence
GNU GPL v3, as the original XR vessels.

### SHA-256
```
@SUMS@
```
