/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "config.h"

#define PD_CLASS_DEF
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf sprintf_s
#endif

t_symbol *class_loadsym;     /* name under which an extern is invoked */
static void pd_defaultfloat(t_pd *x, t_float f);
static void pd_defaultblob(t_pd *x, t_blob *st); /* MP20061226 blob type */
static void pd_defaultlist(t_pd *x, t_symbol *s, int argc, t_atom *argv);
t_pd pd_objectmaker;    /* factory for creating "object" boxes */
t_pd pd_canvasmaker;    /* factory for creating canvases */

typedef struct _classtable
{
    t_class *ct_class;
    struct _classtable *ct_next;
} t_classtable;

t_classtable *ct;

static t_symbol *class_extern_dir = &s_;

static void pd_defaultanything(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    pd_error(x, "%s: no method for '%s'", (*x)->c_name->s_name, s->s_name);
}

static void pd_defaultbang(t_pd *x)
{
    if (*(*x)->c_listmethod != pd_defaultlist)
        (*(*x)->c_listmethod)(x, 0, 0, 0);
    else (*(*x)->c_anymethod)(x, &s_bang, 0, 0);
}

static void pd_defaultblob(t_pd *x, t_blob *st) /* MP 20061226 blob type */
{ /* for now just reject it, later convert to symbol/float/list */
    pd_error(x, "%s: no method for blob so far...", (*x)->c_name->s_name);
}

static void pd_defaultpointer(t_pd *x, t_gpointer *gp)
{
    if (*(*x)->c_listmethod != pd_defaultlist)
    {
        t_atom at;
        SETPOINTER(&at, gp);
        (*(*x)->c_listmethod)(x, 0, 1, &at);
    }
    else
    {
        t_atom at;
        SETPOINTER(&at, gp);
        (*(*x)->c_anymethod)(x, &s_pointer, 1, &at);
    }
}

static void pd_defaultfloat(t_pd *x, t_float f)
{
    if (*(*x)->c_listmethod != pd_defaultlist)
    {
        t_atom at;
        SETFLOAT(&at, f);
        (*(*x)->c_listmethod)(x, 0, 1, &at);
    }
    else
    {
        t_atom at;
        SETFLOAT(&at, f);
        (*(*x)->c_anymethod)(x, &s_float, 1, &at);
    }
}

static void pd_defaultsymbol(t_pd *x, t_symbol *s)
{
    if (*(*x)->c_listmethod != pd_defaultlist)
    {
        t_atom at;
        SETSYMBOL(&at, s);
        (*(*x)->c_listmethod)(x, 0, 1, &at);
    }
    else
    {
        t_atom at;
        SETSYMBOL(&at, s);
        (*(*x)->c_anymethod)(x, &s_symbol, 1, &at);
    }
}

void obj_list(t_object *x, t_symbol *s, int argc, t_atom *argv);
static void class_nosavefn(t_gobj *z, t_binbuf *b);

    /* handle "list" messages to Pds without explicit list methods defined. */
static void pd_defaultlist(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
            /* a list with no elements is handled by the 'bang' method if
            one exists. */
    if (argc == 0 && *(*x)->c_bangmethod != pd_defaultbang)
    {
        (*(*x)->c_bangmethod)(x);
        return;
    }
            /* a list with one element which is a number can be handled by a
            "float" method if any is defined; same for "symbol", "pointer". */
    if (argc == 1)
    {
        if (argv->a_type == A_FLOAT &&
        *(*x)->c_floatmethod != pd_defaultfloat)
        {
            (*(*x)->c_floatmethod)(x, argv->a_w.w_float);
            return;
        }
        else if (argv->a_type == A_SYMBOL &&
            *(*x)->c_symbolmethod != pd_defaultsymbol)
        {
            (*(*x)->c_symbolmethod)(x, argv->a_w.w_symbol);
            return;
        }
        else if (argv->a_type == A_POINTER &&
            *(*x)->c_pointermethod != pd_defaultpointer)
        {
            (*(*x)->c_pointermethod)(x, argv->a_w.w_gpointer);
            return;
        }
    }
        /* Next try for an "anything" method */
    if ((*x)->c_anymethod != pd_defaultanything)
        (*(*x)->c_anymethod)(x, &s_list, argc, argv);

        /* if the object is patchable (i.e., can have proper inlets)
            send it on to obj_list which will unpack the list into the inlets */
    else if (argc>0 && (*x)->c_patchable)
        obj_list((t_object *)x, s, argc, argv);
            /* otherwise give up and complain. */
    else pd_defaultanything(x, &s_list, argc, argv);
}

    /* for now we assume that all "gobjs" are text unless explicitly
    overridden later by calling class_setbehavior().  I'm not sure
    how to deal with Pds that aren't gobjs; shouldn't there be a
    way to check that at run time?  Perhaps the presence of a "newmethod"
    should be our cue, or perhaps the "tiny" flag.  */

    /* another matter.  This routine does two unrelated things: it creates
    a Pd class, but also adds a "new" method to create an instance of it.
    These are combined for historical reasons and for brevity in writing
    objects.  To avoid adding a "new" method send a null function pointer.
    To add additional ones, use class_addcreator below.  Some "classes", like
    "select", are actually two classes of the same name, one for the single-
    argument form, one for the multiple one; see select_setup() to find out
    how this is handled.  */

