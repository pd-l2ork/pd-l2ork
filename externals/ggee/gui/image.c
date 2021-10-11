/*
    image object by Ivica Ico Bukvic <ico@vt.edu> 2020-12-03
    this object is backwards compatible with the ggee/image
*/

#include <string.h>
#include <m_pd.h>
#include "g_canvas.h"
#include "s_stuff.h"
#include <stdio.h>
#include <g_all_guis.h>

#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#ifdef _WIN32
#include <stdlib.h>
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

/* ------------------------ image ----------------------------- */

t_class *image_class;

typedef struct _image
{
    t_iemgui x_gui;
    int x_adj_img_width;    // user customized width/height
    int x_adj_img_height;
    int x_img_width;        // original image width/height
    int x_img_height;
    int x_constrain_w;      // used for custom constrain
    int x_constrain_h;
    int x_gop_spill;        // toggle clickable size to be smaller than the image
    int x_click;            // toggle no click recognition (0), bang (1),
                            // toggle (2), or passthrough toggle (3)
    t_symbol* x_fname;      // file name
    t_symbol* x_inlet;
    int x_legacy;           // flag for when detecting a legacy object
    int x_img_loaded;       // check if an image has been loaded
    int x_constrain;        /* constrain aspect ratio:
                               0 = free resize
                               1 = fixed aspect ratio of the original image
                               2 = custom aspect ratio specified by user
                            */
    int x_mouse_x;          // used to track mouse position
    int x_mouse_y;
    t_float x_rot_x;        // rotation point location x and y
    t_float x_rot_y;
    int x_rot_pt_init;      // whether the rotation point has been initialized
                            // we use this since rotation point could be anything
    t_float x_rot_angle;    // rotation angle
    int x_draw_firstime;    // removed the firstime flag from drawme, so that it is
                            // compatible with iemgui DRAW_CONFIG call
    int x_visible;          // whether image should be visible
    t_float x_alpha;        // image alpha
    int x_mode3_click;      // keep track of the click for the mode 3 since
                            // mouse up is faked by sending dbl = -1 variable
} t_image;

// x_gui.x_w and x_h are used for the getbox size. This could be either
// both the image and selection box size (when gop_spill is off), or only
// the selection box size (when gop_spill is on)

// x_img_width and x_img_height are the image's original size

// x_adj_img_width and height are adjusted image size (based on user input)

extern t_symbol *s_image_empty;
extern t_int gfxstub_haveproperties(void *key);

/* widget helper functions */
static void image_select(t_gobj *z, t_glist *glist, int state);
static void image_vis(t_gobj *z, t_glist *glist, int vis);
static void image_update(t_image *x, t_glist *glist, int width, int height, int resizemode);
static void image_dorotate(t_image *x);
static void image_visible(t_image *x, t_floatarg f);
static void image_alpha(t_image *x, t_floatarg f);
/* from g_editor.c--we use this to detect if the user has clicked with the
   right button, so that even in runtime they can select ggee/image help
*/
extern int glob_lmclick;

t_symbol *image_trytoopen(t_image* x)
{
    char fname[FILENAME_MAX];
    FILE *file;
    if (x->x_fname == &s_)
    {
        x->x_fname = gensym("@pd_extra/ggee/empty_image.png");
        //return 0;
    }
    canvas_makefilename(
        glist_getcanvas(x->x_gui.x_glist),
        x->x_fname->s_name,
        fname, FILENAME_MAX
        );
    // try to open the file
    if ((file = sys_fopen(fname, "r")))
    {
        sys_fclose(file);
        return gensym(fname);
    }
    else
    {
        return 0;
    }
}

static void image_drawme(t_image *x, t_glist *glist)
{
    if(gobj_shouldvis((t_gobj *)x, glist))
    {
        if (x->x_draw_firstime)
        {
            t_symbol *fname = image_trytoopen(x);
            gui_vmess("gui_gobj_new", "xxsiii",
                glist_getcanvas(glist),
                x,
                "obj",
                text_xpix(&x->x_gui.x_obj, glist),
                text_ypix(&x->x_gui.x_obj, glist),
                glist_istoplevel(glist));
            if (fname) {
                // if we are using gop_spill, reloading may cause flicker
                // because the image may need to be repositioned after
                // being loaded, so we toggle its visibility until the image
                // is loaded and its size is known to pd via the callback
                // the gui_ggee_image_display in pdgui.js checks if there is
                // a valid object before trying to change ints configuration,
                // so we don't have to do that here.
                if (x->x_gop_spill)
                    gui_vmess("gui_ggee_image_display", "xxi",
                        glist_getcanvas(glist), x, 0);
                // ico@vt.edu 2021-10-08: here we need to convert the path
                // to realpath because nw.js has an inconsistent way of
                // handling paths. e.g., when running pd-l2ork on Linux from
                // a command line, first time you open the patch it works ok.
                // however, if you open it the second time from the recent
                // files menu, it fails to locate files with a relative path.
                char realfname[FILENAME_MAX];
                realpath(fname->s_name, realfname);
                gui_vmess("gui_load_image", "xxs",
                    glist_getcanvas(glist), x, realfname);
            }
            else
            {
                gui_vmess("gui_load_default_image", "xx",
                    glist_getcanvas(glist), x);
            }
            // draw the new canvas image
            gui_vmess("gui_gobj_draw_image", "xxxsiiiii",
                glist_getcanvas(glist),
                x,
                x,
                "nw",
                x->x_gui.x_w,
                x->x_gui.x_h,
                x->x_constrain,
                0, // this denotes ggee/image object type
                   // 0 = ggee/image
                   // 1 = moonlib/image
                x->x_gop_spill // for ggee/image this lets the nw.js know
                               // if we have gop_spill enabled, so that we
                               // can hide the visibility until image is
                               // loaded and properly displaced
            );
            // draw border (it will be invisible in runmode via css adjustments)
            if (x->x_gui.x_glist == glist_getcanvas(x->x_gui.x_glist))
                gui_vmess("gui_image_draw_border", "xxiiiii", glist, x,
                    x->x_gui.x_obj.te_xpix, x->x_gui.x_obj.te_ypix,
                    x->x_gui.x_w, x->x_gui.x_h, 1);
            gui_vmess("gui_image_size_callback", "xxs",
                glist_getcanvas(glist), x, x->x_inlet->s_name);
            iemgui_label_draw_new(&x->x_gui);
            x->x_draw_firstime = 0;

            iemgui_draw_io(&x->x_gui, 7);
            // here we manually set this to avoid g_all_guis.c to redundantly
            // redraw label and other issues
            image_visible(x, x->x_visible);
            image_alpha(x, x->x_alpha);
            x->x_gui.x_vis = 1;
        }
        else
        {
            image_visible(x, x->x_visible);
            image_alpha(x, x->x_alpha);
            // move the gobj
            gui_vmess("gui_image_coords", "xxii",
                glist_getcanvas(glist),
                x,
                text_xpix(&x->x_gui.x_obj, glist),
                text_ypix(&x->x_gui.x_obj, glist)
            );
            if (x->x_img_loaded)
            {
                image_update(x, glist_getcanvas(glist),
                    x->x_adj_img_width, x->x_adj_img_height, 0);
                gui_vmess("gui_ggee_image_offset", "xxxii",
                    glist_getcanvas(glist),
                    x,
                    x,
                    (x->x_gop_spill ? -(x->x_adj_img_width/2 - x->x_gui.x_w/2) : 0),
                    (x->x_gop_spill ? -(x->x_adj_img_height/2 - x->x_gui.x_h/2) : 0)
                );
            }
            if (x->x_gop_spill)
                gui_vmess("gui_ggee_image_display", "xxi",
                    glist_getcanvas(glist), x, 1);

            if (glist_isselected(x->x_gui.x_glist, (t_gobj *)x))
            {
                image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 0);
                image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 1);
            }
            iemgui_label_draw_move(&x->x_gui);
            iemgui_label_draw_config(&x->x_gui);
            canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
        }
    }
}

