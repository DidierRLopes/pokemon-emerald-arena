# Emerald Arena 0.3

Esmeralda con combate en tiempo real, ocho direcciones, poderes y escenario
destructible. Requiere tu propia ROM limpia de Emerald (USA/Europe).

1. Abrí **Preparar-Emerald-Arena.html** en Chrome, Safari o Firefox actual.
2. Elegí tu `.gba` y descargá el resultado. Tu ROM se procesa localmente.
3. Abrila en tu emulador GBA. Pulsá SELECT en NUEVA PARTIDA, sin guardado previo.
4. L+R en el mapa abre una pelea. Charizard está primero; cambiá el orden en Pokémon.

La preparación necesita Internet para las animaciones. Después podés jugar sin
conexión. Alternativa terminal con Node 22+: `node install.mjs original.gba arena.gba`.

Controles GBA: flechas, A poder, B esquiva, L/R selección, START pausa,
SELECT combate clásico. Guardá desde el menú del mapa para conservar progreso.

Equipo: Charizard, Blastoise, Eevee, Dragonite, Scizor y Blaziken.
Caja 1: Treecko, Poochyena, Bulbasaur, Squirtle, Grovyle y Sceptile.
Diez perfiles de poder; los efectos todavía no adaptados conservan combate clásico.
Es una demo jugable, no una aventura completa rebalanceada.

La descarga contiene código compilado diferencial y el preparador, no ROMs,
partidas ni láminas de sprites. Verifica los recursos fijados de SpriteCollab y
exige que el resultado sea idéntico al release validado, byte por byte.

Proyecto y créditos: https://github.com/GBurgardt/pokemon-emerald-arena