extern void text_save(t_gobj *z, t_binbuf *b);

void classtable_register(t_class *c)
{
    t_classtable *t;
    for(t = ct; t; t = t->ct_next)
        if (t->ct_class == c) post("already registered %s", c->c_name->s_name);
    t = (t_classtable *)t_getbytes(sizeof(*t));
    t->ct_class = c;
    t->ct_next = ct;
    ct = t;
}

int classtable_size(void) {
    t_classtable *t;
    int i;
    for(t = ct, i = 0; t; t = t->ct_next)
        i++;
    return i;
}

void classtable_tovec(int size, t_atom *vec)
{
    t_classtable *t;
    int i;
    for(t = ct, i = 0; t && i < size; t = t->ct_next, i++)
        if (!t->ct_class->c_name)
        {
            SETSYMBOL(vec+i, gensym("anonymous-class"));
        }
        else
            SETSYMBOL(vec+i, t->ct_class->c_name);
}

// todo-- make accessors so m_imp.h isn't needed by x_interface.c

t_class *classtable_findbyname(t_symbol *s)
{
    t_classtable *t;
    for (t = ct; t; t = t->ct_next)
        if (t->ct_class->c_name == s)
            return t->ct_class;
    return NULL;
}

t_class *class_new(t_symbol *s, t_newmethod newmethod, t_method freemethod,
    size_t size, int flags, t_atomtype type1, ...)
{
    va_list ap;
    t_atomtype vec[MAXPDARG+1], *vp = vec;
    int count = 0;
    t_class *c;
    int typeflag = flags & CLASS_TYPEMASK;
    if (!typeflag) typeflag = CLASS_PATCHABLE;
    *vp = type1;

    va_start(ap, type1);
    while (*vp)
    {
        if (count == MAXPDARG)
        {
            error("class %s: sorry: only %d args typechecked; use A_GIMME",
                s->s_name, MAXPDARG);
            break;
        }
        vp++;
        count++;
        *vp = va_arg(ap, t_atomtype);
    }
    va_end(ap);
    if (pd_objectmaker && newmethod)
    {
            /* add a "new" method by the name specified by the object */
        class_addmethod(pd_objectmaker, (t_method)newmethod, s,
            vec[0], vec[1], vec[2], vec[3], vec[4], vec[5]);
        if (class_loadsym)
        {
                /* if we're loading an extern it might have been invoked by a
                longer file name; in this case, make this an admissible name
                too. */
            char *loadstring = class_loadsym->s_name;
            int l1 = strlen(s->s_name), l2 = strlen(loadstring);
            if (l2 > l1 && !strcmp(s->s_name, loadstring + (l2 - l1)))
                class_addmethod(pd_objectmaker, (t_method)newmethod,
                    class_loadsym,
                    vec[0], vec[1], vec[2], vec[3], vec[4], vec[5]);
        }
    }
    c = (t_class *)t_getbytes(sizeof(*c));
    c->c_name = c->c_helpname = s;
    c->c_size = size;
    c->c_methods = t_getbytes(0);
    c->c_nmethod = 0;
    c->c_freemethod = (t_method)freemethod;
    c->c_bangmethod = pd_defaultbang;
    c->c_pointermethod = pd_defaultpointer;
    c->c_floatmethod = pd_defaultfloat;
    c->c_symbolmethod = pd_defaultsymbol;
    c->c_blobmethod = pd_defaultblob; /* MP 20061226 blob type */
    c->c_listmethod = pd_defaultlist;
    c->c_anymethod = pd_defaultanything;
    c->c_wb = (typeflag == CLASS_PATCHABLE ? &text_widgetbehavior : 0);
    c->c_pwb = 0;
    c->c_firstin = ((flags & CLASS_NOINLET) == 0);
    c->c_patchable = (typeflag == CLASS_PATCHABLE);
    c->c_gobj = (typeflag >= CLASS_GOBJ);
    c->c_drawcommand = 0;
    c->c_floatsignalin = 0;
    c->c_externdir = class_extern_dir;
    c->c_savefn = (typeflag == CLASS_PATCHABLE ? text_save : class_nosavefn);
#if 0 
    post("class: %s", c->c_name->s_name);
#endif
    classtable_register(c);
//    post("class: %s", c->c_name->s_name);
    return (c);
}

    /* add a creation method, which is a function that returns a Pd object
    suitable for putting in an object box.  We presume you've got a class it
    can belong to, but this won't be used until the newmethod is actually
    called back (and the new method explicitly takes care of this.) */

void class_addcreator(t_newmethod newmethod, t_symbol *s, 
    t_atomtype type1, ...)
{
    va_list ap;
    t_atomtype vec[MAXPDARG+1], *vp = vec;
    int count = 0;
    *vp = type1;

    va_start(ap, type1);
    while (*vp)
    {
        if (count == MAXPDARG)
        {
            error("class %s: sorry: only %d creation args allowed",
                s->s_name, MAXPDARG);
            break;
        }
        vp++;
        count++;
        *vp = va_arg(ap, t_atomtype);
    } 
    va_end(ap);
    class_addmethod(pd_objectmaker, (t_method)newmethod, s,
        vec[0], vec[1], vec[2], vec[3], vec[4], vec[5]);
    if (class_loadsym)
    {
            /* if we're loading an extern it might have been invoked by a
            longer file name; in this case, make this an admissible name
            too. */
        char *loadstring = class_loadsym->s_name,
            l1 = strlen(s->s_name), l2 = strlen(loadstring);
        if (l2 > l1 && !strcmp(s->s_name, loadstring + (l2 - l1)))
            class_addmethod(pd_objectmaker, (t_method)newmethod,
                class_loadsym,
                vec[0], vec[1], vec[2], vec[3], vec[4], vec[5]);
    }
}

