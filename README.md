# Emerald Arena

Pokémon Emerald, pero las peleas se juegan en tiempo real.
Movimiento en ocho direcciones, esquivas, poderes distintos y un escenario que se rompe.
HP, PP, experiencia y equipo siguen siendo parte de la partida de Esmeralda.

## Probar

[**Descargar la demo 0.3**](https://github.com/GBurgardt/pokemon-emerald-arena/releases/latest)

Abrí `Preparar-Emerald-Arena.html`, elegí tu ROM limpia de Emerald (USA/Europe)
y descargá el resultado. Tu archivo se procesa localmente, no se sube.
La preparación necesita Internet; después, el juego funciona sin conexión.

Abrí el resultado en un emulador GBA. En NUEVA PARTIDA, sin guardado previo,
pulsá **SELECT** para el equipo de práctica. En el mapa, **L+R** inicia combate.

| Control GBA | Acción |
|---|---|
| Cruceta | Moverse en ocho direcciones |
| A | Usar poder; cruceta + A dirige el ataque |
| B + cruceta | Esquiva corta |
| L / R | Poder anterior / siguiente |
| START | Pausa y ayuda |
| SELECT | Volver al combate clásico |
| L + R en el mapa | Otro rival de práctica |

Guardá desde el menú del mapa. El atajo de práctica no cura ni reinicia tu equipo.
Las partidas normales conservan sus encuentros y no reciben el equipo de demo.

## Qué incluye

- Doce Pokémon con animación en ocho direcciones: Charizard, Blastoise, Eevee,
  Dragonite, Scizor, Blaziken, Treecko, Poochyena, Bulbasaur, Squirtle, Grovyle y Sceptile.
- Diez perfiles: Destructor, Placaje, Ataque Rápido, Malicioso, Absorber,
  Pistola Agua, Ataque Ala, Atizar, Picotazo y Arañazo.
- Rocas, madera, hojas, cristal y vainas explosivas; colisión, esquirlas y cadenas.
- IA que navega alrededor de obstáculos, apunta, esquiva y varía con el nivel.
- Entrada y resultado rápidos. Se mantienen las decisiones de aprendizaje,
  cambios de equipo y evolución. El daño y las recompensas son nativos.

Es una **demo jugable**, no una aventura completa rebalanceada. Los movimientos,
habilidades y situaciones aún no compatibles conservan el combate clásico.
Lanzallamas, por ejemplo, todavía no tiene su versión de arena. No incluye Tyranitar.

## Para desarrollar

Este repositorio publica nuestro código como overlay y parche sobre
[pret/pokeemerald](https://github.com/pret/pokeemerald), pin
`5eff78649e7170a877b961ef0b3da13b81a16038`. No contiene la ROM, guardados ni sprites
extraídos. [Cómo reconstruir](BUILD.md). [Créditos](CREDITS.md).

`release/installer.mjs` prepara la ROM en el navegador o Node. El payload
contiene un delta BPS compilado con las zonas PMD vacías y un manifest fijado.
El preparador obtiene los recursos de su fuente, reconstruye esos bloques en
tu equipo y exige SHA-256 idéntico a la ROM de release validada.

La lógica del juego corre dentro de la GBA: no es una recreación web ni una
película que simula gameplay. El laboratorio privado verificó 211 comprobaciones
sobre su hash de aceptación; el inicio rápido de release y el ensamblado público
se verifican aparte. No se afirma haber validado esta release en hardware GBA físico.
