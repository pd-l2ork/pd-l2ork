/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "m_imp.h"
#include "g_all_guis.h"
#include <math.h>

t_symbol *s_empty;
t_class *scalehandle_class;

int iemgui_color_hex[] = {
    0xfcfcfc, 0xa0a0a0, 0x404040, 0xfce0e0, 0xfce0c0, 0xfcfcc8, 0xd8fcd8, 0xd8fcfc, 0xdce4fc, 0xf8d8fc,
    0xe0e0e0, 0x7c7c7c, 0x202020, 0xfc2828, 0xfcac44, 0xe8e828, 0x14e814, 0x28f4f4, 0x3c50fc, 0xf430f0,
    0xbcbcbc, 0x606060, 0x000000, 0x8c0808, 0x583000, 0x782814, 0x285014, 0x004450, 0x001488, 0x580050
};

int iemgui_clip_size(int size) {return maxi(size,IEM_GUI_MINSIZE);}
int iemgui_clip_font(int size) {return maxi(size,IEM_FONT_MINSIZE);}
static void scalehandle_check_and_redraw(t_iemgui *x);

/* forward declarations */
void iemgui_update_properties(t_iemgui *x, int option);
void properties_set_field_int(t_int props, const char *gui_field, int value);
void properties_set_field_float(t_int props, const char *gui_field, t_floatarg value);
void properties_set_field_symbol(t_int props, const char *gui_field, t_symbol *value);

/* helper function to negate legacy draw offset for labels
*/
void iemgui_getrect_legacy_label(t_iemgui *x, int *xp1, int *yp1)
{
    *xp1 -= x->legacy_x;
    *yp1 -= x->legacy_y;
}

static int iemgui_modulo_color(int col)
{
    const int IEM_GUI_MAX_COLOR = 30;
    col %= IEM_GUI_MAX_COLOR;
    if (col<0) col += IEM_GUI_MAX_COLOR;
    return col;
}

t_symbol *iemgui_dollar2raute(t_symbol *s)
{
    char buf[MAXPDSTRING+1], *s1, *s2;
    if (strlen(s->s_name) >= MAXPDSTRING)
        return (s);
    for (s1 = s->s_name, s2 = buf; ; s1++, s2++)
    {
        if (*s1 == '$' && *s1 && isdigit(s1[1]))
            *s2 = '#';
        else if (!(*s2 = *s1))
            break;
    }
    return(gensym(buf));
}

t_symbol *iemgui_raute2dollar(t_symbol *s)
{
    char buf[MAXPDSTRING+1], *s1, *s2;
    if (strlen(s->s_name) >= MAXPDSTRING)
        return (s);
    for (s1 = s->s_name, s2 = buf; ; s1++, s2++)
    {
        if (*s1 == '#' && *s1 && isdigit(s1[1]))
            *s2 = '$';
        else if (!(*s2 = *s1))
            break;
    }
    return(gensym(buf));
}

void iemgui_verify_snd_ne_rcv(t_iemgui *x)
{
    x->x_put_in2out = 
        !(iemgui_has_snd(x) && iemgui_has_rcv(x) && x->x_snd==x->x_rcv);
}

t_symbol *iemgui_getfloatsym(t_atom *a)
{
    if (IS_A_SYMBOL(a,0)) return (atom_getsymbol(a));
    if (IS_A_FLOAT(a,0)) {
        char str[40];
        sprintf(str, "%d", (int)atom_getint(a));
        return gensym(str);
    }
    return s_empty;
}
t_symbol *iemgui_getfloatsymarg(int i, int argc, t_atom *argv)
{
    if (i>=0 && i<argc) return iemgui_getfloatsym(argv+i);
    return s_empty;
}

void iemgui_new_getnames(t_iemgui *x, int indx, t_atom *argv)
{
    if (argv)
    {
        x->x_snd = iemgui_getfloatsym(argv+indx+0);
        x->x_rcv = iemgui_getfloatsym(argv+indx+1);
        x->x_lab = iemgui_getfloatsym(argv+indx+2);
    }
    else x->x_snd = x->x_rcv = x->x_lab = s_empty;
    x->x_snd_unexpanded = x->x_rcv_unexpanded = x->x_lab_unexpanded = 0;
    x->x_binbufindex = indx;
    x->x_labelbindex = indx + 3;
}

/* initialize a single symbol in unexpanded form.  We reach into the
   binbuf to grab them; if there's nothing there, set it to the
   fallback; if still nothing, set to "empty". */
static void iemgui_init_sym2dollararg(t_iemgui *x, t_symbol **symp,
    int indx, t_symbol *fallback)
{
    if (!*symp)
    {
        t_binbuf *b = x->x_obj.ob_binbuf;
        if (binbuf_getnatom(b) > indx)
        {
            char buf[80];
            atom_string(binbuf_getvec(b) + indx, buf, 80);
            *symp = gensym(buf);
        }
        else if (fallback)
            *symp = fallback;
        else *symp = s_empty;
    }
}

static t_symbol *color2symbol(t_symbol *col)
{
    char *start = col->s_name + 1;

    if ('#' == col->s_name[0] && strlen(start) == 8)
    {
        char expanded[10];
        sprintf(expanded, "#%c%c%c%c%c%c%c%c",
            start[6], start[7],
            start[0], start[1],
            start[2], start[3],
            start[4], start[5]);
        return gensym(expanded);
    }
    else
    {
        // this is a hex that is shorter than 8 values, or
        // a non hex text that we return verbatim
        return col;
    }
}


/* get the unexpanded versions of the symbols; initialize them if necessary. */
void iemgui_all_sym2dollararg(t_iemgui *x, t_symbol **srlsym)
{
    iemgui_init_sym2dollararg(x, &x->x_snd_unexpanded, x->x_binbufindex+1, x->x_snd);
    iemgui_init_sym2dollararg(x, &x->x_rcv_unexpanded, x->x_binbufindex+2, x->x_rcv);
    iemgui_init_sym2dollararg(x, &x->x_lab_unexpanded, x->x_labelbindex, x->x_lab);
    srlsym[0] = x->x_snd_unexpanded;
    srlsym[1] = x->x_rcv_unexpanded;
    srlsym[2] = x->x_lab_unexpanded;
}

void iemgui_all_col2save(t_iemgui *x, t_symbol **bflcol)
{
    bflcol[0] = color2symbol(x->x_bcol);
    bflcol[1] = color2symbol(x->x_fcol);
    bflcol[2] = color2symbol(x->x_lcol);
}

t_symbol *iemgui_getcolorarg(t_iemgui *x, int index, int argc, t_atom *argv)
{
    //post("iemgui_getcolorarg");
    char *classname;
    if (index < 0 || index >= argc || !argc)
        return 0;

    if (IS_A_FLOAT(argv, index))
    {
        // only legacy values are numerical, they are either
        // hex (0 or greater) value, or old rgb ones that can
        // convert to a six-channel rgb value
        int val = atom_getfloatarg(index, argc, argv);
        char output[10];
        if (val >= 0)
        {
            sprintf(output, "#%6.6xff",
                iemgui_color_hex[iemgui_modulo_color(val)]);
            return gensym(output);
        }
        else
        {
            sprintf(output, "#%6.6xff", val);
            return gensym(output);
        }
    }

    classname = class_getname(pd_class(&x->x_obj.te_pd));
    if (IS_A_SYMBOL(argv, index))
    {
        t_symbol *s = atom_getsymbolarg(index, argc, argv);
        //post("...%s", s->s_name);
        if ('#' == s->s_name[0])
        {
            char *start = s->s_name + 1;
            char expanded[10];
            int len = strlen(start);
            if (len == 1)
            {
                sprintf(expanded, "#%c%c%c%c%c%cff",
                    start[0], start[0],
                    start[0], start[0],
                    start[0], start[0]);
            }
            if (len == 3)
            {
                sprintf(expanded, "#%c%c%c%c%c%cff",
                    start[0], start[0],
                    start[1], start[1],
                    start[2], start[2]);
            }
            if (len == 4)
            {
                sprintf(expanded, "#%c%c%c%c%c%c%c%c",
                    start[1], start[1],
                    start[2], start[2],
                    start[3], start[3],
                    start[0], start[0]);
            }
            if (len == 6)
            {
                sprintf(expanded, "#%c%c%c%c%c%cff",
                    start[0], start[1],
                    start[2], start[3],
                    start[4], start[5]
                );
            }
            //post("...expanded=<%s> s=<%s>", expanded, s);
            if (len == 8)
            {
                sprintf(expanded, "#%c%c%c%c%c%c%c%c",
                    start[2], start[3],
                    start[4], start[5],
                    start[6], start[7],
                    start[0], start[1]
                );
            }

            //post("......%s", expanded);
            return gensym(expanded);
        }
        if (s == &s_)
            pd_error(x, "%s: empty symbol detected in hex color argument. "
                "Falling back to black. (Hit the sack.:)",
                classname);
        else
            // color is a word (e.g. "blue") or something invalid
            // which will be handled by the front-end CSS
            return s;
            /*
            pd_error(x, "%s: expected '#f', #fff', '#ffff', '#ffffff', or "
                "'#ffffffff' hex color format but got '%s'. Falling back "
                "to black.",
                classname, s->s_name);
            */
        return gensym("#ff000000");
    }
    pd_error(x, "%s: color method only accepts symbol or float arguments. "
        "Falling back to black.",
        classname);
    return gensym("#ff000000");
}

