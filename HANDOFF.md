# HANDOFF — estado del port MAME4ALL → NAOMI

Documento de traspaso entre sesiones (personas o IAs). **Actualízalo en cada sesión.**

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

### Super Sprint (`GAME=ssprint`, 2026-11-05)

- Driver Atari System 2 (`src/drivers/atarisy2.cpp`, `machine/atarisy2.cpp`, `vidhrdw/atarisy2.cpp`,
  `machine/atarigen.cpp`, `machine/slapstic.cpp`), CPUs T11 (`src/t11`) + 6502 (`src/m6502`),
  YM2151 + 2 POKEY. `-DUSE_T11_GP -DUSE_M6502_GP`.
- El romset del usuario es la **revisión 1** (programa 136042-136/137/138/139, sonido 119/120); MAME 0.35 solo
  tenía la revisión 4 (325/329/330/331, 419/420). Añadida bajo `NAOMI_SSPRINT1`; gráficos idénticos.
  `romzip.py` empaqueta los ficheros por CRC.
- Pantalla 512x384 → textura PVR ampliada a 512x512 (`TEX_H 512`).
- Entrada `IPT_DIAL` (volante giratorio): el ángulo del volante da la velocidad de giro, zona muerta ±10,
  `NAOMI_DIAL_SPEED` (16 por defecto; **por ajustar en el mueble**). El acelerador es BUTTON1 (pedal > mitad).
- Atajos de espera (solo rev. 1, direcciones fijas):
  - T11 en `0x8530`: `TSTB @#2514` (octal = 0x54C) / `BEQ` espera a la interrupción → `ssprint_spin_r` en 0x54C
    hace `cpu_spinuntil_int()`. T11 4,6 → 3,5 ms (Flycast).
  - 6502 en `0x84A9-0x84AE`: espera a que el T11 lea el byte enviado → `cpu_yield()` en `atarisys2_6502_switch_r`.
  - 6502 bucle principal `0x80FA-0x8137` con las dos colas vacías (lectura de 0x211 en 0x8122) →
    `cpu_spinuntil_int()`. 6502 4,9 → 0,9 ms.
- Resultado Flycast (demo, sin `NAOMI_FLYCAST`): 60 emulados / 60 mostrados, ~97 % ocupado. Juego y sonido OK
  (captura de audio con sonido en partida; la demo es muda).
- **Velocidad del juego (2026-11-05):** el usuario notó que iba más lento que la recreativa aun a 60 fps.
  Contador de pasadas del bucle principal del T11 (última cifra de la 1ª línea del contador, `/ N`):
  60 en el título pero **30 en carrera** al 100 % de reloj, con y sin atajos (`-DNAOMI_SS_NOSPIN`). El núcleo
  T11 de MAME 0.35 cuenta demasiados ciclos por instrucción. Al 150 %: 45–56; **al 200 %: 59–60**.
  Por defecto `NAOMI_CLOCK_CPU=200` para ssprint (Makefile). Coste T11 en Flycast: 3,5 → 9 ms. **Pendiente:
  medir en placa**; si no llega, opciones: 150 %, o acelerar el núcleo T11.
- **Placa real con T11 al 200 % (vídeo 2026-11-05):** velocidad de juego y música correctas, pero 43–46 fps
  emulados (bucles 37–46). Carrera: T11 10,2 ms, 6502 4,6 ms (0,9 en Flycast: 5×), sonido 3,3 ms, vídeo 1,9,
  subida+dibujo 8,3 ms por frame mostrado (512x384). Entregada también una versión **sin sonido**
  (`-DNAOMI_SOUND=0`, `ssprint_nosound_...`) para comparar.
- Volante: el usuario lo encontró demasiado sensible → curva cuadrática (suave cerca del centro) y
  `NAOMI_DIAL_SPEED` 16 → 10. Pendiente de probar.
- **Sin sonido en placa (vídeo 2026-11-05):** 54/14, bucles 53; T11 10,7 ms, 6502 2,5 (con sonido 4,6), sonido 0,1,
  vídeo 1,6, subida+dibujo 8,3 ms por frame mostrado. ≈17 ms/frame → le falta ~1 ms para 60. Ahora el límite es
  el **T11** (10,7) y la **subida de la imagen 512x384** (8,3 ms en placa, 4,5 en Flycast). Ideas: textura
  paletizada de 8 bits (exige twiddle), menos slices de intercalado (`interleave` 10), acelerar el núcleo T11.
