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

/* ------------------------ image ----------------------------- */

static t_class *image_class;

typedef struct _image
{
    t_iemgui x_gui;
    int x_adj_img_width;
    int x_adj_img_height;
    int x_img_width;
    int x_img_height;
    int x_gop_spill;
    int x_click;
    //t_float x_clicked;
    //int x_selected;
    t_symbol* x_fname;
    t_symbol* x_inlet;
    int x_legacy;
    int x_img_loaded;
    int x_constrain;
} t_image;

// x_gui.x_w and x_h are used for the getbox size. This could be either
// both the image and selection box size (when gop_spill is off), or only
// the selection box size (when gop_spill is on)

// x_img_width and x_img_height are the image's original size

// x_adj_img_width and height are adjusted image size (based on user input)

extern t_symbol *s_image_empty;

/* widget helper functions */
static void image_select(t_gobj *z, t_glist *glist, int state);
static void image_vis(t_gobj *z, t_glist *glist, int vis);
static void image_resize(t_image *x, t_glist *glist, int width, int height, int resizemode);
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
        return 0;
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

// defined in s_main.c and used to offset object at creation time
// in case it is being autopatched
extern int glob_autopatch_connectme;

static void image_drawme(t_image *x, t_glist *glist, int firstime)
{
    //post("image_drawme");
    if(gobj_shouldvis((t_gobj *)x, glist))
    {
        if (firstime)
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
                gui_vmess("gui_load_image", "xxs",
                    glist_getcanvas(glist), x, fname->s_name);
            }
            else
            {
                gui_vmess("gui_load_default_image", "xx",
                    glist_getcanvas(glist), x);
            }
            // draw the new canvas image
            gui_vmess("gui_gobj_draw_image", "xxxsiiii",
                glist_getcanvas(glist),
                x,
                x,
                "nw",
                x->x_gui.x_w,
                x->x_gui.x_h,
                x->x_constrain,
                0 // this denotes ggee/image object type
            );
            //sys_vgui("catch {.x%zx.c delete %xS}\n", glist_getcanvas(glist), x);
            //sys_vgui(".x%x.c create image %d %d -tags %xS\n",
            //    glist_getcanvas(glist),text_xpix(&x->x_obj, glist),
            //    text_ypix(&x->x_obj, glist), x);
            gui_vmess("gui_image_size_callback", "xxs",
                glist_getcanvas(glist), x, x->x_inlet->s_name);
        }
        else
        {
            // move the gobj
            //sys_vgui(".x%x.c coords %xS %d %d\n",
            //    glist_getcanvas(glist), x,
            //    text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist));
            gui_vmess("gui_image_coords", "xxii",
                glist_getcanvas(glist),
                x,
                text_xpix(&x->x_gui.x_obj, glist),
                text_ypix(&x->x_gui.x_obj, glist)
            );
            if (x->x_img_loaded)
            {
                if (x->x_gop_spill)
                    image_resize(x, glist_getcanvas(glist),
                        x->x_adj_img_width, x->x_adj_img_height, 0);
                else
                    gui_vmess("gui_ggee_image_offset", "xxxii",
                        glist_getcanvas(glist),
                        x,
                        x,
                        (x->x_gop_spill ? -(x->x_adj_img_width/2 - x->x_gui.x_w/2) : 0),
                        (x->x_gop_spill ? -(x->x_adj_img_height/2 - x->x_gui.x_h/2) : 0)
                    );
            }
            if (glist_isselected(x->x_gui.x_glist, (t_gobj *)x) &&
                glist_getcanvas(x->x_gui.x_glist) == x->x_gui.x_glist)
            {
                image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 0);
                image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 1);
            }
            canvas_fixlinesfor(x->x_gui.x_glist, (t_text*)x);
        }
    }
}

static void image_erase(t_image* x, t_glist* glist)
{
    gui_vmess("gui_gobj_erase", "xx", glist_getcanvas(glist), x);
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
    post("image_getrect");
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
        ((glist_getcanvas(glist) != glist && !x->x_click) || (!glist->gl_edit && !x->x_click)))
    {
        *xp2 = *xp1;
        // only if we have an image loaded and we are placed within a GOP obliterate the height
        //if (glist_getcanvas(glist) != glist && (x->x_img_width + x->x_img_height) >= 2)
        //{
            //printf("blah\n");
            //*yp2 = *yp1;
        //}
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
    image_drawme(x, glist, 0);
    canvas_fixlinesfor(glist,(t_text*) x);
}

