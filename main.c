#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include <string.h>
#include "mpc.h"

typedef enum { 
    LVAL_NUM, 
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR
} lispValType;

typedef enum { 
    LERR_DIV_ZERO, 
    LERR_BAD_OP, 
    LERR_BAD_NUM 
} lispErrType;

typedef struct lispVal{
    lispValType type;
    long num;
    // lispErrType err;

    // error and symbol have string data
    char* err;
    char* symbol;

    // Count
    int count;

    // pointer to a list of "lval*"
    struct lispVal** cell;
} lispVal;

void lval_print(lispVal* v);
lispVal* lval_add(lispVal* v, lispVal* x);
lispVal* lval_eval(lispVal* v);
lispVal* lval_take(lispVal* v, int i);
lispVal* lval_pop(lispVal* v, int i);
lispVal* builtin_op(lispVal* v, char* symbol);

lispVal* lval_num(long x){
    lispVal* v = malloc(sizeof(lispVal));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lispVal* lval_err(char* x){
    lispVal* v = malloc(sizeof(lispVal));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(x) + 1);
    strcpy(v->err, x);
    return v;
}

lispVal* lval_sym(char* s){
    lispVal* v = malloc(sizeof(lispVal));
    v->type = LVAL_SYM;
    v->symbol = malloc(strlen(s) + 1);
    strcpy(v->symbol, s);
    return v;
}

lispVal* lval_sexpr(void){
    lispVal* v = malloc(sizeof(lispVal));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lispVal* v){
    switch (v->type) {
        // num values don't allocate memory
        case LVAL_NUM: break;

        // delete string data from error and symbols
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->symbol); break;

        // in the case of a s-expr, delete all inside elements
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++){
                lval_del(v->cell[i]);
            }
            // free the memory allocated to contain the pointers
            free(v->cell);
        break;
    }
    // free the memory of the struct itself
    free(v);
}

lispVal* lval_read_num(mpc_ast_t* ast){
    errno = 0;
    long x = strtol(ast->contents, NULL, 10);
    return errno != ERANGE 
    ? lval_num(x)
    : lval_err("invalid number");
}

lispVal* lval_read(mpc_ast_t* ast){
    // if Symbol or Number return conversion to that type
    if (strstr(ast->tag, "number")) { return lval_read_num(ast); }
    if (strstr(ast->tag, "symbol")) { return lval_sym(ast->contents); }

    // if we are at the root, or sexpr then create empty list
    lispVal* x = NULL;
    if (  (strcmp(ast->tag, ">") == 0) 
       || strstr(ast->tag, "sexpr")) { x = lval_sexpr(); }

    //fill the list with any valid expression within
    for (int i = 0; i < ast->children_num; i++){
        if (strcmp(ast->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(ast->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(ast->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(ast->children[i]));
    }

    return x;
}

lispVal* lval_add(lispVal* v, lispVal* x){
    v->count++;
    v->cell = realloc(v->cell, sizeof(lispVal*) * v->count);
    v->cell[v->count - 1]= x;
    return v;
}


void lval_expr_print(lispVal* v, char open, char close){
    putchar(open);
    for (int i = 0; i < v->count; i++){
        // print values in the inner expressions
        lval_print(v->cell[i]); 

        // dont print trailing space if last element
        if (i != (v->count - 1)){
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lispVal* v){
    switch (v->type) {
        case LVAL_NUM: printf("%ld", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->symbol); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

void lval_println(lispVal* v){
    lval_print(v);
    putchar('\n');
}

lispVal* lval_eval_sexpr(lispVal* v){
    // evaluate children
    for (int i = 0; i < v->count; i++){
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // error checking
    for (int i = 0; i< v->count; i++){
        if(v->cell[i]->type == LVAL_ERR) { return lval_take(v, i);}
    }

    // empty expression
    if (v->count == 0) { return v; }

    // single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // ensure that the first element is a Symbol
    lispVal* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM){
        lval_del(f); lval_del(v);
        return lval_err("S-expression does not start with symbol");
    }

    // in all other cases, it must be a builtin operator
    lispVal* result = builtin_op(v, f->symbol);
    lval_del(f);
    return result;
}

lispVal* lval_eval(lispVal* v){
    //evaluate s-expressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    //all other lispVal's remain the same
    return v;
}

lispVal* lval_pop(lispVal* v, int i){
    // find item at i
    lispVal* x = v->cell[i];

    // shift memory after the item at i over the top
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lispVal*) * (v->count-i-1));

    // decrease the count of items in the list
    v->count--;

    // realocate the memory used
    v->cell = realloc(v->cell, sizeof(lispVal*) * v->count);

    return x;
}


lispVal* lval_take(lispVal* v, int i){
    lispVal* x = lval_pop(v,i);
    lval_del(v);
    return x;
}

lispVal* builtin_op(lispVal* a, char* op){
    // ensure all arguments are numbers
    for (int i = 0; i < a->count; i++){
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    // pop the first element
    lispVal* x = lval_pop(a, 0);

    // if no arguments and sub then perform unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0){
        x->num = -x->num;
    }

    // while there are still elements remaining
    while (a->count > 0){
        // pop the next element
        lispVal* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        else if (strcmp(op, "-") == 0) { x->num -= y->num; }
        else if (strcmp(op, "*") == 0) { x->num *= y->num; }
        else if (strcmp(op, "/") == 0) { 
            if (y->num == 0){
                lval_del(x); lval_del(y);
                x = lval_err("Division by zero!"); break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(a); 
    return x;
}

int main(int argc, char **argv){
     // LANGUAGE GRAMMAR
    mpc_parser_t *Number   = mpc_new("number");
    mpc_parser_t *Symbol   = mpc_new("symbol");
    mpc_parser_t *Sexpr    = mpc_new("sexpr");
    mpc_parser_t *Qexpr   = mpc_new("qexpr");
    mpc_parser_t *Expr     = mpc_new("expr");
    mpc_parser_t *Bblisp   = mpc_new("bblisp");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
          number : /-?[0-9]+/ ;                                 \
          symbol : '+' | '-' | '*' | '/' | \"min\" | \"max\";   \
          sexpr  : '(' <expr>* ')' ;                            \
          qexpr  : '{' <expr>* '}' ;                            \
          expr   : <number> | <symbol> | <sexpr>  ;             \
          bblisp : /^/ <expr>* /$/ ;                            \
        ", 
        Number, Symbol, Sexpr, Expr, Bblisp);
     // END OF LANGUAGE GRAMMAR

    puts("Baby-Lisp Version 0.0.0.0.3\nPress Ctrl+c to Exit\n");

    while (1){
        char *input = readline("bb-lisp> ");

        add_history(input);

        // Attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Bblisp, &r)){
            // On success print AST

            lispVal* result = lval_eval(lval_read(r.output));
            lval_println(result);
            lval_del(result);
            
            mpc_ast_delete(r.output);
        } else {
            // On failure print error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
    }

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Bblisp);


    return 0;
}

