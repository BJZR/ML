#include "ml.h"

static Token *toks;
static int pos, cnt;

/* Tabla temporal de constantes durante el parsing */
typedef struct {
    char *name;
    int64_t int_val;
    double float_val;
    int is_float;
    PrimitiveType ptype;
} ConstEntry;

static ConstEntry const_table[256];
static int const_count = 0;

static Token *cur() { return &toks[pos]; }
static Token *next() { return &toks[pos++]; }
static int match(TokenType t) { return cur()->type == t; }

static void error(const char *msg) {
    fprintf(stderr, "Error línea %d col %d: %s\n", cur()->line, cur()->col, msg);
    exit(1);
}

static void expect(TokenType t) {
    if (!match(t)) error("token inesperado");
    next();
}

static Node *mknode(NodeType t) {
    Node *n = calloc(1, sizeof(Node));
    n->type = t;
    n->line = cur()->line;
    return n;
}

/* Buscar constante en tabla temporal */
static ConstEntry *find_const(const char *name) {
    for (int i = 0; i < const_count; i++) {
        if (!strcmp(const_table[i].name, name))
            return &const_table[i];
    }
    return NULL;
}

/* Agregar constante a tabla temporal */
static void add_const_entry(const char *name, int64_t ival, double fval, int is_float, PrimitiveType pt) {
    const_table[const_count].name = ml_strdup(name);
    const_table[const_count].int_val = ival;
    const_table[const_count].float_val = fval;
    const_table[const_count].is_float = is_float;
    const_table[const_count].ptype = pt;
    const_count++;
}

static PrimitiveType parse_type() {
    TokenType tt = cur()->type;
    
    if (tt == TOK_I64) { next(); return TYPE_I64; }
    if (tt == TOK_I32) { next(); return TYPE_I32; }
    if (tt == TOK_I16) { next(); return TYPE_I16; }
    if (tt == TOK_I8)  { next(); return TYPE_I8; }
    if (tt == TOK_U64) { next(); return TYPE_U64; }
    if (tt == TOK_U32) { next(); return TYPE_U32; }
    if (tt == TOK_U16) { next(); return TYPE_U16; }
    if (tt == TOK_U8)  { next(); return TYPE_U8; }
    if (tt == TOK_F64) { next(); return TYPE_F64; }
    if (tt == TOK_F32) { next(); return TYPE_F32; }
    if (tt == TOK_F16) { next(); return TYPE_F16; }
    if (tt == TOK_BOOL) { next(); return TYPE_BOOL; }
    if (tt == TOK_CHAR) { next(); return TYPE_CHAR; }
    if (tt == TOK_VOID) { next(); return TYPE_VOID; }
    
    if (tt == TOK_PTR) {
        next();
        /* ptr puede tener subtipo: ptr i8, ptr i32, etc */
        if (match(TOK_I64) || match(TOK_I32) || match(TOK_I16) || match(TOK_I8) ||
            match(TOK_U64) || match(TOK_U32) || match(TOK_U16) || match(TOK_U8) ||
            match(TOK_F64) || match(TOK_F32) || match(TOK_F16) ||
            match(TOK_BOOL) || match(TOK_CHAR)) {
            next();
        }
        return TYPE_PTR;
    }
    
    /* Verificar si es un alias */
    if (tt == TOK_IDENT) {
        PrimitiveType t = resolve_alias(cur()->val);
        if (t != TYPE_I64 || find_struct(cur()->val) == NULL) {
            next();
            return t;
        }
    }
    
    return TYPE_I64; /* default */
}

static Node *expr();
static Node *stmt();