void class_addmethod(t_class *c, t_method fn, t_symbol *sel,
    t_atomtype arg1, ...)
{
    va_list ap;
    t_methodentry *m;
    t_atomtype argtype = arg1;
    int nargs;
    
    va_start(ap, arg1);
        /* "signal" method specifies that we take audio signals but
        that we don't want automatic float to signal conversion.  This
        is obsolete; you should now use the CLASS_MAINSIGNALIN macro. */
    if (sel == &s_signal)
    {
        if (c->c_floatsignalin)
            post("warning: signal method overrides class_mainsignalin");
        c->c_floatsignalin = -1;
    }
        /* check for special cases.  "Pointer" is missing here so that
        pd_objectmaker's pointer method can be typechecked differently.  */
    if (sel == &s_bang)
    {
        if (argtype) goto phooey;
        class_addbang(c, fn);
    }
    else if (sel == &s_float)
    {
        if (argtype != A_FLOAT || va_arg(ap, t_atomtype)) goto phooey;
        class_doaddfloat(c, fn);
    }
    else if (sel == &s_symbol)
    {
        if (argtype != A_SYMBOL || va_arg(ap, t_atomtype)) goto phooey;
        class_addsymbol(c, fn);
    }
    else if (sel == &s_blob) /* MP 20070106 blob type */
    {
        post("class_addmethod: %p", fn);
        if (argtype != A_BLOB || va_arg(ap, t_atomtype)) goto phooey;
        class_addblob(c, fn);
    }
    else if (sel == &s_list)
    {
        if (argtype != A_GIMME) goto phooey;
        class_addlist(c, fn);
    }
    else if (sel == &s_anything)
    {
        if (argtype != A_GIMME) goto phooey;
        class_addanything(c, fn);
    }
    else
    {
        /* Pd-extended doesn't use the aliasing automagic
        int i;
        for (i = 0; i < c->c_nmethod; i++)
            if (c->c_methods[i].me_name == sel)
        {
            char nbuf[80];
            snprintf(nbuf, 80, "%s_aliased", sel->s_name);
            c->c_methods[i].me_name = gensym(nbuf);
            if (c == pd_objectmaker)
                post("warning: class '%s' overwritten; old one renamed '%s'",
                    sel->s_name, nbuf);
            else post("warning: old method '%s' for class '%s' renamed '%s'",
                sel->s_name, c->c_name->s_name, nbuf);
        }
        */
        c->c_methods = t_resizebytes(c->c_methods,
            c->c_nmethod * sizeof(*c->c_methods),
            (c->c_nmethod + 1) * sizeof(*c->c_methods));
        m = c->c_methods +  c->c_nmethod;
        c->c_nmethod++;
        m->me_name = sel;
        m->me_fun = (t_gotfn)fn;
        nargs = 0;
        while (argtype != A_NULL && nargs < MAXPDARG)
        {
            m->me_arg[nargs++] = argtype;
            argtype = va_arg(ap, t_atomtype);
        }
        if (argtype != A_NULL)
            error("%s_%s: only 5 arguments are typecheckable; use A_GIMME",
                c->c_name->s_name, sel->s_name);
        m->me_arg[nargs] = A_NULL;
    }
    va_end(ap);
    return;
phooey:
    bug("class_addmethod: %s_%s: bad argument types\n",
        c->c_name->s_name, sel->s_name);
}

    /* Instead of these, see the "class_addfloat", etc.,  macros in m_pd.h */
void class_addbang(t_class *c, t_method fn)
{
    c->c_bangmethod = (t_bangmethod)fn;
}

void class_addpointer(t_class *c, t_method fn)
{
    c->c_pointermethod = (t_pointermethod)fn;
}

void class_doaddfloat(t_class *c, t_method fn)
{
    c->c_floatmethod = (t_floatmethod)fn;
}

void class_addsymbol(t_class *c, t_method fn)
{
    c->c_symbolmethod = (t_symbolmethod)fn;
}

void class_addblob(t_class *c, t_method fn) /* MP 20061226 blob type */
{
    c->c_blobmethod = (t_blobmethod)fn;
}

void class_addlist(t_class *c, t_method fn)
{
    c->c_listmethod = (t_listmethod)fn;
}

void class_addanything(t_class *c, t_method fn)
{
    c->c_anymethod = (t_anymethod)fn;
}

void class_setwidget(t_class *c, t_widgetbehavior *w)
{
    c->c_wb = w;
}

void class_setparentwidget(t_class *c, t_parentwidgetbehavior *pw)
{
    c->c_pwb = pw;
}

char *class_getname(t_class *c)
{
    return (c->c_name->s_name);
}

char *class_gethelpname(t_class *c)
{
    return (c->c_helpname->s_name);
}

void class_sethelpsymbol(t_class *c, t_symbol *s)
{
    c->c_helpname = s;
}

