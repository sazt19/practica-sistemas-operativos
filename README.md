# Ejecutor de Lotes

**Estudiante:** Sara Zuluaga Trujillo
**Curso:** Sistemas Operativos
**Plataforma:** Windows 11 — MSYS2 UCRT64

Sistema de ejecución de procesos por lotes implementado en C con Named Pipes Win32.

## Requisitos

- MSYS2 con entorno **UCRT64**
- Paquetes necesarios:
```bash
  pacman -S mingw-w64-ucrt-x86_64-gcc make
```
- Biblioteca **cJSON** en `src/common/cjson/`

## Configuración inicial

### 1. Clonar el repositorio

```bash
git clone https://github.com/sazt19/practica-sistemas-operativos.git
cd practica-sistemas-operativos
```

### 2. Obtener cJSON

```bash
mkdir -p src/common/cjson
curl -o src/common/cjson/cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -o src/common/cjson/cJSON.c  https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
```

### 3. Compilar

```bash
make
```

Los ejecutables quedan en `bin/`.

## Ejecución

Abrir **4 terminales MSYS2** en la raíz del proyecto, en este orden:

```bash
# Terminal 1
./bin/gesfich.exe -x aralmac/ficheros

# Terminal 2
./bin/gesprog.exe -x aralmac/programas

# Terminal 3
./bin/ejecutor.exe -x aralmac/ficheros

# Terminal 4 (cuando las anteriores digan "Servidor escuchando")
./bin/ctrllt.exe
```

## Ejemplo de flujo completo

```bash
# 1. Crear ficheros de entrada y salida
./bin/cliente.exe gesfich CREAR   # → f-0001
./bin/cliente.exe gesfich CREAR   # → f-0002

# 2. Cargar contenido en el fichero de entrada
echo "banana
apple
cherry
date" > /tmp/entrada.txt
./bin/cliente.exe gesfich ACTUALIZAR '{"id":"f-0001","ruta":"C:/msys64/tmp/entrada.txt"}'

# 3. Registrar un programa
./bin/cliente.exe gesprog GUARDAR '{"ejecutable":"C:/Windows/System32/sort.exe","argumentos":[],"ambiente":[]}'

# 4. Ejecutar el lote
./bin/cliente.exe ejecutor EJECUTAR '{"id_programa":"p-0001","fichero_entrada":"f-0001","fichero_salida":"f-0002"}'

# 5. Consultar estado
./bin/cliente.exe ejecutor ESTADO '{"id_lote":"l-0001"}'

# 6. Ver resultado (frutas ordenadas alfabéticamente)
./bin/cliente.exe gesfich LEER '{"id":"f-0002"}'
```

## Referencia del cliente

```bash
./bin/cliente.exe gesfich  CREAR
./bin/cliente.exe gesfich  LEER      '{"id":"f-0001"}'
./bin/cliente.exe gesfich  ACTUALIZAR '{"id":"f-0001","ruta":"C:/ruta/archivo.txt"}'
./bin/cliente.exe gesfich  BORRAR    '{"id":"f-0001"}'

./bin/cliente.exe gesprog  GUARDAR   '{"ejecutable":"C:/prog.exe","argumentos":[],"ambiente":[]}'
./bin/cliente.exe gesprog  LEER      '{"id":"p-0001"}'
./bin/cliente.exe gesprog  BORRAR    '{"id":"p-0001"}'

./bin/cliente.exe ejecutor EJECUTAR  '{"id_programa":"p-0001","fichero_entrada":"f-0001","fichero_salida":"f-0002"}'
./bin/cliente.exe ejecutor ESTADO    '{"id_lote":"l-0001"}'
./bin/cliente.exe ejecutor MATAR     '{"id_lote":"l-0001"}'
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
└── Makefile
```