static Node *primary() {
    if (match(TOK_NUM)) {
        Node *n = mknode(NODE_NUM);
        n->num.val = strtoll(next()->val, NULL, 0);
        n->num.ptype = TYPE_I64;
        return n;
    }
    
    if (match(TOK_FLOAT)) {
        Node *n = mknode(NODE_FLOAT);
        n->flt.fval = atof(next()->val);
        n->flt.ptype = TYPE_F64;
        return n;
    }
    
    if (match(TOK_TRUE)) {
        next();
        Node *n = mknode(NODE_BOOL);
        n->boolean.bval = 1;
        return n;
    }
    
    if (match(TOK_FALSE)) {
        next();
        Node *n = mknode(NODE_BOOL);
        n->boolean.bval = 0;
        return n;
    }
    
    if (match(TOK_CHARLIT)) {
        Node *n = mknode(NODE_CHAR);
        n->character.cval = next()->val[0];
        return n;
    }
    
    if (match(TOK_STRLIT)) {
        Node *n = mknode(NODE_STR);
        n->string.sval = ml_strdup(next()->val);
        n->string.len = strlen(n->string.sval);
        return n;
    }
    
    if (match(TOK_IDENT)) {
        char *name = ml_strdup(cur()->val);
        
        /* Verificar si es una constante */
        ConstEntry *ce = find_const(name);
        if (ce) {
            next();
            if (ce->is_float) {
                Node *n = mknode(NODE_FLOAT);
                n->flt.fval = ce->float_val;
                n->flt.ptype = ce->ptype;
                free(name);
                return n;
            } else {
                Node *n = mknode(NODE_NUM);
                n->num.val = ce->int_val;
                n->num.ptype = ce->ptype;
                free(name);
                return n;
            }
        }
        
        next();
        Node *n = mknode(NODE_VAR);
        n->var.name = name;
        n->var.ptype = TYPE_I64;
        n->var.struct_type = NULL;
        return n;
    }
    
    if (match(TOK_LPAREN)) {
        next();
        Node *n = expr();
        expect(TOK_RPAREN);
        return n;
    }
    
    if (match(TOK_SYSCALL)) {
        next();
        expect(TOK_LPAREN);
        Node *n = mknode(NODE_SYSCALL);
        n->call.argc = 0;
        n->call.args = malloc(7 * sizeof(Node*));
        
        if (!match(TOK_RPAREN)) {
            do {
                if (match(TOK_COMMA)) next();
                n->call.args[n->call.argc++] = expr();
            } while (match(TOK_COMMA));
        }
        expect(TOK_RPAREN);
        return n;
    }
    
    if (match(TOK_AMP)) {
        next();
        Node *n = mknode(NODE_ADDR);
        n->addr.expr = primary();
        n->addr.target = TYPE_PTR;
        return n;
    }
    
    if (match(TOK_STAR)) {
        next();
        Node *n = mknode(NODE_DEREF);
        n->deref.expr = primary();
        return n;
    }
    
    if (match(TOK_TILDE)) {
        next();
        Node *n = mknode(NODE_UNARY);
        n->unary.op = '~';
        n->unary.operand = primary();
        return n;
    }
    
    if (match(TOK_NOT)) {
        next();
        Node *n = mknode(NODE_UNARY);
        n->unary.op = '!';
        n->unary.operand = primary();
        return n;
    }
    
    error("expresión inválida");
    return NULL;
}

static Node *postfix() {
    Node *n = primary();
    
    while (1) {
        if (match(TOK_LPAREN)) {
            next();
            Node *call = mknode(NODE_CALL);
            call->call.name = n->var.name;
            call->call.argc = 0;
            call->call.args = malloc(16 * sizeof(Node*));
            call->call.is_variadic = 0;
            
            if (!match(TOK_RPAREN)) {
                do {
                    if (match(TOK_COMMA)) next();
                    call->call.args[call->call.argc++] = expr();
                } while (match(TOK_COMMA));
            }
            expect(TOK_RPAREN);
            n = call;
        } else if (match(TOK_LBRACK)) {
            next();
            Node *idx = mknode(NODE_INDEX);
            idx->index.arr = n;
            idx->index.idx = expr();
            expect(TOK_RBRACK);
            n = idx;
        } else if (match(TOK_DOT)) {
            next();
            Node *mem = mknode(NODE_MEMBER);
            mem->member.obj = n;
            mem->member.field = ml_strdup(cur()->val);
            expect(TOK_IDENT);
            n = mem;
        } else {
            break;
        }
    }
    
    return n;
}

static Node *unary() {
    if (match(TOK_MINUS)) {
        next();
        Node *n = mknode(NODE_UNARY);
        n->unary.op = '-';
        n->unary.operand = postfix();
        return n;
    }
    return postfix();
}

static Node *term() {
    Node *n = unary();
    while (match(TOK_STAR) || match(TOK_SLASH) || match(TOK_MOD)) {
        char op = cur()->type == TOK_STAR ? '*' : 
                  cur()->type == TOK_SLASH ? '/' : '%';
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = op;
        bin->bin.l = n;
        bin->bin.r = unary();
        n = bin;
    }
    return n;
}

