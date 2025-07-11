/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* connective objects */

#include "m_pd.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* -------------------------- int ------------------------------ */
static t_class *pdint_class;

typedef struct _pdint
{
    t_object x_obj;
    t_float x_f;
} t_pdint;

static void *pdint_new(t_floatarg f)
{
    t_pdint *x = (t_pdint *)pd_new(pdint_class);
    x->x_f = f;
    outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f);
    return (x);
}

static void pdint_bang(t_pdint *x)
{
    outlet_float(x->x_obj.ob_outlet, (t_float)(int64_t)(x->x_f));
}

static void pdint_float(t_pdint *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, (t_float)(int64_t)(x->x_f = f));
}

static void pdint_send(t_pdint *x, t_symbol *s)
{
    if (s->s_thing)
        pd_float(s->s_thing, (t_float)(int64_t)x->x_f);
    else pd_error(x, "%s: no such object", s->s_name);
}


void pdint_setup(void)
{
    pdint_class = class_new(gensym("int"), (t_newmethod)pdint_new, 0,
        sizeof(t_pdint), 0, A_DEFFLOAT, 0);
    class_addcreator((t_newmethod)pdint_new, gensym("i"), A_DEFFLOAT, 0);
    class_addmethod(pdint_class, (t_method)pdint_send, gensym("send"),
        A_SYMBOL, 0);
    class_addbang(pdint_class, pdint_bang);
    class_addfloat(pdint_class, pdint_float);
}

/* -------------------------- float ------------------------------ */
static t_class *pdfloat_class;

typedef struct _pdfloat
{
    t_object x_obj;
    t_float x_f;
} t_pdfloat;

    /* "float," "symbol," and "bang" are special because
    they're created by short-circuited messages to the "new"
    object which are handled specially in pd_typedmess(). */

void *pdfloat_new(t_pd *dummy, t_float f)
{
    t_pdfloat *x = (t_pdfloat *)pd_new(pdfloat_class);
    x->x_f = f;
    outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f);
    pd_this->pd_newest = &x->x_obj.ob_pd;
    return (x);
}

static void *pdfloat_new2(t_floatarg f)
{
    return (pdfloat_new(0, f));
}

static void pdfloat_bang(t_pdfloat *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f);
}

static void pdfloat_float(t_pdfloat *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f = f);
}

    /* check if a symbol payload can be interpreted as a floating point number.
       We get these cases sometimes from [makefilename], [keyname], and some
       externals that shoot out data that would be parsed as a float if loaded
       from a file.

       return values are: 0 not a number
                          1 successfully parsed as float (stored in f)

       if the number would underflow f is set to -1
       if the number would overflow f is set to greater than zero
    */
int symbol_can_float(t_symbol *s, t_float *f)
{
        int ret;
        char c, *str_end;
        c = s->s_name[0];
        if (c != '-' && c != '+' && (c < 48 || c > 57)) return 0;
        errno = 0;
        *f = strtod(s->s_name, &str_end);
        if (errno == ERANGE)
        {
            ret = 0;
            if (*f == 0) *f = -1; /* underflow */
            else *f = 1; /* assume overflow otherwise */
        }
        else if (*f == 0 && s->s_name == str_end)
        {
            ret = 0;
        }
        else
            ret = 1;
        return ret;
}

char *type_hint(t_symbol *s, int argc, t_atom *argv, int check_symforfloat);

static void pdfloat_symbol(t_pdfloat *x, t_symbol *s)
{
        t_float f = 0;
        if (symbol_can_float(s, &f))
            outlet_float(x->x_obj.ob_outlet, x->x_f = f);
        else
        {
            t_atom at;
            SETSYMBOL(&at, s);
            pd_error(x, "couldn't convert 'symbol %s' to float%s",
                s->s_name,
                f == -1 ? " (number too small)" :
                    f == 1 ? " (number too large)" :
                        type_hint(&s_symbol, 1, &at, 0));
        }
}

static void pdfloat_send(t_pdfloat *x, t_symbol *s)
{
    if (s->s_thing)
        pd_float(s->s_thing, x->x_f);
    else pd_error(x, "%s: no such object", s->s_name);
}

void pdfloat_setup(void)
{
    pdfloat_class = class_new(gensym("float"), (t_newmethod)pdfloat_new, 0,
        sizeof(t_pdfloat), 0, A_FLOAT, 0);
    class_addcreator((t_newmethod)pdfloat_new2, gensym("f"), A_DEFFLOAT, 0);
    class_addmethod(pdfloat_class, (t_method)pdfloat_send, gensym("send"),
        A_SYMBOL, 0);
    class_addbang(pdfloat_class, pdfloat_bang);
    class_addfloat(pdfloat_class, (t_method)pdfloat_float);
    class_addsymbol(pdfloat_class, (t_method)pdfloat_symbol);
}

/* -------------------------- symbol ------------------------------ */
static t_class *pdsymbol_class;

typedef struct _pdsymbol
{
    t_object x_obj;
    t_symbol *x_s;
} t_pdsymbol;

void *pdsymbol_new(t_pd *dummy, t_symbol *s)
{
    t_pdsymbol *x = (t_pdsymbol *)pd_new(pdsymbol_class);
    x->x_s = s;
    outlet_new(&x->x_obj, &s_symbol);
    symbolinlet_new(&x->x_obj, &x->x_s);
    pd_this->pd_newest = &x->x_obj.ob_pd;
    return (x);
}

static void pdsymbol_bang(t_pdsymbol *x)
{
    outlet_symbol(x->x_obj.ob_outlet, x->x_s);
}

static void pdsymbol_symbol(t_pdsymbol *x, t_symbol *s)
{
    outlet_symbol(x->x_obj.ob_outlet, x->x_s = s);
}

static void pdsymbol_anything(t_pdsymbol *x, t_symbol *s, int ac, t_atom *av)
{
    outlet_symbol(x->x_obj.ob_outlet, x->x_s = s);
}

    /* For "list" message don't just output "list"; if empty, we want to
    bang the symbol and if it starts with a symbol, we output that.
    Otherwise it's not clear what we should do so we just go for the
    "anything" method.  LATER figure out if there are other places where
    empty lists aren't equivalent to "bang"???  Should Pd's message passer
    always check and call the more specific method, or should it be the 
    object's responsibility?  Dunno... */
    /* Currently this function isn't used... */
/*
static void pdsymbol_list(t_pdsymbol *x, t_symbol *s, int ac, t_atom *av)
{
    if (!ac)
        pdsymbol_bang(x);
    else if (av->a_type == A_SYMBOL)
        pdsymbol_symbol(x, av->a_w.w_symbol);
    else pdsymbol_anything(x, s, ac, av);
}
*/

void pdsymbol_setup(void)
{
    pdsymbol_class = class_new(gensym("symbol"), (t_newmethod)pdsymbol_new, 0,
        sizeof(t_pdsymbol), 0, A_SYMBOL, 0);
    class_addbang(pdsymbol_class, pdsymbol_bang);
    class_addsymbol(pdsymbol_class, pdsymbol_symbol);
    class_addanything(pdsymbol_class, pdsymbol_anything);
}

/* -------------------------- bang ------------------------------ */
static t_class *bang_class;

typedef struct _bang
{
    t_object x_obj;
} t_bang;

void *bang_new(t_pd *dummy)
{
    t_bang *x = (t_bang *)pd_new(bang_class);
    outlet_new(&x->x_obj, &s_bang);
    pd_this->pd_newest = &x->x_obj.ob_pd;
    return (x);
}

static void *bang_new2(void)
{
    return (bang_new(0));
}

static void bang_bang(t_bang *x)
{
    outlet_bang(x->x_obj.ob_outlet);
}

static void bang_float(t_bang *x, t_float f) {
    outlet_bang(x->x_obj.ob_outlet);
}

static void bang_symbol(t_bang *x, t_symbol *s) {
    outlet_bang(x->x_obj.ob_outlet);
}

static void bang_list(t_bang *x, t_symbol *s, int argc, t_atom *argv) {
    outlet_bang(x->x_obj.ob_outlet);
}

