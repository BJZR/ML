#include "ml.h"

/* Tabla global de structs */
static StructDef struct_table[128];
static int struct_count = 0;

/* Tabla global de alias */
typedef struct {
    char *alias;
    PrimitiveType target;
} Alias;

static Alias alias_table[128];
static int alias_count = 0;

char *ml_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

const char *type_name(PrimitiveType t) {
    switch (t) {
        case TYPE_I64: return "i64";
        case TYPE_I32: return "i32";
        case TYPE_I16: return "i16";
        case TYPE_I8: return "i8";
        case TYPE_U64: return "u64";
        case TYPE_U32: return "u32";
        case TYPE_U16: return "u16";
        case TYPE_U8: return "u8";
        case TYPE_F64: return "f64";
        case TYPE_F32: return "f32";
        case TYPE_F16: return "f16";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_PTR: return "ptr";
        case TYPE_VOID: return "void";
        default: return "unknown";
    }
}

int type_size(PrimitiveType t) {
    switch (t) {
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_BOOL:
        case TYPE_CHAR:
            return 1;
        case TYPE_I16:
        case TYPE_U16:
        case TYPE_F16:
            return 2;
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_F32:
            return 4;
        case TYPE_I64:
        case TYPE_U64:
        case TYPE_F64:
        case TYPE_PTR:
            return 8;
        case TYPE_VOID:
            return 0;
        default:
            return 8;
    }
}

int is_float_type(PrimitiveType t) {
    return t == TYPE_F16 || t == TYPE_F32 || t == TYPE_F64;
}

StructDef *find_struct(const char *name) {
    for (int i = 0; i < struct_count; i++) {
        if (!strcmp(struct_table[i].name, name))
            return &struct_table[i];
    }
    return NULL;
}

void add_struct(StructDef *s) {
    struct_table[struct_count++] = *s;
}

int struct_size(const char *name) {
    StructDef *s = find_struct(name);
    return s ? s->size : 0;
}

int struct_field_offset(const char *struct_name, const char *field_name) {
    StructDef *s = find_struct(struct_name);
    if (!s) return -1;
    
    for (int i = 0; i < s->field_count; i++) {
        if (!strcmp(s->fields[i].name, field_name))
            return s->fields[i].offset;
    }
    return -1;
}

StructField *struct_get_field(const char *struct_name, const char *field_name) {
    StructDef *s = find_struct(struct_name);
    if (!s) return NULL;
    
    for (int i = 0; i < s->field_count; i++) {
        if (!strcmp(s->fields[i].name, field_name))
            return &s->fields[i];
    }
    return NULL;
}

PrimitiveType resolve_alias(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (!strcmp(alias_table[i].alias, name))
            return alias_table[i].target;
    }
    return TYPE_I64; /* default si no se encuentra */
}

void add_alias(const char *alias, PrimitiveType target) {
    alias_table[alias_count].alias = ml_strdup(alias);
    alias_table[alias_count].target = target;
    alias_count++;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: no se puede abrir '%s'\n", path);
        exit(1);
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(len + 1);
    if (!buf) {
        fprintf(stderr, "Error: memoria insuficiente\n");
        exit(1);
    }
    
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    
    return buf;
}

void process_imports(Node *root, const char *base_path) {
    for (int i = 0; i < root->block.count; i++) {
        Node *n = root->block.stmts[i];
        if (n->type == NODE_IMPORT) {
            printf("Importando: %s\n", n->import.path);
            
            char *src = read_file(n->import.path);
            
            int tok_cnt;
            Token *toks = lex(src, &tok_cnt);
            Node *mod_ast = parse(toks, tok_cnt);
            
            process_imports(mod_ast, n->import.path);
            
            /* Fusionar AST */
            for (int j = 0; j < mod_ast->block.count; j++) {
                Node *mod_node = mod_ast->block.stmts[j];
                
                if ((mod_node->type == NODE_FNDEF && mod_node->fndef.is_export) ||
                    mod_node->type == NODE_STRUCTDEF ||
                    mod_node->type == NODE_CONST ||
                    mod_node->type == NODE_ALIAS) {
                    root->block.stmts[root->block.count++] = mod_node;
                }
            }
            
            free_tokens(toks, tok_cnt);
            free(src);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo.ml>\n", argv[0]);
        return 1;
    }
    
    printf("ML Compiler v4.0 - Completo\n");
    printf("Compilando: %s\n", argv[1]);
    
    char *src = read_file(argv[1]);
    
    /* Lexer */
    int tok_cnt;
    Token *toks = lex(src, &tok_cnt);
    printf("Tokens: %d\n", tok_cnt);
    
    /* Parser */
    Node *ast = parse(toks, tok_cnt);
    printf("AST construido\n");
    
    /* Procesar imports */
    process_imports(ast, argv[1]);
    printf("Imports procesados\n");
    
    /* Codegen */
    FILE *out = fopen("out.asm", "w");
    if (!out) {
        fprintf(stderr, "Error: no se puede crear out.asm\n");
        return 1;
    }
    
    codegen(ast, out);
    fclose(out);
    
    printf("Generado: out.asm\n");
    printf("\nCompilar:\n");
    printf("  nasm -f elf64 out.asm\n");
    printf("  ld out.o -o programa\n");
    printf("  ./programa\n");
    
    /* Cleanup */
    free_tokens(toks, tok_cnt);
    free_node(ast);
    free(src);
    
    return 0;
}