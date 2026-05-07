# Diseño del Sistema: Ejecutor de Lotes

**Curso:** Sistemas Operativos  
**Estudiante:** Sara Zuluaga Trujillo  
**Fecha:** Mayo 2025  
**Plataforma:** Windows 11  
**Lenguaje:** C (C11)  
**Repositorio:** https://github.com/sazt19/practica-sistemas-operativos

---

## 1. Descripción General

El sistema **Ejecutor de Lotes** simula un entorno de ejecución de procesos por lotes similar al encontrado en sistemas operativos de mainframe. El sistema está compuesto por cinco procesos que se comunican entre sí mediante **Named Pipes (tuberías nombradas)** de Win32.

Los componentes del sistema son:

- **cliente**: Interfaz de línea de comandos que envía peticiones al sistema.
- **ctrllt**: Pasarela central que recibe peticiones del cliente y las redirige al servicio correspondiente.
- **gesfich**: Gestiona los ficheros almacenados en el área de almacenamiento (`aralmac`).
- **gesprog**: Gestiona los programas (ejecutables) almacenados en `aralmac`.
- **ejecutor**: Ejecuta los procesos de lotes utilizando los programas y ficheros registrados.

```
cliente  ──►  ctrllt  ──►  gesfich
                      ──►  gesprog
                      ──►  ejecutor
                                └──►  aralmac
```

---

## 2. Plataforma y Herramientas

| Elemento          | Descripción                        |
|-------------------|------------------------------------|
| Sistema operativo | Windows 11                         |
| Lenguaje          | C (estándar C11)                   |
| Compilador        | gcc (MSYS2 UCRT64)                 |
| Build             | GNU Make                           |
| IPC               | Named Pipes Win32 (full-duplex)    |
| Formato mensajes  | JSON (biblioteca cJSON)            |
| Repositorio       | GitHub                             |

---

## 3. Comunicación entre Procesos

### 3.1 Mecanismo: Named Pipes de Windows

En Windows las Named Pipes son **full-duplex**, lo que permite enviar y recibir datos por la misma tubería. Por lo tanto, cada conexión lógica requiere **una sola tubería nombrada**, no dos.

Los nombres de las tuberías siguen el formato estándar de Windows:

```
\\.\pipe\<nombre>
```

| Tubería                    | Servidor  | Cliente  | Propósito                       |
|----------------------------|-----------|----------|---------------------------------|
| `\\.\pipe\ctrllt_cliente`  | ctrllt    | cliente  | Peticiones del cliente a ctrllt |
| `\\.\pipe\ctrllt_gesfich`  | gesfich   | ctrllt   | ctrllt redirige a gesfich       |
| `\\.\pipe\ctrllt_gesprog`  | gesprog   | ctrllt   | ctrllt redirige a gesprog       |
| `\\.\pipe\ctrllt_ejecutor` | ejecutor  | ctrllt   | ctrllt redirige a ejecutor      |

### 3.2 Formato de Mensajes: JSON

Todos los mensajes son cadenas JSON. Se utiliza la biblioteca **cJSON** para serializar y deserializar.

**Petición:**
```json
{
  "servicio":   "gesfich | gesprog | ejecutor",
  "operacion":  "CREAR | LEER | GUARDAR | EJECUTAR | ...",
  "parametros": { }
}
```

**Respuesta:**
```json
{
  "estado":  "OK | ERROR",
  "datos":   { },
  "mensaje": "descripción opcional"
}
```

---

## 4. Diseño de Componentes

### 4.1 cliente

Proceso que el usuario ejecuta para interactuar con el sistema. Envía peticiones JSON a `ctrllt` y muestra las respuestas.

**Sinopsis:**
```
cliente <servicio> <operacion> [parametros_json]
```

**Responsabilidades:**
- Conectarse a la tubería `ctrllt_cliente`.
- Serializar la petición como JSON y enviarla.
- Leer la respuesta JSON y mostrarla al usuario.

---

### 4.2 ctrllt (Control de Lotes)

Proceso central del sistema. Actúa como **pasarela**: recibe peticiones de múltiples clientes, determina a qué servicio pertenece cada una, la reenvía y devuelve la respuesta al cliente original.

**Sinopsis:**
```
ctrllt
```

**Responsabilidades:**
- Crear y escuchar la tubería `ctrllt_cliente`.
- Mantener conexiones con `gesfich`, `gesprog` y `ejecutor`.
- Parsear el campo `"servicio"` del JSON para hacer el ruteo.
- Soportar múltiples clientes concurrentes con un hilo por conexión (`CreateThread`).

**Máquina de estados:**

```
[Inicio] ──► [Corriendo] ──► [Terminar] ──► [Terminado]
```

| Estado    | Descripción                                   |
|-----------|-----------------------------------------------|
| Inicio    | Crea tuberías y establece conexiones          |
| Corriendo | Atiende peticiones de clientes en bucle       |
| Terminar  | Recibió señal de cierre, finaliza conexiones  |
| Terminado | Proceso terminado                             |

---

### 4.3 gesfich (Gestor de Ficheros)

Administra ficheros en el área de almacenamiento `aralmac/ficheros`.

**Sinopsis:**
```
gesfich -x <ruta-aralmac>
```

**Máquina de estados:**

```
[Inicio] ──► [Corriendo] ◄──► [Suspendido]
                 │
            [Terminado]
```

