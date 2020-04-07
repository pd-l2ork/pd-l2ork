/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* arithmetic: binops ala C language.  The 4 functions and relationals are
done on floats; the logical and bitwise binops convert their
inputs to int and their outputs back to float. */

#include "config.h"

#include "m_pd.h"
#include "string.h"
#include <math.h>

#if PD_FLOATSIZE == 32
#define POW powf
#define SIN sinf
#define COS cosf
#define ATAN atanf
#define ATAN2 atan2f
#define SQRT sqrtf
#define LOG logf
#define EXP expf
#define FABS fabsf
#define MAXLOG 87.3365
#define FMOD fmodf
#elif PD_FLOATSIZE == 64
#define POW pow
#define SIN sin
#define COS cos
#define ATAN atan
#define ATAN2 atan2
#define SQRT sqrt
#define LOG log
#define EXP exp
#define FABS fabs
#define MAXLOG 87.3365
#define FMOD fmod
#endif

/* ----------------- vm operators ------------- */

/*
*  This is our main macro where our mappings are found.
*
*  The FN argument is assumed to be another macro. The C preprocessor will
*  first do substitution for the GENERATE macro; when finished it will make
*  another pass through and do substitution for the macro we define with FN.
*/

#define GENERATE(FN) \
    FN(OP_NULL, "",    0) \
    FN(OP_N,    "!",  70) \
    FN(OP_OVR,  "/",  60) \
    FN(OP_PC,   "%",  60) \
    FN(OP_MUL,  "*",  60) \
    FN(OP_SUB,  "-",  50) \
    FN(OP_ADD,  "+",  50) \
    FN(OP_LS,   "<<", 40) \
    FN(OP_RS,   ">>", 40) \
    FN(OP_NE,   "!=", 40) \
    FN(OP_EQ,   "==", 40) \
    FN(OP_GE,   ">=", 40) \
    FN(OP_GT,   ">",  40) \
    FN(OP_LE,   "<=", 40) \
    FN(OP_LT,   "<",  40) \
    FN(OP_LA,   "&&", 30) \
    FN(OP_LO,   "||", 30) \
    FN(OP_BA,   "&",  30) \
    FN(OP_BO,   "|",  30) \
    FN(OP_Q,    "?",  20) \
    FN(OP_B,    ":",  20) \
    FN(OP_SEMI, ";",   0) \
    FN(OP_ATAN, "atan", 0) \
    FN(OP_ATAN2, "atan2", 0) \
    FN(OP_COS,  "cos", 0) \
    FN(OP_DIV,  "div", 0) \
    FN(OP_EXP,  "exp", 0) \
    FN(OP_FABS, "fabs", 0) \
    FN(OP_FMOD, "fmod", 0) \
    FN(OP_LOG,  "log", 0) \
    FN(OP_MAX,  "max", 0) \
    FN(OP_MIN,  "min", 0) \
    FN(OP_MOD,  "mod", 0) \
    FN(OP_POW,  "pow", 0) \
    FN(OP_SIN,  "sin", 0) \
    FN(OP_SQRT, "sqrt", 0) \
    FN(OP_IF,   "if", 0) \
    FN(OP_ELSE, "else", 0) \
    FN(OP_ENDIF, "endif", 0) \
    FN(OP_STARTLOOP, "loop", 0) \
    FN(OP_ENDLOOP, "endloop", 0) \
    FN(OP_RETURN, "return", 0)

/*
*  Here is our first "FN" macro. Given a pair of strings it outputs the first
*  one followed by string concatenations that will handle our combinations
*  of constant and variable argument types. We just put them right in the enum
*  field so that we can do the type-checking in the parser. Then our evaluator
*  can simply jump to the relevant case given the args so we don't have to
*  type-check there.
*/

#define OP_NAMES(op, str, bp) op, \
                          op ## _V, \
                          op ## _CV, \
                          op ## _VV,

/*
*  Now armed with our macros, let's first create our enum for
*  our supported ops.
*/
typedef enum
{
    GENERATE(OP_NAMES)
    OP_C89_SUPPORT /* to remove a trailing comma */
} t_optype;

/*
*  Now we just need some macros for the getters. In my current design parsing
*  is only done at load time, and serialization is just for debugging. So we
*  can just use a dead simple implementation-- an if for each op returning
*  the op if there's a match. I *think* we could generate an array of strings
*  and use the enum fields to index into it, but I can't remember if
*  enums must start from zero.
*/
#define OP_FROM_STRING(op, str, bp) if (!strcmp(str, name)) return op;
#define STRING_FROM_OP(op, str, bp) if (oparg == op) return str;

/* Now throw our macros into some functions. */

static t_optype op_from_symbol(t_symbol *s)
{
    char *name = s->s_name;
    GENERATE(OP_FROM_STRING);
    /* if we didn't hit a match... */
    return OP_NULL;
}

static char *string_from_op(t_optype oparg)
{
    GENERATE(STRING_FROM_OP);
    /* default... */
    return "";
}

#undef GENERATE
#undef OP_FROM_STRING
#undef STRING_FROM_OP
#undef OP_NAMES


/* ------------------------ vm ------------------------ */

static t_class *vm_class;

/* the ops */

/* let's see if we can get away with all ops being the same size */
#define OPLEN 4

#define MAX_LOOPSTACK_LEN 4

typedef struct _param
{
    t_symbol *p_name;
    t_word p_w;
} t_param;

typedef struct _token
{
    t_symbol *t_oprefsym; /* reference variable if token is an operator */
    t_atom t_at;
} t_token;

typedef struct _pratt
{
    int p_token_nr;
    int p_varno;
    t_binbuf *p_irbuf;
    t_token p_t;
} t_pratt;

typedef struct _jumpcode
{
    t_optype t;
    int index;
    int popme; /* hack to know if we're ready to get popped off the stack */
} t_jumpcode;

#define JUMPSTACKSIZE 4
#define PARAMSIZE 4

