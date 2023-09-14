#include "windows.h"
#include "helpful.h"

#include <stdio.h>

//TODO(surein): clean up string handling ensuring no memory leaks
//TODO(surein): compelte disassembler sufficiently to complete listing 42

#define MAX_LABELS 1000

struct Bytes {
    u8* buffer = 0; // malloced buffer
    s32 size = 0;
};

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

void decode_memory(u8 rm, char* str, s16 disp) {
    if (rm == 0b00000000) {
        strcpy(str, "[BX + SI");
    } else if (rm == 0b00000001) {
        strcpy(str, "[BX + DI");
    } else if (rm == 0b00000010) {
        strcpy(str, "[BP + SI");
    } else if (rm == 0b00000011) {
        strcpy(str, "[BP + DI");
    } else if (rm == 0b00000100) {
        strcpy(str, "[SI");
    } else if (rm == 0b00000101) {
        strcpy(str, "[DI");
    } else if (rm == 0b00000110) {
        strcpy(str, "[BP");
    } else if (rm == 0b00000111) {
        strcpy(str, "[BX");
    } else {
        Assert(!"Invalid rm code");
        return;
    }

    if (disp > 0) {
        char* cur = str + strlen(str);
        sprintf(cur, " + %d", disp);
    } else if (disp < 0) {
        s32 big_disp = disp;
        char* cur = str + strlen(str);
        sprintf(cur, " - %d", -big_disp);
    }

    strcat(str, "]");
}

const char* decode_register(u8 reg, bool w) 
{
    if (reg == 0b00000000) {
        return w ? "AX" : "AL";
    } else if (reg == 0b00000001) {
        return w ? "CX" : "CL";
    } else if (reg == 0b00000010) {
        return w ? "DX" : "DL";
    } else if (reg == 0b00000011) {
        return w ? "BX" : "BL";
    } else if (reg == 0b00000100) {
        return w ? "SP" : "AH";
    } else if (reg == 0b00000101) {
        return w ? "BP" : "CH";
    } else if (reg == 0b00000110) {
        return w ? "SI" : "DH";
    } else if (reg == 0b00000111) {
        return w ? "DI" : "BH";
    }
    Assert(!"Invalid reg code");
    return "";
}

// dest_str / source_str must be large-enough buffers
void extract_dwmodregrm(Bytes asm_file, int* current, char* dest_str, char* source_str) {
        // We initially read from two bytes for this instruction
        u8 byte1 = asm_file.buffer[(*current)++];
        bool d = !!(byte1 & 0b00000010);
        bool w = !!(byte1 & 0b00000001);

        u8 byte2 = asm_file.buffer[(*current)++];
        u8 mod = byte2 & 0b11000000;
        u8 reg = byte2 & 0b00111000;
        u8 rm = byte2 & 0b00000111;

        u8 register_bits = reg >> 3;


        if (mod == 0b11000000) {
            // Register mode (no displacement)

            if (d) {
                strcpy(dest_str, decode_register(register_bits, w));
                strcpy(source_str, decode_register(rm, w));
            } else {
                strcpy(source_str, decode_register(register_bits, w));
                strcpy(dest_str, decode_register(rm, w));
            }

        } else if (mod == 0b00000000) {
            // Memory mode (no displacement except for direct address)

            char memory_str[100]; // 100 should be large enough
            if (rm == 0b00000110) {
                // Use direct address. Read 2 more bytes for this.
                u8 low = asm_file.buffer[(*current)++];
                u8 high = asm_file.buffer[(*current)++];
                u16 address = low + (high << 8);
                strcpy(memory_str, "[");
                char* num_start = memory_str + strlen(memory_str);
                sprintf(num_start, "%d", address);
                strcat(memory_str, "]");
            } else {
                decode_memory(rm, memory_str, 0);
            }

            if (d) {
                strcpy(dest_str, decode_register(register_bits, w));
                strcpy(source_str, memory_str);
            } else {
                strcpy(source_str, decode_register(register_bits, w));
                strcpy(dest_str,  memory_str);
            }

        } else if (mod == 0b01000000) {
            // Memory mode with 8-bit displacement

            char memory_str[100]; // 100 should be large enough
            u8 low = asm_file.buffer[(*current)++];
            decode_memory(rm, memory_str, (s8)low);
            if (d) {
                strcpy(dest_str, decode_register(register_bits, w));
                strcpy(source_str, memory_str);
            } else {
                strcpy(source_str, decode_register(register_bits, w));
                strcpy(dest_str, memory_str);
            }

        } else {
            // Memory mode with 16-bit displacement
            Assert(mod == 0b10000000);

            char memory_str[100]; // 100 should be large enough
            u8 low = asm_file.buffer[(*current)++];
            u8 high = asm_file.buffer[(*current)++];
            u16 disp = low | (high << 8);
            decode_memory(rm, memory_str, (s16)disp);
            if (d) {
                strcpy(dest_str, decode_register(register_bits, w));
                strcpy(source_str, memory_str);
            } else {
                strcpy(source_str, decode_register(register_bits, w));
                strcpy(dest_str, memory_str);
            }
        }
}