void bang_setup(void)
{
    bang_class = class_new(gensym("bang"), (t_newmethod)bang_new, 0,
        sizeof(t_bang), 0, 0);
    class_addcreator((t_newmethod)bang_new2, gensym("b"), 0);
    class_addbang(bang_class, bang_bang);
    class_addfloat(bang_class, bang_float);
    class_addsymbol(bang_class, bang_symbol);
    class_addlist(bang_class, bang_list);
    class_addanything(bang_class, bang_list);
}

/* -------------------- send ------------------------------ */

static t_class *send_class;

typedef struct _send
{
    t_object x_obj;
    t_symbol *x_sym;
} t_send;

static void send_bang(t_send *x)
{
    if (x->x_sym->s_thing) pd_bang(x->x_sym->s_thing);
}

static void send_float(t_send *x, t_float f)
{
    if (x->x_sym->s_thing) pd_float(x->x_sym->s_thing, f);
}

static void send_symbol(t_send *x, t_symbol *s)
{
    if (x->x_sym->s_thing) pd_symbol(x->x_sym->s_thing, s);
}

static void send_pointer(t_send *x, t_gpointer *gp)
{
    if (x->x_sym->s_thing) pd_pointer(x->x_sym->s_thing, gp);
}

static void send_list(t_send *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_sym->s_thing) pd_list(x->x_sym->s_thing, s, argc, argv);
}

static void send_anything(t_send *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_sym->s_thing) typedmess(x->x_sym->s_thing, s, argc, argv);
}

static void *send_new(t_symbol *s)
{
    t_send *x = (t_send *)pd_new(send_class);
    if (!*s->s_name)
        symbolinlet_new(&x->x_obj, &x->x_sym);
    x->x_sym = s;
    return (x);
}

static void send_setup(void)
{
    send_class = class_new(gensym("send"), (t_newmethod)send_new, 0,
        sizeof(t_send), 0, A_DEFSYM, 0);
    class_addcreator((t_newmethod)send_new, gensym("s"), A_DEFSYM, 0);
    class_addbang(send_class, send_bang);
    class_addfloat(send_class, send_float);
    class_addsymbol(send_class, send_symbol);
    class_addpointer(send_class, send_pointer);
    class_addlist(send_class, send_list);
    class_addanything(send_class, send_anything);
}
/* -------------------- receive ------------------------------ */

static t_class *receive_class;

typedef struct _receive
{
    t_object x_obj;
    t_symbol *x_sym;
} t_receive;

static void receive_bang(t_receive *x)
{
    outlet_bang(x->x_obj.ob_outlet);
}

static void receive_float(t_receive *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, f);
}

static void receive_symbol(t_receive *x, t_symbol *s)
{
    outlet_symbol(x->x_obj.ob_outlet, s);
}

static void receive_pointer(t_receive *x, t_gpointer *gp)
{
    outlet_pointer(x->x_obj.ob_outlet, gp);
}

static void receive_list(t_receive *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_list(x->x_obj.ob_outlet, s, argc, argv);
}

static void receive_anything(t_receive *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void receive_set2(t_receive *x, t_symbol *s)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    x->x_sym = s;
    pd_bind(&x->x_obj.ob_pd, s);
}

static void *receive_new(t_symbol *s)
{
    t_receive *x = (t_receive *)pd_new(receive_class);
    x->x_sym = s;
    pd_bind(&x->x_obj.ob_pd, s);
    if (s == &s_)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd,
            gensym("set"), gensym("set2"));
    outlet_new(&x->x_obj, 0);
    return (x);
}

static void receive_free(t_receive *x)
{
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
}

static void receive_setup(void)
{
    receive_class = class_new(gensym("receive"), (t_newmethod)receive_new, 
        (t_method)receive_free, sizeof(t_receive), 0, A_DEFSYM, 0);
    class_addcreator((t_newmethod)receive_new, gensym("r"), A_DEFSYM, 0);
    class_addbang(receive_class, receive_bang);
    class_addfloat(receive_class, (t_method)receive_float);
    class_addsymbol(receive_class, receive_symbol);
    class_addpointer(receive_class, receive_pointer);
    class_addlist(receive_class, receive_list);
    class_addanything(receive_class, receive_anything);
    class_addmethod(receive_class, (t_method)receive_set2,
        gensym("set2"), A_SYMBOL, 0);
}

/* -------------------------- select ------------------------------ */

static t_class *sel1_class;

static t_class *sel1_proxy_class;

typedef struct _sel1_proxy
{
    t_pd l_pd;
    void *parent;
} t_sel1_proxy;

typedef struct _sel1
{
    t_object x_obj;
    t_atom x_atom;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
    t_sel1_proxy x_pxy;
} t_sel1;

static void sel1_proxy_init(t_sel1_proxy *x, t_sel1 *p)
{
    x->l_pd = sel1_proxy_class;
    x->parent = (void *)p;
}

static void sel1_proxy_float(t_sel1_proxy *x, t_float f)
{
    t_sel1 *p = (t_sel1 *)x->parent;
    SETFLOAT(&p->x_atom, f);
}

static void sel1_proxy_symbol(t_sel1_proxy *x, t_symbol *s)
{
    t_sel1 *p = (t_sel1 *)x->parent;
    SETSYMBOL(&p->x_atom, s);
}

static void sel1_float(t_sel1 *x, t_float f)
{
    if (x->x_atom.a_type == A_FLOAT && f == x->x_atom.a_w.w_float)
        outlet_bang(x->x_outlet1);
    else outlet_float(x->x_outlet2, f);
}

static void sel1_symbol(t_sel1 *x, t_symbol *s)
{
    if (x->x_atom.a_type == A_SYMBOL && s == x->x_atom.a_w.w_symbol)
        outlet_bang(x->x_outlet1);
    else outlet_symbol(x->x_outlet2, s);
}

static t_class *sel2_class;

typedef struct _selectelement
{
    t_atom e_atom;
    t_outlet *e_outlet;
} t_selectelement;

typedef struct _sel2
{
    t_object x_obj;
    t_atomtype x_type;
    t_int x_mixed;
    t_int x_nelement;
    t_selectelement *x_vec;
    t_outlet *x_rejectout;
} t_sel2;

static void sel2_float(t_sel2 *x, t_float f)
{
    t_selectelement *e;
    int nelement;
    if (x->x_type == A_FLOAT || x->x_mixed)
    {
        for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            if (e->e_atom.a_type == A_FLOAT && e->e_atom.a_w.w_float == f)
            {
                outlet_bang(e->e_outlet);
                return;
            }
    }
    outlet_float(x->x_rejectout, f);
}

static void sel2_symbol(t_sel2 *x, t_symbol *s)
{
    t_selectelement *e;
    int nelement;
    if (x->x_type == A_SYMBOL || x->x_mixed)
    {
        for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            if (e->e_atom.a_type == A_SYMBOL && e->e_atom.a_w.w_symbol == s)
            {
                outlet_bang(e->e_outlet);
                return;
            }
    }
    outlet_symbol(x->x_rejectout, s);
}

static void sel2_free(t_sel2 *x)
{
    freebytes(x->x_vec, x->x_nelement * sizeof(*x->x_vec));
}

static void *select_new(t_symbol *s, int argc, t_atom *argv)
{
    t_atom a;
    if (argc == 0)
    {
        argc = 1;
        SETFLOAT(&a, 0);
        argv = &a;
    }
    if (argc == 1)
    {
        t_sel1 *x = (t_sel1 *)pd_new(sel1_class);
        x->x_atom = *argv;
        x->x_outlet1 = outlet_new(&x->x_obj, &s_bang);
        sel1_proxy_init(&x->x_pxy, x);
        inlet_new(&x->x_obj, &x->x_pxy.l_pd, 0, 0);
        if (argv->a_type == A_FLOAT)
            x->x_outlet2 = outlet_new(&x->x_obj, &s_float);
        else
            x->x_outlet2 = outlet_new(&x->x_obj, &s_symbol);
        return (x);
    }
    else
    {
        int n;
        t_selectelement *e;
        t_sel2 *x = (t_sel2 *)pd_new(sel2_class);
        x->x_nelement = argc;
        x->x_type = argv[0].a_type;
        t_int f, s = 0;
        x->x_vec = (t_selectelement *)getbytes(argc * sizeof(*x->x_vec));
        for (n = 0, e = x->x_vec; n < argc; n++, e++)
        {
            e->e_outlet = outlet_new(&x->x_obj, &s_bang);
            if (argv[0].a_type == A_FLOAT)
            {
                SETFLOAT(&e->e_atom, atom_getfloatarg(n, argc, argv));
                f = 1;
            }
            else
            {
                SETSYMBOL(&e->e_atom, atom_getsymbolarg(n, argc, argv));
                s = 1;
            }
        }
        x->x_mixed = f && s;
        x->x_rejectout = outlet_new(&x->x_obj, &s_float);
        return (x);
    }

}