static t_symbol *colfromatomload(t_iemgui *x, t_atom *colatom)
{
    //post("colfromatomload");
    int color;
    char output[10];
    /* old-fashioned color argument, either a number or symbol
       evaluating to an integer */
    if (colatom->a_type == A_FLOAT)
    {
        color = atom_getfloat(colatom);
        if (color >= 0)
        {
            sprintf(output, "#%6.6xff",
                iemgui_color_hex[iemgui_modulo_color(color)]);
            return gensym(output);
        }
        else
        {
            color = -1 - color;
            color = ((color & 0x3f000) << 6)|
                ((color & 0xfc0) << 4)|((color & 0x3f) << 2);
            sprintf(output, "#%6.6xff", color);
            return gensym(output);
        }
    }
    else if (colatom->a_type == A_SYMBOL &&
        (isdigit(colatom->a_w.w_symbol->s_name[0]) ||
         colatom->a_w.w_symbol->s_name[0] == '-'))
    {
        color = atoi(colatom->a_w.w_symbol->s_name);
    }

    /* symbolic color */
    else return (iemgui_getcolorarg(x, 0, 1, colatom));
}

void iemgui_all_loadcolors(t_iemgui *x, t_atom *bcol, t_atom *fcol,
    t_atom *lcol)
{
    if (bcol) x->x_bcol = colfromatomload(x, bcol);
    if (fcol) x->x_fcol = colfromatomload(x, fcol);
    if (lcol) x->x_lcol = colfromatomload(x, lcol);
}


static t_symbol *colfromload(int color) {
    char output[10];
    if (color >= 0)
    {
        sprintf(output, "#%6.6xff",
            iemgui_color_hex[iemgui_modulo_color(color)]);
        return gensym(output);
    }
    else
    {
        color = -1 - color;
        color = ((color & 0x3f000) << 6)|
            ((color & 0xfc0) << 4)|((color & 0x3f) << 2);
        sprintf(output, "#%6.6xff", color);
        return gensym(output);
    }
}
void iemgui_all_colfromload(t_iemgui *x, int *bflcol)
{
    x->x_bcol = colfromload(bflcol[0]);
    x->x_fcol = colfromload(bflcol[1]);
    x->x_lcol = colfromload(bflcol[2]);
}

t_symbol *iemgui_compatible_colorarg(t_iemgui *x, int index, int argc, t_atom* argv)
{
    //post("iemgui_compatible_colorarg");
    if (index < 0 || index >= argc || !argc)
        return 0;
        /* old style, lossy int values */
    if (IS_A_FLOAT(argv, index))
    {
        int col = atom_getfloatarg(index, argc, argv);
        char output[10];
        if (col >= 0)
        {
            sprintf(output, "#%6.6xff",
                iemgui_color_hex[iemgui_modulo_color(col)]);
            //post("...col >= 0 %s", output);
            return gensym(output);
        }
        else
        {
            col = -1 - col;
            sprintf(output, "#%6.6xff", col);
            //post("...col < 0 %s", output);
            return gensym(output);
        }
    }
    return iemgui_getcolorarg(x, index, argc, argv);
}

void iemgui_all_raute2dollar(t_symbol **srlsym)
{
    srlsym[0] = iemgui_raute2dollar(srlsym[0]);
    srlsym[1] = iemgui_raute2dollar(srlsym[1]);
    srlsym[2] = iemgui_raute2dollar(srlsym[2]);
}

void iemgui_send(t_iemgui *x, t_symbol *s)
{
    t_symbol *snd;
    if (s == &s_) s = s_empty; //tb: fix for empty label
    int oldsndrcvable=0;
    if(iemgui_has_rcv(x)) oldsndrcvable += IEM_GUI_OLD_RCV_FLAG;
    if(iemgui_has_snd(x)) oldsndrcvable += IEM_GUI_OLD_SND_FLAG;

    snd = iemgui_raute2dollar(s);
    x->x_snd_unexpanded = snd;
    x->x_snd = snd = canvas_realizedollar(x->x_glist, snd);
    iemgui_verify_snd_ne_rcv(x);
    iemgui_draw_io(x, oldsndrcvable);
    iemgui_update_properties(x, IEM_GUI_PROP_SEND);
}

void iemgui_receive(t_iemgui *x, t_symbol *s)
{
    t_symbol *rcv;
    if (s == &s_) s = s_empty; //tb: fix for empty label
    int oldsndrcvable=0;
    if(iemgui_has_rcv(x)) oldsndrcvable += IEM_GUI_OLD_RCV_FLAG;
    if(iemgui_has_snd(x)) oldsndrcvable += IEM_GUI_OLD_SND_FLAG;

    rcv = iemgui_raute2dollar(s);
    x->x_rcv_unexpanded = rcv;
    rcv = canvas_realizedollar(x->x_glist, rcv);
    if(s!=s_empty)
    {
        if(rcv!=x->x_rcv)
        {
            if(iemgui_has_rcv(x))
                pd_unbind((t_pd *)x, x->x_rcv);
            x->x_rcv = rcv;
            pd_bind((t_pd *)x, x->x_rcv);
        }
    }
    else if(s==s_empty && iemgui_has_rcv(x))
    {
        pd_unbind((t_pd *)x, x->x_rcv);
        x->x_rcv = rcv;
    }
    iemgui_verify_snd_ne_rcv(x);
    iemgui_draw_io(x, oldsndrcvable);
    iemgui_update_properties(x, IEM_GUI_PROP_RECEIVE);
}

void iemgui_label(t_iemgui *x, t_symbol *s)
{
    t_symbol *old;
    if (s == &s_) s = s_empty; //tb: fix for empty label
    old = x->x_lab;
    t_symbol *lab = iemgui_raute2dollar(s);
    x->x_lab_unexpanded = lab;
    x->x_lab = lab = canvas_realizedollar(x->x_glist, lab);

    if (glist_isvisible(x->x_glist) && lab != old)
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_CONFIG);
    iemgui_update_properties(x, IEM_GUI_PROP_LABEL);
}

void iemgui_label_pos(t_iemgui *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_ldx = atom_getintarg(0, ac, av);
    x->x_ldy = atom_getintarg(1, ac, av);
    if (glist_isvisible(x->x_glist))
    {
        int x1 = x->x_ldx;
        int y1 = x->x_ldy;
        //iemgui_getrect_legacy_label(x, &x1, &y1);

        gui_vmess("gui_iemgui_label_coords", "xxii",
            glist_getcanvas(x->x_glist),
            x,
            x1,
            y1);
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_CONFIG);
    }
    iemgui_update_properties(x, IEM_GUI_PROP_LABEL_XY);
}

void iemgui_label_font(t_iemgui *x, t_symbol *s, int ac, t_atom *av)
{
    int f = atom_getintarg(0, ac, av);
    if (f<0 || f>2) f=0;
    x->x_font_style = f;
    x->x_fontsize = maxi(atom_getintarg(1, ac, av),4);
    if(glist_isvisible(x->x_glist))
    {
        gui_vmess("gui_iemgui_label_font", "xxssi",
            glist_getcanvas(x->x_glist),
            x,
            iemgui_typeface(x),
            sys_fontweight,
            x->x_fontsize);
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_CONFIG);
    }
    iemgui_update_properties(x, IEM_GUI_PROP_FONT);
}

