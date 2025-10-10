/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arm64.h"
#include "cmdline.h"
#include "config.h"
#include "da.h"
#include "fs.h"
#include "ir.h"
#include "node.h"
#include "process.h"
#include "type.h"

void arm64_add_line(arm64_function_t *f, slice_t line)
{
    if (line.len == 0) {
        sb_append_char(f->sections + f->active, '\n');
        return;
    }
    if (line.items[0] == ';') {
        sb_append_char(f->sections + f->active, '\t');
        sb_append(f->sections + f->active, line);
        sb_append_char(f->sections + f->active, '\n');
        return;
    }
    if ((line.items[0] == '.') || line.items[line.len - 1] == ':') {
        sb_append(f->sections + f->active, line);
        sb_append_cstr(f->sections + f->active, " ; label \n");
        return;
    }

    opt_size_t sp = slice_first_of(line, C(" \t"));
    while (sp.ok) {
        slice_t p = slice_first(line, sp.value);
        line = slice_ltrim(slice_tail(line, sp.value + 1));
        sb_append_char(f->sections + f->active, '\t');
        sb_append(f->sections + f->active, p);
        sp = slice_first_of(line, C(" \t"));
    }
    if (line.len > 0) {
        sb_append_char(f->sections + f->active, '\t');
        sb_append(f->sections + f->active, line);
    }
    sb_append_char(f->sections + f->active, '\n');
}

void arm64_add_instruction(arm64_function_t *f, slice_t mnemonic, char const *param_fmt, ...)
{
    va_list args;
    va_start(args, param_fmt);
    sb_append_char(f->sections + f->active, '\t');
    sb_append(f->sections + f->active, mnemonic);
    sb_append_char(f->sections + f->active, '\t');
    sb_vprintf(f->sections + f->active, param_fmt, args);
    sb_append_char(f->sections + f->active, '\n');
    va_end(args);
}

void arm64_add_instruction_param(arm64_function_t *f, slice_t mnemonic, slice_t param)
{
    arm64_add_text(f, SL " " SL, SLARG(mnemonic), SLARG(param));
}

void arm64_add_simple_instruction(arm64_function_t *f, slice_t mnemonic)
{
    arm64_add_line(f, mnemonic);
}

void arm64_add_vtext(arm64_function_t *f, char const *fmt, va_list args)
{
    static sb_t text_buffer = { 0 };
    sb_vprintf(&text_buffer, fmt, args);

    slice_t text = sb_as_slice(text_buffer);
    if (text.len == 0) {
        return;
    }
    text = slice_trim(text);
    opt_size_t nl = slice_indexof(text, '\n');
    while (nl.ok) {
        slice_t line = slice_trim(slice_first(text, nl.value));
        text = slice_tail(text, nl.value + 1);
        arm64_add_line(f, line);
        nl = slice_indexof(text, '\n');
    }
    if (text.len > 0) {
        arm64_add_line(f, text);
    }
    sb_clear(&text_buffer);
}

void arm64_add_text(arm64_function_t *f, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    arm64_add_vtext(f, fmt, args);
    va_end(args);
}

void arm64_add_label(arm64_function_t *f, slice_t label)
{
    arm64_add_text(f, "\n" SL ":", SLARG(label));
}

void arm64_add_directive_f(arm64_function_t *f, slice_t directive, slice_t args)
{
    arm64_add_text(f, SL "\t" SL, SLARG(directive), SLARG(args));
}

void arm64_add_comment(arm64_function_t *f, slice_t comment)
{
    if (comment.len == 0) {
        return;
    }
    arm64_add_text(f, "\n; " SL "\n", SLARG(comment));
    sb_append_char(f->sections + f->active, '\n');
    opt_size_t nl = slice_indexof(comment, '\n');
    while (nl.ok) {
        slice_t line = slice_trim(slice_first(comment, nl.value));
        comment = slice_tail(comment, nl.value + 1);
        sb_printf(f->sections + f->active, "\t; " SL "\n", SLARG(line));
        nl = slice_indexof(comment, '\n');
    }
}

bool arm64_empty(arm64_function_t *f)
{
    return f->sections[CS_Code].len == 0;
}

bool arm64_has_text(arm64_function_t *f)
{
    return arm64_empty(f);
}

