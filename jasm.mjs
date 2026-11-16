import { resolve, extname, relative, sep, dirname } from 'path'
import { readdirSync, readFileSync, writeFileSync, mkdirSync, existsSync } from 'fs'

const OPCODES = {
    nop: 0x00,
    iconst_m1: 0x02, iconst_0: 0x03, iconst_1: 0x04, iconst_2: 0x05,
    iconst_3: 0x06, iconst_4: 0x07, iconst_5: 0x08,
    bipush: 0x10, sipush: 0x11, ldc: 0x12,
    aload: 0x15, iload: 0x19,
    aload_0: 0x1a, aload_1: 0x1b, aload_2: 0x1c, aload_3: 0x1d,
    iload_0: 0x2a, iload_1: 0x2b, iload_2: 0x2c, iload_3: 0x2d,
    istore: 0x36, astore: 0x3a,
    istore_0: 0x3b, istore_1: 0x3c, istore_2: 0x3d, istore_3: 0x3e,
    astore_0: 0x4b, astore_1: 0x4c, astore_2: 0x4d, astore_3: 0x4e,
    aastore: 0x4f, bastore: 0x54,
    pop: 0x57, dup: 0x59,
    iadd: 0x60, isub: 0x64, imul: 0x68, idiv: 0x6c, irem: 0x70,
    iand: 0x74, ior: 0x78, ixor: 0x7a,
    ishl: 0x7c, ishr: 0x7e, iushr: 0x80,
    iinc: 0x84,
    if_icmpeq: 0x9f, if_icmpne: 0xa0, if_icmplt: 0xa1, if_icmpge: 0xa2,
    if_icmpgt: 0xa3, if_icmple: 0xa4,
    ifeq: 0xa5, ifne: 0xa6, goto: 0xa7,
    ireturn: 0xb0, areturn: 0xac, return: 0xb1,
    getstatic: 0xb2, putstatic: 0xb3,
    invokestatic: 0xb6, invokespecial: 0xb7, invokevirtual: 0xb8,
    newarray: 0xbc, arraylength: 0xbe,
}

const BRANCH_OPCODES = new Set([0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7])

const TAG = {
    Utf8: 1, Integer: 3, Float: 4, Class: 7, String: 8,
    Fieldref: 9, Methodref: 10, InterfaceMethodref: 11, NameAndType: 12
}

