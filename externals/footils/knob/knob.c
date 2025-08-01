/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

/* 'knob' gui object by Frank Barknecht, 'externalised' by Olaf Matthes */
/* changed for devel_0.37 by Christoph Kummerer - hardly tested         */

/* I had to out-comment the loadbang stuff because I couldn't find the code
   in Pd where it is declared */

/* Complete overhaul based on initial Jonathan Wilkes' Purr-Data port by
   Ivica Ico Bukvic ico@vt.edu 2020-09-24
*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"

#ifndef PD_MAJOR_VERSION
#include "s_stuff.h"
#else 
#include "m_imp.h"
#endif

#include "g_canvas.h"

#include "g_all_guis.h"
/* This really should be part of g_all_guis.h... */
EXTERN t_int gfxstub_haveproperties(void *key);

#include <math.h>


#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef M_PI
#define M_PI 3.141592654
#endif

#define IEM_KNOB_DEFAULTSIZE 32

/* ------------ knob gui-vertical  slider ----------------------- */

t_widgetbehavior knob_widgetbehavior;
static t_class *knob_class;
t_symbol *iemgui_key_sym=0;     /* taken from g_all_guis.c */

typedef struct _knob            /* taken from Frank's modified g_all_guis.h */
{
    t_iemgui  x_gui;
    t_float   x_pos;
    t_float   x_val;
    int       x_lin0_log1;
    int       x_steady;
    t_float   x_min;
    t_float   x_max;
    t_int     x_dial_w;     // dial width (used for stroke-width)
    t_int     x_circle_w;   // background circle width
    t_int     x_frag_w;     // "on" fragment width
} t_knob;

/* ----------------- forward declarations ----------------------- */

static void knob_list(void *z, t_symbol *s, int ac, t_atom *av);
static void knob_draw_config(t_knob *x,t_glist *glist);
static void knob_set(t_knob *x, t_floatarg f);
static void knob_interactive(t_knob *x, t_floatarg f);
static void knob_width(t_knob *x, t_symbol *s, int ac, t_atom *av);

/* ---------------- widget helper functions --------------------- */

static void knob_io_draw_move(t_knob *z)
{
    t_iemgui *x = &z->x_gui;
    char tagbuf[MAXPDSTRING];
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    t_class *c = pd_class((t_pd *)x);
    int x1,y1,x2,y2;
    c->c_wb->w_getrectfn((t_gobj *)x,canvas,&x1,&y1,&x2,&y2);
    //iemgui_getrect_draw(x, &x1, &y1, &x2, &y2);

    int xtra_w = z->x_circle_w;
    if (z->x_frag_w > xtra_w) xtra_w = z->x_frag_w;
    if (xtra_w < 0) xtra_w = 0;

    //post("knob_io_draw_move %d %d | %d", z->x_circle_w, z->x_frag_w, xtra_w);

    if (!iemgui_has_snd(x) && canvas == x->x_glist)
    {
        sprintf(tagbuf, "%so0", iem_get_tag(canvas, x));
        gui_start_vmess("gui_configure_item", "xs",
            canvas, tagbuf);
        gui_start_array();
        gui_s("x");
        gui_i(-xtra_w/2);
        gui_s("y");
        gui_i(y2 - y1 - 2 - xtra_w/2);
        gui_end_array();
        gui_end_vmess();
    }
    if (!iemgui_has_rcv(x) && canvas == x->x_glist)
    {
        sprintf(tagbuf, "%si0", iem_get_tag(canvas, x));
        gui_start_vmess("gui_configure_item", "xs",
            canvas, tagbuf);
        gui_start_array();
        gui_s("x");
        gui_i(-xtra_w/2);
        gui_s("y");
        gui_i(-xtra_w/2);
        gui_end_array();
        gui_end_vmess();
    }
}

