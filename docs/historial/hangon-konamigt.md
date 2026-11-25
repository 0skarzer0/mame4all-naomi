# Historial: Hang-On y Konami GT (2026-11-04 a 11-13)

> Historial: diario de sesiones copiado tal cual del antiguo `HANDOFF.md` (2026-11-25). Puede estar superado por lo escrito después; lo vigente está en [HANDOFF.md](../../HANDOFF.md).

## Estado (2026-11-04, primera prueba en placa real)

**En placa real (NAOMI 2, mueble Crazy Taxi) los dos juegos van lentos y se cuelgan; en Flycast iban a 60.**

| Juego | Overlay en placa | En Flycast | Observado |
|---|---|---|---|
| Hang-On | `47/16 100` (47 emulados, 16 mostrados, 100 % ocupado) | 60 / 41–55 | Muy poco framerate; se cuelga en la etapa 1 y no continúa |
| Konami GT | `46/13 100` | 60 / 60 | Sonido malísimo; se cuelga poco después de activar el contador de FPS |

- **Controles: perfectos en los dos** (volante, pedales, palanca, monedas). Validados en placa.
- Freno + START = crédito: no confirmado explícitamente.

### Sesión 2026-11-04 (noche)

- Confirmado por el usuario: **sin el contador de FPS no hay cuelgues** (los dos juegos un buen rato).
- Arreglo: el contador ya no escribe directamente en la textura (VRAM); se compone en RAM y se envía con
  las store queues junto con la imagen (`naomi_video.cpp`, `ovl[]`, `build_overlay`). Probado en Flycast;
  **pendiente de probar en placa**.
- Profiler ampliado (`-DPROFILER_MAME4ALL`): la 2ª línea del contador muestra, en décimas de ms por frame
  emulado: `total(0) 68K Z80 68Ksub sonido vídeo-juego subida+dibujo(por frame mostrado) salida-audio`.
- Medido en Flycast (Hang-On, demo): 68K 1–2,4 ms, Z80 2,1–2,5, 68K sub 3,1, sonido 0,9–1,8,
  vídeo del juego 4,7–6,6 ms por frame emulado (~10 ms por frame dibujado), subida+dibujo 1,7 ms.
  **En Flycast ya iba al límite (100 % ocupado)**, no sobrado. CPU al 75 % casi no cambia nada
  (los speed hacks ya ahorran la espera); sonido a 11025 Hz ahorra ~1 ms.
- Añadido `-falign-*=32` (línea de caché del SH-4), como el build original de Dreamcast (`ALIGN=1`).
- Entregadas builds de diagnóstico (profiler + contador activo al arrancar). **Siguiente paso:** foto del
  contador en placa, en carrera, para ver qué parte se dispara frente a Flycast.

### Sesión 2026-11-05 (madrugada): números de la placa real

Confirmado en placa: el contador compuesto en RAM **ya no cuelga**. Profiler en placa (décimas de ms por
frame emulado; subida+dibujo por frame mostrado):

| | 68K principal | Z80 | 68K sub | sonido | vídeo juego | subida+dibujo | fps emulados/mostrados |
|---|---|---|---|---|---|---|---|
| Hang-On carrera (placa) | **7,9** | 5,0 | 5,7 | 2,0 | 2,4 (≈10 por frame dibujado) | 3,1 | 42/10 |
| Hang-On carrera (Flycast) | 2,2–2,7 | 2,1–2,6 | 3,1 | 1,2–1,9 | ≈10 por dibujado | 1,7 | 60 |
| Konami GT carrera (placa) | **9,2–9,5** | 4,0 | — | 1,3 | 4,0 | 2,4 | 50–54/14 |
| Konami GT carrera (Flycast) | 3,2–3,4 | 2,9–3,0 | — | 0,7–0,8 | — | 1,3 | — |

**Conclusión:** el dibujo cuesta lo mismo en placa y en Flycast; **los núcleos de CPU van 2–3,5× más lentos
en el SH-4 real** (FAMEC el peor: 3,3×; RAZE Z80 2,3×; Musashi 1,8×). Flycast no sirve para predecir la
velocidad de los núcleos de CPU.

Probado en Flycast, sin resultado útil:
- CPU al 63 % (la placa original lleva los 68000 a 6,25 MHz, MAME 0.35 a 10 MHz): no baja el tiempo del 68K
  principal (su trabajo por frame es fijo; el resto del tiempo ya lo salta el speed hack). Nota: un slice que
  termina por `spinuntil` cuenta como "ran" completo, así que "ran 100 %" no significa que no haya espera.