**Operaciones:**

| Operación  | Entrada                    | Salida                      |
|------------|----------------------------|-----------------------------|
| CREAR      | —                          | `id-fichero` (ej. `f-0001`) |
| LEER       | `id` (opcional)            | Contenido o lista           |
| ACTUALIZAR | `id`, `ruta` fichero local | `OK` o `ERROR`              |
| BORRAR     | `id`                       | `OK` o `ERROR`              |
| SUSPENDER  | —                          | Pasa a Suspendido           |
| REASUMIR   | —                          | Pasa a Corriendo            |
| TERMINAR   | —                          | Pasa a Terminado            |

**Formato de ID:** `f-XXXX` (ej. `f-0001`, `f-0042`)

---

### 4.4 gesprog (Gestor de Programas)

Administra los ejecutables en `aralmac/programas`. Cada programa se almacena en su propio directorio junto con un archivo `meta.json` que contiene sus metadatos.

**Sinopsis:**
```
gesprog -x <ruta-aralmac>
```

**Máquina de estados:** idéntica a la de `gesfich`.

**Operaciones:**

| Operación  | Entrada                                | Salida                       |
|------------|----------------------------------------|------------------------------|
| GUARDAR    | `ejecutable`, `argumentos`, `ambiente` | `id-programa` (ej. `p-0001`) |
| LEER       | `id` (opcional)                        | Metadatos o lista            |
| BORRAR     | `id`                                   | `OK` o `ERROR`               |
| SUSPENDER  | —                                      | Pasa a Suspendido            |
| REASUMIR   | —                                      | Pasa a Corriendo             |
| TERMINAR   | —                                      | Pasa a Terminado             |

**Formato de ID:** `p-XXXX` (ej. `p-0001`)

**Metadatos almacenados (`meta.json`):**
```json
{
  "id":          "p-0001",
  "ejecutable":  "aralmac\\programas\\p-0001\\sort.exe",
  "argumentos":  [],
  "ambiente":    []
}
```

---

### 4.5 ejecutor

Lanza y gestiona los procesos de lotes a partir de los programas y ficheros registrados en `aralmac`.

**Sinopsis:**
```
ejecutor -x <ruta-aralmac>
```

**Máquina de estados:**

```
[Inicio] ──► [Ejecutar] ◄──► [Suspendidos]
                 │
              [Parar]
                 │
            [Terminado]
```

**Operaciones:**

| Operación  | Entrada                                              | Salida                    |
|------------|------------------------------------------------------|---------------------------|
| EJECUTAR   | `id_programa`, `fichero_entrada`, `fichero_salida`   | `id-lote` (ej. `l-0001`) |
| ESTADO     | `id_lote` (opcional)                                 | Estado actual o lista     |
| MATAR      | `id_lote`                                            | `OK` o `ERROR`            |
| SUSPENDER  | —                                                    | Pasa a Suspendidos        |
| REASUMIR   | —                                                    | Pasa a Ejecutar           |
| PARAR      | —                                                    | Pasa a Parar              |
| TERMINAR   | —                                                    | Pasa a Terminado          |

**Petición de ejecución:**
```json
{
  "servicio":   "ejecutor",
  "operacion":  "EJECUTAR",
  "parametros": {
    "id_programa":     "p-0001",
    "fichero_entrada": "f-0001",
    "fichero_salida":  "f-0002"
  }
}
```

La ejecución usa `CreateProcess()` de Win32 con redirección de `stdin`/`stdout` a los ficheros de `aralmac`.

---

## 5. Estructura del Repositorio

```
practica-sistemas-operativos/
├── docs/
│   └── Diseño.md
├── src/
│   ├── common/
│   │   ├── pipe_utils.h      ← API de Named Pipes Win32
│   │   ├── pipe_utils.c
│   │   ├── json_utils.h      ← Wrapper sobre cJSON
│   │   ├── json_utils.c
│   │   └── cjson/
│   │       ├── cJSON.h
│   │       └── cJSON.c
│   ├── ctrllt/main.c
│   ├── gesfich/main.c
│   ├── gesprog/main.c
│   ├── ejecutor/main.c
│   └── cliente/main.c
├── aralmac/
│   ├── ficheros/             ← Almacenamiento de gesfich
│   └── programas/            ← Almacenamiento de gesprog
├── bin/                      ← Ejecutables compilados
├── Makefile
└── README.md
```

---

## 6. Consideraciones de Implementación

### Named Pipes Win32
- Se usa `CreateNamedPipe()` en el proceso servidor de cada tubería.
- Se usa `CreateFile()` en el proceso cliente para conectarse.
- Al ser full-duplex en Windows no se necesita tubería de retorno separada.
- `ctrllt` crea un hilo (`CreateThread`) por cada cliente que se conecta para soportar múltiples clientes concurrentes.

### Creación de procesos
- `ejecutor` usa `CreateProcess()` con `STARTUPINFO` para redirigir `stdin`/`stdout` a los ficheros de `aralmac`.
- Los procesos hijo heredan los handles de fichero necesarios (`bInheritHandles = TRUE`).

### Sincronización
- Se usa `CRITICAL_SECTION` para proteger el acceso concurrente a los contadores de IDs en `gesfich`, `gesprog` y `ejecutor`.

### Biblioteca JSON
- Se usa **cJSON** (licencia MIT) para serializar y deserializar todos los mensajes del sistema.