typedef struct _vm
{
    t_object x_obj;
    t_pratt x_parser;
    t_float x_in;
    t_param x_params[PARAMSIZE];
    int x_nparams;
    int x_gotparams;
    t_jumpcode x_jumpstack[JUMPSTACKSIZE];
    int x_jumpstack_nitems;
    int x_coins;
    t_word *x_pipeline;
    int x_nops;
    t_word *x_last_op;
    t_float x_ret;
    t_outlet *x_out;
} t_vm;

/* little kludge to eventually change the name to fn */
#define t_fn t_vm

static int vm_getconstvalue(t_symbol *s, t_float *f)
{
    int ret = 1;
    if (s == gensym("pi"))
        *f = 3.141592653589793;
    else if (s == gensym("tau"))
        *f = 6.28318530717958647692;
    else if (s == gensym("e"))
        *f = 2.71828182845904523536;
    else
        ret = 0;
    return ret;
}

static int token_lbp(t_vm *x, t_token *t)
{
    if (t->t_at.a_type == A_FLOAT)
        return 0;
    else if (t->t_at.a_type == A_SYMBOL)
    {
        t_symbol *s = t->t_at.a_w.w_symbol;
        if (s == &s_) return 0;
        else if (s == gensym(";")) return 0;
        else if (s == gensym("?")) return 20;
        else if (s == gensym("||")) return 30;
        else if (s == gensym("&&")) return 30;
        else if (s == gensym("<")) return 40;
        else if (s == gensym("<=")) return 40;
        else if (s == gensym(">")) return 40;
        else if (s == gensym(">=")) return 40;
        else if (s == gensym("==")) return 40;
        else if (s == gensym("!=")) return 40;
        else if (s == gensym("+")) return 50;
        else if (s == gensym("-")) return 50;
        else if (s == gensym("*")) return 60;
        else if (s == gensym("/")) return 60;
        else if (s == gensym("!")) return 70;
        else
        {
            pd_error(x, "vm: bad operator %s",
                s->s_name);
            return 0;
        }
    }
    else
    {
        bug("vm: bad atom type to token_lbp");
        return 0;
    }
}

static int get_rbp(t_vm *x, t_token *op)
{
    return token_lbp(x, op);
}

static void vm_ir_set_operand(t_vm *x, t_token *t, t_atom *at)
{
    if (t->t_at.a_type == A_SYMBOL)
        SETSYMBOL(at, t->t_oprefsym);
    else if (t->t_at.a_type == A_FLOAT)
        *at = t->t_at;
    else
        pd_error(x, "vm: bad argument type. Only float and symbol allowed.");
}

static void vm_irbuf_add(t_vm *x, t_token *operator, t_token op1, t_token op2)
{
    t_atom at[4];
    int count = x->x_parser.p_varno++;
        /* Now set our operator reference symbol. Each opcode below just
           assigns to a temporary memory location. When one of our
           operands is the output from a previous operation, we use the
           oprefno to fetch it from the correct memory location. In this
           way we can convert the AST of our Pratt parser into the opcode
           pipeline below */
    t_symbol *target;
    char buf[MAXPDSTRING];
    sprintf(buf, "_var%d", count);
    target = gensym(buf);
    operator->t_oprefsym = target;

    SETSYMBOL(at, operator->t_at.a_w.w_symbol);
    SETSYMBOL(at+1, target);
    vm_ir_set_operand(x, &op1, at+2);
    vm_ir_set_operand(x, &op2, at+3);

    binbuf_add(x->x_parser.p_irbuf, 4, at);
    binbuf_addv(x->x_parser.p_irbuf, "s", gensym(";"));
}

static void doadvance(t_vm *x, t_symbol *s, int argc, t_atom *argv)
{
    int i = x->x_parser.p_token_nr;
    if (i >= argc)
    {
        SETSYMBOL(&x->x_parser.p_t.t_at, &s_);
        return;
    }
    x->x_parser.p_t.t_at = argv[i];
    x->x_parser.p_token_nr += 1;
}

static void advance(t_vm *x, int argc, t_atom *argv)
{
    doadvance(x, 0, argc, argv);
}

/* Make sure to plug this in for better error checking... */
/*
static void advance_and_expect(t_vm *x, t_symbol *s, int argc, t_atom *argv)
{
    doadvance(x, s, argc, argv);
}
*/

static t_token expression(t_vm *x, int rbp, int argc, t_atom *argv);

static void infix(t_vm *x, t_token left, t_token *op, int argc, t_atom *argv)
{
    int rbp = get_rbp(x, op);
    vm_irbuf_add(
        x,
        op,
        left,
        expression(x, rbp, argc, argv) 
    );
}

static void ternary(t_vm *x, t_token left, t_token *op, int argc, t_atom *argv)
{
    int rbp = get_rbp(x, op);
    /* first let's set the condition based on "left" */
    vm_irbuf_add(
        x,
        op,
        left,
        expression(x, rbp, argc, argv) 
    );
}

static t_token token_led(t_vm *x, t_token left, t_token t,
    int argc, t_atom *argv)
{
    if (t.t_at.a_type == A_FLOAT)
        return t;
    else if (t.t_at.a_type == A_SYMBOL)
    {
        t_symbol *s = t.t_at.a_w.w_symbol;
        if (s == gensym("+") ||
            s == gensym("-") ||
            s == gensym("*") ||
            s == gensym("/") ||
            s == gensym("==") ||
            s == gensym("!=") ||
            s == gensym("<") ||
            s == gensym("<=") ||
            s == gensym(">") ||
            s == gensym(">="))
        {
            infix(x, left, &t, argc, argv);
            return t;
        }
        else if (s == gensym("?"))
        {
            ternary(x, left, &t, argc, argv);
            return t;
        }
        else
        {
            pd_error(x, "vm: only infix and ternary supported");
            return t;
        }
    }
    else
    {
        pd_error(x, "vm: only symbols and floats allowed");
        return t;
    }
}


static int vm_get_param_index(t_vm *x, t_symbol *s);

