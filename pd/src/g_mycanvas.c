/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "g_all_guis.h"
#include <math.h>

extern t_int gfxstub_haveproperties(void *key);
void my_canvas_draw_select(t_my_canvas* x, t_glist* glist);

extern void gobj_vis_gethelpname(t_gobj *z, char *namebuf);

t_widgetbehavior my_canvas_widgetbehavior;
/*static*/ t_class *my_canvas_class;

void my_canvas_draw_new(t_my_canvas *x, t_glist *glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int x1 = text_xpix(&x->x_gui.x_obj, glist);
    int y1 = text_ypix(&x->x_gui.x_obj, glist);

    t_rtext *y = glist_findrtext(glist, x);
    char buf[FILENAME_MAX];
    rtext_getterminatedtext(y, &buf);

    char namebuf[FILENAME_MAX];
    gobj_vis_gethelpname((t_gobj *)x, &namebuf);

    gui_vmess("gui_gobj_new", "xxxxsiiiiss", canvas,
        x->x_gui.x_glist, x->x_gui.x_glist->gl_owner,
        x, "iemgui", x1, y1, glist_istoplevel(glist), 0,
        namebuf, buf);
    gui_vmess("gui_mycanvas_new", "xxxxsiiiiiii", canvas,
        x->x_gui.x_glist, x->x_gui.x_glist->gl_owner,
        x, x->x_gui.x_bcol->s_name, x1, y1, x1+x->x_vis_w, y1+x->x_vis_h,
        x1+x->x_gui.x_w, y1+x->x_gui.x_h, glist_istoplevel(glist));
}

void my_canvas_draw_move(t_my_canvas *x, t_glist *glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    if (!glist_isvisible(canvas)) return;
    iemgui_base_draw_move(&x->x_gui);
    gui_vmess("gui_mycanvas_coords", "xxiiii",
        canvas, x,
        x->x_vis_w, x->x_vis_h, x->x_gui.x_w, x->x_gui.x_h);
}

void my_canvas_draw_config(t_my_canvas* x, t_glist* glist)
{
    t_canvas *canvas=glist_getcanvas(glist);
    int isselected;
    isselected = x->x_gui.x_selected == canvas && x->x_gui.x_glist == canvas;
    gui_vmess("gui_mycanvas_update", "xxsi",
        canvas, x, x->x_gui.x_bcol->s_name, isselected);
}

void my_canvas_draw_select(t_my_canvas* x, t_glist* glist)
{
    /* No longer needed */
}

static void my_canvas__clickhook(t_scalehandle *sh, int newstate)
{
    t_my_canvas *x = (t_my_canvas *)(sh->h_master);
    if (newstate)
    {
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
        if (!sh->h_scale)
            scalehandle_click_label(sh);
    }
    if (sh->h_scale)
    {
        sh->h_adjust_x = sh->h_offset_x -
            (((t_object *)x)->te_xpix + x->x_vis_w);
        sh->h_adjust_y = sh->h_offset_y -
            (((t_object *)x)->te_ypix + x->x_vis_h);
        /* Hack to set the cursor since we're doing and end-run
           around canas_doclick here */
    }
    sh->h_dragon = newstate;
}

static void my_canvas__motionhook(t_scalehandle *sh, t_floatarg mouse_x, t_floatarg mouse_y)
{
    if (sh->h_scale)
    {
        t_my_canvas *x = (t_my_canvas *)(sh->h_master);

        int width = (sh->h_constrain == CURSOR_EDITMODE_RESIZE_Y) ?
            x->x_vis_w :
            (int)mouse_x - text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist) -
                sh->h_adjust_x;
        int height = (sh->h_constrain == CURSOR_EDITMODE_RESIZE_X) ?
            x->x_vis_h :
            (int)mouse_y - text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist) -
                sh->h_adjust_y;
        x->x_vis_w = maxi(width, IEM_GUI_MYCANVAS_MINSIZE);
        x->x_vis_h = maxi(height, IEM_GUI_MYCANVAS_MINSIZE);

        scalehandle_drag_scale(sh);

        if (glist_isvisible(x->x_gui.x_glist))
        {
            my_canvas_draw_move(x, x->x_gui.x_glist);
            scalehandle_unclick_scale(sh);
        }

        t_int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            properties_set_field_int(properties,"rng.min_ent",width);
            properties_set_field_int(properties,"rng.max_ent",height);
        }
    }
    scalehandle_dragon_label(sh, mouse_x, mouse_y);
}