void select_setup(void)
{
    sel1_proxy_class = class_new(gensym("select_inlet"),
        0, 0, sizeof(t_sel1_proxy), 0, 0);
    class_addfloat(sel1_proxy_class, (t_method)sel1_proxy_float);
    class_addsymbol(sel1_proxy_class, (t_method)sel1_proxy_symbol);

    sel1_class = class_new(gensym("select"), 0, 0,
        sizeof(t_sel1), 0, 0);
    class_addfloat(sel1_class, sel1_float);
    class_addsymbol(sel1_class, sel1_symbol);

    sel2_class = class_new(gensym("select"), 0, (t_method)sel2_free,
        sizeof(t_sel2), 0, 0);
    class_addfloat(sel2_class, sel2_float);
    class_addsymbol(sel2_class, sel2_symbol);

    class_addcreator((t_newmethod)select_new, gensym("select"),  A_GIMME, 0);
    class_addcreator((t_newmethod)select_new, gensym("sel"),  A_GIMME, 0);
}

/* -------------------------- route ------------------------------ */

static t_class *route_class;

static t_class *route_proxy_class;

typedef struct _routeelement
{
    t_word e_w;
    t_outlet *e_outlet;
} t_routeelement;

typedef struct _route_proxy
{
    t_pd l_pd;
    void *parent;
} t_route_proxy;

typedef struct _route
{
    t_object x_obj;
    t_atomtype x_type;
    t_int x_nelement;
    t_routeelement *x_vec;
    t_outlet *x_rejectout;
    t_route_proxy x_pxy;
    t_int x_mixed;
} t_route;

static void route_proxy_init(t_route_proxy *x, t_route *p)
{
    x->l_pd = route_proxy_class;
    x->parent = (void *)p;
}

static void route_proxy_float(t_route_proxy *x, t_float f)
{
    t_route *p = (t_route *)x->parent;
    p->x_type = A_FLOAT;
    p->x_vec->e_w.w_float = f;
}

static void route_proxy_symbol(t_route_proxy *x, t_symbol *s)
{
    t_route *p = (t_route *)x->parent;
    p->x_type = A_SYMBOL;
    p->x_vec->e_w.w_symbol = s;
}

static void route_anything(t_route *x, t_symbol *sel, int argc, t_atom *argv)
{
    t_routeelement *e;
    int nelement;
    if (x->x_type == A_SYMBOL || x->x_mixed) 
    {
        for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            if (e->e_w.w_symbol == sel)
        {
            if (argc > 0 && argv[0].a_type == A_SYMBOL)
                outlet_anything(e->e_outlet, argv[0].a_w.w_symbol,
                    argc-1, argv+1);
            else outlet_list(e->e_outlet, 0, argc, argv);
            return;
        }
    }
    outlet_anything(x->x_rejectout, sel, argc, argv);
}

static void route_list(t_route *x, t_symbol *sel, int argc, t_atom *argv)
{
    //fprintf(stderr,"route_list\n");
    t_routeelement *e;
    int nelement;
    if (x->x_type == A_FLOAT || x->x_mixed)
    {
        t_float f;
        if (!argc || argv->a_type != A_FLOAT)
            goto try_symbol;
        f = atom_getfloat(argv);
        for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            if (e->e_w.w_float == f)
        {
            if (argc > 1 && argv[1].a_type == A_SYMBOL)
                outlet_anything(e->e_outlet, argv[1].a_w.w_symbol,
                    argc-2, argv+2);
            else
            {
                if (argc > 1)
                    outlet_list(e->e_outlet, 0, argc-1, argv+1);
                else
                    outlet_bang(e->e_outlet);
            }
            return;
        }
    }
 try_symbol:
    if (x->x_type == A_SYMBOL || x->x_mixed)    /* symbol arguments */
    {
        if (argc > 1)       /* 2 or more args: treat as "list" */
        {
            for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            {
                if (e->e_w.w_symbol == &s_list)
                {
                    if (argc > 0 && argv[0].a_type == A_SYMBOL)
                        outlet_anything(e->e_outlet, argv[0].a_w.w_symbol,
                            argc-1, argv+1);
                    else outlet_list(e->e_outlet, 0, argc, argv);
                    return;
                }
            }
        }
        else if (argc == 0)         /* no args: treat as "bang" */
        {
            for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            {
                if (e->e_w.w_symbol == &s_bang)
                {
                    outlet_bang(e->e_outlet);
                    return;
                }
            }
        }
        else if (argv[0].a_type == A_FLOAT)     /* one float arg */
        {
            for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            {
                if (e->e_w.w_symbol == &s_float)
                {
                    outlet_float(e->e_outlet, argv[0].a_w.w_float);
                    return;
                }
            }
        }
        else if (argv[0].a_type == A_SYMBOL)   /* one symbol arg */
        {
            for (nelement = x->x_nelement, e = x->x_vec; nelement--; e++)
            {
                if (e->e_w.w_symbol == &s_symbol)
                {
                    outlet_symbol(e->e_outlet, argv[0].a_w.w_symbol);
                    return;
                }
            }
        }
    }
    outlet_list(x->x_rejectout, 0, argc, argv);
}


static void route_free(t_route *x)
{
    freebytes(x->x_vec, x->x_nelement * sizeof(*x->x_vec));
}

static void *route_new(t_symbol *s, int argc, t_atom *argv)
{
    int n, flt = 0, sym = 0;
    t_routeelement *e;
    t_route *x = (t_route *)pd_new(route_class);
    t_atom a;
    if (argc == 0)
    {
        argc = 1;
        SETFLOAT(&a, 0);
        argv = &a;
    }
    x->x_type = argv[0].a_type;
    x->x_nelement = argc;
    x->x_vec = (t_routeelement *)getbytes(argc * sizeof(*x->x_vec));
    for (n = 0, e = x->x_vec; n < argc; n++, e++)
    {
        e->e_outlet = outlet_new(&x->x_obj, &s_list);
        if (argv[n].a_type == A_FLOAT)
        {
            e->e_w.w_float = atom_getfloatarg(n, argc, argv);
            flt = 1;
        }
        else
        {
            e->e_w.w_symbol = atom_getsymbolarg(n, argc, argv);
            sym = 1;
        }
    }
    if (argc == 1)
    {
        route_proxy_init(&x->x_pxy, x);
        inlet_new(&x->x_obj, &x->x_pxy.l_pd, 0, 0);
        /*if (argv->a_type == A_FLOAT)
            floatinlet_new(&x->x_obj, &x->x_vec->e_w.w_float);
        else
            symbolinlet_new(&x->x_obj, &x->x_vec->e_w.w_symbol);*/
    }
    x->x_mixed = flt * sym;
    x->x_rejectout = outlet_new(&x->x_obj, &s_list);
    return (x);
}

void route_setup(void)
{
    route_proxy_class = class_new(gensym("route_inlet"),
        0, 0, sizeof(t_route_proxy), 0, 0);
    class_addfloat(route_proxy_class, (t_method)route_proxy_float);
    class_addsymbol(route_proxy_class, (t_method)route_proxy_symbol);
    route_class = class_new(gensym("route"), (t_newmethod)route_new,
        (t_method)route_free, sizeof(t_route), 0, A_GIMME, 0);
    class_addlist(route_class, route_list);
    class_addanything(route_class, route_anything);
}

/* -------------------------- pack ------------------------------ */

static t_class *pack_class;

typedef struct _pack
{
    t_object x_obj;
    t_int x_n;              /* number of args */
    t_atom *x_vec;          /* input values */
    t_int x_nptr;           /* number of pointers */
    t_gpointer *x_gpointer; /* the pointers */
    t_atom *x_outvec;       /* space for output values */
} t_pack;

