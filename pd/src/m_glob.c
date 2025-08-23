/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"

t_class *glob_pdobject;
static t_class *maxclass;

/* ico@vt.edu 2023-04-17: moved sys_curved_cords, sys_snaptogrid, sys_gridsize, sys_autocomplete_relevance, sys_autocomplete_prefix, and sys_autocomplete to m_glob.c to allow for
   emscripten to compile since emscripten currently does not compile s_file.c */
int sys_curved_cords = 1;
int sys_snaptogrid = 1;
int sys_gridsize = 10;
int sys_autocomplete_relevance = 1;
int sys_autocomplete_prefix;
int sys_autocomplete = 1;


int sys_perf;   /* true if we should query user on close and quit */

/* Compatibility level, e.g., 43 for pd 0.43 compatibility. We default to
   the current minor version. If Pd Vanilla ever changes the major version
   we will need to revisit them implementation of this feature */
int pd_compatibilitylevel = 100000;
/* These "glob" routines, which implement messages to Pd, are from all
over.  Some others are prototyped in m_imp.h as well. */

void glob_setfilename(void *dummy, t_symbol *filesym, t_symbol *dirsym);
void glob_quit(void *dummy, t_floatarg status);
void glob_verifyquit(void *dummy, t_floatarg f);
void glob_dsp(void *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_key(void *dummy, t_symbol *s, int ac, t_atom *av);
void glob_pastetext(void *dummy, t_symbol *s, int ac, t_atom *av);
void glob_audiostatus(void *dummy);
void glob_finderror(t_pd *dummy);
void glob_findinstance(t_pd *dummy, t_symbol*s);
void glob_audio_properties(t_pd *dummy, t_floatarg flongform);
void glob_audio_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_audio_setapi(t_pd *dummy, t_floatarg f);
void glob_audio_refresh(t_pd *dummy);
void glob_midi_properties(t_pd *dummy, t_floatarg flongform);
void glob_midi_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_midi_setapi(t_pd *dummy, t_floatarg f);
void glob_midi_refresh(t_pd *dummy);
void glob_start_path_dialog(t_pd *dummy, t_floatarg flongform);
void glob_path_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_addtopath(t_pd *dummy, t_symbol *path, t_float saveit);
void glob_start_startup_dialog(t_pd *dummy, t_floatarg flongform);
void glob_startup_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv);
void glob_ping(t_pd *dummy);
void glob_watchdog(void *dummy);
void glob_savepreferences(t_pd *dummy);
void glob_forward_files_from_secondary_instance(void);
void glob_recent_files(t_pd *dummy);
void glob_add_recent_file(t_pd *dummy, t_symbol *s);
void glob_clear_recent_files(t_pd *dummy);
void glob_open(t_pd *ignore, t_symbol *name, t_symbol *dir, t_floatarg f);
void glob_settracing(void *dummy, t_float f);
void glob_fastforward(t_pd *ignore, t_floatarg f);
void glob_colors(void *dummy, t_symbol *fg, t_symbol *bg, t_symbol *sel,
    t_symbol *gop);

static void glob_compatibility(t_pd *dummy, t_floatarg level)
{
    int dspwas = canvas_suspend_dsp();
    pd_compatibilitylevel = 0.5 + 100. * level;
    canvas_resume_dsp(dspwas);
}

#ifdef _WIN32
void glob_audio(void *dummy, t_floatarg adc, t_floatarg dac);
#endif

/* a method you add for debugging printout */
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv);

#if 1
void glob_foo(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
#ifdef USEAPI_ALSA
    void alsa_printstate(void);
    alsa_printstate();
#endif
}
#endif

static void glob_version(t_pd *dummy, t_float f)
{
    if (f > (PD_MAJOR_VERSION + 0.01*PD_MINOR_VERSION + 0.001))
    {
        static int warned;
        if (warned < 1)
            post("warning: file format (%g) newer than this version (%g) of Pd",
                f, PD_MAJOR_VERSION + 0.01*PD_MINOR_VERSION);
        else if (warned < 2)
            post("(... more file format messages suppressed)");
        warned++;
    }
}

static void glob_perf(t_pd *dummy, t_float f)
{
    sys_perf = (f != 0);
}

extern int sys_snaptogrid, sys_gridsize, sys_zoom,
    sys_autocomplete, sys_autocomplete_prefix, sys_autocomplete_relevance,
    sys_browser_doc, sys_browser_path, sys_browser_init,
    sys_autopatch_yoffset, sys_curved_cords;
extern t_symbol *sys_gui_preset;
static void glob_gui_prefs(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    sys_gui_preset = atom_getsymbolarg(0, argc--, argv++);
    //sys_grid = !!atom_getintarg(0, argc--, argv++);
    sys_snaptogrid = !!atom_getintarg(0, argc--, argv++);
    sys_gridsize = atom_getintarg(0, argc--, argv++);
    sys_zoom = !!atom_getintarg(0, argc--, argv++);
    sys_autocomplete = !!atom_getintarg(0, argc--, argv++);
    sys_autocomplete_prefix = !!atom_getintarg(0, argc--, argv++);
    sys_autocomplete_relevance = !!atom_getintarg(0, argc--, argv++);
    sys_curved_cords = !!atom_getintarg(0, argc--, argv++);
    sys_browser_doc = !!atom_getintarg(0, argc--, argv++);
    sys_browser_path = !!atom_getintarg(0, argc--, argv++);
    sys_browser_init = !!atom_getintarg(0, argc--, argv++);
    sys_autopatch_yoffset = atom_getintarg(0, argc--, argv++);
}