static t_token token_nud(t_vm *x, t_token token, int argc, t_atom *argv)
{
    t_token ret;
    if (token.t_at.a_type == A_FLOAT)
        return token;
    else if (token.t_at.a_type == A_SYMBOL)
    {
        t_float f;
        t_symbol *s = token.t_at.a_w.w_symbol;
        /* if it's a parameter return the name */
        if (vm_get_param_index(x, s) != -1)
        {
            return token;
        }
        else if (vm_getconstvalue(s, &f))
        {
            SETFLOAT(&ret.t_at, f);
            return ret;
        }
        else if (s == gensym("("))
        {
            ret = expression(x, 0, argc, argv);
            /* need to check for closing parens below */
            advance(x, argc, argv);
            return ret;
        }
        else
        {
            pd_error(x, "vm: symbol %s must be a parameter or a constant",
                token.t_at.a_w.w_symbol->s_name);
            return token;
        }
    }
    else
    {
        pd_error(x, "vm: only symbols and floats allowed");
        return token;
    }
}

static t_token expression(t_vm *x, int rbp, int argc, t_atom *argv)
{
    t_token left;
    t_token t = x->x_parser.p_t;
    advance(x, argc, argv);
    left = token_nud(x, t, argc, argv);
    while (rbp < token_lbp(x, &x->x_parser.p_t))
    {
        t = x->x_parser.p_t;
        advance(x, argc, argv);
        left = token_led(x, left, t, argc, argv);
    }
    return left;
}

static int vm_keyword_that_accepts_an_expression(t_symbol *s)
{
    if (s == gensym("loop") ||
        s == gensym("if") ||
        s == gensym("return"))
    {
        return 1;
    }
    else return 0;
}

static int vm_keyword_that_rejects_an_expression(t_symbol *s)
{
    if (s == gensym("endloop") ||
        s == gensym("endif") ||
        s == gensym("else"))
    {
        return 1;
    }
    else return 0;
}

static int vm_parse_expression(t_vm *x, int argc, t_atom *argv)
{
    if (argc)
    {
        /* our parameter symbol or keyword */
        t_symbol *s = atom_getsymbolarg(0, argc, argv);
        int got_param_index = vm_get_param_index(x, s) != -1;
        int got_keyword = vm_keyword_that_accepts_an_expression(s) ||
            vm_keyword_that_rejects_an_expression(s);
        if (got_param_index || got_keyword)
        {
            if (argc > 1)
            {
                t_token t;
                    /* Let's go ahead and error out for keywords that don't
                       accept an argument */
                if (vm_keyword_that_rejects_an_expression(s))
                {
                    pd_error(x, "vm: extra arguments detected after '%s'",
                        s->s_name);
                    return 0;
                }
                argc--, argv++;
                    /* eat an initial arg into the parser */
                advance(x, argc, argv);
                    /* now feed an expression into our FUDI intermediate
                       representation buffer */
                t = expression(x, 0, argc, argv);
                    /* done with the expression. Now assign the result of
                       the expression to the parameter or keyword */
                binbuf_addv(x->x_parser.p_irbuf, "sssfs",
                    s,
                    gensym("+"),
                    t.t_oprefsym,
                    0.,
                    gensym(";")
                );
            }
            else if (got_keyword)
            {
                binbuf_addv(x->x_parser.p_irbuf, "s;", s);
            }
            else
            {
                pd_error(x, "vm: parameter must be followed by an "
                    "expression");
                return 0;
            }
        }
        else
        {
            pd_error(x, "vm: need a parameter for the 1st argument");
            return 0;
        }
        return 1;
    }
    else return 1;
}


static int vm_set_params(t_vm *x, int argc, t_atom *argv);

static void vm_init_parser(t_vm *x)
{
    x->x_parser.p_irbuf = binbuf_new();
    x->x_parser.p_token_nr = 0;
}

static void vm_free_parser(t_fn *x)
{
    binbuf_free(x->x_parser.p_irbuf);
}

/* given a separator "s", return the number of tokens before "s"
   if "s" isn't found, return the number of tokens searched and set the
   terminator flag to 0 */
static int vm_get_submessage(t_symbol *s, int argc, t_atom *argv, int *term)
{

post("my submessage args are:");
postatom(argc, argv);
post("argc is %d", argc);

    int i;
    *term = 0;
    for (i = 0; i < argc; i++)
    {
        if (atom_getsymbolarg(i, argc, argv) == s)
        {
            *term = 1;
            return i;
        }
    }
    return i;
}

static int vm_parse_messages(t_vm *x, int argc, t_atom *argv)
{
    int i;
    int head = 0, terminator = 0, stackdepth = 0;

    for (i = 0; argc > 0; i++)
    {
post("in parse_messages head is %d", head);
        /* set the number of variables used per line back to zero */
        x->x_parser.p_varno = 0;

        int c = vm_get_submessage(gensym(";"), argc, argv + head, &terminator);

        /*
           each semicolon separated message is a statement.
           It starts with a selector that is either a parameter
           to which we assign the output of the expression that
           follows it, or else a keyword. The current keywords
           are "if", "else", "endif", "loop", "endloop" and "return".

           The first time through we fetch any parameters we may
           have.
        */

        if (head == 0)
        {
            if (!vm_set_params(x, c, argv + head))
                return 0;
post("got here!!!");
            x->x_gotparams = 1;
        }
        else if (!vm_parse_expression(x, c, argv + head))
            return 0;
            /* now see if we got a bigger stacksize than the previous pass */
        if (x->x_parser.p_varno > stackdepth)
        {
            stackdepth = x->x_parser.p_varno;
        }

        /* Success! */
        head = c + terminator;
        argc -= head;
    }
    
    /* now that we're through, set the max stackdepth */
    x->x_parser.p_varno = stackdepth;

    return 1;
}

static int vm_get_param_index(t_vm *x, t_symbol *s)
{
    int i;
    for (i = 0; i < PARAMSIZE; i++)
    {
        if (x->x_params[i].p_name == s) return i;
    }
    return -1;
}

