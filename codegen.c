#include "ml.h"

static FILE *out;
static int label_cnt = 0;
static int stack_off = 0;

static Symbol symtab[512];
static int sym_cnt = 0;

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(out, "    ");
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    va_end(args);
}

static int new_label() {
    return label_cnt++;
}

static Symbol *find_sym(const char *name) {
    for (int i = sym_cnt - 1; i >= 0; i--) {
        if (!strcmp(symtab[i].name, name))
            return &symtab[i];
    }
    return NULL;
}

static void add_sym(const char *name, int offset, int is_global, 
                   PrimitiveType ptype, int size, const char *struct_type) {
    symtab[sym_cnt].name = ml_strdup(name);
    symtab[sym_cnt].offset = offset;
    symtab[sym_cnt].is_global = is_global;
    symtab[sym_cnt].ptype = ptype;
    symtab[sym_cnt].size = size;
    symtab[sym_cnt].struct_type = struct_type ? ml_strdup(struct_type) : NULL;
    symtab[sym_cnt].is_const = 0;
    symtab[sym_cnt].is_float_const = 0;
    sym_cnt++;
}

static void add_const_sym(const char *name, int64_t int_val, double float_val, 
                         int is_float, PrimitiveType ptype) {
    symtab[sym_cnt].name = ml_strdup(name);
    symtab[sym_cnt].is_const = 1;
    symtab[sym_cnt].const_val = int_val;
    symtab[sym_cnt].const_fval = float_val;
    symtab[sym_cnt].is_float_const = is_float;
    symtab[sym_cnt].ptype = ptype;
    sym_cnt++;
}

static const char *reg_for_type(PrimitiveType t, const char *base) {
    if (!strcmp(base, "rax")) {
        switch (t) {
            case TYPE_I8:
            case TYPE_U8:
            case TYPE_BOOL:
            case TYPE_CHAR:
                return "al";
            case TYPE_I16:
            case TYPE_U16:
                return "ax";
            case TYPE_I32:
            case TYPE_U32:
                return "eax";
            default:
                return "rax";
        }
    } else if (!strcmp(base, "rdi")) {
        switch (t) {
            case TYPE_I8:
            case TYPE_U8:
            case TYPE_BOOL:
            case TYPE_CHAR:
                return "dil";
            case TYPE_I16:
            case TYPE_U16:
                return "di";
            case TYPE_I32:
            case TYPE_U32:
                return "edi";
            default:
                return "rdi";
        }
    }
    return base;
}

static void gen_expr(Node *n);

static void gen_lval(Node *n) {
    if (n->type == NODE_VAR) {
        Symbol *s = find_sym(n->var.name);
        if (!s) {
            fprintf(stderr, "Error línea %d: variable no definida '%s'\n", 
                    n->line, n->var.name);
            exit(1);
        }
        
        if (s->is_global) {
            emit("lea rax, [rel %s]", s->name);
        } else {
            emit("lea rax, [rbp%+d]", s->offset);
        }
    } 
    else if (n->type == NODE_INDEX) {
        gen_lval(n->index.arr);
        emit("push rax");
        gen_expr(n->index.idx);
        emit("pop rdi");
        
        /* Calcular tamaño del elemento */
        Symbol *s = NULL;
        if (n->index.arr->type == NODE_VAR) {
            s = find_sym(n->index.arr->var.name);
        } else if (n->index.arr->type == NODE_MEMBER) {
            Node *obj = n->index.arr->member.obj;
            if (obj->type == NODE_VAR) {
                Symbol *os = find_sym(obj->var.name);
                if (os && os->struct_type) {
                    StructField *fld = struct_get_field(os->struct_type, 
                                                        n->index.arr->member.field);
                    if (fld && fld->is_struct) {
                        /* Crear símbolo temporal */
                        s = (Symbol*)malloc(sizeof(Symbol));
                        s->struct_type = fld->struct_name;
                        s->ptype = TYPE_PTR;
                    } else if (fld) {
                        s = (Symbol*)malloc(sizeof(Symbol));
                        s->struct_type = NULL;
                        s->ptype = fld->ptype;
                    }
                }
            }
        }
        
        int elem_size;
        if (s) {
            if (s->struct_type) {
                elem_size = struct_size(s->struct_type);
            } else {
                elem_size = type_size(s->ptype);
            }
        } else {
            elem_size = 8; /* default */
        }
        
        if (elem_size != 1) {
            emit("imul rax, %d", elem_size);
        }
        emit("add rax, rdi");
    }
    else if (n->type == NODE_MEMBER) {
        gen_lval(n->member.obj);
        
        /* Obtener información del objeto */
        Symbol *s = NULL;
        if (n->member.obj->type == NODE_VAR) {
            s = find_sym(n->member.obj->var.name);
        } else if (n->member.obj->type == NODE_MEMBER) {
            /* Miembro anidado: struct.substruct.field */
            Node *obj = n->member.obj->member.obj;
            if (obj->type == NODE_VAR) {
                Symbol *os = find_sym(obj->var.name);
                if (os && os->struct_type) {
                    StructField *f = struct_get_field(os->struct_type, 
                                                      n->member.obj->member.field);
                    if (f && f->is_struct) {
                        s = (Symbol*)malloc(sizeof(Symbol));
                        s->struct_type = f->struct_name;
                    }
                }
            }
        }
        
        /* Agregar offset del campo */
        if (s && s->struct_type) {
            int offset = struct_field_offset(s->struct_type, n->member.field);
            if (offset >= 0) {
                emit("add rax, %d", offset);
            }
        }
    }
    else if (n->type == NODE_DEREF) {
        gen_expr(n->deref.expr);
    }
}

