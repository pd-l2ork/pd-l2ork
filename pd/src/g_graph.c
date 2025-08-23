/* Copyright (c) 1997-2001 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file deals with the behavior of glists as either "text objects" or
"graphs" inside another glist.  LATER move the inlet/outlet code of g_canvas.c 
to this file... */

#include <stdlib.h>
#include "m_pd.h"
#include "m_imp.h"
#include "g_canvas.h"
#include "g_all_guis.h" /* for canvas handle freeing */
#include "s_stuff.h"    /* for sys_hostfontsize */
#include <stdio.h>
#include <string.h>

//extern int array_joc;
//int garray_joc(t_garray *x);

/* ---------------------- forward definitions ----------------- */

static void graph_vis(t_gobj *gr, t_glist *unused_glist, int vis);
void graph_graphrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2);
static void graph_getrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2);
void graph_checkgop_rect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2);
void graph_gopspill(t_canvas *x, t_floatarg f);

extern t_template *template_findbydrawcommand(t_gobj *g);

extern int do_not_redraw;
int gop_redraw = 0;

/* -------------------- maintaining the list -------------------- */

void canvas_drawredrect(t_canvas *x, int doit);

int canvas_isgroup(t_canvas *x)
{
    //t_binbuf *b = x->gl_obj.te_binbuf;
    //if (!b)
    //{
    //    bug("canvas_isgroup");
    //    return 0;
    //}
    //t_atom *argv = binbuf_getvec(x->gl_obj.te_binbuf);
    //if (argv[0].a_type == A_SYMBOL &&
    //    argv[0].a_w.w_symbol == gensym("g"))
    //    return 1;
    //else
    //    return 0;
    if (x->gl_svg)
        return 1;
    else
        return 0;
}

extern t_template *canvas_findtemplate(t_canvas *c);
extern t_canvas *canvas_templatecanvas_forgroup(t_canvas *c);


/* ico@vt.edu 2020-08-24:
check if canvas consists of only scalars and returns 2. if the canvas only
has the last object as a non-scalar (e.g. a new object has just been created,
then we return 1, otherwise return 0. this is used to determine whether the
GOP redrect should be drawn inside the GOP-enabled toplevel window, depending
whether it only has scalars inside it or scalars with one newly created object
that is yet to be typed into and therefore properly instantiated */
int canvas_has_scalars_only(t_canvas *x)
{
    t_gobj *g = x->gl_list;
    int hasonlyscalars = 0;
    while (g)
    {
        //post("g...");
        hasonlyscalars = 2;
        if (pd_class(&g->g_pd) != scalar_class)
        {
            /*
            post("...scalar=NO %s %s", 
                (pd_class(&g->g_pd) == text_class ? "text_class" : "NOT_text_class"),
                (((t_text *)g)->te_type == T_TEXT) ? "T_TEXT" : "NOT_T_TEXT");
            */

            /* ico@vt.edu 2020-08-24:
            if we have one more object or the last object is not newly
            instantiated text object to distinguish between a comment and
            a text object that is yet to be instantiated we use:
               1) comment is text_class and its te_type is T_TEXT
               2) blank object one is typing into is text_class but is NOT T_TEXT
               3) instantiated object is something other than text_class (unless
                  it is a comment)
               4) object that has failed to create is same as blank object
            */
            if (g->g_next || (pd_class(&g->g_pd) != text_class || ((t_text *)g)->te_type == T_TEXT))
                hasonlyscalars = 0;
            // check if we are not the only object on the canvas, in which case we should still
            // return 0 since we have no scalars inside the canvas
            else if (g == x->gl_list && !g->g_next)
                hasonlyscalars = 0;
            else
                hasonlyscalars = 1;
            break;
        }
        //post("...scalar, comment, or uninitialized object=yes");
        g = g->g_next;
    }
    return(hasonlyscalars);
}

/* ico@vt.edu 2020-08-24: this draws or erases redrect on a gop window
   and is being refactored due to complex logic involving subpatches with
   scalars only that should not have a redrect until a non-scalar object
   has been instantiated (this does not include empty objects that are
   yet to be typed into, as this is one way how one can instantiate new
   scalar inside a subpatch)
*/
extern void canvas_draw_gop_resize_hooks(t_canvas* x);

void glist_update_redrect(t_glist *x)
{
    //post("glist_update_redrect");
    t_gobj *y = x->gl_list;
    while(y->g_next) y = y->g_next;

    if (x->gl_editor && x->gl_isgraph && !x->gl_goprect
        && pd_checkobject(&y->g_pd) && !canvas_has_scalars_only(x))
    {
        //post("...draw %d", canvas_has_scalars_only(x));
        x->gl_goprect = 1;
        canvas_drawredrect(x, 1);
        canvas_draw_gop_resize_hooks(x);
    }
    else if (canvas_has_scalars_only(x) && x->gl_goprect)
    {
        //post("...erase");
        x->gl_goprect = 0;
        canvas_drawredrect(x, 0);
        canvas_draw_gop_resize_hooks(x);    
    }
}

void glist_add(t_glist *x, t_gobj *y)
{
    //fprintf(stderr,"glist_add %zx %d\n", (t_uint)x, (x->gl_editor ? 1 : 0));    
    t_object *ob;
    y->g_next = 0;
    int index = 0;

    if (!x->gl_list) x->gl_list = y;
    else
    {
        t_gobj *y2;
        for (y2 = x->gl_list; y2->g_next; y2 = y2->g_next)
            index++;
        y2->g_next = y;
    }
    if (x->gl_editor && (ob = pd_checkobject(&y->g_pd)))
    {
        rtext_new(x, ob);
        //let's now set up create undo
        //glist_select(x, y);
        //canvas_setundo(x, canvas_undo_create,
        //    canvas_undo_set_create(x, index), "create");
        //glist_noselect(x);
    }
    glist_update_redrect(x);
    if (glist_isvisible(x))
        gobj_vis(y, x, 1);
    if (class_isdrawcommand(y->g_pd)) 
    {
        t_template *tmpl = template_findbydrawcommand(y);
        canvas_redrawallfortemplate(tmpl, 0);
    }
    if (pd_class(&y->g_pd) == canvas_class &&
        canvas_isgroup((t_canvas *)y))
    {
        t_canvas *templatecanvas =
            canvas_templatecanvas_forgroup((t_canvas *)y);
        t_template *tmpl = canvas_findtemplate(templatecanvas);
        canvas_redrawallfortemplate(tmpl, 0);
    }
}

    /* this is to protect against a hairy problem in which deleting
    a sub-canvas might delete an inlet on a box, after the box had
    been invisible-ized, so that we have to protect against redrawing it! */
int canvas_setdeleting(t_canvas *x, int flag)
{
    int ret = x->gl_isdeleting;
    x->gl_isdeleting = flag;
    return (ret);
}

    /* check if canvas has an array and return number of arrays
    this is used to prevent creation of new objects in an array window */
int canvas_hasarray(t_canvas *x)
{
    t_gobj *g = x->gl_list;
    int hasarray = 0;
    while (g)
    {
        if (pd_class(&g->g_pd) == garray_class) 
        {
            hasarray++;
        }
        g = g->g_next;
    }
    return(hasarray);
}

/* ico@vt.edu 2021-11-12:
   check if canvas has a toplevel scalar that requires resizing upon
   resizing of the window and return 2, otherwise return 0. we use this
   to inform front-end via gui_canvas_new if instead of updating
   scrollbars on resize, it needs to redraw canvas to properly scale
   all resizable scalars */
int canvas_hastoplevelscalar(t_canvas *x)
{
    int hastoplevelscalar = 0;
    t_gobj *g = x->gl_list;
    if (g && g->g_pd != garray_class)
    {
        while (g)
        {
            if (pd_class(&g->g_pd) == scalar_class)
            {
                hastoplevelscalar = 2;
                break;
            }
            g = g->g_next;
        }
    }
    return(hastoplevelscalar);
}

/* JMZ: emit a closebang message */
void canvas_closebang(t_canvas *x);

void canvas_dirtyclimb(t_canvas *x, int n, int draw_only);

    /* delete an object from a glist and free it */
