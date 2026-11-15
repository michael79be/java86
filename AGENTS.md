# Java 1.4 Bytecode Interpreter in C

Minimal VM for executing Java 1.4 bytecode from uncompressed ZIP archives on FreeDOS via V86.js.

## Project Scope

**Limitations (by design):**
- Static methods only (no instance methods)
- No object model
- No `long` or `double` types
- No interfaces
- Limited inheritance support

**Native methods:** Via `platform.Native` class (e.g., `Native.interrupt(...)`); console output goes through `io.Console` using DOS interrupts

**Future goals:**
- JASMIN-compatible assembler in Node (jasm.mjs)

## Status

**Completed:**
- [x] FAT16 64MB HDD image with toolchain
- [x] FreeDOS boot and keyboard interaction
- [x] AUTOEXEC.BAT injection
- [x] gcc-ia16 and DJGPP setup with CWSDPMI
- [x] Preemptive execution: `Native.newProcess()` with bytecode loop

**In Progress:**
- [ ] Test C compilation with gcc-ia16
- [x] Test Java compilation with janino
- [ ] Implement build.mjs orchestrator
- [ ] Implement java.c interpreter

## Code Conventions

- **No comments** (code must be self-explanatory)
- Use `.editorconfig` formatting
- Prefer `null === var` over `var === null`
- OOP style (no standalone functions outside classes)
- All logging in English
- No trailing whitespace
- Remove unused npm packages
- No test or demo files unless actively used
- Prefer pure-JS npm packages over native bindings
- **No marker files:** Verify actual file presence instead

## Developer Role

- Autonomous execution of downloads and commands
- Respect project constraints; find alternatives within scope
- Ask when uncertain
- Update AGENTS.md based on chat progress

## Environment

**Local (Node):**
- Scripts: `.mjs` files at project root
- No OS-specific dependencies
- npm packages for everything possible
- `package.json` for npm dependencies

**FreeDOS VM (V86.js):**
- Boots from floppy: `freedos722.img`
- HDD: 64MB FAT16 image (in-memory buffer)
- BIOS: `seabios.bin`, `vgabios.bin`
- Keyboard/mouse: MKEYB, VBMOUSE

**Toolchain:**
- **C compiler:** gcc-ia16 (real/tiny mode)
- **C++ DPMI:** cwsdpmi from `bin/devel/CWSDPMI/`
- **Java compiler:** Janino (CLI)
- **Java runtime:** Custom interpreter (java.c → java.com)

## File Structure

```
bin/
  ├── seabios.bin, vgabios.bin (BIOS)
  ├── freedos722.img (floppy)
  ├── hdd-cache.bin (generated)
  ├── devel/ (toolchain)
  │   ├── I16GNU/, I16BUTIL/, I16NEWLIB/ (gcc-ia16)
  │   ├── DJGPP/ (with cwsdpmi in BIN/)
  │   └── CWSDPMI/
  ├── janino/ (Java compiler JARs)
  ├── mkeyb.exe, vbmouse.exe

src/
  ├── java.c (VM interpreter)
  └── demo/ (Java classes)

build/
  ├── *.class (Java bytecode)
  └── *.jar (archives)

.mjs scripts:
  ├── install.mjs (download & setup)
  ├── image.mjs (FAT16 image operations)
  ├── vm.mjs (V86.js instance)
  ├── start.mjs (interactive FreeDOS)
  └── build.mjs (compile C + Java)
```

## Scripts

**install.mjs**
- Downloads BIOS, FreeDOS, compilers, CWSDPMI
- Extracts into `bin/devel/` and `bin/janino/`
- Builds HDD image via image.mjs
- Smart detection: no re-download/re-extract if files exist

**image.mjs**
- Library for FAT16 operations
- Copies toolchain to `/I16GNU/`, `/DJGPP/`, `/TOOLS/`
- Injects `/SRC/` demo code
- In-memory buffer; no disk writes

**vm.mjs**
- V86.js VM instance for FreeDOS