t_parentwidgetbehavior *pd_getparentwidget(t_pd *x)
{
    return ((*x)->c_pwb);
}

void class_setdrawcommand(t_class *c)
{
    c->c_drawcommand = 1;
}

int class_isdrawcommand(t_class *c)
{
    return (c->c_drawcommand);
}

static void pd_floatforsignal(t_pd *x, t_float f)
{
    int offset = (*x)->c_floatsignalin;
    if (offset > 0)
        *(t_float *)(((char *)x) + offset) = f;
    else
        pd_error(x, "%s: float unexpected for signal input",
            (*x)->c_name->s_name);
}

void class_domainsignalin(t_class *c, int onset)
{
    if (onset <= 0) onset = -1;
    else
    {
        if (c->c_floatmethod != pd_defaultfloat)
            post("warning: %s: float method overwritten", c->c_name->s_name);
        c->c_floatmethod = (t_floatmethod)pd_floatforsignal;
    }
    c->c_floatsignalin = onset;
}

void class_set_extern_dir(t_symbol *s)
{
    class_extern_dir = s;
}

char *class_gethelpdir(t_class *c)
{
    return (c->c_externdir->s_name);
}

static void class_nosavefn(t_gobj *z, t_binbuf *b)
{
    bug("save function called but not defined");
}

void class_setsavefn(t_class *c, t_savefn f)
{
    c->c_savefn = f;
}

t_savefn class_getsavefn(t_class *c)
{
    return (c->c_savefn);
}

void class_setpropertiesfn(t_class *c, t_propertiesfn f)
{
    c->c_propertiesfn = f;
}

t_propertiesfn class_getpropertiesfn(t_class *c)
{
    return (c->c_propertiesfn);
}

/* ---------------- the symbol table ------------------------ */

#define HASHSIZE 1024

static t_symbol *symhash[HASHSIZE];

t_symbol *dogensym(const char *s, t_symbol *oldsym)
{
    t_symbol **sym1, *sym2;
    unsigned int hash1 = 0,  hash2 = 0;
    int length = 0;
    const char *s2 = s;
    while (*s2)
    {
        hash1 += *s2;
        hash2 += hash1;
        length++;
        s2++;
    }
    sym1 = symhash + (hash2 & (HASHSIZE-1));
    while (sym2 = *sym1)
    {
        if (!strcmp(sym2->s_name, s)) return(sym2);
        sym1 = &sym2->s_next;
    }
    if (oldsym) sym2 = oldsym;
    else
    {
        sym2 = (t_symbol *)t_getbytes(sizeof(*sym2));
        sym2->s_name = t_getbytes(length+1);
        sym2->s_next = 0;
        sym2->s_thing = 0;
        strcpy(sym2->s_name, s);
    }
    *sym1 = sym2;
    return (sym2);
}

t_symbol *gensym(const char *s)
{
    return(dogensym(s, 0));
}

#define MAXOBJDEPTH 1000
static int tryingalready;

void canvas_popabstraction(t_canvas *x);
void canvas_initbang(t_canvas *x);

extern t_pd *newest;

t_symbol* pathsearch(t_symbol *s,char* ext);
int pd_setloadingabstraction(t_symbol *sym);

    /* this routine is called when a new "object" is requested whose class Pd
    doesn't know.  Pd tries to load it as an extern, then as an abstraction. */
void new_anything(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    if (tryingalready>MAXOBJDEPTH){
      error("maximum object loading depth %d reached", MAXOBJDEPTH);
      return;
    }
    newest = 0;
    class_loadsym = s;
    if (sys_load_lib(canvas_getcurrent(), s->s_name))
    {
        tryingalready++;
        typedmess(dummy, s, argc, argv);
        tryingalready--;
        return;
    }
    class_loadsym = 0;
}

t_symbol  s_pointer =   {"pointer", 0, 0};
t_symbol  s_float =     {"float", 0, 0};
t_symbol  s_symbol =    {"symbol", 0, 0};
t_symbol  s_bang =      {"bang", 0, 0};
t_symbol  s_list =      {"list", 0, 0};
t_symbol  s_anything =  {"anything", 0, 0};
t_symbol  s_signal =    {"signal", 0, 0};
t_symbol  s__N =        {"#N", 0, 0};
t_symbol  s__X =        {"#X", 0, 0};
t_symbol  s_x =         {"x", 0, 0};
t_symbol  s_y =         {"y", 0, 0};
t_symbol  s_ =          {"", 0, 0};
t_symbol  s_blob =      {"blob", 0, 0}; /* MP 20061223 blob type */

static t_symbol *symlist[] = { &s_pointer, &s_float, &s_symbol, &s_bang,
    &s_list, &s_anything, &s_signal, &s__N, &s__X, &s_x, &s_y, &s_, &s_blob}; /* MP 20061223 added s_blob */

void mess_init(void)
{
    t_symbol **sp;
    int i;

    if (pd_objectmaker) return;    
    for (i = sizeof(symlist)/sizeof(*symlist), sp = symlist; i--; sp++)
        (void) dogensym((*sp)->s_name, *sp);
    pd_objectmaker = class_new(gensym("objectmaker"), 0, 0, sizeof(t_pd),
        CLASS_DEFAULT, A_NULL);
    pd_canvasmaker = class_new(gensym("classmaker"), 0, 0, sizeof(t_pd),
        CLASS_DEFAULT, A_NULL);
    pd_bind(&pd_canvasmaker, &s__N);
    class_addanything(pd_objectmaker, (t_method)new_anything);
}