void glist_delete(t_glist *x, t_gobj *y)
{
    //fprintf(stderr,"glist_delete y=%zx x=%zx glist_getcanvas=%zx\n", y, x, glist_getcanvas(x));
    if (x->gl_list)
    {
        //fprintf(stderr,"glist_delete YES\n");
        t_gobj *g;
        t_object *ob;
        t_template *tmpl = NULL;
        t_gotfn chkdsp = zgetfn(&y->g_pd, gensym("dsp"));
        t_canvas *canvas = glist_getcanvas(x);
        int drawcommand = class_isdrawcommand(y->g_pd);
        int wasdeleting;
        t_rtext *rt = NULL;
        int late_rtext_free = 0;

        if (pd_class(&y->g_pd) == canvas_class)
        {
            /* JMZ: send a closebang to the canvas */
            canvas_closebang((t_canvas *)y);
            /* and this little hack so drawing commands can tell
             if a [group] is deleting them (and thus suppress
             their own redraws) */
            ((t_canvas *)y)->gl_unloading = 1;
            /* if we are a group, let's call ourselves a drawcommand */
            if (((t_canvas *)y)->gl_svg)
                drawcommand = 1;

            if(((t_canvas *)y)->gl_dirty)
                canvas_dirtyclimb((t_canvas *)y, 0, 0);
        }
     
        wasdeleting = canvas_setdeleting(canvas, 1);
        if (x->gl_editor)
        {
            if (x->gl_editor->e_grab == y) x->gl_editor->e_grab = 0;
            if (glist_isselected(x, y)) glist_deselect(x, y);

                /* HACK -- we had phantom outlets not getting erased on the
                screen because the canvas_setdeleting() mechanism is too
                crude.  LATER carefully set up rules for when the rtexts
                should exist, so that they stay around until all the
                steps of becoming invisible are done.  In the meantime, just
                zap the inlets and outlets here... */
            if (pd_class(&y->g_pd) == canvas_class)
            {
                if (glist_isvisible(x))
                {
                    t_glist *gl = (t_glist *)y;
                    if (gl->gl_isgraph)
                    {
                        char tag[80];
                        //sprintf(tag, "graph%zx", (t_uint)gl);
                        //t_glist *yy = (t_glist *)y;
                        sprintf(tag, "%s",
                            rtext_gettag(glist_getrtext(x, &gl->gl_obj)));
                        glist_eraseiofor(x, &gl->gl_obj, tag);
                        text_eraseborder(&gl->gl_obj, x,
                            rtext_gettag(glist_getrtext(x, &gl->gl_obj)));
                    }
                    else
                    {
                        text_eraseborder(&gl->gl_obj, x,
                            rtext_gettag(glist_getrtext(x, &gl->gl_obj)));
                    }
                }
            }
        }
            /* if we're a drawing command, erase all scalars that
                       belong to our template, before deleting
               it; we'll redraw them once it's deleted below. */
        if (drawcommand)
        {
            tmpl = template_findbydrawcommand(y);
            if (!(canvas_isgroup(canvas) && canvas->gl_unloading))
            {
                canvas_redrawallfortemplate(tmpl, 2);
            }
        }
        if (glist_isvisible(canvas))
        {
            //fprintf(stderr,"...deleting %zx %zx\n", x, glist_getcanvas(x));
            gobj_vis(y, x, 0);
        }
        if (x->gl_editor && (ob = pd_checkobject(&y->g_pd)))
        {
            //rtext_new(x, ob);
            rt = glist_getrtext(x, ob);
            if (rt)
                late_rtext_free = 1;
        }
        if (x->gl_list == y)
        {
            if (y->g_next)
                x->gl_list = y->g_next;
            else
                x->gl_list = NULL;
        }
        else for (g = x->gl_list; g; g = g->g_next)
        {
            if (g->g_next == y)
            {
                if (y->g_next)
                    g->g_next = y->g_next;
                else g->g_next = NULL;
                break;
            }
        }
        gobj_delete(y, x);
        pd_free(&y->g_pd);
        if (chkdsp) canvas_update_dsp();
        if (drawcommand)
        {
            if (tmpl != NULL && !(canvas_isgroup(canvas) && canvas->gl_unloading))
            {
                canvas_redrawallfortemplate(tmpl, 1);
            }
        }
        canvas_setdeleting(canvas, wasdeleting);
        x->gl_valid = ++glist_valid;
        if (late_rtext_free)
        {
            //fprintf(stderr,"glist_delete late_rtext_free\n");
            rtext_free(rt);
        }

        if (x->gl_list) glist_update_redrect(x);
    }
}

    /* remove every object from a glist.  Experimental. */
void glist_clear(t_glist *x)
{
    t_gobj *y;
    int dspstate = 0, suspended = 0;
    t_symbol *dspsym = gensym("dsp");
    while (y = x->gl_list)
    {
            /* to avoid unnecessary DSP resorting, we suspend DSP
            only if we hit a patchable object. */
        if (!suspended && pd_checkobject(&y->g_pd) && zgetfn(&y->g_pd, dspsym))
        {
            dspstate = canvas_suspend_dsp();
            suspended = 1;
        }
            /* here's the real deletion. */
        glist_delete(x, y);
    }
    if (suspended)
        canvas_resume_dsp(dspstate);
}

void glist_retext(t_glist *glist, t_text *y)
{
        /* check that we have built rtexts yet.  LATER need a better test. */
    if (glist->gl_editor && glist->gl_editor->e_rtext)
    {
        t_rtext *rt = glist_getrtext(glist, y);
        if (rt)
            rtext_retext(rt);
    }
}

// 2020-10-05 ico@vt.edu:
// exclusive flag only applies to keyboard events (keyfn and keynameafn)
// as of right now I cannot think of a scenario where mouse motion should be
// exclusive to a single object.
// 2023-11-29 ico: here we also reference g_editor.c's global states of keys
// this allows us to update key states in case grab was exclusive, which
// may have caused some of the key releases to be lost to the general pd
// state, as they were only forwarded to the object with exclusive focus
extern int glob_shift;
extern int glob_ctrl;
extern int glob_alt;

void glist_grab(t_glist *x, t_gobj *y, t_glistmotionfn motionfn, t_glistkeyfn keyfn,
    t_glistkeynameafn keynameafn, int xpos, int ypos, int exclusive)
{
    //post("glist_grab exclusive=%d", exclusive);
    t_glist *x2 = glist_getcanvas(x);
    if (motionfn)
    {
        x2->gl_editor->e_onmotion = MA_PASSOUT;
        gui_vmess("gui_gobj_grabbed", "xiiiii", x, 1,
            exclusive, glob_ctrl, glob_alt, glob_shift);
    }
    else
    {
        x2->gl_editor->e_onmotion = 0;
        gui_vmess("gui_gobj_grabbed", "xiiiii", x, 0,
            exclusive, glob_ctrl, glob_alt, glob_shift);
    }
    x2->gl_editor->e_grab = y;
    x2->gl_editor->e_motionfn = motionfn;
    x2->gl_editor->e_keyfn = keyfn;
    x2->gl_editor->e_keynameafn = keynameafn;
    x2->gl_editor->e_xwas = xpos;
    x2->gl_editor->e_ywas = ypos;
    x2->gl_editor->exclusive = exclusive;
}

// change glist_grab exclusive flag separately
// from the rest of the glist_grab vars
// only do so if e_grab is not null
//  1 = enable exclusive focus
//  0 = disable exclusive focus
// -1 = delayed disable of the exclusive focus inside g_editor.c's canvas_key
int glist_grab_exclusive(t_glist *x, int exclusive)
{
    t_glist *x2 = glist_getcanvas(x);
    if (x2->gl_editor->e_grab)
    {
        if (exclusive < -1 || exclusive > 1) return(1);
        x2->gl_editor->exclusive = exclusive;
        return(0);
    }
    return(1);
}

// ico@vt.edu 2021-04-15: used for allowing message-induced focus on gatoms
// needs to disable motionfn enabled by the text_click->gatom_click call
// to disable accidental drag that may result from a scripted focus, but not
// from the actual clicked focus
void glist_grab_disable_motion(t_glist *x)
{
    if (x->gl_editor->e_grab && x->gl_editor->e_motionfn)
    {
        x->gl_editor->e_onmotion = 0;
        x->gl_editor->e_motionfn = 0;
    }
}

t_canvas *glist_getcanvas(t_glist *x)
{
    //fprintf(stderr,"glist_getcanvas\n");
    while (x->gl_owner && !x->gl_havewindow && x->gl_isgraph)
    {
            //fprintf(stderr,"x=%zx x->gl_owner=%d x->gl_havewindow=%d "
            //               "x->gl_isgraph=%d gobj_shouldvis=%d\n", 
            //    x, (x->gl_owner ? 1:0), x->gl_havewindow, x->gl_isgraph,
            //    gobj_shouldvis(&x->gl_gobj, x->gl_owner));
            x = x->gl_owner;
            //fprintf(stderr,"+\n");
    }
    return((t_canvas *)x);
}

static t_float gobj_getxforsort(t_gobj *g)
{
    if (pd_class(&g->g_pd) == scalar_class)
    {
        t_float x1, y1;
        scalar_getbasexy((t_scalar *)g, &x1, &y1);
        return(x1);
    }
    else return (0);
}

static t_gobj *glist_merge(t_glist *x, t_gobj *g1, t_gobj *g2)
{
    t_gobj *g = 0, *g9 = 0;
    t_float f1 = 0, f2 = 0;
    if (g1)
        f1 = gobj_getxforsort(g1);
    if (g2)
        f2 = gobj_getxforsort(g2);
    while (1)
    {
        if (g1)
        {
            if (g2)
            {
                if (f1 <= f2)
                    goto put1;
                else goto put2;
            }
            else goto put1;     
        }
        else if (g2)
            goto put2;
        else break;
    put1:
        if (g9)
            g9->g_next = g1, g9 = g1;
        else g9 = g = g1;
        if (g1 = g1->g_next)
            f1 = gobj_getxforsort(g1);
        g9->g_next = 0;
        continue;
    put2:
        if (g9)
            g9->g_next = g2, g9 = g2;
        else g9 = g = g2;
        if (g2 = g2->g_next)
            f2 = gobj_getxforsort(g2);
        g9->g_next = 0;
        continue;
    }
    return (g);
}

static t_gobj *glist_dosort(t_glist *x,
    t_gobj *g, int nitems)
{
    if (nitems < 2)
        return (g);
    else
    {
        int n1 = nitems/2, n2 = nitems - n1, i;
        t_gobj *g2, *g3;
        for (g2 = g, i = n1-1; i--; g2 = g2->g_next)
            ;
        g3 = g2->g_next;
        g2->g_next = 0;
        g = glist_dosort(x, g, n1);
        g3 = glist_dosort(x, g3, n2);
        return (glist_merge(x, g, g3));
    }
}