// dest_str / source_str must be large-enough buffers
// s is an optional signed bit. set to false if not relevant
void extract_wmodrm(Bytes asm_file, int* current, bool s, char* dest_str, char* source_str) {
    // Read first byte
    u8 byte1 = asm_file.buffer[(*current)++];
    bool w = !!(byte1 & 0b00000001);

    u8 byte2 = asm_file.buffer[(*current)++];
    u8 mod = byte2 & 0b11000000;
    u8 rm = byte2 & 0b00000111;

    if (mod == 0b11000000) {
        // Register mode (no displacement)

        strcpy(dest_str, decode_register(rm, w));

    } else if (mod == 0b00000000) {
        // Memory mode (no displacement except for direct address)

        char memory_str[100]; // 100 should be large enough
        if (rm == 0b00000110) {
            // Use direct address. Read 2 more bytes for this.
            u8 low = asm_file.buffer[(*current)++];
            u8 high = asm_file.buffer[(*current)++];
            u16 address = low + (high << 8);
            strcpy(memory_str, "[");
            char* num_start = memory_str + strlen(memory_str);
            sprintf(num_start, "%d", address);
            strcat(memory_str, "]");
        } else {
            decode_memory(rm, memory_str, 0);
        }
        strcpy(dest_str, memory_str);

    } else if (mod == 0b01000000) {
        // Memory mode with 8-bit displacement

        char memory_str[100]; // 100 should be large enough
        u8 low = asm_file.buffer[(*current)++];
        decode_memory(rm, memory_str, (s8)low);
        strcpy(dest_str, memory_str);

    } else {
        // Memory mode with 16-bit displacement
        Assert(mod == 0b10000000);

        char memory_str[100]; // 100 should be large enough
        u8 low = asm_file.buffer[(*current)++];
        u8 high = asm_file.buffer[(*current)++];
        u16 disp = low | (high << 8);
        decode_memory(rm, memory_str, (s16)disp);
        strcpy(dest_str, memory_str);
    }

    // Get more bytes for the immediate
    char data_str[100];

    // Read first data byte
    u8 data8 = asm_file.buffer[(*current)++];
    if (w && !s) {
        // There's a second data byte
        u16 data16;
        data16 |= asm_file.buffer[(*current)++] << 8;
        sprintf(data_str, "word %d", data16);
    } else if (w && s) {
        s16 data16 = (s8)data8;
        sprintf(data_str, "word %d", data16);
    } else if (!w && !s) {
        sprintf(data_str, "byte %d", data8);
    } else if (!w && s) {
        sprintf(data_str, "byte %d", (s8)data8);
    }
    strcpy(source_str, data_str);
}

// Get instruction str given substrings. Returns malloced ptr.
char* instruction_line(const char* instruction, const char* dest_str, const char* source_str) {
    const char* mid_str = ", ";
    const char* end_str = "\n";
    char* instruction_str = (char*)malloc(sizeof(char)*(strlen(instruction)+strlen(mid_str)+strlen(end_str)+strlen(dest_str)+strlen(source_str)+1));
    strcpy(instruction_str, instruction);
    strcat(instruction_str, dest_str);
    strcat(instruction_str, mid_str);
    strcat(instruction_str, source_str);
    strcat(instruction_str, end_str);
    return instruction_str;
}

char* instruction_line_offset(const char* instruction, const char* label_str, s8 offset) {
    const char* mid_str = " ; ";
    const char* end_str = "\n";
    char offset_str[20];
    sprintf(offset_str, "%d", offset);
    char* instruction_str = (char*)malloc(sizeof(char)*(strlen(instruction)+strlen(end_str)+strlen(mid_str) + strlen(label_str) + strlen(offset_str)+1));
    strcpy(instruction_str, instruction);
    strcat(instruction_str, label_str);
    strcat(instruction_str, mid_str); 
    strcat(instruction_str, offset_str);
    strcat(instruction_str, end_str);
    return instruction_str;
}

const char* get_subvariant(u8 code) {
    if (code == 0b00000000) {
        return "add ";
    } else if (code == 0b00101000) {
        return "sub ";
    } else if (code == 0b00111000) {
        return "cmp ";
    } else {
        // Not supported yet
        Assert(FALSE);
        return "";
    }
}

