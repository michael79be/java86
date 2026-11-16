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
- None

## Status

**Completed:**
- [x] FAT16 64MB HDD image with toolchain
- [x] FreeDOS boot and keyboard interaction
- [x] AUTOEXEC.BAT injection
- [x] gcc-ia16 and DJGPP setup with CWSDPMI
- [x] Preemptive execution: `Native.newProcess()` with bytecode loop
- [x] Test C compilation with gcc-ia16 (`release/java.com`, `release/hello.com`)
- [x] Test Java compilation with janino
- [x] Implement build.mjs orchestrator
- [x] Implement java.c interpreter
- [x] UI: `Graphics.drawHLine`, `drawVLine`, `drawRect`, `fillRect`
- [x] UI: `Button.create(text, font)` + `Button.state(bounds, pressed)` 3D border effect
- [x] UI: `Menu.create(x,y,width,height)` returns `int[] menu` (no draw); `Menu.draw(menu)` fills background color 7 + renders 3D border; `Menu.show(menu)` computes `.MSK` path, captures behind-menu to file (returns `String path`), then draws; `Menu.hide(menu, path)` restores from `.MSK` file
- [x] UI: `MenuItem.create(menu, font, text, pos)` returns `int[] item` ([x, y, width, height, pressed]) filled color 7 with padding + draws text; `MenuItem.state(item, pressed)` draws a down bevel border (color 8/15) like `Button.state` without filling or redrawing text; skips if already in requested state
- [x] UI: `Font.open(path)`, `Font.close()`, `Font.drawChar(font,x,y,char)`, `Font.drawString(font,x,y,text)`, `Font.stringWidth(font,text)` loading widths from `FONTS.DAT`
- [x] UI: `Mouse.click(bounds)` left-click hit test; `Mouse.in(bounds)` inside-bounds test; `Mouse.out(bounds)` outside-bounds test
- [x] Demo: `Desktop` renders ToolBar with a Start button using `Font` + `Button`; Start button toggles a `Menu` overlay via `Menu.show`/`Menu.hide`
- [x] `font.html` browser tool: font/size/bold/italic picker, chars 33-255, 256-color bottom-to-top BMP with DOS palette, DOS 8.3 uppercase filename (font + 2-digit size + B/I), localStorage persistence, copyable FONTS.DAT info line
- [x] `data/FONTS.DAT`: multi-font width table (font name + 223 width bytes per line, \r\n separated)
- [x] `jasm.mjs`: Jasmin-like assembler compiling `.jasm` to Java 1.4 `.class` files (major version 48); writes to `build/<package>/<Class>.class`; variable-length constant pool encoding matching `cpool.c`; `npm run jasm -- <input.jasm> [output_dir]`

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
- HDD: 64MB FAT16 image (in-memory buffer, cached to `bin/hdd-cache.bin`)
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
  ├── hdd-cache.bin (generated FAT16 HDD image)
  ├── devel/ (toolchain)
  │   ├── I16GNU/, DJGPP/ (gcc-ia16)
  │   └── CWSDPMI/
  ├── janino/ (Java compiler JARs)
  ├── mkeyb.exe, vbmouse.exe

src/
  ├── java.c (VM entry point)
  ├── sys.c, jar.c, class.c, cpool.c, method.c, array.c, field.c, opcode.c, math.c, native.c, proc.c, zip.c, hello.c (VM interpreter)
  ├── demo/ (Demo, Desktop, Hello, Proc1, Proc2, Shell)
  ├── driver/ (Mouse)
  ├── io/ (Console, File)
  ├── platform/ (Native)
  ├── ui/ (BMP, Button, Cursor, Font, Graphics, Menu, ToolBar)
  └── utils/ (StringUtils)

data/
  └── FONTS.DAT (multi-font width table)

font.html (browser font BMP generator)

build/
  ├── *.class (Java bytecode)
  └── demo/ (jasm .jasm output, packaged into JARs)

release/
  └── *.com, *.JAR (compiled C binaries: java.com, hello.com; packaged JARs: DEMO.JAR, DRIVER.JAR, IO.JAR, PLATFORM.JAR, UI.JAR, UTILS.JAR)

.mjs scripts:
  ├── init.mjs (download & setup, builds HDD image)
  ├── image.mjs (FAT16 image operations)
  ├── vm.mjs (V86.js VM lifecycle)
  ├── start.mjs (interactive FreeDOS terminal)
  ├── build.mjs (automated Java + C build)
  ├── make.mjs (Java compilation + JAR packaging only)
  ├── jasm.mjs (Jasmin-like assembler, `.jasm` → Java 1.4 `.class`; `npm run jasm -- <input.jasm> [output_dir]`)
  └── emulator.mjs (HTTP server, port 8086, VGA screen)
