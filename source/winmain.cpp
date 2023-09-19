#include <stdio.h>

#include "windows.h"
#include "helpful.h"
#include "data.h"

// Single unit compilation
#include "decode.cpp"

//TODO(surein): clean up string handling ensuring no memory leaks
//TODO(surein): complete disassembler sufficiently to complete listing 42

void write_entire_file(Bytes bytes, const char* file_path)
{
    FILE* file = fopen(file_path, "wb");
    Assert(file);
    u64 written = fwrite(bytes.buffer, 1, bytes.size, file);
    Assert(written == bytes.size);
    fclose(file);
}

Bytes read_entire_file(const char* file_path)
{
    Bytes bytes;
    
    FILE* file = fopen(file_path, "rb");
    fseek(file, 0, SEEK_END);
    
    bytes.size = ftell(file);
    bytes.buffer = (u8*)malloc(bytes.size);
    
    fseek(file, 0, SEEK_SET);
    fread(bytes.buffer, 1, bytes.size, file);
    fclose(file);
    
    return bytes;
}

Bytes append_chars(Bytes bytes, const char* chars)
{
    u8* old_buffer = bytes.buffer;
    s32 old_size = bytes.size;
    s32 length = (s32)strlen(chars);
    bytes.size = bytes.size + length;
    bytes.buffer = (u8*)malloc(bytes.size);
    if (old_buffer) {
        memcpy(bytes.buffer, old_buffer, old_size);
        free(old_buffer);
    }
    memcpy(bytes.buffer+old_size, (u8*)chars, length);

    return bytes;
}

char* get_label_line(s32 label_num) {
    char* label_str = (char*)malloc(100*sizeof(char*));
    sprintf(label_str, "label_%d:\n", label_num);
    return label_str;
}

s32 get_next_label_index(s32 total_bytes, s32 label_count, s32* label_indices) {
    int next = -1;
    int min_dist = INT_MAX;
    for (int i = 0; i < label_count; ++i) {
        if (label_indices[i] > total_bytes && (label_indices[i]-total_bytes) < min_dist) {
            next = i;
            min_dist = label_indices[i]-total_bytes;
        }
    }
    return next;
}

// Get instruction line string given substrings. Returns malloced ptr.
char* instruction_line(const char* instruction, const char* operand0, const char* operand1, char* log_str) {
    const char* space_str = " ";
    const char* mid_str = ", ";
    const char* end_str = "\n";
    const char* separate_str = " ; ";
    u64 len = strlen(instruction)+strlen(space_str)+strlen(mid_str)+strlen(end_str)+strlen(operand0)+strlen(operand1) + 1;
    if (log_str) {
        len += strlen(log_str) + strlen(separate_str);
    }
    char* instruction_str = (char*)malloc(sizeof(char)*(len));
    strcpy(instruction_str, instruction);
    strcat(instruction_str, space_str);
    strcat(instruction_str, operand0);
    strcat(instruction_str, mid_str);
    strcat(instruction_str, operand1);
    if (log_str) {
        strcat(instruction_str, separate_str);
        strcat(instruction_str, log_str);
    }
    strcat(instruction_str, end_str);
    return instruction_str;
}

// Get instruction line string given substrings for a conditional jump. Returns malloced ptr.
char* instruction_line_jump(const char* instruction, s8 offset, s32 label, char* log_str) {
    char offset_str[20];
    sprintf(offset_str, "%d", offset);
    char label_str[100];
    strcpy(label_str, "label_");
    sprintf(label_str + strlen(label_str), "%d", label);
    const char* space_str = " ";
    const char* mid_str = " ; ";
    const char* end_str = "\n";
    u64 len = strlen(instruction)+strlen(space_str)+strlen(end_str)+strlen(mid_str) + strlen(label_str) + strlen(offset_str)+1;
    if (log_str) {
        len += strlen(log_str) + strlen(mid_str);
    }
    char* instruction_str = (char*)malloc(sizeof(char)*(len));
    strcpy(instruction_str, instruction);
    strcat(instruction_str, space_str);
    strcat(instruction_str, label_str);
    strcat(instruction_str, mid_str); 
    strcat(instruction_str, offset_str);
    if (log_str) {
        strcat(instruction_str, mid_str);
        strcat(instruction_str, log_str);
    }
    strcat(instruction_str, end_str);
    return instruction_str;
}

