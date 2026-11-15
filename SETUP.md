# Project Setup and Build

## First Time Setup

```bash
npm run init
```

This downloads and prepares:
- FreeDOS OS image
- gcc-ia16 compiler packages
- janino Java compiler
- BIOS files for V86

## Development Environment

### Interactive VM Session
```bash
npm run start
```

This boots FreeDOS with gcc-ia16 packages on the hard drive and copies `release/*.COM` plus `build/CLASSES.JAR` to the root of C: when present.

First boot requires setup:
```dos
format C: /Q
md devel
cd devel
unzip ..\i16*.zip
set PATH=%PATH%;c:\devel\bin
```

### Building the Project
```bash
npm run build -- src/hello.c
```

Compiles:
- Java classes (`.java` → `.class`) using Janino and packages them in an uncompressed `build/CLASSES.JAR`
- The selected C program (`.c` → `.COM`) using gcc-ia16 in the FreeDOS VM

## Architecture

- `init.mjs`: Downloads and prepares all components
- `vm.mjs`: Manages V86 FreeDOS VM lifecycle
- `start.mjs`: Interactive VM terminal for manual work
- `build.mjs`: Automated build process

## Project Structure

- `src/`: Source files (Java with packages, C directly)
- `bin/`: Downloaded binaries and disk images
- `build/`: Compiled output (.class, .jar)
