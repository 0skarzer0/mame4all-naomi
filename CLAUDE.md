# mame4all-naomi — instrucciones para Claude

## Qué es
Port de **MAME4ALL** (MAME 0.35) a **NAOMI / NAOMI 2** por netboot, para el mueble Crazy Taxi: cada juego es un `.bin` que arranca directo en él (Super Sprint, Spy Hunter, Turbo, Paperboy…). Rama de trabajo `naomi`; lo propio de NAOMI está en `naomi/` y `Makefile.naomi`.

## Qué leer primero
1. [HANDOFF.md](HANDOFF.md) (339 líneas): primero "Estado" y la sección del juego en el que se trabaje; el resto, solo si hace falta.
2. [README.md](README.md): tabla de juegos y estado.

## Reglas que no se saltan
- **Los juegos del usuario (ROMs, discos, imágenes) y su `info.txt` están en `C:\Users\Usuario\Documents\dc2naomi` y no se suben nunca a GitHub** (ni se copian al repo, ni se citan sus datos en commits o documentos). Tampoco BIOS ni binarios que lleven ROMs dentro.
- **Las contraseñas van en variables de entorno** (la de las Raspberry, `PIPASS`), **nunca en un archivo** del repo ni en un commit.
- "Probado" = funciona en la placa real; Flycast es solo el primer paso.
- Raspberry (WiPi Netbooter): mueble Crazy Taxi **192.168.68.251** (los documentos antiguos dicen **.60**), mueble estándar **192.168.68.250** (antes .59).
- Al terminar: HANDOFF y docs al día, commit y push.

# Equipo de agentes (Claude + Gemini)

Este proyecto puede usar Gemini vía Antigravity CLI (`agy`) como ayudante para ahorrar tokens.
- Activa el modo equipo con `/equipo <tarea>`; las reglas completas están en `.claude/commands/equipo.md`.
- Encargos y respuestas: `.agy-tasks/` (plantilla en `_plantilla.md`). Script: `scripts/agy-task.ps1` (estado del equipo en `.agy-tasks/_estado.md`).
- `agy` debe usar la API key de Google AI Studio (`GEMINI_API_KEY`), nunca el login de la suscripción.
- Los ayudantes solo leen; los cambios en archivos los hace Claude tras revisar.