static void image_erase(t_image* x, t_glist* glist)
{
    gui_vmess("gui_gobj_erase", "xx", glist_getcanvas(glist), x);
    x->x_gui.x_vis = 0;
}

static t_symbol *get_filename(t_int argc, t_atom *argv)
{
    t_symbol *fname;
    fname = atom_getsymbolarg(0, argc, argv);
    if (argc > 1)
    {
        int i;
        char buf[MAXPDSTRING];
        strcpy(buf, fname->s_name);
        for (i = 1; i < argc; i++)
        {
            if (argv[i].a_type == A_SYMBOL)
            {
                strcat(buf, " ");
                strcat(buf, atom_getsymbolarg(i, argc, argv)->s_name);
            }
            else
            {
                break;
            }
        }
        fname = gensym(buf);
    }
    return fname;
}

/* ------------------------ image widgetbehaviour----------------------------- */

static void image_getrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int width, height;
    t_image* x = (t_image*)z;

    if (!x->x_gop_spill && (x->x_adj_img_width + x->x_adj_img_height) >= 2) {
        width = x->x_adj_img_width;
        height = x->x_adj_img_height;
        //post("image_getrect A");
    }
    else
    {
        width = x->x_gui.x_w;
        height = x->x_gui.x_h;
        //post("image_getrect B");
    }
    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = text_xpix(&x->x_gui.x_obj, glist) + width;
    *yp2 = text_ypix(&x->x_gui.x_obj, glist) + height;

    // if we have click detection disabled and we are in runmode,
    // return 0 size to allow for click relegation to hidden objects below
    // CAREFUL: this code is not reusable for objects that have more than
    // one inlet or outlet because it will cram them together
    if (glob_lmclick && 
        ((glist_getcanvas(glist) != glist && !x->x_click) ||
            (!glist->gl_edit && !x->x_click)))
    {
        *xp2 = *xp1;
    }
    //post("...%d %d %d %d", *xp1, *yp1, *xp2, *yp2);
}

static void image_displace(t_gobj *z, t_glist *glist,
    int dx, int dy)
{
    //post("image_displace");
    t_image *x = (t_image *)z;
    x->x_gui.x_obj.te_xpix += dx;
    x->x_gui.x_obj.te_ypix += dy;
    x->x_draw_firstime = 0;
    gui_vmess("gui_image_coords", "xxii",
        glist_getcanvas(glist),
        x,
        text_xpix(&x->x_gui.x_obj, glist),
        text_ypix(&x->x_gui.x_obj, glist)
    );
    canvas_fixlinesfor(glist,(t_text*) x);
}

static void image_displace_wtag(t_gobj *z, t_glist *glist,
    int dx, int dy)
{
    //post("image_displace_wtag");
    t_image *x = (t_image *)z;
    x->x_gui.x_obj.te_xpix += dx;
    x->x_gui.x_obj.te_ypix += dy;
    //image_drawme(x, glist, 0);
    canvas_fixlinesfor(glist,(t_text*) x);
}

static void image_draw_move(t_image *x, t_glist *glist)
{
    image_displace((t_gobj *)x, glist, 0, 0);
}

static void image_select(t_gobj *z, t_glist *glist, int state)
{
    //post("image_select %d", state);
    t_image *x = (t_image *)z;

    int height, width, x1, x2, y1, y2;

    // Borrowing getrect code here since getrect has also an exception
    // where non-edit mode stuff does not yield proper info (x1 is made
    // equal to x2 and therefore object's selection box rendered invisible)
    if (!x->x_gop_spill && (x->x_adj_img_width + x->x_adj_img_height) >= 2) {
        width = x->x_adj_img_width;
        height = x->x_adj_img_height;
        //post("image_getrect A");
    }
    else
    {
        width = x->x_gui.x_w;
        height = x->x_gui.x_h;
        //post("image_getrect B");
    }
    x1 = text_xpix(&x->x_gui.x_obj, glist);
    y1 = text_ypix(&x->x_gui.x_obj, glist);
    x2 = text_xpix(&x->x_gui.x_obj, glist) + width;
    y2 = text_ypix(&x->x_gui.x_obj, glist) + height;

    if (state)
    {
        // we need to redundantly call gui_gobj_select due to weird
        // order of redraw/reselect that causes gop object with visible image
        // upon cut/undo-cut to not have the image selected
        gui_vmess("gui_gobj_select", "xx", glist_getcanvas(x->x_gui.x_glist), x);
        // here we borrow the iemgui mycanvas resize handles
        // and add 8 to the width since the function below is originally
        // aimed at mycanvas and its offset. Do this only if we are on
        // our own parent canvas...
        if (x->x_gui.x_selected == NULL)
        {
            if (glist == x->x_gui.x_glist)
            {
                gui_vmess("gui_iemgui_label_show_drag_handle", "xxiiii",
                    glist, x, state, 
                    (x2 - x1) + (x->x_adj_img_width/2 - (x2 - x1)/2) - 
                        (x->x_adj_img_width == x->x_gui.x_w ? 0 : 2) + 8,
                    (y2 - y1) + (x->x_adj_img_height/2 - (y2 - y1)/2) -
                        (x->x_adj_img_height == x->x_gui.x_h ? 0 : 2),
                    1);
                if (x->x_gui.x_lab != s_empty)
                {
                    scalehandle_draw_select(
                        x->x_gui.x_lhandle, x->x_gui.x_ldx + 5, x->x_gui.x_ldy + 10);
                }
            }
            x->x_gui.x_selected = glist;
        }
    }
    else if (!state && x->x_gui.x_selected == glist)
    {
        gui_vmess("gui_iemgui_label_show_drag_handle", "xxiiii",
            glist, x, state, 0, 0, 1);
        gui_vmess("gui_gobj_deselect", "xx", glist_getcanvas(x->x_gui.x_glist), x);
        x->x_gui.x_selected = NULL;
        scalehandle_draw_erase(x->x_gui.x_lhandle);
    }
    iemgui_label_draw_select(&x->x_gui);
}

static void image_activate(t_gobj *z, t_glist *glist, int state)
{
    /*
    t_text *x = (t_text *)z;
    t_rtext *y = glist_findrtext(glist, x);
    rtext_activate(y, state);
    t_image *i = (t_image *)z;
    */
}

static void image_delete(t_gobj *z, t_glist *glist)
{
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}