void arm64_analyze(arm64_function_t *f, ir_generator_t *gen, operations_t *operations)
{
    if (f->function.ok) {
        ir_node_t *func = gen->ir_nodes.items + f->function.value;
        for (size_t ix = 0; ix < func->function.parameters.len; ++ix) {
            name_t *name = func->function.parameters.items + ix;
            f->stack_depth += align_at(16, type_size_of(name->type));
            dynarr_append_s(arm64_variable_t, &f->variables, .name = name->name, .depth = f->stack_depth)
        }
    }

    uint64s  depths = { 0 };
    uint64_t depth = f->stack_depth;
    for (size_t ix = 0; ix < operations->len; ++ix) {
        operation_t *op = operations->items + ix;
        switch (op->type) {
        case IRO_ScopeBegin:
            dynarr_append(&depths, depth);
            for (size_t iix = 0; iix < op->ScopeBegin.len; ++iix) {
                name_t *name = op->ScopeBegin.items + iix;
                depth += align_at(16, type_size_of(name->type));
                dynarr_append_s(arm64_variable_t, &f->variables, .name = name->name, .depth = depth)
            }
            f->stack_depth = MAX(f->stack_depth, depth);
            break;
        case IRO_ScopeEnd:
            depth = *dynarr_back(&depths);
            dynarr_pop(&depths);
            break;
        default:
            break;
        }
    }
}

void arm64_emit_return(arm64_function_t *f)
{
    arm64_add_instruction_param(f, C("mov"), C("sp,fp"));
    arm64_add_instruction_param(f, C("ldp"), C("fp,lr,[sp],16"));
    arm64_add_simple_instruction(f, C("ret"));
}

void push_to_stack(arm64_function_t *f, size_t size, int from_reg)
{
    if (size == 0) {
        return;
    }
    int num_regs = words_needed(8, size);
    arm64_add_instruction(f, C("sub"), "sp,sp,%zu", align_at(16, num_regs * 8));

    for (int ix = 0; ix < num_regs; ++ix) {
        if (ix < num_regs - 1) {
            arm64_add_instruction(f, C("stp"), "x%d,x%d,[sp,%d]", from_reg + ix, from_reg + ix + 1, ix * 8);
            ++ix;
        } else {
            arm64_add_instruction(f, C("str"), "x%d,[sp,%d]", from_reg + ix, ix * 8);
        }
    }
}

void pop_from_stack(arm64_function_t *f, size_t size, int to_reg)
{
    if (size == 0) {
        return;
    }
    int num_regs = words_needed(8, size);
    for (int ix = 0; ix < num_regs; ++ix) {
        if (ix < num_regs - 1) {
            arm64_add_instruction(f, C("ldp"), "x%d,x%d,[sp,%d]", to_reg + ix, to_reg + ix + 1, ix * 8);
            ++ix;
        } else {
            arm64_add_instruction(f, C("ldr"), "x%d,[sp,%d]", to_reg + ix, ix * 8);
        }
    }
    arm64_add_instruction(f, C("add"), "sp,sp,%zu", align_at(16, num_regs * 8));
}

int move_into_stack(arm64_function_t *f, size_t size, int from_reg, uint64_t to_pos)
{
    if (size == 0) {
        return 0;
    }
    int num_regs = words_needed(8, size);
    for (int ix = 0; ix < num_regs; ++ix) {
        if (ix < num_regs - 1) {
            arm64_add_instruction(f, C("stp"), "x%d,x%d,[fp,-%llu]", from_reg + ix, from_reg + ix + 1, to_pos - ix * 8);
            ++ix;
        } else {
            arm64_add_instruction(f, C("str"), "x%d,[fp,-%llu]", from_reg + ix, to_pos - ix * 8);
        }
    }
    return from_reg + num_regs;
}

void arm64_skeleton(arm64_function_t *f, ir_generator_t *gen)
{
    f->active = CS_Prolog;
    sb_printf(f->sections + CS_Prolog, SL ":\n_" SL ":\n", SLARG(f->name), SLARG(f->name));
    arm64_add_instruction_param(f, C("stp"), C("fp,lr,[sp,#-16]!"));
    arm64_add_instruction_param(f, C("mov"), C("fp,sp"));
    if (f->stack_depth > 0) {
        arm64_add_instruction(f, C("sub"), "sp,sp,#%llu", f->stack_depth);
    }
    if (f->function.ok) {
        int        reg = 0;
        ir_node_t *func = gen->ir_nodes.items + f->function.value;
        for (size_t ix = 0; ix < func->function.parameters.len; ++ix) {
            name_t *param = func->function.parameters.items + ix;
            int     pos = -1;
            for (size_t iix = 0; iix < f->variables.len; ++iix) {
                if (slice_eq(f->variables.items[iix].name, param->name)) {
                    pos = f->variables.items[iix].depth;
                    break;
                }
            }
            assert(pos >= 0);
            reg = move_into_stack(f, type_size_of(param->type), reg, pos);
        }
    }
    f->active = CS_Epilog;
    arm64_emit_return(f);
    f->active = CS_Code;
}

arm64_register_allocation_t arm64_push_reg_by_type(arm64_function_t *f, nodeptr type)
{
    return arm64_push_reg(f, type_size_of(type));
}