**emulator.mjs**
- Node HTTP server (`npm run emulator`, port 8086) serving the browser build of v86 with VGA screen and keyboard
- Prepares the same floppy/HDD images as vm.mjs but strips `CTTY` from AUTOEXEC.BAT so the console stays on the VGA screen

**start.mjs**
- Launches VM with interactive terminal
- Reads/writes to V86 stdin/stdout
- Injects AUTOEXEC.BAT for startup commands

**build.mjs**
- Compiles all Java files under `src/` with Janino into `build/` and packages each top-level package in an uncompressed per-package JAR (e.g. `build/DEMO.JAR`, `build/PLATFORM.JAR`)
- Copies C files directly inside `src/` to `/SRC/`
- Compiles only the selected C file with `npm run build -- src/file.c`
- Outputs the compiled `.com` file to `release/`

**java.c, sys.c, jar.c, class.c, cpool.c, method.c, array.c, field.c, opcode.c, math.c, native.c, proc.c, zip.c**
- `java.com -classpath DEMO.JAR;PLATFORM.JAR demo.Hello` searches uncompressed ZIP entries in the classpath JARs
- Classpath files stay open until the VM exits; the ten-entry circular cache uses the class name CRC16 without collision checks
- Constant pools load into five-byte entries (tag + four data bytes); UTF-8 entries store the JAR offset of their length field instead of string data; ten-pool circular cache keyed by the class name CRC16
- Interfaces are rejected; fields must be `public static` without initializers: `getstatic`/`putstatic` use a shared 100-slot table keyed by class CRC16 + field name CRC16, zero-initialized lazily, shared by all processes and scanned as GC roots; `run_method` locates a static method's Code attribute via `method_find`, cached in a ten-entry circular cache keyed by class CRC16 and signature CRC16 (e.g. `main([Ljava/lang/String;)V`)
- opcode.c interprets `nop`, constants (`iconst`, `bipush`, `sipush`, `ldc`, `aconst_null`), locals (`iload`/`istore`/`aload`/`astore` + variants, `iinc`), stack (`pop`, `dup`), integer math and bit ops, comparisons and `goto`, `return`/`ireturn`/`areturn`, and arrays (`newarray` int/byte, loads/stores, `arraylength`); strings stay on disk and print as raw bytes without UTF-8 decoding; `main` arguments are ignored
- array.c stores up to 16 arrays; allocation tries NEAR `malloc` first then FAR DOS INT 21h/48h under gcc-ia16 (`__far` access); refs carry the 0x80000000 flag; a conservative mark-and-sweep GC (roots: stacks and locals of active processes) runs when allocation fails
- proc.c schedules up to ten processes preemptively, one opcode per process per round; each process holds one frame (method, pc, 16 stack slots, 8 locals)
- `invokestatic` spawns a child process and blocks the parent until the child returns; `ireturn`/`areturn` push the value onto the parent's stack; the CLI class starts through the same `process_call` path
- native.c dispatches `platform/Native`: `newProcess(String)` returning the process id (detached, no parent), `killProcess(int)`, `toString(byte[])`, `getBytes(String)` (converts JAR strings into heap byte arrays), FAR memory access (`readByte`/`writeByte`/`readBytes`/`writeBytes`/`getAddress`) and `interrupt(int, int[9])` returning the modified registers (AX,BX,CX,DX,SI,DI,DS,ES,FLAGS)
- sys.c wraps all I/O as DOS INT 21h calls on ia16 (POSIX on the host); stdio is not linked, keeping the tiny-model binary small

**AUTOEXEC.BAT**
- Auto-run at FreeDOS boot
- Set PATH, DOSKEY, keyboard layout
- User can customize (not overwritten)

## Memory Constraints

- VM executable: ≤ 64KB
- Per-class bytecode: ≤ 64KB (with disk swap if needed)
- Trade-off: RAM vs. disk I/O
- Tiny model: code + data + BSS + heap + stack share one 64KB segment; compile with `-Os`; stdio is banned (sys.c DOS calls only) to keep the binary and heap small

## Example Workflow

```bash
npm run init                   # Install Node deps
npm run start                  # Interactive FreeDOS terminal
npm run build -- src/java.c    # Copy C files and compile the selected file
```