static void image_vis(t_gobj *z, t_glist *glist, int vis)
{
    //post("image_vis");
    t_image* x = (t_image*)z;
    if (vis)
    {
        x->x_draw_firstime = 1;
        image_drawme(x, glist);
    }
    else
        image_erase(x, glist);
}

static void image_save(t_gobj *z, t_binbuf *b)
{
    //post("image_save");
    t_image *x = (t_image *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];
    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiissiiiisssiiiiisiifffif", gensym("#X"), gensym("obj"),
                x->x_gui.x_obj.te_xpix, x->x_gui.x_obj.te_ypix,   
                atom_getsymbol(binbuf_getvec(x->x_gui.x_obj.te_binbuf)),
                x->x_fname, x->x_gop_spill, x->x_click, x->x_gui.x_w,
                x->x_gui.x_h, srl[0], srl[1], srl[2], x->x_adj_img_width,
                x->x_adj_img_height, x->x_constrain,
                iem_fstyletoint(&x->x_gui), x->x_gui.x_fontsize,
                bflcol[2], x->x_gui.x_ldx, x->x_gui.x_ldy,
                x->x_rot_x, x->x_rot_y, x->x_rot_angle, x->x_visible, x->x_alpha);
    binbuf_addv(b, ";");
}

static t_widgetbehavior image_widgetbehavior;

static void image_motion(t_image *x, t_floatarg dx, t_floatarg dy)
{
    if (x->x_click == 2)
    {
        //post("image_motion stored: %d %d | delta: %d %d", x->x_mouse_x, x->x_mouse_y, dx, dy);
        int x1, x2, y1, y2;
        image_getrect((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), &x1, &y1, &x2, &y2);
        t_atom at[5];
        x->x_mouse_x += dx;
        //x->x_mouse_x = maxi(x->x_mouse_x, x->x_gui.x_obj.te_xpix);
        //x->x_mouse_x = mini(x->x_mouse_x, x->x_gui.x_obj.te_xpix + x->x_gui.x_w);

        x->x_mouse_y += dy;
        //x->x_mouse_y = maxi(x->x_mouse_y, x->x_gui.x_obj.te_ypix);
        //x->x_mouse_y = mini(x->x_mouse_y, x->x_gui.x_obj.te_ypix + x->x_gui.x_h);

        SETFLOAT(at, 0.0);
        SETFLOAT(at+1, (t_floatarg)x1);
        SETFLOAT(at+2, (t_floatarg)y1);
        SETFLOAT(at+3, (t_floatarg)x->x_mouse_x);
        SETFLOAT(at+4, (t_floatarg)x->x_mouse_y);
        iemgui_out_list(&x->x_gui, 0, 0, &s_list, 5, at);
    }
}

static int image_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix,
    int shift, int alt, int dbl, int doit)
{
    t_image *x = (t_image *)z;
    t_atom at[5];
    int x1, x2, y1, y2;
    //post("image_newclick x->x_click=%d dbl=%d iemgui=%d", x->x_click, dbl, x->x_gui.x_obj.te_iemgui);
    image_getrect((t_gobj *)x, glist, &x1, &y1, &x2, &y2);
    /*
    post("==============\nimage_newclick\n...getrect x1=%d x2=%d \
        y1=%d y2=%d\nte_xpix=%d te_typix=%d x_mouse_x=%d mouse_y=%d\nxpix=%d ypix=%d\n==============",
        x1, x2, y1, y2, x->x_gui.x_obj.te_xpix, x->x_gui.x_obj.te_ypix, x->x_mouse_x, x->x_mouse_y, xpix, ypix);
    */

    if (doit && x->x_click && x->x_click < 3)
    {
        if (x->x_click == 1)
        {
            iemgui_out_bang(&x->x_gui, 0, 1);
        }
        else if (x->x_click == 2)
        {
            x->x_mouse_x = (xpix - x1);
            x->x_mouse_y = (ypix - y1);
            glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
                (t_glistmotionfn)image_motion, 0, 0, (t_floatarg)xpix, (t_floatarg)ypix, 0);
            SETFLOAT(at, 1.0);
            SETFLOAT(at+1, (t_floatarg)x1);
            SETFLOAT(at+2, (t_floatarg)y1);
            SETFLOAT(at+3, (t_floatarg)(xpix - x1));
            SETFLOAT(at+4, (t_floatarg)(ypix - y1));
            iemgui_out_list(&x->x_gui, 0, 0, &s_list, 5, at);
        }

    }

    // this is how we catch mouse-up but only if the object calls glist_grab
    // and has a motionfn
    else if (dbl == -1 && x->x_click == 2)
    {
        glist_grab(x->x_gui.x_glist, 0, 0, 0, 0, 0, 0, 0);
        SETFLOAT(at, 0.0);
        SETFLOAT(at+1, (t_floatarg)x1);
        SETFLOAT(at+2, (t_floatarg)y1);
        SETFLOAT(at+3, (t_floatarg)(xpix - x1));
        SETFLOAT(at+4, (t_floatarg)(ypix - y1));
        iemgui_out_list(&x->x_gui, 0, 0, &s_list, 5, at);
    }

    // now check if we are in the mode 3 in which case we pass everything regardless
    // whether we have clicked or not
    else if (x->x_click == 3)
    {
        //post("mode 3 dbl=%d doit=%d", dbl, doit);
        if (doit) 
        {
            x->x_mode3_click = 1;
            //glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
            //    (t_glistmotionfn)image_motion, 0, 0, (t_floatarg)xpix, (t_floatarg)ypix, 0);
        }
        // the following can be either -1 for a regular mouseup or -2 for a silent one
        // -2 is used when we are in mode 3 and the mouseup comes outside the object
        // which means the object should not have received the mouseup unless grabbed,
        // yet in mode 3 because we pass the click through to objects below, we cannot
        // grab the object, since currently the mostly inherited behavior of a grab
        // is that there can be only one of those per canvas.
        else if (dbl < 0) 
        {
            x->x_mode3_click = -1;
            //post("==image mode3_click=%d", x->x_mode3_click);
            //glist_grab(x->x_gui.x_glist, 0, 0, 0, 0, 0, 0, 0);
        }
        else
        {
            // we neither clicked nor released the mouse button, which could mean
            // we are either holding it clicked or not pressing
            x->x_mode3_click = 0;
        }

        if (dbl != -2)
        {
            SETFLOAT(at, (t_floatarg)x->x_mode3_click);
            SETFLOAT(at+1, (t_floatarg)x1);
            SETFLOAT(at+2, (t_floatarg)y1);
            SETFLOAT(at+3, (t_floatarg)(xpix - x1));
            SETFLOAT(at+4, (t_floatarg)(ypix - y1));
            iemgui_out_list(&x->x_gui, 0, 0, &s_list, 5, at);
        }
    }

    return(1);
}

static void image_clickmode(t_image *x, t_float f)
{
    if (f >= 0 && f <= 3)
        x->x_click = (int)f;
    // ico@vt.edu 2021-10-08: change the iemgui object version
    // based on whether it has passthrough click mode, so that
    // g_editor.c can handle it properly
    x->x_gui.x_obj.te_iemgui = (x->x_click == 3 ? 3 : 2);
}