void my_canvas_draw(t_my_canvas *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_MOVE)        my_canvas_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)    my_canvas_draw_new(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_SELECT) my_canvas_draw_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG) my_canvas_draw_config(x, glist);
}

/* ------------------------ cnv widgetbehaviour----------------------------- */

static void my_canvas_getrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_my_canvas *x = (t_my_canvas *)z;
    
    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    if (!glist_istoplevel(glist) || !glist->gl_edit)
    {
        //if we are trying to calculate visibility of a widget inside a GOP
        //or are calculating getrect during runtime
        *xp2 = *xp1 + x->x_vis_w;
        *yp2 = *yp1 + x->x_vis_h;
    }
    else
    {
        *xp2 = *xp1 + x->x_gui.x_w;
        *yp2 = *yp1 + x->x_gui.x_h;
    }
    iemgui_label_getrect(x->x_gui, glist, xp1, yp1, xp2, yp2);
}

static void my_canvas_save(t_gobj *z, t_binbuf *b)
{
    t_my_canvas *x = (t_my_canvas *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];
    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiisssiiiissi;", gensym("#X"),gensym("obj"),
        (int)x->x_gui.x_obj.te_xpix, (int)x->x_gui.x_obj.te_ypix,
        gensym("cnv"), x->x_gui.x_w, x->x_vis_w, x->x_vis_h,
        srl[0], srl[1], srl[2], x->x_gui.x_ldx, x->x_gui.x_ldy,
        iem_fstyletoint(&x->x_gui), x->x_gui.x_fontsize,
        bflcol[0], bflcol[2], iem_symargstoint(&x->x_gui));
}

static void my_canvas_properties(t_gobj *z, t_glist *owner)
{
    t_my_canvas *x = (t_my_canvas *)z;
    char buf[800], *gfx_tag;
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);
/*
    sprintf(buf, "pdtk_iemgui_dialog %%s |cnv| \
            ------selectable_dimensions(pix):------ %d %d size: 0.0 0.0 empty \
            ------visible_rectangle(pix)(pix):------ %d width: %d height: %d \
            %d empty empty %d %d empty %d \
            {%s} {%s} \
            {%s} %d %d \
            %d %d \
            %d %d %d\n",
            x->x_gui.x_w, 1,
*/
            //x->x_vis_w, x->x_vis_h, 0,/*no_schedule*/
            //-1, -1, -1, -1,/*no linlog, no init, no multi*/
            //srl[0]->s_name, srl[1]->s_name,
            //srl[2]->s_name, x->x_gui.x_ldx, x->x_gui.x_ldy,
            //x->x_gui.x_font_style, x->x_gui.x_fontsize,
            //x->x_gui.x_bcol, -1/*no frontcolor*/, x->x_gui.x_lcol);
    //gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);

    gfx_tag = gfxstub_new2(&x->x_gui.x_obj.ob_pd, &x->x_gui);
    /* todo: send along the x/y of the object here so we can
       create the window in the right place */

    gui_start_vmess("gui_iemgui_dialog", "s", gfx_tag);
    gui_start_array();

    gui_s("type");           gui_s("cnv");
    gui_s("selection_size"); gui_i(x->x_gui.x_w);
    gui_s("visible_width");  gui_i(x->x_vis_w);
    gui_s("visible_height"); gui_i(x->x_vis_h);
    gui_s("minimum_size");   gui_i(IEM_GUI_MYCANVAS_MINSIZE);

    gui_s("range_schedule"); // no idea what this is...
    gui_i(0);

    gui_s("send_symbol"); gui_s(srl[0]->s_name);
    gui_s("receive_symbol"); gui_s(srl[1]->s_name);
    gui_s("label"); gui_s(srl[2]->s_name);
    gui_s("x_offset"); gui_i(x->x_gui.x_ldx);
    gui_s("y_offset");  gui_i(x->x_gui.x_ldy);
    gui_s("font_style"); gui_i(x->x_gui.x_font_style);
    gui_s("font_size"); gui_i(x->x_gui.x_fontsize);
    gui_s("background_color"); gui_s(x->x_gui.x_bcol->s_name);
    //gui_s("foreground_color"); gui_s(x->x_gui.x_fcol->s_name);
    gui_s("label_color"); gui_s(x->x_gui.x_lcol->s_name);
    
    gui_end_array();
    gui_end_vmess();
}