/* just the gui-preset, the save-zoom toggle and various help browser options for now */
static void glob_gui_properties(t_pd *dummy)
{
    gui_vmess("gui_gui_properties", "xsiiiiiiiiiii",
        dummy,
        sys_gui_preset->s_name,
        sys_snaptogrid,
        sys_gridsize,
        sys_zoom,
        sys_autocomplete,
        sys_autocomplete_prefix,
        sys_autocomplete_relevance,
        sys_browser_doc,
        sys_browser_path,
        sys_browser_init,
        sys_autopatch_yoffset,
        sys_curved_cords);
}

int sys_gui_busy;
static void glob_gui_busy(void *dummy, t_float f)
{
  sys_gui_busy = f != 0;
}

// ths one lives inside g_editor so that it can access the clipboard
extern void glob_clipboard_text(t_pd *dummy, float f);

void max_default(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    char str[80];
    startpost("%s: unknown message %s", class_getname(pd_class(x)),
        s->s_name);
    for (i = 0; i < argc; i++)
    {
        atom_string(argv+i, str, 80);
        poststring(str);
    }
    endpost();
}

// if the k12 flag is set from the GUI
// requires <s_stuff.h> to be able to
// access sys_k12_mode

void glob_set_k12_mode(void *dummy, t_float f)
{
    //post("glob_set_k12_mode %f", f);
    if ((int)f != 0 && (int)f != 1)
        return;
    sys_k12_mode = (int)f;
    //fprintf(stderr, "k12_mode=%d", sys_k12_mode);
}

#ifndef __EMSCRIPTEN__
// located in s_file.c
extern int reinit_user_settings(void *dummy);
#endif

void glob_init(void)
{
    maxclass = class_new(gensym("max"), 0, 0, sizeof(t_pd),
        CLASS_DEFAULT, A_NULL);
    class_addanything(maxclass, max_default);
    pd_bind(&maxclass, gensym("max"));

    glob_pdobject = class_new(gensym("pd"), 0, 0, sizeof(t_pd),
        CLASS_DEFAULT, A_NULL);
    class_addmethod(glob_pdobject, (t_method)glob_initfromgui, gensym("init"),
        A_GIMME, 0);
    class_addmethod(glob_pdobject,
        (t_method)glob_forward_files_from_secondary_instance,
        gensym("forward_files_from_secondary_instance"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_setfilename, 
        gensym("filename"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_setfilename, 
        gensym("menunew"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_open, gensym("open"),
        A_SYMBOL, A_SYMBOL, A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_quit, gensym("quit"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_quit, gensym("exit"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_verifyquit,
        gensym("verifyquit"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_foo, gensym("foo"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_dsp, gensym("dsp"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_key, gensym("key"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_pastetext, gensym("pastetext"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audiostatus,
        gensym("audiostatus"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_finderror,
        gensym("finderror"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_findinstance,
          gensym("findinstance"), A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_properties,
        gensym("audio-properties"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_dialog,
        gensym("audio-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_setapi,
        gensym("audio-setapi"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_audio_refresh,
        gensym("audio-refresh"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_setapi,
        gensym("midi-setapi"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_refresh,
        gensym("midi-refresh"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_properties,
        gensym("midi-properties"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_midi_dialog,
        gensym("midi-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_start_path_dialog,
        gensym("start-path-dialog"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_path_dialog,
        gensym("path-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_addtopath,
        gensym("add-to-path"), A_SYMBOL, A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_start_startup_dialog,
        gensym("start-startup-dialog"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_startup_dialog,
        gensym("startup-dialog"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_ping, gensym("ping"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_savepreferences,
        gensym("save-preferences"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_version,
        gensym("version"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_perf,
        gensym("perf"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_compatibility,
        gensym("compatibility"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_clipboard_text,
        gensym("clipboardtext"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_gui_prefs,
        gensym("gui-prefs"), A_GIMME, 0);
    class_addmethod(glob_pdobject, (t_method)glob_gui_properties,
        gensym("gui-properties"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_recent_files,
        gensym("recent-files"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_add_recent_file,
        gensym("add-recent-file"), A_SYMBOL, 0);
    class_addmethod(glob_pdobject, (t_method)glob_clear_recent_files,
        gensym("clear-recent-files"), 0);
    class_addmethod(glob_pdobject, (t_method)glob_gui_busy,
        gensym("gui-busy"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_set_k12_mode,
        gensym("set-k12-mode"), A_DEFFLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_settracing,
         gensym("set-tracing"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_fastforward,
         gensym("fast-forward"), A_FLOAT, 0);
    class_addmethod(glob_pdobject, (t_method)glob_colors,
        gensym("colors"), A_SYMBOL, A_SYMBOL, A_SYMBOL, A_DEFSYMBOL, 0);
    /* -------------- windows delete all registry keys in s_file.c ------------------ */
#ifndef __EMSCRIPTEN__
    class_addmethod(glob_pdobject, (t_method)reinit_user_settings,
        gensym("reinit-user-settings"), 0);
#endif
#if defined(__linux__) || defined(__FreeBSD_kernel__)
    class_addmethod(glob_pdobject, (t_method)glob_watchdog,
        gensym("watchdog"), 0);
#endif
    class_addanything(glob_pdobject, max_default);
    pd_bind(&glob_pdobject, gensym("pd"));
}

    /* function to return version number at run time.  Any of the
    calling pointers may be zero in case you don't need all of them. */
unsigned int sys_getversion(int *major, int *minor, int *bugfix)
{
    if (major)
        *major = PD_MAJOR_VERSION;
    if (minor)
        *minor = PD_MINOR_VERSION;
    if (bugfix)
        *bugfix = PD_BUGFIX_VERSION;

    return PD_VERSION_CODE;
}

unsigned int sys_getfloatsize()
{
    return sizeof(t_float);
}