// ico@vt.edu 2020-12-11: pass various css attribute changes
void iemgui_css(t_iemgui *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc > 2 && glist_isvisible(x->x_glist))
    {
        gui_start_vmess("gui_iemgui_css", "xxss",
            glist_getcanvas(x->x_glist),
            x,
            atom_getsymbol(argv)->s_name,
            atom_getsymbol(argv+1)->s_name
        );
        gui_start_array();

        int i;
        for (i = 2; i < argc; i++)
        {
            if (argv[i].a_type == A_FLOAT)
            {
                gui_f(atom_getfloat(argv+i));
            }
            else if (argv[i].a_type == A_SEMI)
            {
                gui_s(";");
            }
            else if (argv[i].a_type == A_COMMA)
            {
                gui_s(",");
            }
            else
            {
                gui_s(atom_getsymbol(argv+i)->s_name);
            }
        }

        gui_end_array();
        gui_end_vmess();
    }
}

//Sans: 84 x 10 (14) -> 6 x 10 -> 1.0
//Helvetica: 70 x 10 (14) -> 5 x 10 -> 0.83333
//Times: 61 x 10 (14) -> 4.357 x 10 -> 0.72619; 0.735 appears to work better

// We use this global var to check when getrect should report label:
// It should report it when drawing inside gop to see if we truly fit.
// Otherwise we should not report it while inside gop to avoid label being
// misinterpreted as part of the "hot" area of a widget (e.g. toggle)
extern int gop_redraw;

void iemgui_label_getrect(t_iemgui x_gui, t_glist *x,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    //fprintf(stderr,"gop_redraw = %d\n", gop_redraw);
    if (!gop_redraw || sys_legacy)
    {
        //fprintf(stderr,"ignoring label\n");
        return;
    }

    t_float width_multiplier;
    int label_length;
    int label_x1;
    int label_y1;
    int label_x2;
    int label_y2;
    int actual_fontsize; //seems tk does its own thing when rendering
    int actual_height;

    if (x->gl_isgraph && !glist_istoplevel(x))
    {
        if (x_gui.x_lab!=s_empty)
        {
            switch(x_gui.x_font_style)
            {
                case 1:  width_multiplier = 0.83333; break;
                case 2:  width_multiplier = 0.735;   break;
                default: width_multiplier = 1.0;     break;
            }
            actual_fontsize = x_gui.x_fontsize;
            actual_height = actual_fontsize;
            //exceptions
            if (x_gui.x_font_style == 0 &&
                (actual_fontsize == 8 || actual_fontsize == 13 ||
                actual_fontsize % 10 == 1 || actual_fontsize % 10 == 6 ||
                    (actual_fontsize > 48 && actual_fontsize < 100 &&
                    (actual_fontsize %10 == 4 || actual_fontsize %10 == 9))))
            {
                actual_fontsize += 1;
            }
            else if (x_gui.x_font_style == 1 && actual_fontsize >= 5 &&
                actual_fontsize < 13 && actual_fontsize % 2 == 1)
                actual_fontsize += 1;
            else if (x_gui.x_font_style == 2 && actual_fontsize >= 5 &&
                actual_fontsize % 2 == 1)
                actual_fontsize += 1;
            if (actual_height == 9)
                actual_height += 1;
            //done with exceptions

            width_multiplier = width_multiplier * (actual_fontsize * 0.6);

            label_length = strlen(x_gui.x_lab->s_name);
            label_x1 = *xp1 + x_gui.x_ldx;
            label_y1 = *yp1 + x_gui.x_ldy - actual_height/2;
            label_x2 = label_x1 + (label_length * width_multiplier);
            label_y2 = label_y1 + actual_height*1.1;

            //DEBUG
            // Note: we may want to port this debug code to the
            // new interface, but we haven't had a need to do it yet
            //fprintf(stderr,"%f %d %d\n", width_multiplier,
            //    label_length, x_gui.x_font_style);
            //sys_vgui(".x%zx.c delete iemguiDEBUG\n", x);
            //sys_vgui(".x%zx.c create rectangle %d %d %d %d "
            //    "-tags iemguiDEBUG\n",
            //    x, label_x1, label_y1, label_x2, label_y2);
            if (label_x1 < *xp1) *xp1 = label_x1;
            if (label_x2 > *xp2) *xp2 = label_x2;
            if (label_y1 < *yp1) *yp1 = label_y1;
            if (label_y2 > *yp2) *yp2 = label_y2;
            //DEBUG
            //sys_vgui(".x%zx.c delete iemguiDEBUG\n", x);
            //sys_vgui(".x%zx.c create rectangle %d %d %d %d "
            //    "-tags iemguiDEBUG\n", x, *xp1, *yp1, *xp2, *yp2);
        }
    }
}

#if 0 // future way of reordering stuff for iemgui_shouldvis
/*
    // some day when the object tagging is
    // properly done for all GUI objects
    glist_noselect(canvas);
    glist_select(canvas, y);
    t_gobj *yy = canvas->gl_list;
    if (yy != y)
    {
        while (yy && yy->g_next != y)
            yy = yy->g_next;
        // now we have yy which is right before our y graph
        t_rtext *yr = NULL;
        if (yy)
        {
            yr = glist_findrtext(canvas, (t_text *)yy);
        }
        if (yr)
        {
            fprintf(stderr,"lower\n");
            sys_vgui(".x%zx.c lower selected %s\n", canvas, rtext_gettag(yr));
            sys_vgui(".x%zx.c raise selected %s\n", canvas, rtext_gettag(yr));
            //canvas_raise_all_cords(canvas);
        }
        else
        {
            // fall back to legacy redraw for objects
            //   that are not patchable
            fprintf(stderr,"lower fallback redraw\n");
            canvas_redraw(canvas);
        }
    }
    else
    {
        // we get here if we are supposed to go
        //   all the way to the bottom
        fprintf(stderr,"lower to the bottom\n");
        sys_vgui(".x%zx.c lower selected\n", canvas);
    }
    glist_noselect(canvas);
*/
#endif

void iemgui_shouldvis(t_iemgui *x, int mode)
{
    gop_redraw = 1;
    if(gobj_shouldvis((t_gobj *)x, x->x_glist))
    {
        if (!x->x_vis)
        {
            //fprintf(stderr,"draw new %d\n", mode);
            iemgui_draw_new(x);
            canvas_fixlinesfor(glist_getcanvas(x->x_glist), (t_text*)x);
            x->x_vis = 1;
            if (x->x_glist != glist_getcanvas(x->x_glist))
            {
                /* if we are inside gop and just have had our object's
                   properties changed we'll adjust our layer position
                   to ensure that ordering is honored */
                t_canvas *canvas = glist_getcanvas(x->x_glist);
                //t_gobj *y = (t_gobj *)x->x_glist;
                // this crashes when the label goes invisible and then
                // needs to be made visible again, so for the time being
                // we use the brute force redraw below for both the
                // vis/unvis-ing of the object and its reordering
                //gobj_vis(y, canvas, 0);
                //gobj_vis(y, canvas, 1);
                // reorder it visually
                glist_redraw(canvas);
            }
        }
        //fprintf(stderr,"draw move x->x_w=%d\n", x->x_w);
        if      (mode==IEM_GUI_DRAW_MODE_NEW)    iemgui_draw_new(   x);
        else if (mode==IEM_GUI_DRAW_MODE_MOVE)   iemgui_draw_move(  x);
        else if (mode==IEM_GUI_DRAW_MODE_CONFIG) iemgui_draw_config(x);
        else bug("iemgui_shouldvis");
        scalehandle_check_and_redraw(x);
        canvas_fixlinesfor(glist_getcanvas(x->x_glist), (t_text*)x);
    }
    else if (x->x_vis)
    {
        //fprintf(stderr,"draw erase %d\n", mode);
        iemgui_draw_erase(x);
        x->x_vis = 0;
    }
    gop_redraw = 0;
}

void iemgui_size(t_iemgui *x)
{
    if (glist_isvisible(x->x_glist))
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_MOVE);
}

void iemgui_delta(t_iemgui *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_obj.te_xpix += atom_getintarg(0, ac, av);
    x->x_obj.te_ypix += atom_getintarg(1, ac, av);
    if(glist_isvisible(x->x_glist))
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_MOVE);
}

void iemgui_pos(t_iemgui *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_obj.te_xpix = atom_getintarg(0, ac, av);
    x->x_obj.te_ypix = atom_getintarg(1, ac, av);
    if (glist_isvisible(x->x_glist))
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_MOVE);
}