static Node *arith() {
    Node *n = term();
    while (match(TOK_PLUS) || match(TOK_MINUS)) {
        char op = cur()->type == TOK_PLUS ? '+' : '-';
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = op;
        bin->bin.l = n;
        bin->bin.r = term();
        n = bin;
    }
    return n;
}

static Node *shift() {
    Node *n = arith();
    while (match(TOK_SHL) || match(TOK_SHR)) {
        char op = cur()->type == TOK_SHL ? '<' : '>';
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = op;
        bin->bin.l = n;
        bin->bin.r = arith();
        n = bin;
    }
    return n;
}

static Node *cmp() {
    Node *n = shift();
    if (match(TOK_EQEQ) || match(TOK_NE) || match(TOK_LT) || 
        match(TOK_GT) || match(TOK_LE) || match(TOK_GE)) {
        char op = cur()->type == TOK_EQEQ ? 'e' :
                  cur()->type == TOK_NE ? 'n' :
                  cur()->type == TOK_LT ? 'l' :
                  cur()->type == TOK_GT ? 'g' :
                  cur()->type == TOK_LE ? 'L' : 'G';
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = op;
        bin->bin.l = n;
        bin->bin.r = shift();
        n = bin;
    }
    return n;
}

static Node *bit_and() {
    Node *n = cmp();
    while (match(TOK_AMP)) {
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = '&';
        bin->bin.l = n;
        bin->bin.r = cmp();
        n = bin;
    }
    return n;
}

static Node *bit_xor() {
    Node *n = bit_and();
    while (match(TOK_XOR)) {
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = '^';
        bin->bin.l = n;
        bin->bin.r = bit_and();
        n = bin;
    }
    return n;
}

static Node *bit_or() {
    Node *n = bit_xor();
    while (match(TOK_PIPE)) {
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = '|';
        bin->bin.l = n;
        bin->bin.r = bit_xor();
        n = bin;
    }
    return n;
}

static Node *log_and() {
    Node *n = bit_or();
    while (match(TOK_AND)) {
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = 'a';
        bin->bin.l = n;
        bin->bin.r = bit_or();
        n = bin;
    }
    return n;
}

static Node *log_or() {
    Node *n = log_and();
    while (match(TOK_OR)) {
        next();
        Node *bin = mknode(NODE_BINOP);
        bin->bin.op = 'o';
        bin->bin.l = n;
        bin->bin.r = log_and();
        n = bin;
    }
    return n;
}

static Node *expr() {
    return log_or();
}