void get_label(char* label_str, s32 current, s8 offset, s32* label_indices, s32* label_count) {
    strcpy(label_str, "label_");
    s32 location = current + offset;
    for (s32 i = 0; i < *label_count; ++i) {
        if (label_indices[i] == location) {
            char num_str[20];
            sprintf(num_str, "%d", i+1);
            strcat(label_str, num_str);
            return;
        }
    }
    if (*label_count >= MAX_LABELS) {
        Assert(!"Too many labels.");
        return;
    }
    char num_str[20];
    sprintf(num_str, "%d", *label_count+1);
    strcat(label_str, num_str);
    label_indices[(*label_count)++] = location;
}

// Returns malloced char
char* decode_instruction(Bytes asm_file, s32* current, s32* label_indices, s32* label_count) {

    if ((asm_file.buffer[*current] & 0b11111100) == 0b10001000) {
        // MOV register/memory to/from register

        char dest_str[100];
        char source_str[100];
        extract_dwmodregrm(asm_file, current, dest_str, source_str);
        return instruction_line("mov ", dest_str, source_str);

    } else if ((asm_file.buffer[*current] & 0b11110000) == 0b10110000) {
        // MOV immediate to register

        // Read first byte
        u8 byte1 = asm_file.buffer[(*current)++];
        bool w = !!(byte1 & 0b00001000);
        u8 reg = byte1 & 0b00000111;

        const char* dest_str = decode_register(reg, w);
        char source_str[100];

        // Read first data byte
        u16 data = asm_file.buffer[(*current)++];
        if (w) {
            // There's a second data byte
            data |= asm_file.buffer[(*current)++] << 8;
        }
        sprintf(source_str, "%d", data);

        return instruction_line("mov ", dest_str, source_str);
    } else if ((asm_file.buffer[*current] & 0b11111100) == 0b10100000) {
        // MOV accumulator to memory / memory to accumulator

        // Read first byte
        u8 byte1 = asm_file.buffer[(*current)++];
        bool d = !!(byte1 & 0b00000010);
        bool w = !!(byte1 & 0b00000001);

        u8 addr_lo = asm_file.buffer[(*current)++];
        u8 addr_hi = asm_file.buffer[(*current)++];
        u16 addr = addr_lo | (addr_hi << 8);

        const char* reg_str = w ? "AX" : "AL";

        char memory_str[100];
        strcpy(memory_str, "[");
        char* num_start = memory_str + strlen(memory_str);
        sprintf(num_start, "%d", addr);
        strcat(memory_str, "]");

        const char* dest_str = 0;
        const char* source_str = 0;
        if (d) {
            dest_str = memory_str;
            source_str = reg_str;
        } else {
            dest_str = reg_str;
            source_str = memory_str;
        }

        return instruction_line("mov ", dest_str, source_str);
    } else if ((asm_file.buffer[*current] & 0b11111110) == 0b11000110) {
        // MOV immediate to register / memory

        char dest_str[100];
        char source_str[100];
        extract_wmodrm(asm_file, current, false, dest_str, source_str);
        return instruction_line("mov ", dest_str, source_str);
    } else if ((asm_file.buffer[*current] & 0b11000100) == 0b00000000) {
        // Some subvariant of reg/memory and register to either

        const char* instruction_str = get_subvariant(asm_file.buffer[*current] & 0b00111000);

        char dest_str[100];
        char source_str[100];
        extract_dwmodregrm(asm_file, current, dest_str, source_str);
        return instruction_line(instruction_str, dest_str, source_str);
    } else if  ((asm_file.buffer[*current] & 0b11111100) == 0b10000000) {
        // Some subvariant of immediate to register / memory

        bool s = !!(asm_file.buffer[*current] & 0b00000010);
        const char* instruction_str = get_subvariant(asm_file.buffer[*current+1] & 0b00111000);

        char dest_str[100];
        char source_str[100];
        extract_wmodrm(asm_file, current, s, dest_str, source_str);
        return instruction_line(instruction_str, dest_str, source_str);
    } else if ((asm_file.buffer[*current] & 0b11000110) == 0b00000100) {
        // Some subvariant of immediate to accumulator

        const char* instruction_str = get_subvariant(asm_file.buffer[*current] & 0b00111000);
        bool w = !!(asm_file.buffer[*current] & 0b00000001);
        (*current)++;

        char source_str[100];
        // Read first data byte
        u16 data = asm_file.buffer[(*current)++];
        if (w) {
            // There's a second data byte
            data |= asm_file.buffer[(*current)++] << 8;
        }
        sprintf(source_str, "%d", data);
        const char* dest_str = w ? "AX" : "AL";
        return instruction_line(instruction_str, dest_str, source_str);
    } else if (asm_file.buffer[*current] == 0b01110100) {
        // JE
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("je ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111100) {
        // JL
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jl ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111110) {
        // JLE
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jle ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110010) {
        // JB
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jb ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110110) {
        // JBE
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jbe ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111010) {
        // JP
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jp ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110000) {
        // JO
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jo ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111000) {
        // JS
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("js ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110101) {
        // JNE
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jne ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111101) {
        // JNL
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jnl ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111111) {
        // JG
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jg ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110011) {
        // JNB
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jnb ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110111) {
        // JA
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("ja ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111011) {
        // JNP
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jnp ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01110001) {
        // JNO
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jno ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b01111001) {
        // JNS
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jns ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b11100010) {
        // LOOP
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("loop ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b11100001) {
        // LOOPZ
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("loopz ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b11100000) {
        // LOOPNZ
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("loopnz ", label_str, offset);
    } else if (asm_file.buffer[*current] == 0b11100011) {
        // JCXZ
        (*current)++;
        s8 offset = (s8)asm_file.buffer[(*current)++];
        char label_str[100];
        get_label(label_str, *current, offset, label_indices, label_count);
        return instruction_line_offset("jcxz ", label_str, offset);
    }

    // Not supported yet
    Assert(FALSE);
    return "";
}

Bytes insert_label_at_line(Bytes bytes, s32 label_num, s32 line_num) {
    u8* old_buffer = bytes.buffer;
    s32 old_size = bytes.size;
    char label_str[100];
    sprintf(label_str, "label_%d:\n", label_num);
    s32 length = (s32)strlen(label_str);
    bytes.size = bytes.size + length;
    bytes.buffer = (u8*)malloc(bytes.size);

    s32 insert_point = 0;
    s32 cur_line = 0;
    while (cur_line < line_num) {
        if (old_buffer[insert_point++] == '\n') {
            cur_line++;
        }
    }

    memcpy(bytes.buffer, old_buffer, insert_point);
    memcpy(bytes.buffer+insert_point, (u8*)label_str, length);
    memcpy(bytes.buffer+insert_point+length, old_buffer+insert_point, old_size-insert_point);
    free(old_buffer);

    return bytes;
}

// disassemble ASM and output
void disassemble(Bytes asm_file, const char* output_path)
{
    Bytes output;
    s32 current = 0;

    // Save number of bytes output for each instruction
    s32* instruction_bytes = (s32*)malloc(asm_file.size*sizeof(s32));
    s32 instruction_count = 0;

    // Save index of each label found.
    s32* label_indices = (s32*)malloc(MAX_LABELS*sizeof(s32));
    s32 label_count = 0;

    const char* preamble = "bits 16\n";
    output = append_chars(output, preamble);
    
    while (current < asm_file.size) {
        s32 prev = current;

        char* instruction_str = decode_instruction(asm_file, &current, label_indices, &label_count);
        if (current == prev) {
            Assert(!"Decode failed.");
            break;
        }
        instruction_bytes[instruction_count++] = current-prev;

        output = append_chars(output, instruction_str);
        free(instruction_str);
    }

    // Insert labels
    //TODO(surein): clean this up. Currently depends on stuff like existence of preamble line / empty lines and is slow.
    for (s32 i = 0; i < label_count; ++i) {
        s32 total_bytes = 0;
        bool inserted = false;
        for (s32 j = 0; j < instruction_count; ++j) {
            if (label_indices[i] == total_bytes) {
                // Insert label (i+1) at (j+i+1)th line (+1 from preamble)
                s32 line = j + 1;
                for (int k = 0; k < i; ++k) {
                    if (label_indices[k] < label_indices[i]) {
                        line++; // One line further due to previously inserted labels
                    }
                }
                output = insert_label_at_line(output, i+1, line);
                inserted = true;
                break;
            }
            total_bytes += instruction_bytes[j];
        }
        if (!inserted) {
            Assert(!"Failed to match up labels.");
            return;
        }
    }

    free(instruction_bytes);
    free(label_indices);

    write_entire_file(output, output_path);
}

s32 APIENTRY WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPTSTR cmd_line,
                     int show)
{
    Bytes bytes39 = read_entire_file("../data/listing_0039_more_movs");
    disassemble(bytes39, "../output/listing_0039_out.asm");

    Bytes bytes40 = read_entire_file("../data/listing_0040_challenge_movs");
    disassemble(bytes40, "../output/listing_0040_out.asm");

    Bytes bytes41 = read_entire_file("../data/listing_0041_add_sub_cmp_jnz");
    disassemble(bytes41, "../output/listing_0041_out.asm");

    return 0;
}