- La versión con sonido pasa a 11025 Hz (`NAOMI_RATE=11025`) para ahorrar ~1,5–2,5 ms.
- Volante (2.ª petición del usuario): "como con teclado". Ahora digital con dos velocidades: zona muerta ±14,
  velocidad lenta fija (`NAOMI_DIAL_SLOW` 40/16 por frame) en casi todo el recorrido y rápida
  (`NAOMI_DIAL_FAST` 96/16) pasado ±105 de 127. Pendiente de probar.
- **Vídeo en 8 bits (2026-11-05, petición del usuario):** textura PVR paletizada de 8 bits, 512x512, twiddled
  (bloques de 32 bytes = 4x8 texels por store queue, tablas `tw_x`/`tw_y`), paleta del juego en la RAM de paleta
  del PVR (RGB565) cada frame. Mitad de bytes a VRAM y sin traducir cada píxel. Flycast: subida 4,5 → 3,7 ms
  (Super Sprint), 1,7 → 1,3 (Hang-On). `-DNAOMI_VIDEO16` = camino anterior.
- **Sin filtro ni escalado** (petición del usuario): filtro NONE (antes bilineal); imagen 1:1 centrada; doble
  (píxeles enteros) solo si cabe (320x224 → 640x448). Super Sprint sale a 512x384 con bordes.
- Contador de FPS en su propia textura ARGB1555 (lista TR), se reenvía solo cuando cambia (1 vez/s).
- El contador de FPS ya no sale al arrancar (`build.sh ssprint 0`); se activa con START 3 s.
- Nota: los builds `-DNAOMI_FLYCAST` (lectura directa del MIE) cuestan casi 2 frames por llamada: **no sirven
  para medir velocidad**, solo para probar controles. Medir con `ssperf.sh`/`perf.sh` (sin entrada).

### Recompilación estática del T11 de Super Sprint (2026-11-12)

Petición del usuario: "recompila supersprint para naomi2".

- **Diagnóstico (Flycast, `-DNAOMI_T11STAT`):** ~13.200 instrucciones T11 por frame; solo ~2.200 accesos van a
  handlers de E/S (~1,5 ms). El resto (~6,8 ms) es interpretar: ~100 ciclos SH-4 por instrucción T11.
  El núcleo cobra 22 ciclos fijos por instrucción (por eso hacía falta el 200 %: la media real es ~12).
- **Accesos rápidos** en `src/t11/t11.cpp` (solo `NAOMI_SSPRINT1`): RAM 0x0000-0x0fff, ROM con banco 0x4000-0x7fff y
  ROM fija 0x8200-0xffff se leen directo de `state.bank[]`; RAM se escribe directo. Excepciones: el slapstic
  (0x8000-0x81ff) y el byte del atajo 0x54C siguen por handler. Ganancia pequeña (~5 %).
- **Recompilador** `naomi/tools/t11recomp.py`: descubre el código del programa fijo (0x8000-0xffff) siguiendo saltos
  desde 0x8000, inmediatos que parecen direcciones de código y `naomi/tools/ssprint_t11_entries.txt` (direcciones
  que caían al intérprete, sacadas con `-DNAOMI_T11RC_MISSES`). Cada bloque (hasta un salto, máx. 64 instrucciones)
  es una función C que ejecuta **los mismos cuerpos de instrucción de t11ops.h** con el opcode y los operandos como
  constantes (`OPC`, `I0`, `I1`; `ROPCODE()` redefinido). Tras cada instrucción sale si `t11_ICount <= 0` o si hay
  una interrupción lista (`t11_irq_ready`), como el intérprete. Memoria por funciones no inline para que el código
  sea pequeño. Despacho: `t11_rc_table[(PC-0x8000)/2]` en `t11_execute`; sin bloque → intérprete.
- **Validación:** `-DNAOMI_RAMCRC` muestra un hash de la RAM del T11 en los frames 600/1200/1800. Intérprete y
  recompilado dan **idéntico** (97D93465 91E04DF1 96534805) → misma emulación instrucción a instrucción.