static void image_gop_spill(t_image* x, t_floatarg f)
{
    x->x_gop_spill = (f == 0 || f == 1 ? f : 0);
    if (x->x_img_loaded)
        image_update(
            x, x->x_gui.x_glist, x->x_adj_img_width, x->x_adj_img_height, 0);
    image_displace((t_gobj*)x, x->x_gui.x_glist, 0.0, 0.0);
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"gop_spill",x->x_gop_spill);
    }
}

static void image_alpha(t_image *x, t_floatarg f)
{
    if (f > 1.0) f = 1.0;
    if (f < 0.0) f = 0.0;
    x->x_alpha = f;
    gui_vmess("gui_ggee_image_alpha", "xxf",
        glist_getcanvas(x->x_gui.x_glist), x, x->x_alpha);
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_float(properties,"alpha",x->x_alpha);
    }
}

static void image_visible(t_image *x, t_floatarg f)
{

    if (f == 0 || f == 1)
    {
        x->x_visible = (int)f;
        if (x->x_img_loaded)
            gui_vmess("gui_ggee_image_toggle_visible", "xxi",
                glist_getcanvas(x->x_gui.x_glist), x, x->x_visible);
    }
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"visible",x->x_visible);
    }
}

// constrain is by default on (or 1)
static void image_constrain(t_image* x, t_floatarg f)
{
    x->x_constrain = (f >= 0 && f <= 2 ? f : 1);
    if (x->x_img_loaded)
    {
        if (x->x_constrain == 2)
        {
            x->x_constrain_w = x->x_adj_img_width;
            x->x_constrain_h = x->x_adj_img_height;
        }
        image_update(
            x, x->x_gui.x_glist, x->x_adj_img_width, x->x_adj_img_height, 0);
    }
    image_displace((t_gobj*)x, x->x_gui.x_glist, 0.0, 0.0);
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"constrain",x->x_constrain);
    }
}


static void image_gop_spill_size(t_image* x, t_floatarg w, t_floatarg h)
{
    //post("image_gop_spill_size %d %d", (int)w, (int)h);
    // we need a size of at least IEM_GUI_MINSIZE (8) to have a meaningful
    // selection frame around the selection box
    if (!x->x_gop_spill)
    {
        post("warning: image is ignoring the gop_spill message "
             "since the gop_spill option is disabled");
        return;
    }

    int changed = 0;

    if ((int)w >= IEM_GUI_MINSIZE)
    {
        x->x_gui.x_w = ((int)w <= x->x_adj_img_width ? (int)w : x->x_adj_img_width);
        changed = 1;
    }

    if ((int)h >= IEM_GUI_MINSIZE)
    {
        x->x_gui.x_h = ((int)h <= x->x_adj_img_height ? (int)h : x->x_adj_img_height);
        changed = 1;
    }
    else if (changed)
    {
        x->x_gui.x_h = ((int)w <= x->x_adj_img_height ? (int)w : x->x_adj_img_height);
    }

    if (changed)
    {
        if (x->x_img_loaded)
            image_update(
                x, x->x_gui.x_glist, x->x_adj_img_width, x->x_adj_img_height, 0);
        image_displace((t_gobj*)x, x->x_gui.x_glist, 0.0, 0.0);
    }
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"width",x->x_gui.x_w);
        properties_set_field_int(properties,"height",x->x_gui.x_h);
    }
}

static void image_size(t_image* x, t_floatarg w, t_floatarg h)
{
    //post("image_size %d %d", (int)w, (int)h);
    // we need a size of at least IEM_GUI_MINSIZE (8) to have a meaningful
    // selection frame around the selection box
    int changed = 0;

    if ((int)w >= IEM_GUI_MINSIZE)
    {
        x->x_adj_img_width = (int)w;
        changed = 1;
    }

    if ((int)h >= IEM_GUI_MINSIZE)
    {
        x->x_adj_img_height = (int)h;
        changed = 1;
    }
    else if (changed)
    {
        x->x_adj_img_height = (int)w;
    }

    if (changed)
    {
        if (x->x_img_loaded)
            image_update(
                x, x->x_gui.x_glist, x->x_adj_img_width, x->x_adj_img_height, 0);
        image_displace((t_gobj*)x, x->x_gui.x_glist, 0.0, 0.0);
    }
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"visible_width",x->x_adj_img_width);
        properties_set_field_int(properties,"visible_height",x->x_adj_img_height);
        properties_set_field_int(properties,"width",x->x_gui.x_w);
        properties_set_field_int(properties,"height",x->x_gui.x_h);
    }
}

static void image_open(t_image* x, t_symbol *s, t_int argc, t_atom *argv)
{
    x->x_img_loaded = 0;
    t_symbol *oldfname = NULL;
    if (strcmp("@pd_extra/ggee/empty_image.png", x->x_fname->s_name) != 0)
        oldfname = x->x_fname;
    x->x_fname = get_filename(argc, argv);
    x->x_img_width = 0;
    x->x_img_height = 0;
    t_symbol *fname = image_trytoopen(x);
    if (fname) {
        if (x->x_gop_spill)
            gui_vmess("gui_ggee_image_display", "xxi", 
                glist_getcanvas(x->x_gui.x_glist), x, 0);
        // ico@vt.edu 2021-10-08: here we need to convert the path
        // to realpath because nw.js has an inconsistent way of
        // handling paths. e.g., when running pd-l2ork on Linux from
        // a command line, first time you open the patch it works ok.
        // however, if you open it the second time from the recent
        // files menu, it fails to locate files with a relative path.
        char realfname[FILENAME_MAX];
        realpath(fname->s_name, realfname);
        gui_vmess("gui_load_image", "xxs",
            glist_getcanvas(x->x_gui.x_glist), x, realfname);
    }
    else
    {
        if (!oldfname)
        {
            pd_error(x, "requested image %s not found... "
                "reverting to default...", x->x_fname->s_name);
            gui_vmess("gui_load_default_image", "xx",
                glist_getcanvas(x->x_gui.x_glist), x);
        }
        else
        {
            pd_error(x, "requested image %s not found... "
                "reverting filename to the current image %s...",
                x->x_fname->s_name, oldfname->s_name);
            x->x_fname = oldfname;
        }
    }
    // this used to check whether the fname has changed using,
    // if (x->x_fname != oldfname && ... Doing so, however, prevents
    // dialog values to stick since user may not want to change the
    // the filename, yet they need to pass it together with other vars
    if (glist_isvisible(glist_getcanvas(x->x_gui.x_glist)))
    {
        gui_vmess("gui_image_configure", "xxxs",
            glist_getcanvas(x->x_gui.x_glist),
            x,
            x,
            "nw");
        gui_vmess("gui_image_size_callback", "xxs",
            glist_getcanvas(x->x_gui.x_glist), x, x->x_inlet->s_name);
    }

    if (x->x_fname == oldfname)
        x->x_img_loaded = 1;
    //image_vis((t_gobj *)x, x->x_glist, 0);
    //image_vis((t_gobj *)x, x->x_glist, 1);
}

static void image_reload(t_image *x)
{
    if (x->x_img_loaded)
    {
        t_atom at[1];
        SETSYMBOL(at, x->x_fname);
        image_open(x, NULL, 1, &at);
    }
}