static void gen_expr(Node *n) {
    switch (n->type) {
        case NODE_NUM:
            emit("mov rax, %ld", n->num.val);
            break;
        
        case NODE_FLOAT: {
            /* Para flotantes, crear constante en .data */
            static int float_cnt = 0;
            fprintf(out, "section .data\n");
            fprintf(out, ".float%d: dq %f\n", float_cnt, n->flt.fval);
            fprintf(out, "section .text\n");
            emit("movsd xmm0, [rel .float%d]", float_cnt);
            float_cnt++;
            break;
        }
        
        case NODE_BOOL:
            emit("mov rax, %d", n->boolean.bval);
            break;
        
        case NODE_CHAR:
            emit("mov rax, %d", (int)n->character.cval);
            break;
        
        case NODE_STR: {
            static int str_cnt = 0;
            fprintf(out, "section .data\n");
            fprintf(out, ".str%d: db ", str_cnt);
            for (int i = 0; i < n->string.len; i++) {
                fprintf(out, "%d", (unsigned char)n->string.sval[i]);
                if (i < n->string.len - 1) fprintf(out, ",");
            }
            fprintf(out, ",0\n");
            fprintf(out, "section .text\n");
            emit("lea rax, [rel .str%d]", str_cnt);
            str_cnt++;
            break;
        }
        
        case NODE_VAR: {
            Symbol *s = find_sym(n->var.name);
            if (!s) {
                fprintf(stderr, "Error línea %d: variable no definida '%s'\n", 
                        n->line, n->var.name);
                exit(1);
            }
            
            /* Verificar si es constante */
            if (s->is_const) {
                if (s->is_float_const) {
                    static int fc = 0;
                    fprintf(out, "section .data\n");
                    fprintf(out, ".fconst%d: dq %f\n", fc, s->const_fval);
                    fprintf(out, "section .text\n");
                    emit("movsd xmm0, [rel .fconst%d]", fc);
                    fc++;
                } else {
                    emit("mov rax, %ld", s->const_val);
                }
                break;
            }
            
            /* Variable normal */
            if (s->is_global) {
                if (s->ptype == TYPE_I8 || s->ptype == TYPE_U8 || 
                    s->ptype == TYPE_BOOL || s->ptype == TYPE_CHAR) {
                    emit("movzx rax, byte [rel %s]", s->name);
                } else if (s->ptype == TYPE_I16 || s->ptype == TYPE_U16) {
                    emit("movzx rax, word [rel %s]", s->name);
                } else if (s->ptype == TYPE_I32 || s->ptype == TYPE_U32) {
                    emit("mov eax, [rel %s]", s->name);
                } else if (s->ptype == TYPE_F32) {
                    emit("movss xmm0, [rel %s]", s->name);
                } else if (s->ptype == TYPE_F64) {
                    emit("movsd xmm0, [rel %s]", s->name);
                } else {
                    emit("mov rax, [rel %s]", s->name);
                }
            } else {
                if (s->ptype == TYPE_I8 || s->ptype == TYPE_U8 || 
                    s->ptype == TYPE_BOOL || s->ptype == TYPE_CHAR) {
                    emit("movzx rax, byte [rbp%+d]", s->offset);
                } else if (s->ptype == TYPE_I16 || s->ptype == TYPE_U16) {
                    emit("movzx rax, word [rbp%+d]", s->offset);
                } else if (s->ptype == TYPE_I32 || s->ptype == TYPE_U32) {
                    emit("mov eax, [rbp%+d]", s->offset);
                } else if (s->ptype == TYPE_F32) {
                    emit("movss xmm0, [rbp%+d]", s->offset);
                } else if (s->ptype == TYPE_F64) {
                    emit("movsd xmm0, [rbp%+d]", s->offset);
                } else {
                    emit("mov rax, [rbp%+d]", s->offset);
                }
            }
            break;
        }
        
        case NODE_MEMBER: {
            gen_lval(n);
            
            /* Determinar tipo del campo para cargar correctamente */
            Symbol *s = NULL;
            PrimitiveType field_type = TYPE_I64;
            
            if (n->member.obj->type == NODE_VAR) {
                s = find_sym(n->member.obj->var.name);
                if (s && s->struct_type) {
                    StructField *f = struct_get_field(s->struct_type, n->member.field);
                    if (f) field_type = f->ptype;
                }
            }
            
            /* Cargar según tipo */
            if (field_type == TYPE_I8 || field_type == TYPE_U8 || 
                field_type == TYPE_BOOL || field_type == TYPE_CHAR) {
                emit("movzx rax, byte [rax]");
            } else if (field_type == TYPE_I16 || field_type == TYPE_U16) {
                emit("movzx rax, word [rax]");
            } else if (field_type == TYPE_I32 || field_type == TYPE_U32) {
                emit("mov eax, [rax]");
            } else if (field_type == TYPE_F32) {
                emit("movss xmm0, [rax]");
            } else if (field_type == TYPE_F64) {
                emit("movsd xmm0, [rax]");
            } else {
                emit("mov rax, [rax]");
            }
            break;
        }
        
        case NODE_UNARY:
            gen_expr(n->unary.operand);
            switch (n->unary.op) {
                case '-':
                    emit("neg rax");
                    break;
                case '~':
                    emit("not rax");
                    break;
                case '!':
                    emit("test rax, rax");
                    emit("setz al");
                    emit("movzx rax, al");
                    break;
            }
            break;
        
        case NODE_BINOP:
            gen_expr(n->bin.r);
            emit("push rax");
            gen_expr(n->bin.l);
            emit("pop rdi");
            
            switch (n->bin.op) {
                case '+': emit("add rax, rdi"); break;
                case '-': emit("sub rax, rdi"); break;
                case '*': emit("imul rax, rdi"); break;
                case '/':
                    emit("cqo");
                    emit("idiv rdi");
                    break;
                case '%':
                    emit("cqo");
                    emit("idiv rdi");
                    emit("mov rax, rdx");
                    break;
                case '&': emit("and rax, rdi"); break;
                case '|': emit("or rax, rdi"); break;
                case '^': emit("xor rax, rdi"); break;
                case '<':
                    emit("mov rcx, rdi");
                    emit("sal rax, cl");
                    break;
                case '>':
                    emit("mov rcx, rdi");
                    emit("sar rax, cl");
                    break;
                case 'e':
                    emit("cmp rax, rdi");
                    emit("sete al");
                    emit("movzx rax, al");
                    break;
                case 'n':
                    emit("cmp rax, rdi");
                    emit("setne al");
                    emit("movzx rax, al");
                    break;
                case 'l':
                    emit("cmp rax, rdi");
                    emit("setl al");
                    emit("movzx rax, al");
                    break;
                case 'g':
                    emit("cmp rax, rdi");
                    emit("setg al");
                    emit("movzx rax, al");
                    break;
                case 'L':
                    emit("cmp rax, rdi");
                    emit("setle al");
                    emit("movzx rax, al");
                    break;
                case 'G':
                    emit("cmp rax, rdi");
                    emit("setge al");
                    emit("movzx rax, al");
                    break;
                case 'a': { /* && */
                    int lbl = new_label();
                    emit("test rax, rax");
                    emit("jz .L%d", lbl);
                    emit("test rdi, rdi");
                    emit("jz .L%d", lbl);
                    emit("mov rax, 1");
                    emit("jmp .L%d", lbl + 1);
                    fprintf(out, ".L%d:\n", lbl);
                    emit("xor rax, rax");
                    fprintf(out, ".L%d:\n", lbl + 1);
                    label_cnt += 2;
                    break;
                }
                case 'o': { /* || */
                    int lbl = new_label();
                    emit("test rax, rax");
                    emit("jnz .L%d", lbl);
                    emit("test rdi, rdi");
                    emit("jnz .L%d", lbl);
                    emit("xor rax, rax");
                    emit("jmp .L%d", lbl + 1);
                    fprintf(out, ".L%d:\n", lbl);
                    emit("mov rax, 1");
                    fprintf(out, ".L%d:\n", lbl + 1);
                    label_cnt += 2;
                    break;
                }
            }
            break;
        
        case NODE_ASSIGN:
            gen_expr(n->assign.rval);
            emit("push rax");
            gen_lval(n->assign.lval);
            emit("pop rdi");
            
            /* Determinar tipo para store correcto */
            PrimitiveType store_type = TYPE_I64;
            if (n->assign.lval->type == NODE_VAR) {
                Symbol *s = find_sym(n->assign.lval->var.name);
                if (s) store_type = s->ptype;
            } else if (n->assign.lval->type == NODE_MEMBER) {
                Symbol *s = NULL;
                if (n->assign.lval->member.obj->type == NODE_VAR) {
                    s = find_sym(n->assign.lval->member.obj->var.name);
                }
                if (s && s->struct_type) {
                    StructField *f = struct_get_field(s->struct_type, 
                                                      n->assign.lval->member.field);
                    if (f) store_type = f->ptype;
                }
            }
            
            /* Store según tipo */
            if (store_type == TYPE_I8 || store_type == TYPE_U8 || 
                store_type == TYPE_BOOL || store_type == TYPE_CHAR) {
                emit("mov byte [rax], dil");
            } else if (store_type == TYPE_I16 || store_type == TYPE_U16) {
                emit("mov word [rax], di");
            } else if (store_type == TYPE_I32 || store_type == TYPE_U32) {
                emit("mov dword [rax], edi");
            } else if (store_type == TYPE_F32) {
                emit("movss [rax], xmm0");
            } else if (store_type == TYPE_F64) {
                emit("movsd [rax], xmm0");
            } else {
                emit("mov qword [rax], rdi");
            }
            
            emit("mov rax, rdi");
            break;
        
        case NODE_CALL: {
            char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            
            /* Push args en reversa */
            for (int i = n->call.argc - 1; i >= 0; i--) {
                gen_expr(n->call.args[i]);
                emit("push rax");
            }
            
            /* Pop a registros */
            for (int i = 0; i < n->call.argc && i < 6; i++) {
                emit("pop %s", regs[i]);
            }
            
            /* Si hay más de 6 args, ya están en stack */
            
            emit("call %s", n->call.name);
            break;
        }
        
        case NODE_SYSCALL: {
            char *regs[] = {"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"};
            
            for (int i = n->call.argc - 1; i >= 0; i--) {
                gen_expr(n->call.args[i]);
                emit("push rax");
            }
            
            for (int i = 0; i < n->call.argc && i < 7; i++) {
                emit("pop %s", regs[i]);
            }
            
            emit("syscall");
            break;
        }
        
        case NODE_ADDR:
            gen_lval(n->addr.expr);
            break;
        
        case NODE_DEREF:
            gen_expr(n->deref.expr);
            emit("mov rax, [rax]");
            break;
        
        case NODE_INDEX:
            gen_lval(n);
            emit("mov rax, [rax]");
            break;
        
        default:
            break;
    }
}

