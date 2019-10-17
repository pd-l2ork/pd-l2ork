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
#endif

/* ------------------------ vm ------------------------ */

static t_class *vm_class;

/* the ops */

typedef enum
{
    OP_NULL,
    OP_ADD,
    OP_ADD_V,
    OP_ADD_VV,
    OP_ATAN,
    OP_ATAN_V,
    OP_ATAN_VV,
    OP_ATAN2,
    OP_ATAN2_V,
    OP_ATAN2_VV,
    OP_BA,
    OP_BA_V,
    OP_BA_VV,
    OP_BO,
    OP_BO_V,
    OP_BO_VV,
    OP_COS,
    OP_COS_V,
    OP_COS_VV,
    OP_DIV,
    OP_DIV_V,
    OP_DIV_VV,
    OP_EQ,
    OP_EQ_V,
    OP_EQ_VV,
    OP_EXP,
    OP_EXP_V,
    OP_EXP_VV,
    OP_FABS,
    OP_FABS_V,
    OP_FABS_VV,
    OP_GE,
    OP_GE_V,
    OP_GE_VV,
    OP_GT,
    OP_GT_V,
    OP_GT_VV,
    OP_LA,
    OP_LA_V,
    OP_LA_VV,
    OP_LE,
    OP_LE_V,
    OP_LE_VV,
    OP_LO,
    OP_LO_V,
    OP_LO_VV,
    OP_LOG,
    OP_LOG_V,
    OP_LOG_VV,
    OP_LS,
    OP_LS_V,
    OP_LS_VV,
    OP_LT,
    OP_LT_V,
    OP_LT_VV,
    OP_MAX,
    OP_MAX_V,
    OP_MAX_VV,
    OP_MIN,
    OP_MIN_V,
    OP_MIN_VV,
    OP_MOD,
    OP_MOD_V,
    OP_MOD_VV,
    OP_MUL,
    OP_MUL_V,
    OP_MUL_VV,
    OP_NE,
    OP_NE_V,
    OP_NE_VV,
    OP_OVR,
    OP_OVR_V,
    OP_OVR_VV,
    OP_PC,
    OP_PC_V,
    OP_PC_VV,
    OP_POW,
    OP_POW_V,
    OP_POW_VV,
    OP_RS,
    OP_RS_V,
    OP_RS_VV,
    OP_SIN,
    OP_SIN_V,
    OP_SIN_VV,
    OP_SQRT,
    OP_SQRT_V,
    OP_SQRT_VV,
    OP_SUB,
    OP_SUB_V,
    OP_SUB_VV,
    OP_IF,
    OP_IF_V,
    OP_ELSE,
    OP_ELSE_V,
    OP_ENDIF,
    OP_STARTLOOP,
    OP_STARTLOOP_V,
    OP_ENDLOOP,
    OP_ENDLOOP_V,
    OP_RETURN
} t_optype;

/* let's see if we can get away with all ops being the same size */
#define OPLEN 4

#define MAX_LOOPSTACK_LEN 4

typedef struct _param
{
    t_symbol *p_name;
    t_word p_w;
} t_param;

typedef struct _jumpcode
{
    t_optype t;
    int index;
    int popme; /* hack to know if we're ready to get popped off the stack */
} t_jumpcode;

#define JUMPSTACKSIZE 4

typedef struct _vm
{
    t_object x_obj;
    t_float x_in;
    t_param x_params[4];
    t_jumpcode x_jumpstack[JUMPSTACKSIZE];
    int x_jumpstack_nitems;
    int x_coins;
    t_word *x_pipeline;
    int x_nops;
    t_word *x_last_op;
    t_float x_ret;
    t_outlet *x_out;
} t_vm;