const char* instruction_str(InstrType type) {
    switch (type) {
        case InstrType::MOV: return "mov";
        case InstrType::ADD: return "add";
        case InstrType::SUB: return "sub";
        case InstrType::CMP: return "cmp";
        case InstrType::JE: return "je";
        case InstrType::JL: return "jl";
        case InstrType::JLE: return "jle";
        case InstrType::JB: return "jb";
        case InstrType::JBE: return "jbe";
        case InstrType::JP: return "jp";
        case InstrType::JO: return "jo";
        case InstrType::JS: return "js";
        case InstrType::JNE: return "jne";
        case InstrType::JNL: return "jnl";
        case InstrType::JG: return "jg";
        case InstrType::JNB: return "jnb";
        case InstrType::JA: return "ja";
        case InstrType::JNP: return "jnp";
        case InstrType::JNO: return "jno";
        case InstrType::JNS: return "jns";
        case InstrType::LOOP: return "loop";
        case InstrType::LOOPZ: return "loopz";
        case InstrType::LOOPNZ: return "loopnz";
        case InstrType::JCXZ: return "jcxz";
        case InstrType::NONE:
        case InstrType::COUNT:
            break;
    }
    Assert(!"Invalid instruction.");
    return "";
}

const char* reg_str(Register reg) {
    switch (reg) {
        case Register::AX: return "AX";
        case Register::AL: return "AL";
        case Register::AH: return "AH";
        case Register::BX: return "BX";
        case Register::BL: return "BL";
        case Register::BH: return "BH";
        case Register::CX: return "CX";
        case Register::CL: return "CL";
        case Register::CH: return "CH";
        case Register::DX: return "DX";
        case Register::DL: return "DL";
        case Register::DH: return "DH";
        case Register::SP: return "SP";
        case Register::BP: return "BP";
        case Register::SI: return "SI";
        case Register::DI: return "DI";
        case Register::ES: return "ES";
        case Register::CS: return "CS";
        case Register::SS: return "SS";
        case Register::DS: return "DS";
        case Register::IP: return "IP";
        case Register::FLAGS: return "FLAGS";
        case Register::NONE:
        case Register::COUNT:
            break;
    }
    Assert(!"Invalid register.");
    return "";
}

const char* memory_address_str(MemoryAddress address) {
    switch (address) {
        case MemoryAddress::BX_SI: return "BX + SI";
        case MemoryAddress::BX_DI: return "BX + DI";
        case MemoryAddress::BP_SI: return "BP + SI";
        case MemoryAddress::BP_DI: return "BP + DI";
        case MemoryAddress::SI: return "SI";
        case MemoryAddress::DI: return "DI";
        case MemoryAddress::BP: return "BP";
        case MemoryAddress::BX: return "BX";
        case MemoryAddress::DIRECT:
        case MemoryAddress::NONE:
        case MemoryAddress::COUNT:
            break;
    }
    Assert(!"Invalid memory address.");
    return "";
}

