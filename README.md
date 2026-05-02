# Ejecutor de Lotes

Sistema de ejecución de procesos por lotes para Windows 11, implementado en C con Named Pipes Win32.

## Requisitos

- MSYS2 con entorno **UCRT64**
- Paquetes necesarios:
  ```bash
  pacman -S mingw-w64-ucrt-x86_64-gcc make
  ```
- Biblioteca **cJSON** copiada en `src/common/cjson/` (ver abajo)

## Configuración inicial

### 1. Obtener cJSON

```bash
# Desde la raíz del proyecto
mkdir -p src/common/cjson
curl -o src/common/cjson/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -o src/common/cjson/cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
```

### 2. Compilar

```bash
make
```

Los ejecutables quedan en `bin/`.

## Ejecución

### Opción A — Script automático (abre ventanas separadas)

```bash
./start.sh
```

### Opción B — Manual (una terminal por servicio)

```bash
# Terminal 1
./bin/gesfich.exe -x aralmac/ficheros

# Terminal 2
./bin/gesprog.exe -x aralmac/programas

# Terminal 3
./bin/ejecutor.exe -x aralmac/ficheros

# Terminal 4 (después de que los anteriores estén listos)
./bin/ctrllt.exe
```

## Uso del cliente

```bash
# Crear un fichero vacío
./bin/cliente.exe gesfich CREAR

# Leer un fichero
./bin/cliente.exe gesfich LEER '{"id":"f-0001"}'

# Actualizar un fichero con contenido local
./bin/cliente.exe gesfich ACTUALIZAR '{"id":"f-0001","ruta":"C:/datos/entrada.txt"}'

# Registrar un programa
./bin/cliente.exe gesprog GUARDAR '{"ejecutable":"C:/mi_prog.exe","argumentos":[],"ambiente":[]}'

# Ejecutar un lote
./bin/cliente.exe ejecutor EJECUTAR '{"id_programa":"p-0001","fichero_entrada":"f-0001","fichero_salida":"f-0002"}'

# Consultar estado de un lote
./bin/cliente.exe ejecutor ESTADO '{"id_lote":"l-0001"}'

# Matar un lote
./bin/cliente.exe ejecutor MATAR '{"id_lote":"l-0001"}'
```

## Estructura

```
.
├── docs/Diseño.md
├── src/
│   ├── common/         ← pipe_utils, json_utils, cjson
│   ├── ctrllt/
│   ├── gesfich/
│   ├── gesprog/
│   ├── ejecutor/
│   └── cliente/
├── aralmac/
│   ├── ficheros/       ← almacenamiento de gesfich
│   └── programas/      ← almacenamiento de gesprog
├── bin/                ← ejecutables compilados
├── Makefile
└── start.sh
```