static int vm_set_param(t_vm *x, int argc, t_atom *at)
{
    t_float f;
    t_symbol *name = atom_getsymbolarg(0, argc, at);

post("in vm_set_param argc is %d", argc);

    /* We don't mind trailing commas. But we want to error out
       if we get multiple commas in a row or leading commas.
       If we get called with no arguments we're either a leading or
       multiple comma. */
    if (!argc) goto badnoop;

    if (op_from_symbol(name) != OP_NULL) goto badparamop;
    if (vm_getconstvalue(name, &f)) goto badparamconst;
    x->x_params[x->x_nparams].p_name = name;

    x->x_nparams += 1;

    return 1;
badnoop:
    pd_error(x, "vm: leading and double commas not allowed");
    return 0;
badparamop:
    pd_error(x, "vm: parameter name %s cannot be an operator", name->s_name);
    return 0;
badparamconst:
    pd_error(x, "vm: parameter name %s is a reserved constant", name->s_name);
    return 0;
}

static int vm_set_params(t_vm *x, int argc, t_atom *argv)
{
    int i;
    int head = 0, terminator = 0;
post("inside vm_set_params");
post("argc is %d", argc);
    for (i = 0; argc > 0; i++)
    {
post("head is %d", head);
        /* Let's just start with 4 params and see how it goes */
        if (i > PARAMSIZE)
        {
            pd_error(x, "vm: only 4 parameters allowed");
            return 0;
        }

        int c = vm_get_submessage(gensym(","), argc, argv + head, &terminator);

        if (!vm_set_param(x, c, argv + head))
        {
            return 0;
        }
        /* Success! */
        head = c + terminator;
        argc -= head;
    }
//    if (argc)
//        if (!vm_set_param(x, argv)) return 0;
//    if (argc > 1)
//        if (!vm_set_param(x, argv + 1)) return 0;
//    if (argc > 2)
//        if (!vm_set_param(x, argv + 2)) return 0;
//    if (argc > 3)
//        if (!vm_set_param(x, argv + 3)) return 0;

    return 1;
}

static int vm_jumpstack_push(t_vm *x, t_optype t, int index)
{
    if (x->x_jumpstack_nitems >= JUMPSTACKSIZE)
    {
        pd_error(x, "vm: maximum number of jump ops exceeded");
        return 0;
    }
    x->x_jumpstack[x->x_jumpstack_nitems].t = t;
    x->x_jumpstack[x->x_jumpstack_nitems].index = index;
    x->x_jumpstack_nitems += 1;
    return 1;
}

static t_jumpcode *vm_jumpstack_pop(t_vm *x)
{
    if (x->x_jumpstack_nitems < 1)
    {
//        pd_error(x, "vm: can't pop a jump opcode off the stack");
        return NULL;
    }
    x->x_jumpstack_nitems -= 1;
    return (&x->x_jumpstack[x->x_jumpstack_nitems]);
}

static int vm_jumper_forwardref(t_optype t)
{
    if (t == OP_STARTLOOP || t == OP_IF || t == OP_ELSE)
        return 1;
    else
        return 0;
}

static int vm_jumper_backref(t_optype t)
{
    if (t == OP_ENDLOOP || t == OP_ELSE)
        return 1;
    else
        return 0;
}

static int vm_check_for_endif(t_vm *x, int argc, t_atom *argv)
{
    t_symbol *s = atom_getsymbolarg(0, argc, argv);
    if (s == gensym("endif")) return 1;
    else return 0;
}

static int vm_check_for_jumper(t_vm *x, int argc, t_atom *argv)
{
    t_symbol *s = atom_getsymbolarg(0, argc, argv);
    if (s == gensym("else")) return 1;
    else if (s == gensym("endloop")) return 1;
    else if (s == gensym("if")) return 1;
    else if (s == gensym("startloop")) return 1;
    else return 0;
}

static void vm_op_set_warpzone(t_word *w, int index)
{
        /* all jump opcodes store their "warp zone" at index 2. Probably
           should document this better instead of using raw indices... */
    t_word *slot = w + 2;
    slot->w_index = index;
}

