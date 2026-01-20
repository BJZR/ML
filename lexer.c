#include "ml.h"

static Token mktok(TokenType t, const char *v, int ln, int col) {
    Token tok;
    tok.type = t;
    tok.val = v ? ml_strdup(v) : NULL;
    tok.line = ln;
    tok.col = col;
    return tok;
}

static TokenType keyword(const char *s) {
    if (!strcmp(s, "var")) return TOK_VAR;
    if (!strcmp(s, "fn")) return TOK_FN;
    if (!strcmp(s, "ret")) return TOK_RET;
    if (!strcmp(s, "if")) return TOK_IF;
    if (!strcmp(s, "else")) return TOK_ELSE;
    if (!strcmp(s, "while")) return TOK_WHILE;
    if (!strcmp(s, "goto")) return TOK_GOTO;
    if (!strcmp(s, "label")) return TOK_LABEL;
    if (!strcmp(s, "syscall")) return TOK_SYSCALL;
    if (!strcmp(s, "import")) return TOK_IMPORT;
    if (!strcmp(s, "export")) return TOK_EXPORT;
    if (!strcmp(s, "struct")) return TOK_STRUCT;
    if (!strcmp(s, "const")) return TOK_CONST;
    if (!strcmp(s, "alias")) return TOK_ALIAS;
    if (!strcmp(s, "i64")) return TOK_I64;
    if (!strcmp(s, "i32")) return TOK_I32;
    if (!strcmp(s, "i16")) return TOK_I16;
    if (!strcmp(s, "i8")) return TOK_I8;
    if (!strcmp(s, "u64")) return TOK_U64;
    if (!strcmp(s, "u32")) return TOK_U32;
    if (!strcmp(s, "u16")) return TOK_U16;
    if (!strcmp(s, "u8")) return TOK_U8;
    if (!strcmp(s, "f64")) return TOK_F64;
    if (!strcmp(s, "f32")) return TOK_F32;
    if (!strcmp(s, "f16")) return TOK_F16;
    if (!strcmp(s, "bool")) return TOK_BOOL;
    if (!strcmp(s, "char")) return TOK_CHAR;
    if (!strcmp(s, "ptr")) return TOK_PTR;
    if (!strcmp(s, "void")) return TOK_VOID;
    if (!strcmp(s, "true")) return TOK_TRUE;
    if (!strcmp(s, "false")) return TOK_FALSE;
    return TOK_IDENT;
}