int iemgui_old_color_args(int argc, t_atom *argv)
{
    int gotsym = 0, gotfloat = 0;
    gotsym += atom_getsymbolarg(0, argc, argv) != &s_;
    gotsym += atom_getsymbolarg(1, argc, argv) != &s_;
    gotsym += atom_getsymbolarg(2, argc, argv) != &s_;
    
    gotfloat += argc >=1 && argv[0].a_type == A_FLOAT;
    gotfloat += argc >=2 && argv[1].a_type == A_FLOAT;
    gotfloat += argc >=2 && argv[2].a_type == A_FLOAT;

    if (gotfloat && gotsym)
    {
        post("warning: unexpected mixing of symbol args with deprecated "
             "float color syntax.");
        return 1;
    }
    else if (gotfloat) return 1;
    else return 0;
}

void iemgui_color(t_iemgui *x, t_symbol *s, int ac, t_atom *av)
{
    //post("iemgui_color");
    if (ac)
    {
        if (ac >= 1 && x->x_color1)
        {
            if (x->x_color1 == 1)
                x->x_bcol = iemgui_compatible_colorarg(x, 0, ac, av);
            else if (x->x_color1 == 2)
                x->x_fcol = iemgui_compatible_colorarg(x, 0, ac, av);
            else
                x->x_lcol = iemgui_compatible_colorarg(x, 0, ac, av);
        }
        if (ac >= 2)
        {
                /* if there are only two args, the old style was to make
                   the 2nd argument the label color. So here we check for the
                   old-style float args and use that format if they are
                   present. */
            if (ac == 2 && iemgui_old_color_args(ac, av))
                x->x_lcol = iemgui_compatible_colorarg(x, 1, ac, av);
            else {
                if (x->x_color2)
                    if (x->x_color2 == 1)
                        x->x_bcol = iemgui_compatible_colorarg(x, 1, ac, av);
                    else if (x->x_color2 == 2)
                        x->x_fcol = iemgui_compatible_colorarg(x, 1, ac, av);
                    else
                        x->x_lcol = iemgui_compatible_colorarg(x, 1, ac, av);
            }
        }
        if (ac >= 3 && x->x_color3)
            if (x->x_color3 == 1)
                x->x_bcol = iemgui_compatible_colorarg(x, 2, ac, av);
            else if (x->x_color3 == 2)
                x->x_fcol = iemgui_compatible_colorarg(x, 2, ac, av);
            else
                x->x_lcol = iemgui_compatible_colorarg(x, 2, ac, av);
        if (glist_isvisible(x->x_glist))
        {
            x->x_draw(x, x->x_glist, IEM_GUI_DRAW_MODE_CONFIG);
            iemgui_label_draw_config(x);
        }
        iemgui_update_properties(x, IEM_GUI_PROP_COLORS);
    }
}

void iemgui_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_iemgui *x = (t_iemgui *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    if (glist_isvisible(glist))
        iemgui_shouldvis(x, IEM_GUI_DRAW_MODE_MOVE);
}

void iemgui_displace_withtag(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_iemgui *x = (t_iemgui *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    //iemgui_draw_move(x);
    canvas_fixlinesfor(glist_getcanvas(glist), (t_text *)z);
}

void iemgui_select(t_gobj *z, t_glist *glist, int selected)
{
    t_iemgui *x = (t_iemgui *)z;
    t_canvas *canvas=glist_getcanvas(glist);
    if (selected)
        x->x_selected = canvas;
    else
        x->x_selected = NULL;
    x->x_draw((void *)z, glist, IEM_GUI_DRAW_MODE_SELECT);
    if (selected < 2)
    {
        scalehandle_draw(x);
    }
    else
    {
        // exception where we get rid of handles when moving tiny objects
        // because tkpath's slowness sometimes makes mouse pointer go over
        // a handle and messes things up. we only do this when using
        // startmotion (see g_editor.c).
        // LATER: get rid of this because we will deal with this better using
        // the new toolkit.
        scalehandle_draw_erase2(x);
    }
    iemgui_label_draw_select(x);
    iemgui_tag_selected(x);
}

void iemgui_delete(t_gobj *z, t_glist *glist)
{
    canvas_deletelinesfor(glist, (t_text *)z);
}

void iemgui_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_iemgui *x = (t_iemgui *)z;
    if (gobj_shouldvis(z, glist))
    {
        if (vis)
            iemgui_draw_new(x);
        else
        {
            iemgui_draw_erase(x);
            sys_unqueuegui(z);
        }
        x->x_vis = vis;
    }
}

void iemgui_save(t_iemgui *x, t_symbol **srl, t_symbol **bflcol)
{
    if (srl) {
       srl[0] = x->x_snd;
       srl[1] = x->x_rcv;
       srl[2] = x->x_lab;
    }
    iemgui_all_sym2dollararg(x, srl);
    iemgui_all_col2save(x, bflcol);
}

void iemgui_properties(t_iemgui *x, t_symbol **srl)
{
    srl[0] = x->x_snd;
    srl[1] = x->x_rcv;
    srl[2] = x->x_lab;
    iemgui_all_sym2dollararg(x, srl);
    srl[0] = iemgui_dollar2raute(srl[0]);
    srl[1] = iemgui_dollar2raute(srl[1]);
    srl[2] = iemgui_dollar2raute(srl[2]);
}

int iemgui_dialog(t_iemgui *x, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    x->x_loadinit = !!atom_getintarg(5, argc, argv);
    srl[0] = iemgui_getfloatsymarg(7,argc,argv);
    srl[1] = iemgui_getfloatsymarg(8,argc,argv);
    srl[2] = iemgui_getfloatsymarg(9,argc,argv);
    x->x_ldx = atom_getintarg(10, argc, argv);
    x->x_ldy = atom_getintarg(11, argc, argv);
    int f = atom_getintarg(12, argc, argv);
    x->x_fontsize = maxi(atom_getintarg(13, argc, argv),4);
    //iemgui_all_loadcolors(&x->x_gui, argv+14, argv+15, argv+16);
    x->x_bcol = iemgui_getcolorarg(x, 14, argc, argv);
    x->x_fcol = iemgui_getcolorarg(x, 15, argc, argv);
    x->x_lcol = iemgui_getcolorarg(x, 16, argc, argv);
    //post("colors bfl %s %s %s", x->x_bcol->s_name, x->x_fcol->s_name, x->x_lcol->s_name);
    int oldsndrcvable=0;
    if(iemgui_has_rcv(x)) oldsndrcvable |= IEM_GUI_OLD_RCV_FLAG;
    if(iemgui_has_snd(x)) oldsndrcvable |= IEM_GUI_OLD_SND_FLAG;
    iemgui_all_raute2dollar(srl);

    // replace ascii code 11 (\v or vertical tab) with spaces
    // we do this so that the string with spaces can survive argc,argv
    // conversion when coming from dialog side of things where it is parsed
    char *c;
    for(c = srl[2]->s_name; c != NULL && *c != '\0'; c++)
    {
        if(*c == '\v')
        {
            *c = ' ';
        }
    }

    x->x_snd_unexpanded=srl[0]; srl[0]=canvas_realizedollar(x->x_glist, srl[0]);
    x->x_rcv_unexpanded=srl[1]; srl[1]=canvas_realizedollar(x->x_glist, srl[1]);
    x->x_lab_unexpanded=srl[2]; srl[2]=canvas_realizedollar(x->x_glist, srl[2]);
    if(srl[1]!=x->x_rcv)
    {
        if(iemgui_has_rcv(x))
            pd_unbind((t_pd *)x, x->x_rcv);
        x->x_rcv = srl[1];
        pd_bind((t_pd *)x, x->x_rcv);
    }
    x->x_snd = srl[0];
    x->x_lab = srl[2];
    if(f<0 || f>2) f=0;
    x->x_font_style = f;
    iemgui_verify_snd_ne_rcv(x);
    canvas_dirty(x->x_glist, 1);
    return oldsndrcvable;
}

void iem_inttosymargs(t_iemgui *x, int n)
{
    //post("iem_inttosymargs");
    x->x_loadinit = (n >>  0);
    x->x_locked = 0;
    x->x_reverse = 0;
    x->x_color1 = 1;
    x->x_color2 = 2;
    x->x_color3 = 3;
}

int iem_symargstoint(t_iemgui *x)
{
    return ((x->x_loadinit & 1) <<  0);
}