void glist_sort(t_glist *x)
{
    int nitems = 0, foo = 0;
    t_float lastx = -1e37;
    t_gobj *g;
    for (g = x->gl_list; g; g = g->g_next)
    {
        t_float x1 = gobj_getxforsort(g);
        if (x1 < lastx)
            foo = 1;
        lastx = x1;
        nitems++;
    }
    if (foo)
        x->gl_list = glist_dosort(x, x->gl_list, nitems);
}

/* --------------- inlets and outlets  ----------- */


t_inlet *canvas_addinlet(t_canvas *x, t_pd *who, t_symbol *s)
{
    //fprintf(stderr,"canvas_addinlet %d %zx %d\n", x->gl_loading, x->gl_owner, glist_isvisible(x->gl_owner));
    t_inlet *ip = inlet_new(&x->gl_obj, who, s, 0);
    if (!x->gl_loading && x->gl_owner && glist_isvisible(x->gl_owner))
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
    if (!x->gl_loading) canvas_resortinlets(x);
    return (ip);
}

void canvas_rminlet(t_canvas *x, t_inlet *ip)
{
    t_canvas *owner = x->gl_owner;
    int redraw = (owner && glist_isvisible(owner) && (!owner->gl_isdeleting)
        && glist_istoplevel(owner));
    
    if (owner) canvas_deletelinesforio(owner, &x->gl_obj, ip, 0);
    if (redraw)
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
    inlet_free(ip);
    if (redraw)
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
}

extern t_inlet *vinlet_getit(t_pd *x);
extern void obj_moveinletfirst(t_object *x, t_inlet *i);

void canvas_resortinlets(t_canvas *x)
{
    int ninlets = 0, i, j, xmax;
    t_gobj *y, **vec, **vp, **maxp;
    
    for (ninlets = 0, y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == vinlet_class) ninlets++;

    if (ninlets < 2  && !(canvas_isgroup(x))) return;
    vec = (t_gobj **)getbytes(ninlets * sizeof(*vec));
    
    for (y = x->gl_list, vp = vec; y; y = y->g_next)
        if (pd_class(&y->g_pd) == vinlet_class) *vp++ = y;
    
    for (i = ninlets; i--;)
    {
        t_inlet *ip;
        for (vp = vec, xmax = -0x7fffffff, maxp = 0, j = ninlets;
            j--; vp++)
        {
            int x1, y1, x2, y2;
            t_gobj *g = *vp;
            if (!g) continue;
            gobj_getrect(g, x, &x1, &y1, &x2, &y2);
            if (x1 > xmax) xmax = x1, maxp = vp;
        }
        if (!maxp) break;
        y = *maxp;
        *maxp = 0;
        ip = vinlet_getit(&y->g_pd);
        
        obj_moveinletfirst(&x->gl_obj, ip);
    }
    freebytes(vec, ninlets * sizeof(*vec));
    if (x->gl_owner &&
        glist_isvisible(x->gl_owner) && glist_isvisible(x) &&
        !x->gl_owner->gl_loading && !x->gl_loading)
    {
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
        //fprintf(stderr,"good place to fix redrawing of inlets "
        //               ".x%zx owner=.x%zx %d (parent)%d\n",
        //    x, x->gl_owner, x->gl_loading, x->gl_owner->gl_loading);

        /*
        t_object *ob = pd_checkobject(&y->g_pd);
        t_rtext *rt = glist_getrtext(x->gl_owner, (t_text *)&ob->ob_g);
        for (i = 0; i < ninlets; i++)
        {
            //sys_vgui(".x%x.c itemconfigure %si%d -fill %s -width 1\n",
            //    x, rtext_gettag(rt), i, 
            //    (obj_issignalinlet(ob, i) ?
            //        "$signal_nlet" : "$pd_colors_control_nlet)"));
            sprintf(xlet_tag, "%si%d", rtext_gettag(rt), i);
            char xlet_tag[MAXPDSTRING];
            gui_vmess("gui_gobj_configure_io", "xsiii",
                x,
                xlet_tag,
                0,
                obj_issignalinlet(ob, i),
                1);
        }
        */

        //glist_redraw(x);
        graph_vis(&x->gl_gobj, x->gl_owner, 0); 
        graph_vis(&x->gl_gobj, x->gl_owner, 1);
    }
}

t_outlet *canvas_addoutlet(t_canvas *x, t_pd *who, t_symbol *s)
{
    t_outlet *op = outlet_new(&x->gl_obj, s);
    if (!x->gl_loading && x->gl_owner && glist_isvisible(x->gl_owner))
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
    if (!x->gl_loading) canvas_resortoutlets(x);
    return (op);
}

void canvas_rmoutlet(t_canvas *x, t_outlet *op)
{
    t_canvas *owner = x->gl_owner;
    int redraw = (owner && glist_isvisible(owner) && (!owner->gl_isdeleting)
        && glist_istoplevel(owner));
    
    if (owner) canvas_deletelinesforio(owner, &x->gl_obj, 0, op);
    if (redraw)
        gobj_vis(&x->gl_gobj, x->gl_owner, 0);

    outlet_free(op);
    if (redraw)
    {
        gobj_vis(&x->gl_gobj, x->gl_owner, 1);
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
    }
}

extern t_outlet *voutlet_getit(t_pd *x);
extern void obj_moveoutletfirst(t_object *x, t_outlet *i);

void canvas_resortoutlets(t_canvas *x)
{
    int noutlets = 0, i, j, xmax;
    t_gobj *y, **vec, **vp, **maxp;
    
    for (noutlets = 0, y = x->gl_list; y; y = y->g_next)
        if (pd_class(&y->g_pd) == voutlet_class) noutlets++;

    if (noutlets < 2 && !(canvas_isgroup(x))) return;
    
    vec = (t_gobj **)getbytes(noutlets * sizeof(*vec));
    
    for (y = x->gl_list, vp = vec; y; y = y->g_next)
        if (pd_class(&y->g_pd) == voutlet_class) *vp++ = y;
    
    for (i = noutlets; i--;)
    {
        t_outlet *ip;
        for (vp = vec, xmax = -0x7fffffff, maxp = 0, j = noutlets;
            j--; vp++)
        {
            int x1, y1, x2, y2;
            t_gobj *g = *vp;
            if (!g) continue;
            gobj_getrect(g, x, &x1, &y1, &x2, &y2);
            if (x1 > xmax) xmax = x1, maxp = vp;
        }
        if (!maxp) break;
        y = *maxp;
        *maxp = 0;
        ip = voutlet_getit(&y->g_pd);
        
        obj_moveoutletfirst(&x->gl_obj, ip);
    }
    freebytes(vec, noutlets * sizeof(*vec));
    if (x->gl_owner &&
        glist_isvisible(x->gl_owner) && glist_isvisible(x) &&
        !x->gl_owner->gl_loading && !x->gl_loading)
    {
        canvas_fixlinesfor(x->gl_owner, &x->gl_obj);
        //fprintf(stderr,"good place to fix redrawing of outlets\n");
        //fprintf(stderr,"found it\n");
        //glist_redraw(x);
        graph_vis(&x->gl_gobj, x->gl_owner, 0); 
        graph_vis(&x->gl_gobj, x->gl_owner, 1);
    }
}

/* ----------calculating coordinates and controlling appearance --------- */


static void graph_bounds(t_glist *x, t_floatarg x1, t_floatarg y1,
    t_floatarg x2, t_floatarg y2)
{
    if (x1==x2 || y1==y2) {
        error("graph: empty bounds rectangle");
        x1 = y1 = 0;
        x2 = y2 = 1;
    }
    if (x->gl_x1!=x1 || x->gl_y1!=y1 || x->gl_x2!=x2 || x->gl_y2!=y2) {
        //printf("%f %f %f %f %f %f %f %f\n",x->gl_x1,x1,x->gl_y1,y1,x->gl_x2,x2,x->gl_y2,y2);
        x->gl_x1 = x1;
        x->gl_x2 = x2;
        x->gl_y1 = y1;
        x->gl_y2 = y2;
        if (!do_not_redraw)
            glist_redraw(x);
    }
}

static void graph_xticks(t_glist *x,
    t_floatarg point, t_floatarg inc, t_floatarg f)
{
    x->gl_xtick.k_point = point;
    x->gl_xtick.k_inc = inc;
    x->gl_xtick.k_lperb = f;
    glist_redraw(x);
}

static void graph_yticks(t_glist *x,
    t_floatarg point, t_floatarg inc, t_floatarg f)
{
    x->gl_ytick.k_point = point;
    x->gl_ytick.k_inc = inc;
    x->gl_ytick.k_lperb = f;
    glist_redraw(x);
}

static void graph_xlabel(t_glist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    if (argc < 1) error("graph_xlabel: no y value given");
    else
    {
        x->gl_xlabely = atom_getfloat(argv);
        argv++; argc--;
        x->gl_xlabel = (t_symbol **)t_resizebytes(x->gl_xlabel, 
            x->gl_nxlabels * sizeof (t_symbol *), argc * sizeof (t_symbol *));
        x->gl_nxlabels = argc;
        for (i = 0; i < argc; i++) x->gl_xlabel[i] = atom_gensym(&argv[i]);
    }
    glist_redraw(x);
}
    