Token *lex(const char *src, int *cnt) {
    int cap = 512;
    int n = 0;
    int ln = 1;
    int col = 1;
    Token *toks = malloc(cap * sizeof(Token));
    const char *p = src;

    while (*p) {
        int start_col = col;
        
        /* Espacios */
        if (isspace(*p)) {
            if (*p == '\n') {
                ln++;
                col = 1;
            } else {
                col++;
            }
            p++;
            continue;
        }

        /* Comentarios */
        if (*p == '#') {
            while (*p && *p != '\n') {
                p++;
                col++;
            }
            continue;
        }

        /* Expandir buffer */
        if (n >= cap) {
            cap *= 2;
            toks = realloc(toks, cap * sizeof(Token));
        }

        /* Números (enteros y flotantes) */
        if (isdigit(*p)) {
            char buf[64];
            int i = 0;
            int is_float = 0;
            
            while (isdigit(*p) || *p == '.' || *p == 'x' || 
                   (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                if (*p == '.') is_float = 1;
                buf[i++] = *p++;
                col++;
            }
            buf[i] = 0;
            toks[n++] = mktok(is_float ? TOK_FLOAT : TOK_NUM, buf, ln, start_col);
            continue;
        }

        /* Identificadores */
        if (isalpha(*p) || *p == '_') {
            char buf[64];
            int i = 0;
            while (isalnum(*p) || *p == '_') {
                buf[i++] = *p++;
                col++;
            }
            buf[i] = 0;
            toks[n++] = mktok(keyword(buf), buf, ln, start_col);
            continue;
        }

        /* Char literal: 'a' */
        if (*p == '\'') {
            p++;
            col++;
            char c = *p;
            if (*p == '\\') {
                p++;
                col++;
                switch (*p) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '0': c = '\0'; break;
                    case '\\': c = '\\'; break;
                    case '\'': c = '\''; break;
                    default: c = *p;
                }
            }
            p++;
            col++;
            if (*p != '\'') {
                fprintf(stderr, "Error léxico línea %d col %d: char literal no cerrado\n", 
                        ln, start_col);
                exit(1);
            }
            p++;
            col++;
            char buf[2] = {c, 0};
            toks[n++] = mktok(TOK_CHARLIT, buf, ln, start_col);
            continue;
        }

        /* String literal: "abc" */
        if (*p == '"') {
            p++;
            col++;
            char buf[512];
            int i = 0;
            while (*p && *p != '"') {
                if (*p == '\\') {
                    p++;
                    col++;
                    switch (*p) {
                        case 'n': buf[i++] = '\n'; break;
                        case 't': buf[i++] = '\t'; break;
                        case 'r': buf[i++] = '\r'; break;
                        case '0': buf[i++] = '\0'; break;
                        case '\\': buf[i++] = '\\'; break;
                        case '"': buf[i++] = '"'; break;
                        default: buf[i++] = *p;
                    }
                    p++;
                    col++;
                } else {
                    buf[i++] = *p++;
                    col++;
                }
            }
            if (*p != '"') {
                fprintf(stderr, "Error léxico línea %d col %d: string literal no cerrado\n", 
                        ln, start_col);
                exit(1);
            }
            p++;
            col++;
            buf[i] = 0;
            toks[n++] = mktok(TOK_STRLIT, buf, ln, start_col);
            continue;
        }

        /* Operadores de tres caracteres */
        if (*p == '.' && p[1] == '.' && p[2] == '.') {
            toks[n++] = mktok(TOK_ELLIPSIS, NULL, ln, start_col);
            p += 3;
            col += 3;
            continue;
        }

        /* Operadores dobles */
        if (*p == '=' && p[1] == '=') {
            toks[n++] = mktok(TOK_EQEQ, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '!' && p[1] == '=') {
            toks[n++] = mktok(TOK_NE, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '<' && p[1] == '=') {
            toks[n++] = mktok(TOK_LE, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '>' && p[1] == '=') {
            toks[n++] = mktok(TOK_GE, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '<' && p[1] == '<') {
            toks[n++] = mktok(TOK_SHL, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '>' && p[1] == '>') {
            toks[n++] = mktok(TOK_SHR, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '&' && p[1] == '&') {
            toks[n++] = mktok(TOK_AND, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }
        if (*p == '|' && p[1] == '|') {
            toks[n++] = mktok(TOK_OR, NULL, ln, start_col);
            p += 2;
            col += 2;
            continue;
        }

        /* Operadores simples */
        switch (*p) {
            case '+': toks[n++] = mktok(TOK_PLUS, NULL, ln, start_col); p++; col++; break;
            case '-': toks[n++] = mktok(TOK_MINUS, NULL, ln, start_col); p++; col++; break;
            case '*': toks[n++] = mktok(TOK_STAR, NULL, ln, start_col); p++; col++; break;
            case '/': toks[n++] = mktok(TOK_SLASH, NULL, ln, start_col); p++; col++; break;
            case '%': toks[n++] = mktok(TOK_MOD, NULL, ln, start_col); p++; col++; break;
            case '=': toks[n++] = mktok(TOK_EQ, NULL, ln, start_col); p++; col++; break;
            case '<': toks[n++] = mktok(TOK_LT, NULL, ln, start_col); p++; col++; break;
            case '>': toks[n++] = mktok(TOK_GT, NULL, ln, start_col); p++; col++; break;
            case '&': toks[n++] = mktok(TOK_AMP, NULL, ln, start_col); p++; col++; break;
            case '|': toks[n++] = mktok(TOK_PIPE, NULL, ln, start_col); p++; col++; break;
            case '^': toks[n++] = mktok(TOK_XOR, NULL, ln, start_col); p++; col++; break;
            case '~': toks[n++] = mktok(TOK_TILDE, NULL, ln, start_col); p++; col++; break;
            case '!': toks[n++] = mktok(TOK_NOT, NULL, ln, start_col); p++; col++; break;
            case '.': toks[n++] = mktok(TOK_DOT, NULL, ln, start_col); p++; col++; break;
            case '(': toks[n++] = mktok(TOK_LPAREN, NULL, ln, start_col); p++; col++; break;
            case ')': toks[n++] = mktok(TOK_RPAREN, NULL, ln, start_col); p++; col++; break;
            case '{': toks[n++] = mktok(TOK_LBRACE, NULL, ln, start_col); p++; col++; break;
            case '}': toks[n++] = mktok(TOK_RBRACE, NULL, ln, start_col); p++; col++; break;
            case '[': toks[n++] = mktok(TOK_LBRACK, NULL, ln, start_col); p++; col++; break;
            case ']': toks[n++] = mktok(TOK_RBRACK, NULL, ln, start_col); p++; col++; break;
            case ';': toks[n++] = mktok(TOK_SEMI, NULL, ln, start_col); p++; col++; break;
            case ',': toks[n++] = mktok(TOK_COMMA, NULL, ln, start_col); p++; col++; break;
            case ':': toks[n++] = mktok(TOK_COLON, NULL, ln, start_col); p++; col++; break;
            default:
                fprintf(stderr, "Error léxico línea %d col %d: carácter inválido '%c'\n", 
                        ln, col, *p);
                exit(1);
        }
    }

    toks[n++] = mktok(TOK_EOF, NULL, ln, col);
    *cnt = n;
    return toks;
}

void free_tokens(Token *toks, int cnt) {
    for (int i = 0; i < cnt; i++)
        free(toks[i].val);
    free(toks);
}