t_pd *newest;

/* This is externally available, but note that it might later disappear; the
whole "newest" thing is a hack which needs to be redesigned. */
t_pd *pd_newest(void)
{
    return (newest);
}

    /* horribly, we need prototypes for each of the artificial function
    calls in typedmess(), to keep the compiler quiet. */
typedef t_pd *(*t_newgimme)(t_symbol *s, int argc, t_atom *argv);
typedef void(*t_messgimme)(t_pd *x, t_symbol *s, int argc, t_atom *argv);
typedef void*(*t_messgimmer)(t_pd *x, t_symbol *s, int argc, t_atom *argv);

typedef t_pd *(*t_fun00)(void);
typedef t_pd *(*t_fun10)(t_int i1    );
typedef t_pd *(*t_fun20)(t_int i1, t_int i2    );
typedef t_pd *(*t_fun30)(t_int i1, t_int i2, t_int i3    );
typedef t_pd *(*t_fun40)(t_int i1, t_int i2, t_int i3, t_int i4    );
typedef t_pd *(*t_fun50)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5    );
typedef t_pd *(*t_fun60)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6    );

typedef t_pd *(*t_fun01)(
    t_floatarg d1);
typedef t_pd *(*t_fun11)(t_int i1,
    t_floatarg d1);
typedef t_pd *(*t_fun21)(t_int i1, t_int i2,
    t_floatarg d1);
typedef t_pd *(*t_fun31)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1);
typedef t_pd *(*t_fun41)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1);
typedef t_pd *(*t_fun51)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1);
typedef t_pd *(*t_fun61)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1);

typedef t_pd *(*t_fun02)(
    t_floatarg d1, t_floatarg d2);
typedef t_pd *(*t_fun12)(t_int i1,
    t_floatarg d1, t_floatarg d2);
typedef t_pd *(*t_fun22)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2);
typedef t_pd *(*t_fun32)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2);
typedef t_pd *(*t_fun42)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2);
typedef t_pd *(*t_fun52)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2);
typedef t_pd *(*t_fun62)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2);

typedef t_pd *(*t_fun03)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef t_pd *(*t_fun13)(t_int i1,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef t_pd *(*t_fun23)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef t_pd *(*t_fun33)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef t_pd *(*t_fun43)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef t_pd *(*t_fun53)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef t_pd *(*t_fun63)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);

typedef t_pd *(*t_fun04)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef t_pd *(*t_fun14)(t_int i1,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef t_pd *(*t_fun24)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef t_pd *(*t_fun34)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef t_pd *(*t_fun44)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef t_pd *(*t_fun54)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef t_pd *(*t_fun64)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);

typedef t_pd *(*t_fun05)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun15)(t_int i1,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun25)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun35)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun45)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun55)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun65)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);

typedef void(*t_vfun00)(void);
typedef void(*t_vfun10)(t_int i1    );
typedef void(*t_vfun20)(t_int i1, t_int i2    );
typedef void(*t_vfun30)(t_int i1, t_int i2, t_int i3    );
typedef void(*t_vfun40)(t_int i1, t_int i2, t_int i3, t_int i4    );
typedef void(*t_vfun50)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5    );
typedef void(*t_vfun60)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6    );

typedef void(*t_vfun01)(
    t_floatarg d1);
typedef void(*t_vfun11)(t_int i1,
    t_floatarg d1);
typedef void(*t_vfun21)(t_int i1, t_int i2,
    t_floatarg d1);
typedef void(*t_vfun31)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1);
typedef void(*t_vfun41)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1);
typedef void(*t_vfun51)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1);
typedef void(*t_vfun61)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1);

typedef void(*t_vfun02)(
    t_floatarg d1, t_floatarg d2);
typedef void(*t_vfun12)(t_int i1,
    t_floatarg d1, t_floatarg d2);
typedef void(*t_vfun22)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2);
typedef void(*t_vfun32)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2);
typedef void(*t_vfun42)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2);
typedef void(*t_vfun52)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2);
typedef void(*t_vfun62)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2);

typedef void(*t_vfun03)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef void(*t_vfun13)(t_int i1,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef void(*t_vfun23)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef void(*t_vfun33)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef void(*t_vfun43)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef void(*t_vfun53)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);
typedef void(*t_vfun63)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2, t_floatarg d3);

typedef void(*t_vfun04)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef void(*t_vfun14)(t_int i1,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef void(*t_vfun24)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef void(*t_vfun34)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef void(*t_vfun44)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef void(*t_vfun54)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);
typedef void(*t_vfun64)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4);

