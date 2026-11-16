# java86

**Minimal Java 1.4 bytecode interpreter for 16‑bit DOS**

This repository contains a tiny virtual machine that can run Java 1.4 class files on a 16‑bit DOS environment. The VM is built on top of the **v86.js** x86 emulator running under Node.js, providing a full DOS system with serial I/O redirection.

---

## Quick start

```bash
# Install node dependencies
npm install

# Download DOS dependencies
npm run init

# Start DOS with a serial console (CTTY COM1)
npm run start
```

The terminal you are running the command from will be hooked up to `COM1` of the emulated DOS machine, so any DOS command you type is executed inside the VM. The VM automatically copies compiled `.COM` binaries from `release/` and all `.JAR` files from `build/` to the root of the C: drive.

---

## Architecture overview

- **v86.js + DOS** – 16‑bit x86 emulator written in JavaScript. Runs inside Node.js and provides a VGA screen, keyboard and serial ports.
- **Serial redirection** – `CTTY COM1` inside DOS is wired to the Node.js process’ `stdin`/`stdout`, giving you a live console.
- **C toolchain** – `gcc-ia16` (tiny model) is used inside the emulated environment to compile the VM (`java.c`) and any native helpers.
- **Java compilation** – Java source files in `src/` are compiled with **Janino** (via `make.mjs`). The resulting `.class` files are packaged into **uncompressed JARs** (one per top‑level package) placed in `build/`.
- **Native bridge** – `platform.Native` exposes a small set of native functions (memory access, interrupts, I/O) to Java code.
- **Executable size** – The final binary (`java.com`) is a **NEAR** model executable limited to **< 64 KB**.

---

## Constraints (by design)

- Only **static methods** are supported – no object instances.
- No support for `long`, `double`, interfaces or inheritance beyond the simple static lookup.
- All classes must be stored **uncompressed** inside ZIP/JAR entries.
- Classpath is a semi‑colon (`;`) separated list of JAR files.
- Native methods are called through `platform.Native`.
- The VM runs in a **tiny** memory model (single 64 KB segment for code, data, stack and heap).

---

## Build scripts

| Script | Description |
|--------|-------------|
| `npm run start` | Launches DOS with serial redirection, copies `.COM` files from `release/` and `.JAR` files from `build/` to the C: drive. |
| `npm run emulator` | Serves the V86.js UI on `http://localhost:8086/` – you can interact with DOS through a VGA screen in the browser. |
| `npm run build -- src/java.c` | Compiles all Java source files in `src/` with Janino, creates an uncompressed JAR per package, then compiles the specified C file (`java.c`) into `java.com`. |
| `npm run make` | Runs `make.mjs` – a dedicated script that only compiles Java sources and creates the JARs (useful for faster iteration). |
| `npm run init` | Downloads all development dependencies (v86.js, Janino, gcc‑ia16 toolchain, etc.). |

---

## Usage inside DOS

```bash
java -classpath DEMO.JAR;IO.JAR;PLATFORM.JAR demo.Hello
```

- The JAR files stay open for the lifetime of the VM; class loading uses a ten‑entry circular cache indexed by the CRC‑16 of the class name.
- The interpreter expects a `main(String[] args)` static method.

---

## Current status

- ✅ v86.js emulation and serial redirection working
- ✅ Java compilation via Janino and JAR packaging
- ✅ gcc‑ia16 toolchain integration (`release/java.com`, `release/hello.com`)
- ✅ Full Java bytecode interpreter implemented (`src/java.c` through `src/zip.c`)
- ⏳ Fix mouse handling: use absolute mouse mode, invert Y, clamp to screen height

---

## Contributing

Feel free to open issues or submit pull requests. The project follows a **no‑comments** code style (self‑explanatory identifiers, 4‑space indentation, no trailing whitespace).

---

## License

This project is released under the MIT License.