static void knob_draw_update(t_knob *x, t_glist *glist)
{
    t_float normalized_value;
    if (!x->x_gui.x_reverse)
        normalized_value = (t_float)((x->x_pos - x->x_min)/(x->x_max - x->x_min));
    else
        normalized_value = (t_float)((x->x_min - x->x_pos)/(x->x_min - x->x_max));
    /* compute dial:*/ 
    t_float radius = 0.51*(t_float)x->x_gui.x_h;
    t_float angle = 2.0/9.0 + ((t_float)normalized_value * (2.0 * M_PI - (4.0/9.0)));
    /* center point: */
    int x1 = radius;
    int y1 = radius;
    int x2 = radius + radius * sin(-angle); 
        int y2 = radius + radius * cos(angle);

    gui_vmess("gui_turn_mknob", "xxiiiiif",
        glist_getcanvas(glist),
        x,
        x1,
        y1,
        x2,
        y2,
        1,
        normalized_value
    );
}

static void knob_draw_new(t_knob *x, t_glist *glist)
{
    int xpos=text_xpix(&x->x_gui.x_obj, glist);
    int ypos=text_ypix(&x->x_gui.x_obj, glist);
    t_canvas *canvas=glist_getcanvas(glist);
    
    /* reuse mknob drawing routine */
    gui_vmess("gui_mknob_new", "xxxxiiiiii",
        canvas,
        x->x_gui.x_glist,
        x->x_gui.x_glist->gl_owner,
        x,
        xpos,
        ypos - 2,
        glist_istoplevel(glist),
        !iemgui_has_snd(&x->x_gui),
        !iemgui_has_rcv(&x->x_gui),
        1
    );
    knob_draw_config(x, glist);
}

static void knob_draw_config(t_knob *x,t_glist *glist)
{
    //post("knob_draw_config");
    t_canvas *canvas = glist_getcanvas(glist);

    /* reuse mknob interface */
    gui_vmess("gui_configure_mknob", "xxissiiii",
        canvas,
        x,
        x->x_gui.x_h,
        x->x_gui.x_bcol->s_name,
        x->x_gui.x_fcol->s_name,
        1, /* flag to tell we are a footils/knob */
        x->x_dial_w,
        x->x_circle_w,
        x->x_frag_w
    );
    knob_draw_update(x, glist);
    knob_io_draw_move(x);
}

static void knob_draw(t_knob* x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)
        knob_draw_update(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)
        iemgui_base_draw_move(&x->x_gui);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
        knob_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
        knob_draw_config(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_IO)
        knob_io_draw_move(x);
}

/* ------------------------ knob widgetbehaviour----------------------------- */


static void knob_getrect(t_gobj *z, t_glist *glist,
                int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_knob* x = (t_knob*)z;

    int xtra_w = x->x_circle_w;
    if (x->x_frag_w > xtra_w) xtra_w = x->x_frag_w;
    //xtra_w = xtra_w / 2;
    if (xtra_w < 0) xtra_w = 0;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist) - xtra_w/2;
    *yp1 = text_ypix(&x->x_gui.x_obj, glist) - 2 - xtra_w/2;
    *xp2 = *xp1 + x->x_gui.x_h + xtra_w;
    *yp2 = *yp1 + x->x_gui.x_h + xtra_w;
    //post("knob_getrect %d %d %d %d | %d", *xp1, *yp1, *xp2, *yp2, xtra_w);
}

static void knob_save(t_gobj *z, t_binbuf *b)
{
    t_knob *x = (t_knob *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiffiisssiiiisssfiiiii", gensym("#X"),gensym("obj"),
        (t_int)x->x_gui.x_obj.te_xpix, (t_int)x->x_gui.x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_gui.x_obj.te_binbuf)),
        x->x_gui.x_h, x->x_gui.x_h,
        (float)x->x_min, (float)x->x_max,
        x->x_lin0_log1, iem_symargstoint(&x->x_gui),
        srl[0], srl[1], srl[2],
        x->x_gui.x_ldx, x->x_gui.x_ldy,
        iem_fstyletoint(&x->x_gui), x->x_gui.x_fontsize,
        bflcol[0], bflcol[1], bflcol[2],
        x->x_val, x->x_steady, x->x_gui.x_click,
        x->x_dial_w, x->x_circle_w, x->x_frag_w);
    binbuf_addv(b, ";");
}

static void knob_check_height(t_knob *x, int h)
{
    if(h < IEM_SL_MINSIZE)
    h = IEM_SL_MINSIZE;
    x->x_gui.x_h = h;
}