typedef void(*t_vfun05)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef void(*t_vfun15)(t_int i1,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef void(*t_vfun25)(t_int i1, t_int i2,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef void(*t_vfun35)(t_int i1, t_int i2, t_int i3,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef void(*t_vfun45)(t_int i1, t_int i2, t_int i3, t_int i4,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef void(*t_vfun55)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef void(*t_vfun65)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5, t_int i6,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);

void *bang_new(t_pd *dummy);
void *pdfloat_new(t_pd *dummy, t_float f);
void *pdsymbol_new(t_pd *dummy, t_symbol *s);
void *list_new(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);

t_canvas *canvas_new(void *dummy, t_symbol *sel, int argc, t_atom *argv);

/* needed for proper error reporting */
extern t_pd *pd_mess_from_responder(t_pd *x);

/* This is a hack for the messages in Pd files that follow a comma, such as:
     #X restore..., f 12
   In that case the "f 12" would get sent to the subcanvas where we actually
   want it to apply to the parent.

   Pd Vanilla apparently made a special case to handle this-- instead of using
   the comma after the "restore" message it starts a new "#X f 12;" which
   ensures that the #X is bound to the correct canvas. But that means it fails
   for the old the old syntax, and there are patches in the wild that use it.

   So we support _both_ of these styles in Purr Data by doing the following:
   1. Checking if the last typedmess was "restore" selector.
   2. Checking if the last t_pd was a canvas that matches the current one.
      If so, we're dealing with the old "#X..., f 12;" syntax. If not, it's
      the new Pd Vanilla syntax.
*/
t_symbol *last_typedmess;
t_pd *last_typedmess_pd;

void pd_typedmess(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    t_method *f;
    t_class *c = *x;
    t_methodentry *m, *mlist;
    t_atomtype *wp, wanttype;
    int i;
    t_int ai[MAXPDARG+1], *ap = ai;
    t_floatarg ad[MAXPDARG+1], *dp = ad;
    int niarg = 0;
    int nfarg = 0;
    t_pd *bonzo;

        /* check for messages that are handled by fixed slots in the class
        structure.  We don't catch "pointer" though so that sending "pointer"
        to pd_objectmaker doesn't require that we supply a pointer value. */
    if (s == &s_float)
    {
        if (x == &pd_objectmaker)
          if (!argc)
              newest = pdfloat_new(x, 0.);
          else if (argv->a_type == A_FLOAT)
              newest = pdfloat_new(x, argv->a_w.w_float);
          else goto badarg;
        else
          if (!argc) (*c->c_floatmethod)(x, 0.);
          else if (argv->a_type == A_FLOAT)
              (*c->c_floatmethod)(x, argv->a_w.w_float);
          else goto badarg;
        return;
    }
    if (s == &s_bang)
    {
        if (x == &pd_objectmaker)
            newest = bang_new(x);
        else
            (*c->c_bangmethod)(x);
        return;
    }
    if (s == &s_list)
    {
        if (x == &pd_objectmaker)
            newest = list_new(x, s, argc, argv);
        else
            (*c->c_listmethod)(x, s, argc, argv);
        return;
    }
    if (s == &s_symbol)
    {
        if (argc && argv->a_type == A_SYMBOL)
           if (x == &pd_objectmaker)
                newest = pdsymbol_new(x, argv->a_w.w_symbol);
           else
                (*c->c_symbolmethod)(x, argv->a_w.w_symbol);
        else
           if (x == &pd_objectmaker)
                newest = pdsymbol_new(x, &s_);
           else
                (*c->c_symbolmethod)(x, &s_);
        return;
    }
#ifdef PDINSTANCE
    mlist = c->c_methods[pd_this->pd_instanceno];
#else
    mlist = c->c_methods;
#endif
    for (i = c->c_nmethod, m = mlist; i--; m++)
        if (m->me_name == s)
    {
        wp = m->me_arg;
        if (*wp == A_GIMME)
        {
            if (x == &pd_objectmaker)
                newest =
                    (*((t_newgimme)(m->me_fun)))(s, argc, argv);
            else if (((t_messgimmer)(m->me_fun)) == ((t_messgimmer)(canvas_new)))
                (*((t_messgimmer)(m->me_fun)))(x, s, argc, argv);
            else
                (*((t_messgimme)(m->me_fun)))(x, s, argc, argv);
            return;
        }
        if (argc > MAXPDARG) argc = MAXPDARG;
        if (x != &pd_objectmaker) *(ap++) = (t_int)x, niarg++;
        while ((wanttype = *wp++))
        {
            switch (wanttype)
            {
            case A_POINTER:
                if (!argc) goto badarg;
                else
                {
                    if (argv->a_type == A_POINTER)
                        *ap = (t_int)(argv->a_w.w_gpointer);
                    else goto badarg;
                    argc--;
                    argv++;
                }
                niarg++;
                ap++;
                break;
            case A_FLOAT:
                if (!argc) goto badarg;  /* falls through */
            case A_DEFFLOAT:
                if (!argc) *dp = 0;
                else
                {
                    if (argv->a_type == A_FLOAT)
                        *dp = argv->a_w.w_float;
                    else goto badarg;
                    argc--;
                    argv++;
                }
                nfarg++;
                dp++;
                break;
            case A_SYMBOL:
                if (!argc) goto badarg;  /* falls through */
            case A_DEFSYM:
                if (!argc) *ap = (t_int)(&s_);
                else
                {
                    if (argv->a_type == A_SYMBOL)
                        *ap = (t_int)(argv->a_w.w_symbol);
                            /* if it's an unfilled "dollar" argument it appears
                            as zero here; cheat and bash it to the null
                            symbol.  Unfortunately, this lets real zeros
                            pass as symbols too, which seems wrong... */
                    else if (x == &pd_objectmaker && argv->a_type == A_FLOAT
                        && argv->a_w.w_float == 0)
                        *ap = (t_int)(&s_);
                    else goto badarg;
                    argc--;
                    argv++;
                }
                niarg++;
                ap++;
                break;
            default:
                goto badarg;
            }
        }

        if (x == &pd_objectmaker)
        {
            switch (niarg * 10 + nfarg)
            {
            case 0 : bonzo = (*(t_fun00)(m->me_fun))
                (); break;
            case 10 : bonzo = (*(t_fun10)(m->me_fun))
                (ai[0]); break;
            case 20 : bonzo = (*(t_fun20)(m->me_fun))
                (ai[0], ai[1]); break;
            case 30 : bonzo = (*(t_fun30)(m->me_fun))
                (ai[0], ai[1], ai[2]); break;
            case 40 : bonzo = (*(t_fun40)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3]); break;
            case 50 : bonzo = (*(t_fun50)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4]); break;
            case 60 : bonzo = (*(t_fun60)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]); break;

            case 1 : bonzo = (*(t_fun01)(m->me_fun))
                (ad[0]); break;
            case 11 : bonzo = (*(t_fun11)(m->me_fun))
                (ai[0], ad[0]); break;
            case 21 : bonzo = (*(t_fun21)(m->me_fun))
                (ai[0], ai[1], ad[0]); break;
            case 31 : bonzo = (*(t_fun31)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0]); break;
            case 41 : bonzo = (*(t_fun41)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0]); break;
            case 51 : bonzo = (*(t_fun51)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0]); break;
            case 61 : bonzo = (*(t_fun61)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0]); break;

            case 2 : bonzo = (*(t_fun02)(m->me_fun))
                (ad[0], ad[1]); break;
            case 12 : bonzo = (*(t_fun12)(m->me_fun))
                (ai[0], ad[0], ad[1]); break;
            case 22 : bonzo = (*(t_fun22)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1]); break;
            case 32 : bonzo = (*(t_fun32)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1]); break;
            case 42 : bonzo = (*(t_fun42)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1]); break;
            case 52 : bonzo = (*(t_fun52)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1]); break;
            case 62 : bonzo = (*(t_fun62)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1]); break;

            case 3 : bonzo = (*(t_fun03)(m->me_fun))
                (ad[0], ad[1], ad[2]); break;
            case 13 : bonzo = (*(t_fun13)(m->me_fun))
                (ai[0], ad[0], ad[1], ad[2]); break;
            case 23 : bonzo = (*(t_fun23)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1], ad[2]); break;
            case 33 : bonzo = (*(t_fun33)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1], ad[2]); break;
            case 43 : bonzo = (*(t_fun43)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1], ad[2]); break;
            case 53 : bonzo = (*(t_fun53)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1], ad[2]); break;
            case 63 : bonzo = (*(t_fun63)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1], ad[2]); break;

            case 4 : bonzo = (*(t_fun04)(m->me_fun))
                (ad[0], ad[1], ad[2], ad[3]); break;
            case 14 : bonzo = (*(t_fun14)(m->me_fun))
                (ai[0], ad[0], ad[1], ad[2], ad[3]); break;
            case 24 : bonzo = (*(t_fun24)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1], ad[2], ad[3]); break;
            case 34 : bonzo = (*(t_fun34)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1], ad[2], ad[3]); break;
            case 44 : bonzo = (*(t_fun44)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1], ad[2], ad[3]); break;
            case 54 : bonzo = (*(t_fun54)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1], ad[2], ad[3]); break;
            case 64 : bonzo = (*(t_fun64)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1], ad[2], ad[3]); break;

            case 5 : bonzo = (*(t_fun05)(m->me_fun))
                (ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 15 : bonzo = (*(t_fun15)(m->me_fun))
                (ai[0], ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 25 : bonzo = (*(t_fun25)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 35 : bonzo = (*(t_fun35)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 45 : bonzo = (*(t_fun45)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 55 : bonzo = (*(t_fun55)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 65 : bonzo = (*(t_fun65)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            default: bonzo = 0;
            }
            newest = bonzo;
        }
        else
        {
            switch (niarg * 10 + nfarg)
            {
            case 0 : (*(t_vfun00)(m->me_fun))
                (); break;
            case 10 : (*(t_vfun10)(m->me_fun))
                (ai[0]); break;
            case 20 : (*(t_vfun20)(m->me_fun))
                (ai[0], ai[1]); break;
            case 30 : (*(t_vfun30)(m->me_fun))
                (ai[0], ai[1], ai[2]); break;
            case 40 : (*(t_vfun40)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3]); break;
            case 50 : (*(t_vfun50)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4]); break;
            case 60 : (*(t_vfun60)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5]); break;

            case 1 : (*(t_vfun01)(m->me_fun))
                (ad[0]); break;
            case 11 : (*(t_vfun11)(m->me_fun))
                (ai[0], ad[0]); break;
            case 21 : (*(t_vfun21)(m->me_fun))
                (ai[0], ai[1], ad[0]); break;
            case 31 : (*(t_vfun31)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0]); break;
            case 41 : (*(t_vfun41)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0]); break;
            case 51 : (*(t_vfun51)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0]); break;
            case 61 : (*(t_vfun61)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0]); break;

            case 2 : (*(t_vfun02)(m->me_fun))
                (ad[0], ad[1]); break;
            case 12 : (*(t_vfun12)(m->me_fun))
                (ai[0], ad[0], ad[1]); break;
            case 22 : (*(t_vfun22)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1]); break;
            case 32 : (*(t_vfun32)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1]); break;
            case 42 : (*(t_vfun42)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1]); break;
            case 52 : (*(t_vfun52)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1]); break;
            case 62 : (*(t_vfun62)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1]); break;

            case 3 : (*(t_vfun03)(m->me_fun))
                (ad[0], ad[1], ad[2]); break;
            case 13 : (*(t_vfun13)(m->me_fun))
                (ai[0], ad[0], ad[1], ad[2]); break;
            case 23 : (*(t_vfun23)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1], ad[2]); break;
            case 33 : (*(t_vfun33)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1], ad[2]); break;
            case 43 : (*(t_vfun43)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1], ad[2]); break;
            case 53 : (*(t_vfun53)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1], ad[2]); break;
            case 63 : (*(t_vfun63)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1], ad[2]); break;

            case 4 : (*(t_vfun04)(m->me_fun))
                (ad[0], ad[1], ad[2], ad[3]); break;
            case 14 : (*(t_vfun14)(m->me_fun))
                (ai[0], ad[0], ad[1], ad[2], ad[3]); break;
            case 24 : (*(t_vfun24)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1], ad[2], ad[3]); break;
            case 34 : (*(t_vfun34)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1], ad[2], ad[3]); break;
            case 44 : (*(t_vfun44)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1], ad[2], ad[3]); break;
            case 54 : (*(t_vfun54)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1], ad[2], ad[3]); break;
            case 64 : (*(t_vfun64)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1], ad[2], ad[3]); break;

            case 5 : (*(t_vfun05)(m->me_fun))
                (ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 15 : (*(t_vfun15)(m->me_fun))
                (ai[0], ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 25 : (*(t_vfun25)(m->me_fun))
                (ai[0], ai[1], ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 35 : (*(t_vfun35)(m->me_fun))
                (ai[0], ai[1], ai[2], ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 45 : (*(t_vfun45)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3],
                    ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 55 : (*(t_vfun55)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4],
                    ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            case 65 : (*(t_vfun65)(m->me_fun))
                (ai[0], ai[1], ai[2], ai[3], ai[4], ai[5],
                    ad[0], ad[1], ad[2], ad[3], ad[4]); break;
            default: ;
            }
        }
        return;
    }
    (*c->c_anymethod)(x, s, argc, argv);
    return;
badarg:
    pd_error(x, "Bad arguments for message '%s' to object '%s'",
        s->s_name, c->c_name->s_name);
}

    /* convenience routine giving a stdarg interface to typedmess().  Only
    ten args supported; it seems unlikely anyone will need more since
    longer messages are likely to be programmatically generated anyway. */
