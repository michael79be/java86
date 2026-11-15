#include <stdint.h>

int opcode_math(Frame *frame, unsigned char opcode) {
    uint32_t right;
    uint32_t left;
    uint32_t result;

    if (!frame_pop(frame, &right)) return 0;
    if (0x74 == opcode) {
        return frame_push(frame, (uint32_t)0 - right);
    }
    if (!frame_pop(frame, &left)) return 0;
    switch (opcode) {
    case 0x60: result = left + right; break;
    case 0x64: result = left - right; break;
    case 0x68: result = left * right; break;
    case 0x6c:
    case 0x70:
        if (0 == right) {
            sys_error("Division by zero\r\n");
            return 0;
        }
        if (0x80000000UL == left && 0xffffffffUL == right) {
            result = (0x6c == opcode) ? left : 0;
        } else if (0x6c == opcode) {
            result = (uint32_t)((int32_t)left / (int32_t)right);
        } else {
            result = (uint32_t)((int32_t)left % (int32_t)right);
        }
        break;
    case 0x78: result = left << (right & 31); break;
    case 0x7a: result = (uint32_t)((int32_t)left >> (right & 31)); break;
    case 0x7c: result = left >> (right & 31); break;
    case 0x7e: result = left & right; break;
    case 0x80: result = left | right; break;
    default: result = left ^ right; break;
    }
    return frame_push(frame, result);
}