bool check_reg(arm64_function_t *f, int num, int reg)
{
    bool available = true;
    for (int ix = reg; ix < reg + num; ++ix) {
        if (f->regs & (1 << ix)) {
            available = false;
            break;
        }
    }
    if (available) {
        for (int ix = reg; ix < reg + num; ++ix) {
            f->regs |= 1 << ix;
        }
    }
    return available;
};

arm64_register_allocation_t arm64_push_reg(arm64_function_t *f, size_t size)
{
    int                         num = words_needed(8, size);
    arm64_register_allocation_t ret = { .reg = -1, .num_regs = num };

    for (int reg = 9; reg + num < 16; ++reg) {
        if (check_reg(f, num, reg)) {
            ret.reg = reg;
            break;
        }
    }
    if (ret.reg == -1) {
        for (int reg = 22; reg + num < 29; ++reg) {
            if (check_reg(f, num, reg)) {
                ret.reg = reg;
                for (int ix = reg; ix < reg + num; ++ix) {
                    f->save_regs |= 1 << ix;
                }
                break;
            }
        }
    }
    dynarr_append_s(arm64_value_stack_entry_t, &f->stack, .type = VSE_RegisterAllocation, .register_allocation = ret);
    return ret;
}

arm64_register_allocation_t arm64_pop_reg(arm64_function_t *f)
{
    assert(f->stack.len > 0);
    arm64_value_stack_entry_t *e = dynarr_back(&f->stack);
    assert(e->type == VSE_RegisterAllocation);

    arm64_register_allocation_t ret = e->register_allocation;
    if (ret.reg != -1) {
        for (int ix = ret.reg; ix < ret.reg + ret.num_regs; ++ix) {
            f->regs &= ~(uint32_t) (1 << ix);
        }
    }
    dynarr_pop(&f->stack);
    return ret;
}

// Pushes the value currently in x0... to the register stack
void arm64_push_by_type(arm64_function_t *f, nodeptr type)
{
    if (type_kind(type) == TYPK_VoidType) {
        return;
    }
    arm64_push(f, type_size_of(type));
}

void arm64_push(arm64_function_t *f, size_t size)
{
    if (size == 0) {
        return;
    }
    arm64_register_allocation_t dest = arm64_push_reg(f, size);
    if (dest.reg > 0) {
        for (int i = 0; i < dest.num_regs; ++i) {
            arm64_add_instruction(f, C("mov"), "x%d,x%d", dest.reg + i, i);
        }
    } else {
        int num = dest.num_regs;
        while (num > 0) {
            if (num > 1) {
                arm64_add_instruction(f, C("stp"), "x%zu,x%zu,[sp,#16]!", dest.num_regs - num, dest.num_regs - num + 1);
                num -= 2;
            } else {
                arm64_add_instruction(f, C("str"), "x%zu,[sp,#16]!", dest.num_regs - num);
                --num;
            }
        }
    }
}

// Pops the value currently at the top of the value stack to x0..
int arm64_pop_by_type(arm64_function_t *f, nodeptr type, int target)
{
    if (type_kind(type) == TYPK_VoidType) {
        return 0;
    }
    return arm64_pop(f, type_size_of(type), target);
}

int arm64_pop(arm64_function_t *f, size_t size, int target)
{
    if (size == 0) {
        return 0;
    }
    assert(f->stack.len > 0);
    arm64_value_stack_entry_t *e = dynarr_back(&f->stack);
    switch (e->type) {
    case VSE_RegisterAllocation: {
        arm64_register_allocation_t src = arm64_pop_reg(f);
        if (src.reg > 0) {
            for (int i = 0; i < src.num_regs; ++i) {
                arm64_add_instruction(f, C("mov"), "x%d,x%d", target + i, src.reg + i);
            }
        } else {
            int num = src.num_regs;
            while (num > 0) {
                if (num > 1) {
                    arm64_add_instruction(f, C("ldp"), "x%zu,x%zu,[sp],#16", target + src.num_regs - num, target + src.num_regs - num + 1);
                    num -= 2;
                } else {
                    arm64_add_instruction(f, C("str"), "x%zu,[sp],#16", target + src.num_regs - num);
                    --num;
                }
            }
        }
    } break;
    case VSE_VarPointer:
        arm64_deref(f, size, target);
        break;
    }
    return target + words_needed(8, size);
}

// Pops the variable reference off the value stack and moves the value
// into x0...
arm64_var_pointer_t arm64_deref_by_type(arm64_function_t *f, nodeptr type, int target)
{
    if (type_kind(type) == TYPK_VoidType) {
        return 0;
    }
    return arm64_deref(f, type_size_of(type), target);
}