void iem_inttofstyle(t_iemgui *x, int n)
{
    x->x_font_style = (n >> 0);
    x->x_selected = NULL;
    x->x_finemoved = 0;
    x->x_put_in2out = 0;
    x->x_change = 0;
}

int iem_fstyletoint(t_iemgui *x)
{
    return ((x->x_font_style << 0) & 63);
}

char *iem_get_tag(t_canvas *glist, t_iemgui *iem_obj)
{
    t_gobj *y = (t_gobj *)iem_obj;
    t_object *ob = pd_checkobject(&y->g_pd);

    /* GOP objects are unable to call findrtext
       triggering consistency check error */
    t_rtext *yyyy = NULL;
    if (!glist->gl_isgraph || glist_istoplevel(glist))
        yyyy = glist_findrtext(glist_getcanvas(glist), (t_text *)&ob->ob_g);

    /* on GOP we cause segfault as text_gettag() returns bogus data */
    if (yyyy) return(rtext_gettag(yyyy));
    else return("bogus");
}

//----------------------------------------------------------------
// SCALEHANDLE COMMON CODE (by Mathieu, refactored from existing code)

extern t_int gfxstub_haveproperties(void *key);

int   mini(int   a, int   b) {return a<b?a:b;}
int   maxi(int   a, int   b) {return a>b?a:b;}
float minf(float a, float b) {return a<b?a:b;}
float maxf(float a, float b) {return a>b?a:b;}

extern t_class *my_canvas_class;

/* dynamically update properties dialog (if it exists) for iemgui mapped calls
   0 = colors (bfl)
   1 = send
   2 = receive
   3 = label
   4 = label x y
   5 = label font style and size
   6 = interactive
*/
void iemgui_update_properties(t_iemgui *x, int option)
{
    //post("iemgui_update_properties option=%d", option);
    t_symbol *srl[3];
    iemgui_properties(x, srl);

    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        switch(option) {
            case IEM_GUI_PROP_COLORS:
                properties_set_field_symbol(properties,"background_color", x->x_bcol->s_name);
                properties_set_field_symbol(properties,"foreground_color", x->x_fcol->s_name);
                properties_set_field_symbol(properties,"label_color", x->x_lcol->s_name);
                break;
            case IEM_GUI_PROP_SEND:
                properties_set_field_symbol(properties,"send_symbol",srl[0]);
                break;
            case IEM_GUI_PROP_RECEIVE:
                properties_set_field_symbol(properties,"receive_symbol",srl[1]);
                break;
            case IEM_GUI_PROP_LABEL:
                properties_set_field_symbol(properties,"label",srl[2]);
                break;
            case IEM_GUI_PROP_LABEL_XY:
                properties_set_field_int(properties,"x_offset",x->x_ldx);
                properties_set_field_int(properties,"y_offset",x->x_ldy);
                break;
            case IEM_GUI_PROP_FONT:
                properties_set_field_int(properties,"font_style",x->x_font_style);
                properties_set_field_int(properties,"font_size",x->x_fontsize);
                break;
            case IEM_GUI_PROP_INTERACTIVE:
                properties_set_field_int(properties,"interactive",x->x_click);
                break;
            case IEM_GUI_PROP_INIT:
                properties_set_field_int(properties,"init",x->x_loadinit);
                break;
        }
    }
}

// in 18 cases only, because canvas does not fit the pattern below.
// canvas has no label handle and has a motion handle
// but in the case of canvas, the "iemgui" tag is added (it wasn't the case originally)
void scalehandle_draw_select(t_scalehandle *h, int px, int py)
{
    t_object *x = h->h_master;
    t_canvas *canvas=glist_getcanvas(h->h_glist);

    int sx = h->h_scale ? SCALEHANDLE_WIDTH  : LABELHANDLE_WIDTH;
    int sy = h->h_scale ? SCALEHANDLE_HEIGHT : LABELHANDLE_HEIGHT;

    scalehandle_draw_erase(h);

    if (!h->h_vis) {
        gui_vmess("gui_iemgui_label_show_drag_handle", "xxiiiii",
            canvas, x, 1, px - sx, py - sy, h->h_scale, glist_istoplevel(h->h_glist));
        h->h_vis = 1;
    }
}

void scalehandle_draw_select2(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    t_class *c = pd_class((t_pd *)x);
    int sx, sy;
    if (c == my_canvas_class)
    {
        t_my_canvas *y = (t_my_canvas *)x;
        sx = y->x_vis_w;
        sy = y->x_vis_h;
    }
    else
    {
        int x1, y1, x2, y2;
        c->c_wb->w_getrectfn((t_gobj *)x,canvas, &x1, &y1, &x2, &y2);
        //iemgui_getrect_draw(x, &x1, &y1, &x2, &y2);
        sx = x2 - x1; sy = y2 - y1;
    }
    /* we're not drawing the scalehandle for the actual iemgui-- just
       the one for the label. Special case for [cnv] which for some reason
       allows a smaller selection area than its painted rectangle. */
    if (c == my_canvas_class)
        scalehandle_draw_select(x->x_handle,
            (int)(sx + SCALEHANDLE_WIDTH * 1.5) + 1,
            sy + SCALEHANDLE_HEIGHT);
    if (x->x_lab != s_empty)
        scalehandle_draw_select(x->x_lhandle, x->x_ldx + 5, x->x_ldy + 10);
}

void scalehandle_draw_erase(t_scalehandle *h)
{
    //t_canvas *canvas = glist_getcanvas(h->h_glist);
    if (!h->h_vis) return;
    gui_vmess("gui_iemgui_label_show_drag_handle", "xxiiiii",
        h->h_glist,
        h->h_master,
        0,
        0,
        0,
        h->h_scale,
        glist_istoplevel(h->h_glist));
    h->h_vis = 0;
}

void scalehandle_draw_erase2(t_iemgui *x)
{
    t_scalehandle *sh = (t_scalehandle *)(x->x_handle);
    t_scalehandle *lh = (t_scalehandle *)(x->x_lhandle);
    if (sh->h_vis) scalehandle_draw_erase(sh);
    if (lh->h_vis) scalehandle_draw_erase(lh);
}

void scalehandle_draw(t_iemgui *x)
{
    if (x->x_glist == glist_getcanvas(x->x_glist))
    {
        if (x->x_selected == x->x_glist) scalehandle_draw_select2(x);
        else scalehandle_draw_erase2(x);
    }
}

t_scalehandle *scalehandle_new(t_object *x, t_glist *glist, int scale,
    t_clickhandlefn chf, t_motionhandlefn mhf)
{
    t_scalehandle *h = (t_scalehandle *)pd_new(scalehandle_class);
    char buf[19]; // 3 + max size of %zx
    h->h_master = x;
    h->h_glist = glist;
    if (!scale) /* Only bind for labels-- scaling uses pd_vmess in g_editor.c */
    {
        sprintf(buf, "_l%zx", (t_uint)x);
        pd_bind((t_pd *)h, h->h_bindsym = gensym(buf));
    }
    else if (scale && pd_class((t_pd *)x) == my_canvas_class)
    {
        sprintf(buf, "_s%zx", (t_uint)x);
        pd_bind((t_pd *)h, h->h_bindsym = gensym(buf));
    }
    sprintf(h->h_outlinetag, "h%zux", (t_uint)h);
    h->h_dragon = 0;
    h->h_scale = scale;
    h->h_offset_x = 0;
    h->h_offset_y = 0;
    h->h_adjust_x = 0;
    h->h_adjust_y = 0;
    h->h_vis = 0;
    h->h_constrain = 0;
    sprintf(h->h_pathname, ".x%zx.h%zx", (t_uint)h->h_glist, (t_uint)h);
    h->h_clickfn = chf;
    h->h_motionfn = mhf;
    return h;
}

void scalehandle_free(t_scalehandle *h)
{
    if (!h->h_scale || pd_class((t_pd *)(h->h_master)) == my_canvas_class)
    { /* only binding label handles and my_canvas resize handle */
        pd_unbind((t_pd *)h, h->h_bindsym);
    }
    pd_free((t_pd *)h);
}

void properties_set_field_int(t_int props, const char *gui_field, int value)
{
    //post("properties_set_field_int %lx %zx %s %d", props, props, gui_field, value);
    char tagbuf[MAXPDSTRING];
    sprintf(tagbuf, ".gfxstub%zx", props);
    gui_vmess("gui_dialog_set_field", "ssi",
        tagbuf,
        gui_field,
        value);
}