static void graph_ylabel(t_glist *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    if (argc < 1) error("graph_ylabel: no x value given");
    else
    {
        x->gl_ylabelx = atom_getfloat(argv);
        argv++; argc--;
        x->gl_ylabel = (t_symbol **)t_resizebytes(x->gl_ylabel, 
            x->gl_nylabels * sizeof (t_symbol *), argc * sizeof (t_symbol *));
        x->gl_nylabels = argc;
        for (i = 0; i < argc; i++) x->gl_ylabel[i] = atom_gensym(&argv[i]);
    }
    glist_redraw(x);
}

/****** routines to convert pixels to X or Y value and vice versa ******/

    /* convert an x pixel value to an x coordinate value */
t_float glist_pixelstox(t_glist *x, t_float xpix)
{
        /* if we appear as a text box on parent, our range in our
        coordinates (x1, etc.) specifies the coordinate range
        of a one-pixel square at top left of the window. */
    if (!x->gl_isgraph)
        return (x->gl_x1 + (x->gl_x2 - x->gl_x1) * xpix);

        /* if we're a graph when shown on parent, but own our own
        window right now, our range in our coordinates (x1, etc.) is spread
        over the visible window size, given by screenx1, etc. */  
    else if (x->gl_isgraph && x->gl_havewindow)
        return (x->gl_x1 + (x->gl_x2 - x->gl_x1) * 
            (xpix) / (x->gl_screenx2 - x->gl_screenx1));

        /* otherwise, we appear in a graph within a parent glist,
         so get our screen rectangle on parent and transform. */
    else 
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstox");         
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (x->gl_x1 + (x->gl_x2 - x->gl_x1) * 
            (xpix - x1) / (x2 - x1));
    }
}

t_float glist_pixelstoy(t_glist *x, t_float ypix)
{
    if (!x->gl_isgraph)
        return (x->gl_y1 + (x->gl_y2 - x->gl_y1) * ypix);
    else if (x->gl_isgraph && x->gl_havewindow)
        return (x->gl_y1 + (x->gl_y2 - x->gl_y1) * 
                (ypix) / (x->gl_screeny2 - x->gl_screeny1));
    else 
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstoy");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (x->gl_y1 + (x->gl_y2 - x->gl_y1) * 
            (ypix - y1) / (y2 - y1));
    }
}

/* ico@vt.edu: used by the scalar_vis to adjust visual offset
   based on the graph drawing style, affects bar graph */
extern int garray_get_style(t_garray *x);

    /* convert an x coordinate value to an x pixel location in window */
t_float glist_xtopixels(t_glist *x, t_float xval)
{
    //post("glist_Xtopixels %zx %f isgraph=%d", (t_uint)x, xval, x->gl_isgraph);
    // ico@vt.edu: used to deal with the bar graph
    t_float plot_offset = 0;
    t_gobj *g = x->gl_list;

    if (!x->gl_isgraph)
        return ((xval - x->gl_x1) / (x->gl_x2 - x->gl_x1));
    else if (x->gl_isgraph && x->gl_havewindow)
    {
        /*
           TODO LATER: cycle through all garray classes in case there are multiple
           garrays inside the same array object? is this necessary?
        */
        /*
        if (g != NULL && g->g_pd == garray_class)
        {
            t_garray *g_a = (t_garray *)g;
            if (garray_get_style(g_a) == PLOTSTYLE_BARS)
                plot_offset = 10;
        }
        */
        //post("xtopixels xval=%f gl_x1=%f gl_x2=%f screenx1=%d screenx2=%d",
        //    xval, x->gl_x1, x->gl_x2, x->gl_screenx1, x->gl_screenx2);
        return (x->gl_screenx2 - x->gl_screenx1) *
            (xval - x->gl_x1 - plot_offset) / (x->gl_x2 - x->gl_x1);
    }
    else
    {
        /* ico@vt.edu: some really stupid code to compensate for the fact
           that the svg stroke feature adds unaccounted width to the bars
           TODO LATER: cycle through all garray classes in case there are multiple
           garrays inside the same array object? is this necessary?
        */
        if (g != NULL && g->g_pd == garray_class)
        {
            if (garray_get_style((t_garray *)g) == PLOTSTYLE_BARS)
                plot_offset = 1;
        }
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstox");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (x1 + (x2 - x1 - plot_offset) * (xval - x->gl_x1) / (x->gl_x2 - x->gl_x1) - plot_offset);
    }
}

/* ico@vt.edu 2022-11-29:
   provide width per scalar (used for arrays only to minimize errors
   with large arrays where x offset appears to play a role in producing
   a rounding error. only applies to scalars drawn inside GOP on
   the parent canvas */
t_float glist_norm_x_per_scalar(t_glist *x, t_float xval)
{
    /*
    post("glist_norm_x_per_scalar "
        "canvas=%zx xval=%f isgraph=%d haswindow=%d istoplevel=%d",
        (t_uint)x, xval, x->gl_isgraph, x->gl_havewindow, glist_istoplevel(x));
    */
    // we use this function only for scalars drawn inside GOP on a parent canvas
    // for other cases, we revert to the glist_xtopixels above.
    if (!x->gl_isgraph || x->gl_havewindow)
        return glist_xtopixels(x, xval);
    //post("...using norm for x");

    // ico@vt.edu: used to deal with the bar graph
    t_float plot_offset = 0;

    t_gobj *g = x->gl_list;

    /* ico@vt.edu: some really stupid code to compensate for the fact
       that the svg stroke feature adds unaccounted width to the bars
       TODO LATER: cycle through all garray classes in case there are multiple
       garrays inside the same array object? is this necessary?
    */
    if (g != NULL && g->g_pd == garray_class)
    {
        if (garray_get_style((t_garray *)g) == PLOTSTYLE_BARS)
            plot_offset = 1;
    }
    int x1, y1, x2, y2;
    if (!x->gl_owner)
        bug("glist_norm_x_per_scalar");
    graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
    // key difference here (when compared to the glist_xtopixels above
    // is the removal of "x1 + " at the beginning of the function,
    // which makes the calculation more accurate and prevents too long
    // or too short of array width
    return ((x2 - x1 - plot_offset) * (xval - x->gl_x1) / (x->gl_x2 - x->gl_x1) - plot_offset);
}

t_float glist_ytopixels(t_glist *x, t_float yval)
{
    // ico@vt.edu 2022-10-01: debugging in this function should be done
    // using fprintf, as otherwise post command could corrupt a mid-point
    // array message output, which will break the front-end
    //fprintf(stderr,"glist_Ytopixels %zx %f isgraph=%d", (t_uint)x, yval, x->gl_isgraph);
    t_float plot_offset = 0;
    t_gobj *g = x->gl_list;

    if (!x->gl_isgraph)
        return ((yval - x->gl_y1) / (x->gl_y2 - x->gl_y1));
    else if (x->gl_isgraph && x->gl_havewindow)
    {
        //fprintf(stderr, "glist_ytopixels toplevel graph\n");
        return (x->gl_screeny2 - x->gl_screeny1 - plot_offset) *
            (yval - x->gl_y1) / (x->gl_y2 - x->gl_y1);
    }
    else 
    {
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_pixelstoy");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        return (y1 + (y2 - y1 - plot_offset) * (yval - x->gl_y1) / (x->gl_y2 - x->gl_y1));
    }
}

/* ico@vt.edu 2022-11-29:
   provide width per scalar (used for arrays only to minimize errors
   with large arrays where y offset appears to play a role in producing
   a rounding error. only applies to scalars drawn inside GOP on
   the parent canvas */
t_float glist_norm_y_per_scalar(t_glist *x, t_float yval)
{
    // ico@vt.edu 2022-10-01: debugging in this function should be done
    // using fprintf, as otherwise post command could corrupt a mid-point
    // array message output, which will break the front-end
    //fprintf(stderr,"glist_Ytopixels %zx %f isgraph=%d", (t_uint)x, yval, x->gl_isgraph);
    // we use this function only for scalars drawn inside GOP on a parent canvas
    // for other cases, we revert to the glist_xtopixels above.
    if (!x->gl_isgraph || x->gl_havewindow)
        return glist_ytopixels(x, yval);
    else 
    {
        //post("...using norm for y");
        int x1, y1, x2, y2;
        if (!x->gl_owner)
            bug("glist_norm_y_per_scalar");
        graph_graphrect(&x->gl_gobj, x->gl_owner, &x1, &y1, &x2, &y2);
        // key difference here (when compared to the glist_xtopixels above
        // is the removal of "y1 + " at the beginning of the function,
        // which makes the calculation more accurate and prevents too long
        // or too short of array height
        return ((y2 - y1) * (yval - x->gl_y1) / (x->gl_y2 - x->gl_y1));
    }
}

    /* convert an X screen distance to an X coordinate increment.
      This is terribly inefficient;
      but probably not a big enough CPU hog to warrant optimizing. */
t_float glist_dpixtodx(t_glist *x, t_float dxpix)
{ 
    return (dxpix * (glist_pixelstox(x, 1) - glist_pixelstox(x, 0)));
}

t_float glist_dpixtody(t_glist *x, t_float dypix)
{
    return (dypix * (glist_pixelstoy(x, 1) - glist_pixelstoy(x, 0)));
}

    /* get the window location in pixels of a "text" object.  The
    object's x and y positions are in pixels when the glist they're
    in is toplevel.  Otherwise, if it's a new-style graph-on-parent
    (so gl_goprect is set) we use the offset into the framing subrectangle
    as an offset into the parent rectangle.  Finally, it might be an old,
    proportional-style GOP.  In this case we do a coordinate transformation. */