static void image_displace_wtag(t_gobj *z, t_glist *glist,
    int dx, int dy)
{
    //post("image_displace_wtag");
    t_image *x = (t_image *)z;
    x->x_gui.x_obj.te_xpix += dx;
    x->x_gui.x_obj.te_ypix += dy;
    /*sys_vgui(".x%x.c coords %xSEL %d %d %d %d\n",
           glist_getcanvas(glist), x,
           text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist),
           text_xpix(&x->x_obj, glist) + x->x_width, text_ypix(&x->x_obj, glist) + x->x_height);

    image_drawme(x, glist, 0);*/
    canvas_fixlinesfor(glist,(t_text*) x);
}

static void image_select(t_gobj *z, t_glist *glist, int state)
{
    //post("image_select %d\n", state);
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
        if (x->x_gui.x_glist == glist_getcanvas(x->x_gui.x_glist))
            gui_vmess("gui_image_toggle_border", "xxiiiiiii", glist, x,
                x1, y1, x2 - x1, y2 - y1,
                (x2 - x1) + (x->x_adj_img_width/2 - (x2 - x1)/2),
                (y2 - y1) + (x->x_adj_img_height/2 - (y2 - y1)/2),
                1
            );
        post("toggle border on %d %d", x2-x1, (x2 - x1) + (x->x_adj_img_width/2 - (x2 - x1)/2));
        gui_vmess("gui_gobj_select", "xx", glist_getcanvas(x->x_gui.x_glist), x);
    }
    else
    {
        if (x->x_gui.x_glist == glist_getcanvas(x->x_gui.x_glist))
            gui_vmess("gui_image_toggle_border", "xxiiiiiii", glist, x,
                0, 0, 0, 0, 0, 0, 0);
        gui_vmess("gui_gobj_deselect", "xx", glist_getcanvas(x->x_gui.x_glist), x);
    }
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
        image_drawme(x, glist, 1);
    else
        image_erase(x,glist);
}

/* can we use the normal text save function ?? */

static void image_save(t_gobj *z, t_binbuf *b)
{
    //post("image_save");
    t_image *x = (t_image *)z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];
    iemgui_save(&x->x_gui, srl, bflcol);
    post("image_save srl %s %s %s", srl[0]->s_name, srl[1]->s_name, srl[2]->s_name);
    binbuf_addv(b, "ssiissiiiisssiii", gensym("#X"), gensym("obj"),
                x->x_gui.x_obj.te_xpix, x->x_gui.x_obj.te_ypix,   
                atom_getsymbol(binbuf_getvec(x->x_gui.x_obj.te_binbuf)),
                x->x_fname, x->x_gop_spill, x->x_click, x->x_gui.x_w,
                x->x_gui.x_h, srl[0], srl[1], srl[2], x->x_adj_img_width,
                x->x_adj_img_height, x->x_constrain);
    binbuf_addv(b, ";");
}

static t_widgetbehavior image_widgetbehavior;

/*void image_size(t_image* x,t_floatarg w,t_floatarg h) {
     x->x_width = w;
     x->x_height = h;
     image_displace((t_gobj*)x, x->x_glist, 0.0, 0.0);
}*/

/*void image_color(t_image* x,t_symbol* col)
{
     //outlet_bang(x->x_obj.ob_outlet); only bang if there was a bang .. 
     //so color black does the same as bang, but doesn't forward the bang 

}*/

static int image_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix,
    int shift, int alt, int dbl, int doit)
{
    t_image *x = (t_image *)z;
    if (doit && x->x_click)
    {
        iemgui_out_bang(&x->x_gui, 0, 1);
    }
    // LATER: figure out how to do click on and click off
    // and provide a toggle button behavior instead
    /*{
        x->x_clicked = 1;
        outlet_float(x->x_obj.ob_outlet, x->x_clicked);
    }
    else if (x->x_clicked)
    {
        x->x_clicked = 0;
        outlet_float(x->x_obj.ob_outlet, x->x_clicked);
    }*/

    // this is how we catch mouse-up but only if the object calls glist_grab
    /*else if (dbl == -1)
    {

    }*/

    return(1);
}