static void knob_check_minmax(t_knob *x, double min, double max)
{
    if(x->x_lin0_log1)
    {
        if((min == 0.0)&&(max == 0.0))
            max = 1.0;
        if(max > 0.0)
        {
            if(min <= 0.0)
                min = 0.01*max;
        }
        else
        {
            if(min > 0.0)
                max = 0.01*min;
        }
    }
    x->x_min = min;
    x->x_max = max;
    if(x->x_min > x->x_max) /* bugfix */
    {
        x->x_gui.x_reverse = 1;
        if (x->x_val > x->x_min)
            x->x_val = x->x_min;
        if (x->x_val < x->x_max)
            x->x_val = x->x_max;
    }
    else
    {
        x->x_gui.x_reverse = 0;
        if (x->x_val < x->x_min)
            x->x_val = x->x_min;
        if (x->x_val > x->x_max)
            x->x_val = x->x_max;
    }
}

static void knob_properties(t_gobj *z, t_glist *owner)
{
    t_knob *x = (t_knob *)z;
    char *gfx_tag;
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);
    gfx_tag = gfxstub_new2(&x->x_gui.x_obj.ob_pd, &x->x_gui);

    gui_start_vmess("gui_iemgui_dialog", "s", gfx_tag);
    gui_start_array();

    gui_s("type");             gui_s("knob");
    /* Since mknob reuses the iemgui dialog code, we just
       use the "width" slot for "size" and the "height" slot
       for the number of steps and re-label it on the GUI side. */
//    gui_s("width");            gui_i(x->x_gui.x_w);
//    gui_s("height");           gui_i(x->x_gui.x_h);
    gui_s("size");             gui_i(x->x_gui.x_h);
    gui_s("minimum_range");    gui_f(x->x_min);
    gui_s("maximum_range");    gui_f(x->x_max);
    gui_s("log_scaling");      gui_i(x->x_lin0_log1);
    gui_s("init");             gui_i(x->x_gui.x_loadinit);
    gui_s("steady_on_click");   gui_i(x->x_steady);
    gui_s("send_symbol");      gui_s(srl[0]->s_name);
    gui_s("receive_symbol");   gui_s(srl[1]->s_name);
    gui_s("label");            gui_s(srl[2]->s_name);
    gui_s("x_offset");         gui_i(x->x_gui.x_ldx);
    gui_s("y_offset");         gui_i(x->x_gui.x_ldy);
    gui_s("font_style");       gui_i(x->x_gui.x_font_style);
    gui_s("font_size");        gui_i(x->x_gui.x_fontsize);
    gui_s("background_color"); gui_s(x->x_gui.x_bcol->s_name);
    gui_s("foreground_color"); gui_s(x->x_gui.x_fcol->s_name);
    gui_s("label_color");      gui_s(x->x_gui.x_lcol->s_name);
    gui_s("interactive");      gui_i(x->x_gui.x_click);
    gui_s("dial_width");       gui_i(x->x_dial_w);
    gui_s("off_circle_width"); gui_i(x->x_circle_w);
    gui_s("on_circle_width");  gui_i(x->x_frag_w);

    gui_end_array();
    gui_end_vmess();
}

static void knob_bang(t_knob *x)
{
    double out;
    
    out = x->x_val;
    if((out < 1.0e-10)&&(out > -1.0e-10))
    out = 0.0;

    outlet_float(x->x_gui.x_obj.ob_outlet, out);
    if(iemgui_has_snd(&x->x_gui) && x->x_gui.x_snd->s_thing)
    iemgui_out_float(&x->x_gui, 0, 0, out);
}

static void knob_dialog(t_knob *x, t_symbol *s, int argc, t_atom *argv)
{
    //post("knob_dialog %d args", argc);
    if (atom_getintarg(21, argc, argv))
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
    int w = (int)atom_getintarg(0, argc, argv);
    int h = (int)atom_getintarg(1, argc, argv);
    double min = (double)atom_getfloatarg(2, argc, argv);
    double max = (double)atom_getfloatarg(3, argc, argv);
    int lilo = (int)atom_getintarg(4, argc, argv);
    int steady = (int)atom_getintarg(17, argc, argv);
    int sr_flags;

    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(steady)
       x->x_steady = 1;
    else
       x->x_steady = 0;
    sr_flags = iemgui_dialog(&x->x_gui, argc, argv);
    knob_interactive(x, atom_getintarg(20, argc, argv));
    if (argc >= 26)
    {
        /*
        post("knob width %d %d %d",
            atom_getintarg(23, argc, argv),
            atom_getintarg(24, argc, argv),
            atom_getintarg(25, argc, argv));
        */
        knob_width(x, gensym("width"), 3, argv+23);
    }
    x->x_gui.x_h = iemgui_clip_size(h);
    knob_check_height(x, h);
    knob_check_minmax(x, min, max);
    knob_set(x, x->x_val);
    iemgui_draw_config(&x->x_gui);
    iemgui_draw_io(&x->x_gui, sr_flags);
    knob_io_draw_move(&x->x_gui);
    scalehandle_draw(&x->x_gui);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
}

