# Build

The release is already compiled. Players do not need a GBA toolchain.
For local setup without a browser, use Node 22+:

```sh
node release/install.mjs original.gba Emerald-Arena.gba
```

The installer rejects the wrong source ROM and never overwrites an existing file.

## Game source

Run these commands from this repository:

```sh
git clone https://github.com/pret/pokeemerald.git workspace
git -C workspace checkout 5eff78649e7170a877b961ef0b3da13b81a16038
git -C workspace apply ../game/native-engine.patch
cp -R game/overlay/. workspace/
```

Set up the toolchain using that revision's `INSTALL.md`. The verified build
uses macOS arm64, agbcc `da598c1d918402c42c0c0d7128ba14567f3175e9`,
ARM binutils 2.47, cpp-15, Python 3, libpng and the host C compiler.
The wrapper uses Homebrew's `/opt/homebrew` prefix; adapt it on other platforms.

```sh
cd workspace
./tools/arena/dev.sh assets
./tools/arena/dev.sh build
./tools/arena/dev.sh release
```

`assets` downloads pinned sources and converts them without cropping.
Keep generated sprite assets out of Git. Lab and release builds are separate.

## Checks

Public installer tests do not need a ROM:

```sh
node --test tests/installer.test.mjs
```

The private game acceptance suite runs 211 checks against real ARM code in mGBA,
not a mock engine. Saves, snapshots and full test evidence remain private.
Host C tests for navigation, physics, geometry and numbers are included in the
overlay; compile them with their `*_HOST` macros and sanitizers.

The public installer verifies every source PNG, every converted graphics block
and the final ROM hash. Release boot and local reconstruction are tested
separately. Physical GBA hardware has not been validated.
