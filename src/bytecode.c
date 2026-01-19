/**
 * Logex 字节码实现
 */

#include "bytecode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CODE_SIZE 256
#define INITIAL_CONST_SIZE 64

/* 创建字节码程序 */
BytecodeProgram* bytecode_create(void) {
    BytecodeProgram *prog = (BytecodeProgram*)calloc(1, sizeof(BytecodeProgram));
    if (!prog) return NULL;
    
    /* 初始化文件头 */
    prog->header.magic = LOGEX_MAGIC;
    prog->header.version = LOGEX_VERSION;
    prog->header.const_count = 0;
    prog->header.code_size = 0;
    prog->header.entry_point = 0;
    prog->header.flags = 0;
    
    /* 分配常量池 */
    prog->const_pool = (Constant*)calloc(INITIAL_CONST_SIZE, sizeof(Constant));
    if (!prog->const_pool) {
        free(prog);
        return NULL;
    }
    
    /* 分配代码段 */
    prog->code = (Instruction*)calloc(INITIAL_CODE_SIZE, sizeof(Instruction));
    if (!prog->code) {
        free(prog->const_pool);
        free(prog);
        return NULL;
    }
    
    return prog;
}

/* 销毁字节码程序 */
void bytecode_destroy(BytecodeProgram *prog) {
    if (!prog) return;
    
    /* 释放常量池 */
    for (uint32_t i = 0; i < prog->header.const_count; i++) {
        if (prog->const_pool[i].type == CONST_STRING || 
            prog->const_pool[i].type == CONST_IDENTIFIER) {
            free(prog->const_pool[i].value.str);
        } else if (prog->const_pool[i].type == CONST_NUMBER) {
            free(prog->const_pool[i].value.num.digits);
        }
    }
    free(prog->const_pool);
    
    /* 释放代码段 */
    free(prog->code);
    
    free(prog);
}

/* 添加数字常量 */
uint32_t bytecode_add_const_number(BytecodeProgram *prog, const char *digits, int decimal_pos, int is_negative) {
    if (!prog || !digits) return 0;
    
    uint32_t idx = prog->header.const_count++;
    prog->const_pool[idx].type = CONST_NUMBER;
    prog->const_pool[idx].value.num.digits = strdup(digits);
    prog->const_pool[idx].value.num.decimal_pos = decimal_pos;
    prog->const_pool[idx].value.num.is_negative = is_negative;
    
    return idx;
}

/* 添加字符串常量 */
uint32_t bytecode_add_const_string(BytecodeProgram *prog, const char *str) {
    if (!prog || !str) return 0;
    
    uint32_t idx = prog->header.const_count++;
    prog->const_pool[idx].type = CONST_STRING;
    prog->const_pool[idx].value.str = strdup(str);
    
    return idx;
}

/* 添加标识符常量 */
uint32_t bytecode_add_const_identifier(BytecodeProgram *prog, const char *id) {
    if (!prog || !id) return 0;
    
    uint32_t idx = prog->header.const_count++;
    prog->const_pool[idx].type = CONST_IDENTIFIER;
    prog->const_pool[idx].value.str = strdup(id);
    
    return idx;
}

/* 发射指令（无操作数） */
void bytecode_emit(BytecodeProgram *prog, OpCode opcode) {
    if (!prog) return;
    
    uint32_t idx = prog->header.code_size++;
    prog->code[idx].opcode = opcode;
    prog->code[idx].operand.i64 = 0;
}

/* 发射指令（整数操作数） */
void bytecode_emit_i64(BytecodeProgram *prog, OpCode opcode, int64_t operand) {
    if (!prog) return;
    
    uint32_t idx = prog->header.code_size++;
    prog->code[idx].opcode = opcode;
    prog->code[idx].operand.i64 = operand;
}

/* 发射指令（无符号整数操作数） */
void bytecode_emit_u32(BytecodeProgram *prog, OpCode opcode, uint32_t operand) {
    if (!prog) return;
    
    uint32_t idx = prog->header.code_size++;
    prog->code[idx].opcode = opcode;
    prog->code[idx].operand.u32 = operand;
}

/* 发射指令（引用操作数） */
void bytecode_emit_ref(BytecodeProgram *prog, OpCode opcode, uint32_t idx, uint32_t len) {
    if (!prog) return;
    
    uint32_t pos = prog->header.code_size++;
    prog->code[pos].opcode = opcode;
    prog->code[pos].operand.ref.idx = idx;
    prog->code[pos].operand.ref.len = len;
}

/* 获取当前代码位置 */
uint32_t bytecode_current_pos(BytecodeProgram *prog) {
    return prog ? prog->header.code_size : 0;
}

/* 修补跳转指令 */
void bytecode_patch_jump(BytecodeProgram *prog, uint32_t pos, uint32_t target) {
    if (!prog || pos >= prog->header.code_size) return;
    prog->code[pos].operand.u32 = target;
}

