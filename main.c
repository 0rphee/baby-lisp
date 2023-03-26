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

typedef struct {
    lispValType type;
    long num;
    int err;
} lispVal;


lispVal lval_num(long x){
    lispVal v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lispVal lval_err(int x){
    lispVal v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(lispVal v){
    switch (v.type) {

        case LVAL_NUM: 
            printf("%ld", v.num);
            break;

        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO){
                printf("ERROR: Division by Zero");
            } else if (v.err == LERR_BAD_OP) {
                printf("ERROR: Invalid Operator");
            } else if (v.err == LERR_BAD_NUM) {
                printf("ERROR: Invalid Number");
            }
            break;

        case LVAL_SEXPR: // TODO: finish case statements
            break;

        case LVAL_SYM:
            break;
    }
}

void lval_println(lispVal v){
    lval_print(v);
    putchar('\n');
}

lispVal eval_op(lispVal x, char *operator, lispVal y){
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }
    
    if (strcmp(operator, "+") == 0) { return lval_num(x.num + y.num); }
    if (strcmp(operator, "-") == 0) { return lval_num(x.num - y.num); }
    if (strcmp(operator, "*") == 0) { return lval_num(x.num * y.num); }
    if (strcmp(operator, "/") == 0) { 
        return y.num == 0 
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }

    if (strcmp(operator, "min") == 0) { 
        if (x.num < y.num){ return x; } else { return y; }
    }
    if (strcmp(operator, "max") == 0) { 
        if (y.num < x.num){ return x; } else { return y; }
    }
    return lval_err(LERR_BAD_OP);
}

lispVal eval(mpc_ast_t *ast){
    if (strstr(ast->tag, "number")){
        errno = 0;
        long x = strtol(ast->contents, NULL, 10);

        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // The operator is always the second child
    char *operator = ast->children[1]->contents;

    lispVal x = eval(ast->children[2]);

    // iterate the remaining children and combining
    int i = 3;
    while (strstr(ast->children[i]->tag, "expr")){
        x = eval_op(x, operator, eval(ast->children[i]));
        i++;
    }
    return x;
}

int main(int argc, char **argv){
     // LANGUAGE GRAMMAR
    mpc_parser_t *Number   = mpc_new("number");
    mpc_parser_t *Symbol   = mpc_new("symbol");
    mpc_parser_t *Sexpr    = mpc_new("sexpr");
    mpc_parser_t *Expr     = mpc_new("expr");
    mpc_parser_t *Bblisp   = mpc_new("bblisp");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                         \
          number   : /-?[0-9]+/ ;                                 \
          symbol   : '+' | '-' | '*' | '/' | \"min\" | \"max\";   \
          sexpr    : '(' <expr>* ')' ;                            \
          expr     : <number> | <symbol> | <sexpr>  ;             \
          bblisp   : /^/ <expr>* /$/ ;                            \
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

            lispVal result = eval(r.output);
            lval_println(result);
            
            mpc_ast_delete(r.output);
        } else {
            // On failure print error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        

        free(input);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Bblisp);

    return 0;
}