static void image_imagesize_callback(t_image *x, t_float w, t_float h) {
    //post("received w %f h %f should %d spill %d\n", w, h, \
        gobj_shouldvis((t_gobj *)x, glist_getcanvas(x->x_glist)), x->x_gop_spill);
    x->x_img_loaded = 1;
    x->x_img_width = w;
    x->x_img_height = h;
    if (!x->x_adj_img_width || !x->x_adj_img_height)
    {
        x->x_adj_img_width = x->x_img_width;
        x->x_adj_img_height = x->x_img_height;
    }
    if (x->x_constrain == 2)
    {
        x->x_constrain_w = x->x_adj_img_width;
        x->x_constrain_h = x->x_adj_img_height;
    }
    if (x->x_img_width + x->x_img_height == 0)
    {
        //invalid image
        if (strcmp("@pd_extra/ggee/empty_image.png", x->x_fname->s_name) != 0)
        {
            pd_error(x, "image %s not found... opening default image...",
                x->x_fname->s_name);
            x->x_fname = gensym("@pd_extra/ggee/empty_image.png");
            image_trytoopen(x);
            return;
        }
    }
    // if we got this far, we have a successfully loaded image, and if no rotation
    // point has been set, we set the default rotation point at the image center
    if (!x->x_rot_pt_init)
    {
        x->x_rot_x = (t_float)x->x_adj_img_width / 2.0;
        x->x_rot_y = (t_float)x->x_adj_img_height / 2.0;
        x->x_rot_pt_init = 1;
    }

    // removed in the following statement additional condition
    // as it does not seem necessary: && !x->x_gop_spill
    if (!gobj_shouldvis((t_gobj *)x, x->x_gui.x_glist))
    {
            //fprintf(stderr,"erasing\n");
            image_erase(x, glist_getcanvas(x->x_gui.x_glist));
    }
    else
    {
        // if necessary, reselect if we are on a toplevel canvas to adjust
        // the selection rectangle
        if (x->x_legacy)
        {
            //post("callback offset");
            x->x_gui.x_obj.te_xpix -= x->x_img_width/2;
            x->x_gui.x_obj.te_ypix -= x->x_img_height/2;
            x->x_legacy = 0;
            //if (x->x_glist != glist_getcanvas(x->x_glist))
            //    glist_redraw(x->x_glist);
            //image_displace((t_gobj *)x, glist_getcanvas_(x->x_glist),
            //    -(x->x_img_width)/2, -(x->x_img_height)/2);
        }
        // the following call *must* use x->x_glist, not glist_getcanvas
        // because otherwise gop objects are erroneously drawn on a parent
        // canvas' x and y coordinates instead of ones inside the gop
        // subpatch/abstraction
        x->x_draw_firstime = 0;
        image_drawme(x, x->x_gui.x_glist);

        if (glist_isselected(x->x_gui.x_glist, (t_gobj *)x) &&
            glist_getcanvas(x->x_gui.x_glist) == x->x_gui.x_glist)
        {
            image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 0);
            image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 1);
        }
        
        canvas_fixlinesfor(x->x_gui.x_glist,(t_text*) x);
    }

    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"visible_width",x->x_adj_img_width);
        properties_set_field_int(properties,"visible_height",x->x_adj_img_height);
        properties_set_field_int(properties,"width",x->x_gui.x_w);
        properties_set_field_int(properties,"height",x->x_gui.x_h);
        properties_set_field_float(properties,"rotate_x",x->x_rot_x);
        properties_set_field_float(properties,"rotate_y",x->x_rot_y);
        properties_set_field_float(properties,"reset_width",x->x_img_width);
        properties_set_field_float(properties,"reset_height",x->x_img_height);
    }
}

// resizemode is primarily used for the motionhook (constrain resize by neither
// axis or 0, by X axis or 1, or by Y axis or 2). Most other calls will use 0.
static void image_update(t_image *x, t_glist *glist, int width, int height, int resizemode)
{
    t_float c; // constrain
    t_atom at[3];
    if (x->x_constrain == 2) { // custom
        c = (float)x->x_constrain_w / (float)x->x_constrain_h;
    } else if (x->x_constrain == 1) {
        c = (float)x->x_img_width / (float)x->x_img_height;
    }

    if (x->x_constrain > 0)
    {
        switch (resizemode)
        {
            case 0:
                x->x_adj_img_width = maxi(width, IEM_GUI_MINSIZE);
                x->x_adj_img_height = maxi(width * (1/c), IEM_GUI_MINSIZE);
                break;
            case 1:
                x->x_adj_img_width = maxi(width, IEM_GUI_MINSIZE);
                x->x_adj_img_height = maxi(width * (1/c), IEM_GUI_MINSIZE);
                break;
            case 2:
                x->x_adj_img_width = maxi(height * c, IEM_GUI_MINSIZE);
                x->x_adj_img_height = maxi(height, IEM_GUI_MINSIZE);
                break;
        }
    } else {
        x->x_adj_img_width = maxi(width, IEM_GUI_MINSIZE);
        x->x_adj_img_height = maxi(height, IEM_GUI_MINSIZE);
    }

    gui_vmess("gui_ggee_image_resize", "xxiiii",
        glist_getcanvas(glist),
        x,
        x->x_adj_img_width,
        x->x_adj_img_height,
        resizemode,
        x->x_constrain        
    );
    gui_vmess("gui_ggee_image_offset", "xxxii",
        glist_getcanvas(glist),
        x,
        x,
        (x->x_gop_spill ? -(x->x_adj_img_width/2 - x->x_gui.x_w/2) : 0),
        (x->x_gop_spill ? -(x->x_adj_img_height/2 - x->x_gui.x_h/2) : 0)
    );
    if (x->x_gui.x_w > x->x_adj_img_width || !x->x_gop_spill)
    {
        x->x_gui.x_w = x->x_adj_img_width;
    }
    if (x->x_gui.x_h > x->x_adj_img_height || !x->x_gop_spill)
    {
        x->x_gui.x_h = x->x_adj_img_height;
    }
    if(glist_istoplevel(x->x_gui.x_glist))
    {
        gui_vmess("gui_image_update_border", "xxii", 
            glist_getcanvas(glist), x, x->x_gui.x_w, x->x_gui.x_h);
        iemgui_io_draw_move(&x->x_gui);
    }
    image_dorotate(x);
    scrollbar_update(x->x_gui.x_glist);
    SETSYMBOL(at, gensym("size"));
    SETFLOAT(at+1, (t_floatarg)(x->x_adj_img_width));
    SETFLOAT(at+2, (t_floatarg)(x->x_adj_img_height));
    iemgui_out_list(&x->x_gui, 0, 0, &s_list, 3, at);
}

// resets the image to its original size
static void image_reset(t_image *x)
{
    if (x->x_img_loaded)
    {
        x->x_adj_img_width = x->x_img_width;
        x->x_adj_img_height = x->x_img_height;
        image_update(x, x->x_gui.x_glist,
            x->x_adj_img_width, x->x_adj_img_width, 0);
    }
}