arm64_var_pointer_t arm64_deref(arm64_function_t *f, size_t size, int target)
{
    if (size == 0) {
        return 0;
    }
    assert(f->stack.len > 0);
    arm64_value_stack_entry_t *e = dynarr_back(&f->stack);
    assert(e->type == VSE_VarPointer);
    arm64_var_pointer_t ptr = e->var_pointer;
    arm64_var_pointer_t ret = ptr;
    int                 num_regs = words_needed(8, size);
    int                 num = num_regs;
    while (num > 0) {
        if (num > 1) {
            arm64_add_instruction(f, C("ldp"), "x%d,x%d,[fp,-%llu]", target + num_regs - num, target + num_regs - num + 1, ptr);
            num -= 2;
            ptr += 2;
        } else {
            arm64_add_instruction(f, C("ldr"), "x%d,[fp,-%llu]", target + num_regs - num, ptr);
            --num;
            ++ptr;
        }
    }
    dynarr_pop(&f->stack);
    return ret;
}

// Pops the variable reference off the value stack, then pops an allocation
// and moves x0... into the variable
arm64_var_pointer_t arm64_assign_by_type(arm64_function_t *f, nodeptr type)
{
    if (type_kind(type) == TYPK_VoidType) {
        return 0;
    }
    return arm64_assign(f, type_size_of(type));
}

arm64_var_pointer_t arm64_assign(arm64_function_t *f, size_t size)
{
    if (size == 0) {
        return 0;
    }

    // Pop the variable reference:
    assert(f->stack.len > 0);
    arm64_value_stack_entry_t *e = dynarr_back(&f->stack);
    assert(e->type == VSE_VarPointer);
    arm64_var_pointer_t ptr = e->var_pointer;
    arm64_var_pointer_t ret = ptr;
    dynarr_pop(&f->stack);

    // Pop the top of the value stack into x0...
    arm64_pop(f, size, 0);

    // Move x0... into the variable:
    int num_regs = words_needed(8, size);
    int num = num_regs;
    while (num > 0) {
        if (num > 1) {
            arm64_add_instruction(f, C("stp"), "x%d,x%d,[fp,-%llu]", num_regs - num, num_regs - num + 1, ptr);
            num -= 2;
            ptr += 2;
        } else {
            arm64_add_instruction(f, C("str"), "x%d,[fp,-%llu]", num_regs - num, ptr);
            --num;
            ++ptr;
        }
    }
    dynarr_pop(&f->stack);
    return ret;
}

void arm64_function_write(arm64_function_t *func, FILE *f)
{
    slice_fwrite(sb_as_slice(func->sections[CS_Prolog]), f);
    slice_fwrite(sb_as_slice(func->sections[CS_Code]), f);
    slice_fwrite(sb_as_slice(func->sections[CS_Epilog]), f);
}

void generate_default(arm64_function_t *f, operation_t *op)
{
    (void) f;
    (void) op;
}

void generate_AssignFromRef(arm64_function_t *f, operation_t *op)
{
    // arm64_stack.push_back(arm64_assign(impl.payload));
    arm64_assign_by_type(f, op->AssignFromRef);
    // debug_stack(function, "AssignFromRef");
}

void generate_AssignValue(arm64_function_t *f, operation_t *op)
{
    // arm64_stack.push_back(arm64_assign(impl.payload));
    arm64_assign_by_type(f, op->AssignValue);
    // debug_stack(function, "AssignValue");
}

void generate_BinaryOperator(arm64_function_t *f, operation_t *op)
{
    (void) f;
    (void) op;
    // generate_binop(function, impl.payload.lhs, impl.payload.op, impl.payload.rhs);
}

void generate_Break(arm64_function_t *f, operation_t *op)
{
    if (op->Break.label != op->Break.scope_end) {
        f->save_regs |= 3 << 19; // reg 19 and 20
                                 //        arm64_add_instruction(f, C("mov"), "x19,%llu", op->Break.depth);
                                 //        arm64_add_instruction(f, C("adr"), "x20,lbl_%llu", op->Break.label);
        arm64_add_instruction(f, C("b"), "lbl_%llu", op->Break.scope_end);
    }
}

void generate_CallOp(arm64_function_t *f, call_op_t *call)
{
    arm64_register_allocations_t allocations = { 0 };
    int                          reg = 0;
    for (size_t ix = 0; ix < call->parameters.len; ++ix) {
        name_t *param = call->parameters.items + ix;
        int     num = words_needed(8, type_size_of(param->type));
        dynarr_append_s(arm64_register_allocation_t, &allocations, .reg = reg, .num_regs = num);
        reg += num;
    }
    for (int ix = call->parameters.len - 1; ix >= 0; --ix) {
        name_t                      *param = call->parameters.items + ix;
        arm64_register_allocation_t *dest = allocations.items + ix;
        arm64_pop_by_type(f, param->type, dest->reg);
    }
    slice_t    name = call->name;
    opt_size_t colon = slice_last_indexof(name, ':');
    if (colon.ok) {
        name = slice_tail(name, colon.value + 1);
    }
    arm64_add_instruction(f, C("bl"), "_" SL, SLARG(name));
    arm64_push_by_type(f, call->return_type);
}