void properties_set_field_float(t_int props, const char *gui_field, t_floatarg value)
{
    char tagbuf[MAXPDSTRING];
    sprintf(tagbuf, ".gfxstub%zx", props);
    gui_vmess("gui_dialog_set_field", "ssf",
        tagbuf,
        gui_field,
        value);
}

void properties_set_field_symbol(t_int props, const char *gui_field, t_symbol *value)
{
    char tagbuf[MAXPDSTRING];
    sprintf(tagbuf, ".gfxstub%zx", props);
    gui_vmess("gui_dialog_set_field", "sss",
        tagbuf,
        gui_field,
        value->s_name);
}

void scalehandle_dragon_label(t_scalehandle *h, float mouse_x, float mouse_y)
{
    if (h->h_dragon && !h->h_scale)
    {
        t_iemgui *x = (t_iemgui *)(h->h_master);
        int dx = (h->h_constrain == CURSOR_EDITMODE_RESIZE_Y) ? 0 :
            (int)mouse_x - (int)h->h_offset_x,
        dy = (h->h_constrain == CURSOR_EDITMODE_RESIZE_X) ? 0 :
            (int)mouse_y - (int)h->h_offset_y;
        h->h_dragx = dx;
        h->h_dragy = dy;

        t_int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            int new_x = x->x_ldx + h->h_dragx;
            int new_y = x->x_ldy + h->h_dragy;
            properties_set_field_int(properties, "x_offset", new_x);
            properties_set_field_int(properties, "y_offset", new_y);
        }
        x->x_ldx += dx;
        x->x_ldy += dy;

        if (glist_isvisible(x->x_glist))
        {
            //int xpos=text_xpix(&x->x_obj, x->x_glist);
            //int ypos=text_ypix(&x->x_obj, x->x_glist);
            //iemgui_getrect_legacy_label(x, &xpos, &ypos);
            t_canvas *canvas=glist_getcanvas(x->x_glist);
            gui_vmess("gui_iemgui_label_coords", "xxii",
                canvas,
                x,
                x->x_ldx,
                x->x_ldy);
            gui_vmess("gui_iemgui_label_displace_drag_handle", "xxii",
                canvas,
                x,
                dx,
                dy);
        }
    }
}

void scalehandle_click_label(t_scalehandle *h)
{
    t_iemgui *x = (t_iemgui *)h->h_master;
    if (glist_isvisible(x->x_glist))
    {
        //sys_vgui("lower %s\n", h->h_pathname);
        //t_scalehandle *othersh = x->x_handle;
        //sys_vgui("lower .x%zx.h%zx\n",
        //    (t_int)glist_getcanvas(x->x_glist), (t_int)othersh);
    }
    h->h_dragx = 0;
    h->h_dragy = 0;
}

void scalehandle_getrect_master(t_scalehandle *h, int *x1, int *y1, int *x2, int *y2) {
    t_iemgui *x = (t_iemgui *)h->h_master;
    t_class *c = pd_class((t_pd *)x);
    c->c_wb->w_getrectfn((t_gobj *)x,x->x_glist,x1,y1,x2,y2);
    //fprintf(stderr,"%d %d %d %d\n",*x1,*y1,*x2,*y2);
    //iemgui_getrect_draw((t_iemgui *)x, x1, y1, x2, y2);
    //fprintf(stderr,"%d %d %d %d\n",*x1,*y1,*x2,*y2);
    //printf("%s\n",c->c_name->s_name);
    if (c==my_canvas_class) {
        t_my_canvas *xx = (t_my_canvas *)x;
        *x2=*x1+xx->x_vis_w;
        *y2=*y1+xx->x_vis_h;
    }
}

void scalehandle_click_scale(t_scalehandle *h) {
    int x1,y1,x2,y2;
    t_iemgui *x = (t_iemgui *)h->h_master;
    scalehandle_getrect_master(h,&x1,&y1,&x2,&y2);
    if (glist_isvisible(x->x_glist))
    {
        //sys_vgui("lower %s\n", h->h_pathname);
        //sys_vgui(".x%x.c create prect %d %d %d %d -stroke $pd_colors(selection) -strokewidth 1 -tags %s\n",
        //    x->x_glist, x1, y1, x2, y2, h->h_outlinetag);
    }
    h->h_dragx = 0;
    h->h_dragy = 0;
}

// here we don't need to use glist_getcanvas(t_canvas *)
// because scalehandle on iemgui objects appears only when
// they are on their own canvas
void scalehandle_unclick_scale(t_scalehandle *h) {
    t_iemgui *x = (t_iemgui *)h->h_master;
    //sys_vgui(".x%x.c delete %s\n", x->x_glist, h->h_outlinetag);
    iemgui_io_draw_move(x);
    iemgui_select((t_gobj *)x, x->x_glist, 1);
    canvas_fixlinesfor(x->x_glist, (t_text *)x);
    canvas_getscroll(x->x_glist);
}

void scalehandle_drag_scale(t_scalehandle *h) {
    int x1,y1,x2,y2;
    t_iemgui *x = (t_iemgui *)h->h_master;
    scalehandle_getrect_master(h,&x1,&y1,&x2,&y2);
    if (glist_isvisible(x->x_glist))
    {
        //sys_vgui(".x%x.c coords %s %d %d %d %d\n", x->x_glist, h->h_outlinetag,
        //    x1, y1, x2+h->h_dragx, y2+h->h_dragy);
    }
}

static void scalehandle_clickhook(t_scalehandle *h, t_floatarg f,
    t_floatarg xxx, t_floatarg yyy)
{
    /* The label scalehandles do an end run around canvas_doclick,
       which is where the cursor gets set. So we go ahead and set
       the cursor here, too. "f" is our cursor number as defined
       by the CURSOR_* macros in canvas.h */
    canvas_setcursor(h->h_glist, (int)f);
    /* We also go ahead and set h_constrain here so we don't have
       to do it separately for each widget. Any widget that wants
       to constrain movement along an axis can just check that
       field against the CURSOR_* values in the motionhook callback. */
    h->h_constrain = f;
    h->h_offset_x = xxx;
    h->h_offset_y = yyy;
    h->h_clickfn(h, f);
}

static void scalehandle_motionhook(t_scalehandle *h,
    t_floatarg f1, t_floatarg f2)
{
    h->h_motionfn(h, f1, f2);
    // Now set the offset to the new mouse position
    h->h_offset_x = f1;
    h->h_offset_y = f2;
}

void iemgui__clickhook3(t_scalehandle *sh, int newstate) {
    if (!sh->h_dragon && newstate && sh->h_scale)
        scalehandle_click_scale(sh);
    else if (!sh->h_dragon && newstate && !sh->h_scale)
        scalehandle_click_label(sh);
    sh->h_dragon = newstate;
}

// function for updating of handles on iemgui objects
// we don't need glist_getcanvas() because handles are only
// drawn when object is selected on its canvas (instead of GOP)
static void scalehandle_check_and_redraw(t_iemgui *x)
{
    if (x->x_selected == x->x_glist)
        scalehandle_draw_select2(x);
}

//----------------------------------------------------------------
// IEMGUI refactor (by Mathieu)

void iemgui_tag_selected(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    if (x->x_selected)
        gui_vmess("gui_gobj_select", "xx", canvas, x);
    else
        gui_vmess("gui_gobj_deselect", "xx", canvas, x);
}

void iemgui_label_draw_new(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    int x1=text_xpix(&x->x_obj, x->x_glist) + (sys_legacy ? x->legacy_x : 0);
    int y1=text_ypix(&x->x_obj, x->x_glist) + (sys_legacy ? x->legacy_y : 0);
    iemgui_getrect_legacy_label(x, &x1, &y1);
    gui_vmess("gui_iemgui_label_new", "xxiissssi",
        canvas,
        x,
        x->x_ldx,
        x->x_ldy,
        x->x_lcol->s_name,
        x->x_lab != s_empty ? x->x_lab->s_name : "",
        iemgui_typeface(x),
        sys_fontweight,
        x->x_fontsize);
}