static void knob_motion(t_knob *x, t_floatarg dx, t_floatarg dy)
{
    t_float old = x->x_val;
    if (x->x_gui.x_reverse)
        dy = -dy;
    if(x->x_gui.x_finemoved)
       x->x_pos -= dy * 0.01 * ((t_float)(abs(x->x_max - x->x_min))/ x->x_gui.x_h);
    else
       x->x_pos -= dy * ((t_float)(abs(x->x_max - x->x_min))/ x->x_gui.x_h);

    if (!x->x_gui.x_reverse)
    {
        if(x->x_pos > x->x_max)
            x->x_pos = x->x_max;
        if(x->x_pos < x->x_min)
            x->x_pos = x->x_min;
    }
    else
    {
        if(x->x_pos < x->x_max)
            x->x_pos = x->x_max;
        if(x->x_pos > x->x_min)
            x->x_pos = x->x_min;        
    }

/* ico@vt.edu 2020-09-25
DO NOT ERASE -- this took 2 days to flesh out from the old code and can be now reused for other purposes
t_float norm_val = (x->x_pos - x->x_min) / (x->x_max - x->x_min);
t_float log_norm_val = log(1 + (norm_val * 99))/(log(100));
t_float exp_norm_val = (exp(norm_val * log(100))/exp(log(100)) - 0.01) / 0.99;
post("norm_val=%f log=%f log*norm=%f exp=%f exp*norm=%f",
    norm_val,
    log(1 + (norm_val * 99))/(log(100)),
    norm_val * log(1 + (norm_val * 99))/(log(100)),
    (exp(norm_val * log(100))/exp(log(100)) - 0.01) / 0.99,
    norm_val * exp(norm_val * log(100))/exp(log(100))
    );
t_float norm2log = log_norm_val * (abs(x->x_max-x->x_min)) + x->x_min;
t_float log2norm = (exp(log_norm_val * log(100))/exp(log(100)) - 0.01) / 0.99 * (abs(x->x_max-x->x_min)) + x->x_min;
post("norm2log=%f log2norm=%f", norm2log, (exp(log_norm_val * log(100))/exp(log(100)) - 0.01) / 0.99 * (abs(x->x_max-x->x_min)) + x->x_min);
*/

    if (x->x_lin0_log1)
    {
        t_float norm_val = (x->x_pos - x->x_min) / (x->x_max - x->x_min);
        if (x->x_gui.x_reverse)
        {
            x->x_val = x->x_max - (exp((1 - norm_val) * log(100))/exp(log(100)) - 0.01) / 0.99 * (x->x_max-x->x_min);
        }
        else
        {
            x->x_val = (exp(norm_val * log(100))/exp(log(100)) - 0.01) / 0.99 * (abs(x->x_max-x->x_min)) + x->x_min;
        }
    }
    else
        x->x_val = x->x_pos;

    if (!x->x_gui.x_reverse)
    {
        if(x->x_pos >= x->x_max)
            x->x_val = x->x_max;
        if(x->x_pos <= x->x_min)
            x->x_val = x->x_min;
    }
    else
    {
        if(x->x_pos <= x->x_max)
            x->x_val = x->x_max;
        if(x->x_pos >= x->x_min)
            x->x_val = x->x_min;       
    }

    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        knob_bang(x);
    }
}