```

## Scripts

**init.mjs**
- Downloads BIOS, FreeDOS, gcc-ia16 toolchain, DJGPP, CWSDPMI, Janino
- Extracts into `bin/devel/` and `bin/janino/`
- Builds/loads cached HDD image (`bin/hdd-cache.bin`) via image.mjs
- Smart detection: no re-download/re-extract if files exist

**image.mjs**
- FAT16 library: `createBlankFat16HddBuffer`, `mountFatPartition`, `createOrLoadHddImage` (cached to disk)
- `copySources`: copies `src/` to `/SRC/` on the HDD image
- `copyDevel`: copies toolchain to `/I16GNU/` and `/DJGPP/`
- `copyTools`: copies MKEYB/VBMOUSE to `/TOOLS/`
- `copyBuildOutputs`: copies `release/*.COM` and `release/*.JAR` to C:\
- `copyData`: copies `data/` to C:\
- In-memory buffer; optional disk cache

**vm.mjs**
- `VM` class managing V86.js FreeDOS VM lifecycle
- Injects AUTOEXEC.BAT and CONFIG.SYS into the floppy
- Copies sources, toolchain, build outputs, and data to the HDD image
- Starts V86 with 256MB memory, SeaBIOS + VGA BIOS

**start.mjs**
- Launches VM with interactive serial terminal (CTTY COM1 ↔ stdin/stdout)
- Accepts optional main class argument: `npm run start -- <class>` (default: `demo.Desktop`)
- AUTOEXEC.BAT uses `%MAIN_CLASS%` placeholder, replaced at VM boot
- Reads/writes to V86 stdin/stdout

**build.mjs**
- Compiles all Java sources via `make.mjs`, then boots a headless FreeDOS VM
- Copies all C sources to `/SRC/` and compiles the selected file with `i16gcc -mcmodel=tiny`
- Waits for the `.COM` to appear, retrieves it to `release/`
- Usage: `npm run build -- src/java.c`

**make.mjs**
- Compiles all `.java` files under `src/` with Janino into `build/`
- Incremental: skips compilation if no `.java` source is newer than its `.class`
- Packages each top-level package into an uncompressed per-package JAR (e.g. `release/DEMO.JAR`)
- Incremental: skips JAR repackaging if no `.class` in the package is newer than the JAR
- Usage: `npm run make`

**emulator.mjs**
- Node HTTP server (`npm run emulator`, port 8086) serving the browser build of v86 with VGA screen and keyboard
- Prepares the same floppy/HDD images as vm.mjs but strips `CTTY` from AUTOEXEC.BAT so the console stays on the VGA screen
- Replaces `%MAIN_CLASS%` placeholder with `demo.Desktop`

**java.c, sys.c, jar.c, class.c, cpool.c, method.c, array.c, field.c, opcode.c, math.c, native.c, proc.c, zip.c**
- `java.com -classpath DEMO.JAR;PLATFORM.JAR demo.Hello` searches uncompressed ZIP entries in the classpath JARs
- Classpath files stay open until the VM exits; the ten-entry circular cache uses the class name CRC16 without collision checks
- Constant pools load into five-byte entries (tag + four data bytes); UTF-8 entries store the JAR offset of their length field instead of string data; ten-pool circular cache keyed by the class name CRC16
- Interfaces are rejected; fields must be `public static` without initializers: `getstatic`/`putstatic` use a shared 100-slot table keyed by class CRC16 + field name CRC16, zero-initialized lazily, shared by all processes and scanned as GC roots; `run_method` locates a static method's Code attribute via `method_find`, cached in a ten-entry circular cache keyed by class CRC16 and signature CRC16 (e.g. `main([Ljava/lang/String;)V`)
- opcode.c interprets `nop`, constants (`iconst`, `bipush`, `sipush`, `ldc`, `aconst_null`), locals (`iload`/`istore`/`aload`/`astore` + variants, `iinc`), stack (`pop`, `dup`), integer math and bit ops, comparisons and `goto`, `return`/`ireturn`/`areturn`, and arrays (`newarray` int/byte, loads/stores, `arraylength`); strings stay on disk and print as raw bytes without UTF-8 decoding; `main` arguments are ignored
- array.c stores up to 16 arrays; allocation tries NEAR `malloc` first then FAR DOS INT 21h/48h under gcc-ia16 (`__far` access); refs carry the 0x80000000 flag; a conservative mark-and-sweep GC (roots: stacks and locals of active processes) runs when allocation fails
- proc.c schedules up to ten processes preemptively, one opcode per process per round; each process holds one frame (method, pc, 16 stack slots, 8 locals)
- `invokestatic` spawns a child process and blocks the parent until the child returns; `ireturn`/`areturn` push the value onto the parent's stack; the CLI class starts through the same `process_call` path
- native.c dispatches `platform/Native`: `newProcess(String)` returning the process id (detached, no parent), `killProcess(int)`, `toString(byte[])`, `getBytes(String)` (converts JAR strings into heap byte arrays), FAR memory access (`readByte`/`writeByte`/`readBytes`/`writeBytes`/`getAddress`), `fillBytes`, `readPort`, and `interrupt(int, int[9])` returning the modified registers (AX,BX,CX,DX,SI,DI,DS,ES,FLAGS)
- sys.c wraps all I/O as DOS INT 21h calls on ia16 (POSIX on the host); stdio is not linked, keeping the tiny-model binary small

**AUTOEXEC.BAT**
- Auto-run at FreeDOS boot
- Set PATH, DOSKEY, keyboard layout, launch `demo.Desktop` with all JARs
- User can customize (not overwritten)

## Memory Constraints

- VM executable: ≤ 64KB
- Per-class bytecode: ≤ 64KB (with disk swap if needed)
- Trade-off: RAM vs. disk I/O
- Tiny model: code + data + BSS + heap + stack share one 64KB segment; compile with `-Os`; stdio is banned (sys.c DOS calls only) to keep the binary and heap small

## Example Workflow

```bash
npm run init                   # Install Node deps + download toolchain
npm run make                   # Compile Java sources + package JARs to release/ (fast iteration)
npm run jasm -- <input.jasm>   # Assemble a .jasm file to build/<package>/<Class>.class
npm run make                   # Repackage JARs including jasm output to release/
npm run start                  # Interactive FreeDOS terminal (serial)
npm run emulator               # Browser-based VGA emulator (port 8086)
npm run build -- src/java.c    # Full build: Java + compile selected C file to release/
```