void iemgui_label_draw_move(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    //int x1=text_xpix(&x->x_obj, x->x_glist)+x->legacy_x;
    //int y1=text_ypix(&x->x_obj, x->x_glist)+x->legacy_y;
    //iemgui_getrect_legacy_label(x, &x1, &y1);
    //sys_vgui(".x%zx.c coords %zxLABEL %d %d\n",
    //    canvas, x, x1+x->x_ldx, y1+x->x_ldy);

    /* Note-- since we're not using x1/y1 above in the new GUI call,
       Ivica's legacy logic isn't affecting us. Quick fix below by
       just adding the legacy offsets... */
    gui_vmess("gui_iemgui_label_coords", "xxii",
        canvas,
        x,
        x->x_ldx + (sys_legacy ? x->legacy_x : 0),
        x->x_ldy + (sys_legacy ? x->legacy_y : 0));
}

void iemgui_label_draw_config(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    if (x->x_selected == canvas && x->x_glist == canvas)
    {
        //post("iemgui_label_draw_config %s", x->x_lcol->s_name);
        gui_vmess("gui_iemgui_label_font", "xxssi",
            glist_getcanvas(x->x_glist),
            x,
            iemgui_typeface(x),
            sys_fontweight,
            x->x_fontsize);
        gui_vmess("gui_iemgui_label_set", "xxs",
            glist_getcanvas(x->x_glist),
            x,
            x->x_lab != s_empty ? x->x_lab->s_name: "");
        gui_vmess("gui_iemgui_label_color", "xxs",
            canvas,
            x,
            x->x_lcol->s_name);
    }
    else
    {
        gui_vmess("gui_iemgui_label_font", "xxssi",
            glist_getcanvas(x->x_glist),
            x,
            iemgui_typeface(x),
            sys_fontweight,
            x->x_fontsize);
        gui_vmess("gui_iemgui_label_set", "xxs",
            glist_getcanvas(x->x_glist),
            x,
            x->x_lab != s_empty ? x->x_lab->s_name: "");
        gui_vmess("gui_iemgui_label_color", "xxs",
            canvas,
            x,
            x->x_lcol->s_name);
    }
    if (x->x_selected == canvas && x->x_glist == canvas)
    {
        t_scalehandle *lh = (t_scalehandle *)(x->x_lhandle);
        if (x->x_lab == s_empty)
            scalehandle_draw_erase(x->x_lhandle);
        else if (lh->h_vis == 0)
            scalehandle_draw_select(lh,x->x_ldx,x->x_ldy);
    }
}

void iemgui_label_draw_select(t_iemgui *x)
{
    t_canvas *canvas = glist_getcanvas(x->x_glist);
    if (x->x_selected == canvas && x->x_glist == canvas)
    {
        gui_vmess("gui_iemgui_label_select", "xxi",
            canvas,
            x,
            1);
    }
    else
    {
        gui_vmess("gui_iemgui_label_select", "xxi",
            canvas,
            x,
            0);
    }
}

extern t_class *my_numbox_class;
extern t_class *vu_class;
extern t_class *my_canvas_class;
void iemgui_draw_io(t_iemgui *x, int old_sr_flags)
{
    char tagbuf[MAXPDSTRING];
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    if (x->x_glist != canvas) return; // is gop
    t_class *c = pd_class((t_pd *)x);
    if (c == my_numbox_class && ((t_my_numbox *)x)->x_drawstyle > 1)
        return; //sigh
    if (!(old_sr_flags&4) && !glist_isvisible(canvas))
    {
        return;
    }
    if (c==my_canvas_class) return;

    int x1,y1,x2,y2;
    c->c_wb->w_getrectfn((t_gobj *)x,canvas,&x1,&y1,&x2,&y2);
    //iemgui_getrect_draw(x, &x1, &y1, &x2, &y2);

    int i, n = c==vu_class ? 2 : 1, k=(x2-x1)-IOWIDTH;
    /* cnv has no inlets */
    if (c == my_canvas_class)
        n = 0;
    int a=old_sr_flags&IEM_GUI_OLD_SND_FLAG;
    int b=x->x_snd!=s_empty;
    /*
    post("%zx SND: old_sr_flags=%d SND_FLAG=%d || "
         "OUTCOME: OLD_SND_FLAG=%d not_empty=%d\n",
         (t_int)x, old_sr_flags, IEM_GUI_OLD_SND_FLAG, a, b);
    */
    
    if (a && !b)
    {
        for (i=0; i<n; i++)
        {
            sprintf(tagbuf, "%so%d", iem_get_tag(canvas, x), i);
            gui_vmess("gui_gobj_draw_io", "xxsiiiiiisiii", canvas,
                x, tagbuf,
                x1+i*k, y2-1, x1+i*k + IOWIDTH, y2, x1, y1, "o", i,
                0, 1);
        }
    }
    if (!a && b)
    {
        for (i=0; i<n; i++)
        {
            sprintf(tagbuf, "%so%d", iem_get_tag(canvas, x), i);
            gui_vmess("gui_gobj_erase_io", "xs",
                canvas, tagbuf);
        }
    }
    a = old_sr_flags & IEM_GUI_OLD_RCV_FLAG;
    b = x->x_rcv != s_empty;
    /*
    post("%zx RCV: old_sr_flags=%d RCV_FLAG=%d || "
         "OUTCOME: OLD_RCV_FLAG=%d not_empty=%d\n",
         (t_int)x, old_sr_flags, IEM_GUI_OLD_RCV_FLAG, a, b);
    */
    if (a && !b)
    {
        for (i=0; i<n; i++)
        {
            sprintf(tagbuf, "%si%d", iem_get_tag(canvas, x), i);
            gui_vmess("gui_gobj_draw_io", "xxsiiiiiisiii", canvas,
                x, tagbuf,
                x1+i*k, y1, x1+i*k + IOWIDTH, y1+1, x1, y1, "i", i,
                0, 1);
        }
    }
    if (!a && b)
    {
        for (i=0; i<n; i++)
        {
            sprintf(tagbuf, "%si%d", iem_get_tag(canvas, x), i);
            gui_vmess("gui_gobj_erase_io", "xs",
                canvas, tagbuf);
        }
    }
    x->x_draw(x, x->x_glist, IEM_GUI_DRAW_MODE_IO);
}

void iemgui_io_draw_move(t_iemgui *x)
{
    char tagbuf[MAXPDSTRING];
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    t_class *c = pd_class((t_pd *)x);
    int x1,y1,x2,y2;
    c->c_wb->w_getrectfn((t_gobj *)x,canvas,&x1,&y1,&x2,&y2);
    //iemgui_getrect_draw(x, &x1, &y1, &x2, &y2);

    int i, n = c==vu_class ? 2 : 1, k=(x2-x1)-IOWIDTH;
    /* cnv has no xlets */
    if (c == my_canvas_class)
        n = 0;
    if (!iemgui_has_snd(x) && canvas == x->x_glist)
    {
        for (i=0; i<n; i++)
        {
            sprintf(tagbuf, "%so%d", iem_get_tag(canvas, x), i);
            gui_start_vmess("gui_configure_item", "xs",
                canvas, tagbuf);
            gui_start_array();
            gui_s("x");
            gui_i(i*k);
            gui_s("y");
            gui_i(y2 - y1 - 1);
            gui_end_array();
            gui_end_vmess();
        }
    }
    if (!iemgui_has_rcv(x) && canvas == x->x_glist)
    {
        for (i=0; i<n; i++)
        {
            sprintf(tagbuf, "%si%d", iem_get_tag(canvas, x), i);
            gui_start_vmess("gui_configure_item", "xs",
                canvas, tagbuf);
            gui_start_array();
            gui_s("x");
            gui_i(i*k);
            gui_end_array();
            gui_end_vmess();
        }
    }
}

extern void gobj_vis_gethelpname(t_gobj *z, char *namebuf);