int text_xpix(t_text *x, t_glist *glist)
{
    int xpix = 0;
    if (glist->gl_havewindow || !glist->gl_isgraph)
        xpix = x->te_xpix;
    else if (glist->gl_goprect)
        xpix = glist_xtopixels(glist, glist->gl_x1) +
            x->te_xpix - glist->gl_xmargin;
    else xpix = (glist_xtopixels(glist, 
            glist->gl_x1 + (glist->gl_x2 - glist->gl_x1) * 
                x->te_xpix / (glist->gl_screenx2 - glist->gl_screenx1)));
    if (x->te_iemgui == 1)
        xpix += ((t_iemgui *)x)->legacy_x*sys_legacy;
    return(xpix);
}

int text_ypix(t_text *x, t_glist *glist)
{
    int ypix = 0; 
    if (glist->gl_havewindow || !glist->gl_isgraph)
        ypix = x->te_ypix; 
    else if (glist->gl_goprect)
        ypix = glist_ytopixels(glist, glist->gl_y1) +
            x->te_ypix - glist->gl_ymargin;
    else ypix = (glist_ytopixels(glist, 
            glist->gl_y1 + (glist->gl_y2 - glist->gl_y1) * 
                x->te_ypix / (glist->gl_screeny2 - glist->gl_screeny1)));
    if (x->te_iemgui == 1)
        ypix += ((t_iemgui *)x)->legacy_y*sys_legacy;
    return(ypix);
}

extern void canvas_updateconnection(t_canvas *x, int lx1, int ly1, int lx2, int ly2, t_int tag);

    /* redraw all the items in a glist.  We construe this to mean
    redrawing in its own window and on parent, as needed in each case.
    This is too conservative -- for instance, when you draw an "open"
    rectangle on the parent, you shouldn't have to redraw the window!  */
void glist_redraw(t_glist *x)
{
    //post("glist_redraw %zx", (t_uint)x);
    if (glist_isvisible(x))
    {
            /* LATER fix the graph_vis() code to handle both cases */
        if (glist_istoplevel(x) && x->gl_havewindow)
        {
            t_gobj *g;
            t_linetraverser t;
            t_outconnect *oc;
            for (g = x->gl_list; g; g = g->g_next)
            {
                gobj_vis(g, x, 0);
                gobj_vis(g, x, 1);
            }
                /* redraw all the lines */
            linetraverser_start(&t, x);
            while (oc = linetraverser_next(&t))
                canvas_updateconnection(glist_getcanvas(x), t.tr_lx1, t.tr_ly1, t.tr_lx2, t.tr_ly2, (t_int)oc);
                //sys_vgui(".x%zx.c coords l%zx %d %d %d %d\n",
                //    glist_getcanvas(x), oc,
                //        t.tr_lx1, t.tr_ly1, t.tr_lx2, t.tr_ly2);
            canvas_drawredrect(x, 0);
            if (x->gl_goprect)
            {
                //post("draw it");
                /* update gop rect size on toplevel in case font has
                changed and we are showing text */
                /*if (!x->gl_hidetext) {
                    int x1, y1, x2, y2;
                    graph_getrect((t_gobj *)x, x, &x1, &y1, &x2, &y2);
                    if (x2-x1 > x->gl_pixwidth) x->gl_pixwidth = x2-x1;
                    if (y2-y1 > x->gl_pixheight) x->gl_pixheight = y2-y1;
                }*/
                canvas_drawredrect(x, 1);
            }
        }
    }
    if (x->gl_owner && glist_isvisible(x->gl_owner))
    {
        graph_vis(&x->gl_gobj, x->gl_owner, 0); 
        graph_vis(&x->gl_gobj, x->gl_owner, 1);
    }
}

/* --------------------------- widget behavior  ------------------- */

int garray_getname(t_garray *x, t_symbol **namep);
t_symbol *garray_getlabelcolor(t_garray *x);
extern void gobj_vis_gethelpname(t_gobj *z, char *namebuf);

    /* Note that some code in here would also be useful for drawing
    graph decorations in toplevels... */
