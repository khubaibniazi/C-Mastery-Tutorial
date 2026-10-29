/*
 * ============================================================
 *  EXPRESSION CALCULATOR  -  C Mastery Level
 * ============================================================
 *  Type a full math expression and get the correct answer,
 *  with brackets, operator priority, functions and more:
 *
 *      3 + 4 * (2 - 1)        ->  7
 *      2 ^ 3 ^ 2              ->  512     (^ is right-to-left)
 *      -2 ^ 2                 ->  -4
 *      sqrt(16) + 2(3 + 4)    ->  18      (implicit multiply)
 *      sin(pi / 2) * 10 % 4   ->  2
 *
 *  Supported:  + - * / % ^   ( )   unary minus
 *  Functions:  sqrt sin cos tan log ln exp abs floor ceil
 *  Constants:  pi  e  ans (your previous answer)
 *
 *  HOW IT WORKS (the Shunting-Yard algorithm by Dijkstra):
 *    1. TOKENIZE   "3 + 4 * 2"  ->  [3] [+] [4] [*] [2]
 *    2. TO RPN     reorder using an operator STACK to get
 *                  Reverse Polish Notation:  3 4 2 * +
 *    3. EVALUATE   walk the RPN with a value STACK.
 *  RPN needs no brackets and no priority rules at all!
 *  Type "rpn" to switch showing the RPN on or off.
 *
 *  Concepts used: stacks, parsing, enums, structs, error
 *  handling, math.h
 *
 *  Compile:   gcc calculator.c -o calculator -lm
 *  Run:       ./calculator       (Windows: calculator.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_TOKENS 256

typedef enum { T_NUM, T_OP, T_FUNC, T_LP, T_RP } TokenType;

typedef struct {
    TokenType type;
    double    value;      /* for T_NUM                                    */
    char      op;         /* for T_OP: + - * / % ^  and 'u' = unary minus */
    char      name[8];    /* for T_FUNC                                   */
} Token;

static const char *FUNCS[] = {"sqrt", "sin", "cos", "tan", "log", "ln", "exp", "abs", "floor", "ceil"};
#define NUM_FUNCS ((int)(sizeof FUNCS / sizeof FUNCS[0]))

static double last_answer = 0;
static int    show_rpn = 1;

/* ======================= step 1: tokenizer ======================= */

static int is_function(const char *s)
{
    for (int i = 0; i < NUM_FUNCS; i++)
        if (strcmp(FUNCS[i], s) == 0) return 1;
    return 0;
}

/* Adds a token; inserts a hidden '*' for things like  2(3+4)  or  2pi */
static int add_token(Token *t, int *n, Token tok, char *err)
{
    if (*n >= MAX_TOKENS - 2) { strcpy(err, "Expression is too long."); return 0; }

    if (*n > 0 && (tok.type == T_NUM || tok.type == T_LP || tok.type == T_FUNC)) {
        TokenType prev = t[*n - 1].type;
        if (prev == T_NUM || prev == T_RP) {
            Token mul = { T_OP, 0, '*', "" };
            t[(*n)++] = mul;
        }
    }
    t[(*n)++] = tok;
    return 1;
}