static void *pack_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pack *x = (t_pack *)pd_new(pack_class);
    t_atom defarg[2], *ap, *vp;
    t_gpointer *gp;
    int nptr = 0;
    int i;
    if (!argc)
    {
        argv = defarg;
        argc = 2;
        SETFLOAT(&defarg[0], 0);
        SETFLOAT(&defarg[1], 0);
    }

    x->x_n = argc;
    x->x_vec = (t_atom *)getbytes(argc * sizeof(*x->x_vec));
    x->x_outvec = (t_atom *)getbytes(argc * sizeof(*x->x_outvec));

    for (i = argc, ap = argv; i--; ap++)
        if (ap->a_type == A_SYMBOL && *ap->a_w.w_symbol->s_name == 'p')
            nptr++;

    gp = x->x_gpointer = (t_gpointer *)t_getbytes(nptr * sizeof (*gp));
    x->x_nptr = nptr;

    for (i = 0, vp = x->x_vec, ap = argv; i < argc; i++, ap++, vp++)
    {
        if (ap->a_type == A_FLOAT)
        {
            *vp = *ap;
            if (i) floatinlet_new(&x->x_obj, &vp->a_w.w_float);
        }
        else if (ap->a_type == A_SYMBOL)
        {
            char c = *ap->a_w.w_symbol->s_name;
            if (c == 's')
            {
                SETSYMBOL(vp, &s_symbol);
                if (i) symbolinlet_new(&x->x_obj, &vp->a_w.w_symbol);
            }
            else if (c == 'p')
            {
                vp->a_type = A_POINTER;
                vp->a_w.w_gpointer = gp;
                gpointer_init(gp);
                if (i) pointerinlet_new(&x->x_obj, gp);
                gp++;
            }
            else
            {
                if (c != 'f') pd_error(x, "pack: %s: bad type",
                    ap->a_w.w_symbol->s_name);
                SETFLOAT(vp, 0);
                if (i) floatinlet_new(&x->x_obj, &vp->a_w.w_float);
            }
        }
    }
    outlet_new(&x->x_obj, &s_list);
    return (x);
}

static void pack_bang(t_pack *x)
{
    int i, reentered = 0, size = x->x_n * sizeof (t_atom);
    t_gpointer *gp;
    t_atom *outvec;
    for (i = x->x_nptr, gp = x->x_gpointer; i--; gp++)
        if (!gpointer_check(gp, 1))
    {
        pd_error(x, "pack: stale pointer");
        return;
    }
        /* reentrancy protection.  The first time through use the pre-allocated
        x_outvec; if we're reentered we have to allocate new memory. */
    if (!x->x_outvec)
    {
            /* LATER figure out how to deal with reentrancy and pointers... */
        if (x->x_nptr)
            post("pack_bang: warning: reentry with pointers unprotected");
        outvec = t_getbytes(size);
        reentered = 1;
    }
    else
    {
        outvec = x->x_outvec;
        x->x_outvec = 0;
    }
    memcpy(outvec, x->x_vec, size);
    outlet_list(x->x_obj.ob_outlet, &s_list, x->x_n, outvec);
    if (reentered)
        t_freebytes(outvec, size);
    else x->x_outvec = outvec;
}

static void pack_pointer(t_pack *x, t_gpointer *gp)
{
    if (x->x_vec->a_type == A_POINTER)
    {
        gpointer_unset(x->x_gpointer);
        *x->x_gpointer = *gp;
        if (gp->gp_stub) gp->gp_stub->gs_refcount++;
        pack_bang(x);
    }
    else pd_error(x, "pack_pointer: wrong type");
}

static void pack_float(t_pack *x, t_float f)
{
    if (x->x_vec->a_type == A_FLOAT)
    {
        x->x_vec->a_w.w_float = f;
        pack_bang(x);
    }
    else pd_error(x, "pack_float: wrong type");
}

static void pack_symbol(t_pack *x, t_symbol *s)
{
    if (x->x_vec->a_type == A_SYMBOL)
    {
        x->x_vec->a_w.w_symbol = s;
        pack_bang(x);
    }
    else pd_error(x, "pack_symbol: wrong type");
}

static void pack_list(t_pack *x, t_symbol *s, int ac, t_atom *av)
{
    if (ac==0)
        pack_bang(x);
    else
        obj_list(&x->x_obj, 0, ac, av);
}

static void pack_anything(t_pack *x, t_symbol *s, int ac, t_atom *av)
{
    t_atom *av2 = (t_atom *)getbytes((ac + 1) * sizeof(t_atom));
    int i;
    for (i = 0; i < ac; i++)
        av2[i + 1] = av[i];
    SETSYMBOL(av2, s);
    obj_list(&x->x_obj, 0, ac+1, av2);
    freebytes(av2, (ac + 1) * sizeof(t_atom));
}

static void pack_free(t_pack *x)
{
    t_gpointer *gp;
    int i;
    for (gp = x->x_gpointer, i = x->x_nptr; i--; gp++)
        gpointer_unset(gp);
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
    freebytes(x->x_outvec, x->x_n * sizeof(*x->x_outvec));
    freebytes(x->x_gpointer, x->x_nptr * sizeof(*x->x_gpointer));
}

static void pack_setup(void)
{
    pack_class = class_new(gensym("pack"), (t_newmethod)pack_new,
        (t_method)pack_free, sizeof(t_pack), 0, A_GIMME, 0);
    class_addbang(pack_class, pack_bang);
    class_addpointer(pack_class, pack_pointer);
    class_addfloat(pack_class, pack_float);
    class_addsymbol(pack_class, pack_symbol);
    class_addlist(pack_class, pack_list);
    class_addanything(pack_class, pack_anything);
}

/* -------------------------- unpack ------------------------------ */

static t_class *unpack_class;

typedef struct unpackout
{
    t_atomtype u_type;
    t_outlet *u_outlet;
} t_unpackout;

typedef struct _unpack
{
    t_object x_obj;
    t_int x_n;
    t_unpackout *x_vec;
} t_unpack;

static void *unpack_new(t_symbol *s, int argc, t_atom *argv)
{
    t_unpack *x = (t_unpack *)pd_new(unpack_class);
    t_atom defarg[2], *ap;
    t_unpackout *u;
    int i;
    if (!argc)
    {
        argv = defarg;
        argc = 2;
        SETFLOAT(&defarg[0], 0);
        SETFLOAT(&defarg[1], 0);
    }
    x->x_n = argc;
    x->x_vec = (t_unpackout *)getbytes(argc * sizeof(*x->x_vec));
    for (i = 0, ap = argv, u = x->x_vec; i < argc; u++, ap++, i++)
    {
        t_atomtype type = ap->a_type;
        if (type == A_SYMBOL)
        {
            char c = *ap->a_w.w_symbol->s_name;
            if (c == 's')
            {
                u->u_type = A_SYMBOL;
                u->u_outlet = outlet_new(&x->x_obj, &s_symbol);
            }
            else if (c == 'p')
            {
                u->u_type =  A_POINTER;
                u->u_outlet = outlet_new(&x->x_obj, &s_pointer);
            }
            else
            {
                if (c != 'f') pd_error(x, "unpack: %s: bad type",
                    ap->a_w.w_symbol->s_name);
                u->u_type = A_FLOAT;
                u->u_outlet = outlet_new(&x->x_obj, &s_float);
            }
        }
        else
        {
            u->u_type =  A_FLOAT;
            u->u_outlet = outlet_new(&x->x_obj, &s_float);
        }
    }
    return (x);
}

static void unpack_list(t_unpack *x, t_symbol *s, int argc, t_atom *argv)
{
    t_atom *ap;
    t_unpackout *u;
    int i;
    if (argc > x->x_n) argc = x->x_n;
    for (i = argc, u = x->x_vec + i, ap = argv + i; u--, ap--, i--;)
    {
        t_atomtype type = u->u_type;
        if (type != ap->a_type)
            pd_error(x, "unpack: type mismatch");
        else if (type == A_FLOAT)
            outlet_float(u->u_outlet, ap->a_w.w_float);
        else if (type == A_SYMBOL)
            outlet_symbol(u->u_outlet, ap->a_w.w_symbol);
        else outlet_pointer(u->u_outlet, ap->a_w.w_gpointer);
    }
}

static void unpack_anything(t_unpack *x, t_symbol *s, int ac, t_atom *av)
{
    t_atom *av2 = (t_atom *)getbytes((ac + 1) * sizeof(t_atom));
    int i;
    for (i = 0; i < ac; i++)
        av2[i + 1] = av[i];
    SETSYMBOL(av2, s);
    unpack_list(x, 0, ac+1, av2);
    freebytes(av2, (ac + 1) * sizeof(t_atom));
}