static void knob_click(t_knob *x, t_floatarg xpos, t_floatarg ypos,
              t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    if (x->x_gui.x_click)
    {
        if(!x->x_steady)
        {
            t_float center_x = x->x_gui.x_obj.te_xpix + (x->x_gui.x_h/2);
            t_float center_y = x->x_gui.x_obj.te_ypix + (x->x_gui.x_h/2);
            t_float angle = atan2(xpos - center_x, ypos - center_y) * 180.0 / 3.14159;
            if (xpos < center_x)
                angle = 360.0 + angle;
            angle = 360 - angle;
            if (angle < 14) angle = 14;
            if (angle > 346) angle = 346;
            t_float norm_val = (angle - 14)/332;

            if (x->x_lin0_log1)
            {
                if (x->x_gui.x_reverse)
                {
                    x->x_val = x->x_max - (exp((1 - norm_val) * log(100))/exp(log(100)) - 0.01) / 0.99 * (x->x_max-x->x_min);
                }
                else
                {
                    x->x_val = (exp(norm_val * log(100))/exp(log(100)) - 0.01) / 0.99 * (abs(x->x_max-x->x_min)) + x->x_min;
                }
                x->x_pos = ((x->x_max - x->x_min) * norm_val) + x->x_min;
            }
            else
            {
                x->x_val = ((x->x_max - x->x_min) * norm_val) + x->x_min;
                x->x_pos = x->x_val;
            }
        }
        if (x->x_gui.x_reverse)
        {
            if(x->x_val < x->x_max)
                x->x_val = x->x_max;
            if(x->x_val > x->x_min)
                x->x_val = x->x_min;
        }
        else
        {
            if(x->x_val > x->x_max)
                x->x_val = x->x_max;
            if(x->x_val < x->x_min)
                x->x_val = x->x_min;
        }
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        knob_bang(x);
        glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g, (t_glistmotionfn)knob_motion,
    	       0, knob_list, xpos, ypos, 0);
    }
}

static void knob_list(void *z, t_symbol *s, int ac, t_atom *av)
{
    t_knob *x = (t_knob*)z;
    if (ac == 2 && IS_A_FLOAT(av,0) && IS_A_SYMBOL(av,1))
    {
        // we allow shift to propagate in both focused modes 1 and 2
        // so as to enable fine movement that may be used in mode 1
        if (!strcmp("Shift", av[1].a_w.w_symbol->s_name))
        {
            x->x_gui.x_finemoved = (int)av[0].a_w.w_float;
            //post("...Shift %d", x->x_gui.x_finemoved);
        }
    }
}

static int knob_newclick(t_gobj *z, struct _glist *glist,
                int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_knob* x = (t_knob *)z;

    if (x->x_gui.x_click)
    {
        if (doit)
        {
        knob_click (x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift,
                   0, (t_floatarg)alt);
        if (shift)
            x->x_gui.x_finemoved = 1;
        else
            x->x_gui.x_finemoved = 0;
        }
    }
    return (1);
}

static void knob_set(t_knob *x, t_floatarg f)
{
    if (x->x_gui.x_reverse)    /* bugfix */
    {
        if (f > x->x_min)
           f = x->x_min;
        if (f < x->x_max)
            f = x->x_max;
    }
    else
    {
        if (f > x->x_max)
            f = x->x_max;
        if (f < x->x_min)
            f = x->x_min;
    }
    x->x_val = f;
    if (x->x_lin0_log1)
    {
        if (!x->x_gui.x_reverse && x->x_val == x->x_min)
            x->x_pos = x->x_min;
        else if (x->x_gui.x_reverse && x->x_val == x->x_max)
            x->x_pos = x->x_max;
        else
        {
            t_float norm_val = (x->x_val - x->x_min) / (x->x_max - x->x_min);
            if (x->x_gui.x_reverse)
            {
                t_float log_norm_val = log(100 - (norm_val * 99))/(log(100));
                x->x_pos = log_norm_val * (abs(x->x_max-x->x_min)) + x->x_max;
            }
            else
            {
                t_float log_norm_val = log(1 + (norm_val * 99))/(log(100));
                x->x_pos = log_norm_val * (abs(x->x_max-x->x_min)) + x->x_min;
            }
        }
    }
    else
    {
        x->x_pos = x->x_val;
    }
    if (glist_isvisible(x->x_gui.x_glist) && x->x_gui.x_vis)
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void knob_float(t_knob *x, t_floatarg f)
{
    knob_set(x, f);
    if (x->x_gui.x_put_in2out)
    knob_bang(x);
}

static void knob_size(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_gui.x_h = iemgui_clip_size((int)atom_getintarg(0, ac, av));
    if(ac > 1)
    knob_check_height(x, (int)atom_getintarg(1, ac, av));
    iemgui_size(&x->x_gui);
}

static void knob_delta(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_delta(&x->x_gui, s, ac, av);
}

static void knob_pos(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_pos(&x->x_gui, s, ac, av);
}

static void knob_range(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    knob_check_minmax(x, (double)atom_getfloatarg(0, ac, av),
             (double)atom_getfloatarg(1, ac, av));
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"minimum_range",x->x_min);
        properties_set_field_int(properties,"maximum_range",x->x_max);
    }
}