static Node *stmt() {
    /* Import */
    if (match(TOK_IMPORT)) {
        next();
        Node *n = mknode(NODE_IMPORT);
        n->import.path = ml_strdup(cur()->val);
        expect(TOK_STRLIT);
        expect(TOK_SEMI);
        return n;
    }
    
    /* Const */
    if (match(TOK_CONST)) {
        next();
        PrimitiveType ptype = parse_type();
        Node *n = mknode(NODE_CONST);
        n->constdef.name = ml_strdup(cur()->val);
        n->constdef.ptype = ptype;
        expect(TOK_IDENT);
        expect(TOK_EQ);
        n->constdef.value = expr();
        expect(TOK_SEMI);
        
        /* Agregar a tabla temporal para uso inmediato */
        if (n->constdef.value->type == NODE_NUM) {
            add_const_entry(n->constdef.name, n->constdef.value->num.val, 
                          0.0, 0, ptype);
        } else if (n->constdef.value->type == NODE_FLOAT) {
            add_const_entry(n->constdef.name, 0, 
                          n->constdef.value->flt.fval, 1, ptype);
        } else if (n->constdef.value->type == NODE_BOOL) {
            add_const_entry(n->constdef.name, n->constdef.value->boolean.bval,
                          0.0, 0, ptype);
        }
        
        return n;
    }
    
    /* Alias */
    if (match(TOK_ALIAS)) {
        next();
        Node *n = mknode(NODE_ALIAS);
        n->aliasdef.alias = ml_strdup(cur()->val);
        expect(TOK_IDENT);
        expect(TOK_EQ);
        n->aliasdef.target = parse_type();
        expect(TOK_SEMI);
        add_alias(n->aliasdef.alias, n->aliasdef.target);
        return n;
    }
    
    /* Struct */
    if (match(TOK_STRUCT)) {
        next();
        Node *n = mknode(NODE_STRUCTDEF);
        n->structdef.name = ml_strdup(cur()->val);
        expect(TOK_IDENT);
        expect(TOK_LBRACE);
        
        n->structdef.fields = malloc(32 * sizeof(StructField));
        n->structdef.field_count = 0;
        int offset = 0;
        
        while (!match(TOK_RBRACE) && !match(TOK_EOF)) {
            PrimitiveType ft = TYPE_I64;
            char *stype = NULL;
            
            /* Verificar si el tipo es un struct */
            if (match(TOK_IDENT) && find_struct(cur()->val)) {
                stype = ml_strdup(cur()->val);
                ft = TYPE_PTR;
                next();
            } else {
                ft = parse_type();
            }
            
            char *fname = ml_strdup(cur()->val);
            expect(TOK_IDENT);
            
            int arr_size = 1;
            if (match(TOK_LBRACK)) {
                next();
                /* Soporte para constantes en tamaño de array */
                if (match(TOK_IDENT)) {
                    ConstEntry *ce = find_const(cur()->val);
                    if (ce && !ce->is_float) {
                        arr_size = (int)ce->int_val;
                        next();
                    } else {
                        error("se esperaba constante entera para tamaño de array");
                    }
                } else {
                    arr_size = atoi(cur()->val);
                    expect(TOK_NUM);
                }
                expect(TOK_RBRACK);
            }
            
            expect(TOK_SEMI);
            
            n->structdef.fields[n->structdef.field_count].name = fname;
            n->structdef.fields[n->structdef.field_count].ptype = ft;
            n->structdef.fields[n->structdef.field_count].offset = offset;
            n->structdef.fields[n->structdef.field_count].is_struct = stype ? 1 : 0;
            n->structdef.fields[n->structdef.field_count].struct_name = stype;
            n->structdef.fields[n->structdef.field_count].array_size = arr_size;
            
            int sz = stype ? struct_size(stype) * arr_size : type_size(ft) * arr_size;
            offset += sz;
            n->structdef.field_count++;
        }
        expect(TOK_RBRACE);
        
        /* Registrar struct */
        StructDef sd;
        sd.name = n->structdef.name;
        sd.fields = n->structdef.fields;
        sd.field_count = n->structdef.field_count;
        sd.size = offset;
        add_struct(&sd);
        
        return n;
    }
    
    /* Variable */
    if (match(TOK_VAR)) {
        next();
        char *stype = NULL;
        PrimitiveType ptype = TYPE_I64;
        
        /* Verificar si es un struct */
        if (match(TOK_IDENT) && find_struct(cur()->val)) {
            stype = ml_strdup(cur()->val);
            ptype = TYPE_PTR;
            next();
        } else {
            ptype = parse_type();
        }
        
        Node *n = mknode(NODE_VARDEF);
        n->vardef.name = ml_strdup(cur()->val);
        n->vardef.ptype = ptype;
        n->vardef.struct_type = stype;
        expect(TOK_IDENT);
        
        if (match(TOK_LBRACK)) {
            next();
            /* Soporte para constantes en tamaño de array */
            if (match(TOK_IDENT)) {
                ConstEntry *ce = find_const(cur()->val);
                if (ce && !ce->is_float) {
                    n->vardef.size = (int)ce->int_val;
                    next();
                } else {
                    error("se esperaba constante entera para tamaño de array");
                }
            } else {
                n->vardef.size = atoi(cur()->val);
                expect(TOK_NUM);
            }
            expect(TOK_RBRACK);
        } else {
            n->vardef.size = 1;
        }
        
        expect(TOK_SEMI);
        return n;
    }
    
    if (match(TOK_RET)) {
        next();
        Node *n = mknode(NODE_RET);
        if (!match(TOK_SEMI)) {
            n->ret.expr = expr();
        } else {
            n->ret.expr = NULL;
        }
        expect(TOK_SEMI);
        return n;
    }
    
    if (match(TOK_LABEL)) {
        next();
        Node *n = mknode(NODE_LABEL);
        n->labelstmt.label = ml_strdup(cur()->val);
        expect(TOK_IDENT);
        expect(TOK_SEMI);
        return n;
    }
    
    if (match(TOK_GOTO)) {
        next();
        Node *n = mknode(NODE_GOTO);
        n->gotostmt.label = ml_strdup(cur()->val);
        expect(TOK_IDENT);
        expect(TOK_SEMI);
        return n;
    }
    
    if (match(TOK_IF)) {
        next();
        Node *n = mknode(NODE_IF);
        n->ifstmt.cond = expr();
        n->ifstmt.then = stmt();
        if (match(TOK_ELSE)) {
            next();
            n->ifstmt.els = stmt();
        } else {
            n->ifstmt.els = NULL;
        }
        return n;
    }
    
    if (match(TOK_WHILE)) {
        next();
        Node *n = mknode(NODE_WHILE);
        n->whilestmt.cond = expr();
        n->whilestmt.body = stmt();
        return n;
    }
    
    if (match(TOK_LBRACE)) {
        next();
        Node *n = mknode(NODE_BLOCK);
        n->block.stmts = malloc(64 * sizeof(Node*));
        n->block.count = 0;
        
        while (!match(TOK_RBRACE) && !match(TOK_EOF)) {
            n->block.stmts[n->block.count++] = stmt();
        }
        expect(TOK_RBRACE);
        return n;
    }
    
    Node *e = expr();
    
    if (match(TOK_EQ)) {
        next();
        Node *n = mknode(NODE_ASSIGN);
        n->assign.lval = e;
        n->assign.rval = expr();
        expect(TOK_SEMI);
        return n;
    }
    
    expect(TOK_SEMI);
    return e;
}