- Musashi para los dos 68000 (`NAOMI_ALL_MUSASHI`): 1,7× más lento que FAMEC en Flycast.
- FAME en ensamblador SH-4 (`FAMEC=0`): ~20 % más rápido que FAMEC en Flycast, **pero roto en los dos juegos**
  (velocidad 0 km/h: no lee el acelerador; en Konami GT, además, imagen corrupta). En placa real podría ser
  mucho más rápido que FAMEC: **arreglarlo es la vía más prometedora**.
- Sonido a 11025 Hz: ahorra ~0,4–1 ms.

Herramienta nueva `-DNAOMI_PCHIST`: filas 3-4 del contador = PCs donde terminan los slices de CPU 0 y 1
(top 3, % de slices) y % de ciclos ejecutados. Ojo: no está ponderado por tiempo; los NMI de sonido acortan
slices y sesgan el histograma.
- Hang-On, 68K principal: `0x3E9C` (justo tras escribir comandos de sonido en `0xE00000`, con bucles de
  retardo `dbra`). Z80: repartido por `0x1BD-0x1EB`.
- Konami GT, Z80: `0x08-0x0E` = rutina RST 08 (escritura/lectura del AY8910); cada escritura al AY
  provoca una actualización del stream de sonido → parte del coste del Z80 es generar audio.

**Siguientes pasos propuestos:**
1. Depurar FAME asm (por qué no lee el acelerador / handlers de lectura) y medirlo en placa.
2. Konami GT: le faltan ~2,6 ms por frame para 60. Candidatos: FAME asm, agrupar las escrituras al AY,
   sonido a 11 kHz.
3. Hang-On: le faltan ~8 ms; necesita los dos 68000 más rápidos.

### Hipótesis (sin comprobar)

1. **Causa común del port, no de cada juego:** dos drivers muy distintos se quedan en ~46–47 fps emulados.
   Flycast no reproduce el coste real de caché y memoria del SH-4. Candidatos: subida de textura por
   store queues, conversión de paleta, mezcla de audio, memoria sin caché o copias de bitmap.
   Primer paso: medir en placa con el profiler (`PROFILER_MAME4ALL`) qué parte se come el frame.
2. **Sonido malo = consecuencia de (1):** el audio espera 60 fps emulados; a ~46 se vacía el buffer.
3. **Cuelgues posiblemente ligados al overlay de FPS:** en ambas pruebas estaba activo. Sospecha:
   escrituras de byte en VRAM al dibujarlo (la VRAM real no las admite; ver arreglo de unzip/ROMs).
   Prueba: partida larga sin activar el overlay; revisar `naomi_video.cpp` (dibujo del overlay).

## Rendimiento medido (Hang-On, Flycast, carrera)

Vídeo ~7 ms, CPUs ~6–7 ms, sonido ~1–1,5 ms por frame → 60 emulados, 41–55 mostrados. La emulación no baja
de 60, así que el audio va bien.

## Siguientes pasos

1. Diagnosticar la lentitud en placa (profiler) y los cuelgues (overlay de FPS / escrituras de byte en VRAM).
2. Añadir Super Hang-On y Enduro Racer (mismo hardware System 16, poco trabajo).
3. Spy Hunter / Max RPM (MCR3), Turbo, Sprint 2, Super Sprint, Arkanoid (volante → velocidad de spinner).
4. Ficha por juego en `docs/games/`.

**Hang-On y Konami GT con PVR + CPU recompilada: no empezado.** Cuentas (placa, carrera, ms por frame; hay 16,6):
- Hang-On: 68K 7,9 + Z80 5,0 + 68K sub 5,7 + sonido 2,0 + vídeo 2,4 + otros 3,1 ≈ 26. Pasar el vídeo al PVR
  ahorra ~1,5; recompilar solo el 68K principal, quizá 2–3. No llega a 60 sin acelerar también el 68K sub y el Z80.
- Konami GT: 68K 9,2–9,5 + Z80 4,0 + sonido 1,3 + vídeo 4,0 + otros 2,4 ≈ 21. Vídeo al PVR (~3 ms) y 68K
  recompilado (~3 ms) lo dejarían cerca de 15–16: es viable, pero ajustado.
- El renderer PVR de Nemesis es más difícil que el de Super Sprint: los caracteres están en RAM
  (`char_dirty`, se redefinen en juego), hay scroll por línea (256 filas, `copyscrollbitmap`) y sprites con zoom.
- Un recompilador estático de 68000 es mucho mayor que el de T11 (modos de dirección, flags, 68000 con
  interrupciones y código automodificable). Alternativa más barata a probar primero: arreglar FAME en ensamblador
  (hoy roto en estos dos juegos, ~20 % más rápido en Flycast).
- Todo se debe medir en la placa; la Raspberry no respondía este día.
