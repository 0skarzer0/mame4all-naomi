# MAME4ALL para Sega NAOMI

Port de **MAME4ALL** (MAME 0.35) a la placa recreativa **Sega NAOMI / NAOMI 2**, pensado para cargarse por
**netboot (NetDIMM)** en un mueble **Crazy Taxi** con volante, pedales y palanca de cambios.
Cada juego se compila como un `.bin` independiente que arranca directamente en ese juego.

> **No se incluyen ROMs.** Cada usuario compila con sus propios volcados. No distribuyas binarios
> que lleven ROMs dentro.

## Juegos

| Juego | Driver | Estado | Rendimiento (Flycast) | Notas |
|---|---|---|---|---|
| Hang-On | `hangon` | ⚠️ Placa real: 47 fps emulados / 16 mostrados, cuelgues; controles OK | 60 emulados / 41–55 mostrados en carrera | Manillar y acelerador analógicos |
| Super Sprint | `ssprint` | ⚠️ Placa: velocidad de juego correcta, 44–54 fps; nueva versión con el T11 **recompilado estáticamente**, pendiente en placa | T11 3,5 ms, 6502 0,9 ms (con atajos de espera) | Volante del mueble → volante giratorio (velocidad según ángulo); acelerador = pedal. ROM: **revisión 1** (`NAOMI_SSPRINT1`) |
| Konami GT | `konamigt` | ⚠️ Placa real: 46/13, sonido malo, cuelgues; controles OK | 60 fps | Volante, acelerador, freno y marcha LOW/HIGH (palanca) |

Próximos candidatos (MAME 0.35): Super Hang-On (`shangonb`), Enduro Racer (`endurobl`), Spy Hunter,
Max RPM, Turbo, Sprint 2, Championship Sprint, Arkanoid.

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

## Pruebas con Flycast

`naomi/flycast/` contiene un parche para Flycast (MIE en modo NAOMI) y scripts para probar sin la placa.
Ver [HANDOFF.md](HANDOFF.md).

## Licencia

Basado en MAME4ALL / MAME 0.35, con **licencia no comercial de MAME**. Este port mantiene esa licencia.
Las marcas y juegos pertenecen a sus propietarios (Sega, Konami…).

## Agradecimientos

- **MAME** (Nicola Salmoria y el MAME Team) y **MAME4ALL** (Franxis), base de este port.
- **KallistiOS** (KOS) y su soporte NAOMI, y `naomibintool`.
- **Flycast**, para probar sin la placa.
- **DragonMinded**: `libnaomi` (`dimmcomms.c`, protocolo de peek/poke de la NetDIMM) y `netdimm.py` (WiPi / PiForce).
- **alphanu1**: Daytona USA recomp y su port nativo SH-4; de su adaptación a NAOMI (0skarzer0/daytona-arcade-recomp, rama naomi-native) salen `naomi_dimmcomms.cpp` y la idea de `live.py`.
- **WiPi Netbooter** / PiForce Tools, para la carga por red.
- El conocimiento del T-11 y del hardware Atari System 2 viene de los drivers de MAME.