static int vm_add_opcode(t_vm *x, int argc, t_atom *argv)
{
    int offset = (x->x_nops - 1) * OPLEN;
    t_word *w = x->x_pipeline;
    t_symbol *s;
    int flag = 0;
    int got_a_jumper = vm_check_for_jumper(x, argc, argv);
    t_jumpcode *jmp = NULL;

        /* Get the op. We'll adjust it with a type-checking flag after
           fetching the args. */
    t_symbol *op = atom_getsymbolarg(0, argc, argv);
    t_optype t = op_from_symbol(op);
    if (t == OP_NULL)
    {
        pd_error(x, "vm: unknown operator");
        return 0;
    }
    else w[offset].w_index = t;

        /* Now we fetch argv[1] through argv[3] to figure out what our arg
           types are. We need to know the types to figure out which op to
           use. */
        /* arg 1: must be a symbol except for jumpers and return */
    s = atom_getsymbolarg(1, argc, argv);
    if (s != &s_)
    {
        /* should be one of our parameters. Get the index and set it. */
        if ((w[offset+1].w_index = vm_get_param_index(x, s)) == -1)
        {
            pd_error(x, "vm: unknown variable name %s", s->s_name);
            return 0;
        }
        /* if it's a jumper, flag it */
        if (got_a_jumper)
            flag ++;
    }
    else if (got_a_jumper || t == OP_RETURN)
    {
        if (argc > 1 && argv[1].a_type == A_FLOAT)
            w[offset+1].w_float = argv[1].a_w.w_float;
    }
    else
    {
        pd_error(x, "vm: 2nd argument must be a symbol");
        return 0;
    }

        /* arg 2 */
    if (got_a_jumper)
    {
            /* disallow extra args */
        if (argc > 2)
        {
            pd_error(x, "vm: loop syntax: 'loop parameterName;'");
            return 0;
        }

        if (vm_jumper_backref(t))
        {
            /* If we have a jumper backref go ahead and pop it off the stack.
               We'll need it possibly for two args below */
            jmp = vm_jumpstack_pop(x);
            if (!jmp)
            {
                if (t == OP_ELSE)
                    pd_error(x, "vm: else clause detected with no if clause");
                else if (t == OP_ENDLOOP)
                    pd_error(x, "vm: endloop detected with no startloop");
                else
                    pd_error(x, "vm: reference to non-existent instruction");
                return 0;
            }
                /* This is currently just for OP_ENDLOOP and OP_ELSE. So let's
                   also check that our backref is to the correct op */
            if (t == OP_ENDLOOP && jmp->t != OP_STARTLOOP)
            {
                pd_error(x, "vm: endloop detected with no startloop");
                return 0;
            }
            if (t == OP_ELSE && jmp->t != OP_IF)
            {
                pd_error(x, "vm: else clause detected with no if clause");
                return 0;
            }

            if (t == OP_ENDLOOP)
            {
                /* Get OP_STARTLOOP and OP_ENDLOOP to point at each other */
                w[offset+2].w_index = jmp->index;
                x->x_pipeline[jmp->index + 2].w_index = offset;
            }
            else if (t == OP_ELSE)
            {
                /* set a value for the corresponding OP_IF so it can quickly
                   check if it has an else clause */
                x->x_pipeline[jmp->index + 3].w_float = 1.;
                /* set a warpzone to one past ourselves for the OP_IF */
                x->x_pipeline[jmp->index + 2].w_index = offset + OPLEN;
            }
        }

            /* if, else, startloop and endloop store indices for jump points */
        if (vm_jumper_forwardref(t))
        {
            if (!vm_jumpstack_push(x, t, offset))
                return 0;
        }
    }
    else if (argc > 2)
    {
        if (argv[2].a_type == A_SYMBOL)
        {
            s = atom_getsymbolarg(2, argc, argv);
            /* should be one of our parameters. Get the index and set it. */
            if ((w[offset+2].w_index = vm_get_param_index(x, s)) == -1)
            {
                pd_error(x, "vm: unknown variable name %s", s->s_name);
                return 0;
            }
            flag ++;
        }
        else
            w[offset+2].w_float = atom_getfloatarg(2, argc, argv);
    }
    else w[offset+2].w_float = 0;

        /* arg 3 */
    if (argc > 3)
    {
        if (argv[3].a_type == A_SYMBOL)
        {
            s = atom_getsymbolarg(3, argc, argv);
            /* should be one of our parameters. Get the index and set it. */
            if ((w[offset+3].w_index = vm_get_param_index(x, s)) == -1)
            {
                pd_error(x, "vm: unknown variable name %s", s->s_name);
                return 0;
            }
            flag += 2;
        }
        else
            w[offset+3].w_float = atom_getfloatarg(3, argc, argv);
    }
    else w[offset+3].w_float = 0;

        /* Finally, let's add the flag to our t_optype:
           * flag == 1 means we have one parameter as an arg
           * flag == 2 means both args are parameters

           We then just hard code all these as cases rather than checking
           what the type is.
        */

if (t == OP_ADD) fprintf(stderr, "+ flag is %d\n", flag);
    t += flag;
    w[offset].w_index = t;
    return 1;
}

static int vm_allocate_opcode(t_vm *x, int argc, t_atom *argv)
{
        /* First check for the "endif" "psueudo opcode" that doesn't need to
           allocate anything. Instead, it just does some jump point bookkeeping
           for the branching ops. */
    if (vm_check_for_endif(x, argc, argv))
    {
        t_jumpcode *jmp = vm_jumpstack_pop(x);
            /* need previous branching op on stack... otherwise it's an error */
        if (!jmp)
        {
            pd_error(x, "vm: 'endif' needs a previous 'if' statement");
            goto badop;
        }
            /* op should be an if or an else */
        if (jmp->t != OP_IF && jmp->t != OP_ELSE)
        {
            pd_error(x, "vm: 'endif' found in the middle of a loop");
            goto badop;
        }

            /* Note-- we can set to x->x_nops because we're always setting
               a return op as the final opcode in the pipeline. */
        vm_op_set_warpzone(&x->x_pipeline[jmp->index], x->x_nops);
    }
    else
    {
        x->x_nops += 1;
            /* if we're the first op let's allocate using getbytes */
        if (x->x_nops < 2)
            x->x_pipeline = t_getbytes(sizeof(t_word) * OPLEN);
        else
            x->x_pipeline = t_resizebytes(x->x_pipeline,
                sizeof(t_word) * OPLEN * (x->x_nops - 1),
                sizeof(t_word) * OPLEN * x->x_nops);

        if (!vm_add_opcode(x, argc, argv))
        {
            goto badop;
        }
    }
    return 1;
badop: 
    t_freebytes(x->x_pipeline, sizeof(t_word) * OPLEN * x->x_nops);
    return 0;
}

static int vm_allocate_pipeline(t_vm *x, int argc, t_atom *argv)
{
    int head = 0, terminator = 0;
    t_symbol *defaultparamsym;
    x->x_nops = 0;

post("in allocate_pipeline args is:");
postatom(argc, argv);
post("argc is %d", argc);

    while (argc > 0)
    {
        int c = vm_get_submessage(gensym(";"), argc, argv + head, &terminator);
        if (head == 0 && !x->x_gotparams)
        {
post("here we are in vm_allocate_pipeline!");
            if (!vm_set_params(x, c, argv + head))
                return 0;
        }
        else if (!vm_allocate_opcode(x, c, argv + head))
        {
            return 0;
        }
        /* Success! */
        head = c + terminator;
        argc -= head;
    }

        /* Add a final OP to return the value of the first param by the end
           of the pipeline */
    defaultparamsym = x->x_params[0].p_name;
    t_atom at[2];
    SETSYMBOL(at, gensym("return"));
    SETSYMBOL(at+1, defaultparamsym);
    if (!vm_allocate_opcode(x, 2, at))
    {
        return 0;
    }

    return 1;
}