static void graph_vis(t_gobj *gr, t_glist *parent_glist, int vis)
{
    //post("graph_vis %d %zx", vis, (t_uint)gr);
    t_glist *x = (t_glist *)gr;
    //fprintf(stderr,"graph vis canvas=%zx gobj=%zx %d\n",
    //    (t_int)parent_glist, (t_int)gr, vis);
    //fprintf(stderr, "graph_vis gr=.x%zx parent_glist=.x%zx "
    //                "glist_getcanvas(x->gl_owner)=.x%zx vis=%d\n",
    //    (t_int)gr, (t_int)parent_glist,
    //    (t_int)glist_getcanvas(x->gl_owner), vis);  
    char tag[50];
    t_gobj *g;
    int x1, y1, x2, y2;
    t_rtext *rtext;
        /* ordinary subpatches: just act like a text object */
    if (!x->gl_isgraph)
    {
        text_widgetbehavior.w_visfn(gr, parent_glist, vis);
        return;
    }

        /* Sanity check */
    //post("parent_glist=%lx x->gl_obj=%lx", parent_glist, &x->gl_obj);
    rtext = glist_getrtext(parent_glist, &x->gl_obj);
    if (!rtext)
    {
        bug("graph_vis");
        return;
    }
    sprintf(tag, "%s", rtext_gettag(rtext));
    // weird exception
    //int exception = 0;
    //t_canvas* tgt = glist_getcanvas(x->gl_owner);
    //if (parent_glist->gl_owner && !parent_glist->gl_mapped &&
    //        parent_glist->gl_owner->gl_mapped) {
    //    tgt = parent_glist;
    //    exception = 1;
    //}
    //fprintf(stderr,"tgt=.x%zx %d\n", (t_uint)tgt, exception);

    if (vis & gobj_shouldvis(gr, parent_glist))
    {
        int xpix, ypix;
        xpix = text_xpix(&x->gl_obj, parent_glist);
        ypix = text_ypix(&x->gl_obj, parent_glist);

        char buf[FILENAME_MAX];
        rtext_getterminatedtext(rtext, &buf);

        char namebuf[FILENAME_MAX];
        gobj_vis_gethelpname((t_gobj *)gr, &namebuf);

        gui_vmess("gui_gobj_new", "xxxssiiiiss",
            glist_getcanvas(x->gl_owner),
            x,
            x->gl_owner,
            tag, "graph", xpix, ypix,
            (parent_glist == glist_getcanvas(x->gl_owner) ? 1 : 0),
            // ico@vt.edu 2022-09-26: changed last argument to 1
            // since this is a canvas object after all
            // need to test further, so far it checks out
            1,
            namebuf,
            buf
        );
        if (canvas_showtext(x))
            rtext_draw(glist_getrtext(parent_glist, &x->gl_obj));
    }

    //sprintf(tag, "%s", rtext_gettag(glist_getrtext(parent_glist, &x->gl_obj)));

    // need the rect to create the gobj, so this should perhaps be above the
    // conditional
    graph_getrect(gr, parent_glist, &x1, &y1, &x2, &y2);
    //fprintf(stderr,"%d %d %d %d\n", x1, y1, x2, y2);
    if (sys_legacy == 1)
    {
        //fprintf(stderr,"legacy  gop\n");
        y1 += 1;
        y2 += 1;
    }

    if (!vis)
        rtext_erase(glist_getrtext(parent_glist, &x->gl_obj));

    //sprintf(tag, "graph%zx", (t_uint)x);
    //fprintf(stderr, "gettag=%s, tag=graph%zx\n",
    //    rtext_gettag(glist_getrtext(parent_glist, &x->gl_obj)),(t_int)x);
    /* if we look like a graph but have been moved to a toplevel,
       just show the bounding rectangle */
    if (x->gl_havewindow)
    {
        if (vis && gobj_shouldvis(gr, parent_glist))
        {
            gui_vmess("gui_text_draw_border", "xssiiii",
                glist_getcanvas(x->gl_owner),
                tag,
                "none",
                0,
                x2 - x1,
                y2 - y1,
                1
            );
            // ico@vt.edu 2022-12-15: make sure that the dirty
            // rectangle is redrawn based on dirty and subdirty
            gui_vmess("gui_gobj_dirty", "xsi",
                glist_getcanvas(x->gl_owner), tag,
                (x->gl_dirty ? 1 : x->gl_subdirties ? 2 : 0));
            glist_noselect(x->gl_owner);
            gui_vmess("gui_graph_fill_border", "xsi",
                glist_getcanvas(x->gl_owner),
                tag);
			/* ico@vt.edu: do we need to redraw scalars here? */
			for (g = x->gl_list; g; g = g->g_next)
			{
				gop_redraw = 1;
				//fprintf(stderr,"drawing gop objects\n");
				if (g->g_pd == scalar_class)
					gobj_vis(g, x, 1);
				//fprintf(stderr,"done\n");
				gop_redraw = 0;
			}
        }
        else if (gobj_shouldvis(gr, parent_glist))
        {
            gui_vmess("gui_gobj_erase", "xs",
                glist_getcanvas(x->gl_owner),
                tag);
        }
        return;
    }
        /* otherwise draw (or erase) us as a graph inside another glist. */
    if (vis)
    {
        int i;
        t_float f;
        t_gobj *g;
        t_symbol *arrayname;
            /* draw a rectangle around the graph */
        char *ylabelanchor =
            (x->gl_ylabelx > 0.5*(x->gl_x1 + x->gl_x2) ? "w" : "e");
        char *xlabelanchor =
            (x->gl_xlabely > 0.5*(x->gl_y1 + x->gl_y2) ? "s" : "n");
        gui_vmess("gui_text_draw_border", "xssiiii",
            glist_getcanvas(x->gl_owner),
            tag,
            "none",
            0,
            x2 - x1,
            y2 - y1,
            (glist_getcanvas(x->gl_owner) == x->gl_owner ? 1 : 0));
            /* write garrays' names along the top. Since we can have multiple
               arrays inside a single graph, we want to send all the fun
               label data to the GUI in one message. That way the GUI can do
               fun stuff like selectively displaying a color key if we have
               multiple arrays with different colors. */
        gui_start_vmess("gui_graph_label", "xsiiiii",
            glist_getcanvas(x),
            tag,
            sys_hostfontsize(glist_getfont(x)),
            sys_fontheight(glist_getfont(x)),
            glist_isselected(x, gr),
            sys_legacy,
            (glist_getcanvas(x->gl_owner) == x->gl_owner ? 1 : 0)
        );

            /* Now start an array to hold each array of label info */
        gui_start_array();

        for (i = 0, g = x->gl_list; g; g = g->g_next, i++)
        {
            //fprintf(stderr,".\n");
            //if (g->g_pd == garray_class)
            //    fprintf(stderr,"garray_getname=%d\n",garray_getname((t_garray *)g, &arrayname));
            if (g->g_pd == garray_class &&
                !garray_getname((t_garray *)g, &arrayname))
            {
                t_symbol *labelcolor = garray_getlabelcolor((t_garray *)g);
                    /* Now send an attribute array with the label data */
                gui_start_array();
                gui_s("label"); gui_s(arrayname->s_name);
                gui_s("color"); gui_s(labelcolor->s_name);
                gui_end_array();
            }
        }
            /* Finally, end the final array as well as the call to the GUI */
        gui_end_array();
        gui_end_vmess();

            /* draw ticks on horizontal borders.  If lperb field is
            zero, this is disabled. */
        if (x->gl_xtick.k_lperb)
        {
            t_float upix, lpix;
            if (y2 < y1)
                upix = y1, lpix = y2;
                        else
                upix = y2, lpix = y1;

            for (i = (int)((x->gl_x1 - x->gl_xtick.k_point) / x->gl_xtick.k_inc) + (x->gl_xtick.k_point<x->gl_x1);
                 i <= (int)((x->gl_x2 - x->gl_xtick.k_point) / x->gl_xtick.k_inc) - (x->gl_xtick.k_point>x->gl_x2);
                 i++)
            {
                t_float fx = x->gl_xtick.k_point + (i * x->gl_xtick.k_inc);
                int tickpix = (i % x->gl_xtick.k_lperb ? 2 : 4);
                int ix = (int)glist_xtopixels(x, x->gl_xtick.k_point + (i * x->gl_xtick.k_inc));
                gui_vmess("gui_graph_vtick", "xsiiiiii",
                    glist_getcanvas(x->gl_owner),
                    tag,
                    ix,
                    (int)upix,
                    (int)lpix,
                    (int)tickpix,
                    x1,
                    y1);
            }
        }

            /* draw ticks in vertical borders*/
        if (x->gl_ytick.k_lperb)
        {
            t_float ubound, lbound;
            if (x->gl_y2 < x->gl_y1)
                ubound = x->gl_y1, lbound = x->gl_y2;
            else
                ubound = x->gl_y2, lbound = x->gl_y1;
            for (i =  (int)((lbound - x->gl_ytick.k_point) / x->gl_ytick.k_inc) + (x->gl_ytick.k_point<lbound);
                 i <= (int)((ubound - x->gl_ytick.k_point) / x->gl_ytick.k_inc) - (x->gl_ytick.k_point>ubound);
                 i++)
            {
                t_float fy = x->gl_ytick.k_point + (i * x->gl_ytick.k_inc);
                int tickpix = (i % x->gl_ytick.k_lperb ? 2 : 4);
                int iy = (int)glist_ytopixels(x, fy);
                gui_vmess("gui_graph_htick", "xsiiiiii",
                    glist_getcanvas(x->gl_owner),
                    tag,
                    iy,
                    x1,
                    x2,
                    (int)tickpix,
                    x1,
                    y1);
            }
        }
            /* draw x labels */
        for (i = 0; i < x->gl_nxlabels; i++)
        {
            gui_vmess("gui_graph_tick_label", "xsiissisiis",
                glist_getcanvas(x),
                tag,
                (int)glist_xtopixels(x, atof(x->gl_xlabel[i]->s_name)),
                // ico@vt.edu 2022-06-13: added +7 offset for the x label
                // because it was otherwise overlapping with the array
                // and potentially unreadable
                (int)glist_ytopixels(x, x->gl_xlabely)+7,
                x->gl_xlabel[i]->s_name,
                sys_font, 
                sys_hostfontsize(glist_getfont(x)),
                sys_fontweight,
                x1,
                y1,
                xlabelanchor);
        }

            /* draw y labels */
        for (i = 0; i < x->gl_nylabels; i++)
        {
            gui_vmess("gui_graph_tick_label", "xsiissisiis",
                glist_getcanvas(x),
                tag,
                (int)glist_xtopixels(x, x->gl_ylabelx),
                (int)glist_ytopixels(x, atof(x->gl_ylabel[i]->s_name)),
                x->gl_ylabel[i]->s_name,
                sys_font, 
                sys_hostfontsize(glist_getfont(x)),
                sys_fontweight,
                x1,
                y1,
                ylabelanchor);
        }

            /* draw contents of graph as glist */
        if (!x->gl_disablecontentredraw)
        {
            for (g = x->gl_list; g; g = g->g_next)
            {
                gop_redraw = 1;
                //fprintf(stderr,"drawing gop objects\n");
                //post("graph_vis drawing object %lx...", g);
                gobj_vis(g, x, 1);
                //fprintf(stderr,"done\n");
                gop_redraw = 0;
            }
        }
        // ico@vt.edu 2022-11-09: if we have gopspill option enabled
        // allow for object to be drawn outside the boundaries
        // here we lower the border line, so that it does not cover
        // spilled objects. this is achieved by adding the
        // "overflow: visible" css option
        //post("graph_vis gopspill as float %f", (t_floatarg)x->gl_gopspill);
        // this currently does not work because the object is not
        // mapped yet, which is a prerequisite for the graph_gopspill
        // function below, so we have to issue a draw command here
        // directly
        //graph_gopspill(x, (t_floatarg)x->gl_gopspill);
        if (x->gl_gopspill)
            gui_vmess("gui_graph_gopspill", "xsi",
                glist_getcanvas(x->gl_owner),
                tag,
                x->gl_gopspill
            );
        glist_drawiofor(parent_glist, &x->gl_obj, 1,
            tag, x1, y1, x2, y2);
        // ico@vt.edu 2022-09-28: reattach labels dirty and subdirty
        // upon redrawing, so that the GOP border is colored accordingly.
        //post("===========is gop dirty? %zx %d", x, x->gl_dirty);
        canvas_dirtyclimb(x, x->gl_dirty, 1);
        /* reselect it upon redrawing if it was selected before */
        if (glist_isselected(parent_glist, gr))
            gobj_select(gr, parent_glist, 1);
        // here we check for changes in scrollbar because of legacy
        // objects that can fall outside gop window, e.g. scalars
        canvas_getscroll(glist_getcanvas(x->gl_owner));
        //fprintf(stderr,"******************graph_vis SELECT\n");
    }
    else
    {
        glist_eraseiofor(parent_glist, &x->gl_obj, tag);
        if (!x->gl_disablecontentredraw)
        {
            for (g = x->gl_list; g; g = g->g_next)
                gobj_vis(g, x, 0);
        }

        gui_vmess("gui_gobj_erase", "xs",
            glist_getcanvas(x->gl_owner),
            tag);

        // here we check for changes in scrollbar because of legacy
        // objects that can fall outside gop window, e.g. scalars
        canvas_getscroll(glist_getcanvas(x->gl_owner));
    }
}

void graph_gopspill(t_canvas *x, t_floatarg f)
{
    // don't allow this for graph canvases that have arrays in them
    if (canvas_hasarray(x))
        return;

    //post("graph_gopspill %f", f);
    if (x->gl_gopspill == (int)f || ((int)f != 0 && (int)f != 1))
        return;

    x->gl_gopspill = (int)f;

    if (x->gl_mapped && x->gl_isgraph && !glist_getcanvas(x) != x)
    {
        char tag[50];
        t_rtext *rtext;

        rtext = glist_getrtext(x->gl_owner, &x->gl_obj);
        if (!rtext)
        {
            bug("canvas_gopspill");
            return;
        }
        sprintf(tag, "%s", rtext_gettag(rtext));

        gui_vmess("gui_graph_gopspill", "xsi",
            glist_getcanvas(x),
            tag,
            x->gl_gopspill
        );
    }
}

    /* get the graph's rectangle, not counting extra swelling for controls
    to keep them inside the graph.  This is the "logical" pixel size. */

void graph_graphrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_glist *x = (t_glist *)z;
    int x1 = text_xpix(&x->gl_obj, glist);
    int y1 = text_ypix(&x->gl_obj, glist);
    int x2, y2;
    x2 = x1 + x->gl_pixwidth;
    y2 = y1 + x->gl_pixheight;

    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

    /* check if the gop size needs to change due to gop's text
    in case hidetext is not enabled */