static void unpack_free(t_unpack *x)
{
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
}

static void unpack_setup(void)
{
    unpack_class = class_new(gensym("unpack"), (t_newmethod)unpack_new,
        (t_method)unpack_free, sizeof(t_unpack), 0, A_GIMME, 0);
    class_addlist(unpack_class, unpack_list);
    class_addanything(unpack_class, unpack_anything);
}

/* -------------------------- trigger ------------------------------ */

static t_class *trigger_class;
#define TR_BANG 0
#define TR_FLOAT 1
#define TR_SYMBOL 2
#define TR_POINTER 3
#define TR_LIST 4
#define TR_ANYTHING 5

#define TR_STATIC_FLOAT 6
#define TR_STATIC_SYMBOL 7

typedef struct triggerout
{
    int u_type;         /* outlet type from above */
    t_symbol *u_sym;     /* static value */
    t_float u_float;    /* static value */
    t_outlet *u_outlet;
} t_triggerout;

typedef struct _trigger
{
    t_object x_obj;
    t_int x_n;
    t_triggerout *x_vec;
} t_trigger;

// forward declaration
static void trigger_symbol(t_trigger *x, t_symbol *s);

static void *trigger_new(t_symbol *s, int argc, t_atom *argv)
{
    t_trigger *x = (t_trigger *)pd_new(trigger_class);
    t_atom defarg[2], *ap;
    t_triggerout *u;
    int i;
    if (!argc)
    {
        argv = defarg;
        argc = 2;
        SETSYMBOL(&defarg[0], &s_bang);
        SETSYMBOL(&defarg[1], &s_bang);
    }
    x->x_n = argc;
    x->x_vec = (t_triggerout *)getbytes(argc * sizeof(*x->x_vec));
    for (i = 0, ap = argv, u = x->x_vec; i < argc; u++, ap++, i++)
    {
        t_atomtype thistype = ap->a_type;
        char c;
        if (thistype == TR_SYMBOL)
        {
            if (strlen(ap->a_w.w_symbol->s_name) == 1)
                c = ap->a_w.w_symbol->s_name[0];
            else if (strcmp(ap->a_w.w_symbol->s_name, "anything") == 0)
                c = 'a';
            else if (strcmp(ap->a_w.w_symbol->s_name, "bang") == 0)
                c = 'b';
            else if (strcmp(ap->a_w.w_symbol->s_name, "float") == 0)
                c = 'f';
            else if (strcmp(ap->a_w.w_symbol->s_name, "list") == 0)
                c = 'l';
            else if (strcmp(ap->a_w.w_symbol->s_name, "pointer") == 0)
                c = 'p';            
            else if (strcmp(ap->a_w.w_symbol->s_name, "symbol") == 0)
                c = 's';
            else c = 'S';
        }
        else if (thistype == TR_FLOAT)
            c = 'F';
        else c = 0;
        if (c == 'p')
            u->u_type = TR_POINTER,
                u->u_outlet = outlet_new(&x->x_obj, &s_pointer);
        else if (c == 'f')
            u->u_type = TR_FLOAT, u->u_outlet = outlet_new(&x->x_obj, &s_float);
        else if (c == 'b')
            u->u_type = TR_BANG, u->u_outlet = outlet_new(&x->x_obj, &s_bang);
        else if (c == 'l')
            u->u_type = TR_LIST, u->u_outlet = outlet_new(&x->x_obj, &s_list);
        else if (c == 's')
            u->u_type = TR_SYMBOL,
                u->u_outlet = outlet_new(&x->x_obj, &s_symbol);
        else if (c == 'a')
            u->u_type = TR_ANYTHING,
                u->u_outlet = outlet_new(&x->x_obj, &s_symbol);
        else if (c == 'F')
        {
            //static float
            u->u_float = ap->a_w.w_float;
            u->u_type = TR_STATIC_FLOAT;
            u->u_outlet = outlet_new(&x->x_obj, &s_float);
        }
        else if (c == 'S')
        {
            //static symbol
            u->u_sym = gensym(ap->a_w.w_symbol->s_name);
            u->u_type = TR_STATIC_SYMBOL;
            u->u_outlet = outlet_new(&x->x_obj, &s_symbol);
        }
        else
        {
            pd_error(x, "trigger: %s: bad type", ap->a_w.w_symbol->s_name);
            u->u_type = TR_FLOAT, u->u_outlet = outlet_new(&x->x_obj, &s_float);
        }
    }
    return (x);
}

static void trigger_list(t_trigger *x, t_symbol *s, int argc, t_atom *argv)
{
    t_triggerout *u;
    int i;
    for (i = x->x_n, u = x->x_vec + i; u--, i--;)
    {
        if (u->u_type == TR_FLOAT)
            outlet_float(u->u_outlet, (argc ? atom_getfloat(argv) : 0));
        else if (u->u_type == TR_BANG)
            outlet_bang(u->u_outlet);
        else if (u->u_type == TR_SYMBOL)
            outlet_symbol(u->u_outlet,
                (argc ? atom_getsymbol(argv) : (s != NULL ? s : &s_symbol)));
        else if (u->u_type == TR_ANYTHING)
            outlet_anything(u->u_outlet, s, argc, argv);
        else if (u->u_type == TR_POINTER)
        {
            if (!argc || argv->a_type != TR_POINTER)
                pd_error(x, "unpack: bad pointer");
            else outlet_pointer(u->u_outlet, argv->a_w.w_gpointer);
        }
        else if (u->u_type == TR_STATIC_FLOAT)
        {
            outlet_float(u->u_outlet, u->u_float);    
        }
        else if (u->u_type == TR_STATIC_SYMBOL)
        {
            outlet_symbol(u->u_outlet, u->u_sym);
        }
        else outlet_list(u->u_outlet, &s_list, argc, argv);
    }
}

static void trigger_anything(t_trigger *x, t_symbol *s, int argc, t_atom *argv)
{
    //fprintf(stderr,"trigger_anything %s %d\n", s->s_name, argc);
    t_atom *av2 = NULL;
    t_triggerout *u;
    int i, j = 0;
    for (i = x->x_n, u = x->x_vec + i; u--, i--;)
    {
        if (u->u_type == TR_BANG)
            outlet_bang(u->u_outlet);
        else if (u->u_type == TR_ANYTHING)
        {
            //fprintf(stderr,"TR_ANYTHING\n");
            outlet_anything(u->u_outlet, s, argc, argv);
        }
        else if (u->u_type == TR_STATIC_FLOAT)
        {
            outlet_float(u->u_outlet, u->u_float);    
        }
        else if (u->u_type == TR_STATIC_SYMBOL)
        {
            outlet_symbol(u->u_outlet, u->u_sym);
        }        
        //else trigger_symbol(x, s);
        else
        {
            // copying trigger_list behavior except that here we keep
            // the outlet number and therefore avoid redundant printouts
            if (u->u_type == TR_FLOAT)
            {
                //fprintf(stderr,"trigger_anything -> TR_FLOAT %d\n", argc);
                outlet_float(u->u_outlet, (argc ? atom_getfloat(argv) : 0));
            }
            else if (u->u_type == TR_BANG)
            {
                //fprintf(stderr,"trigger_anything -> TR_BANG %d\n", argc);
                outlet_bang(u->u_outlet);
            }
            else if (u->u_type == TR_SYMBOL)
            {
                //fprintf(stderr,"trigger_anything -> TR_SYMBOL %d\n", argc);
                outlet_symbol(u->u_outlet,
                    (s != NULL ? s : (argc ? atom_getsymbol(argv) : &s_symbol)));
            }
            else if (u->u_type == TR_ANYTHING)
            {
                //fprintf(stderr,"trigger_anything -> TR_ANYTHING %d\n", argc);
                outlet_anything(u->u_outlet, s, argc, argv);
            }
            else if (u->u_type == TR_POINTER)
            {
                //fprintf(stderr,"trigger_anything -> TR_POINTER %d\n", argc);
                if (!argc || argv->a_type != TR_POINTER)
                    pd_error(x, "unpack: bad pointer");
                else outlet_pointer(u->u_outlet, argv->a_w.w_gpointer);
            }
            else if (u->u_type == TR_STATIC_FLOAT)
            {
                //fprintf(stderr,"trigger_anything -> TR_STATIC_FLOAT %d\n", argc);
                outlet_float(u->u_outlet, u->u_float);
            }
            else if (u->u_type == TR_STATIC_SYMBOL)
            {
                //fprintf(stderr,"trigger_anything -> TR_STATIC_SYMBOL %d\n", argc);
                outlet_symbol(u->u_outlet, u->u_sym);
            }
            else
            {
                // Ico: don't have to worry about zero element case (AFAICT)
                av2 = (t_atom *)getbytes((argc + 1) * sizeof(t_atom));
                SETSYMBOL(av2, s);
                if (argc == 0)
                {
                    //fprintf(stderr,"trigger_anything -> symbol %d\n", argc);
                    outlet_list(u->u_outlet, &s_symbol, argc+1, av2);
                }
                else
                {
                    for (j = 0; j < argc; j++)
                        av2[j + 1] = argv[j];
                    //fprintf(stderr,"trigger_anything -> list %d\n", argc);
                    SETSYMBOL(av2, s);
                    outlet_list(u->u_outlet, &s_list, argc+1, av2);
                }
                freebytes(av2, (argc + 1) * sizeof(t_atom));
            }
        }
    }
}