Node *parse(Token *t, int c) {
    toks = t;
    cnt = c;
    pos = 0;
    const_count = 0; /* Reset const table */
    
    Node *root = mknode(NODE_BLOCK);
    root->block.stmts = malloc(256 * sizeof(Node*));
    root->block.count = 0;
    
    while (!match(TOK_EOF)) {
        if (match(TOK_EXPORT)) {
            next();
            expect(TOK_FN);
            PrimitiveType ret_type = parse_type();
            Node *fn = mknode(NODE_FNDEF);
            fn->fndef.name = ml_strdup(cur()->val);
            fn->fndef.ret_type = ret_type;
            fn->fndef.is_export = 1;
            fn->fndef.is_variadic = 0;
            expect(TOK_IDENT);
            expect(TOK_LPAREN);
            
            fn->fndef.params = malloc(16 * sizeof(char*));
            fn->fndef.ptypes = malloc(16 * sizeof(PrimitiveType));
            fn->fndef.pcount = 0;
            
            if (!match(TOK_RPAREN)) {
                do {
                    if (match(TOK_COMMA)) next();
                    
                    /* Verificar variadic ... */
                    if (match(TOK_ELLIPSIS)) {
                        fn->fndef.is_variadic = 1;
                        next();
                        break;
                    }
                    
                    PrimitiveType pt = parse_type();
                    fn->fndef.ptypes[fn->fndef.pcount] = pt;
                    fn->fndef.params[fn->fndef.pcount++] = ml_strdup(cur()->val);
                    expect(TOK_IDENT);
                } while (match(TOK_COMMA));
            }
            expect(TOK_RPAREN);
            fn->fndef.body = stmt();
            root->block.stmts[root->block.count++] = fn;
        } else if (match(TOK_FN)) {
            next();
            PrimitiveType ret_type = parse_type();
            Node *fn = mknode(NODE_FNDEF);
            fn->fndef.name = ml_strdup(cur()->val);
            fn->fndef.ret_type = ret_type;
            fn->fndef.is_export = 0;
            fn->fndef.is_variadic = 0;
            expect(TOK_IDENT);
            expect(TOK_LPAREN);
            
            fn->fndef.params = malloc(16 * sizeof(char*));
            fn->fndef.ptypes = malloc(16 * sizeof(PrimitiveType));
            fn->fndef.pcount = 0;
            
            if (!match(TOK_RPAREN)) {
                do {
                    if (match(TOK_COMMA)) next();
                    
                    /* Verificar variadic ... */
                    if (match(TOK_ELLIPSIS)) {
                        fn->fndef.is_variadic = 1;
                        next();
                        break;
                    }
                    
                    PrimitiveType pt = parse_type();
                    fn->fndef.ptypes[fn->fndef.pcount] = pt;
                    fn->fndef.params[fn->fndef.pcount++] = ml_strdup(cur()->val);
                    expect(TOK_IDENT);
                } while (match(TOK_COMMA));
            }
            expect(TOK_RPAREN);
            fn->fndef.body = stmt();
            root->block.stmts[root->block.count++] = fn;
        } else {
            root->block.stmts[root->block.count++] = stmt();
        }
    }
    
    return root;
}

void free_node(Node *n) {
    if (!n) return;
    /* Simplificado - en producción liberar recursivamente */
    free(n);
}