- **Resultado Flycast (carrera de la demo):** T11 8,3–9 ms → 5,3–6,2 ms; mostrados 27–39 → 50–56. ~1.800 bloques,
  ~6.850 instrucciones; ~350 instrucciones/frame siguen interpretadas (3 %).
- **Ojo con el tamaño del código** (caché de instrucciones de 8 KB): la primera versión ocupaba 1,1 MB y en Flycast
  iba *más lenta*. Quitar `-falign-labels/-falign-jumps` (rellenaban cada salto) y sacar la memoria a funciones lo
  dejó en ~0,9 MB. Ideas si en placa no basta: `-Os` para t11.o (~660 KB), encadenar bloques, guardar registros en
  variables locales dentro del bloque.
- **Legal:** el código generado (`src/t11/t11_ssprint_rc.h`) es una traducción de la ROM: lo genera `naomi/build.sh`
  desde `naomi/roms/ssprint.zip` y está en `.gitignore`. Solo se publican el generador y la lista de entradas.
- **Pendiente: medir en placa.**
- **Lectura en directo por la NetDIMM (2026-11-12):** `src/naomi/naomi_dimmcomms.cpp` contesta los peek/poke de la
  NetDIMM (copiado del port nativo de Daytona, rama naomi-native, que sigue a libnaomi y al residente de dc2naomi) y
  `naomi_live` (naomi.h) guarda cada segundo: emulados, mostrados, % ocupado, bucles de Super Sprint y los tiempos del
  profiler. Se lee **en la Raspberry** con `live.py NETDIMM_IP 0xADDR [SEGUNDOS]` (el lector común de
  [naomi-pi-tools](https://github.com/0skarzer0/naomi-pi-tools/blob/main/pi/live.py); hasta el 2026-11-13 estaba aquí en `naomi/tools/live.py`)
  (`ADDR` = `sh-elf-nm mame4all_GAME.elf | grep naomi_live`; cambia con cada build). Funciona sin el contador en
  pantalla. Acceso a la Pi: ver HANDOFF de dc2naomi (`tools/board/pi.py`, `piput.py`; contraseña solo en el chat).
- **Medido en la placa por red (2026-11-12, demo de Super Sprint, sonido 11 kHz, décimas→ms por frame emulado):**

  | Variante | emu/mostrados | T11 | 6502 | sonido | vídeo MAME | subida (por mostrado) |
  |---|---|---|---|---|---|---|
  | T11 recompilado -O3 (con frameskip) | 52–61 / 12–15 | 8,7 | 3,1 | 2,2 | 1,5 | 6,5 |
  | T11 recompilado -Os | 46–54 / 14–15 | **10,9** (peor) | 3,3 | 2,1 | 1,7 | 6,4 |
  | Intérprete (con frameskip) | 46–65 / 12–14 | 9,2–9,8 | 1,4–5 | 1,5–3 | 1,2–4 | 6,4 |
  | Recompilado **sin frameskip** (`-DNAOMI_NOSKIP`) | **36–37 / 36–37** | 8,8 | 3,0 | 2,0 | 6,0 | 6,5 |

  En placa la recompilación gana poco (~8 %): la caché de instrucciones de 8 KB se come la ventaja (en Flycast
  era ~35 %). La imagen en 8 bits sí ayuda: subida 8,3 → 6,5 ms. **El usuario no quiere frameskip ni filtros**:
  dibujando todos los frames cuesta ~26 ms por frame (37 fps). Para 60 sin frameskip hay que quitar ~10 ms:
  la vía es que el **PVR dibuje la imagen** (fondo de tiles, sprites y textos como polígonos con texturas en VRAM),
  que elimina el dibujo de MAME (~6 ms) y la subida (~6,5 ms).

### Hipótesis (sin comprobar)

1. **Causa común del port, no de cada juego:** dos drivers muy distintos se quedan en ~46–47 fps emulados.
   Flycast no reproduce el coste real de caché y memoria del SH-4. Candidatos: subida de textura por
   store queues, conversión de paleta, mezcla de audio, memoria sin caché o copias de bitmap.
   Primer paso: medir en placa con el profiler (`PROFILER_MAME4ALL`) qué parte se come el frame.
2. **Sonido malo = consecuencia de (1):** el audio espera 60 fps emulados; a ~46 se vacía el buffer.
3. **Cuelgues posiblemente ligados al overlay de FPS:** en ambas pruebas estaba activo. Sospecha:
   escrituras de byte en VRAM al dibujarlo (la VRAM real no las admite; ver arreglo de unzip/ROMs).
   Prueba: partida larga sin activar el overlay; revisar `naomi_video.cpp` (dibujo del overlay).

## Hardware objetivo

NAOMI 2 multibios, NetDIMM 512 MB, netboot con WiPi Netbooter (Raspberry Pi 3), monitor 31 kHz,
mueble Crazy Taxi: volante, acelerador, freno, palanca de 2 posiciones con retorno, START, monedero.

## Arquitectura

| Parte | Fichero | Detalle |
|---|---|---|
| Build | `Makefile.naomi`, `naomi/build.sh GAME [FPS]` | `GAME=hangon|konamigt`; solo se compila el driver del juego (`NO_DRIVER_*`). `build.sh` limpia todo si cambian `GAME`/`EXTRA`/`MAKEOPTS` (`.naomi_last`) y mete solo `naomi/roms/$GAME.zip` en el romdisk |
| Flags | | `-std=gnu++11 -fpermissive -O3`, `-DNAOMI -DNO_FASTMEM`, `FAMEC=1` por defecto |
| Drivers | `src/naomi/naomi_drivers.cpp` | `drivers[] = {&NAOMI_DRV(NAOMI_GAME), 0}` |
| Vídeo | `src/naomi/naomi_video.cpp` | bitmap 8 bits → paleta RGB565 → store queues → textura PVR 512x256 sin twiddle, doble buffer, 1 quad 640x480. Overlay FPS: "emulados/mostrados busy%" + línea de depuración |
| Audio | `src/naomi/naomi_audio.cpp` | `snd_stream` KOS, mono, 23040 Hz (slice×fps), anillo 16384, descarta lo más viejo por encima de 4096 |
| Entrada | `src/naomi/naomi_input.cpp` | driver MIE de KOS (`maple_enum_type(0, MAPLE_FUNC_MIE)`) |
| Frameskip | `src/sdl/video.cpp` (bloque `NAOMI`) | salta solo si va >1 frame tarde, máx 4 seguidos |
| Main | `src/naomi/naomi_main.cpp` | `frameskip=-1`, sonido activado |

### Entrada (`naomi_input.cpp`)

- Calibración: volante 8448 / 33792 / 60160; acelerador 11776–47104; freno 14080–47360.
- **Monedas por diferencia de contadores** `coin[i].count` (el `coin_pulse` del driver ya llega borrado).
  Pulso de 20 frames (activo >10). SERVICE = crédito.
- **Freno (>128) + START = crédito**; ese START se oculta al juego hasta soltarlo (`start_coin`).
- Palanca adelante/atrás enclava `gear_high` → `naomi_gear()`. Pedales → `naomi_pedal(0/1)`.
- `naomi_analog()` (llamado desde `src/inptport.cpp`): AD_STICK_X/PADDLE/DIAL = volante;
  AD_STICK_Y jugador 1 = acelerador, jugador 2 = freno.
- Si el puerto no está en modo JVS, re-sondea el MIE.
- `-DNAOMI_FLYCAST`: modo de prueba que lee el MIE directamente (`mie_cmd_resp(0x86)`), tecla C = moneda.

## Cambios en el núcleo

- `src/unzip.cpp`, `src/mame.cpp`: **nada de VRAM** en NAOMI (la VRAM real no admite escrituras de byte;
  provocaba "Wrong checksum of ROM" y pantalla negra en placa real; Flycast no lo detecta).
- `src/sdl/minimal.h`: `DC_RAM_TOP 0x8e000000`, `gp2x_malloc = calloc`.
- `src/fame/m68000.cpp`: la 2ª CPU 68000 siempre en Musashi; `NAOMI_ALL_MUSASHI` opcional.
  El núcleo ASM de FAME falla con Hang-On → se usa FAMEC (C).
- `src/tilemap.cpp`: quitado `volatile` de `tile_index`. `src/sndhrdw/ym2151.cpp`: casts de `abs`.

### Hang-On (`src/drivers/system16.cpp`)

- `HANGON_DIGITAL_CONTROLS` desactivado en NAOMI (`-DNAOMI_HO_DIGITAL` lo reactiva).
- ADC: canal = bits 2-3 del puerto I/O en `0xe03001`; nuevo `ho_io_adc_r` mapeado en `0xe03020-0xe0302f`
  (canal 0 = puerto 0, 1 = puerto 1, 2 = puerto 5).
- `NAOMI_HO_SLICES` = 8 (la IRQ de la CPU secundaria espera a la principal en `0xc7e000`).
- Speed hacks: principal en PC `0x17e6` leyendo `sys16_extraram[0x400]`; secundaria con `sys16_extraram3[0x1000]`
  (`NAOMI_HO_NOMAINSKIP` / `NAOMI_HO_NOSUBSKIP` los desactivan).

### Konami GT (`src/drivers/nemesis.cpp`)

`konamigt_input_r`: acelerador `(pedal0>>4)<<12`, freno `(pedal1>>6)<<8`, marcha `0x0400` = HIGH/TURBO,
`0x0800` = LOW.

## Pruebas con Flycast (`naomi/flycast/`)

- `flycast-mie-naomi.patch` sobre Flycast `7adbf3c`: con `FC_MIE_NODEVINFO=1` el MIE no responde a
  `MDC_DeviceRequest`, `GET_ID` va en una respuesta de 12 palabras y `SB_G1SYSM = 0xa1` (NAOMI).
  Sin esto, el driver MIE de KOS no se activa en Flycast.
- `start.sh ROM` (Xvfb + Flycast headless), `key.sh`, `shot.sh`, `stop.sh`.
- `perf.sh LABEL "EXTRA" "MAKEOPTS"`: captura el overlay de FPS en la demo.
- `race.sh` / `race2.sh`: build con `-DNAOMI_FLYCAST`, moneda (C), START (Return), acelerador (K), dirección (J/L).
- Audio a fichero: `AUDIODRV=disk` → `audio.raw`.
- El timing de Flycast se parece al real (Daytona recomp: 27 fps en Flycast vs 27–32 en placa).

## Rendimiento medido (Hang-On, Flycast, carrera)

Vídeo ~7 ms, CPUs ~6–7 ms, sonido ~1–1,5 ms por frame → 60 emulados, 41–55 mostrados. La emulación no baja
de 60, así que el audio va bien.

## Siguientes pasos

1. Diagnosticar la lentitud en placa (profiler) y los cuelgues (overlay de FPS / escrituras de byte en VRAM).
2. Añadir Super Hang-On y Enduro Racer (mismo hardware System 16, poco trabajo).
3. Spy Hunter / Max RPM (MCR3), Turbo, Sprint 2, Super Sprint, Arkanoid (volante → velocidad de spinner).
4. Ficha por juego en `docs/games/`.

## Trabajo pendiente tras Super Sprint (2026-11-13)

**Hecho (compilación; los cuatro de abajo también probados en Flycast):** spyhunt, turbo, paperboy, csprint. Antes:  `Makefile.naomi` admite `GAME=csprint|shangon|shangonb|enduror|
endurobl|endurob2`. Comprobado compilando con zips vacíos. Falta: ROMs (`romzip.py` los reempaqueta), mapear
controles (Super Hang-On y Enduro Racer no tienen el `ho_io_adc_r` de Hang-On; Super Hang-On está marcado
`GAME_NOT_WORKING` en MAME 0.35), y para `csprint` generar la lista de entradas del T11 (`naomi/tools/
ssprint_t11_entries.txt` es de Super Sprint) y los atajos de espera (`NAOMI_SSPRINT1` es específico de la ROM).
Spy Hunter (`mcr3`) y Turbo (`turbo`) no tienen bloque en el Makefile aún.

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

## Super Sprint con el PVR dibujando (2026-11-12)

- `src/naomi/naomi_pvr_atarisy2.cpp`: el PVR dibuja la imagen (playfield como textura 8bpp de 1024x512 en
  tiles, objetos y caracteres como quads de atlas 4bpp, paletas ARGB1555). Sin subir framebuffer: vídeo ~1 ms.
  Vertex buffer de 512 KB (con 64 KB se desbordaba: 5 fps en placa). Antes de tocar textura/paleta se espera
  a que el PVR acabe el frame anterior (`naomi_pvr_atarisys2_sync`): quita el efecto "persiana" en los fundidos.
- **Sin frameskip en todos los juegos** (`-DNAOMI_NOSKIP` global) y sin filtro, imagen 8 bits 1:1.
- Super Sprint usa sonido a 11 kHz por defecto (`-DNAOMI_RATE=11025` en el Makefile). Sin él el sonido
  cuesta ~4 ms/frame y el usuario notó el juego lento.
- Placa: 60 fps mostrados = emulados, busy ~75 %; T11 ~7 ms, 6502 ~1,5, sonido 1,4, vídeo 1-4.
- Ojo: el contador "loops" (bucle principal del T11) NO mide la velocidad: el propio juego va a 30 o 60
  según la pantalla.
- Entrada (todos los juegos): START pulsado = moneda, soltado = START (tras entrar la moneda); START 3 s
  = mostrar/ocultar fps sin mandar START. Volante en IPT_DIAL tratado como digital: pasada una zona muerta
  pequeña (10) solo cuenta la dirección; la velocidad sube de 12 a 64 (/16 por frame, 64 = la de teclado de
  MAME) en 40 frames mientras se mantiene el giro.
- Telemetría: `pi/live.py` de [naomi-pi-tools](https://github.com/0skarzer0/naomi-pi-tools/blob/main/pi/live.py) en la Pi (`python3 live.py 10.0.0.4 0xADDR SEG`; reconoce el bloque `MAME`).

### Aviso (2026-11-13): Super Sprint lento = interleave 5

- Todas las imágenes de prueba posteriores a `test_il5` salieron con **interleave 5** sin quererlo:
  `build.sh` hacía `make clean` sin `GAME=`, así que no borraba `src/drivers/atarisy2.o` y se quedó el de
  la prueba `-DNAOMI_SS_INTERLEAVE=5`. Con interleave 5 el bucle del juego va a 30 por segundo en carrera
  (60 fps mostrados, pero el juego a media velocidad). **Interleave 10 es obligatorio**; arreglado el clean.
- En carrera, "loops" ≈ 58 es la velocidad correcta; 30 es lento. (En otras pantallas el juego va a 30 él solo.)
- La espera `naomi_pvr_atarisys2_sync` (persiana) está comentada, pendiente de volver a probar con interleave 10.

### Super Sprint: versión definitiva del usuario (2026-11-13)

Instalada en la Pi como `h-ssprint_mame4all_naomi.bin.gz` (la versión sin sonido queda descartada).
- **Imagen:** pantalla completa 640x480. `NAOMI_SS_FULL 3` (por defecto): el juego se dibuja 1:1 sin filtro
  en una textura 512x384 (`pvr_scene_begin_rtt`) y esa textura se muestra en toda la pantalla con filtro
  bilineal. 60 fps en placa. No esperar al render de la textura antes de la escena de pantalla (esa espera
  dejaba la CPU parada: 35-41 fps); solo `pvr_wait_render_done` antes de cerrar la última lista del RTT.
  - `NAOMI_SS_FULL 1` (filtrar cada pieza): rompe los trazos finos del texto punch-through y se ven juntas. No usar.
  - `NAOMI_SS_FULL 2` (sharp bilinear, x2 en 4 renders): 25-30 fps en placa. No usar.
  - `NAOMI_SS_FULL 0`: ventana 1:1 sin filtro.
- **Scanlines:** `NAOMI_SCANLINES` 0-15 (naomi_video.cpp), por defecto 0 (el usuario las probó a 6 y al final las quitó).
- **Sonido:** 11 kHz. A 22 kHz cae a 53-54 fps cuando suena la música (sonido + 6502 ≈ 10 ms/frame).
- **Volante (IPT_DIAL):** analógico, curva cuadrática, `NAOMI_DIAL_MAX` 24 (/16 por frame a tope), zona muerta 6.
  `-DNAOMI_DIAL_DIGITAL`: como teclas, `NAOMI_DIAL_KEY` 8 (= MAME sensibilidad 10, incremento 5).
- **START:** pulsar = moneda, soltar = START; 3 s = mostrar/ocultar fps.
- Interleave 10 obligatorio (ver aviso anterior). La espera `naomi_pvr_atarisys2_sync` sigue comentada.