static void trigger_bang(t_trigger *x)
{
    trigger_list(x, &s_bang, 0, 0);
}

static void trigger_pointer(t_trigger *x, t_gpointer *gp)
{
    t_atom at;
    SETPOINTER(&at, gp);
    trigger_list(x, &s_list, 1, &at);
}

static void trigger_float(t_trigger *x, t_float f)
{
    t_atom at;
    SETFLOAT(&at, f);
    trigger_list(x, &s_float, 1, &at);
}

static void trigger_symbol(t_trigger *x, t_symbol *s)
{
    t_atom at;
    SETSYMBOL(&at, s);
    trigger_list(x, &s_symbol, 1, &at);
}

static void trigger_free(t_trigger *x)
{
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
}

static void trigger_setup(void)
{
    trigger_class = class_new(gensym("trigger"), (t_newmethod)trigger_new,
        (t_method)trigger_free, sizeof(t_trigger), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)trigger_new, gensym("t"), A_GIMME, 0);
    class_addlist(trigger_class, trigger_list);
    class_addbang(trigger_class, trigger_bang);
    class_addpointer(trigger_class, trigger_pointer);
    class_addfloat(trigger_class, (t_method)trigger_float);
    class_addsymbol(trigger_class, trigger_symbol);
    class_addanything(trigger_class, trigger_anything);
}

/* -------------------------- spigot ------------------------------ */
static t_class *spigot_class;

typedef struct _spigot
{
    t_object x_obj;
    t_float x_state;
} t_spigot;

static void *spigot_new(t_floatarg f)
{
    t_spigot *x = (t_spigot *)pd_new(spigot_class);
    floatinlet_new(&x->x_obj, &x->x_state);
    outlet_new(&x->x_obj, 0);
    x->x_state = f;
    return (x);
}

static void spigot_bang(t_spigot *x)
{
    if (x->x_state != 0) outlet_bang(x->x_obj.ob_outlet);
}

static void spigot_pointer(t_spigot *x, t_gpointer *gp)
{
    if (x->x_state != 0) outlet_pointer(x->x_obj.ob_outlet, gp);
}

static void spigot_float(t_spigot *x, t_float f)
{
    if (x->x_state != 0) outlet_float(x->x_obj.ob_outlet, f);
}

static void spigot_symbol(t_spigot *x, t_symbol *s)
{
    if (x->x_state != 0) outlet_symbol(x->x_obj.ob_outlet, s);
}

static void spigot_list(t_spigot *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_state != 0) outlet_list(x->x_obj.ob_outlet, s, argc, argv);
}

