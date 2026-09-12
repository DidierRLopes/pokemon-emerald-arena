# Reconstruir

La descarga del release ya está compilada: un usuario no necesita toolchain GBA.
Para crear la ROM localmente sin navegador: Node 22+ y
`node release/install.mjs original.gba Emerald-Arena.gba`.
No reemplaza archivos existentes ni acepta una ROM distinta del hash de origen.

## Fuente de la arena

```sh
git clone https://github.com/pret/pokeemerald.git workspace
git -C workspace checkout 5eff78649e7170a877b961ef0b3da13b81a16038
git -C workspace apply ../game/native-engine.patch
cp -R game/overlay/. workspace/
```

Preparar el toolchain siguiendo el `INSTALL.md` de esa revisión. Build validado
en macOS arm64 con agbcc `da598c1d918402c42c0c0d7128ba14567f3175e9`, binutils ARM
2.47, cpp-15, Python 3, libpng y compilador C del host. El wrapper actual usa
prefijos Homebrew `/opt/homebrew`; adaptar esos prefijos en otra plataforma.

```sh
cd workspace
./tools/arena/dev.sh assets
./tools/arena/dev.sh build
./tools/arena/dev.sh release
```

`assets` descarga las fuentes fijadas y las transforma sin recorte; no las subas
a Git. El build guarda lab y release por separado. El archivo público de
preparación comprueba cada PNG, cada bloque convertido y el SHA-256 final.

Las pruebas públicas de instalador no requieren ROM:

```sh
node --test tests/installer.test.mjs
```

La batería de 211 comprobaciones de juego se ejecutó en el laboratorio privado
sobre mGBA real, no sobre mocks. Los snapshots/SAV y las evidencias completas
permanecen privados. Los tests C de navegación, física, geometría y números
están en el overlay y pueden compilarse con sus macros `*_HOST` y sanitizers.