static void gen_stmt(Node *n);

static void gen_block(Node *n) {
    for (int i = 0; i < n->block.count; i++) {
        gen_stmt(n->block.stmts[i]);
    }
}

static void gen_stmt(Node *n) {
    switch (n->type) {
        case NODE_CONST:
            /* Agregar constante a tabla de símbolos */
            if (n->constdef.value->type == NODE_NUM) {
                add_const_sym(n->constdef.name, n->constdef.value->num.val, 
                            0.0, 0, n->constdef.ptype);
            } else if (n->constdef.value->type == NODE_FLOAT) {
                add_const_sym(n->constdef.name, 0, 
                            n->constdef.value->flt.fval, 1, n->constdef.ptype);
            } else if (n->constdef.value->type == NODE_BOOL) {
                add_const_sym(n->constdef.name, n->constdef.value->boolean.bval, 
                            0.0, 0, n->constdef.ptype);
            }
            break;
        
        case NODE_STRUCTDEF:
        case NODE_IMPORT:
        case NODE_ALIAS:
            /* Ya procesados */
            break;
        
        case NODE_VARDEF: {
            int sz;
            if (n->vardef.struct_type) {
                sz = n->vardef.size * struct_size(n->vardef.struct_type);
            } else {
                sz = n->vardef.size * type_size(n->vardef.ptype);
            }
            
            stack_off -= sz;
            emit("sub rsp, %d", sz);
            add_sym(n->vardef.name, stack_off, 0, n->vardef.ptype, 
                   n->vardef.size, n->vardef.struct_type);
            break;
        }
        
        case NODE_RET:
            if (n->ret.expr) {
                gen_expr(n->ret.expr);
            }
            emit("mov rsp, rbp");
            emit("pop rbp");
            emit("ret");
            break;
        
        case NODE_IF: {
            int lbl_else = new_label();
            int lbl_end = new_label();
            
            gen_expr(n->ifstmt.cond);
            emit("test rax, rax");
            emit("jz .L%d", lbl_else);
            gen_stmt(n->ifstmt.then);
            
            if (n->ifstmt.els) {
                emit("jmp .L%d", lbl_end);
            }
            
            fprintf(out, ".L%d:\n", lbl_else);
            
            if (n->ifstmt.els) {
                gen_stmt(n->ifstmt.els);
                fprintf(out, ".L%d:\n", lbl_end);
            }
            break;
        }
        
        case NODE_WHILE: {
            int lbl_start = new_label();
            int lbl_end = new_label();
            
            fprintf(out, ".L%d:\n", lbl_start);
            gen_expr(n->whilestmt.cond);
            emit("test rax, rax");
            emit("jz .L%d", lbl_end);
            gen_stmt(n->whilestmt.body);
            emit("jmp .L%d", lbl_start);
            fprintf(out, ".L%d:\n", lbl_end);
            break;
        }
        
        case NODE_GOTO:
            emit("jmp .%s", n->gotostmt.label);
            break;
        
        case NODE_LABEL:
            fprintf(out, ".%s:\n", n->labelstmt.label);
            break;
        
        case NODE_BLOCK:
            gen_block(n);
            break;
        
        default:
            gen_expr(n);
            break;
    }
}