static void spigot_anything(t_spigot *x, t_symbol *s, int argc, t_atom *argv)
{
    if (x->x_state != 0) outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void spigot_setup(void)
{
    spigot_class = class_new(gensym("spigot"), (t_newmethod)spigot_new, 0,
        sizeof(t_spigot), 0, A_DEFFLOAT, 0);
    class_addbang(spigot_class, spigot_bang);
    class_addpointer(spigot_class, spigot_pointer);
    class_addfloat(spigot_class, spigot_float);
    class_addsymbol(spigot_class, spigot_symbol);
    class_addlist(spigot_class, spigot_list);
    class_addanything(spigot_class, spigot_anything);
}

/* --------------------------- moses ----------------------------- */
static t_class *moses_class;

typedef struct _moses
{
    t_object x_ob;
    t_outlet *x_out2;
    t_float x_y;
} t_moses;

static void *moses_new(t_floatarg f)
{
    t_moses *x = (t_moses *)pd_new(moses_class);
    floatinlet_new(&x->x_ob, &x->x_y);
    outlet_new(&x->x_ob, &s_float);
    x->x_out2 = outlet_new(&x->x_ob, &s_float);
    x->x_y = f;
    return (x);
}

static void moses_float(t_moses *x, t_float f)
{
    if (f < x->x_y) outlet_float(x->x_ob.ob_outlet, f);
    else outlet_float(x->x_out2, f);
}

static void moses_setup(void)
{
    moses_class = class_new(gensym("moses"), (t_newmethod)moses_new, 0,
        sizeof(t_moses), 0, A_DEFFLOAT, 0);
    class_addfloat(moses_class, moses_float);
}

/* ----------------------- until --------------------- */

static t_class *until_class;

typedef struct _until
{
    t_object x_obj;
    int x_run;
    int x_count;
} t_until;

static void *until_new(void)
{
    t_until *x = (t_until *)pd_new(until_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("bang"), gensym("bang2"));
    outlet_new(&x->x_obj, &s_bang);
    x->x_run = 0;
    return (x);
}

static void until_bang(t_until *x)
{
    x->x_run = 1;
    x->x_count = -1;
    while (x->x_run && x->x_count)
        x->x_count--, outlet_bang(x->x_obj.ob_outlet);
}

static void until_float(t_until *x, t_float f)
{
    if (f < 0)
        f = 0;
    x->x_run = 1;
    x->x_count = f;
    while (x->x_run && x->x_count)
        x->x_count--, outlet_bang(x->x_obj.ob_outlet);
}

static void until_bang2(t_until *x)
{
    x->x_run = 0;
}

static void until_setup(void)
{
    until_class = class_new(gensym("until"), (t_newmethod)until_new, 0,
        sizeof(t_until), 0, 0);
    class_addbang(until_class, until_bang);
    class_addfloat(until_class, until_float);
    class_addmethod(until_class, (t_method)until_bang2, gensym("bang2"), 0);
}

/* ----------------------- makefilename --------------------- */

static t_class *makefilename_class;

typedef enum {
    NONE = 0,
    INT,
    FLOAT,
    STRING,
    POINTER,
} t_printtype;

typedef struct _makefilename
{
    t_object x_obj;
    t_symbol *x_format;
    t_printtype x_accept;
} t_makefilename;

static const char* makefilename_doscanformat(const char *str, t_printtype *typ,
    char *errormsg)
{
    int infmt = 0, i, hashflag = 0, zeroflag = 0;
    for (; *str; str++)
    {
        if (!infmt && *str == '%')
        {
            /* A single '%' character isn't a valid. It needs a conversion
               specifier ('g', 's', etc.) to follow it */
            if (*(str+1) == '\0')
            {
                str++;
                sprintf(errormsg, "specifier missing");
                *typ = NONE;
                return str;
            }
            infmt = 1;
            continue;
        }
        if (infmt)
        {
            /* 1) flags
               Check for "%%" which produces a literal '%' in the output */
            if (*str == '%')
            {
                /* Not in a format specifier after all, so let's reset
                   infmt and continue searching... */
                infmt = 0;
                continue;
            }
            for (i = 0; *str && strchr("-+#0", *str) != 0; str++, i++)
            {
                /* Check for flags. While a space is a legal flag, Pd's
                   parser would split it off into a separate arg. And since
                   makefilename has always truncated extra args we don't
                   support spaces here. We also don't support the single
                   quote flag.
                   Some flag/specifier combinations can cause undefined
                   behavior so we need to track them. */

                if (*str == '#') hashflag++;
                if (*str == '0') zeroflag++;

                /* Since we're dealing with arbitrary input let's keep
                   the total number of flags sane. */
                if (i > 15)
                {
                    sprintf(errormsg, "too many flags");
                    *typ = NONE;
                    return str;
                }
            }
            /* 2) width field
               Consecutive digits. Technically a width field may also be
               '*' to use a variable to set the value. But makefilename has
               never supported that, either generating a memory error or crash
               upon use. So we exclude it here. */
            if (*str == '*')
            {
                sprintf(errormsg, "variable width value not supported");
                *typ = NONE;
                return str;
            }
            int maxwidth = 3;
            for (i = 0; *str && strchr("0123456789", *str) != 0; str++, i++)
            {
                /* Limit width length to prevent out-of-memory errors. */
                if (i >= maxwidth)
                {
                    sprintf(errormsg, "width field cannot be greater than "
                        "%d digits", maxwidth);
                    *typ = NONE;
                    return str;
                }
            }
            /* 3) precision field
               A '.' followed by consecutive digits. Can also have a '*' for
               variable, so we need to check and exclude as with the width
               field above. */
            if (*str == '.')
            {
                str++;
                if (*str == '*')
                {
                    sprintf(errormsg, "variable precision field not supported");
                    *typ = NONE;
                    return str;
                }
                int maxwidth = 3;
                for (i = 0; *str && strchr("0123456789", *str) != 0; str++, i++)
                {
                    if (i >= maxwidth)
                    {
                        sprintf(errormsg, "precision field cannot be greater "
                            "than %d digits", maxwidth);
                        *typ = NONE;
                        return str;
                    }
                }
                /* precision isn't defined for all conversion specifiers. */
                if (*str && strchr("diouxXeEfFgGs", *str) == 0)
                {
                    sprintf(errormsg, "precision field restricted to "
                                      "[diouxXeEfFgGs] specifiers");
                    *typ = NONE;
                    return str;
                }
            }
            /* 4) length field
               Fairly certain the length field doesn't make any sense
               here. At least I've never seen it used, so let's skip it. */

            /* 5) conversion specifier
               The type of value we want to fill the slot with. */

            /* First, check our flags against the allowed specifiers */
            if (*str && hashflag && strchr("fFgGeExXo", *str) == 0)
            {
                sprintf(errormsg, "'#' flag restricted to [fFgGeExXo] "
                                  "specifiers");
                *typ = NONE;
                return str;
            }
            if (*str && zeroflag && strchr("diouxXeEfFgG", *str) == 0)
            {
                sprintf(errormsg, "'0' flag restricted to [diouxXeEfFgG] "
                                  "specifiers");
                *typ = NONE;
                return str;
            }

            /* a C string */
            if (*str == 's')
            {
                *typ = STRING;
                return str;
            }

            /* a float. There's also 'a' and 'A' from C99, but let's stick
               to the old school ones for now... */
            if (*str && strchr("fFgGeE", *str) != 0)
            {
                *typ = FLOAT;
                return str;
            }

            /* an int */
            if (*str && strchr("xXdiouc", *str) != 0)
            {
                *typ = INT;
                return str;
            }

            /* a pointer. We don't support this in Pd-L2Ork because of the
               possibility of both undefined and implementation-specific
               behavior. */
            if (*str && strchr("p", *str) != 0)
            {
                /* *typ = POINTER; */
                /* return str; */
                sprintf(errormsg, "p specifier not supported");
                *typ = NONE;
                return str;
            }

            /* if we've gotten here it means we are missing a type field.
               Undefined behavior would result, so we will bail here */
            
            /* First, let's check for any remaining type fields which
               we don't support. That way we can give a better error message. */
            if (*str == 'a' || *str == 'A')
                sprintf(errormsg, "hexfloat specifier not supported. If you "
                                  "need this feature make a case on the "
                                  "mailing list and we'll consider adding it");
            else if (*str == 'n')
                sprintf(errormsg, "n specifier not supported");
            else if (*str == 'm')
                sprintf(errormsg, "m specifier not supported");
            else if (*str)
                sprintf(errormsg, "bad specifier type '%c'", *str);
            else
                sprintf(errormsg, "specifier missing");
            *typ = NONE;
            return str;
        }
    }
    *typ = NONE;
    return str;
}

static void makefilename_scanformat(t_makefilename *x)
{
    char errorbuf[MAXPDSTRING];
    errorbuf[0] = '\0';
    const char *str;
    t_printtype typ;
    if (!x->x_format) return;
    str = x->x_format->s_name;
    /* First attempt at parsing the format string for the field type. */
    str = makefilename_doscanformat(str, &typ, errorbuf);
    /* If we got any errors we will have some content in our errorbuf... */
    if (*errorbuf)
    {
        pd_error(x, "makefilename: invalid format string '%s' "
                    "(%s). Supressing output.",
            x->x_format->s_name, errorbuf);
        /* set the format string to zero. It would be great to refuse to
           create the object here, but we also have to deal with new format
           strings with the 'set' message. So for consistency we zero out
           the format string and make it a runtimer error. */
        x->x_format = 0;
        return;
    }
    x->x_accept = typ;
    if (str && (typ != NONE))
    {
        /* try again, to see if there's another format specifier
           (which we forbid) */
        str = makefilename_doscanformat(str, &typ, errorbuf);
        /* If we've got a type other than none-- OR if we've got something
           in our errorbuf-- we've got a syntax error. */
        if (typ != NONE || *errorbuf)
        {
            pd_error(x, "makefilename: invalid format string '%s' "
                        "(too many specifiers). "
                        "Suppressing output.",
                x->x_format->s_name);
            x->x_format = 0;
            return;
        }
    }
}

static void *makefilename_new(t_symbol *s)
{
    t_makefilename *x = (t_makefilename *)pd_new(makefilename_class);
    if (!s || !*s->s_name)
        s = gensym("file.%d");
    outlet_new(&x->x_obj, &s_symbol);
    x->x_format = s;
    x->x_accept = NONE;
    makefilename_scanformat(x);
    return (x);
}

static void makefilename_snprintf(t_makefilename *x, char *buf, char *fmt, ...)
{
    int length_minus_null_terminator;
    va_list ap;
    va_start(ap, fmt);
    length_minus_null_terminator =
        vsnprintf(buf, MAXPDSTRING, fmt, ap);
    va_end(ap);
    if (length_minus_null_terminator >= MAXPDSTRING)
    {
        /* Just don't trust snprintf... */
        buf[MAXPDSTRING-1] = '\0';
        pd_error(x, "makefilename: output truncated to %d characters",
            MAXPDSTRING);
    }
}

static void makefilename_float(t_makefilename *x, t_floatarg f)
{
    char buf[MAXPDSTRING];
    if (!x->x_format)
    {
        pd_error(x, "makefilename: no format specifier given");
        return;
    }
    switch(x->x_accept)
    {
    case FLOAT:
        makefilename_snprintf(x, buf, x->x_format->s_name, f);
        break;
    case INT:
        makefilename_snprintf(x, buf, x->x_format->s_name, (int)f);
        break;
    case STRING: {
        char buf2[MAXPDSTRING];
        makefilename_snprintf(x, buf2, "%g", f);
        makefilename_snprintf(x, buf, x->x_format->s_name, buf2);
        break;
    case NONE:
        makefilename_snprintf(x, buf, x->x_format->s_name);
        break;
    }
    default:
        /* POINTER type would fall here. We probably don't want to expose
           implementation-specific output for whatever rando float values
           the user wants to hurl at makefilename. */
        pd_error(x, "cannot convert float with specifier: %s",
            x->x_format->s_name);
        return;
    }
    if (buf[0] != 0)
        outlet_symbol(x->x_obj.ob_outlet, gensym(buf));
}

static void makefilename_symbol(t_makefilename *x, t_symbol *s)
{
    char buf[MAXPDSTRING];
    if (!x->x_format)
    {
        pd_error(x, "makefilename: no format specifier given");
        return;
    }
    switch(x->x_accept)
    {
    case STRING:
        makefilename_snprintf(x, buf, x->x_format->s_name, s->s_name);
        break;
    case INT:
        makefilename_snprintf(x, buf, x->x_format->s_name, 0);
        break;
    case FLOAT:
        makefilename_snprintf(x, buf, x->x_format->s_name, 0.);
        break;
    case NONE:
        makefilename_snprintf(x, buf, x->x_format->s_name);
        break;
    default:
         /* POINTER case falls here. Technically we could print out
            symbol addys using whatever the compiler's implementation-specific
            output format happens to be. But that probably shouldn't be exposed
            within the patch like this. */
        pd_error(x, "cannot convert symbol with specifier: %s",
            x->x_format->s_name);
        return;
    }
    if (buf[0] != 0)
        outlet_symbol(x->x_obj.ob_outlet, gensym(buf));
}

static void makefilename_bang(t_makefilename *x)
{
    char buf[MAXPDSTRING];
    if(!x->x_format)
    {
        pd_error(x, "makefilename: no format specifier given");
        return;
    }
    switch(x->x_accept)
    {
    case INT:
        makefilename_snprintf(x, buf, x->x_format->s_name, 0);
        break;
    case FLOAT:
        makefilename_snprintf(x, buf, x->x_format->s_name, 0.);
        break;
    case STRING:
        makefilename_snprintf(x, buf, x->x_format->s_name, "");
        break;
    case NONE:
        makefilename_snprintf(x, buf, x->x_format->s_name);
        break;
    default:
        pd_error(x, "cannot convert bang with specifier: %s",
            x->x_format->s_name);
        return;
    }
    if (buf[0] != 0)
        outlet_symbol(x->x_obj.ob_outlet, gensym(buf));
}

static void makefilename_set(t_makefilename *x, t_symbol *s)
{
    x->x_format = s;
    makefilename_scanformat(x);
}

static void makefilename_setup(void)
{
    makefilename_class = class_new(gensym("makefilename"),
    (t_newmethod)makefilename_new, 0,
        sizeof(t_makefilename), 0, A_DEFSYM, 0);
    class_addfloat(makefilename_class, makefilename_float);
    class_addsymbol(makefilename_class, makefilename_symbol);
    class_addbang(makefilename_class, makefilename_bang);
    class_addmethod(makefilename_class, (t_method)makefilename_set,
        gensym("set"), A_SYMBOL, 0);
}

/* -------------------------- swap ------------------------------ */
static t_class *swap_class;

typedef struct _swap
{
    t_object x_obj;
    t_outlet *x_out2;
    t_float x_f1;
    t_float x_f2;
} t_swap;

static void *swap_new(t_floatarg f)
{
    t_swap *x = (t_swap *)pd_new(swap_class);
    x->x_f2 = f;
    x->x_f1 = 0;
    outlet_new(&x->x_obj, &s_float);
    x->x_out2 = outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f2);
    return (x);
}