static t_optype vm_getop(t_symbol *sym)
{
    char *s = sym->s_name;
    if (!strcmp(s, "*")) return OP_MUL;
    else if (!strcmp(s, "/")) return OP_OVR;
    else if (!strcmp(s, "+")) return OP_ADD;
    else if (!strcmp(s, "-")) return OP_SUB;
    else if (!strcmp(s, ">")) return OP_GT;
    else if (!strcmp(s, "<")) return OP_LT;
    else if (!strcmp(s, "%")) return OP_PC;
    else if (!strcmp(s, "&")) return OP_BA;
    else if (!strcmp(s, "|")) return OP_BO;
    else if (!strcmp(s, "&&")) return OP_LA;
    else if (!strcmp(s, "||")) return OP_LO;
    else if (!strcmp(s, "==")) return OP_EQ;
    else if (!strcmp(s, "!=")) return OP_NE;
    else if (!strcmp(s, ">=")) return OP_GE;
    else if (!strcmp(s, ">>")) return OP_RS;
    else if (!strcmp(s, "<=")) return OP_LE;
    else if (!strcmp(s, "<<")) return OP_LS;
    else if (!strcmp(s, "atan")) return OP_ATAN;
    else if (!strcmp(s, "atan2")) return OP_ATAN2;
    else if (!strcmp(s, "cos")) return OP_COS;
    else if (!strcmp(s, "div")) return OP_DIV;
    else if (!strcmp(s, "exp")) return OP_EXP;
    else if (!strcmp(s, "fabs")) return OP_FABS;
    else if (!strcmp(s, "log")) return OP_LOG;
    else if (!strcmp(s, "max")) return OP_MAX;
    else if (!strcmp(s, "min")) return OP_MIN;
    else if (!strcmp(s, "mod")) return OP_MOD;
    else if (!strcmp(s, "pow")) return OP_POW;
    else if (!strcmp(s, "sin")) return OP_SIN;
    else if (!strcmp(s, "sqrt")) return OP_SQRT;
    else if (!strcmp(s, "startloop")) return OP_STARTLOOP;
    else if (!strcmp(s, "endloop")) return OP_ENDLOOP;
    else if (!strcmp(s, "if")) return OP_IF;
    else if (!strcmp(s, "else")) return OP_ELSE;
    else if (!strcmp(s, "endif")) return OP_ENDIF;
    else if (!strcmp(s, "return")) return OP_RETURN;
    else return OP_NULL;
}

static int vm_get_param_index(t_vm *x, t_symbol *s)
{
    int i;
    for (i = 0; i < OPLEN; i++)
    {
        if (x->x_params[i].p_name == s) return i;
    }
    return -1;
}

static int vm_set_params(t_vm *x, int argc, t_atom *argv)
{
    t_symbol *name;

fprintf(stderr, "did this\n");
    if (argc)
    {
        name = atom_getsymbol(argv);
        if (vm_getop(name) != OP_NULL) goto badparam;
        x->x_params[0].p_name = name;
    }
    else x->x_params[0].p_name = &s_;
    x->x_params[0].p_w.w_float = 0;

    if (argc >= 1)
    {
        name = atom_getsymbol(++argv);
        if (vm_getop(name) != OP_NULL) goto badparam;
        x->x_params[1].p_name = name;
    }
    else x->x_params[1].p_name = &s_;
    x->x_params[1].p_w.w_float = 0.;

    if (argc >= 2)
    {
        name = atom_getsymbol(++argv);
        if (vm_getop(name) != OP_NULL) goto badparam;
        x->x_params[2].p_name = name;
    }
    else x->x_params[2].p_name = &s_;
    x->x_params[2].p_w.w_float = 0.;

    if (argc >= 3)
    {
        name = atom_getsymbol(++argv);
        if (vm_getop(name) != OP_NULL) goto badparam;
        x->x_params[3].p_name = name;
    }
    else x->x_params[3].p_name = &s_;
    x->x_params[3].p_w.w_float = 0.;

    return 1;

badparam:
    pd_error(x, "vm: parameter name %s cannot be an operator", name->s_name);
    return 0;
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
        pd_error(x, "vm: can't pop a jump opcode off the stack");
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
    t_optype t = vm_getop(op);
    if (t == OP_NULL) return 0;
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
        if (argc && argv->a_type == A_FLOAT)
            w[offset+1].w_float = argv->a_w.w_float;
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
            if (!jmp) return 0;
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
                x->x_pipeline[offset + 2].w_index = offset;
            }
            else if (t == OP_ELSE)
            {
                /* set a value so the OP_IF can quickly check if it has an
                   else clause */
                x->x_pipeline[offset + 3].w_float = 1.;
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
            flag++;
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
            flag++;
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
    t += flag;

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
        return 1;
    }
badop: 
    t_freebytes(x->x_pipeline, sizeof(t_word) * OPLEN * x->x_nops);
    return 0;
}