static void vm_free(t_vm *x)
{
    vm_free_parser(x);
}

static void vm_create_inlets(t_fn *x)
{
    int i, n = x->x_nparams;
    /* Create an inlet for all except the first parameter which gets
       handled by the main class inlet. */
    for (i = 0; i < n; i++)
    {
        if (i) floatinlet_new(&x->x_obj, &(x->x_params[i].p_w.w_float));
    }
}

static void *fn_new(t_symbol *s, int argc, t_atom *argv)
{
post("in new, argc is %d", argc);
    t_fn *x = (t_fn *)pd_new(vm_class);
    int irmode;

    /* check first arg to see if we're in ir mode */
    if (atom_getsymbolarg(0, argc, argv) == gensym("-ir"))
    {
        irmode = 1;
        argc--, argv++;
    }
    else
        irmode = 0;

    x->x_jumpstack_nitems = 0;
    x->x_nparams = 0;
    x->x_gotparams = 0;

    /* If we're not in irmode, go ahead and run the args through
       the pratt parser to get the ir. */
    if (!irmode)
    {
        vm_init_parser(x);
        if (!vm_parse_messages(x, argc, argv))
        {
            pd_error(x, "vm: bad arguments");
            vm_free_parser(x);
            return (0);
        }
        else
        {
            argc = binbuf_getnatom(x->x_parser.p_irbuf);
            argv = binbuf_getvec(x->x_parser.p_irbuf);
        }
    }

    /* Now create the pipeline using our ir */
    if (!vm_allocate_pipeline(x, argc, argv))
    {
        pd_error(x, "vm: bad arguments");
        return (0);
    }
    
    /* Create our inlets. We use an array so no need to allocate */
    vm_create_inlets(x);
//    floatinlet_new(&x->x_obj, &x->x_in);
    outlet_new(&x->x_obj, &s_float);
    x->x_out = outlet_new(&x->x_obj, &s_float);
    x->x_in = 0;
    x->x_last_op = NULL;
    return (x);
}

/* stolen from x_arithmetic.c */
static inline t_float vm_div_op(t_float f1, t_float f2)
{
    int n1 = (int)f1, n2 = (int)f2, result;
    if (n2 < 0) n2 = -n2;
    else if (!n2) n2 = 1;
    if (n1 < 0) n1 -= (n2-1);
    result = n1 / n2;
    return ((t_float)result);
}

/* stolen from x_arithmetic.c */
static inline t_float vm_mod_op(t_float f1, t_float f2)
{
    int n2 = (int)f2, result;
    if (n2 < 0) n2 = -n2;
    else if (!n2) n2 = 1;
    result = ((int)(f1)) % n2;
    if (result < 0) result += n2;
    return ((t_float)result);
}

static inline t_float vm_pow_op(t_vm *x, t_float f1, t_float f2)
{
    if (f1 >= 0)
        return (POW(f1, f2));
    else if (f2 <= -1 || f2 >= 1 || f2 == 0)
        return (POW(f1, f2));
    else {
        pd_error(x, "vm: pow: calculation resulted in a NaN");
        return 0;
    }
}

#define PARAM(i) x->x_params[p[i].w_index].p_w.w_float
#define CONST(i) p[i].w_float