static void my_canvas_get_pos(t_my_canvas *x)
{
    if(iemgui_has_snd(&x->x_gui) && x->x_gui.x_snd->s_thing)
    {
        x->x_at[0].a_w.w_float = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
        x->x_at[1].a_w.w_float = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
        pd_list(x->x_gui.x_snd->s_thing, &s_list, 2, x->x_at);
    }
}

static void my_canvas_dialog(t_my_canvas *x, t_symbol *s, int argc, t_atom *argv)
{
    //printf("cnv_dialog: selected=%d\n",x->x_gui.x_selected);
    if (atom_getintarg(21, argc, argv))
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
    //printf("cnv_dialog: selected=%d\n",x->x_gui.x_selected);
    x->x_gui.x_h =
    x->x_gui.x_w = maxi(atom_getintarg(0, argc, argv),1);
    x->x_vis_w = maxi(atom_getintarg(2, argc, argv),1);
    x->x_vis_h = maxi(atom_getintarg(3, argc, argv),1);
    iemgui_dialog(&x->x_gui, argc, argv);
    x->x_gui.x_loadinit = 0;
    iemgui_draw_config(&x->x_gui);
    iemgui_shouldvis(&x->x_gui, IEM_GUI_DRAW_MODE_MOVE);
    scalehandle_draw(&x->x_gui);
    scrollbar_update(x->x_gui.x_glist);
}

static void my_canvas_size(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int i = (int)atom_getintarg(0, ac, av);

    if(i < 1)
        i = 1;
    x->x_gui.x_w = i;
    x->x_gui.x_h = i;
    iemgui_size(&x->x_gui);
}

static void my_canvas_vis_size(t_my_canvas *x, t_symbol *s, int ac, t_atom *av)
{
    int i;

    i = (int)atom_getintarg(0, ac, av);
    if(i < 1)
        i = 1;
    x->x_vis_w = i;
    if(ac > 1)
    {
        i = (int)atom_getintarg(1, ac, av);
        if(i < 1)
            i = 1;
    }
    x->x_vis_h = i;
    if(glist_isvisible(x->x_gui.x_glist)) iemgui_draw_move(&x->x_gui);
}

