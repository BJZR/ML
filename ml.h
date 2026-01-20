#ifndef ML_H
#define ML_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

/* Tipos primitivos */
typedef enum {
    TYPE_I64, TYPE_I32, TYPE_I16, TYPE_I8,
    TYPE_U64, TYPE_U32, TYPE_U16, TYPE_U8,
    TYPE_F64, TYPE_F32, TYPE_F16,
    TYPE_BOOL, TYPE_CHAR, TYPE_PTR, TYPE_VOID
} PrimitiveType;

/* Tokens */
typedef enum {
    TOK_EOF, TOK_VAR, TOK_FN, TOK_RET, TOK_IF, TOK_ELSE, TOK_WHILE,
    TOK_GOTO, TOK_LABEL, TOK_SYSCALL, TOK_IMPORT, TOK_EXPORT,
    TOK_STRUCT, TOK_CONST, TOK_ALIAS,
    TOK_I64, TOK_I32, TOK_I16, TOK_I8,
    TOK_U64, TOK_U32, TOK_U16, TOK_U8,
    TOK_F64, TOK_F32, TOK_F16,
    TOK_BOOL, TOK_CHAR, TOK_PTR, TOK_VOID,
    TOK_TRUE, TOK_FALSE,
    TOK_IDENT, TOK_NUM, TOK_FLOAT, TOK_CHARLIT, TOK_STRLIT,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_MOD,
    TOK_EQ, TOK_EQEQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_AND, TOK_OR, TOK_XOR, TOK_SHL, TOK_SHR, TOK_NOT,
    TOK_AMP, TOK_PIPE, TOK_TILDE, TOK_DOT, TOK_ELLIPSIS,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACK, TOK_RBRACK,
    TOK_SEMI, TOK_COMMA, TOK_COLON
} TokenType;

typedef struct {
    TokenType type;
    char *val;
    int line;
    int col;
} Token;

/* AST Node types */
typedef enum {
    NODE_NUM, NODE_VAR, NODE_BINOP, NODE_UNARY, NODE_ASSIGN,
    NODE_CALL, NODE_SYSCALL, NODE_IF, NODE_WHILE, NODE_GOTO, NODE_LABEL,
    NODE_RET, NODE_BLOCK, NODE_VARDEF, NODE_FNDEF,
    NODE_INDEX, NODE_ADDR, NODE_DEREF,
    NODE_BOOL, NODE_CHAR, NODE_STR, NODE_FLOAT,
    NODE_IMPORT, NODE_STRUCTDEF, NODE_CONST, NODE_MEMBER, NODE_ALIAS
} NodeType;

/* Struct field */
typedef struct {
    char *name;
    PrimitiveType ptype;
    int offset;
    int is_struct;
    int array_size;
    char *struct_name;
} StructField;

/* Struct definition */
typedef struct {
    char *name;
    StructField *fields;
    int field_count;
    int size;
} StructDef;

/* AST Node */
typedef struct Node {
    NodeType type;
    int line;
    union {
        struct { int64_t val; PrimitiveType ptype; } num;
        struct { double fval; PrimitiveType ptype; } flt;
        struct { int bval; } boolean;
        struct { char cval; } character;
        struct { char *sval; int len; } string;
        struct { char *name; PrimitiveType ptype; char *struct_type; } var;
        struct { char op; struct Node *l, *r; } bin;
        struct { char op; struct Node *operand; } unary;
        struct { struct Node *lval, *rval; } assign;
        struct { char *name; struct Node **args; int argc; int is_variadic; } call;
        struct { struct Node *cond, *then, *els; } ifstmt;
        struct { struct Node *cond, *body; } whilestmt;
        struct { char *label; } gotostmt;
        struct { char *label; } labelstmt;
        struct { struct Node *expr; } ret;
        struct { struct Node **stmts; int count; } block;
        struct { char *name; int size; PrimitiveType ptype; char *struct_type; } vardef;
        struct {
            char *name;
            char **params;
            PrimitiveType *ptypes;
            int pcount;
            struct Node *body;
            PrimitiveType ret_type;
            int is_export;
            int is_variadic;
        } fndef;
        struct { struct Node *arr, *idx; } index;
        struct { struct Node *expr; PrimitiveType target; } addr;
        struct { struct Node *expr; } deref;
        struct { char *path; } import;
        struct { char *name; StructField *fields; int field_count; } structdef;
        struct { char *name; struct Node *value; PrimitiveType ptype; } constdef;
        struct { struct Node *obj; char *field; } member;
        struct { char *alias; PrimitiveType target; } aliasdef;
    };
} Node;

/* Symbol table */
typedef struct {
    char *name;
    int offset;
    int is_global;
    PrimitiveType ptype;
    int size;
    char *struct_type;
    int is_const;
    int64_t const_val;
    double const_fval;
    int is_float_const;
} Symbol;

/* API Functions */
Token *lex(const char *src, int *count);
Node *parse(Token *toks, int count);
void codegen(Node *root, FILE *out);
void free_tokens(Token *toks, int count);
void free_node(Node *n);
char *ml_strdup(const char *s);

/* Type utilities */
const char *type_name(PrimitiveType t);
int type_size(PrimitiveType t);
int is_float_type(PrimitiveType t);

/* Struct utilities */
StructDef *find_struct(const char *name);
void add_struct(StructDef *s);
int struct_size(const char *name);
int struct_field_offset(const char *struct_name, const char *field_name);
StructField *struct_get_field(const char *struct_name, const char *field_name);

/* Import processing */
void process_imports(Node *root, const char *base_path);

/* Alias utilities */
PrimitiveType resolve_alias(const char *name);
void add_alias(const char *alias, PrimitiveType target);

#endif