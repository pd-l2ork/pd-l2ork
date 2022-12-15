/* disis_gpio_dummy */
/* used for non-RPi platforms as a dummy replacement external */

/* Copyright Ivica Ico Bukic <ico@vt.edu> */
/* Based on Miller Puckette's example - Copyright Miller Puckette - BSD license */
/* with changes to make external rely on the gpio executable to sidestep permmission issues */

#include "m_pd.h"

static t_class *disis_gpio_dummy_class;

typedef struct _disis_gpio_dummy
{
    t_object x_obj;
    t_outlet *x_out1;
    t_outlet *x_out2;
} t_disis_gpio_dummy;

static void disis_gpio_export(t_disis_gpio_dummy *x, t_float f)
{

}

static void disis_gpio_unexport(t_disis_gpio_dummy *x)
{

}

static void disis_gpio_direction(t_disis_gpio_dummy *x, t_symbol *dir)
{

}

static void disis_gpio_open(t_disis_gpio_dummy *x)
{

}

static void disis_gpio_close(t_disis_gpio_dummy *x)
{

}

static void disis_gpio_float(t_disis_gpio_dummy *x, t_float f)
{

}

static void disis_gpio_pwm(t_disis_gpio_dummy *x, t_float val)
{

}

static void disis_gpio_togglesoftpwm(t_disis_gpio_dummy *x, t_float state)
{

}

static void disis_gpio_togglepwm(t_disis_gpio_dummy *x, t_float state)
{

}

static void disis_gpio_bang(t_disis_gpio_dummy *x)
{

}

static void disis_gpio_pwmrange(t_disis_gpio_dummy *x, t_float range)
{

}

static void disis_gpio_pwmclock(t_disis_gpio_dummy *x, t_float clock)
{

}

static void disis_gpio_servomode(t_disis_gpio_dummy *x, t_float mode)
{

}

static void disis_gpio_free(t_disis_gpio_dummy *x) {

}

static void *disis_gpio_new(t_floatarg f)
{
    post("WARNING: this is a dummy version of the disis_gpio object because you appear to be running it on a platform other than Raspberry Pi.");

    t_disis_gpio_dummy *x = (t_disis_gpio_dummy *)pd_new(disis_gpio_dummy_class);

    x->x_out1 = outlet_new(&x->x_obj, gensym("float"));
    x->x_out2 = outlet_new(&x->x_obj, gensym("float"));

    return (x);
}


void disis_gpio_setup(void)
{
    disis_gpio_dummy_class = class_new(gensym("disis_gpio"), (t_newmethod)disis_gpio_new,
        (t_method)disis_gpio_free, sizeof(t_disis_gpio_dummy), 0, A_DEFFLOAT, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_export, gensym("export"), 
        A_DEFFLOAT, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_unexport, gensym("unexport"), 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_direction, gensym("direction"), 
        A_DEFSYMBOL, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_open, gensym("open"), 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_close, gensym("close"), 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_pwm, gensym("pwm"), 
        A_FLOAT, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_togglepwm, gensym("togglepwm"), 
        A_FLOAT, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_togglesoftpwm, gensym("togglesoftpwm"), 
        A_FLOAT, 0);
    //class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_pwmrange, gensym("pwmrange"), 
    //    A_FLOAT, 0);
    class_addfloat(disis_gpio_dummy_class, disis_gpio_float);
    class_addbang(disis_gpio_dummy_class, disis_gpio_bang);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_servomode, gensym("servomode"),
        A_FLOAT, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_pwmrange, gensym("pwmrange"),
        A_FLOAT, 0);
    class_addmethod(disis_gpio_dummy_class, (t_method)disis_gpio_pwmclock, gensym("pwmclock"),
        A_FLOAT, 0);
}