static void *my_canvas_new(t_symbol *s, int argc, t_atom *argv)
{
    t_my_canvas *x = (t_my_canvas *)pd_new(my_canvas_class);
    int a=IEM_GUI_DEFAULTSIZE, w=100, h=60;
    int ldx=20, ldy=12, i=0;
    int fs=14;

    iem_inttosymargs(&x->x_gui, 0);
    iem_inttofstyle(&x->x_gui, 0);

    x->x_gui.x_bcol = gensym("#E0E0E0FF");
    x->x_gui.x_fcol = gensym("#000000FF");
    x->x_gui.x_lcol = gensym("#404040FF");

    if(((argc >= 10)&&(argc <= 13))
       &&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)&&IS_A_FLOAT(argv,2))
    {
        a = maxi(atom_getintarg(0, argc, argv),1);
        w = maxi(atom_getintarg(1, argc, argv),1);
        h = maxi(atom_getintarg(2, argc, argv),1);
    }
    if((argc >= 12) && (IS_A_SYMBOL(argv,3) || IS_A_FLOAT(argv,3)) &&
        (IS_A_SYMBOL(argv,4) || IS_A_FLOAT(argv,4)))
    {
        i = 2;
        iemgui_new_getnames(&x->x_gui, 3, argv);
    }
    else if((argc == 11)&&(IS_A_SYMBOL(argv,3)||IS_A_FLOAT(argv,3)))
    {
        i = 1;
        iemgui_new_getnames(&x->x_gui, 3, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 3, 0);

    if(((argc >= 10)&&(argc <= 13))
       &&(IS_A_SYMBOL(argv,i+3)||IS_A_FLOAT(argv,i+3))&&IS_A_FLOAT(argv,i+4)
       &&IS_A_FLOAT(argv,i+5)&&IS_A_FLOAT(argv,i+6)
       &&IS_A_FLOAT(argv,i+7))
    {
            /* disastrously, the "label" sits in a different part of the
            message.  So we have to track its location separately (in
            the slot x_labelbindex) and initialize it specially here. */
        iemgui_getfloatsym(argv+i+3);
        x->x_gui.x_labelbindex = i+4;
        ldx = atom_getintarg(i+4, argc, argv);
        ldy = atom_getintarg(i+5, argc, argv);
        iem_inttofstyle(&x->x_gui, atom_getintarg(i+6, argc, argv));
        fs = atom_getintarg(i+7, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv+i+8, 0, argv+i+9);
    }
    if((argc == 13)&&IS_A_FLOAT(argv,i+10))
    {
        iem_inttosymargs(&x->x_gui, atom_getintarg(i+10, argc, argv));
    }
    x->x_gui.x_draw = (t_iemfunptr)my_canvas_draw;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    x->x_gui.x_h = x->x_gui.x_w = a;
    x->x_vis_w = w;
    x->x_vis_h = h;
    if (x->x_gui.x_font_style<0 || x->x_gui.x_font_style>2) x->x_gui.x_font_style=0;
    if (iemgui_has_rcv(&x->x_gui))
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    if(fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    x->x_at[0].a_type = A_FLOAT;
    x->x_at[1].a_type = A_FLOAT;
    iemgui_verify_snd_ne_rcv(&x->x_gui);

    x->x_gui.x_handle = scalehandle_new((t_object *)x,x->x_gui.x_glist,1,my_canvas__clickhook,my_canvas__motionhook);
    x->x_gui.x_lhandle = scalehandle_new((t_object *)x,x->x_gui.x_glist,0,my_canvas__clickhook,my_canvas__motionhook);
    x->x_gui.x_obj.te_iemgui = 1;

    x->x_gui.legacy_x = 0;
    x->x_gui.legacy_y = 1;

    x->x_gui.x_color2 = 3;
    x->x_gui.x_color3 = 0;

    return (x);
}

static void my_canvas_ff(t_my_canvas *x)
{
    if(iemgui_has_rcv(&x->x_gui))
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(&x->x_gui);

    if (x->x_gui. x_handle) scalehandle_free(x->x_gui. x_handle);
    if (x->x_gui.x_lhandle) scalehandle_free(x->x_gui.x_lhandle);
}

void g_mycanvas_setup(void)
{
    my_canvas_class = class_new(gensym("cnv"), (t_newmethod)my_canvas_new,
        (t_method)my_canvas_ff, sizeof(t_my_canvas), CLASS_NOINLET, A_GIMME, 0);
    class_addcreator((t_newmethod)my_canvas_new,
        gensym("my_canvas"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_size,
        gensym("size"), A_GIMME, 0);
    iemgui_class_addmethods(my_canvas_class);
    class_addmethod(my_canvas_class, (t_method)my_canvas_vis_size,
        gensym("vis_size"), A_GIMME, 0);
    class_addmethod(my_canvas_class, (t_method)my_canvas_get_pos,
        gensym("get_pos"), 0);

    wb_init(&my_canvas_widgetbehavior,my_canvas_getrect,0);
    class_setwidget(my_canvas_class, &my_canvas_widgetbehavior);
    class_sethelpsymbol(my_canvas_class, gensym("my_canvas"));
    class_setsavefn(my_canvas_class, my_canvas_save);
    class_setpropertiesfn(my_canvas_class, my_canvas_properties);
}