void graph_checkgop_rect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{

    //fprintf(stderr,"graph_checkgop_rect\n");
    t_glist *x = (t_glist *)z;
    t_gobj *g;
    int fw = sys_fontwidth(x->gl_font);
    int fh = sys_fontheight(x->gl_font);

    if (!x->gl_hidetext)
    {
        int x21, y21, x22, y22;
        text_widgetbehavior.w_getrectfn(z, glist, &x21, &y21, &x22, &y22);
        if (x22 > *xp2)
            *xp2 = x22;
        if (y22 > *yp2) 
            *yp2 = y22;
        // WARNING: ugly hack trying to replicate rtext_senditup
        // if we have no parent. Later consider fixing hardwired values
        int tcols = strlen(x->gl_name->s_name) - 3;
        int th = fh + fh * (tcols/60) + 4;
        if (tcols > 60) tcols = 60;
        int tw = fw * tcols + 4;
        if (tw + *xp1 > *xp2)
            *xp2 = tw + *xp1;
        if (th + *yp1 > *yp2)
            *yp2 = th + *yp1;
    }

    // check if the gop has array members and if so,
    // make its minimum size based on array name's size
    t_symbol *arrayname;
    int cols_tmp = 0;
    int arrayname_cols = 0;
    int arrayname_rows = 0;
    for (g = x->gl_list; g; g = g->g_next)
    {
        if (pd_class(&g->g_pd) == garray_class &&
            !garray_getname((t_garray *)g, &arrayname))
        {
            arrayname_rows += 1;
            cols_tmp = strlen(arrayname->s_name);
            if(cols_tmp > arrayname_cols) arrayname_cols = cols_tmp;
        }
    }
    if (arrayname_rows)
    {
        int fontwidth = sys_fontwidth(x->gl_font);
        int fontheight = sys_fontheight(x->gl_font);
        if ((arrayname_rows * fontheight + 4) > (*yp2 - *yp1))
            *yp2 = *yp1 + (arrayname_rows * fontheight + 4);
        if ((arrayname_cols * fontwidth + 5) > (*xp2 - *xp1))
            *xp2 = *xp1 + (arrayname_cols * fontwidth + 5);
    }

    // failsafe where we cannot have a gop that is smaller than 1x1 pixels
    // regardless whether the text is hidden
    int in = obj_ninlets(pd_checkobject(&z->g_pd));
    int out = obj_noutlets(pd_checkobject(&z->g_pd));
    int max_xlets = in >= out ? in : out;
    int minhsize = (max_xlets * IOWIDTH) +
        ((max_xlets > 1 ? max_xlets - 1 : 0) * IOWIDTH);
    if (minhsize < SCALE_GOP_MINWIDTH) minhsize = SCALE_GOP_MINWIDTH;
    int minvsize = ((in > 0 ? 1 : 0) + (out > 0 ? 1 : 0)) * 2;
    if (minvsize < SCALE_GOP_MINHEIGHT) minvsize = SCALE_GOP_MINHEIGHT;
    if (*xp2 < *xp1+minhsize) *xp2 = *xp1+minhsize;
    if (*yp2 < *yp1+minvsize) *yp2 = *yp1+minvsize;
}

    /* get the rectangle, enlarged to contain all the "contents" --
    meaning their formal bounds rectangles. */
static void graph_getrect(t_gobj *z, t_glist *glist,
    int *xp1, int *yp1, int *xp2, int *yp2)
{
    int x1 = 0x7fffffff, y1 = 0x7fffffff, x2 = -0x7fffffff, y2 = -0x7fffffff;
    t_glist *x = (t_glist *)z;
    //fprintf(stderr,"graph_getrect %d\n", x->gl_isgraph);
    if (x->gl_isgraph)
    {
        int hadwindow;
        t_gobj *g;
        int x21, y21, x22, y22;
        graph_graphrect(z, glist, &x1, &y1, &x2, &y2);
        //fprintf(stderr,"%d %d %d %d\n", x1, y1, x2, y2);

        if (canvas_showtext(x))
        {
            text_widgetbehavior.w_getrectfn(z, glist, &x21, &y21, &x22, &y22);
            if (x22 > x2) 
                x2 = x22;
            if (y22 > y2) 
                y2 = y22;
            //fprintf(stderr,"canvas_showtext %d %d %d %d\n", x1, y1, x2, y2);
        }
        if (!x->gl_goprect)
        {
            /* expand the rectangle to fit in text objects; this applies only
            to the old (0.37) graph-on-parent behavior. */
            /* lie about whether we have our own window to affect gobj_getrect
            calls below.  */
            hadwindow = x->gl_havewindow;
            x->gl_havewindow = 0;
            for (g = x->gl_list; g; g = g->g_next)
            {
                    /* don't do this for arrays, just let them hang outside the
                    box. */
                if (pd_class(&g->g_pd) == garray_class ||
                    pd_class(&g->g_pd) == scalar_class)
                    continue;
                gobj_getrect(g, x, &x21, &y21, &x22, &y22);
                if (x22 > x2) 
                    x2 = x22;
                if (y22 > y2) 
                    y2 = y22;
            }
            x->gl_havewindow = hadwindow;
        }

        //fprintf(stderr,"%d %d %d %d\n", x1, y1, x2, y2);

        // check if the text is not hidden and if so use that as the
        // limit of the gop's size (we check for hidden flag inside
        // the function we point to)
        graph_checkgop_rect(z, glist, &x1, &y1, &x2, &y2);

        /* fix visibility of edge items for garrays */
        /*
        int has_garray = 0;
        for (g = x->gl_list; g; g = g->g_next)
        {
            if (pd_class(&g->g_pd) == garray_class)
            {
                has_garray = 1;
            }
        }
        if (has_garray) {
            x1 -= 1;
            y1 -= 2;
            //x2 += 1;
            y2 += 1;
        }*/
    }
    else text_widgetbehavior.w_getrectfn(z, glist, &x1, &y1, &x2, &y2);

    if (sys_legacy == 1)
    {
        //fprintf(stderr,"legacy  gop\n");
        y1 += 1;
        y2 += 1;
    }
    //fprintf(stderr,"    post %d %d %d %d\n", x1, y1, x2, y2); 

    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

static void graph_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    //fprintf(stderr,"graph_displace %d %d\n", dx, dy);
    t_glist *x = (t_glist *)z;
    if (!x->gl_isgraph)
        text_widgetbehavior.w_displacefn(z, glist, dx, dy);
    else
    {
        x->gl_obj.te_xpix += dx;
        x->gl_obj.te_ypix += dy;
        /*char tag[80];
        sprintf(tag, "%s",
            rtext_gettag(
                glist_getrtext((x->gl_owner ? x->gl_owner: x), &x->gl_obj)));
        sys_vgui(".x%zx.c move %s %d %d\n",
            glist_getcanvas(x->gl_owner), tag, dx, dy);
        sys_vgui(".x%zx.c move %sR %d %d\n",
            glist_getcanvas(x->gl_owner), tag, dx, dy);*/
        if (!do_not_redraw)
        {
            //fprintf(stderr,"graph_displace redraw\n");
            glist_redraw(glist_getcanvas(glist));
            //gobj_select(z, glist, 1);
            canvas_fixlinesfor(glist_getcanvas(glist), &x->gl_obj);
        }
    }
}

extern int old_displace; //from g_editor.c for legacy drawing

static void graph_displace_scalars(t_glist *x, t_glist *glist, int dx, int dy)
{
    t_gobj *g;
    for (g = x->gl_list; g; g = g->g_next)
    {
        if (pd_class((t_pd *)g) == scalar_class &&
            g->g_pd->c_wb->w_displacefnwtag != NULL)
        {
            (*(g->g_pd->c_wb->w_displacefnwtag))(g, glist, dx, dy);
        }
        else if (pd_class(&g->g_pd) == canvas_class &&
                 ((t_glist *)g)->gl_isgraph)
        {
            graph_displace_scalars((t_glist *)g, glist, dx, dy); 
        }
    }
}

static void graph_displace_withtag(t_gobj *z, t_glist *glist, int dx, int dy)
{
    //fprintf(stderr,"graph_displace_withtag %d %d\n", dx, dy);
    t_glist *x = (t_glist *)z;
    if (!x->gl_isgraph)
        text_widgetbehavior.w_displacefnwtag(z, glist, dx, dy);
    else
    {
        // first check for legacy objects that don't offer displacefnwtag
        // and fallback on the old way of doing things
        t_gobj *g;
        /* special case for scalars, which have a group for
           the transform matrix */
        graph_displace_scalars(x, glist, dx, dy); 
        for (g = x->gl_list; g; g = g->g_next)
        {
            //fprintf(stderr,"shouldvis %d %d\n",
            //    gobj_shouldvis(g, glist), gobj_shouldvis(g, x));
            if (g && gobj_shouldvis(g, x) &&
                g->g_pd->c_wb->w_displacefnwtag == NULL &&
                pd_class((t_pd *)g) != garray_class)
            {
                //fprintf(stderr,"old way\n");
                old_displace = 1;
                graph_displace(z, glist, dx, dy);
                return;
            }
        }
        // else we do things the new and more elegant way
        //fprintf(stderr,"new way\n");

        
        x->gl_obj.te_xpix += dx;
        x->gl_obj.te_ypix += dy;
        canvas_fixlinesfor(glist_getcanvas(glist), &x->gl_obj);
    }
}

