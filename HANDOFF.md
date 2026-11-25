# HANDOFF — estado del port MAME4ALL → NAOMI

Documento de traspaso entre sesiones (personas o IAs): **lo vigente**. El diario de cada sesión está en
[`docs/historial/`](#historial-dónde-está-cada-cosa) y cómo está hecho el port, en [docs/arquitectura.md](docs/arquitectura.md).
**Actualízalo en cada sesión:** el estado y las recetas, aquí; el relato y las medidas, en el historial.

**Código original:** el punto de partida es MAME4ALL beta 2 de Chui. Su copia sin cambios, con la
historia de versiones, está en
[`MAME4ALL/src`](https://github.com/0skarzer0/Chui-s-Dreamcast-Projects./tree/main/MAME4ALL/src) del repo
`0skarzer0/Chui-s-Dreamcast-Projects.` (paquete `MAME4ALL/archivos/mame4all-beta2-src.tar.bz2`). Sirve para
comparar con `diff -r` lo que se ha cambiado para NAOMI. Créditos en el README.

## Reglas

- **No se suben ROMs ni nada generado a partir de ellas**: los romsets van en `naomi/roms/` (ignorado) y los juegos del
  usuario están en `C:\Users\Usuario\Documents\dc2naomi`. El código del T11 recompilado (`src/t11/t11_ssprint_rc.h`)
  es una traducción de la ROM: se genera al compilar y está en `.gitignore`; solo se publican el generador y la lista
  de entradas.
- La contraseña de la Raspberry va en la variable `PIPASS`, nunca en un fichero.
- **Medir en la placa, no en Flycast:** los núcleos de CPU van 2-3,5× más lentos en el SH-4 real (FAMEC 3,3×, RAZE Z80
  2,3×, Musashi 1,8×) y la caché de instrucciones de 8 KB se come las ventajas que Flycast enseña. El dibujo sí cuesta
  lo mismo.
- **La VRAM real no admite escrituras de byte** (Flycast sí): nada de bytes a VRAM (el contador de FPS se compone en
  RAM; ROMs y unzip, fuera de VRAM).
- El usuario **no quiere frameskip ni filtros** (salvo el bilineal de la pantalla completa de Super Sprint).

## Recetas

**1. Compilar un juego:** `source /opt/toolchains/dc/kos/environ.sh` (KallistiOS con `KOS_SUBARCH=naomi`), romset en
`naomi/roms/<juego>.zip` con los nombres de MAME 0.35 y `./naomi/build.sh GAME` → `mame4all_GAME_naomi.bin`. Juegos de
`Makefile.naomi`: `hangon`, `konamigt`, `ssprint`, `spyhunt`, `turbo`, `paperboy`, `csprint` (y `shangon`, `enduror`…,
que solo compilan). `build.sh` limpia también los objetos del propio juego si cambian `GAME`/`EXTRA`/`MAKEOPTS`
(`.naomi_last`): antes no lo hacía y se colaron opciones de una prueba (Super Sprint a interleave 5, la mitad de rápido).
El contador de FPS no sale al arrancar; se activa con START 3 s.

**2. Un romset moderno:** `python3 naomi/tools/romzip.py src/drivers/DRIVER.cpp JUEGO moderno.zip naomi/roms/JUEGO.zip`
reempaqueta por CRC32; si una ROM es de otra revisión la sustituye por tamaño y número de chip, y lo avisa.
Los sets con el 68000 cifrado (FD1089/FD1094: Super Hang-On, Enduro Racer, Turbo OutRun, Action Fighter) no sirven con
MAME 0.35.

**3. Probar en Flycast** (`naomi/flycast/`, con `flycast-mie-naomi.patch` sobre Flycast `7adbf3c` y
`FC_MIE_NODEVINFO=1`): `start.sh ROM`, `key.sh`, `shot.sh`, `stop.sh`; `perf.sh`/`ssperf.sh` para medir (sin entrada);
`race.sh`/`race2.sh` con `-DNAOMI_FLYCAST` para probar controles (esos builds leen el MIE directamente y cuestan casi
2 frames por llamada: **no sirven para medir velocidad**). Audio a fichero: `AUDIODRV=disk`.

**4. Medir en la placa:** perfilador con `-DPROFILER_MAME4ALL` (2.ª línea del contador: décimas de ms por frame de cada
CPU, sonido, vídeo y subida). Sin pantalla, por la red: `naomi_live` guarda las cifras cada segundo y en la Raspberry
`python3 live.py 10.0.0.4 0xADDR SEGUNDOS`, con `live.py` de
[naomi-pi-tools](https://github.com/0skarzer0/naomi-pi-tools/blob/main/pi/live.py) (reconoce el bloque `MAME`;
`ADDR` = `sh-elf-nm mame4all_GAME.elf | grep naomi_live`, cambia con cada build). Raspberry del mueble Crazy Taxi:
**192.168.68.251** (el historial dice .60); NetDIMM 10.0.0.4. Subir y cargar: recetas de
[dc2naomi](https://github.com/0skarzer0/dc2naomi/blob/main/HANDOFF.md) (`tools/board/`). Otras ayudas:
`-DNAOMI_PCHIST` (dónde terminan los slices de CPU), `-DNAOMI_T11STAT`, `-DNAOMI_RAMCRC` (hash de la RAM del T11 para
comparar intérprete y recompilado).

**5. Recompilar el T11 de otro juego (Atari System 2):** `naomi/tools/t11recomp.py` con su lista de entradas
(`naomi/tools/ssprint_t11_entries.txt` es la de Super Sprint; sacar la de otro juego con `-DNAOMI_T11RC_MISSES`).
Los atajos de espera de `NAOMI_SSPRINT1` son de la ROM de Super Sprint revisión 1 y no valen para otro juego.

**6. Mandos (todos los juegos, `src/naomi/naomi_input.cpp`):** START pulsado = moneda, soltado = START; freno + START
= crédito; START 3 s = mostrar/ocultar FPS; palanca = marcha enclavada (`naomi_gear()`); volante en `IPT_DIAL`
analógico con curva cuadrática (`NAOMI_DIAL_MAX` 24, zona muerta 6; `-DNAOMI_DIAL_DIGITAL` = como teclas). Calibración
del mueble: [crazytaxi_cabinet_info](https://github.com/0skarzer0/crazytaxi_cabinet_info).

## Estado de los juegos (2026-11-25)

| Juego | Estado | Notas |
|---|---|---|
| Super Sprint `ssprint` | ✅ versión definitiva en la Pi (`h-ssprint_mame4all_naomi.bin.gz`): 60 fps sin frameskip | T11 recompilado, PVR dibujando a 512x384 y pantalla completa con bilineal (`NAOMI_SS_FULL 3`), sonido 11 kHz, **interleave 10 obligatorio**, ROM revisión 1 |
| Hang-On `hangon` | ⚠️ placa: ~42-47 fps emulados, lento; controles OK | Le faltan ~8-10 ms por frame: hacen falta los dos 68000 y el Z80 más rápidos |
| Konami GT `konamigt` | ⚠️ placa: ~46-54 fps emulados, sonido malo; controles OK | Le faltan ~5 ms: vídeo al PVR (~3) y 68K más rápido (~3) lo dejarían justo |
| Spy Hunter, Turbo, Paperboy, Championship Sprint | compilados y probados solo en Flycast (2026-11-13), **sin medir en placa** | Paperboy y Championship Sprint, con el T11 interpretado |
| Super Hang-On, Enduro Racer | solo compilan | 68000 cifrado; Super Hang-On, `GAME_NOT_WORKING` en MAME 0.35 |

Los cuelgues de Hang-On y Konami GT de la primera prueba eran del contador de FPS escribiendo en VRAM: arreglado.

## Pendiente

1. Medir en placa Spy Hunter, Turbo, Paperboy y Championship Sprint; la lista de entradas del T11 de Championship Sprint.
2. Hang-On y Konami GT: lo más barato a probar primero es **arreglar FAME en ensamblador** (`FAMEC=0`, ~20 % más rápido
   en Flycast, hoy roto: no lee el acelerador y en Konami GT la imagen sale corrupta); luego el vídeo con el PVR (el de
   Nemesis es más difícil: caracteres en RAM, scroll por línea, sprites con zoom).
3. Volver a probar la espera `naomi_pvr_atarisys2_sync` (contra la "persiana" en los fundidos de Super Sprint), hoy comentada.
4. Juegos pedidos que no están en este árbol recortado (277 drivers): varios se pueden traer de libretro
   mame2000/mame2003-plus; los de Sega X/Y/System 32 no tienen camino realista
   ([historial/juegos-nuevos.md](docs/historial/juegos-nuevos.md)). Candidatos fáciles: Max RPM, Sprint 2, Arkanoid.

## Historial: dónde está cada cosa

El antiguo HANDOFF (339 líneas) se repartió el 2026-11-25 sin quitar nada:

| Sección antigua | Archivo |
|---|---|
| Hardware objetivo, Arquitectura, Entrada, Cambios en el núcleo (Hang-On, Konami GT), Pruebas con Flycast | [docs/arquitectura.md](docs/arquitectura.md) |
| Estado (2026-11-04), sesiones del 11-04 y 11-05, Hipótesis, Rendimiento medido, Siguientes pasos, presupuesto de Hang-On y Konami GT (2026-11-13) | [docs/historial/hangon-konamigt.md](docs/historial/hangon-konamigt.md) |
| Super Sprint (2026-11-05), Recompilación estática del T11, Super Sprint con el PVR, Aviso interleave 5, versión definitiva | [docs/historial/super-sprint.md](docs/historial/super-sprint.md) |
| Trabajo pendiente tras Super Sprint (juegos nuevos), Juegos pedidos que faltan | [docs/historial/juegos-nuevos.md](docs/historial/juegos-nuevos.md) |
