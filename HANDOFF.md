# HANDOFF — estado del port MAME4ALL → NAOMI

Documento de traspaso entre sesiones (personas o IAs). **Actualízalo en cada sesión.**

## Estado (2026-11-04)

- Hang-On y Konami GT funcionan en Flycast (vídeo, audio, controles, monedas).
- **Pendiente:** prueba en placa real de las builds con el arreglo de VRAM y con freno + START = crédito.
- Freno + START no se ha probado aún en Flycast.

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

1. Probar en placa real Hang-On y Konami GT (incluido freno + START).
2. Añadir Super Hang-On y Enduro Racer (mismo hardware System 16, poco trabajo).
3. Spy Hunter / Max RPM (MCR3), Turbo, Sprint 2, Super Sprint, Arkanoid (volante → velocidad de spinner).
4. Ficha por juego en `docs/games/`.