static void image_dorotate(t_image *x)
{
    // variables for calculating the spill offset
    int off_x = 0;
    int off_y = 0;
    if (x->x_gop_spill)
    {
        if (x->x_gui.x_w < x->x_adj_img_width)
            off_x = x->x_adj_img_width / 2 - x->x_gui.x_w / 2;
        if (x->x_gui.x_w < x->x_adj_img_width)
            off_y = x->x_adj_img_height / 2 - x->x_gui.x_h / 2;
    }
    gui_vmess("gui_ggee_image_rotate", "xxfii", 
        glist_getcanvas(x->x_gui.x_glist), x, x->x_rot_angle,
        (t_int)x->x_rot_x - off_x, (t_int)x->x_rot_y - off_y);
    t_int properties = gfxstub_haveproperties((void *)x);
    if (properties)
    {
        properties_set_field_int(properties,"rotate_x",x->x_rot_x);
        properties_set_field_int(properties,"rotate_y",x->x_rot_y);
        properties_set_field_int(properties,"rotate",x->x_rot_angle);
    }
}

static void image_rotate(t_image* x, t_symbol *s, t_int argc, t_atom *argv)
{
    if (glist_isvisible(glist_getcanvas(x->x_gui.x_glist)))
    {
        if (argc && x->x_img_loaded && argv[0].a_type == A_FLOAT)
        {
            if (argc == 1 && argv[0].a_type == A_FLOAT)
            {
                // rotate around the center of the image
                x->x_rot_angle = atom_getfloat(&argv[0]);
                image_dorotate(x);

            } 
            else if (argc == 3 && argv[1].a_type == A_FLOAT && argv[2].a_type == A_FLOAT)
            {
                // rotate around a specified origin
                x->x_rot_angle = atom_getfloat(&argv[0]);
                x->x_rot_x = atom_getfloat(&argv[1]);
                x->x_rot_y = atom_getfloat(&argv[2]);
                x->x_rot_pt_init = 1;
                image_dorotate(x);        
            }
            else
                pd_error(x, "rotate: invalid number or type of arguments--"
                    "should be either 1 or 3 floats...");
        }
        else
        {
            pd_error(x, "rotate: no image loaded yet...");
        }
    }
}

static void image__clickhook(t_scalehandle *sh, int newstate)
{
    t_image *x = (t_image *)(sh->h_master);
    if (newstate)
    {
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
        if (!sh->h_scale)
            scalehandle_click_label(sh);
    }
    if (sh->h_scale)
    {
        int x1, y1, x2, y2;
        image_getrect((t_gobj *)x, x->x_gui.x_glist, &x1, &y1, &x2, &y2);
        sh->h_adjust_x = sh->h_offset_x - (x2 + (x->x_adj_img_width/2 - x->x_gui.x_w/2));
        sh->h_adjust_y = sh->h_offset_y - (y2 + (x->x_adj_img_height/2 - x->x_gui.x_h/2));
        /* Hack to set the cursor since we're doing and end-run
           around canas_doclick here */
    }
    sh->h_dragon = newstate;
}

static void image__motionhook(t_scalehandle *sh, t_floatarg mouse_x, t_floatarg mouse_y)
{
    //post("image__motionhook %d %d", (int)mouse_x, (int)mouse_y);
    if (sh->h_scale)
    {
        int c; // aspect ratio
        t_image *x = (t_image *)(sh->h_master);
        //post("w=%d h=%d aimgw=%d aimgh=%d | mx=%d my=%d | adjx=%d adjy=%d",\
            x->x_gui.x_w, x->x_gui.x_h, x->x_adj_img_width, x->x_adj_img_height,\
            (int)mouse_x, (int)mouse_y, sh->h_adjust_x, sh->h_adjust_y);

        int width = (sh->h_constrain == CURSOR_EDITMODE_RESIZE_Y) ?
            x->x_adj_img_width :
            (x->x_gop_spill) ?
                ((int)mouse_x - (x->x_gui.x_obj.te_xpix + x->x_gui.x_w/2)
                    - sh->h_adjust_x) * 2 :
                (int)mouse_x - x->x_gui.x_obj.te_xpix;
        int height = (sh->h_constrain == CURSOR_EDITMODE_RESIZE_X) ?
            x->x_adj_img_height :
            (x->x_gop_spill) ?
                ((int)mouse_y - (x->x_gui.x_obj.te_ypix + x->x_gui.x_h/2)
                    - sh->h_adjust_y) * 2 :
                (int)mouse_y - x->x_gui.x_obj.te_ypix;
        //post("c img_resize %d %d", width, height);
        // this function is in g_all_guis.c and in nw.js does nothing
        // LATER: remove it altogether
        //scalehandle_drag_scale(sh);

        if (glist_istoplevel(x->x_gui.x_glist))
        {
            //my_canvas_draw_move(x, x->x_gui.x_glist);
            //scalehandle_unclick_scale(sh);
            int resizemode = 0; // normal resize
            if (sh->h_constrain == CURSOR_EDITMODE_RESIZE_X)
                resizemode = 1;
            else if (sh->h_constrain == CURSOR_EDITMODE_RESIZE_Y)
                resizemode = 2;

            image_update(x, x->x_gui.x_glist, width, height, resizemode);

            gui_vmess("gui_image_update_border", "xxiiiii", 
                x->x_gui.x_glist, x, x->x_gui.x_w, x->x_gui.x_h,
                x->x_gui.x_w + (x->x_adj_img_width/2 - x->x_gui.x_w/2),
                x->x_gui.x_h + (x->x_adj_img_height/2 - x->x_gui.x_h/2)
            );

            gui_vmess("gui_iemgui_label_show_drag_handle", "xxiiii",
                x->x_gui.x_glist, x, 0, 0, 0, 1);
            gui_vmess("gui_iemgui_label_show_drag_handle", "xxiiii",
                x->x_gui.x_glist, x, 1, 
                x->x_gui.x_w + (x->x_adj_img_width/2 - x->x_gui.x_w/2) - 
                    (x->x_adj_img_width == x->x_gui.x_w ? 0 : 2) + 8,
                x->x_gui.x_h + (x->x_adj_img_height/2 - x->x_gui.x_h/2) -
                    (x->x_adj_img_height == x->x_gui.x_h ? 0 : 2),
                1);

            //scalehandle_unclick_scale(sh);
            // here instead of scalehandle_unclick_scale we only call
            // calls inside that function that we need, since reselecting
            // creates extra image
            iemgui_io_draw_move(x);
            canvas_fixlinesfor(x->x_gui.x_glist, (t_text *)x);
            canvas_getscroll(x->x_gui.x_glist);
        }

        t_int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            properties_set_field_int(properties,"visible_width",x->x_adj_img_width);
            properties_set_field_int(properties,"visible_height",x->x_adj_img_height);
            properties_set_field_int(properties,"width",x->x_gui.x_w);
            properties_set_field_int(properties,"height",x->x_gui.x_h);
        }
    }
    scalehandle_dragon_label(sh, mouse_x, mouse_y);
}