static int tokenize(const char *s, Token *t, int *count, char *err)
{
    int n = 0;
    while (*s) {
        if (isspace((unsigned char)*s)) { s++; continue; }

        Token tok;
        memset(&tok, 0, sizeof tok);

        if (isdigit((unsigned char)*s) || *s == '.') {
            char *end;
            tok.type = T_NUM;
            tok.value = strtod(s, &end);
            if (end == s) { snprintf(err, 100, "Bad number near '%c'.", *s); return 0; }
            s = end;
        }
        else if (isalpha((unsigned char)*s)) {
            char word[16];
            int w = 0;
            while (isalpha((unsigned char)*s) && w < 15) word[w++] = (char)tolower((unsigned char)*s++);
            word[w] = '\0';

            if (strcmp(word, "pi") == 0)       { tok.type = T_NUM; tok.value = 3.14159265358979323846; }
            else if (strcmp(word, "e") == 0)   { tok.type = T_NUM; tok.value = 2.71828182845904523536; }
            else if (strcmp(word, "ans") == 0) { tok.type = T_NUM; tok.value = last_answer; }
            else if (is_function(word)) {
                const char *p = s;
                while (isspace((unsigned char)*p)) p++;
                if (*p != '(') { snprintf(err, 100, "Function '%s' needs brackets, like %s(...).", word, word); return 0; }
                tok.type = T_FUNC;
                strcpy(tok.name, word);
            }
            else { snprintf(err, 100, "Unknown word '%s'.", word); return 0; }
        }
        else if (*s == '(') { tok.type = T_LP; s++; }
        else if (*s == ')') { tok.type = T_RP; s++; }
        else if (strchr("+-*/%^", *s)) {
            tok.type = T_OP;
            tok.op = *s;
            /* '-' is "unary minus" when it does not follow a value */
            int follows_value = (n > 0 && (t[n - 1].type == T_NUM || t[n - 1].type == T_RP));
            if (*s == '-' && !follows_value) tok.op = 'u';
            else if (*s == '+' && !follows_value) { s++; continue; }   /* unary plus: ignore */
            s++;
        }
        else { snprintf(err, 100, "Unexpected character '%c'.", *s); return 0; }

        if (!add_token(t, &n, tok, err)) return 0;
    }
    *count = n;
    return 1;
}

/* ======================= step 2: shunting-yard ======================= */

static int precedence(char op)
{
    switch (op) {
        case '+': case '-': return 1;
        case '*': case '/': case '%': return 2;
        case 'u': return 3;
        case '^': return 4;
    }
    return 0;
}

static int right_assoc(char op) { return op == '^' || op == 'u'; }

static int to_rpn(const Token *in, int n, Token *out, int *out_n, char *err)
{
    Token stack[MAX_TOKENS];
    int sp = 0, on = 0;

    for (int i = 0; i < n; i++) {
        const Token *t = &in[i];
        switch (t->type) {
            case T_NUM:
                out[on++] = *t;
                break;

            case T_FUNC:
            case T_LP:
                stack[sp++] = *t;
                break;

            case T_OP:
                /* A prefix operator (unary minus) never pops anything */
                if (t->op != 'u') {
                    while (sp > 0 && stack[sp - 1].type == T_OP) {
                        int top = precedence(stack[sp - 1].op), cur = precedence(t->op);
                        if (top > cur || (top == cur && !right_assoc(t->op))) out[on++] = stack[--sp];
                        else break;
                    }
                }
                stack[sp++] = *t;
                break;

            case T_RP:
                while (sp > 0 && stack[sp - 1].type != T_LP) out[on++] = stack[--sp];
                if (sp == 0) { strcpy(err, "Brackets do not match: extra ')'."); return 0; }
                sp--;                                           /* throw away the '(' */
                if (sp > 0 && stack[sp - 1].type == T_FUNC) out[on++] = stack[--sp];
                break;
        }
    }
    while (sp > 0) {
        if (stack[sp - 1].type == T_LP) { strcpy(err, "Brackets do not match: missing ')'."); return 0; }
        out[on++] = stack[--sp];
    }
    *out_n = on;
    return 1;
}

/* ======================= step 3: evaluate RPN ======================= */

static int apply_function(const char *name, double x, double *result, char *err)
{
    if (strcmp(name, "sqrt") == 0)  { if (x < 0)  { strcpy(err, "Cannot take sqrt of a negative number."); return 0; } *result = sqrt(x); }
    else if (strcmp(name, "sin") == 0)   *result = sin(x);
    else if (strcmp(name, "cos") == 0)   *result = cos(x);
    else if (strcmp(name, "tan") == 0)   *result = tan(x);
    else if (strcmp(name, "log") == 0)   { if (x <= 0) { strcpy(err, "log needs a positive number."); return 0; } *result = log10(x); }
    else if (strcmp(name, "ln") == 0)    { if (x <= 0) { strcpy(err, "ln needs a positive number.");  return 0; } *result = log(x); }
    else if (strcmp(name, "exp") == 0)   *result = exp(x);
    else if (strcmp(name, "abs") == 0)   *result = fabs(x);
    else if (strcmp(name, "floor") == 0) *result = floor(x);
    else if (strcmp(name, "ceil") == 0)  *result = ceil(x);
    else { strcpy(err, "Unknown function."); return 0; }
    return 1;
}