static void knob_width(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    if (ac >= 1)
        x->x_dial_w = atom_getfloatarg(0, ac, av);
    if (ac >= 2)
        x->x_circle_w = atom_getfloatarg(1, ac, av);
    if (ac == 3)
        x->x_frag_w = atom_getfloatarg(2, ac, av);
    //post("knob_width updated values %d %d %d", x->x_dial_w, x->x_circle_w, x->x_frag_w);
    knob_draw(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    //knob_io_draw_move(&x->x_gui);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
}

static void knob_color(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_color(&x->x_gui, s, ac, av);
}

static void knob_send(t_knob *x, t_symbol *s)
{
    iemgui_send(&x->x_gui, s);
}

static void knob_receive(t_knob *x, t_symbol *s)
{
    iemgui_receive(&x->x_gui, s);
}

static void knob_label(t_knob *x, t_symbol *s)
{
    iemgui_label(&x->x_gui, s);
}

static void knob_label_pos(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_pos(&x->x_gui, s, ac, av);
}

static void knob_label_font(t_knob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_font(&x->x_gui, s, ac, av);
}

static void knob_log(t_knob *x)
{
    x->x_lin0_log1 = 1;
    //error("knob: lin and log commands are not anymore supported. Ignoring...");
    knob_check_minmax(x, x->x_min, x->x_max);
    //post("knob_log %lx", x->x_gui);
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"log_scaling",x->x_lin0_log1);
    }
}

static void knob_lin(t_knob *x)
{
    //error("knob: lin and log commands are not anymore supported. Ignoring...");
    x->x_lin0_log1 = 0;
    //post("knob_lin %lx", x->x_gui);
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"log_scaling",x->x_lin0_log1);
    }
}

static void knob_init(t_knob *x, t_floatarg f)
{
    iemgui_init(&x->x_gui, f);
}

static void knob_steady(t_knob *x, t_floatarg f)
{
    x->x_steady = (f==0.0)?0:1;
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
        properties_set_field_int(properties,"steady_on_click",x->x_steady);
}

#define LB_LOAD 0 /* from g_canvas.h */

static void knob_loadbang(t_knob *x, t_floatarg action)
{
    if (action == LB_LOAD && x->x_gui.x_loadinit)
    {
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    knob_bang(x);
    } 
}

/* we may no longer need h_dragon... */
static void knob__clickhook(t_scalehandle *sh, int newstate)
{
    t_knob *x = (t_knob *)(sh->h_master);
    if (newstate)
    {
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
        if (!sh->h_scale) /* click on a label handle */
            scalehandle_click_label(sh);
    }
    /* We no longer need this "clickhook", as we can handle the dragging
       either in the GUI (for the label handle) or or in canvas_doclick */
    //iemgui__clickhook3(sh,newstate);
    sh->h_dragon = newstate;
}

