---
description: Muestra el panel del equipo Claude + Gemini (Antigravity) y reparte tareas
argument-hint: [opcional: "Hera, analiza ..." o una tarea sin asignar]
---

Activa el modo equipo. Petición del usuario: $ARGUMENTS

## 1. Muestra el panel
Lee `.agy-tasks/_estado.md` (si existe) y dibuja esta tabla con el estado actual de cada agente
(🟢 working · ⚪ idle · 🔴 quota exceeded / error · ✅ done). Un agente que no aparece en `_estado.md` está idle.

| Agente | Rol | Modelo |
|---|---|---|
| Claude | Director / orquestador | (este modelo) |
| Hera | Arquitectura y planificación | gemini-4-hydrogen-xhigh |
| Juno | Auditoría y revisión de código | gemini-4-hydrogen-high |
| Mira | Revisión técnica y segunda opinión | claude-sonnet-4-6 |
| Nova | Tareas rápidas | gemini-3.8-flash-medium |

Reservas si un modelo se queda sin cuota: `gemini-3.8-flash-high`, `gpt-oss-120b-medium`.
Los nombres de modelo se comprueban con `agy models`; si alguno no existe, usa el más parecido y avisa.
Si `$ARGUMENTS` está vacío, termina preguntando: **¿Qué tarea asigno y a quién?**

## 2. Delega
Si la petición nombra a un agente ("Hera, analiza el TilingEngine"), se lo encargas a él. Si no, eliges tú según el rol.
1. Escribe el encargo en `.agy-tasks/AAAA-MM-DD-<agente>-<slug>.md` usando `.agy-tasks/_plantilla.md`. Un encargo = una tarea; el ayudante no recuerda nada anterior, así que incluye el contexto mínimo.
2. Lánzalo:
   `powershell -ExecutionPolicy Bypass -File scripts/agy-task.ps1 -Task <ruta> -Model <modelo> -Agent <Nombre>`
3. Lee la sección `## Respuesta de Gemini` añadida al final del .md. Si aparece un `.err`, léelo: con "quota exceeded" pasa la tarea a otro agente con otro modelo (como en "moved from … out of quota") e indícalo en el panel; con otro error, corrígelo y reintenta una sola vez.
4. Comprueba lo que dice contra el código real antes de usarlo. Vuelve a mostrar el panel con una línea de resultado por agente (p. ej. "18 findings, 6 fixed").

## 3. Bucle de revisión (decisiones importantes)
Juno hace el primer pase → tú revisas y escribes tus conclusiones en un encargo nuevo → Mira las critica → tú decides y cuentas al usuario qué aportó cada uno.

## Límites
- Los ayudantes solo leen y responden; los cambios en archivos los haces tú.
- Delega lo que ahorre tokens (leer mucho código, buscar, borradores, primeras revisiones). No delegues lo que cueste más explicar que hacer.
- Si `agy` falla dos veces seguidas, sigue tú solo y avisa.