static void swap_bang(t_swap *x)
{
    outlet_float(x->x_out2, x->x_f1);
    outlet_float(x->x_obj.ob_outlet, x->x_f2);
}

static void swap_float(t_swap *x, t_float f)
{
    x->x_f1 = f;
    swap_bang(x);
}

void swap_setup(void)
{
    swap_class = class_new(gensym("swap"), (t_newmethod)swap_new, 0,
        sizeof(t_swap), 0, A_DEFFLOAT, 0);
    class_addcreator((t_newmethod)swap_new, gensym("fswap"), A_DEFFLOAT, 0);
    class_addbang(swap_class, swap_bang);
    class_addfloat(swap_class, swap_float);
}

/* -------------------------- change ------------------------------ */
static t_class *change_class;

typedef struct _change
{
    t_object x_obj;
    t_float x_f;
} t_change;

static void *change_new(t_floatarg f)
{
    t_change *x = (t_change *)pd_new(change_class);
    x->x_f = f;
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

static void change_bang(t_change *x)
{
    outlet_float(x->x_obj.ob_outlet, x->x_f);
}

static void change_float(t_change *x, t_float f)
{
    if (f != x->x_f)
    {
        x->x_f = f;
        outlet_float(x->x_obj.ob_outlet, x->x_f);
    }
}

static void change_set(t_change *x, t_float f)
{
    x->x_f = f;
}

void change_setup(void)
{
    change_class = class_new(gensym("change"), (t_newmethod)change_new, 0,
        sizeof(t_change), 0, A_DEFFLOAT, 0);
    class_addbang(change_class, change_bang);
    class_addfloat(change_class, change_float);
    class_addmethod(change_class, (t_method)change_set, gensym("set"),
        A_DEFFLOAT, 0);
}

/* -------------------- value ------------------------------ */

static t_class *value_class, *vcommon_class;

typedef struct vcommon
{
    t_pd c_pd;
    int c_refcount;
    t_float c_f;
} t_vcommon;

typedef struct _value
{
    t_object x_obj;
    t_symbol *x_sym;
    t_float *x_floatstar;
} t_value;

    /* get a pointer to a named floating-point variable.  The variable
    belongs to a "vcommon" object, which is created if necessary. */
t_float *value_get(t_symbol *s)
{
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if (!c)
    {
        c = (t_vcommon *)pd_new(vcommon_class);
        c->c_f = 0;
        c->c_refcount = 0;
        pd_bind(&c->c_pd, s);
    }
    c->c_refcount++;
    return (&c->c_f);
}

    /* release a variable.  This only frees the "vcommon" resource when the
    last interested party releases it. */
void value_release(t_symbol *s)
{
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if (c)
    {
        if (!--c->c_refcount)
        {
            pd_unbind(&c->c_pd, s);
            pd_free(&c->c_pd);
        }
    }
    else bug("value_release");
}

/*
 * value_getfloat -- obtain the float value of a "value" object 
 *                  return 0 on success, 1 otherwise
 */
int
value_getfloat(t_symbol *s, t_float *f) 
{
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if (!c)
        return (1);
    *f = c->c_f;
    return (0); 
}
 
/*
 * value_setfloat -- set the float value of a "value" object
 *                  return 0 on success, 1 otherwise
 */
int
value_setfloat(t_symbol *s, t_float f)
{
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if (!c)
        return (1);
    c->c_f = f; 
    return (0); 
}

static void vcommon_float(t_vcommon *x, t_float f)
{
    x->c_f = f;
}

static void *value_new(t_symbol *s)
{
    t_value *x = (t_value *)pd_new(value_class);
    x->x_sym = s;
    x->x_floatstar = value_get(s);
    outlet_new(&x->x_obj, &s_float);
    return (x);
}

static void value_bang(t_value *x)
{
    outlet_float(x->x_obj.ob_outlet, *x->x_floatstar);
}

static void value_float(t_value *x, t_float f)
{
    *x->x_floatstar = f;
}

/* set method */
static void value_symbol2(t_value *x, t_symbol *s)
{
    value_release(x->x_sym);
    x->x_sym = s;
    x->x_floatstar = value_get(s);
}

static void value_send(t_value *x, t_symbol *s)
{
    if (s->s_thing)
        pd_float(s->s_thing, *x->x_floatstar);
    else pd_error(x, "%s: no such object", s->s_name);
}

static void value_ff(t_value *x)
{
    value_release(x->x_sym);
}

static void value_setup(void)
{
    value_class = class_new(gensym("value"), (t_newmethod)value_new,
        (t_method)value_ff,
        sizeof(t_value), 0, A_DEFSYM, 0);
    class_addcreator((t_newmethod)value_new, gensym("v"), A_DEFSYM, 0);
    class_addbang(value_class, value_bang);
    class_addfloat(value_class, value_float);
    class_addmethod(value_class, (t_method)value_symbol2, gensym("symbol2"),
        A_DEFSYM, 0);
    class_addmethod(value_class, (t_method)value_send, gensym("send"), A_SYMBOL, 0);
    vcommon_class = class_new(gensym("value"), 0, 0,
        sizeof(t_vcommon), CLASS_PD, 0);
    class_addfloat(vcommon_class, vcommon_float);
}

/* -------------- overall setup routine for this file ----------------- */

void x_connective_setup(void)
{
    pdint_setup();
    pdfloat_setup();
    pdsymbol_setup();
    bang_setup();
    send_setup();
    receive_setup();
    select_setup();
    route_setup();
    pack_setup();
    unpack_setup();
    trigger_setup();
    spigot_setup();
    moses_setup();
    until_setup();
    makefilename_setup();
    swap_setup();
    change_setup();
    value_setup();
}
