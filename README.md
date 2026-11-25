# MAME4ALL para Sega NAOMI

Port de **MAME4ALL** (MAME 0.35) a la placa recreativa **Sega NAOMI / NAOMI 2**, pensado para cargarse por
**netboot (NetDIMM)** en un mueble **Crazy Taxi** con volante, pedales y palanca de cambios.
Cada juego se compila como un `.bin` independiente que arranca directamente en ese juego.

> **No se incluyen ROMs.** Cada usuario compila con sus propios volcados. No distribuyas binarios
> que lleven ROMs dentro.

## Juegos

| Juego | Driver | Estado | Rendimiento (Flycast) | Notas |
|---|---|---|---|---|
| Hang-On | `hangon` | ⚠️ Placa real: 47 fps emulados / 16 mostrados (lento); controles OK. Los cuelgues eran del contador de FPS: arreglados | 60 emulados / 41–55 mostrados en carrera | Manillar y acelerador analógicos |
| Super Sprint | `ssprint` | ✅ Placa: 60 fps sin frameskip a velocidad correcta. T11 **recompilado estáticamente**, imagen dibujada por el PVR a pantalla completa (512x384 a textura + filtro bilineal), sonido a 11 kHz | T11 3,5 ms, 6502 0,9 ms (con atajos de espera) | Volante del mueble → volante giratorio (velocidad según ángulo, curva suave); acelerador = pedal; START pulsado = moneda, soltado = START. ROM: **revisión 1** (`NAOMI_SSPRINT1`) |
| Konami GT | `konamigt` | ⚠️ Placa real: 46/13, sonido malo (lento); controles OK. Los cuelgues eran del contador de FPS: arreglados | 60 fps | Volante, acelerador, freno y marcha LOW/HIGH (palanca) |

Nuevos (2026-11-13), compilados con las ROMs del propietario y comprobados **solo en Flycast** (arrancan, se ven bien
y responden a moneda, START, acelerador y volante; **sin medir en la placa**): Spy Hunter (`spyhunt`), Turbo (`turbo`),
Paperboy (`paperboy`) y Championship Sprint (`csprint`). Los tres últimos con alguna ROM de otra revisión que
`naomi/tools/romzip.py` sustituye por tamaño y número de chip (lo avisa). Championship Sprint y Paperboy van sin el T11
recompilado (intérprete), así que irán más lentos que Super Sprint. Spy Hunter: acelerador = pedal, volante analógico,
freno = ametralladoras, palanca = marcha; el resto de armas necesita pulsadores extra (JVS 3–6).

**No viables con MAME 0.35:** Super Hang-On, Enduro Racer y Turbo OutRun — los sets modernos llevan el programa del 68000
**cifrado** (claves 317-xxxx / FD1094) y esta versión de MAME no lo descifra.

Próximos candidatos (MAME 0.35): Max RPM, Sprint 2, Arkanoid.

## Controles (mueble Crazy Taxi)

| Control | Función |
|---|---|
| Volante | Dirección analógica (stick X, paddle o dial) |
| Acelerador / freno | Pedales analógicos |
| Palanca adelante / atrás | Marcha HIGH / LOW (se queda enclavada aunque la palanca vuelva al centro) |
| START | Empezar partida |
| **Freno + START** | **Meter un crédito** (en todos los juegos) |
| Monedero / SERVICE | Crédito |
| TEST | Modo test del juego |
| START mantenido 3 s | Mostrar/ocultar contador de FPS |

En un mueble de sticks: izquierda/derecha = dirección, botón 1 = acelerar, botón 2 = frenar.

## Compilar

Requiere [KallistiOS](https://github.com/KallistiOS/KallistiOS) configurado con `KOS_SUBARCH=naomi`
y `naomibintool`.

```sh
source /opt/toolchains/dc/kos/environ.sh
# 1. Pon tu romset en naomi/roms/<juego>.zip con los nombres de MAME 0.35.
#    Si tu romset es moderno, reempaquétalo por CRC32:
python3 naomi/tools/romzip.py src/drivers/system16.cpp hangon hangon_moderno.zip naomi/roms/hangon.zip
# 2. Compila
./naomi/build.sh hangon        # -> mame4all_hangon_naomi.bin
./naomi/build.sh konamigt      # -> mame4all_konamigt_naomi.bin
./naomi/build.sh ssprint       # -> mame4all_ssprint_naomi.bin
```

Para WiPi Netbooter: `gzip` el `.bin` y añade una línea en `romsinfo.csv`.

## Código original (MAME4ALL de Chui)

Este port parte del código de **MAME4ALL beta 2** para Dreamcast, publicado por **Chui** en su web
(*Chui's DC projects*, `chui.dcemu.co.uk`). El árbol `src/` de este repositorio es casi idéntico al de
ese paquete: la capa SDL (`src/sdl/`) conserva su aviso original («SDL wrapper by Chui») y el resto del
código lleva las marcas de **Franxis**, autor de MAME4ALL para GP2X.

Se conserva una copia del paquete original, **descomprimido y sin cambios**, para poder ver qué se ha
modificado para NAOMI:

- Código original: [`MAME4ALL/src`](https://github.com/0skarzer0/Chui-s-Dreamcast-Projects./tree/main/MAME4ALL/src)
  en el repositorio [Chui-s-Dreamcast-Projects.](https://github.com/0skarzer0/Chui-s-Dreamcast-Projects.)
  (copia de preservación de su web, con los fuentes de todos sus emuladores).
- Paquete original: `MAME4ALL/archivos/mame4all-beta2-src.tar.bz2` en ese mismo repositorio.
- Para ver los cambios de este port respecto al original:
  `diff -r <copia-de-MAME4ALL/src>/src src`.

## Pruebas con Flycast

`naomi/flycast/` contiene un parche para Flycast (MIE en modo NAOMI) y scripts para probar sin la placa.
Ver [HANDOFF.md](HANDOFF.md).

## Licencia

Basado en MAME4ALL / MAME 0.35, con **licencia no comercial de MAME**. Este port mantiene esa licencia.
Las marcas y juegos pertenecen a sus propietarios (Sega, Konami…).

## Agradecimientos

- **MAME** (Nicola Salmoria y el MAME Team) y **MAME4ALL** (Franxis), base de este port.
- **Chui** ([*Chui's DC projects*](https://github.com/0skarzer0/Chui-s-Dreamcast-Projects.)): adaptación de
  MAME4ALL a SDL y a Dreamcast, de la que parte directamente este port (MAME4ALL beta 2), y autor de la
  web donde se publicó. Ver «Código original».
- **KallistiOS** (KOS) y su soporte NAOMI, y `naomibintool`.
- **Flycast**, para probar sin la placa.
- **DragonMinded**: `libnaomi` (`dimmcomms.c`, protocolo de peek/poke de la NetDIMM) y `netdimm.py` (WiPi / PiForce).
- **alphanu1**: Daytona USA recomp y su port nativo SH-4; de su adaptación a NAOMI (0skarzer0/daytona-arcade-recomp, rama naomi-native) salen `naomi_dimmcomms.cpp` y la idea de `live.py`.
- **WiPi Netbooter** / PiForce Tools, para la carga por red.
- El conocimiento del T-11 y del hardware Atari System 2 viene de los drivers de MAME.