/* 保存字节码到文件 */
int bytecode_save(BytecodeProgram *prog, const char *filename) {
    if (!prog || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* 写入文件头 */
    fwrite(&prog->header, sizeof(BytecodeHeader), 1, fp);
    
    /* 写入常量池 */
    for (uint32_t i = 0; i < prog->header.const_count; i++) {
        Constant *c = &prog->const_pool[i];
        fwrite(&c->type, sizeof(ConstType), 1, fp);
        
        if (c->type == CONST_NUMBER) {
            uint32_t len = strlen(c->value.num.digits);
            fwrite(&len, sizeof(uint32_t), 1, fp);
            fwrite(c->value.num.digits, 1, len, fp);
            fwrite(&c->value.num.decimal_pos, sizeof(int), 1, fp);
            fwrite(&c->value.num.is_negative, sizeof(int), 1, fp);
        } else {
            uint32_t len = strlen(c->value.str);
            fwrite(&len, sizeof(uint32_t), 1, fp);
            fwrite(c->value.str, 1, len, fp);
        }
    }
    
    /* 写入代码段 */
    fwrite(prog->code, sizeof(Instruction), prog->header.code_size, fp);
    
    fclose(fp);
    return 0;
}

/* 从文件加载字节码 */
BytecodeProgram* bytecode_load(const char *filename) {
    if (!filename) return NULL;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;
    
    BytecodeProgram *prog = (BytecodeProgram*)calloc(1, sizeof(BytecodeProgram));
    if (!prog) {
        fclose(fp);
        return NULL;
    }
    
    /* 读取文件头 */
    fread(&prog->header, sizeof(BytecodeHeader), 1, fp);
    
    /* 验证魔数 */
    if (prog->header.magic != LOGEX_MAGIC) {
        free(prog);
        fclose(fp);
        return NULL;
    }
    
    /* 读取常量池 */
    prog->const_pool = (Constant*)calloc(prog->header.const_count, sizeof(Constant));
    for (uint32_t i = 0; i < prog->header.const_count; i++) {
        Constant *c = &prog->const_pool[i];
        fread(&c->type, sizeof(ConstType), 1, fp);
        
        if (c->type == CONST_NUMBER) {
            uint32_t len;
            fread(&len, sizeof(uint32_t), 1, fp);
            c->value.num.digits = (char*)malloc(len + 1);
            fread(c->value.num.digits, 1, len, fp);
            c->value.num.digits[len] = '\0';
            fread(&c->value.num.decimal_pos, sizeof(int), 1, fp);
            fread(&c->value.num.is_negative, sizeof(int), 1, fp);
        } else {
            uint32_t len;
            fread(&len, sizeof(uint32_t), 1, fp);
            c->value.str = (char*)malloc(len + 1);
            fread(c->value.str, 1, len, fp);
            c->value.str[len] = '\0';
        }
    }
    
    /* 读取代码段 */
    prog->code = (Instruction*)calloc(prog->header.code_size, sizeof(Instruction));
    fread(prog->code, sizeof(Instruction), prog->header.code_size, fp);
    
    fclose(fp);
    return prog;
}

/* 反汇编字节码（调试用） */
void bytecode_disassemble(BytecodeProgram *prog) {
    if (!prog) return;
    
    printf("=== Logex Bytecode Disassembly ===\n");
    printf("Magic: 0x%08X\n", prog->header.magic);
    printf("Version: %u\n", prog->header.version);
    printf("Source: %s\n", prog->header.source_file);
    printf("Constants: %u\n", prog->header.const_count);
    printf("Code Size: %u\n\n", prog->header.code_size);
    
    /* 打印常量池 */
    printf("=== Constant Pool ===\n");
    for (uint32_t i = 0; i < prog->header.const_count; i++) {
        printf("[%u] ", i);
        Constant *c = &prog->const_pool[i];
        if (c->type == CONST_NUMBER) {
            printf("NUMBER: %s (decimal_pos=%d, negative=%d)\n", 
                   c->value.num.digits, c->value.num.decimal_pos, c->value.num.is_negative);
        } else if (c->type == CONST_STRING) {
            printf("STRING: \"%s\"\n", c->value.str);
        } else {
            printf("IDENTIFIER: %s\n", c->value.str);
        }
    }
    
    /* 打印代码段 */
    printf("\n=== Code ===\n");
    for (uint32_t i = 0; i < prog->header.code_size; i++) {
        Instruction *inst = &prog->code[i];
        printf("%04u: ", i);
        
        switch (inst->opcode) {
            case OP_NOP: printf("NOP\n"); break;
            case OP_PUSH_NUM: printf("PUSH_NUM [%u]\n", inst->operand.u32); break;
            case OP_PUSH_STR: printf("PUSH_STR [%u]\n", inst->operand.u32); break;
            case OP_PUSH_VAR: printf("PUSH_VAR [%u]\n", inst->operand.u32); break;
            case OP_POP: printf("POP\n"); break;
            case OP_ADD: printf("ADD\n"); break;
            case OP_SUB: printf("SUB\n"); break;
            case OP_MUL: printf("MUL\n"); break;
            case OP_DIV: printf("DIV\n"); break;
            case OP_STORE_VAR: printf("STORE_VAR [%u]\n", inst->operand.u32); break;
            case OP_LOAD_VAR: printf("LOAD_VAR [%u]\n", inst->operand.u32); break;
            case OP_JMP: printf("JMP %u\n", inst->operand.u32); break;
            case OP_JMP_IF_FALSE: printf("JMP_IF_FALSE %u\n", inst->operand.u32); break;
            case OP_CALL_BUILTIN: printf("CALL_BUILTIN [%u] argc=%u\n", 
                                         inst->operand.ref.idx, inst->operand.ref.len); break;
            case OP_HALT: printf("HALT\n"); break;
            default: printf("UNKNOWN(%d)\n", inst->opcode); break;
        }
    }
}