static void image_click(t_image *x, t_float f)
{
    if (f == 0)
        x->x_click = 0;
    else if (f == 1)
        x->x_click = 1;
}

static void image_gop_spill(t_image* x, t_floatarg f)
{
    x->x_gop_spill = (f == 0 || f == 1 ? f : 0);
    image_displace((t_gobj*)x, x->x_gui.x_glist, 0.0, 0.0);
}

static void image_gop_spill_size(t_image* x, t_floatarg w, t_floatarg h)
{
    //post("image_gop_spill_size %d %d", (int)w, (int)h);
    // we need a size of at least 3 to have a meaningful
    // selection frame around the selection box
    if (!x->x_gop_spill)
    {
        post("warning: image is ignoring the gop_spill message "
             "since the gop_spill option is disabled");
        return;
    }

    int changed = 0;

    if ((int)w >= 3)
    {
        x->x_gui.x_w = ((int)w <= x->x_adj_img_width ? (int)w : x->x_adj_img_width);
        changed = 1;
    }

    if ((int)h >= 3)
    {
        x->x_gui.x_h = ((int)h <= x->x_adj_img_height ? (int)h : x->x_adj_img_height);
        changed = 1;
    }
    else if (changed)
    {
        x->x_gui.x_h = ((int)w <= x->x_adj_img_height ? (int)w : x->x_adj_img_height);
    }

    if (changed)
        image_displace((t_gobj*)x, x->x_gui.x_glist, 0.0, 0.0);
}

static void image_open(t_image* x, t_symbol *s, t_int argc, t_atom *argv)
{
    x->x_img_loaded = 0;
    x->x_fname = get_filename(argc, argv);
    x->x_img_width = 0;
    x->x_img_height = 0;
    t_symbol *fname = image_trytoopen(x);
    if (fname) {
        gui_vmess("gui_load_image", "xxs",
            glist_getcanvas(x->x_gui.x_glist), x, fname->s_name);
    }
    else
    {
        gui_vmess("gui_load_default_image", "xx",
            glist_getcanvas(x->x_gui.x_glist), x);
    }
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
    //image_vis((t_gobj *)x, x->x_glist, 0);
    //image_vis((t_gobj *)x, x->x_glist, 1);
}

static void image_imagesize_callback(t_image *x, t_float w, t_float h) {
    //fprintf(stderr,"received w %f h %f should %d spill %d\n", w, h, \
        gobj_shouldvis((t_gobj *)x, glist_getcanvas(x->x_glist)), x->x_gop_spill);
    x->x_img_loaded = 1;
    x->x_img_width = w;
    x->x_img_height = h;
    if (!x->x_adj_img_width || !x->x_adj_img_height)
    {
        x->x_adj_img_width = x->x_img_width;
        x->x_adj_img_height = x->x_img_height;
    }
    if (x->x_img_width + x->x_img_height == 0)
    {
        //invalid image
        if (strcmp("@pd_extra/ggee/empty_image.png", x->x_fname->s_name) != 0)
        {
            x->x_fname = gensym("@pd_extra/ggee/empty_image.png");
            image_trytoopen(x);
            post("callback invalid image");
            return;
        }
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
        //sys_vgui("catch {.x%x.c delete %xMT}\n", glist_getcanvas(x->x_glist), x);
        // reselect if we are on a toplevel canvas to adjust the selection rectangle, if necessary

        /* ico@vt.edu: this does not work for the spill mode, so we will have to
           draw the select box on demand
        gui_vmess("gui_image_draw_border", "xxiiii",
            glist_getcanvas(x->x_glist),
            x,
            0 - x->x_img_width/2,
            0 - x->x_img_height/2,
            x->x_img_width,
            x->x_img_height);
        */
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
        image_drawme(x, x->x_gui.x_glist, 0);

        if (glist_isselected(x->x_gui.x_glist, (t_gobj *)x) &&
            glist_getcanvas(x->x_gui.x_glist) == x->x_gui.x_glist)
        {
            image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 0);
            image_select((t_gobj *)x, glist_getcanvas(x->x_gui.x_glist), 1);
        }
        
        canvas_fixlinesfor(x->x_gui.x_glist,(t_text*) x);
    }
}

