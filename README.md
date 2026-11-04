# MAME4ALL para Sega NAOMI

Port de **MAME4ALL** (MAME 0.35) a la placa recreativa **Sega NAOMI / NAOMI 2**, pensado para cargarse por
**netboot (NetDIMM)** en un mueble **Crazy Taxi** con volante, pedales y palanca de cambios.
Cada juego se compila como un `.bin` independiente que arranca directamente en ese juego.

> **No se incluyen ROMs.** Cada usuario compila con sus propios volcados. No distribuyas binarios
> que lleven ROMs dentro.

## Juegos

| Juego | Driver | Estado | Rendimiento (Flycast) | Notas |
|---|---|---|---|---|
| Hang-On | `hangon` | 🧪 Flycast OK, pendiente en placa real | 60 emulados / 41–55 mostrados en carrera | Manillar y acelerador analógicos |
| Konami GT | `konamigt` | 🧪 Flycast OK, pendiente en placa real | 60 fps | Volante, acelerador, freno y marcha LOW/HIGH (palanca) |

Próximos candidatos (MAME 0.35): Super Hang-On (`shangonb`), Enduro Racer (`endurobl`), Spy Hunter,
Max RPM, Turbo, Sprint 2, Super Sprint, Arkanoid.

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
```

Para WiPi Netbooter: `gzip` el `.bin` y añade una línea en `romsinfo.csv`.

## Pruebas con Flycast

`naomi/flycast/` contiene un parche para Flycast (MIE en modo NAOMI) y scripts para probar sin la placa.
Ver [HANDOFF.md](HANDOFF.md).

## Licencia

Basado en MAME4ALL / MAME 0.35, con **licencia no comercial de MAME**. Este port mantiene esa licencia.
Las marcas y juegos pertenecen a sus propietarios (Sega, Konami…).