static void image_properties(t_gobj *z, t_glist *owner)
{
    t_image *x = (t_image *)z;
    char *gfx_tag;
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);

    gfx_tag = gfxstub_new2(&x->x_gui.x_obj.ob_pd, x);

    gui_start_vmess("gui_image_dialog", "s", gfx_tag);
    gui_start_array();
    gui_s("type");              gui_s("image");
    gui_s("filename");          gui_s(x->x_fname->s_name);
    gui_s("width");             gui_i(x->x_gui.x_w);
    gui_s("height");            gui_i(x->x_gui.x_h);
    gui_s("visible_width");     gui_i(x->x_adj_img_width);
    gui_s("visible_height");    gui_i(x->x_adj_img_height);
    gui_s("gop_spill");         gui_i(x->x_gop_spill);
    gui_s("click");             gui_i(x->x_click);
    gui_s("constrain");         gui_i(x->x_constrain);
    gui_s("reset_width");       gui_i(x->x_img_width);
    gui_s("reset_height");      gui_i(x->x_img_height);
    gui_s("send_symbol");       gui_s(x->x_gui.x_snd->s_name);
    gui_s("receive_symbol");    gui_s(x->x_gui.x_rcv->s_name);
    gui_s("rotate");            gui_f(x->x_rot_angle);
    gui_s("rotate_x");          gui_f(x->x_rot_x);
    gui_s("rotate_y");          gui_f(x->x_rot_y);
    gui_s("label");             gui_s(srl[2]->s_name);
    gui_s("x_offset");          gui_i(x->x_gui.x_ldx);
    gui_s("y_offset");          gui_i(x->x_gui.x_ldy);
    gui_s("label_color");       gui_i(0xffffff & x->x_gui.x_lcol);
    gui_s("font_style");        gui_i(x->x_gui.x_font_style);
    gui_s("font_size");         gui_i(x->x_gui.x_fontsize);
    gui_s("visible");           gui_i(x->x_visible);
    gui_s("alpha");             gui_f(x->x_alpha);
    gui_end_array();
    gui_end_vmess();
}

static void image_dialog(t_image *x, t_symbol *s, int argc,
    t_atom *argv)
{
    /*post("image_dialog argc=%d undo=%d incoming-label=<%s> cur-label=<%s>", argc,
        atom_getintarg(21, argc, argv),
        iemgui_getfloatsymarg(13,argc,argv)->s_name,
        x->x_gui.x_lab->s_name);*/ // 22 args in total
    t_symbol *srl[3];
    int oldsndrcvable = 0;
    t_symbol *oldfname = x->x_fname;

    if (atom_getintarg(21, argc, argv))
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
    image_open(x, NULL, 1, argv);
    x->x_gui.x_w = atom_getintarg(1, argc, argv);
    x->x_gui.x_h = atom_getintarg(2, argc, argv);
    x->x_adj_img_width = atom_getintarg(3, argc, argv);
    x->x_adj_img_height = atom_getintarg(4, argc, argv);
    x->x_gop_spill = atom_getintarg(5, argc, argv);
    x->x_click = atom_getintarg(6, argc, argv);
    x->x_gui.x_obj.te_iemgui = (x->x_click == 3 ? 3 : 2);
    x->x_constrain = atom_getintarg(7, argc, argv);
    srl[0] = iemgui_getfloatsymarg(8,argc,argv);
    srl[1] = iemgui_getfloatsymarg(9,argc,argv);
    x->x_rot_angle = atom_getintarg(10, argc, argv);
    x->x_rot_x = atom_getfloatarg(11, argc, argv);
    x->x_rot_y = atom_getfloatarg(12, argc, argv);
    srl[2] = iemgui_getfloatsymarg(13,argc,argv);
    x->x_gui.x_ldx = atom_getintarg(14, argc, argv);
    x->x_gui.x_ldy = atom_getintarg(15, argc, argv);
    x->x_gui.x_lcol = atom_getintarg(16, argc, argv) & 0xffffff;
    int f = atom_getintarg(17, argc, argv); // font style (resolved below)
    x->x_gui.x_fontsize = maxi(atom_getintarg(18, argc, argv),4);
    x->x_visible = atom_getintarg(19, argc, argv);
    x->x_alpha = atom_getfloatarg(20, argc, argv);

    if(iemgui_has_rcv(&x->x_gui)) oldsndrcvable |= IEM_GUI_OLD_RCV_FLAG;
    if(iemgui_has_snd(&x->x_gui)) oldsndrcvable |= IEM_GUI_OLD_SND_FLAG;
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

    x->x_gui.x_snd_unexpanded=srl[0];
    srl[0]=canvas_realizedollar(x->x_gui.x_glist, srl[0]);
    x->x_gui.x_rcv_unexpanded=srl[1];
    srl[1]=canvas_realizedollar(x->x_gui.x_glist, srl[1]);
    x->x_gui.x_lab_unexpanded=srl[2];
    srl[2]=canvas_realizedollar(x->x_gui.x_glist, srl[2]);

    if(srl[1]!=x->x_gui.x_rcv)
    {
        if(iemgui_has_rcv(&x->x_gui))
            pd_unbind((t_pd *)&x->x_gui, x->x_gui.x_rcv);
        x->x_gui.x_rcv = srl[1];
        pd_bind((t_pd *)&x->x_gui, x->x_gui.x_rcv);
    }
    x->x_gui.x_snd = srl[0];
    x->x_gui.x_lab = srl[2];
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_draw_io(&x->x_gui, oldsndrcvable);
    if(f<0 || f>2) f=0;
    x->x_gui.x_font_style = f;

    // we should not use this, as doing so messes up the drawing order
    //image_vis(x, x->x_gui.x_glist, 0);
    //image_vis(x, x->x_gui.x_glist, 1);
    // we don't need this either, since it is called by the image callback
    //image_draw(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    if (x->x_gui.x_selected) {
        image_select((t_gobj *)x, x->x_gui.x_glist, 0);
        image_select((t_gobj *)x, x->x_gui.x_glist, 1);
    }
    canvas_dirty(x->x_gui.x_glist, 1);
}

void image_draw(t_image *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)      sys_queuegui(x, glist, image_drawme);
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)   image_draw_move(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_NEW)    image_drawme(x, glist);
    //else if(mode == IEM_GUI_DRAW_MODE_SELECT) iemgui_select(x, glist);
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG) image_drawme(x, glist);
}

static void image_setwidget(void)
{
    image_widgetbehavior.w_getrectfn = image_getrect;
    image_widgetbehavior.w_displacefn = image_displace;
    image_widgetbehavior.w_selectfn = image_select;
    image_widgetbehavior.w_activatefn = image_activate;
    image_widgetbehavior.w_deletefn = image_delete;
    image_widgetbehavior.w_visfn = image_vis;
    image_widgetbehavior.w_clickfn = image_newclick;
    image_widgetbehavior.w_displacefnwtag = image_displace_wtag;
}

static void image_free(t_image *x)
{
    //sys_vgui("image delete img%x\n", x);
    gui_vmess("gui_image_free", "x", x);
    gfxstub_deleteforkey(&x->x_gui);
    if (x->x_inlet)
    {
        pd_unbind(&x->x_gui.x_obj.ob_pd,x->x_inlet);
    }
    if (x->x_gui.x_rcv != s_empty)
    {
        pd_unbind(&x->x_gui.x_obj.ob_pd,x->x_gui.x_rcv);
    }
    if (x->x_gui.x_handle) scalehandle_free(x->x_gui.x_handle);
}