// Returns malloced string for operand
char* operand_str(Operand operand) {
    switch (operand.type) {
        case OperandType::REGISTER:
            {
                const char* reg_string = reg_str(operand.reg);
                char* reg_string_malloc = (char*)malloc((strlen(reg_string)+1)*sizeof(char));
                strcpy(reg_string_malloc, reg_string);
                return reg_string_malloc;
            }
        case OperandType::MEMORY:
            {
                char* mem_str = (char*)malloc(100*sizeof(char));
                strcpy(mem_str, "[");
                if (operand.address == MemoryAddress::DIRECT) {
                    char* cur = mem_str + strlen(mem_str);
                    sprintf(cur, "%d", operand.displacement);
                } else {
                    const char* memory_address_string = memory_address_str(operand.address);
                    strcat(mem_str, memory_address_string);
                    char* cur = mem_str + strlen(mem_str);
                    if (operand.displacement > 0) {
                        sprintf(cur, " + %d", operand.displacement);
                    } else if (operand.displacement < 0) {
                        sprintf(cur, " - %d", -operand.displacement);
                    }
                }
                strcat(mem_str, "]");
                return mem_str;
            }
        case OperandType::IMMEDIATE:
            {
                char* immediate_str = (char*)malloc(100*sizeof(char));
                if (operand.byte) {
                    sprintf(immediate_str, "byte %d", operand.immediate);
                } else {
                    sprintf(immediate_str, "word %d", operand.immediate);
                }
                return immediate_str;
            }
        case OperandType::JUMP_OFFSET:
            // Note: jump offset operand cannot be used with a second operand. We expect caller to handle outputting this. Assert for now. We could improve this later.
        case OperandType::COUNT:
        case OperandType::NONE:
            break;
    }
    Assert(!"Invalid operand type");
    return 0;
}

Bytes write_instruction_line(Bytes bytes, Instruction instruction, char* log_str) {
    char* str = 0;
    switch (instruction.type) {
        case InstrType::MOV:
        case InstrType::ADD:
        case InstrType::SUB:
        case InstrType::CMP: 
            {
                char* dest_operand = operand_str(instruction.operands[0]);
                char* source_operand = operand_str(instruction.operands[1]);
                str = instruction_line(instruction_str(instruction.type), dest_operand, source_operand, log_str);
                free(dest_operand);
                free(source_operand);
                break;
            }
        case InstrType::JE:
        case InstrType::JL:
        case InstrType::JLE:
        case InstrType::JB:
        case InstrType::JBE:
        case InstrType::JP:
        case InstrType::JO:
        case InstrType::JS:
        case InstrType::JNE:
        case InstrType::JNL:
        case InstrType::JG:
        case InstrType::JNB:
        case InstrType::JA:
        case InstrType::JNP:
        case InstrType::JNO:
        case InstrType::JNS:
        case InstrType::LOOP:
        case InstrType::LOOPZ:
        case InstrType::LOOPNZ:
        case InstrType::JCXZ: 
            {
                Assert(instruction.operands[0].type == OperandType::JUMP_OFFSET);
                str = instruction_line_jump(instruction_str(instruction.type), instruction.operands[0].offset, instruction.operands[0].label, log_str);
                break;
            }
        case InstrType::NONE:
        case InstrType::COUNT:
            Assert(!"Invalid instruction.");
            break;
    }

    if (!str) {
        Assert(!"Failed to generate instruction line string.");
        return bytes;
    }

    Bytes out = append_chars(bytes, str);
    free(str);
    return out;
}