static int vm_eval(t_vm *x)
{
    int coins = x->x_coins, spin = 1;
//    for (; coins && spin; x->x_last_op += 4, coins--)

    while(coins-- && spin)
    {
        t_word *p = x->x_last_op;
        t_optype t = p->w_index;
        switch(t)
        {
        case OP_ADD:    PARAM(1) = CONST(2) + CONST(3); break;
        case OP_ADD_V: PARAM(1) = PARAM(2) + CONST(3); break;
        case OP_ADD_CV: PARAM(1) = CONST(2) + PARAM(3); break;
        case OP_ADD_VV: PARAM(1) = PARAM(2) + PARAM(3); break;

        case OP_EQ:     PARAM(1) = CONST(2) == CONST(3); break;
        case OP_EQ_V:   PARAM(1) = PARAM(2) == CONST(3); break;
        case OP_EQ_CV:  PARAM(1) = CONST(2) == PARAM(3); break;
        case OP_EQ_VV:  PARAM(1) = PARAM(2) == PARAM(3); break;

        case OP_NE:     PARAM(1) = CONST(2) != CONST(3); break;
        case OP_NE_V:   PARAM(1) = PARAM(2) != CONST(3); break;
        case OP_NE_CV:  PARAM(1) = CONST(2) != PARAM(3); break;
        case OP_NE_VV:  PARAM(1) = PARAM(2) != PARAM(3); break;

        case OP_GT:     PARAM(1) = CONST(2) > CONST(3); break;
        case OP_GT_V:   PARAM(1) = PARAM(2) > CONST(3); break;
        case OP_GT_CV:  PARAM(1) = CONST(2) > PARAM(3); break;
        case OP_GT_VV:  PARAM(1) = PARAM(2) > PARAM(3); break;

        case OP_LT:     PARAM(1) = CONST(2) < CONST(3); break;
        case OP_LT_V:   PARAM(1) = PARAM(2) < CONST(3); break;
        case OP_LT_CV:  PARAM(1) = CONST(2) < PARAM(3); break;
        case OP_LT_VV:  PARAM(1) = PARAM(2) < PARAM(3); break;

        case OP_GE:     PARAM(1) = CONST(2) >= CONST(3); break;
        case OP_GE_V:   PARAM(1) = PARAM(2) >= CONST(3); break;
        case OP_GE_CV:  PARAM(1) = CONST(2) >= PARAM(3); break;
        case OP_GE_VV:  PARAM(1) = PARAM(2) >= PARAM(3); break;

        case OP_LE:     PARAM(1) = CONST(2) <= CONST(3); break;
        case OP_LE_V:   PARAM(1) = PARAM(2) <= CONST(3); break;
        case OP_LE_CV:  PARAM(1) = CONST(2) <= PARAM(3); break;
        case OP_LE_VV:  PARAM(1) = PARAM(2) <= PARAM(3); break;

        case OP_BA:     PARAM(1) = ((int)(CONST(2))) & ((int)(CONST(3))); break;
        case OP_BA_V:   PARAM(1) = ((int)(PARAM(2))) & ((int)(CONST(3))); break;
        case OP_BA_CV:  PARAM(1) = ((int)(CONST(2))) & ((int)(PARAM(3))); break;
        case OP_BA_VV:  PARAM(1) = ((int)(PARAM(2))) & ((int)(PARAM(3))); break;

        case OP_BO:     PARAM(1) = ((int)(CONST(2))) | ((int)(CONST(3))); break;
        case OP_BO_V:   PARAM(1) = ((int)(PARAM(2))) | ((int)(CONST(3))); break;
        case OP_BO_CV:  PARAM(1) = ((int)(CONST(2))) | ((int)(PARAM(3))); break;
        case OP_BO_VV:  PARAM(1) = ((int)(PARAM(2))) | ((int)(PARAM(3))); break;

        case OP_LA:     PARAM(1) = ((int)(CONST(2))) && ((int)(CONST(3)));
            break;
        case OP_LA_V:   PARAM(1) = ((int)(PARAM(2))) && ((int)(CONST(3)));
            break;
        case OP_LA_CV:  PARAM(1) = ((int)(CONST(2))) && ((int)(PARAM(3)));
            break;
        case OP_LA_VV:  PARAM(1) = ((int)(PARAM(2))) && ((int)(PARAM(3)));
            break;

        case OP_LO:     PARAM(1) = ((int)(CONST(2))) || ((int)(CONST(3)));
            break;
        case OP_LO_V:   PARAM(1) = ((int)(PARAM(2))) || ((int)(CONST(3)));
            break;
        case OP_LO_CV:  PARAM(1) = ((int)(CONST(2))) || ((int)(PARAM(3)));
            break;
        case OP_LO_VV:  PARAM(1) = ((int)(PARAM(2))) || ((int)(PARAM(3)));
            break;

        case OP_LS:     PARAM(1) = ((int)(CONST(2))) << ((int)(CONST(3)));
            break;
        case OP_LS_V:   PARAM(1) = ((int)(PARAM(2))) << ((int)(CONST(3)));
            break;
        case OP_LS_CV:  PARAM(1) = ((int)(CONST(2))) << ((int)(PARAM(3)));
            break;
        case OP_LS_VV:  PARAM(1) = ((int)(PARAM(2))) << ((int)(PARAM(3)));
            break;

        case OP_RS:     PARAM(1) = ((int)(CONST(2))) >> ((int)(CONST(3)));
            break;
        case OP_RS_V:   PARAM(1) = ((int)(PARAM(2))) >> ((int)(CONST(3)));
            break;
        case OP_RS_CV:  PARAM(1) = ((int)(CONST(2))) >> ((int)(PARAM(3)));
            break;
        case OP_RS_VV:  PARAM(1) = ((int)(PARAM(2))) >> ((int)(PARAM(3)));
            break;

        case OP_SUB:    PARAM(1) = CONST(2) - CONST(3); break;
        case OP_SUB_V:  PARAM(1) = PARAM(2) - CONST(3); break;
        case OP_SUB_CV: PARAM(1) = CONST(2) - PARAM(3); break;
        case OP_SUB_VV: PARAM(1) = PARAM(2) - PARAM(3); break;

        case OP_MUL:    PARAM(1) = CONST(2) * CONST(3); break;
        case OP_MUL_V:  PARAM(1) = PARAM(2) * CONST(3); break;
        case OP_MUL_CV: PARAM(1) = CONST(2) * PARAM(3); break;
        case OP_MUL_VV: PARAM(1) = PARAM(2) * PARAM(3); break;

        case OP_OVR:    PARAM(1) = CONST(3) != 0 ? CONST(2) / CONST(3) : 0;
            break;
        case OP_OVR_V:  PARAM(1) = PARAM(2) != 0 ? PARAM(2) / CONST(3) : 0;
            break;
        case OP_OVR_CV: PARAM(1) = CONST(2) != 0 ? CONST(2) / PARAM(3) : 0;
            break;
        case OP_OVR_VV: PARAM(1) = PARAM(2) != 0 ? PARAM(2) / PARAM(3) : 0;
            break;

        case OP_DIV:    PARAM(1) = vm_div_op(CONST(2), CONST(3)); break;
        case OP_DIV_V:  PARAM(1) = vm_div_op(PARAM(2), CONST(3)); break;
        case OP_DIV_CV: PARAM(1) = vm_div_op(CONST(2), PARAM(3)); break;
        case OP_DIV_VV: PARAM(1) = vm_div_op(PARAM(2), PARAM(3)); break;

        case OP_MOD:    PARAM(1) = vm_mod_op(CONST(2), CONST(3)); break;
        case OP_MOD_V:  PARAM(1) = vm_mod_op(PARAM(2), CONST(3)); break;
        case OP_MOD_CV: PARAM(1) = vm_mod_op(CONST(2), PARAM(3)); break;
        case OP_MOD_VV: PARAM(1) = vm_mod_op(PARAM(2), PARAM(3)); break;

        case OP_FMOD:    PARAM(1) = FMOD(CONST(2), CONST(3)); break;
        case OP_FMOD_V:  PARAM(1) = FMOD(PARAM(2), CONST(3)); break;
        case OP_FMOD_CV: PARAM(1) = FMOD(CONST(2), PARAM(3)); break;
        case OP_FMOD_VV: PARAM(1) = FMOD(PARAM(2), PARAM(3)); break;

        case OP_POW:    PARAM(1) = vm_pow_op(x, CONST(2), CONST(3)); break;
        case OP_POW_V:  PARAM(1) = vm_pow_op(x, PARAM(2), CONST(3)); break;
        case OP_POW_CV: PARAM(1) = vm_pow_op(x, CONST(2), PARAM(3)); break;
        case OP_POW_VV: PARAM(1) = vm_pow_op(x, PARAM(2), PARAM(3)); break;

        case OP_PC:    PARAM(1) = ((int)(CONST(2))) %
            (((int)CONST(3)) ? ((int)CONST(3)) : 1);
            break;
        case OP_PC_V:  PARAM(1) = ((int)(PARAM(2))) %
            (((int)CONST(3)) ? ((int)CONST(3)) : 1);
            break;
        case OP_PC_CV: PARAM(1) = ((int)(CONST(2))) %
            (((int)PARAM(3)) ? ((int)PARAM(3)) : 1);
            break;
        case OP_PC_VV: PARAM(1) = ((int)(PARAM(2))) %
            (((int)PARAM(3)) ? ((int)PARAM(3)) : 1);
            break;

        case OP_MAX:    PARAM(1) = CONST(2) > CONST(3) ? CONST(2) : CONST(3);
            break;
        case OP_MAX_V:  PARAM(1) = PARAM(2) > CONST(3) ? PARAM(2) : CONST(3);
            break;
        case OP_MAX_CV: PARAM(1) = CONST(2) > PARAM(3) ? CONST(2) : PARAM(3);
            break;
        case OP_MAX_VV: PARAM(1) = PARAM(2) > PARAM(3) ? PARAM(2) : PARAM(3);
            break;

        case OP_MIN:    PARAM(1) = CONST(2) < CONST(3) ? CONST(2) : CONST(3);
            break;
        case OP_MIN_V:  PARAM(1) = PARAM(2) < CONST(3) ? PARAM(2) : CONST(3);
            break;
        case OP_MIN_CV:  PARAM(1) = CONST(2) < PARAM(3) ? CONST(2) : PARAM(3);
            break;
        case OP_MIN_VV: PARAM(1) = PARAM(2) < PARAM(3) ? PARAM(2) : PARAM(3);
            break;

        case OP_STARTLOOP:
            if (CONST(1) > 0)
            {
                /* copy our looping value to the count parameter of OP_ENDLOOP
                   below. Since we're eating an iteration here we subtract 
                   one... */
                x->x_pipeline[p[2].w_index + 3].w_float = CONST(1) - 1;
                break;
            }
            else
            {
                /* Jump to the opcode past our matching OP_ENDLOOP */
                x->x_last_op = x->x_pipeline + p[2].w_index + OPLEN;
                continue;
            }
            break;
        case OP_STARTLOOP_V:
            if (PARAM(1) > 0)
            {
                x->x_pipeline[p[2].w_index + 3].w_float = PARAM(1) - 1;
                break;
            }
            else
            {
                /* Jump to the opcode past our matching OP_ENDLOOP */
                x->x_last_op = x->x_pipeline + p[2].w_index + OPLEN;
                continue;
            }
            break;

        case OP_ENDLOOP:
        case OP_ENDLOOP_V:
            if (CONST(3) > 0)
            {
                /* jump to the first opcode past the original startloop */
                x->x_last_op = x->x_pipeline + p[2].w_index + OPLEN;
                CONST(3) -= 1.;
                continue;
            }
            else break;
        case OP_IF:
        case OP_ELSE:
            if (CONST(1))
            {
                /* check flag for else clause. This is sometimes set for
                   OP_IF but never set for OP_ELSE */
                if (((int)(p[3].w_float)) != 0)
                {
                    /* if so store the negation of our condition down there */
                    x->x_pipeline[p[2].w_index + 2].w_float = !CONST(1);
                }
                break;
            }
            else
            {
                /* Jump to the first op of our corresponding else clause
                   or outside our if clause body*/
                x->x_last_op = x->x_pipeline + p[2].w_index;
                continue;
            }
            break;
        case OP_IF_V:
        case OP_ELSE_V:
            if (PARAM(1))
            {
                /* check flag for else clause */
                if (((int)(p[3].w_float)) != 0)
                {
                    /* if so store the negation of our condition down there */
                    x->x_pipeline[p[2].w_index + 1].w_float = !PARAM(1);
                }
                break;
            }
            else
            {
                /* Jump to the first op of our corresponding else clause
                   or outside our if clause body*/
                x->x_last_op = x->x_pipeline + p[2].w_index;
                continue;
            }
            break;

        case OP_RETURN: x->x_ret = PARAM(1); spin = 0; break;
        default: break;
        }
        /* Skip to the next opcode. We can't do this in the for loop
           because some of our ops jump around to other ops. */
        x->x_last_op += 4;
    }
    x->x_coins = coins;
    if (spin) return 0;
    else
    {
        x->x_last_op = NULL;
        return 1;
    }
}

static void vm_float(t_vm *x, t_float f)
{
    x->x_coins = (int)f;
    /* If we're not in the middle of computation then set the initial state
       to the value of the secondary inlet. */
    if (!x->x_last_op)
    {
        x->x_params[0].p_w.w_float = x->x_in;
        x->x_last_op = x->x_pipeline;
    }
    if (vm_eval(x))
        outlet_float(x->x_out, x->x_ret);
    outlet_float(x->x_obj.ob_outlet, (t_float)x->x_coins);
}

static void vm_debug(t_vm *x)
{
    int i;
    post("vm debug");
    for (i = 0; i < x->x_nops; i++)
    {
        int z = x->x_pipeline[i*4].w_index;
        post("%s", string_from_op(z));
    }
}

void vm_setup(void)
{
    vm_class = class_new(gensym("vm"), (t_newmethod)fn_new,
        (t_method)vm_free, sizeof(t_vm), 0, A_GIMME, 0);
    class_addfloat(vm_class, vm_float);
    class_addmethod(vm_class, (t_method)vm_debug, gensym("debug"), 0);
}