static void graph_select(t_gobj *z, t_glist *glist, int state)
{
    //fprintf(stderr,"graph_select .x%zx .x%zx %d...\n",
    //    (t_int)z, (t_int)glist, state);
    t_glist *x = (t_glist *)z;
    if (!x->gl_isgraph)
        text_widgetbehavior.w_selectfn(z, glist, state);
    else //if(glist_istoplevel(glist))
    {
        //fprintf(stderr,"...yes\n");
        //fprintf(stderr,"%zx %zx %zx\n", glist_getcanvas(glist), glist, x);
        t_rtext *y = glist_getrtext(glist, &x->gl_obj);
        if (canvas_showtext(x))
        {
            rtext_select(y, state);
        }

        t_glist *canvas;
        if (!glist_istoplevel(glist))
        {
            canvas = glist_getcanvas(glist);
        }
        else
        {
            canvas = glist;
        }
        if (glist_isvisible(glist) &&
                (glist_istoplevel(glist) ||
                 gobj_shouldvis(z, glist)))
        {
            if (state)
                gui_vmess("gui_gobj_select", "xs",
                    canvas, rtext_gettag(y));
            else
                gui_vmess("gui_gobj_deselect", "xs",
                    canvas, rtext_gettag(y));
        }

        // ico@vt.edu 20200914: Not needed anymore for the new
        // implementation of GOP gobjects being drawn inside the GOP group
        /*t_gobj *g;
        //fprintf(stderr,"graph_select\n");
        if (x->gl_list && !glist_istoplevel(x))
        {
            for (g = x->gl_list; g; g = g->g_next)
            {
                //fprintf(stderr,"shouldvis %d\n",gobj_shouldvis(g, x));
                if ((g && gobj_shouldvis(g, x) &&
                    (g->g_pd->c_wb->w_displacefnwtag != NULL) ||
                    (g && pd_class((t_pd *)g) == garray_class)))
                {
                    gobj_select(g, x, state);
                }
            }
        }
        */
        // Don't yet understand the purpose of this call, so not deleting
        // it just yet...
        //sys_vgui("pdtk_select_all_gop_widgets .x%zx %s %d\n",
        //    canvas, rtext_gettag(glist_getrtext(glist, &x->gl_obj)), state);
    }
}

static void graph_activate(t_gobj *z, t_glist *glist, int state)
{
    t_glist *x = (t_glist *)z;
    if (canvas_showtext(x))
        text_widgetbehavior.w_activatefn(z, glist, state);
}

#if 0
static void graph_delete(t_gobj *z, t_glist *glist)
{
    t_glist *x = (t_glist *)z;
    if (!x->gl_isgraph)
        text_widgetbehavior.w_deletefn(z, glist);
    else
    {
        t_gobj *y;
        while (y = x->gl_list) glist_delete(x, y);
#if 0       /* I think this was just wrong. */
        if (glist_isvisible(x))
            sys_vgui(".x%zx.c delete graph%zx\n", glist_getcanvas(glist), x);
#endif
    }
}
#endif

static void graph_delete(t_gobj *z, t_glist *glist)
{
    //fprintf(stderr,"graph_delete\n");
    t_glist *x = (t_glist *)z;
    t_gobj *y;
    text_widgetbehavior.w_deletefn(z, glist);
    while (y = x->gl_list)
    {
        glist_delete(x, y);
    }
    if (glist_istoplevel(glist) && glist_isvisible(glist))
        canvas_getscroll(glist);
}

extern t_class *my_canvas_class; // for ignoring runtime clicks
// found in g_editor.c LATER: add it to g_canvas.h?
extern void canvas_passthrough_click(t_canvas *x, t_int xpos, t_int ypos, t_int click);

static int graph_click(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    //post("graph_click %d", doit);
    t_glist *x = (t_glist *)z;
    t_gobj *y, *clickme = NULL, *got_garray = NULL;
    int clickreturned = 0;
    //int tmpclickreturned = 0;
    if (!x->gl_isgraph)
        return (text_widgetbehavior.w_clickfn(z, glist,
            xpix, ypix, shift, alt, dbl, doit));
    else if (x->gl_havewindow)
        return (0);
    else if (x->gl_list)
    {
        int numgobj = 0, numptgobj = 0, i;
        // find out how many objects there are in the gl_list
        // we use this to allocate an array of pointers, so that
        // we can distribute clicks in order from topmost to
        // bottommost (or stop at a non-passthrough object)
        // see mode 3 for iemgui in m_pd.h and ggee/image
        for (y = x->gl_list; y; y = y->g_next)
            numgobj++;
        t_gobj *ypt[numgobj];
        int x1, y1, x2, y2, passthroughclick = 0;
        t_object *ob;

        for (y = x->gl_list; y; y = y->g_next)
        {
            /*
            ico@vt.edu 2021-11-12: this aspect deals with trying to grab
            points of a graph when they are drawn as a GOP on a parent
            patch. for handling points on a toplevel window, see g_editor.c
            canvas_doclick. we also use this to update the
            array_joc global variable. we can do this outside the
            passthrough call since pd-l2ork does not allow mixing
            of arrays and canvas objects.

            //Original implementation:
            if(pd_class(&y->g_pd) == garray_class &&
               !y->g_next &&
               (array_joc = garray_joc((t_garray *)y)) &&
               (clickreturned =
                   gobj_click(y, x, xpix, ypix, shift, alt, 0, doit)))
            {
                break;
            }
            */
            if(canvas_hasarray(x))
            {
                // here we assign array_joc global variable, so that the
                // array_doclick can be computed properly. we, however,
                // use doit = 0, just to see if we are within the hitbox
                // then we act on the topmost garray below after the
                // passthrough call.
                // ico 2023-11-19: disabling joc everywhere since
                // it is not needed anymore
                //array_joc = 0; //garray_joc((t_garray *)y);
                if (gobj_click(y, x, xpix, ypix, shift, alt, 0, 0))
                {
                    clickme = y;
                }
            }
            else
            {
                //post("!clickreturned");
                // check if the object wants to be clicked
                // (we pick the topmost clickable)
                if (canvas_hitbox(x, y, xpix, ypix, &x1, &y1, &x2, &y2))
                {
                    ob = pd_checkobject(&y->g_pd);
                    /* do not give clicks to comments or cnv during runtime */
                    /* ico@vt.edu 2021-10-10: also do not give clicks to the
                       ggee/image object if it is in click_mode 3 because that
                       one needs to be passed through below it, so we manually
                       acknowledge the click here without interrupting the flow.
                    */
                    //if (ob)
                    //    post("clicking on object with te_iemgui=%d",
                    //        ((t_text *)y)->te_iemgui);
                    if (!ob ||
                            (ob && y->g_pd->c_wb->w_clickfn && 
                            (ob->te_type != T_TEXT && ob->ob_pd != my_canvas_class &&
                            ((t_text *)y)->te_iemgui != 3))
                       )
                    {
                        clickme = y;
                        //post("yclick is set to %lx %d <%s>", y,
                        //   ((t_text *)y)->te_iemgui, ob->ob_pd->c_name->s_name);
                    }
                    //fprintf(stderr,"    MAIN found clickable %d\n",
                    //    clickreturned);
                }
            }
        }
        canvas_passthrough_click(x, xpix, ypix, doit);
        if (clickme)
        {
            clickreturned = gobj_click(clickme, x, xpix, ypix,
                    shift, alt, 0, doit);
            if (canvas_hasarray(x) && clickreturned)
                clickreturned = CURSOR_RUNMODE_THICKEN; //CURSOR_EDITMODE_RESIZE_Y;
            //post("clickme %d", clickreturned);
        }
        if (!doit)
        {
            //post("...!doit clickreturned=%d", clickreturned);
            if (clickme != NULL || clickreturned)
            {
                //post("...clickme");
                canvas_setcursor(glist_getcanvas(x), clickreturned);
            }
            /*else if (!array_joc)
            {
                //post("...!array_joc");
                canvas_setcursor(glist_getcanvas(x), CURSOR_RUNMODE_NOTHING);
            }*/
        }
        return (clickreturned); 
    }
}

t_widgetbehavior graph_widgetbehavior =
{
    graph_getrect,
    graph_displace,
    graph_select,
    graph_activate,
    graph_delete,
    graph_vis,
    graph_click,
    graph_displace_withtag,
};

    /* find the graph most recently added to this glist;
        if none exists, return 0. */

t_glist *glist_findgraph(t_glist *x)
{
    t_gobj *y = 0, *z;
    for (z = x->gl_list; z; z = z->g_next)
        if (pd_class(&z->g_pd) == canvas_class && ((t_glist *)z)->gl_isgraph)
            y = z;
    return ((t_glist *)y);
}

extern void canvas_menuarray(t_glist *canvas);

void g_graph_setup(void)
{
    class_setwidget(canvas_class, &graph_widgetbehavior);
    class_addmethod(canvas_class, (t_method)graph_bounds, gensym("bounds"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(canvas_class, (t_method)graph_xticks, gensym("xticks"),
        A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(canvas_class, (t_method)graph_xlabel, gensym("xlabel"),
        A_GIMME, 0);
    class_addmethod(canvas_class, (t_method)graph_yticks, gensym("yticks"),
        A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(canvas_class, (t_method)graph_ylabel, gensym("ylabel"),
        A_GIMME, 0);
    class_addmethod(canvas_class, (t_method)graph_array, gensym("array"),
        A_GIMME, A_NULL);
    class_addmethod(canvas_class, (t_method)graph_gopspill,
        gensym("gopspill"), A_FLOAT, 0);
    class_addmethod(canvas_class, (t_method)canvas_menuarray,
        gensym("menuarray"), A_NULL);
    class_addmethod(canvas_class, (t_method)glist_sort,
        gensym("sort"), A_NULL);
}