u8* get_register(Register reg, Context* context, u16* bytes) {
    switch (reg) {
        case Register::AX:
            *bytes = 2;
            return (u8*)&context->ax;
        case Register::AL:
            *bytes = 1;
            return (u8*)&context->ax;
        case Register::AH:
            *bytes = 1;
            return (u8*)&context->ax+1;
        case Register::BX:
            *bytes = 2;
            return (u8*)&context->bx;
        case Register::BL:
            *bytes = 1;
            return (u8*)&context->bx;
        case Register::BH:
            *bytes = 1;
            return (u8*)&context->bx+1;
        case Register::CX:
            *bytes = 2;
            return (u8*)&context->cx;
        case Register::CL:
            *bytes = 1;
            return (u8*)&context->cx;
        case Register::CH:
            *bytes = 1;
            return (u8*)&context->cx+1;
        case Register::DX:
            *bytes = 2;
            return (u8*)&context->dx;
        case Register::DL:
            *bytes = 1;
            return (u8*)&context->dx;
        case Register::DH:
            *bytes = 1;
            return (u8*)&context->dx+1;
        case Register::SP:
            *bytes = 2;
            return (u8*)&context->sp;
        case Register::BP:
            *bytes = 2;
            return (u8*)&context->bp;
        case Register::SI:
            *bytes = 2;
            return (u8*)&context->si;
        case Register::DI:
            *bytes = 2;
            return (u8*)&context->di;
        case Register::ES:
            *bytes = 2;
            return (u8*)&context->es;
        case Register::CS:
            *bytes = 2;
            return (u8*)&context->cs;
        case Register::SS:
            *bytes = 2;
            return (u8*)&context->ss;
        case Register::DS:
            *bytes = 2;
            return (u8*)&context->ds;
        case Register::IP:
            *bytes = 2;
            return (u8*)&context->ip;
        case Register::FLAGS:
            *bytes = 2;
            return (u8*)&context->flags;
        case Register::NONE:
        case Register::COUNT:
            break;
    }
    Assert(!"Invalid register.");
    return 0;
}

char* flags_str(u16 flags) {
    char* flags_string = (char*)malloc(100*sizeof(char));
    flags_string[0] = 0;

    if (flags & CARRY_FLAG) {
        strcat(flags_string, "C");
    }
    if (flags & PARITY_FLAG) {
        strcat(flags_string, "P");
    }
    if (flags & AUX_CARRY_FLAG) {
        strcat(flags_string, "A");
    }
    if (flags & ZERO_FLAG) {
        strcat(flags_string, "Z");
    }
    if (flags & SIGN_FLAG) {
        strcat(flags_string, "S");
    }
    if (flags & TRAP_FLAG) {
        strcat(flags_string, "T");
    }
    if (flags & INTERRUPT_FLAG) {
        strcat(flags_string, "I");
    }
    if (flags & DIRECTION_FLAG) {
        strcat(flags_string, "D");
    }
    if (flags & OVERFLOW_FLAG) {
        strcat(flags_string, "O");
    }
    return flags_string;
}