void pd_vmess(t_pd *x, t_symbol *sel, char *fmt, ...)
{
    va_list ap;
    t_atom arg[10], *at = arg;
    int nargs = 0;
    char *fp = fmt;

    va_start(ap, fmt);
    while (1)
    {
        if (nargs >= 10)
        {
            pd_error(x, "pd_vmess: only 10 allowed");
            break;
        }
        switch(*fp++)
        {
        case 'f': SETFLOAT(at, va_arg(ap, double)); break;
        case 's': SETSYMBOL(at, va_arg(ap, t_symbol *)); break;
        case 't':
            SETBLOB(at, va_arg(ap, t_blob *));
            /*post("pd_vmess: arg[0].a_w.w_blob = %p", arg[0].a_w.w_blob);*/
            break; /* MP 20061226 blob type */
        case 'i': SETFLOAT(at, va_arg(ap, t_int)); break;       
        case 'p': SETPOINTER(at, va_arg(ap, t_gpointer *)); break;
        default: goto done;
        }
        at++;
        nargs++;
    }
done:
    va_end(ap);
    typedmess(x, sel, nargs, arg);
}

void pd_forwardmess(t_pd *x, int argc, t_atom *argv)
{
    if (argc)
    {
        t_atomtype t = argv->a_type;
        if (t == A_SYMBOL) pd_typedmess(x, argv->a_w.w_symbol, argc-1, argv+1);
        else if (t == A_POINTER)
        {
            if (argc == 1) pd_pointer(x, argv->a_w.w_gpointer);
            else pd_list(x, &s_list, argc, argv);
        }
        else if (t == A_FLOAT)
        {
            if (argc == 1) pd_float(x, argv->a_w.w_float);
            else pd_list(x, &s_list, argc, argv);
        }
        else bug("pd_forwardmess");
    }

}

void nullfn(void) {}

t_gotfn getfn(t_pd *x, t_symbol *s)
{
    t_class *c = *x;
    t_methodentry *m;
    int i;

    for (i = c->c_nmethod, m = c->c_methods; i--; m++)
        if (m->me_name == s) return(m->me_fun);
    pd_error(x, "%s: no method for message '%s'", c->c_name->s_name, s->s_name);
    return((t_gotfn)nullfn);
}

t_gotfn zgetfn(t_pd *x, t_symbol *s)
{
    t_class *c = *x;
    t_methodentry *m;
    int i;

    for (i = c->c_nmethod, m = c->c_methods; i--; m++)
        if (m->me_name == s) return(m->me_fun);
    return(0);
}