void iemgui_base_draw_new(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    t_class *c = pd_class((t_pd *)x);
    int x1, y1, x2, y2, gr = gop_redraw;
    gop_redraw = 0;
    c->c_wb->w_getrectfn((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    
    t_rtext *y = glist_findrtext(x->x_glist, x);
    char buf[FILENAME_MAX];
    rtext_getterminatedtext(y, &buf);

    //iemgui_getrect_draw(x, &x1, &y1, &x2, &y2); 
    gop_redraw = gr;

    char namebuf[FILENAME_MAX];
    gobj_vis_gethelpname((t_gobj *)x, &namebuf);

    gui_vmess("gui_gobj_new", "xxxxsiiiiss", canvas, x->x_glist,
        x->x_glist->gl_owner, x, "iemgui", x1, y1,
        glist_istoplevel(x->x_glist), 0, namebuf, buf);
    gui_vmess("gui_text_draw_border", "xxsiiii",
        canvas,
        x,
        x->x_bcol->s_name,
        0,
        x2 - x1,
        y2 - y1,
        glist_istoplevel(x->x_glist));
    gui_vmess("gui_iemgui_base_color", "xxs",
        canvas, x, x->x_bcol->s_name);
}

void iemgui_base_draw_move(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    t_class *c = pd_class((t_pd *)x);
    int x1,y1,x2,y2,gr=gop_redraw; gop_redraw=0;
    c->c_wb->w_getrectfn((t_gobj *)x,x->x_glist,&x1,&y1,&x2,&y2);
    //iemgui_getrect_draw(x, &x1, &y1, &x2, &y2);
    gop_redraw=gr;
    // ico@vt.edu 2023-02-20: due to new nested drawing we now
    // have to displace any requested position by the object's
    // original position, but only if we are drawing inside a GOP
    if (x->x_glist != glist_getcanvas(x->x_glist))
    {
        x1 -= glist_xtopixels(x->x_glist, x->x_glist->gl_x1);
        x2 -= glist_xtopixels(x->x_glist, x->x_glist->gl_x1);
        y1 -= glist_ytopixels(x->x_glist, x->x_glist->gl_y1);
        y2 -= glist_ytopixels(x->x_glist, x->x_glist->gl_y1);
    }
    gui_vmess("gui_iemgui_move_and_resize", "xxiiii",
        canvas, x, x1, y1, x2, y2);
}

void iemgui_base_draw_config(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    char tagbuf[MAXPDSTRING];
    sprintf(tagbuf, "x%zxborder", (t_int)x);
    gui_vmess("gui_iemgui_base_color", "xxs",
        canvas, x, x->x_bcol->s_name); 
}

void iemgui_draw_update(t_iemgui *x, t_glist *glist)
{
    x->x_draw(x, x->x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

void iemgui_draw_new(t_iemgui *x)
{
    x->x_draw(x, x->x_glist, IEM_GUI_DRAW_MODE_NEW);
    iemgui_label_draw_new(x);
    iemgui_draw_io(x,7);
    // used to be inside x_draw...
    canvas_raise_all_cords(glist_getcanvas(x->x_glist));
}

void iemgui_draw_config(t_iemgui *x)
{
    x->x_draw(x, x->x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    iemgui_label_draw_config(x);
    //iemgui_base_draw_config(x); // can't
}

void iemgui_draw_move(t_iemgui *x)
{
    x->x_draw(x, x->x_glist, IEM_GUI_DRAW_MODE_MOVE);
    iemgui_label_draw_move(x);
    //iemgui_base_draw_move(x); // can't
    iemgui_io_draw_move(x);
}

void iemgui_draw_erase(t_iemgui *x)
{
    t_canvas *canvas=glist_getcanvas(x->x_glist);
    gui_vmess("gui_gobj_erase", "xx", canvas, x);
    scalehandle_draw_erase2(x);
}

void scrollbar_update(t_glist *glist)
{
    //ico@bukvic.net 100518 update scrollbars when object potentially
    //exceeds window size
    t_canvas *canvas=(t_canvas *)glist_getcanvas(glist);
    canvas_getscroll(canvas);
}

/* ico@vt.edu 20200920: introduced for situation where getscroll
needs to occur before the next command, e.g. automate. */
void scrollbar_synchronous_update(t_glist *glist)
{
    // glist_getcanvas is probably not needed but not before we make
    // sure that there are unneded calls of this kind being made by
    // non-toplevel objects...
    //post("scrollbar_scynchronous_update");
    gui_vmess("gui_canvas_get_immediate_scroll",
        "x", glist_getcanvas(glist));
}

void wb_init(t_widgetbehavior *wb, t_getrectfn gr, t_clickfn cl)
{
    wb->w_getrectfn = gr;
    wb->w_displacefn = iemgui_displace;
    wb->w_selectfn = iemgui_select;
    wb->w_activatefn = NULL;
    wb->w_deletefn = iemgui_delete;
    wb->w_visfn = iemgui_vis;
    wb->w_clickfn = cl;
    wb->w_displacefnwtag = iemgui_displace_withtag;
}

const char *iemgui_typeface(t_iemgui *x)
{
    int f = x->x_font_style;
    if(f == 0) return sys_font;
    if(f == 1) return "helvetica";
    if(f == 2) return "times";
    return "invalid-font";
}

// this uses a static buffer, so don't use it twice in the same sys_vgui.
// the static buffer could be replaced by a malloc when sys_vgui is replaced
// by something that frees that memory.
/* this is probably obsolete now-- we want to just send each one as a
   separate arg so we don't have to parse on the gui side.
   Once we check to make sure all iemguis work without it we can safely
   remove it */

const char *iemgui_font(t_iemgui *x)
{
    static char buf[64];
    sprintf(buf, "{{%s} -%d %s}", iemgui_typeface(x), x->x_fontsize, sys_fontweight);
    return buf;
}

void iemgui_init(t_iemgui *x, t_floatarg f)
{
    x->x_loadinit = f!=0.0;
    iemgui_update_properties(x, IEM_GUI_PROP_INIT);
}

extern void get_runtime_tooltip_text(t_text *x, int ac, t_atom *av);

void iemgui_runtime_tooltip(t_iemgui *x, t_symbol *s, int ac, t_atom *av)
{
    get_runtime_tooltip_text(&x->x_obj, ac, av);
        //post("iemgui_runtime_tooltip x=%lx class=<%s> tooltip=<%s>",
    //    x, class_getname(pd_class(&x->x_obj.te_pd)), x->x_rttp);
    gui_vmess("gobj_set_runtime_tooltip", "xxs",
        glist_getcanvas(x->x_glist),
        x,
        x->x_obj.te_rttp
    );
}

void iemgui_class_addmethods(t_class *c)
{
    class_addmethod(c, (t_method)iemgui_delta,
        gensym("delta"), A_GIMME, 0);
    class_addmethod(c, (t_method)iemgui_pos,
        gensym("pos"), A_GIMME, 0);
    class_addmethod(c, (t_method)iemgui_color,
        gensym("color"), A_GIMME, 0);
    class_addmethod(c, (t_method)iemgui_send,
        gensym("send"), A_DEFSYM, 0);
    class_addmethod(c, (t_method)iemgui_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(c, (t_method)iemgui_label,
        gensym("label"), A_DEFSYM, 0);
    class_addmethod(c, (t_method)iemgui_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(c, (t_method)iemgui_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(c, (t_method)iemgui_css,
        gensym("css"), A_GIMME, 0);
    class_addmethod(c, (t_method)iemgui_runtime_tooltip,
        gensym("tooltip"), A_GIMME, 0);
}

void g_iemgui_setup (void)
{
    s_empty = gensym("empty");
    scalehandle_class = class_new(gensym("_scalehandle"), 0, 0,
                  sizeof(t_scalehandle), CLASS_PD, 0);
    class_addmethod(scalehandle_class, (t_method)scalehandle_clickhook,
            gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scalehandle_class, (t_method)scalehandle_motionhook,
            gensym("_motion"), A_FLOAT, A_FLOAT, 0);
}

const char *selection_color = "$pd_colors(selection)";
const char *border_color = "$pd_colors(iemgui_border)";

#define GET_OUTLET t_outlet *out = x->x_obj.ob_outlet; /* can't use int o because there's not obj_nth_outlet function */
#define SEND_BY_SYMBOL (iemgui_has_snd(x) && x->x_snd->s_thing && (!chk_putin || x->x_put_in2out))

void iemgui_out_bang(t_iemgui *x, int o, int chk_putin)
{
    GET_OUTLET outlet_bang(out);
    if(SEND_BY_SYMBOL) pd_bang(x->x_snd->s_thing);
}

void iemgui_out_float(t_iemgui *x, int o, int chk_putin, t_float f)
{
    GET_OUTLET outlet_float(out,f);
    if(SEND_BY_SYMBOL) pd_float(x->x_snd->s_thing,f);
}

void iemgui_out_list(t_iemgui *x, int o, int chk_putin, t_symbol *s,
    int argc, t_atom *argv)
{
    GET_OUTLET outlet_list(out, s, argc, argv);
    if (SEND_BY_SYMBOL) pd_list(x->x_snd->s_thing, s, argc, argv);
}