void generate_Call(arm64_function_t *f, operation_t *op)
{
    generate_CallOp(f, &op->Call);
}

void generate_DeclVar(arm64_function_t *f, operation_t *op)
{
    (void) f;
    (void) op;
}

void generate_Dereference(arm64_function_t *f, operation_t *op)
{
    arm64_deref_by_type(f, op->Dereference, 0);
    arm64_push_by_type(f, op->Dereference);
}

void generate_Discard(arm64_function_t *f, operation_t *op)
{
    if (type_kind(op->Discard) == TYPK_VoidType) {
        return;
    }
    while (f->stack.len != 0) {
        arm64_register_allocation_t reg = arm64_pop_reg(f);
        if (reg.reg < 0) {
            arm64_add_instruction(f, C("add"), "sp,%ld", align_at(16, reg.num_regs * 8));
        }
    }
}

void generate_Jump(arm64_function_t *f, operation_t *op)
{
    arm64_add_instruction(f, C("b"), "lbl_%llu", op->Jump);
}

void generate_JumpF(arm64_function_t *f, operation_t *op)
{
    arm64_pop_by_type(f, Boolean, 0);
    arm64_add_instruction_param(f, C("mov"), C("x1,xzr"));
    arm64_add_instruction_param(f, C("cmp"), C("x0,x1"));
    arm64_add_instruction(f, C("b.eq"), "lbl_%llu", op->JumpF);
}

void generate_JumpT(arm64_function_t *f, operation_t *op)
{
    arm64_pop_by_type(f, Boolean, 0);
    arm64_add_instruction_param(f, C("mov"), C("x1,xzr"));
    arm64_add_instruction_param(f, C("cmp"), C("x0,x1"));
    arm64_add_instruction(f, C("b.ne"), "lbl_%llu", op->JumpT);
}

void generate_Label(arm64_function_t *f, operation_t *op)
{
    sb_printf(f->sections + f->active, "lbl_%llu:\n", op->Label);
}

void generate_NativeCall(arm64_function_t *f, operation_t *op)
{
    generate_CallOp(f, &op->NativeCall);
}

void generate_Pop(arm64_function_t *f, operation_t *op)
{
    f->save_regs |= 1 << 21;
    arm64_pop_by_type(f, op->Pop, 0);
    arm64_add_instruction_param(f, C("mov"), C("x21,x0"));
    while (f->stack.len > 0) {
        arm64_register_allocation_t reg = arm64_pop_reg(f);
        if (reg.reg < 0) {
            arm64_add_instruction(f, C("add"), "sp,%ld", align_at(16, reg.num_regs * 8));
        }
    }
}

void generate_PushConstant(arm64_function_t *f, operation_t *op)
{
    value_t *v = &op->PushConstant;
    type_t  *t = get_type(v->type);
    switch (t->kind) {
    case TYPK_VoidType:
        return;
    case TYPK_IntType: {
        arm64_register_allocation_t reg = arm64_push_reg_by_type(f, v->type);
        int                         r = (reg.reg > 0) ? reg.reg : 0;
        switch (t->int_type.width_bits) {
        case 8:
            arm64_add_instruction(f, C("mov"), "w%d,#%d", r, (t->int_type.is_signed) ? (int) v->i8 : (int) v->u8);
            break;
        case 16:
            arm64_add_instruction(f, C("mov"), "w%d,#%d", r, (t->int_type.is_signed) ? (int) v->i16 : (int) v->u16);
            break;
        case 32:
            arm64_add_instruction(f, C("mov"), "w%d,#%lld", r, (t->int_type.is_signed) ? (int64_t) v->i32 : (int64_t) v->u16);
            break;
        case 64:
            if (t->int_type.is_signed) {
                arm64_add_instruction(f, C("mov"), "x%d,#%lld", r, v->i64);
            } else {
                arm64_add_instruction(f, C("mov"), "x%d,#%llu", r, v->u64);
            }
            break;
        }
        if (reg.reg < 0) {
            arm64_add_instruction(f, C("str"), "x0,[sp,#-16]!");
        }
    } break;
    case TYPK_SliceType: {
        if (t->slice_of.value == U8.value) {
            slice_t                     slice = v->slice;
            int                         str_id = arm64_add_string(f->object, slice);
            arm64_register_allocation_t reg = arm64_push_reg_by_type(f, v->type);
            if (reg.reg > 0) {
                arm64_add_instruction(f, C("adr"), "x%d,str_%d", reg.reg, str_id);
                arm64_add_instruction(f, C("mov"), "x%d,#%zu", reg.reg + 1, slice.len);
            } else {
                arm64_add_instruction(f, C("adr"), "x0,str_%d", str_id);
                arm64_add_instruction(f, C("mov"), "x1,#%zu", slice.len);
                arm64_add_instruction(f, C("stp"), "x%d,x%d,[sp,#-16]!", reg.reg, reg.reg + 1);
            }
        } else {
            arm64_add_comment(f, sb_as_slice(sb_format("PushConstant " SL, SLARG(type_to_string(v->type)))));
        }
    } break;
    default:
        arm64_add_comment(f, sb_as_slice(sb_format("PushConstant " SL, SLARG(type_to_string(v->type)))));
        break;
    }
}