function u16(n) { return Buffer.from([(n >> 8) & 0xff, n & 0xff]) }
function u32(n) { return Buffer.from([(n >>> 24) & 0xff, (n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff]) }

class ClassFile {
    constructor() {
        this.constantPool = [{ tag: 0, data: Buffer.alloc(4) }]
        this.utf8Entries = []
        this.className = ''
        this.superName = 'java/lang/Object'
        this.accessFlags = 0x0001
        this.classCpIndex = 0
        this.superCpIndex = 0
        this.codeUtf8Index = 0
        this.methods = []
        this.labels = {}
    }

    addUtf8(str) {
        for (const entry of this.utf8Entries) {
            if (entry.string === str) return entry.index
        }
        const index = this.constantPool.length
        this.constantPool.push({ tag: TAG.Utf8, data: Buffer.alloc(4) })
        this.utf8Entries.push({ index, string: str })
        return index
    }

    addClass(className) {
        const utf8Index = this.addUtf8(className)
        const index = this.constantPool.length
        const data = Buffer.alloc(4)
        data[2] = (utf8Index >> 8) & 0xff
        data[3] = utf8Index & 0xff
        this.constantPool.push({ tag: TAG.Class, data })
        return index
    }

    addString(str) {
        const utf8Index = this.addUtf8(str)
        const index = this.constantPool.length
        const data = Buffer.alloc(4)
        data[2] = (utf8Index >> 8) & 0xff
        data[3] = utf8Index & 0xff
        this.constantPool.push({ tag: TAG.String, data })
        return index
    }

    addInteger(value) {
        const index = this.constantPool.length
        const data = u32(value)
        this.constantPool.push({ tag: TAG.Integer, data })
        return index
    }

    addNameAndType(name, descriptor) {
        const nameIndex = this.addUtf8(name)
        const descIndex = this.addUtf8(descriptor)
        const index = this.constantPool.length
        const data = Buffer.alloc(4)
        data[0] = (nameIndex >> 8) & 0xff
        data[1] = nameIndex & 0xff
        data[2] = (descIndex >> 8) & 0xff
        data[3] = descIndex & 0xff
        this.constantPool.push({ tag: TAG.NameAndType, data })
        return index
    }

    addMethodref(className, name, descriptor) {
        const classIndex = this.addClass(className)
        const natIndex = this.addNameAndType(name, descriptor)
        const index = this.constantPool.length
        const data = Buffer.alloc(4)
        data[0] = (classIndex >> 8) & 0xff
        data[1] = classIndex & 0xff
        data[2] = (natIndex >> 8) & 0xff
        data[3] = natIndex & 0xff
        this.constantPool.push({ tag: TAG.Methodref, data })
        return index
    }

    addFieldref(className, name, descriptor) {
        const classIndex = this.addClass(className)
        const natIndex = this.addNameAndType(name, descriptor)
        const index = this.constantPool.length
        const data = Buffer.alloc(4)
        data[0] = (classIndex >> 8) & 0xff
        data[1] = classIndex & 0xff
        data[2] = (natIndex >> 8) & 0xff
        data[3] = natIndex & 0xff
        this.constantPool.push({ tag: TAG.Fieldref, data })
        return index
    }

    finalize() {
        this.classCpIndex = this.addClass(this.className)
        this.superCpIndex = this.addClass(this.superName)
        this.codeUtf8Index = this.addUtf8('Code')
    }

    emit() {
        const cpCount = this.constantPool.length
        const utf8Offset = new Map()

        let pos = 10
        for (let i = 1; i < cpCount; i++) {
            const entry = this.constantPool[i]
            if (entry.tag === TAG.Utf8) {
                utf8Offset.set(i, pos)
                const entryInfo = this.utf8Entries.find(e => e.index === i)
                pos += 1 + 2 + Buffer.byteLength(entryInfo.string)
            } else if (entry.tag === TAG.Class || entry.tag === TAG.String) {
                pos += 1 + 2
            } else {
                pos += 1 + 4
            }
        }

        const parts = []

        parts.push(Buffer.from([0xca, 0xfe, 0xba, 0xbe, 0, 0, 0, 48]))
        parts.push(u16(cpCount))

        for (let i = 1; i < cpCount; i++) {
            const entry = this.constantPool[i]
            parts.push(Buffer.from([entry.tag]))
            if (entry.tag === TAG.Utf8) {
                const off = utf8Offset.get(i)
                entry.data[0] = (off >>> 24) & 0xff
                entry.data[1] = (off >>> 16) & 0xff
                entry.data[2] = (off >>> 8) & 0xff
                entry.data[3] = off & 0xff
                const entryInfo = this.utf8Entries.find(e => e.index === i)
                const strBuf = Buffer.from(entryInfo.string, 'utf8')
                parts.push(u16(strBuf.length))
                parts.push(strBuf)
            } else if (entry.tag === TAG.Class || entry.tag === TAG.String) {
                parts.push(Buffer.from([entry.data[2], entry.data[3]]))
            } else {
                parts.push(entry.data)
            }
        }

        parts.push(u16(this.accessFlags))
        parts.push(u16(this.classCpIndex))
        parts.push(u16(this.superCpIndex))
        parts.push(u16(0))

        parts.push(u16(0))

        const methodBufs = []
        for (const method of this.methods) {
            methodBufs.push(this.emitMethod(method))
        }
        parts.push(u16(methodBufs.length))
        for (const buf of methodBufs) parts.push(buf)

        parts.push(u16(0))

        return Buffer.concat(parts)
    }

    emitMethod(method) {
        const parts = []
        parts.push(u16(method.accessFlags | 0x0008))
        parts.push(u16(method.nameIndex))
        parts.push(u16(method.descIndex))
        parts.push(u16(1))

        const codeBuf = this.emitCode(method)
        const attrLen = codeBuf.length

        parts.push(u16(this.codeUtf8Index))
        parts.push(u32(attrLen))
        parts.push(codeBuf)

        return Buffer.concat(parts)
    }

    emitCode(code) {
        const instOffsets = []
        let offset = 0

        for (const inst of code.instructions) {
            instOffsets.push(offset)
            offset++
            const operand = inst.operand
            if (operand !== null && operand !== undefined) {
                if (BRANCH_OPCODES.has(inst.opcode)) {
                    offset += 2
                } else if (inst.opcode === 0x12) {
                    offset += 1
                } else if (inst.opcode === 0x10) {
                    offset += 1
                } else if (inst.opcode === 0x11) {
                    offset += 2
                } else if (inst.opcode === 0x84) {
                    offset += 2
                } else if ([0x15, 0x19, 0x36, 0x3a].includes(inst.opcode)) {
                    offset += 1
                } else if (inst.opcode === 0xbc) {
                    offset += 1
                } else if ([0xb2, 0xb3, 0xb6, 0xb7, 0xb8].includes(inst.opcode)) {
                    offset += 2
                }
            }
        }

        const parts = []
        parts.push(u16(code.maxStack))
        parts.push(u16(code.maxLocals))
        parts.push(u32(offset))

        for (let i = 0; i < code.instructions.length; i++) {
            const inst = code.instructions[i]
            const instOffset = instOffsets[i]
            parts.push(Buffer.from([inst.opcode]))

            const operand = inst.operand
            if (operand !== null && operand !== undefined) {
                if (BRANCH_OPCODES.has(inst.opcode)) {
                    const labelInstIdx = code.labels ? code.labels[operand] : undefined
                    const target = labelInstIdx !== undefined ? instOffsets[labelInstIdx] : undefined
                    if (target === undefined) {
                        throw new Error(`Undefined label: ${operand}`)
                    }
                    const branchOffset = target - instOffset
                    parts.push(u16(branchOffset))
                } else if (inst.opcode === 0x12) {
                    parts.push(Buffer.from([operand.index & 0xff]))
                } else if (inst.opcode === 0x10) {
                    parts.push(Buffer.from([operand & 0xff]))
                } else if (inst.opcode === 0x11) {
                    parts.push(u16(operand))
                } else if (inst.opcode === 0x84) {
                    parts.push(Buffer.from([operand[0] & 0xff, operand[1] & 0xff]))
                } else if ([0x15, 0x19, 0x36, 0x3a].includes(inst.opcode)) {
                    parts.push(Buffer.from([operand & 0xff]))
                } else if (inst.opcode === 0xbc) {
                    parts.push(Buffer.from([operand]))
                } else if ([0xb2, 0xb3, 0xb6, 0xb7, 0xb8].includes(inst.opcode)) {
                    parts.push(Buffer.from([(operand.index >> 8) & 0xff, operand.index & 0xff]))
                }
            }
        }

        parts.push(u16(0))
        parts.push(u16(0))

        return Buffer.concat(parts)
    }
}

function parseDescriptorRef(ref) {
    const openParen = ref.indexOf('(')
    const closeParen = ref.lastIndexOf(')')
    if (openParen < 0 || closeParen < 0) return null
    const classPart = ref.substring(0, openParen)
    const slash = classPart.lastIndexOf('/')
    let className = classPart
    let name = ''
    if (slash >= 0) {
        name = classPart.substring(slash + 1)
        className = classPart.substring(0, slash)
    }
    const descriptor = ref.substring(openParen)
    return { className, name, descriptor }
}

function parseLine(line) {
    line = line.replace(/\/\/.*$/, '').trim()
    line = line.replace(/(^|\s);.*$/, '$1').trim()
    if (!line) return null

    const labelMatch = line.match(/^([A-Za-z_$][A-Za-z0-9_$]*):\s*$/)
    if (labelMatch) return { type: 'label', name: labelMatch[1] }

    if (line.startsWith('.')) {
        const parts = line.split(/\s+/)
        const directive = parts[0]

        if (directive === '.class') {
            const flags = parts[1] === 'public' ? 0x0001 : 0
            const name = parts.slice(2).join(' ')
            return { type: 'class', accessFlags: flags, name }
        }
        if (directive === '.super') {
            return { type: 'super', name: parts[1] }
        }
        if (directive === '.method') {
            let accessFlags = 0
            let startIdx = 1
            if (parts[1] === 'public' || parts[1] === 'private' || parts[1] === 'protected') {
                accessFlags = parts[1] === 'public' ? 0x0001 : parts[1] === 'private' ? 0x0002 : 0x0004
                startIdx = 2
            }
            if (parts[startIdx] === 'static') {
                accessFlags |= 0x0008
                startIdx++
            }
            const sig = parts.slice(startIdx).join(' ')
            const openParen = sig.indexOf('(')
            const closeParen = sig.lastIndexOf(')')
            if (openParen < 0 || closeParen < 0) {
                throw new Error(`Invalid method signature: ${sig}`)
            }
            const name = sig.substring(0, openParen)
            const descriptor = sig.substring(openParen)
            return { type: 'method', accessFlags, name, descriptor }
        }
        if (directive === '.end' && parts[1] === 'method') {
            return { type: 'endMethod' }
        }
        if (directive === '.limit') {
            return { type: 'limit', kind: parts[1], value: parseInt(parts[2], 10) }
        }
        return { type: 'unknown', line }
    }

    const instParts = line.split(/\s+/)
    const mnemonic = instParts[0]
    const opcode = OPCODES[mnemonic]
    if (opcode === undefined) {
        throw new Error(`Unknown opcode: ${mnemonic}`)
    }

    const operandStr = instParts.slice(1).join(' ')
    let operand = null

    if (operandStr) {
        if (BRANCH_OPCODES.has(opcode)) {
            operand = operandStr
        } else if (opcode === 0x12) {
            if (operandStr.startsWith('"') && operandStr.endsWith('"')) {
                operand = { type: 'string', value: operandStr.slice(1, -1) }
            } else {
                operand = { type: 'int', value: parseInt(operandStr, 10) }
            }
        } else if (opcode === 0x10) {
            operand = parseInt(operandStr, 10)
        } else if (opcode === 0x11) {
            operand = parseInt(operandStr, 10)
        } else if (opcode === 0x84) {
            const p = operandStr.split(/\s+/)
            operand = [parseInt(p[0], 10), parseInt(p[1], 10)]
        } else if ([0x15, 0x19, 0x36, 0x3a].includes(opcode)) {
            operand = parseInt(operandStr, 10)
        } else if (opcode === 0xbc) {
            operand = operandStr === 'int' ? 10 : operandStr === 'byte' ? 8 : parseInt(operandStr, 10)
        } else {
            const ref = parseDescriptorRef(operandStr)
            if (!ref) throw new Error(`Invalid method/field reference: ${operandStr}`)
            operand = { type: 'methodref', ...ref }
        }
    }

    return { type: 'instruction', opcode, operand }
}

function assembleFile(inputPath, outputDir) {
    const source = readFileSync(inputPath, 'utf8')
    const lines = source.split('\n')
    const cf = new ClassFile()
    let currentMethod = null

    for (const rawLine of lines) {
        const parsed = parseLine(rawLine)
        if (!parsed) continue

        switch (parsed.type) {
        case 'class':
            cf.className = parsed.name
            cf.accessFlags = parsed.accessFlags
            break
        case 'super':
            cf.superName = parsed.name
            break
        case 'method':
            currentMethod = {
                accessFlags: parsed.accessFlags,
                name: parsed.name,
                descriptor: parsed.descriptor,
                maxStack: 2,
                maxLocals: 1,
                instructions: [],
            }
            break
        case 'endMethod':
            if (currentMethod) {
                cf.methods.push(currentMethod)
                currentMethod = null
            }
            break
        case 'limit':
            if (currentMethod) {
                if (parsed.kind === 'stack') currentMethod.maxStack = parsed.value
                if (parsed.kind === 'locals') currentMethod.maxLocals = parsed.value
            }
            break
        case 'label':
            if (currentMethod) {
                if (!currentMethod.labels) currentMethod.labels = {}
                currentMethod.labels[parsed.name] = currentMethod.instructions.length
            }
            break
        case 'instruction':
            if (!currentMethod) throw new Error('Instruction outside method: ' + rawLine)
            currentMethod.instructions.push({ opcode: parsed.opcode, operand: parsed.operand })
            break
        }
    }

    for (const method of cf.methods) {
        method.nameIndex = cf.addUtf8(method.name)
        method.descIndex = cf.addUtf8(method.descriptor)

        for (const inst of method.instructions) {
            if (inst.operand && inst.operand.type === 'methodref') {
                inst.operand.index = cf.addMethodref(
                    inst.operand.className,
                    inst.operand.name,
                    inst.operand.descriptor
                )
            }
            if (inst.operand && inst.operand.type === 'string') {
                inst.operand.index = cf.addString(inst.operand.value)
            }
            if (inst.operand && inst.operand.type === 'int') {
                inst.operand.index = cf.addInteger(inst.operand.value)
            }
        }
    }

    cf.finalize()
    const classBytes = cf.emit()
    const outPath = resolve(outputDir, `${cf.className}.class`)
    mkdirSync(dirname(outPath), { recursive: true })
    writeFileSync(outPath, classBytes)
    console.log(`[JASM] ${inputPath} -> ${cf.className}.class (${classBytes.length} bytes)`)
}

function main() {
    const args = process.argv.slice(2)
    if (args.length === 0) {
        console.log('Usage: node jasm.mjs <input.jasm> <output_dir>')
        console.log('       node jasm.mjs --dir <jasm_dir> <output_dir>')
        process.exit(1)
    }

    if (args[0] === '--dir') {
        const jasmDir = resolve(process.cwd(), args[1])
        const outDir = resolve(process.cwd(), args[2] || 'build')
        const files = readdirSync(jasmDir).filter(f => extname(f) === '.jasm')
        for (const f of files) {
            assembleFile(resolve(jasmDir, f), outDir)
        }
    } else {
        assembleFile(resolve(process.cwd(), args[0]), resolve(process.cwd(), args[1] || 'build'))
    }
}

main()