void codegen(Node *root, FILE *f) {
    out = f;
    
    fprintf(out, "; ML Compiler v4.0 - Completo\n");
    fprintf(out, "section .data\n");
    fprintf(out, "section .bss\n");
    
    /* Variables globales */
    for (int i = 0; i < root->block.count; i++) {
        Node *n = root->block.stmts[i];
        if (n->type == NODE_VARDEF) {
            int sz;
            if (n->vardef.struct_type) {
                sz = n->vardef.size * struct_size(n->vardef.struct_type);
            } else {
                sz = n->vardef.size * type_size(n->vardef.ptype);
            }
            fprintf(out, "%s: resb %d\n", n->vardef.name, sz);
            add_sym(n->vardef.name, 0, 1, n->vardef.ptype, 
                   n->vardef.size, n->vardef.struct_type);
        } else if (n->type == NODE_CONST) {
            /* Procesar constantes globales */
            if (n->constdef.value->type == NODE_NUM) {
                add_const_sym(n->constdef.name, n->constdef.value->num.val, 
                            0.0, 0, n->constdef.ptype);
            } else if (n->constdef.value->type == NODE_FLOAT) {
                add_const_sym(n->constdef.name, 0, 
                            n->constdef.value->flt.fval, 1, n->constdef.ptype);
            } else if (n->constdef.value->type == NODE_BOOL) {
                add_const_sym(n->constdef.name, n->constdef.value->boolean.bval, 
                            0.0, 0, n->constdef.ptype);
            }
        }
    }
    
    fprintf(out, "\nsection .text\n");
    fprintf(out, "global _start\n\n");
    
    /* Funciones */
    for (int i = 0; i < root->block.count; i++) {
        Node *n = root->block.stmts[i];
        if (n->type == NODE_FNDEF) {
            /* Si es export, hacer global */
            if (n->fndef.is_export) {
                fprintf(out, "global %s\n", n->fndef.name);
            }
            
            fprintf(out, "%s:\n", n->fndef.name);
            emit("push rbp");
            emit("mov rbp, rsp");
            
            int old_sym_cnt = sym_cnt;
            int old_stack_off = stack_off;
            stack_off = 0;
            
            /* Parámetros */
            char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            for (int j = 0; j < n->fndef.pcount && j < 6; j++) {
                int sz = type_size(n->fndef.ptypes[j]);
                stack_off -= sz;
                emit("sub rsp, %d", sz);
                
                PrimitiveType pt = n->fndef.ptypes[j];
                
                if (pt == TYPE_I8 || pt == TYPE_U8 || pt == TYPE_BOOL || pt == TYPE_CHAR) {
                    emit("mov byte [rbp%+d], %s", stack_off, reg_for_type(pt, regs[j]));
                } else if (pt == TYPE_I16 || pt == TYPE_U16) {
                    emit("mov word [rbp%+d], %s", stack_off, reg_for_type(pt, regs[j]));
                } else if (pt == TYPE_I32 || pt == TYPE_U32) {
                    emit("mov dword [rbp%+d], %s", stack_off, reg_for_type(pt, regs[j]));
                } else if (pt == TYPE_F32) {
                    emit("movss [rbp%+d], xmm%d", stack_off, j);
                } else if (pt == TYPE_F64) {
                    emit("movsd [rbp%+d], xmm%d", stack_off, j);
                } else {
                    emit("mov qword [rbp%+d], %s", stack_off, regs[j]);
                }
                
                add_sym(n->fndef.params[j], stack_off, 0, n->fndef.ptypes[j], 1, NULL);
            }
            
            /* Si es variadic, los args extra ya están en stack */
            
            gen_stmt(n->fndef.body);
            
            /* Default return para funciones void o sin return explícito */
            if (n->fndef.ret_type == TYPE_VOID) {
                emit("xor rax, rax");
            }
            emit("mov rsp, rbp");
            emit("pop rbp");
            emit("ret");
            fprintf(out, "\n");
            
            sym_cnt = old_sym_cnt;
            stack_off = old_stack_off;
        }
    }
    
    /* _start: llamar a main con argc y argv si es necesario */
    fprintf(out, "_start:\n");
    
    /* Verificar si main tiene parámetros */
    Node *main_fn = NULL;
    for (int i = 0; i < root->block.count; i++) {
        if (root->block.stmts[i]->type == NODE_FNDEF &&
            !strcmp(root->block.stmts[i]->fndef.name, "main")) {
            main_fn = root->block.stmts[i];
            break;
        }
    }
    
    if (main_fn && main_fn->fndef.pcount >= 2) {
        /* main(argc, argv) - pasar desde stack */
        emit("pop rdi");        /* argc */
        emit("mov rsi, rsp");   /* argv */
        emit("call main");
    } else {
        /* main() sin parámetros */
        emit("call main");
    }
    
    emit("mov rdi, rax");
    emit("mov rax, 60");
    emit("syscall");
}