void generate_PushValue(arm64_function_t *f, operation_t *op)
{
    (void) f;
    (void) op;
}

void generate_PushVarAddress(arm64_function_t *f, operation_t *op)
{
    for (size_t ix = 0; ix < f->variables.len; ++ix) {
        if (slice_eq(op->PushVarAddress.name, f->variables.items[ix].name)) {
            dynarr_append_s(arm64_value_stack_entry_t, &f->stack, .var_pointer = f->variables.items[ix].depth + op->PushVarAddress.offset, .type = VSE_VarPointer);
        }
    }
    // debug_stack(function, std::format("PushVarAddress {}+{}", as_utf8(impl.payload.name), impl.payload.offset));
}

void generate_ScopeBegin(arm64_function_t *f, operation_t *op)
{
    (void) op;
    (void) f;
    //    f->save_regs |= 3 << 19;
    //    arm64_add_instruction(f, C("mov"), "x19,xzr");
    //    arm64_add_instruction(f, C("mov"), "x20,xzr");
}

void generate_ScopeEnd(arm64_function_t *f, operation_t *op)
{
    if (op->ScopeEnd.has_defers) {
        arm64_add_text(f,
            "cmp x19,xzr\n"
            "b.ne 1f\n"
            "cmp  x20,xzr\n"
            "b.eq 2f\n"
            "br   x20\n"
            "1:\n"
            "sub  x19,x19,1\n"
            "b    lbl_%llu\n"
            "2:",
            op->ScopeEnd.enclosing_end);
    }
}

void generate_UnaryOperator(arm64_function_t *f, operation_t *op)
{
    (void) f;
    (void) op;
    // generate_unary(function, impl.payload.operand, impl.payload.op);
}

void arm64_function_generate(arm64_function_t *f, ir_generator_t *gen, operations_t *operations)
{
    arm64_analyze(f, gen, operations);
    arm64_add_directive_o(f->object, C(".global"), f->name);
    arm64_skeleton(f, gen);
    f->regs = 0;
    f->save_regs = 0;

    for (size_t ix = 0; ix < operations->len; ++ix) {
        operation_t *op = operations->items + ix;
        sb_t         list = { 0 };
        operation_list(&list, op);
        trace("Serializing op #%zu " SL, ix, SLARG(list));
        switch (op->type) {
#undef S
#define S(T, P)              \
    case IRO_##T:            \
        generate_##T(f, op); \
        break;
            IROPERATIONTYPES(S)
        default:
            UNREACHABLE();
        }
    }

    static sb_t pop_saved_regs = { 0 };
    f->active = CS_Prolog;
    for (int ix = 19; ix < 29; ++ix) {
        if (f->save_regs &= 1 << ix) {
            if (f->save_regs &= 1 << (ix + 1)) {
                arm64_add_instruction(f, C("stp"), "x%d,x%d,[sp,-16]!", ix, ix + 1);
                sb_printf(&pop_saved_regs, "ldp x%d,x%d,[sp],16\n", ix, ix + 1);
                ++ix;
            } else {
                arm64_add_instruction(f, C("str"), "x%d,[sp,-16]!", ix);
                sb_printf(&pop_saved_regs, "ldr x%d,[sp],16\n", ix);
            }
        }
    }
    f->active = CS_Code;
    if (pop_saved_regs.len > 0) {
        arm64_add_instruction(f, C("mov"), "x0,x21");
        arm64_add_text(f, SL, SLARG(sb_as_slice(pop_saved_regs)));
    }
}

