# Historial: juegos nuevos y juegos que faltan (2026-11-13)

> Historial: diario de sesiones copiado tal cual del antiguo `HANDOFF.md` (2026-11-25). Puede estar superado por lo escrito después; lo vigente está en [HANDOFF.md](../../HANDOFF.md).

## Trabajo pendiente tras Super Sprint (2026-11-13)

**Hecho (compilación; los cuatro de abajo también probados en Flycast):** spyhunt, turbo, paperboy, csprint. Antes:  `Makefile.naomi` admite `GAME=csprint|shangon|shangonb|enduror|
endurobl|endurob2`. Comprobado compilando con zips vacíos. Falta: ROMs (`romzip.py` los reempaqueta), mapear
controles (Super Hang-On y Enduro Racer no tienen el `ho_io_adc_r` de Hang-On; Super Hang-On está marcado
`GAME_NOT_WORKING` en MAME 0.35), y para `csprint` generar la lista de entradas del T11 (`naomi/tools/
ssprint_t11_entries.txt` es de Super Sprint) y los atajos de espera (`NAOMI_SSPRINT1` es específico de la ROM).
Spy Hunter (`mcr3`) y Turbo (`turbo`) no tienen bloque en el Makefile aún.

## Juegos pedidos que faltan en esta base (2026-11-13)

Este árbol está **recortado** (277 drivers) y no trae: nstocker, buggychl, f1en, smgp, radr, radm, f1lap,
afightere/afighterb, polepos2, chasehq, chqflag, hotchase, bigrun, offroad, sci, driftout, wrally, gtmr, gtmr2.
Corrección: no es que sean «posteriores a MAME 0.35»; varios (polepos, wecleman/hotchase, taito_f2/driftout, kaneko16/gtmr)
aparecen en los árboles de libretro mame2000/mame2003-plus, así que sus drivers se pueden **traer de ahí** (misma
API de estilo C). Otros (Sega X/Y y System 32: smgp, f1en, radr, radm, f1lap) son de 2004 en adelante y llevan
V60 o FD1094: no hay camino realista con emulación en la SH-4. Super Hang-On, Enduro Racer, Turbo OutRun y
Action Fighter traen el 68000 cifrado (FD1089/FD1094): hay que usar sus sets sin proteger o portar el descifrado.
