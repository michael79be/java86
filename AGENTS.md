# Java 1.4 Bytecode Interpreter in C

Minimal VM for executing Java 1.4 bytecode from uncompressed ZIP archives on FreeDOS via V86.js.

## Project Scope

**Limitations (by design):**
- Static methods only (no instance methods)
- No object model
- No `long` or `double` types
- No interfaces
- Limited inheritance support

**Native methods:** Via `platform.Native` class (e.g., `Native.print(...)`)

**Future goals:**
- JASMIN-compatible assembler in Node (jasm.mjs)
- Preemptive execution: `Native.newProcess()` with bytecode loop

## Status

**Completed:**
- [x] FAT16 64MB HDD image with toolchain
- [x] FreeDOS boot and keyboard interaction
- [x] AUTOEXEC.BAT injection
- [x] gcc-ia16 and DJGPP setup with CWSDPMI

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

**start.mjs**
- Launches VM with interactive terminal
- Reads/writes to V86 stdin/stdout
- Injects AUTOEXEC.BAT for startup commands

**build.mjs**
- Compiles all Java files under `src/` with Janino into `build/` and packages the classes in an uncompressed `build/CLASSES.JAR`
- Copies C files directly inside `src/` to `/SRC/`
- Compiles only the selected C file with `npm run build -- src/file.c`
- Outputs the compiled `.com` file to `release/`

**java.c, jar.c, zip.c**
- `java.com -classpath CLASSES.JAR demo.Hello` searches uncompressed ZIP entries in the classpath JARs
- Classpath files stay open until the VM exits; the ten-entry circular cache uses the class name CRC16 without collision checks

**AUTOEXEC.BAT**
- Auto-run at FreeDOS boot
- Set PATH, DOSKEY, keyboard layout
- User can customize (not overwritten)

## Memory Constraints

- VM executable: ≤ 64KB
- Per-class bytecode: ≤ 64KB (with disk swap if needed)
- Trade-off: RAM vs. disk I/O

## Example Workflow

```bash
npm run init                   # Install Node deps
npm run start                  # Interactive FreeDOS terminal
npm run build -- src/java.c    # Copy C files and compile the selected file
```