static void *image_new(t_symbol *s, t_int argc, t_atom *argv)
{
    t_image *x = (t_image *)pd_new(image_class);
    x->x_gui.x_glist = (t_glist*) canvas_getcurrent();
    x->x_gui.x_w = 25;
    x->x_gui.x_h = 25;
    x->x_img_width = 0;
    x->x_img_height = 0;
    // we initialize adjusted img w/h as zero until we know otherwise
    // this allows us to set it to same as x_img_width/height in the
    // image_imagesize_callback. Otherwise, we use it to resize it there
    x->x_adj_img_width = 0;
    x->x_adj_img_height = 0;
    x->x_gop_spill = 0;
    x->x_click = 0;
    x->x_legacy = 0;
    x->x_img_loaded = 0;
    x->x_constrain = 1;
    x->x_rot_x = 0;
    x->x_rot_y = 0;
    x->x_rot_pt_init = 0;
    x->x_rot_angle = 0;
    // We only use the label color and initialize others, so that we can
    // safely use the iemgui calls...
    x->x_gui.x_bcol = 0x00;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x00;
    x->x_gui.x_ldy = -8; // default label y offset
    x->x_draw_firstime = 1;
    x->x_visible = 1;
    x->x_alpha = 1.0;
    char buf[MAXPDSTRING];

    // used for the last if statement since we decrement the argc below
    int n_args = argc;
    int fs = 10;

    /* For debugging:
    post("image instantiating...");
    for(int i = 0; i < argc; i++)
    {
        if (argv[i].a_type == A_FLOAT)
        {
            post("...%d arg f: %d", i, (int)atom_getfloat(&argv[i]));
        }
        else if (argv[i].a_type == A_SYMBOL)
        {
            post("...%d arg s: <%s>", i, atom_getsymbol(argv)->s_name);
        }
        else
            post("...%d unknown variable", i);
    }
    post("...done argc=%d", argc);
    */
    
    // this should take care of the sends and receives
    if (argc >= 9)
        iemgui_new_getnames(&x->x_gui, 5, argv);
    else
        iemgui_new_getnames(&x->x_gui, 5, 0);

    x->x_fname = get_filename(argc, argv);
    if (strlen(x->x_fname->s_name) > 0)
    {
        //post("get_filename succeeded <%s> <%s>\n", \
            x->x_fname->s_name, atom_getsymbol(argv)->s_name);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("gop_spill succeeded");
        x->x_gop_spill = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("click succeeded");
        x->x_click = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("width succeeded");
        x->x_gui.x_w = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("height succeeded");
        x->x_gui.x_h = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc >= 3)
    {
        argc -= 3;
        argv += 3;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("adj_img_width succeeded");
        x->x_adj_img_width = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("adj_img_height succeeded");
        x->x_adj_img_height = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("constrain succeeded");
        x->x_constrain = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("font style succeeded");
        iem_inttofstyle(&x->x_gui, atom_getintarg(0, argc, argv));
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("font size succeeded");
        fs = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_SYMBOL)
    {
        //post("label color succeeded");
        iemgui_all_loadcolors(&x->x_gui, 0, 0, argv);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("label x succeeded");
        x->x_gui.x_ldx = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("label y succeeded");
        x->x_gui.x_ldy = (int)atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("rotate x succeeded");
        x->x_rot_x = atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("rotate y succeeded");
        x->x_rot_y = atom_getfloat(&argv[0]);
        x->x_rot_pt_init = 1;
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("rotate angle succeeded");
        x->x_rot_angle = atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("visible succeeded");
        x->x_visible = atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    if (argc && argv[0].a_type == A_FLOAT)
    {
        //post("alpha succeeded");
        x->x_alpha = atom_getfloat(&argv[0]);
        argc--;
        argv++;
    }
    else if (n_args > 1) {
        // we are dealing with a legacy object and NOT a newly instantiated object
        // which has no arguments
        post("image: detected legacy patch... translating, so that when the "
             "patch is saved with the new version of pd-l2ork, the image "
             "object retains the same location...");
        x->x_legacy = 1;        
    }

    iemgui_verify_snd_ne_rcv(&x->x_gui);
    if(fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    if (x->x_gui.x_font_style < 0 || x->x_gui.x_font_style > 2)
        x->x_gui.x_font_style = 0;

    x->x_gui.x_draw = (t_iemfunptr)image_draw;
    // Create default receiver
    sprintf(buf, "#%zx", (t_uint)x);
    x->x_inlet = gensym(buf);
    pd_bind(&x->x_gui.x_obj.ob_pd, x->x_inlet);
    outlet_new(&x->x_gui.x_obj, 0);
    if (x->x_gui.x_rcv != s_empty)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);    
    x->x_gui.x_handle = scalehandle_new(
        (t_object *)x,
        x->x_gui.x_glist,
        1,
        image__clickhook,
        image__motionhook);
    x->x_gui.x_lhandle = scalehandle_new(
        (t_object *)x,
        x->x_gui.x_glist,
        0,
        image__clickhook,
        image__motionhook);
    sprintf(buf, "_s%zx", (t_uint)x);
    pd_bind((t_pd *)x->x_gui.x_handle, x->x_gui.x_handle->h_bindsym = gensym(buf));
    // we set te_iemgui to denote 3rd-party iemgui-based object
    // 0 = non-iemgui
    // 1 = iemgui
    // 2 = 3rd-party iemgui-based that uses mycanvas resize hooks
    // 3 = same as 2 that also both captures both mouse motion and clicks, while
    //     also passing the same to clickable objects below it
    x->x_gui.x_obj.te_iemgui = (x->x_click == 3 ? 3 : 2);
    //post("click=%d iemgui=%d", x->x_click, x->x_gui.x_obj.te_iemgui);

    x->x_gui.x_color1 = &x->x_gui.x_lcol;
    x->x_gui.x_color2 = NULL;
    x->x_gui.x_color3 = NULL;
    x->x_mode3_click = 0;

    return (x);
}

void image_setup(void)
{
    image_class = class_new(gensym("image"),
                (t_newmethod)image_new, (t_method)image_free,
                sizeof(t_image),0, A_GIMME,0);
    class_addmethod(image_class, (t_method)image_clickmode, gensym("click"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_reset, gensym("reset"),
        A_NULL, 0);
    class_addmethod(image_class, (t_method)image_open, gensym("open"),
        A_GIMME, 0);
    class_addmethod(image_class, (t_method)image_gop_spill, gensym("gopspill"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_constrain, gensym("constrain"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_visible, gensym("visible"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_alpha, gensym("alpha"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_gop_spill_size, gensym("gopspill_size"),
        A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_size, gensym("size"),
        A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_rotate, gensym("rotate"),
        A_GIMME, 0);
    class_addmethod(image_class, (t_method)image_reload, gensym("reload"),
        A_NULL, 0);
    class_addmethod(image_class, (t_method)image_dialog,
        gensym("dialog"), A_GIMME, 0);
    class_addmethod(image_class, (t_method)image_imagesize_callback,
        gensym("_imagesize"), A_DEFFLOAT, A_DEFFLOAT, 0);    
    iemgui_class_addmethods(image_class);

    image_setwidget();
    class_setwidget(image_class, &image_widgetbehavior);
    class_setsavefn(image_class, image_save);
    class_setpropertiesfn(image_class, image_properties);
}