static void knob__motionhook(t_scalehandle *sh,
                    t_floatarg mouse_x, t_floatarg mouse_y)
{
    if (sh->h_scale)
    {
        t_knob *x = (t_knob *)(sh->h_master);
        int width = x->x_gui.x_h,
            height = x->x_gui.x_h;
        int x1, y1, x2, y2, d;
        x1 = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
        y1 = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
        x2 = x1 + width;
        y2 = y1 + height;

        /* This is convoluted, but I can't think of another
           way to get this behavior... */
        if (mouse_x <= x2)
        {
            if (mouse_y > y2)
                d = mouse_y - y2;
            else if (abs(mouse_y - y2) < abs(mouse_x - x2))
                d = mouse_y - y2;
            else
                d = mouse_x - x2;
        }
        else
        {
            if (mouse_y <= y2)
                d = mouse_x - x2;
            else
                d = maxi(mouse_y - y2, mouse_x - x2);
        }
        sh->h_dragx = d;
        sh->h_dragy = d;
        scalehandle_drag_scale(sh);

        width = maxi(width + d, IEM_GUI_MINSIZE);
        height = width;

        x->x_gui.x_w = width;
        x->x_gui.x_h = height;
        /* recalculate stuff */
        knob_check_height(x, height);
        knob_check_minmax(x, x->x_min, x->x_max);
        knob_set(x, x->x_val);

        if (glist_isvisible(x->x_gui.x_glist))
        {
            knob_draw_config(x, x->x_gui.x_glist);
            scalehandle_unclick_scale(sh);
        }

        int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            int new_w = x->x_gui.x_w + sh->h_dragx;
            // This should actually be "size", but we're using the
            // "width" input in dialog_iemgui and just relabelling it
            // as a kluge.
            properties_set_field_int(properties, "size", new_w);
        }
    }
    scalehandle_dragon_label(sh, mouse_x, mouse_y);
}

static void knob_interactive(t_knob *x, t_floatarg f)
{
    if ((int)f == 0 || (int)f == 1)
    {
        x->x_gui.x_click = (int)f;
        iemgui_update_properties(&x->x_gui, IEM_GUI_PROP_INTERACTIVE);
    }

}