static int eval_rpn(const Token *rpn, int n, double *result, char *err)
{
    double st[MAX_TOKENS];
    int sp = 0;

    for (int i = 0; i < n; i++) {
        const Token *t = &rpn[i];

        if (t->type == T_NUM) { st[sp++] = t->value; continue; }

        if (t->type == T_FUNC || (t->type == T_OP && t->op == 'u')) {
            if (sp < 1) { strcpy(err, "Missing value."); return 0; }
            double x = st[--sp], r;
            if (t->type == T_OP) r = -x;
            else if (!apply_function(t->name, x, &r, err)) return 0;
            st[sp++] = r;
            continue;
        }

        /* binary operator: pop two values */
        if (sp < 2) { strcpy(err, "Missing value near an operator."); return 0; }
        double b = st[--sp], a = st[--sp], r = 0;
        switch (t->op) {
            case '+': r = a + b; break;
            case '-': r = a - b; break;
            case '*': r = a * b; break;
            case '/': if (b == 0) { strcpy(err, "Division by zero."); return 0; } r = a / b; break;
            case '%': if (b == 0) { strcpy(err, "Modulo by zero.");   return 0; } r = fmod(a, b); break;
            case '^': r = pow(a, b); break;
        }
        st[sp++] = r;
    }

    if (sp != 1) { strcpy(err, "Invalid expression."); return 0; }
    if (!isfinite(st[0])) { strcpy(err, "The result is not a finite number."); return 0; }
    *result = st[0];
    return 1;
}

/* ======================= user interface ======================= */

static void print_rpn(const Token *rpn, int n)
{
    printf("  RPN : ");
    for (int i = 0; i < n; i++) {
        if (rpn[i].type == T_NUM)       printf("%.10g ", rpn[i].value);
        else if (rpn[i].type == T_FUNC) printf("%s ", rpn[i].name);
        else                            printf("%s ", rpn[i].op == 'u' ? "neg" : (char[]){rpn[i].op, '\0'});
    }
    printf("\n");
}

static void print_number(double v)
{
    if (fabs(v) < 1e15 && v == floor(v)) printf("%.0f\n", v);
    else                                 printf("%.10g\n", v);
}

static void print_help(void)
{
    printf("\n  Operators : + - * / %% ^   and brackets ( )\n");
    printf("  Functions : sqrt sin cos tan log ln exp abs floor ceil\n");
    printf("  Constants : pi  e  ans (previous answer)\n");
    printf("  Commands  : rpn (toggle RPN display), help, quit\n");
    printf("  Examples  : 3 + 4 * (2 - 1)    2^3^2    sqrt(16) + 2(3+4)\n\n");
}

int main(void)
{
    char line[512];

    printf("=====================================\n");
    printf("      EXPRESSION CALCULATOR\n");
    printf("=====================================\n");
    printf("Type a math expression, or 'help'.\n");

    for (;;) {
        printf("\n> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') continue;
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) break;
        if (strcmp(line, "help") == 0) { print_help(); continue; }
        if (strcmp(line, "rpn") == 0) {
            show_rpn = !show_rpn;
            printf("  RPN display is now %s.\n", show_rpn ? "ON" : "OFF");
            continue;
        }

        Token tokens[MAX_TOKENS], rpn[MAX_TOKENS];
        int nt = 0, nr = 0;
        char err[128] = "";
        double result;

        if (!tokenize(line, tokens, &nt, err))      { printf("  Error: %s\n", err); continue; }
        if (nt == 0)                                continue;
        if (!to_rpn(tokens, nt, rpn, &nr, err))     { printf("  Error: %s\n", err); continue; }
        if (show_rpn) print_rpn(rpn, nr);
        if (!eval_rpn(rpn, nr, &result, err))       { printf("  Error: %s\n", err); continue; }

        last_answer = result;
        printf("  = ");
        print_number(result);
    }

    printf("Goodbye!\n");
    return 0;
}
