/* Copyright (c) 2002-2003 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <stdio.h>
#include "m_pd.h"
#include "hammer/gui.h"

typedef struct _active
{
    t_object   x_ob;
    t_symbol  *x_cvname;
    int        x_on;
} t_active;

static t_class *active_class;

static void active_dofocus(t_active *x, t_symbol *s, t_floatarg f)
{
    //post("active_dofocus %s | %s %d", x->x_cvname->s_name, s->s_name, (int)f);
    if ((int)f)
    {
	int on = (s == x->x_cvname);
	if (on != x->x_on)
	    outlet_float(((t_object *)x)->ob_outlet, x->x_on = on);
    }
    else if (x->x_on && s == x->x_cvname)
	outlet_float(((t_object *)x)->ob_outlet, x->x_on = 0);
}

#ifdef PDL2ORK
static void active_pdl2ork_dofocus(t_active *x, t_symbol *s, int ac, t_atom *av)
{
    t_symbol *cnv = av[0].a_w.w_symbol;
    t_floatarg f = av[1].a_w.w_float;
    active_dofocus(x, cnv, f);
}
#endif

static void active_free(t_active *x)
{
#ifdef PDL2ORK
    pd_unbind(&x->x_ob.ob_pd, gensym("_focus"));
#else
    hammergui_unbindfocus((t_pd *)x);
#endif
}

static void *active_new(void)
{
    t_active *x = (t_active *)pd_new(active_class);
    char buf[32];
    sprintf(buf, ".x%zx.c", (t_uint)canvas_getcurrent());
    x->x_cvname = gensym(buf);
    x->x_on = 0;
    outlet_new((t_object *)x, &s_float);
#ifdef PDL2ORK
    pd_bind(&x->x_ob.ob_pd, gensym("_focus"));
#else
    hammergui_bindfocus((t_pd *)x);
#endif
    return (x);
}

void active_setup(void)
{
    active_class = class_new(gensym("active"),
			     (t_newmethod)active_new,
			     (t_method)active_free,
			     sizeof(t_active), CLASS_NOINLET, 0);
    class_addmethod(active_class, (t_method)active_dofocus,
            gensym("_focus"), A_SYMBOL, A_FLOAT, 0);
#ifdef PDL2ORK
    class_addlist(active_class, active_pdl2ork_dofocus);
#endif
}