char* simulate_instruction(Instruction instruction, Context* context) {
    if (instruction.operands[0].type != OperandType::REGISTER) {
        Assert(!"Not implemented yet");
        return 0;
    }

    u16 dest_bytes = 0;
    u16 source_bytes = 0;
    u16 bytes = 0;
    u8 byte1 = 0;
    u8 byte2 = 0;
    u8* dest_reg = get_register(instruction.operands[0].reg, context, &dest_bytes);
    if (instruction.operands[1].type == OperandType::REGISTER) {
        u8* source_reg = get_register(instruction.operands[1].reg, context, &source_bytes);
        if (dest_bytes == 2 && source_bytes == 2) {
            byte1 = source_reg[0];
            byte2 = source_reg[1];
            bytes = 2;
        } else {
            Assert(dest_bytes >= 1 && source_bytes >= 1);
            byte1 = source_reg[0];
            bytes = 1;
        }
    } else if (instruction.operands[1].type == OperandType::IMMEDIATE) {
        s32 immediate = instruction.operands[1].immediate;
        source_bytes = instruction.operands[1].byte ? 1 : 2;
        if (dest_bytes == 2 && source_bytes == 2) {
            byte1 = immediate & 0xFF;
            byte2 = (immediate >> 8) & 0xFF;
            bytes = 2;
        } else {
            Assert(dest_bytes >= 1 && source_bytes >= 1);
            byte1 = immediate & 0xFF;
            bytes = 1;
        }
    } else {
        Assert(!"Not implemented yet");
        return 0;
    }

    u16 before = 0;
    u16 flags_before = context->flags;
    if (bytes == 2) {
        before = dest_reg[0] | dest_reg[1] << 8;
    } else {
        before = dest_reg[0];
    }

    bool set_flags = false;
    u32 result_wide = 0;

    // Update data
    if (instruction.type == InstrType::MOV) {
        dest_reg[0] = byte1;
        if (bytes == 2) {
            dest_reg[1] = byte2;
        }
    } else if (instruction.type == InstrType::ADD) {
        set_flags = true;
        if (bytes == 1) {
            result_wide = dest_reg[0]+byte1;
            dest_reg[0] = (u8)result_wide;
        } else if (bytes == 2) {
            u16* dest_reg_16 = (u16*)dest_reg;
            result_wide = dest_reg_16[0] + (byte1 | byte2 << 8);
            dest_reg_16[0] = (u16)result_wide;
        }
    } else if (instruction.type == InstrType::SUB || instruction.type == InstrType::CMP) {
        set_flags = true;
        if (bytes == 1) {
            result_wide = dest_reg[0] - byte1;
            if (instruction.type == InstrType::SUB) {
                dest_reg[0] = (u8)result_wide;
            }
        } else if (bytes == 2) {
            u16* dest_reg_16 = (u16*)dest_reg;
            result_wide = dest_reg_16[0] - (byte1 | byte2 << 8);
            if (instruction.type == InstrType::SUB) {
                dest_reg_16[0] = (u16)result_wide;
            }
        }
    } else if (instruction.type == InstrType::CMP) {
        // Update no data.
        if (bytes == 1) {
            u8 byte_res = dest_reg[0] - byte1;
        } else if (bytes == 2) {
            u16* dest_reg_16 = (u16*)dest_reg;
            u16 word_res = dest_reg_16[0] - (byte1 | byte2 << 8);
        }
    } else {
        Assert(!"Not implemented yet");
        return 0;
    }

    if (set_flags) {
        u16 flags = 0;
        if (result_wide == 0) {
            flags = flags | ZERO_FLAG;
        }
        if (bytes == 1) {
            if (result_wide & (1 << 7)) {
                flags = flags | SIGN_FLAG;
            }
        } else if (bytes == 2) {
            if (result_wide & (1 << 15)) {
                flags = flags | SIGN_FLAG;
            }
        }
        context->flags = flags;
    }

    u16 after = 0;
    u16 flags_after = context->flags;
    if (bytes == 2) {
        after = dest_reg[0] | dest_reg[1] << 8;
    } else {
        after = dest_reg[0];
    }

    char* log_str = 0;
    if (before != after) {
        log_str = (char*)malloc(200*sizeof(char));
        strcpy(log_str, reg_str(instruction.operands[0].reg));
        sprintf(log_str+strlen(log_str), ":%#x->%#x", before, after);
    }
    if (flags_before != flags_after) {
        if (log_str) {
            strcat(log_str, " ");
        } else {
            log_str = (char*)malloc(200*sizeof(char));
            log_str[0] = 0;
        }
        strcat(log_str, "FLAGS:");
        char* before_flags_str = flags_str(flags_before);
        char* after_flags_str = flags_str(flags_after);
        strcat(log_str, before_flags_str);
        strcat(log_str, "->");
        strcat(log_str, after_flags_str);
        free(before_flags_str);
        free(after_flags_str);
    }

    return log_str;
}