// resizemode is primarily used for the motionhook (constrain resize by neither
// axis or 0, by X axis or 1, or by Y axis or 2). Most other calls will use 0.
static void image_resize(t_image *x, t_glist *glist, int width, int height, int resizemode)
{
    int c, cb = 0; // constrain and change_border
    if (x->x_constrain == 2) { // custom
        c = x->x_gui.x_w / x->x_gui.x_h;
    } else if (x->x_constrain == 1) {
        c = x->x_img_width / x->x_img_height;
    }

    if (x->x_constrain > 0)
    {
        switch (resizemode)
        {
            case 0:
                x->x_adj_img_width = maxi(width, 3);
                x->x_adj_img_height = maxi(width * (1/c), 3);
                break;
            case 1:
                x->x_adj_img_width = maxi(width, 3);
                x->x_adj_img_height = maxi(width * (1/c), 3);
                break;
            case 2:
                x->x_adj_img_width = maxi(height * c, 3);
                x->x_adj_img_height = maxi(height, 3);
                break;
        }
    } else {
        x->x_adj_img_width = maxi(width, 3);
        x->x_adj_img_height = maxi(height, 3);
    }

    post("image_resize w=%d h=%d | final %d %d", width, height,
        x->x_adj_img_width, x->x_adj_img_height);

    gui_vmess("img_resize", "xxiiii",
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
        cb = 1;
    }
    if (x->x_gui.x_h > x->x_adj_img_height || !x->x_gop_spill)
    {
        x->x_gui.x_h = x->x_adj_img_height;
        cb = 1;
    }
    if (cb)
        gui_vmess("gui_image_update_border", "xxiiii", 
            glist_getcanvas(glist), x, x->x_gui.x_w, x->x_gui.x_h,
            x->x_gui.x_w + (x->x_adj_img_width/2 - x->x_gui.x_w/2),
            x->x_gui.x_h + (x->x_adj_img_height/2 - x->x_gui.x_w/2)
        );
}

static void image__clickhook(t_scalehandle *sh, int newstate)
{
    post("image__clickhook");
    t_image *x = (t_image *)(sh->h_master);
    if (newstate)
    {
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
    }
    if (sh->h_scale)
    {
        int x1, y1, x2, y2;
        image_getrect((t_gobj *)x, x->x_gui.x_glist, &x1, &y1, &x2, &y2);
        sh->h_adjust_x = sh->h_offset_x - (x2 + (x->x_adj_img_width/2 - x->x_gui.x_w/2));
        sh->h_adjust_y = sh->h_offset_y - (y2 + (x->x_adj_img_height/2 - x->x_gui.x_h/2));
        post("offset %d", sh->h_adjust_x);
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
        post("w=%d h=%d aimgw=%d aimgh=%d | mx=%d my=%d | adjx=%d adjy=%d",
            x->x_gui.x_w, x->x_gui.x_h, x->x_adj_img_width, x->x_adj_img_height,
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
        post("c img_resize %d %d", width, height);
        // this function is in g_all_guis.c and in nw.js does nothing
        // LATER: remove it altogether
        //scalehandle_drag_scale(sh);

        if (glist_isvisible(x->x_gui.x_glist))
        {
            //my_canvas_draw_move(x, x->x_gui.x_glist);
            //scalehandle_unclick_scale(sh);
            int resizemode = 0; // normal resize
            if (sh->h_constrain == CURSOR_EDITMODE_RESIZE_X)
                resizemode = 1;
            else if (sh->h_constrain == CURSOR_EDITMODE_RESIZE_Y)
                resizemode = 2;

            image_resize(x, x->x_gui.x_glist, width, height, resizemode);

            gui_vmess("gui_image_update_border", "xxiiiii", 
                x->x_gui.x_glist, x, x->x_gui.x_w, x->x_gui.x_h,
                x->x_gui.x_w + (x->x_adj_img_width/2 - x->x_gui.x_w/2),
                x->x_gui.x_h + (x->x_adj_img_height/2 - x->x_gui.x_w/2)
            );
            //scalehandle_unclick_scale(sh);
            // here instead of scalehandle_unclick_scale we only call
            // calls inside that function that we need, since reselecting
            // creates extra image
            iemgui_io_draw_move(x);
            canvas_fixlinesfor(x->x_gui.x_glist, (t_text *)x);
            canvas_getscroll(x->x_gui.x_glist);
        }

        int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            //properties_set_field_int(properties,"rng.min_ent",width);
            //properties_set_field_int(properties,"rng.max_ent",height);
        }
    }
}