static void *knob_new(t_symbol *s, int argc, t_atom *argv)
{
    t_knob *x = (t_knob *)pd_new(knob_class);
    int bflcol[]={-262144, -1, -1};
    int w=IEM_KNOB_DEFAULTSIZE, h=IEM_KNOB_DEFAULTSIZE;
    int lilo=0, ldx=-2, ldy=-8;
    int fs=8, v=0, steady=1;
    double min=0.0, max=(double)(IEM_SL_DEFAULTSIZE-1);

    iem_inttosymargs(&x->x_gui, 0);
    iem_inttofstyle(&x->x_gui, 0);

    x->x_gui.x_bcol = gensym("#FCFCFCFF");
    x->x_gui.x_fcol = gensym("#000000FF");
    x->x_gui.x_lcol = gensym("#000000FF");
    x->x_gui.x_click = 1;
    x->x_dial_w = 2;
    x->x_circle_w = 3;
    x->x_frag_w = 4;

    if((argc >= 17)&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)
       &&IS_A_FLOAT(argv,2)&&IS_A_FLOAT(argv,3)
       &&IS_A_FLOAT(argv,4)&&IS_A_FLOAT(argv,5)
       &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
       &&(IS_A_SYMBOL(argv,7)||IS_A_FLOAT(argv,7))
       &&(IS_A_SYMBOL(argv,8)||IS_A_FLOAT(argv,8))
       &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)
       &&IS_A_FLOAT(argv,11)&&IS_A_FLOAT(argv,12)
       &&IS_A_FLOAT(argv,16))
   {
        w = (int)atom_getintarg(0, argc, argv);
        h = (int)atom_getintarg(1, argc, argv);
        min = (double)atom_getfloatarg(2, argc, argv);
        max = (double)atom_getfloatarg(3, argc, argv);
        lilo = (int)atom_getintarg(4, argc, argv);
        iem_inttosymargs(&x->x_gui, atom_getintarg(5, argc, argv));
        iemgui_new_getnames(&x->x_gui, 6, argv);
        ldx = (int)atom_getintarg(9, argc, argv);
        ldy = (int)atom_getintarg(10, argc, argv);
        iem_inttofstyle(&x->x_gui, atom_getintarg(11, argc, argv));
        fs = (int)atom_getintarg(12, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+13, argv+14, argv+15);
        v = (int)atom_getintarg(16, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 6, 0);
    if ((argc >= 18)&&IS_A_FLOAT(argv,17))
        steady = (int)atom_getintarg(17, argc, argv);
    if ((argc >= 19)&&IS_A_FLOAT(argv,18))
        x->x_gui.x_click = (int)atom_getintarg(18, argc, argv);

    if ((argc >= 20)&&IS_A_FLOAT(argv,19))
        x->x_dial_w = (int)atom_getintarg(19, argc, argv);
    if ((argc >= 21)&&IS_A_FLOAT(argv,20))
        x->x_circle_w = (int)atom_getintarg(20, argc, argv);
    if ((argc >= 22)&&IS_A_FLOAT(argv,21))
        x->x_frag_w = (int)atom_getintarg(21, argc, argv);

    x->x_gui.x_draw = (t_iemfunptr)knob_draw;
/*
    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
*/
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(steady != 0) steady = 1;
    x->x_steady = steady;
/*
    if (!strcmp(x->x_gui.x_snd->s_name, "empty"))
        x->x_gui.x_fsf.x_snd_able = 0;
    if (!strcmp(x->x_gui.x_rcv->s_name, "empty"))
        x->x_gui.x_fsf.x_rcv_able = 0;
*/
    if (iem_fstyletoint(&x->x_gui) == 1)
    {
        //strcpy(x->x_gui.x_font, "helvetica");
    }
    else if (iem_fstyletoint(&x->x_gui) == 2)
    {
        //strcpy(x->x_gui.x_font, "times");
    }
    else
    {
        x->x_gui.x_font_style = 0;
        //strcpy(x->x_gui.x_font, "courier");
    }
    if (iemgui_has_rcv(&x->x_gui))
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    if (fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    x->x_gui.x_h = iemgui_clip_size(h);
    knob_check_height(x, h);
    if (x->x_gui.x_loadinit)
        x->x_val = v;
    else
        x->x_val = 0;
    knob_check_minmax(x, min, max);
    knob_set(x, x->x_val);
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    outlet_new(&x->x_gui.x_obj, &s_float);
    x->x_gui.x_obj.te_iemgui = 1;

    x->x_gui.x_handle = scalehandle_new((t_object *)x,
        x->x_gui.x_glist, 1, knob__clickhook, knob__motionhook);
    x->x_gui.x_lhandle = scalehandle_new((t_object *)x,
        x->x_gui.x_glist, 0, knob__clickhook, knob__motionhook);

    return (x);
}



static void knob_free(t_knob *x)
{
    if (iemgui_has_rcv(&x->x_gui))
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(&x->x_gui);
}

void knob_setup(void)
{
    knob_class = class_new(gensym("knob"), (t_newmethod)knob_new,
                  (t_method)knob_free, sizeof(t_knob), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)knob_new, gensym("knob"), A_GIMME, 0);
    class_addbang(knob_class,knob_bang);
    class_addfloat(knob_class,knob_float);

    class_addlist(knob_class, knob_list);
    class_addmethod(knob_class, (t_method)knob_click, gensym("click"),
            A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_motion, gensym("motion"),
            A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_dialog, gensym("dialog"),
            A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_loadbang, gensym("loadbang"),
        A_DEFFLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_size, gensym("size"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_range, gensym("range"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_color, gensym("color"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_log, gensym("log"), 0);
    class_addmethod(knob_class, (t_method)knob_lin, gensym("lin"), 0);
    class_addmethod(knob_class, (t_method)knob_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_steady, gensym("steady"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_interactive, gensym("interactive"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_width, gensym("width"), A_GIMME, 0);
    if(!iemgui_key_sym)
    iemgui_key_sym = gensym("#keyname");
    knob_widgetbehavior.w_getrectfn =    knob_getrect;
    knob_widgetbehavior.w_displacefn =   iemgui_displace;
    knob_widgetbehavior.w_displacefnwtag = iemgui_displace_withtag;
    knob_widgetbehavior.w_selectfn =     iemgui_select;
    knob_widgetbehavior.w_activatefn =   NULL;
    knob_widgetbehavior.w_deletefn =     iemgui_delete;
    knob_widgetbehavior.w_visfn =        iemgui_vis;
    knob_widgetbehavior.w_clickfn =      knob_newclick;
#if PD_MINOR_VERSION < 37 /* TODO: remove old behaviour in exactly 2 months from now */
    knob_widgetbehavior.w_propertiesfn = knob_properties;;
    knob_widgetbehavior.w_savefn =       knob_save;
#else
    class_setpropertiesfn(knob_class, &knob_properties);
    class_setsavefn(knob_class, &knob_save);
#endif
    class_setwidget(knob_class, &knob_widgetbehavior);
    class_sethelpsymbol(knob_class, gensym("knob"));
}