Bytes write_end_context(Bytes bytes, Context* context) {
    char* context_str = (char*)malloc(10000*sizeof(char));
    strcpy(context_str, "\n");
    strcat(context_str, "Final registers:\n");
    u16 cur = context->ax;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      AX: %#x (%d)\n", cur, cur);
    cur = context->bx;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      BX: %#x (%d)\n", context->bx, context->bx);
    cur = context->cx;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      CX: %#x (%d)\n", cur, cur);
    cur = context->dx;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      DX: %#x (%d)\n", cur, cur);
    cur = context->sp;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      SP: %#x (%d)\n", cur, cur);
    cur = context->bp;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      BP: %#x (%d)\n", cur, cur);
    cur = context->si;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      SI: %#x (%d)\n", cur, cur);
    cur = context->di;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      DI: %#x (%d)\n", cur, cur);
    cur = context->es;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      ES: %#x (%d)\n", cur, cur);
    cur = context->cs;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      CS: %#x (%d)\n", cur, cur);
    cur = context->ss;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      SS: %#x (%d)\n", cur, cur);
    cur = context->ds;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      DS: %#x (%d)\n", cur, cur);
    cur = context->ip;
    if (cur != 0) sprintf(context_str+strlen(context_str), "      IP: %#x (%d)\n", cur, cur);
    cur = context->flags;
    if (cur != 0) {
        sprintf(context_str+strlen(context_str), "   FLAGS: ");
        char* flags_string = flags_str(cur);
        strcat(context_str, flags_string);
        strcat(context_str, "\n");
        free(flags_string);
    }


    Bytes out = append_chars(bytes, context_str);
    free(context_str);
    return out;
}

// disassemble ASM, optionally execute it, and output
void process(Bytes asm_file, const char* output_path, bool exec)
{
    Bytes output;
    Context context;
    s32 current = 0;

    // Save number of bytes output for each instruction
    s32* instruction_bytes = (s32*)malloc(asm_file.size*sizeof(s32));

    // Save index of each label found.
    s32* label_indices = (s32*)malloc(MAX_LABELS*sizeof(s32));
    s32 label_count = 0;

    Instruction* instructions = (Instruction*)malloc(MAX_INSTRUCTIONS*sizeof(Instruction));
    int instruction_count = 0;
    
    while (current < asm_file.size) {
        s32 prev = current;

        instructions[instruction_count] = decode_instruction(asm_file, &current, label_indices, &label_count);
        Assert(instruction_count < MAX_INSTRUCTIONS);
        if (current == prev) {
            Assert(!"Decode failed.");
            break;
        }
        instruction_bytes[instruction_count] = current-prev;
        instruction_count++;
    }

    // Output to bytes ready to be output to file.
    const char* preamble = "bits 16\n";
    output = append_chars(output, preamble);

    s32 labels_inserted = 0; // All labels should be inserted. Debugging check.
    s32 total_bytes = 0; // Keep track number of input bytes corresponding to instructions output so far.
    s32 next_label_index = get_next_label_index(total_bytes, label_count, label_indices);
    for (int i = 0; i < instruction_count; ++i) {
        char* log_str = 0;
       if (exec) {
            log_str = simulate_instruction(instructions[i], &context);
        }
        output = write_instruction_line(output, instructions[i], log_str);
        if (log_str) {
            free(log_str);
        }

        total_bytes += instruction_bytes[i];
        // Insert label if required
        if (next_label_index >= 0 && total_bytes == label_indices[next_label_index]) {
            char* label_line = get_label_line(next_label_index+1);
            output = append_chars(output, label_line);
            free(label_line);
            labels_inserted++;
            next_label_index = get_next_label_index(total_bytes, label_count, label_indices);
        }
    }

    if (exec) {
        output = write_end_context(output, &context);
    }

    Assert(labels_inserted == label_count);

    free(instructions);
    free(instruction_bytes);
    free(label_indices);

    write_entire_file(output, output_path);
}

s32 APIENTRY WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPTSTR cmd_line,
                     int show)
{
    Bytes bytes = read_entire_file("../data/listing_0046_add_sub_cmp");
    process(bytes, "../output/listing_0046_add_sub_cmp.asm", true);
    free(bytes.buffer);
    bytes = {};

    bytes = read_entire_file("../data/listing_0047_challenge_flags");
    process(bytes, "../output/listing_0047_challenge_flags.asm", true);
    free(bytes.buffer);
    bytes = {};

    return 0;
}