static void image_properties(t_gobj *z, t_glist *owner)
{
    t_image *x = (t_image *)z;
    char /*buf[800],*/ *gfx_tag;

    /*
    sprintf(buf, "pdtk_iemgui_dialog %%s |nbx| \
        -------dimensions(digits)(pix):------- %d %d width: %d %d height: \
        -----------output-range:----------- %g min: %g max: %d \
        %d lin log %d %d log-height: %d {%s} {%s} {%s} %d %d %d %d %d %d %d\n",
        x->x_gui.x_w, 1, x->x_gui.x_h, 8, x->x_min, x->x_max,
        x->x_drawstyle,
        x->x_lin0_log1, x->x_gui.x_loadinit, -1,
        x->x_log_height,
        srl[0]->s_name, srl[1]->s_name, srl[2]->s_name,
        x->x_gui.x_ldx, x->x_gui.x_ldy,
        x->x_gui.x_font_style, x->x_gui.x_fontsize,
        0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_fcol,
        0xffffff & x->x_gui.x_lcol);
    //gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
    */
    gfx_tag = gfxstub_new2(&x->x_gui.x_obj.ob_pd, x);

    gui_start_vmess("gui_image_dialog", "s", gfx_tag);
    gui_start_array();
    gui_s("type");              gui_s("image");
    gui_s("file");              gui_s(x->x_fname->s_name); //TODO
    gui_s("width");             gui_i(x->x_gui.x_w);
    gui_s("height");            gui_i(x->x_gui.x_h);
    gui_s("visible_width");     gui_i(x->x_img_width);
    gui_s("visible_height");    gui_i(x->x_img_height);
    gui_s("gop_spill");         gui_i(x->x_gop_spill);  //RENAME
    gui_s("click");             gui_i(x->x_click);      //RENAME
    gui_s("lock_aspect_ratio"); gui_i(x->x_click);      //TODO
    gui_s("reset_size");        gui_i(x->x_img_width);  //TODO
    gui_s("reset_height");      gui_i(x->x_img_height); //TODO
    gui_s("send_symbol");       gui_s(x->x_gui.x_snd->s_name);
    gui_s("receive_symbol");    gui_s(x->x_gui.x_rcv->s_name);
    gui_end_array();
    gui_end_vmess();
}