void arm64_add_data(arm64_object_t *o, slice_t label, bool global, slice_t type, bool is_static, slice_t data)
{
    if (o->sections[CS_Data].len == 0) {
        sb_append_cstr(o->sections + CS_Data, "\n\n.section __DATA,__data\n");
    }
    if (global) {
        sb_append_cstr(o->sections + CS_Data, "\n.global ");
        sb_append(o->sections + CS_Data, label);
    }
    sb_append_cstr(o->sections + CS_Data, "\n.align 8\n");
    sb_append(o->sections + CS_Data, label);
    sb_append_cstr(o->sections + CS_Data, ":\n\t");
    sb_append(o->sections + CS_Data, type);
    sb_append_cstr(o->sections + CS_Data, "\t");
    sb_append(o->sections + CS_Data, data);
    if (is_static) {
        sb_append_cstr(o->sections + CS_Data, "\n\t.short 0");
    }
}

void arm64_add_directive_o(arm64_object_t *o, slice_t directive, slice_t args)
{
    sb_printf(o->sections + CS_Prolog, SL "\t" SL "\n", SLARG(directive), SLARG(args));
    if (slice_eq(directive, C(".global"))) {
        sb_printf(o->sections + CS_Prolog, ".global\t_" SL "\n", SLARG(args));
        o->has_exports = true;
        if (slice_eq(args, C("main"))) {
            o->has_main = true;
        }
    }
}

int arm64_add_string(arm64_object_t *o, slice_t str)
{
    for (size_t ix = 0; ix < o->strings.len; ++ix) {
        if (slice_eq(o->strings.items[ix], str)) {
            return ix;
        }
    }
    uint64_t id = o->strings.len;
    sb_printf(o->sections + CS_Text, ".align 2\nstr_%llu:\n\t.byte\t", id);
    for (size_t ix = 0; ix < str.len; ++ix) {
        char c = str.items[ix];
        if (ix > 0) {
            sb_append_char(o->sections + CS_Text, ',');
        }
        sb_printf(o->sections + CS_Text, "0x%x", c & 0xFF);
    }
    sb_append_char(o->sections + CS_Text, '\n');
    dynarr_append(&o->strings, str);
    return id;
}

void arm64_object_generate(arm64_object_t *o, ir_generator_t *gen)
{
    assert(o->module.ok);
    ir_node_t       *mod = gen->ir_nodes.items + o->module.value;
    arm64_function_t mod_init = {
        .function = nullptr,
        .object = o,
        .name = sb_as_slice(sb_format("_" SL "_init", SLARG(mod->module.name))),
    };
    arm64_function_generate(&mod_init, gen, &mod->module.operations);
    dynarr_append(&o->functions, mod_init);

    for (size_t ix = 0; ix < mod->module.functions.len; ++ix) {
        ir_node_t       *ir_func = gen->ir_nodes.items + mod->module.functions.items[ix].value;
        arm64_function_t arm64_func = {
            .function = mod->module.functions.items[ix],
            .name = ir_func->function.name,
            .object = o,
        };
        arm64_function_generate(&arm64_func, gen, &ir_func->function.operations);
        dynarr_append(&o->functions, arm64_func);
    }
}

void arm64_object_write(arm64_object_t *o, FILE *f)
{
    slice_fwrite(sb_as_slice(o->sections[CS_Prolog]), f);
    for (size_t ix = 0; ix < o->functions.len; ++ix) {
        arm64_function_write(o->functions.items + ix, f);
    }
    fputc('\n', f);
    slice_fwrite(sb_as_slice(o->sections[CS_Text]), f);
    slice_fwrite(sb_as_slice(o->sections[CS_Data]), f);
}

bool arm64_save_and_assemble(arm64_object_t *o, ir_generator_t *gen)
{
    path_t dot_elrond = path_make_relative(".elrond");
    path_t path = path_extend(dot_elrond, o->file_name);
    path_replace_extension(&path, C("s"));
    FILE *f = fopen(path.path.items, "wb+");
    if (f == NULL) {
        return false;
    }
    arm64_object_write(o, f);
    fclose(f);

    if (cmdline_is_set("dump-ir")) {
        path_t ir_path = path_extend(dot_elrond, o->file_name);
        path_replace_extension(&ir_path, C("ir"));
        FILE *f = fopen(ir_path.path.items, "wb+");
        if (f == NULL) {
            return false;
        }
        list(f, gen, o->module);
        fclose(f);
    }

    path_t o_file = path_parse(sb_as_slice(path.path));
    path_replace_extension(&o_file, C("o"));
    if (o->module.ok) {
        ir_node_t *mod = gen->ir_nodes.items + o->module.value;
        if (cmdline_is_set("verbose")) {
            fprintf(stderr, "[ARM64] Assembling `" SL "`\n", SLARG(mod->module.name));
        }
    } else {
        if (cmdline_is_set("verbose")) {
            fprintf(stderr, "[ARM64] Assembling root module\n");
        }
    }

    process_t as = process_create("as", path.path.items, "-o", o_file.path.items);
    as.verbose = cmdline_is_set("verbose");
    process_result_t res = process_execute(&as);
    if (!res.ok) {
        fatal("Assembler execution failed: %s\n", strerror(errno));
    } else if (res.success != 0) {
        fatal("Assembler failed:\n" SL, SLARG(as.out_pipes.pipes[1].text));
    }
    //    if (!cmdline_is_set("keep-assembly")) {
    //        unlink(path.path.items);
    //    }
    return true;
}