static int vm_allocate_pipeline(t_vm *x, int argc, t_atom *argv)
{
    int i;
    x->x_nops = 0;
    int offset = 0;
    int got_params = 0;
    t_symbol *defaultparamsym;

        /* Go ahead and initialize the params to zero... */
    vm_set_params(x, 0, NULL);

    for (i = 0; i < argc; i++)
    {
        if (atom_getsymbol(argv+i) == gensym(";") ||
            i == argc - 1)
        {
            /* if we got a final message with no trailing semi, parse it
               anyway */
            if (i == argc - 1) i += 1;
            /* got a new op */
            int new_argc = i - offset;
            /* let's skip double semis */
            if (!new_argc) continue;

            /* on the first time through we grab the header which contains
               our parameters */
            if (got_params == 0)
            {
                if (!vm_set_params(x, new_argc, argv + offset))
                    return 0;
                got_params = 1;
            }
            else if (!vm_allocate_opcode(x, new_argc, argv + offset))
            {
                return 0;
            }
            /* Success! */
            offset = i + 1;
        }
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

fprintf(stderr, "done allocating. nops is %d\n", x->x_nops);
    return 1;
}

static void *vm_new(t_symbol *s, int argc, t_atom *argv)
{
    t_vm *x = (t_vm *)pd_new(vm_class);
    x->x_jumpstack_nitems = 0;
    if (!vm_allocate_pipeline(x, argc, argv))
    {
        pd_error(x, "vm: bad arguments");
        return (0);
    }
    floatinlet_new(&x->x_obj, &x->x_in);
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

static int vm_compute(t_vm *x)
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
        case OP_ADD_V:  PARAM(1) = PARAM(2) + CONST(3); break;
        case OP_ADD_VV: PARAM(1) = PARAM(2) + PARAM(3); break;

        case OP_EQ:     PARAM(1) = CONST(2) == CONST(3); break;
        case OP_EQ_V:   PARAM(1) = PARAM(2) == CONST(3); break;
        case OP_EQ_VV:  PARAM(1) = PARAM(2) == PARAM(3); break;

        case OP_NE:     PARAM(1) = CONST(2) != CONST(3); break;
        case OP_NE_V:   PARAM(1) = PARAM(2) != CONST(3); break;
        case OP_NE_VV:  PARAM(1) = PARAM(2) != PARAM(3); break;

        case OP_GT:     PARAM(1) = CONST(2) > CONST(3); break;
        case OP_GT_V:   PARAM(1) = PARAM(2) > CONST(3); break;
        case OP_GT_VV:  PARAM(1) = PARAM(2) > PARAM(3); break;

        case OP_LT:     PARAM(1) = CONST(2) < CONST(3); break;
        case OP_LT_V:   PARAM(1) = PARAM(2) < CONST(3); break;
        case OP_LT_VV:  PARAM(1) = PARAM(2) < PARAM(3); break;

        case OP_GE:     PARAM(1) = CONST(2) >= CONST(3); break;
        case OP_GE_V:   PARAM(1) = PARAM(2) >= CONST(3); break;
        case OP_GE_VV:  PARAM(1) = PARAM(2) >= PARAM(3); break;

        case OP_LE:     PARAM(1) = CONST(2) <= CONST(3); break;
        case OP_LE_V:   PARAM(1) = PARAM(2) <= CONST(3); break;
        case OP_LE_VV:  PARAM(1) = PARAM(2) <= PARAM(3); break;

        case OP_BA:     PARAM(1) = ((int)(CONST(2))) & ((int)(CONST(3))); break;
        case OP_BA_V:   PARAM(1) = ((int)(PARAM(2))) & ((int)(CONST(3))); break;
        case OP_BA_VV:  PARAM(1) = ((int)(PARAM(2))) & ((int)(PARAM(3))); break;

        case OP_BO:     PARAM(1) = ((int)(CONST(2))) | ((int)(CONST(3))); break;
        case OP_BO_V:   PARAM(1) = ((int)(PARAM(2))) | ((int)(CONST(3))); break;
        case OP_BO_VV:  PARAM(1) = ((int)(PARAM(2))) | ((int)(PARAM(3))); break;

        case OP_LA:     PARAM(1) = ((int)(CONST(2))) && ((int)(CONST(3)));
            break;
        case OP_LA_V:   PARAM(1) = ((int)(PARAM(2))) && ((int)(CONST(3)));
            break;
        case OP_LA_VV:  PARAM(1) = ((int)(PARAM(2))) && ((int)(PARAM(3)));
            break;

        case OP_LO:     PARAM(1) = ((int)(CONST(2))) || ((int)(CONST(3)));
            break;
        case OP_LO_V:   PARAM(1) = ((int)(PARAM(2))) || ((int)(CONST(3)));
            break;
        case OP_LO_VV:  PARAM(1) = ((int)(PARAM(2))) || ((int)(PARAM(3)));
            break;

        case OP_LS:     PARAM(1) = ((int)(CONST(2))) << ((int)(CONST(3)));
            break;
        case OP_LS_V:   PARAM(1) = ((int)(PARAM(2))) << ((int)(CONST(3)));
            break;
        case OP_LS_VV:  PARAM(1) = ((int)(PARAM(2))) << ((int)(PARAM(3)));
            break;

        case OP_RS:     PARAM(1) = ((int)(CONST(2))) >> ((int)(CONST(3)));
            break;
        case OP_RS_V:   PARAM(1) = ((int)(PARAM(2))) >> ((int)(CONST(3)));
            break;
        case OP_RS_VV:  PARAM(1) = ((int)(PARAM(2))) >> ((int)(PARAM(3)));
            break;

        case OP_SUB:    PARAM(1) = CONST(2) - CONST(3); break;
        case OP_SUB_V:  PARAM(1) = PARAM(2) - CONST(3); break;
        case OP_SUB_VV: PARAM(1) = PARAM(2) - PARAM(3); break;

        case OP_MUL:    PARAM(1) = CONST(2) * CONST(3); break;
        case OP_MUL_V:  PARAM(1) = PARAM(2) * CONST(3); break;
        case OP_MUL_VV: PARAM(1) = PARAM(2) * PARAM(3); break;

        case OP_OVR:    PARAM(1) = CONST(3) != 0 ? CONST(2) / CONST(3) : 0;
            break;
        case OP_OVR_V:  PARAM(1) = PARAM(2) != 0 ? PARAM(2) / CONST(3) : 0;
            break;
        case OP_OVR_VV: PARAM(1) = PARAM(2) != 0 ? PARAM(2) / PARAM(3) : 0;
            break;

        case OP_DIV:    PARAM(1) = vm_div_op(CONST(2), CONST(3)); break;
        case OP_DIV_V:  PARAM(1) = vm_div_op(PARAM(2), CONST(3)); break;
        case OP_DIV_VV: PARAM(1) = vm_div_op(PARAM(2), PARAM(3)); break;

        case OP_MOD:    PARAM(1) = vm_mod_op(CONST(2), CONST(3)); break;
        case OP_MOD_V:  PARAM(1) = vm_mod_op(PARAM(2), CONST(3)); break;
        case OP_MOD_VV: PARAM(1) = vm_mod_op(PARAM(2), PARAM(3)); break;

        case OP_POW:    PARAM(1) = vm_pow_op(x, CONST(2), CONST(3)); break;
        case OP_POW_V:  PARAM(1) = vm_pow_op(x, PARAM(2), CONST(3)); break;
        case OP_POW_VV: PARAM(1) = vm_pow_op(x, PARAM(2), PARAM(3)); break;

        case OP_PC:    PARAM(1) = ((int)(CONST(2))) %
            (((int)CONST(3)) ? ((int)CONST(3)) : 1);
            break;
        case OP_PC_V:  PARAM(1) = ((int)(CONST(2))) %
            (((int)CONST(3)) ? ((int)CONST(3)) : 1);
            break;
        case OP_PC_VV: PARAM(1) = ((int)(CONST(2))) %
            (((int)CONST(3)) ? ((int)CONST(3)) : 1);
            break;

        case OP_MAX:    PARAM(1) = CONST(2) > CONST(3) ? CONST(2) : CONST(3);
            break;
        case OP_MAX_V:  PARAM(1) = PARAM(2) > CONST(3) ? PARAM(2) : CONST(3);
            break;
        case OP_MAX_VV: PARAM(1) = PARAM(2) > PARAM(3) ? PARAM(2) : PARAM(3);
            break;

        case OP_MIN:    PARAM(1) = CONST(2) < CONST(3) ? CONST(2) : CONST(3);
            break;
        case OP_MIN_V:  PARAM(1) = PARAM(2) < CONST(3) ? PARAM(2) : CONST(3);
            break;
        case OP_MIN_VV: PARAM(1) = PARAM(2) < PARAM(3) ? PARAM(2) : PARAM(3);
            break;

        case OP_STARTLOOP:
            if (CONST(1) > 0)
            {
                x->x_pipeline[p[2].w_index + 3].w_float = CONST(1);
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
                x->x_pipeline[p[2].w_index + 3].w_float = PARAM(1);
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
                x->x_last_op = x->x_pipeline + p[2].w_index;
                CONST(3) -= 1.;
                continue;

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
                    x->x_pipeline[p[2].w_index + 2].w_float = !PARAM(1);
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
    if (vm_compute(x))
        outlet_float(x->x_out, x->x_ret);
    outlet_float(x->x_obj.ob_outlet, (t_float)x->x_coins);
}

void vm_setup(void)
{
    vm_class = class_new(gensym("vm"), (t_newmethod)vm_new, 0,
        sizeof(t_vm), 0, A_GIMME, 0);
    class_addfloat(vm_class, vm_float);
}