static void image_dialog(t_image *x, t_symbol *s, int argc,
    t_atom *argv)
{
    //TODOLICIOUS
/*    if (atom_getintarg(19, argc, argv))
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
    x->x_gui.x_w = maxi(atom_getintarg(0, argc, argv),1);
    x->x_gui.x_h = maxi(atom_getintarg(1, argc, argv),8);
    double min = atom_getfloatarg(2, argc, argv);
    double max = atom_getfloatarg(3, argc, argv);
    x->x_lin0_log1 = !!atom_getintarg(4, argc, argv);
    x->x_log_height = maxi(atom_getintarg(6, argc, argv),10);
    x->x_drawstyle = (int)atom_getintarg(18, argc, argv);
    iemgui_dialog(&x->x_gui, argc, argv);
    x->x_numwidth = my_numbox_calc_fontwidth(x);

    my_numbox_check_minmax(x, min, max);
    // automatically adjust the number font size
    x->x_num_fontsize = maxi(x->x_gui.x_h * 0.9, IEM_FONT_MINSIZE);
    // normally, you'd do move+config, but here you have to do erase+new
    // because iemgui_draw_io does not support changes to x_drawstyle.
    iemgui_draw_erase(&x->x_gui);
    iemgui_draw_new(&x->x_gui);
    //iemgui_draw_move(&x->x_gui);
    //iemgui_draw_config(&x->x_gui);
    scalehandle_draw(&x->x_gui);
    if (x->x_gui.x_selected)
        iemgui_select((t_gobj *)x,x->x_gui.x_glist,1);
    //canvas_restore_original_position(x->x_gui.x_glist, (t_gobj *)x,"bogus",-1);
    scrollbar_update(x->x_gui.x_glist);
*/
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
    //x->x_clicked = 0;
    //x->x_selected = 0;
    // These are unused and only initialized, so that we can safely use
    // iemgui calls...
    x->x_gui.x_bcol = 0x00;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x00;

    // used for the last if statement since we decrement the argc below
    int n_args = argc;

    x->x_fname = get_filename(argc, argv);
    
    // this should take care of the sends and receives (we ignore label since
    // image does not use it--we may want to rethink this later...)
    if (argc >= 8)
        iemgui_new_getnames(&x->x_gui, 4, argv);
    else
        iemgui_new_getnames(&x->x_gui, 4, 0);


    if (strlen(x->x_fname->s_name) > 0)
    {
        //post("get_filename succeeded <%s> <%s>\n", 
        //    x->x_fname->s_name, atom_getsymbol(argv)->s_name);
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
    else if (n_args > 0) {
        // we are dealing with a legacy object and NOT a newly instantiated object
        // which has no arguments
        post("image: detected legacy patch... translating, so that when the "
             "patch is saved with the new version of pd-l2ork, the image "
             "object retains the same location...");
        x->x_legacy = 1;        
    }

    post("image_new srl %s %s %s", x->x_gui.x_snd->s_name, x->x_gui.x_rcv->s_name, x->x_gui.x_lab->s_name);
    x->x_gui.x_draw = (t_iemfunptr)image_drawme;
    // Create default receiver
    char buf[MAXPDSTRING];
    sprintf(buf, "#%zx", (t_uint)x);
    x->x_inlet = gensym(buf);
    pd_bind(&x->x_gui.x_obj.ob_pd, x->x_inlet);
    outlet_new(&x->x_gui.x_obj, &s_bang);
    if (x->x_gui.x_rcv != s_empty)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);    
    //outlet_new(&x->x_obj, &s_float);
    x->x_gui.x_handle = scalehandle_new(
        (t_object *)x,
        x->x_gui.x_glist,
        1,
        image__clickhook,
        image__motionhook);
    sprintf(buf, "_s%zx", (t_uint)x);
    pd_bind((t_pd *)x->x_gui.x_handle, x->x_gui.x_handle->h_bindsym = gensym(buf));
    // we set te_iemgui to denote 3rd-party iemgui-based object
    // 0 = non-iemgui
    // 1 = iemgui
    // 2 = 3rd-party iemgui-based that uses mycanvas resize hooks
    x->x_gui.x_obj.te_iemgui = 2;
    post("image_new w=%d h=%d adjw=%d adjh=%d", x->x_gui.x_w, x->x_gui.x_h,
        x->x_adj_img_width, x->x_adj_img_height);
    return (x);
}

void image_setup(void)
{
    image_class = class_new(gensym("image"),
                (t_newmethod)image_new, (t_method)image_free,
                sizeof(t_image),0, A_GIMME,0);
/*
    class_addmethod(image_class, (t_method)image_size, gensym("size"),
        A_FLOAT, A_FLOAT, 0);
    class_addmethod(image_class, (t_method)image_color, gensym("color"),
        A_SYMBOL, 0);
*/
    class_addmethod(image_class, (t_method)image_click, gensym("click"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_open, gensym("open"),
        A_GIMME, 0);
    class_addmethod(image_class, (t_method)image_gop_spill, gensym("gopspill"),
        A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_gop_spill_size, gensym("gopspill_size"),
        A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(image_class, (t_method)image_imagesize_callback,\
                     gensym("_imagesize"), A_DEFFLOAT, A_DEFFLOAT, 0);

    image_setwidget();
    class_setwidget(image_class, &image_widgetbehavior);
    class_setsavefn(image_class, image_save);
    class_setpropertiesfn(image_class, image_properties);
}