bool arm64_executable_generate(arm64_executable_t *exe, ir_generator_t *gen)
{
    path_t dot_elrond = path_make_relative(".elrond");
    mkdir(dot_elrond.path.items, 0777);

    ir_node_t *prog = gen->ir_nodes.items + exe->program.value;

    for (size_t ix = 0; ix < prog->program.modules.len; ++ix) {
        nodeptr        mod_ptr = prog->program.modules.items[ix];
        ir_node_t     *mod = gen->ir_nodes.items + mod_ptr.value;
        arm64_object_t obj = {
            .file_name = mod->module.name,
            .module = mod_ptr,
        };
        // prog_init.add_instruction(f, C("bl"), std::format(L"_{}_init", name));
        arm64_object_generate(&obj, gen);
        dynarr_append(&exe->objects, obj);
    }

    slices_t o_files = { 0 };
    for (size_t ix = 0; ix < exe->objects.len; ++ix) {
        arm64_object_t *obj = exe->objects.items + ix;
        if (obj->has_exports) {
            if (!arm64_save_and_assemble(obj, gen)) {
                return false;
            }
            path_t path = path_extend(dot_elrond, obj->file_name);
            path_replace_extension(&path, C("o"));
            dynarr_append(&o_files, sb_as_slice(path.path))
        }
    }

    if (o_files.len != 0) {
        process_t p = process_create("xcrun", "--sdk", "macosx", "--show-sdk-path");
        p.verbose = cmdline_is_set("verbose");
        process_result_t res = process_execute(&p);
        if (!res.ok) {
            fatal("`xcrun` execution failed: %s\n", strerror(errno));
        } else if (res.success != 0) {
            fatal("xcrun failed:\n" SL, SLARG(p.out_pipes.pipes[1].text));
        }
        sb_t sdk_path = p.out_pipes.pipes[0].text;
        sdk_path.len = slice_rtrim(sb_as_slice(sdk_path)).len;
        sb_append_cstr(&sdk_path, "/usr/lib");
        path_t program_path = path_parse(prog->program.name);
        path_strip_extension(&program_path);
        if (cmdline_is_set("verbose")) {
            printf("[ARM64] Linking `" SL "`\n", SLARG(program_path.path));
            printf("[ARM64] SDK path: `" SL "`\n", SLARG(sdk_path));
        }

        process_t link = process_create(
            "ld",
            "-o",
            program_path.path.items,
            "-L" ELROND_DIR "build",
            "-L",
            sdk_path.items,
            "-lelrstart",
            "-lelrrt",
            "-lSystem",
            "-rpath",
            ELROND_DIR "build",
            "-e",
            "_start",
            "-arch",
            "arm64");

        for (size_t ix = 0; ix < o_files.len; ++ix) {
            dynarr_append(&link.arguments, o_files.items[ix]);
        }
        link.verbose = cmdline_is_set("verbose");

        res = process_execute(&link);
        if (!res.ok) {
            fatal("Linker execution failed: %s\n", strerror(errno));
        } else if (res.success != 0) {
            fatal("Linking failed:\n" SL, SLARG(link.out_pipes.pipes[1].text));
        }
        if (!cmdline_is_set("keep-objects")) {
            for (size_t ix = 0; ix < o_files.len; ++ix) {
                unlink(o_files.items[ix].items);
            }
        }
        process_t install_tool = process_create(
            "install_name_tool",
            "-change",
            "build/libelrrt.dylib",
            "@executable_path/libelrrt.dylib",
            program_path.path.items);
        install_tool.verbose = cmdline_is_set("verbose");
        res = process_execute(&install_tool);
        if (!res.ok) {
            fatal("Install tool execution failed: %s\n", strerror(errno));
        } else if (res.success != 0) {
            fatal("Install tool failed:\n" SL, SLARG(link.out_pipes.pipes[1].text));
        }
    }
    return true;
}

opt_arm64_executable_t arm64_generate(ir_generator_t *gen, nodeptr program)
{
    assert(gen->ir_nodes.items[program.value].type == IRN_Program);
    arm64_executable_t exe = {
        .program = program,
        .objects = { 0 },
    };
    if (arm64_executable_generate(&exe, gen)) {
        return OPTVAL(arm64_executable_t, exe);
    }
    return OPTNULL(arm64_executable_t);
}
