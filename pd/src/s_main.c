/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "s_net.h"
#include "s_version.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <winbase.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h> // for _NSGetExecutablePath
#endif
#include "m_private_utils.h"

char *pd_version;
static const char pd_compiletime[] = __TIME__;
static const char pd_compiledate[] = __DATE__;

void pd_init(void);
void pd_term(void);
int sys_argparse(int argc, const char **argv);
void sys_findprogdir(const char *progname);
void sys_setsignalhandlers(void);
int sys_startgui(const char *guipath);
void sys_stopgui(void);
void sys_setrealtime(const char *guipath);
int sys_rcfile(void);
int m_mainloop(void);
int m_batchmain(void);
void sys_addhelppath(char *p);
#ifdef USEAPI_ALSA
void alsa_adddev(const char *name);
#endif
void sys_doneglobinit( void);
t_symbol *pd_getdirname(void);

int sys_debuglevel;
int sys_verbose;
int sys_noloadbang;
int sys_nogui;
int sys_hipriority = -1;    /* -1 = don't care; 0 = no; 1 = yes */
int sys_guisetportnumber;   /* if started from the GUI, this is the port # */
int sys_nosleep = 0;    /* skip all "sleep" calls and spin instead */
int sys_console = 0;    /* default settings for the console is off */
int sys_k12_mode = 0;   /* by default k12 mode is off */
int sys_unique = 0;     /* by default off, prevents multiple instances
                           of pd-l2ork */
int sys_legacy = 0;     /* by default off, used to enable legacy features,
                           such as offsets in iemgui object positioning */
int sys_legacy_bendin = 0; /* by default off, used to enable vanilla-
                              compatible (unsigned) pitch bend input */
int sys_defeatrt;       /* flag to cancel real-time */
t_symbol *sys_flags;    /* more command-line flags */
const char *sys_guicmd;
t_symbol *sys_gui_preset; /* name of gui theme to be used */
t_symbol *sys_libdir;
t_symbol *sys_guidir;

typedef struct _patchlist
{
    struct _patchlist *pl_next;
    char *pl_file;
    char *pl_args;
} t_patchlist;

static t_patchlist *sys_openlist;
static t_namelist *sys_messagelist;
static int sys_version;
int sys_oldtclversion;      /* hack to warn g_rtext.c about old text sel */

int sys_nmidiout = -1;
int sys_nmidiin = -1;
int sys_midiindevlist[MAXMIDIINDEV] = {1};
int sys_midioutdevlist[MAXMIDIOUTDEV] = {1};

#ifdef __APPLE__
char sys_font[100] = "Monaco"; /* tb: font name */
#else
char sys_font[100] = "DejaVu Sans Mono"; /* tb: font name */
#endif
char sys_fontweight[10] = "normal"; /* currently only used for iemguis */

static int sys_listplease;

int sys_externalschedlib;
char sys_externalschedlibname[MAXPDSTRING];
int sys_batch;
const char *pd_extraflags = 0;
int sys_run_scheduler(const char *externalschedlibname,
    const char *sys_extraflagsstring);
int sys_noautopatch = 0;    /* temporary hack to defeat new 0.42 editing */
int glob_autopatch_connectme = 0;   /* added to compensate for weird gui objects 
                                       whose positioning is not true to its xy
                                       origin, to ensure they can be at least
                                       somewhat reasonably autopatched
                                    */

t_sample *get_sys_soundout() { return STUFF->st_soundout; }
t_sample *get_sys_soundin() { return STUFF->st_soundin; }
double *get_sys_time_per_dsp_tick() { return &STUFF->st_time_per_dsp_tick; }
int *get_sys_schedblocksize() { return &STUFF->st_schedblocksize; }
double *get_sys_time() { return &pd_this->pd_systime; }
t_float *get_sys_dacsr() { return &STUFF->st_dacsr; }
int *get_sys_schedadvance() { return &sys_schedadvance; }

t_namelist *sys_searchpath;  /* so old versions of GEM might compile */

typedef struct _fontinfo
{
    int fi_fontsize;
    int fi_maxwidth;
    int fi_maxheight;
    int fi_hostfontsize;
    int fi_width;
    int fi_height;
} t_fontinfo;

static t_fontinfo sys_fontlist[] = { \
    {8, 6, 10, 1, 1, 1}, {10, 7, 13, 1, 1, 1}, {12, 9, 16, 1, 1, 1},
    {16, 10, 20, 1, 1, 1}, {24, 15, 30, 1, 1, 1}, {36, 25, 45, 1, 1, 1}};
//0.43 values
//    {8, 6, 10, 0, 0, 0}, {10, 7, 13, 0, 0, 0}, {12, 9, 16, 0, 0, 0},
//    {16, 10, 20, 0, 0, 0}, {24, 15, 25, 0, 0, 0}, {36, 25, 45, 0, 0, 0}};
#define NFONT (sizeof(sys_fontlist)/sizeof(*sys_fontlist))

static t_fontinfo *sys_findfont(int fontsize)
{
    unsigned int i;
    t_fontinfo *fi;
    for (i = 0, fi = sys_fontlist; i < (NFONT-1); i++, fi++)
        if (fontsize < fi[1].fi_fontsize) return (fi);
    return (sys_fontlist + (NFONT-1));
}

int sys_nearestfontsize(int fontsize)
{
    return (sys_findfont(fontsize)->fi_fontsize);
}

int sys_hostfontsize(int fontsize)
{
    return (sys_findfont(fontsize)->fi_hostfontsize);
}

int sys_fontwidth(int fontsize)
{
    return (sys_findfont(fontsize)->fi_width);
}

int sys_fontheight(int fontsize)
{
    return (sys_findfont(fontsize)->fi_height);
}

int sys_defaultfont;
#define DEFAULTFONT 10

static t_patchlist * patchlist_append(t_patchlist *listwas,
    const char *files, const char *args)
{
    t_namelist *nl, *nl2;
    nl = namelist_append_files(0, files);
    for (nl2 = nl; nl2; nl2 = nl2->nl_next)
    {
        t_patchlist *pl, *pl2;
        pl = (t_patchlist *)(getbytes(sizeof(*pl)));
        pl->pl_next = 0;
        pl->pl_file = (char *)getbytes(strlen(nl2->nl_string) + 1);
        strcpy(pl->pl_file, nl2->nl_string);
        if (args)
        {
            pl->pl_args = (char *)getbytes(strlen(args) + 1);
            strcpy(pl->pl_args, args);
        }
        else pl->pl_args = 0;
        if (!listwas)
            listwas = pl;
        else
        {
            for (pl2 = listwas; pl2->pl_next; pl2 = pl2->pl_next) ;
            pl2->pl_next = pl;
        }
    }
    namelist_free(nl);
    return (listwas);
}

static void patchlist_free(t_patchlist *list)
{
    t_patchlist *pl, *pl2;
    for (pl = list; pl; pl = pl2)
    {
        pl2 = pl->pl_next;
        freebytes(pl->pl_file, strlen(pl->pl_file) + 1);
        if (pl->pl_args)
            freebytes(pl->pl_args, strlen(pl->pl_args) + 1);
        freebytes(pl, sizeof(*pl));
    }
}

static void openit(const char *dirname, const char *filename, const char *args)
{
    char dirbuf[FILENAME_MAX], *nameptr;
    int fd = open_via_path(dirname, filename, "", dirbuf, &nameptr,
        FILENAME_MAX, 0);
    if (fd >= 0)
    {
        sys_close(fd);
        if (args && *args)
        {
            t_binbuf *b1 = binbuf_new(), *b2 = binbuf_new();
            binbuf_text(b1, args, strlen(args));
            binbuf_addbinbuf(b2, b1); // bash semis, commas and dollars
            canvas_setargs(binbuf_getnatom(b2), binbuf_getvec(b2));
            binbuf_free(b1);
            binbuf_free(b2);
        }
        glob_evalfile(0, gensym(nameptr), gensym(dirbuf));
        gui_vmess("gui_process_open_arg", "s", filename);
    }
    else
        pd_error(0, "%s: can't open", filename);
}

/* this is called from the gui process.  The first argument is the cwd, and
succeeding args give the widths and heights of known fonts.  We wait until 
these are known to open files and send messages specified on the command line.
We ask the GUI to specify the "cwd" in case we don't have a local OS to get it
from; for instance we could be some kind of RT embedded system.  However, to
really make this make sense we would have to implement
open(), read(), etc, calls to be served somehow from the GUI too. */

void glob_initfromgui(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    const char *cwd = atom_getsymbolarg(0, argc, argv)->s_name;
    t_patchlist *pl;
    t_namelist *nl;
    unsigned int i;
    int j;
    int nhostfont = (argc-2)/3;
    sys_oldtclversion = atom_getfloatarg(1, argc, argv);
    if (argc != 2 + 3 * nhostfont) bug("glob_initfromgui");
    for (i = 0; i < NFONT; i++)
    {
        int best = 0;
        int wantheight = sys_fontlist[i].fi_maxheight;
        int wantwidth = sys_fontlist[i].fi_maxwidth;
        for (j = 1; j < nhostfont; j++)
        {
            if (atom_getintarg(3 * j + 4, argc, argv) <= wantheight &&
                atom_getintarg(3 * j + 3, argc, argv) <= wantwidth)
                    best = j;
        }
            /* best is now the host font index for the desired font index i. */
        sys_fontlist[i].fi_hostfontsize = atom_getintarg(3 * best + 2, argc, argv);
        sys_fontlist[i].fi_width = atom_getintarg(3 * best + 3, argc, argv);
        sys_fontlist[i].fi_height = atom_getintarg(3 * best + 4, argc, argv);
        sys_fontlist[i].fi_maxwidth = sys_fontlist[i].fi_width;
        sys_fontlist[i].fi_maxheight = sys_fontlist[i].fi_height;
    }
#if 0
    for (i = 0; i < 6; i++)
        fprintf(stderr, "font (%d %d %d) -> (%d %d %d)\n",
            sys_fontlist[i].fi_fontsize,
            sys_fontlist[i].fi_maxwidth,
            sys_fontlist[i].fi_maxheight,
            sys_fontlist[i].fi_hostfontsize,
            sys_fontlist[i].fi_width,
            sys_fontlist[i].fi_height);
#endif
        /* load dynamic libraries specified with "-lib" args */
    for  (nl = STUFF->st_externlist; nl; nl = nl->nl_next)
        if (!sys_load_lib(0, nl->nl_string))
            post("%s: can't load library", nl->nl_string);
        /* open patches specified with "-open" args */
    for (pl = sys_openlist; pl; pl = pl->pl_next)
        openit(cwd, pl->pl_file, pl->pl_args);
    patchlist_free(sys_openlist);
    sys_openlist = 0;
        /* send messages specified with "-send" args */
    for  (nl = sys_messagelist; nl; nl = nl->nl_next)
    {
        t_binbuf *b = binbuf_new();
        binbuf_text(b, nl->nl_string, strlen(nl->nl_string));
        binbuf_eval(b, 0, 0, 0);
        binbuf_free(b);
    }
    namelist_free(sys_messagelist);
    sys_messagelist = 0;
}

// font char metric triples: pointsize width(pixels) height(pixels)
static int defaultfontshit[] = {
 8, 5, 11, 9, 6, 12,
 10, 6, 13, 12, 7, 16,
 14, 8, 17, 16, 10, 19,
 18, 11, 22, 24, 14, 29,
 30, 18, 37, 36, 22, 44
};
#define NDEFAULTFONT (sizeof(defaultfontshit)/sizeof(*defaultfontshit))

static t_clock *sys_fakefromguiclk;
int socket_init(void);
static void sys_fakefromgui(void)
{
        /* fake the GUI's message giving cwd and font sizes in case
        we aren't starting the gui. */
    t_atom zz[NDEFAULTFONT+2];
    int i;
    char buf[MAXPDSTRING];
#ifdef _WIN32
    if (GetCurrentDirectory(MAXPDSTRING, buf) == 0)
        strcpy(buf, ".");
#else
    if (!getcwd(buf, MAXPDSTRING))
        strcpy(buf, ".");
#endif
    SETSYMBOL(zz, gensym(buf));
    SETFLOAT(zz+1, 0);
    for (i = 0; i < (int)NDEFAULTFONT; i++)
        SETFLOAT(zz+i+2, defaultfontshit[i]);
    glob_initfromgui(0, 0, 2+NDEFAULTFONT, zz);
    clock_free(sys_fakefromguiclk);
}

static void sys_afterargparse(void);
static void sys_printusage(void);

static void pd_makeversion(void)
{
    char foo[100];

    //snprintf(foo, sizeof(foo), "Pd-l2ork version %d.%d-%d%s\n",
    //    PD_MAJOR_VERSION, PD_MINOR_VERSION,
    //    PD_BUGFIX_VERSION, PD_TEST_VERSION);    

    snprintf(foo, sizeof(foo), "Pd-L2Ork version %s (%s)\n", PD_L2ORK_VERSION,
	     PD_BUILD_VERSION);

    pd_version = strdup(foo);
}

/* send the openlist to the GUI before closing a secondary
   instance of Pd. */
void glob_forward_files_from_secondary_instance(void)
{
    //fprintf(stderr, "glob_forward_files_from_secondary_instance\n");
        /* check if we are unique, otherwise, just focus existing
           instance, and if necessary open file inside it. This doesn't
           yet work with the new GUI because we need to set it up to
           allow multiple instances. */
    gui_start_vmess("gui_open_via_unique", "xi", pd_this, sys_unique);
    gui_start_array();
    if (sys_openlist)
    {
        // send the files to be opened to the GUI. We send them as an
        // array here so that we don't have to allocate anything here
        // (as the previous API did)
        t_namelist *nl;
        for (nl = sys_openlist; nl; nl = nl->nl_next)
        {
            gui_s(nl->nl_string);
            //fprintf(stderr, "file: %s\n", nl->nl_string);
        }
    }
    gui_end_array();
    gui_end_vmess();
}

extern void glob_recent_files(t_pd *dummy);
extern int sys_browser_doc, sys_browser_path, sys_browser_init;
extern int sys_autocomplete, sys_autocomplete_prefix,
  sys_autocomplete_relevance;

/* this is called from main() in s_entry.c */
int sys_main(int argc, const char **argv)
{
    //fprintf(stderr, "sys_main");
    int i, noprefs, ret;
    t_namelist *nl;
    sys_externalschedlib = 0;
#ifdef PD_DEBUG
    fprintf(stderr, "Pd-L2Ork: COMPILED FOR DEBUGGING\n");
#endif
    /* We need to call WSAStartup regardless of gui mode, since a user
     * might want to make socket connections even in -nogui mode. So we
     * go ahead and do that here. */
#ifdef _WIN32
    short version = MAKEWORD(2, 0);
    WSADATA nobby;
    if (WSAStartup(version, &nobby)) sys_sockerror("WSAstartup");
    /* use Win32 "binary" mode by default since we don't want the
     * translation that Win32 does by default */
# ifdef MSC /* MS Visual Studio */
    _set_fmode( _O_BINARY );
# else  /* MinGW */
    {
#ifndef _fmode
        extern int _fmode;
#endif
        _fmode = _O_BINARY;
    }
# endif /* MSC */
#endif  /* _WIN32 */
#ifndef _WIN32
    /* long ago Pd used setuid to promote itself to real-time priority.
    Just in case anyone's installation script still makes it setuid, we
    complain to stderr and lose setuid here. */
    if (getuid() != geteuid())
    {
        fprintf(stderr, "warning: canceling setuid privilege\n");
        if(setuid(getuid()) < 0) {
                /* sometimes this fails (which, according to 'man 2 setuid' is a
                 * grave security error), in which case we bail out and quit. */
            fprintf(stderr, "\n\nFATAL: could not cancel setuid privilege");
            fprintf(stderr, "\nTo fix this, please remove the setuid flag from the Pd binary");
            if(argc>0) {
                fprintf(stderr, "\ne.g. by running the following as root/superuser:");
                fprintf(stderr, "\n chmod u-s '%s'", argv[0]);
            }
            fprintf(stderr, "\n\n");
            perror("setuid");
            return (1);
        }
    }
#endif  /* _WIN32 */
if (socket_init())
    sys_sockerror("socket_init()");
    pd_init();                                  /* start the message system */
    sys_gui_preset = gensym("default");
    logpost(NULL, 2, "PD_FLOATSIZE = %u bits", (unsigned)sizeof(t_float)*8);
    sys_findprogdir(argv[0]);                   /* set sys_progname, guipath */
    for (i = noprefs = 0; i < argc; i++)        /* prescan ... */
    {
        /* for prefs override */
        if (!strcmp(argv[i], "-noprefs"))
            noprefs = 1;
        /* for external scheduler (to ignore audio api in sys_loadpreferences) */
        else if (!strcmp(argv[i], "-schedlib") && i < argc-1)
            sys_externalschedlib = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help"))
        {
            sys_printusage();
            return (1);
        }
    }
// do not load preferences and recent files if using emscripten
// LATER: distinguish between WebPdL2Ork emscripten and full online editor
#ifndef __EMSCRIPTEN__
    if (!noprefs)
        sys_loadpreferences();                  /* load default settings */
    sys_load_recent_files();                    /* load recent files table */
#endif
#ifndef _WIN32
    if (!noprefs)
        sys_rcfile();                           /* parse the startup file */
#endif
#ifdef __APPLE__
    // Initialize libquicktime for Gem, so that it finds its plugins.
    char *plugins = strdup(argv[0]), *t = strstr(plugins, "/Contents");
    if (t) {
      strncpy(t, "/Contents/Plugins/libquicktime", strlen(t));
      fprintf(stderr, "LIBQUICKTIME_PLUGIN_DIR = %s\n", plugins);
      // We don't override an existing value here, since the user may have
      // already set up her own libquicktime plugin collection.
      setenv("LIBQUICKTIME_PLUGIN_DIR", plugins, 0);
    }
#endif
    if (sys_argparse(argc-1, argv+1))           /* parse cmd line */
        return (1);
        /* build version string from defines in m_pd.h */
    pd_makeversion();
    if (sys_verbose || sys_version) fprintf(stderr, "%scompiled %s %s\n",
        pd_version, pd_compiletime, pd_compiledate);
    if (sys_verbose)
        fprintf(stderr, "float precision = %lu bits\n", sizeof(t_float)*8);
    if (sys_version)    /* if we were just asked our version, exit here. */
    {
        fflush(stderr);
        return (0);
    }
    sys_setsignalhandlers();
    if (sys_nogui)
        clock_set((sys_fakefromguiclk =
            clock_new(0, (t_method)sys_fakefromgui)), 0);
    else if (sys_startgui(sys_libdir->s_name)) /* start the gui */
        return (1);
    sys_afterargparse();                    /* post-argparse settings */
        /* send the libdir to the GUI */
    gui_vmess("gui_set_lib_dir", "s", sys_libdir->s_name);
        /* send the name of the gui preset */
    gui_vmess("gui_set_gui_preset", "s", sys_gui_preset->s_name);
        /* send the recent files list */
    glob_recent_files(0);
        /* AG: send the browser config; this must come *after* gui_set_lib_dir
           so that the lib_dir is available when help indexing starts */
    gui_start_vmess("gui_set_browser_config", "iiiiii",
                    sys_browser_doc, sys_browser_path, sys_browser_init,
                    sys_autocomplete, sys_autocomplete_prefix,
                    sys_autocomplete_relevance);
    gui_start_array();
    for (nl = STUFF->st_helppath; nl; nl = nl->nl_next)
    {
        gui_s(nl->nl_string);
    }
    gui_end_array();
    gui_end_vmess();

    if (sys_hipriority)
        sys_setrealtime(sys_libdir->s_name); /* set desired process priority */
    if (sys_externalschedlib)
        ret = (sys_run_scheduler(sys_externalschedlibname,
            pd_extraflags));
    else if (sys_batch)
        ret = (m_batchmain());
    else
    {
        /* open audio and MIDI */
        sys_reopen_midi();
        if (audio_shouldkeepopen())
            sys_reopen_audio();

        // ag: no longer needed
        //if (sys_console) sys_vgui("pdtk_toggle_console 1\n");
        if (sys_k12_mode)
        {
            /*
            t_namelist *path = STUFF->st_staticpath;
            while (path->nl_next)
                path = path->nl_next;
            */
            gui_vmess("set_k12_mode", "i", sys_k12_mode);
        }
         /* run scheduler until it quits */
        ret = (m_mainloop());
    }
    sys_stopgui();
    pd_term();
    return (ret);
}

static char *(usagemessage[]) = {
"Usage: pd-l2ork [-flags <value>] [file1 file2 ... filen]\n",
"\nAudio configuration flags:\n",
"-r <n>           -- specify sample rate\n",
"-audioindev ...  -- audio in devices; e.g., \"1,3\" for first and third\n",
"-audiooutdev ... -- audio out devices (same)\n",
"-audiodev ...    -- specify input and output together\n",
"-audioaddindev   -- add an audio input device by name\n",
"-audioaddoutdev  -- add an audio output device by name\n",
"-audioadddev     -- add an audio input and output device by name\n",
"-inchannels ...  -- audio input channels (by device, like \"2\" or \"16,8\")\n",
"-outchannels ... -- number of audio out channels (same)\n",
"-channels ...    -- specify both input and output channels\n",
"-audiobuf <n>    -- specify size of audio buffer in msec\n",
"-blocksize <n>   -- specify audio I/O block size in sample frames\n",
"-sleepgrain <n>  -- specify number of milliseconds to sleep when idle\n",
"-nodac           -- suppress audio output\n",
"-noadc           -- suppress audio input\n",
"-noaudio         -- suppress audio input and output (-nosound is synonym) \n",
"-callback        -- use callbacks if possible\n",
"-nocallback      -- use polling-mode (true by default)\n",
"-listdev         -- list audio and MIDI devices\n",

#ifdef USEAPI_OSS
"-oss             -- use OSS audio API\n",
#endif

#ifdef USEAPI_ALSA
"-alsa            -- use ALSA audio API\n",
"-alsaadd <name>  -- add an ALSA device name to list\n",
#endif

#ifdef USEAPI_JACK
"-jack            -- use JACK audio API\n",
"-jackname <name> -- a name for your JACK client\n",
"-nojackconnect   -- don't make any automatic connections to the jack graph\n",
"-jackconnect     -- automatically connect pd to the JACK graph [default]\n",
#endif

#ifdef USEAPI_PORTAUDIO
#ifdef _WIN32
"-pa              -- use Portaudio API (for ASIO or WASAPI)\n",
"-asio            -- synonym for -pa\n",
#else
"-pa              -- use Portaudio API\n",
#endif
#endif

#ifdef USEAPI_MMIO
"-mmio            -- use legacy MMIO audio API\n",
#endif
"      (default audio API for this platform:  ", API_DEFSTRING, ")\n",

"\nMIDI configuration flags:\n",
"-midiindev ...   -- midi in device list; e.g., \"1,3\" for first and third\n",
"-midioutdev ...  -- midi out device list, same format\n",
"-mididev ...     -- specify -midioutdev and -midiindev together\n",
"-midiaddindev    -- add a MIDI input device by name\n",
"-midiaddoutdev   -- add a MIDI output device by name\n",
"-midiadddev      -- add a MIDI input and output device by name\n",
"-nomidiin        -- suppress MIDI input\n",
"-nomidiout       -- suppress MIDI output\n",
"-nomidi          -- suppress MIDI input and output\n",
#ifdef USEAPI_OSS
"-ossmidi         -- use OSS midi API\n",
#endif
#ifdef USEAPI_ALSA
"-alsamidi        -- use ALSA midi API\n",
#endif


"\nOther flags:\n",
"-path <path>     -- add to file search path\n",
"-nostdpath       -- don't search standard (\"extra\") directory\n",
"-stdpath         -- search standard directory (true by default)\n",
"-helppath <path> -- add to help file search path\n",
"-open <file>     -- open file(s) on startup\n",
"-open-with-args <file> <args> -- open file(s) on startup with arguments\n",
"-lib <file>      -- load object library(s) (omit file extensions)\n",
"-font-size <n>      -- specify default font size in points\n",
"-font-face <name>   -- specify default font (default: Bitstream Vera Sans Mono)\n",
"-font-weight <name> -- specify default font weight (normal or bold)\n",
"-verbose         -- extra printout on startup and when searching for files\n",
"-noverbose       -- no extra printout\n",
"-version         -- don't run Pd; just print out which version it is \n",
"-d <n>           -- specify debug type:\n",
"                    1=Pd->GUI\n",
"                    2=GUI->Pd\n",
"                    3=Pd->GUI and GUI->Pd\n",
"                    5=Pd->GUI with Pd linenumbers\n",
"                    7=Pd->GUI and GUI->Pd with Pd linenumbers\n", 
"-loadbang        -- do not suppress all loadbangs (true by default)\n",
"-noloadbang      -- suppress all loadbangs\n",
"-stderr          -- send printout to standard error instead of GUI\n",
"-nostderr        -- send printout to GUI instead of standard error (true by default)\n",
"-gui             -- start GUI (true by default)\n",
"-nogui           -- suppress starting the GUI\n",
"-guiport <n>     -- connect to pre-existing GUI over port <n>\n",
"-guicmd \"cmd...\" -- start alternative GUI program (e.g., remote via ssh)\n",
"-send \"msg...\"   -- send a message at startup, after patches are loaded\n",
"-prefs           -- load preferences on startup (true by default)\n",
"-noprefs         -- suppress loading preferences on startup\n",
"-console         -- open the console along with the pd window\n",
#ifdef HAVE_UNISTD_H
"-rt or -realtime -- use real-time priority\n",
"-nrt             -- don't use real-time priority\n",
#endif
"-sleep           -- sleep when idle, don't spin (true by default)\n",
"-nosleep         -- spin, don't sleep (may lower latency on multi-CPUs)\n",
"-schedlib <file> -- plug in external scheduler (omit file extensions)\n",
"-extraflags <s>  -- string argument to send schedlib\n",
"-batch           -- run off-line as a batch process\n",
"-nobatch         -- run interactively (true by default)\n",
"-autopatch       -- enable auto-patching new from selected objects\n",
"-k12             -- enable K-12 education mode (requires L2Ork K12 lib)\n",
"-unique          -- enable multiple instances (disabled by default)\n",
"-legacy          -- enable legacy features (disabled by default)\n", 
"-legacy-bendin   -- enable legacy (unsigned) bendin (disabled by default)\n", 
"\n",
};

static void sys_printusage(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(usagemessage)/sizeof(*usagemessage); i++)
    {
        fprintf(stderr, "%s", usagemessage[i]);
        fflush(stderr);
    }
}

    /* parse a comma-separated numeric list, returning the number found */
static int sys_parsedevlist(int *np, int *vecp, int max, const char *str)
{
    int n = 0;
    while (n < max)
    {
        if (! *str) break;
        else
        {
            char *endp;
            vecp[n] = (int)strtol(str, &endp, 10);
            if (endp == str)
                break;
            n++;
            if (! *endp)
                break;
            str = endp + 1;
        }
    }
    return (*np = n);
}

/*
static int sys_getmultidevchannels(int n, int *devlist)
{
    int sum = 0;
    if (n<0)return(-1);
    if (n==0)return 0;
    while(n--)sum+=*devlist++;
    return sum;
}
*/

    /* this routine tries to figure out where to find the auxiliary files
    Pd will need to run.  This is either done by looking at the command line
    invocation for Pd, or if that fails, by consulting the variable
    INSTALL_PREFIX.  In MSW, we don't try to use INSTALL_PREFIX. */
void sys_findprogdir(const char *progname)
{
    char *execdir = pd_getdirname()->s_name, *lastslash;
    char sbuf[FILENAME_MAX], sbuf2[FILENAME_MAX], appbuf[FILENAME_MAX];
    strncpy(sbuf, execdir, FILENAME_MAX-1);
#ifndef _WIN32
    struct stat statbuf;
#endif
    lastslash = strrchr(sbuf, '/');
    if (lastslash)
    {
            // bash last slash to zero so that sbuf is directory pd was in,
            //    e.g., ~/pd/bin
        *lastslash = 0;
            // go back to the parent from there, e.g., ~/pd
        lastslash = strrchr(sbuf, '/');
        if (lastslash)
        {
            strncpy(sbuf2, sbuf, lastslash-sbuf);
            sbuf2[lastslash-sbuf] = 0;
        }
        else strcpy(sbuf2, "..");
    }
    else
    {
            /* no slashes found.  Try INSTALL_PREFIX. */
#ifdef INSTALL_PREFIX
        strncpy(sbuf2, INSTALL_PREFIX, FILENAME_MAX-1);
#else
        strcpy(sbuf2, ".");
#endif
    }
        /* now we believe sbuf2 holds the parent directory of the directory
        pd was found in.  We now want to infer the "lib" directory and the
        "gui" directory.  In "simple" unix installations, the layout is
            .../bin/pd
            .../bin/pd-gui
            .../doc
        and in "complicated" unix installations, it's:
            .../bin/pd
            .../lib/pd-l2ork/bin/pd-gui
            .../lib/pd-l2ork/doc
        To decide which, we stat .../lib/pd-l2ork; if that exists, we assume it's
        the complicated layout.  In MSW, it's the "simple" layout, but
        the gui program is straight wish80:
            .../bin/pd
            .../bin/wish80.exe
            .../doc
        */
#ifdef _WIN32
    sys_libdir = gensym(sbuf2);
    sys_guidir = &s_;   /* in MSW the guipath just depends on the libdir */
#else
    strncpy(sbuf, sbuf2, FILENAME_MAX-30);
    sbuf[FILENAME_MAX-30] = 0;
    strcat(sbuf2, "/lib/pd-l2ork");
    if (stat(sbuf2, &statbuf) >= 0)
    {
            /* complicated layout: lib dir is the one we just stat-ed above */
        sys_libdir = gensym(sbuf2);
            /* gui lives in .../lib/pd-l2ork/bin */
        strncpy(sbuf2, sbuf, FILENAME_MAX-30);
        sbuf[FILENAME_MAX-30] = 0;
        strcat(sbuf2, "/lib/pd-l2ork/bin");
        sys_guidir = gensym(sbuf2);
    }
    else
    {
            /* simple layout: lib dir is the parent */
            /* gui lives in .../bin */
        strncpy(sbuf2, sbuf, FILENAME_MAX-30);
        strncpy(appbuf, sbuf, FILENAME_MAX-30);
        sbuf[FILENAME_MAX-30] = 0;
        sys_libdir = gensym(sbuf);
        strcat(sbuf2, "/bin");
        /* special case-- with the OSX app bundle the guidir is actually
           in app.nw instead of app.nw/bin. So we check for a package.json
           there and then set it accordingly. */
        strcat(appbuf, "/package.json");
        if (stat(appbuf, &statbuf) >= 0)
            sys_guidir = gensym(sbuf);
        else
            sys_guidir = gensym(sbuf2);
    }
#endif
}

int sys_argparse(int argc, const char **argv)
{
    t_audiosettings as;
            /* get the current audio parameters.  These are set
            by the preferences mechanism (sys_loadpreferences()) or
            else are the default.  Overwrite them with any results
            of argument parsing, and store them again. */
    sys_get_audio_settings(&as);
    while ((argc > 0) && **argv == '-')
    {
                /* audio flags */
        if (!strcmp(*argv, "-r") && argc > 1 &&
            sscanf(argv[1], "%d", &as.a_srate) >= 1)
        {
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-inchannels"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nchindev,
                    as.a_chindevvec, MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-outchannels") && (argc > 1))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nchoutdev,
                    as.a_choutdevvec, MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-channels") && (argc > 1))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nchindev,
                    as.a_chindevvec, MAXAUDIOINDEV, argv[1]) ||
                !sys_parsedevlist(&as.a_nchoutdev,
                    as.a_choutdevvec, MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-soundbuf") ||
                 !strcmp(*argv, "-audiobuf") && (argc > 1))
        {
            as.a_advance = atoi(argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-callback"))
        {
            as.a_callback = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nocallback"))
        {
            as.a_callback = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-blocksize"))
        {
            as.a_blocksize = atoi(argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-sleepgrain") && (argc > 1))
        {
            sys_sleepgrain = 1000 * atof(argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-nodac"))
        {
            as.a_noutdev = as.a_nchoutdev = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noadc"))
        {
            as.a_nindev = as.a_nchindev = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nosound") || !strcmp(*argv, "-noaudio"))
        {
            as.a_noutdev = as.a_nchoutdev =  as.a_nindev = as.a_nchindev = 0;
            argc--; argv++;
        }
#ifdef USEAPI_OSS
        else if (!strcmp(*argv, "-oss"))
        {
            as.a_api = API_OSS;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-ossmidi"))
        {
            sys_set_midi_api(API_OSS);
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-oss")) || (!strcmp(*argv, "-ossmidi")))
        {
            fprintf(stderr, "Pd compiled without OSS-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
#ifdef USEAPI_ALSA
        else if (!strcmp(*argv, "-alsa"))
        {
            as.a_api = API_ALSA;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-alsaadd"))
        {
            if (argc < 2)
                goto usage;

            as.a_api = API_ALSA;
            alsa_adddev(argv[1]);
            argc -= 2; argv +=2;
        }
        else if (!strcmp(*argv, "-alsamidi"))
        {
          sys_set_midi_api(API_ALSA);
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-alsa")) || (!strcmp(*argv, "-alsamidi")))
        {
            fprintf(stderr, "Pd compiled without ALSA-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-alsaadd"))
        {
            if (argc < 2)
                goto usage;
            fprintf(stderr, "Pd compiled without ALSA-support, ignoring '%s' flag\n", *argv);
            argc -= 2; argv +=2;
        }
#endif
#ifdef USEAPI_JACK
        else if (!strcmp(*argv, "-jack"))
        {
            as.a_api = API_JACK;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-jackname"))
        {
            if (argc < 2)
                goto usage;

            as.a_api = API_JACK;
            jack_client_name(argv[1]);
            argc -= 2; argv +=2;
        }
        else if (!strcmp(*argv, "-nojackconnect"))
        {
            as.a_api = API_JACK;
            jack_autoconnect(0);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-jackconnect"))
        {
            as.a_api = API_JACK;
            jack_autoconnect(1);
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-jack"))
            || (!strcmp(*argv, "-jackconnect"))
            || (!strcmp(*argv, "-nojackconnect")))
        {
            fprintf(stderr, "Pd compiled without JACK-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-jackname"))
        {
            if (argc < 2)
                goto usage;
            fprintf(stderr, "Pd compiled without JACK-support, ignoring '%s' flag\n", *argv);
            argc -= 2; argv +=2;
        }
#endif
#ifdef USEAPI_PORTAUDIO
        else if (!strcmp(*argv, "-pa") || !strcmp(*argv, "-portaudio")
#ifdef _WIN32
            || !strcmp(*argv, "-asio")
#endif
            )
        {
            as.a_api = API_PORTAUDIO;
            argc--; argv++;
        }
#else
        else if ((!strcmp(*argv, "-pa")) || (!strcmp(*argv, "-portaudio")))
        {
            fprintf(stderr, "Pd compiled without PortAudio-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-asio"))
        {
            fprintf(stderr, "Pd compiled without ASIO-support, ignoring '%s' flag\n", *argv);
            argc -= 2; argv +=2;
        }
#endif
#ifdef USEAPI_MMIO
        else if (!strcmp(*argv, "-mmio"))
        {
            as.a_api = API_MMIO;
            argc--; argv++;
        }
#else
        else if (!strcmp(*argv, "-mmio"))
        {
            fprintf(stderr, "Pd compiled without MMIO-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
        else if (!strcmp(*argv, "-listdev"))
        {
            sys_listplease = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-soundindev") ||
            !strcmp(*argv, "-audioindev"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nindev, as.a_indevvec,
                    MAXAUDIOINDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-soundoutdev") ||
            !strcmp(*argv, "-audiooutdev"))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_noutdev, as.a_outdevvec,
                    MAXAUDIOOUTDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if ((!strcmp(*argv, "-sounddev") || !strcmp(*argv, "-audiodev")))
        {
            if (argc < 2 ||
                !sys_parsedevlist(&as.a_nindev, as.a_indevvec,
                    MAXAUDIOINDEV, argv[1]) ||
                !sys_parsedevlist(&as.a_noutdev, as.a_outdevvec,
                    MAXAUDIOOUTDEV, argv[1]))
                        goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-audioaddindev"))
        {
            if (argc < 2)
                goto usage;

            if (as.a_nindev < 0)
                as.a_nindev = 0;
            if (as.a_nindev < MAXAUDIOINDEV)
            {
                int devn = sys_audiodevnametonumber(0, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio input device: %s\n",
                        argv[1]);
                else as.a_indevvec[as.a_nindev++] = devn + 1;
            }
            else fprintf(stderr, "number of audio devices limited to %d\n",
                MAXAUDIOINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-audioaddoutdev"))
        {
            if (argc < 2)
                goto usage;

            if (as.a_noutdev < 0)
                as.a_noutdev = 0;
            if (as.a_noutdev < MAXAUDIOINDEV)
            {
                int devn = sys_audiodevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio output device: %s\n",
                        argv[1]);
                else as.a_outdevvec[as.a_noutdev++] = devn + 1;
            }
            else fprintf(stderr, "number of audio devices limited to %d\n",
                MAXAUDIOINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-audioadddev"))
        {
            if (argc < 2)
                goto usage;

            if (as.a_nindev < 0)
                as.a_nindev = 0;
            if (as.a_noutdev < 0)
                as.a_noutdev = 0;
            if (as.a_nindev < MAXAUDIOINDEV && as.a_noutdev < MAXAUDIOINDEV)
            {
                int devn = sys_audiodevnametonumber(0, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio input device: %s\n",
                        argv[1]);
                else as.a_indevvec[as.a_nindev++] = devn + 1;
                devn = sys_audiodevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find audio output device: %s\n",
                        argv[1]);
                else as.a_outdevvec[as.a_noutdev++] = devn + 1;
            }
            else fprintf(stderr, "number of audio devices limited to %d",
                MAXAUDIOINDEV);
            argc -= 2; argv += 2;
        }
                /* MIDI flags */
        else if (!strcmp(*argv, "-nomidiin"))
        {
            sys_nmidiin = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nomidiout"))
        {
            sys_nmidiout = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nomidi"))
        {
            sys_nmidiin = sys_nmidiout = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-midiindev") && (argc > 1))
        {
            sys_parsedevlist(&sys_nmidiin, sys_midiindevlist, MAXMIDIINDEV,
                argv[1]);
            if (!sys_nmidiin)
                goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midioutdev") && (argc > 1))
        {
            sys_parsedevlist(&sys_nmidiout, sys_midioutdevlist, MAXMIDIOUTDEV,
                argv[1]);
            if (!sys_nmidiout)
                goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-mididev") && (argc > 1))
        {
            sys_parsedevlist(&sys_nmidiin, sys_midiindevlist, MAXMIDIINDEV,
                argv[1]);
            sys_parsedevlist(&sys_nmidiout, sys_midioutdevlist, MAXMIDIOUTDEV,
                argv[1]);
            if (!sys_nmidiout)
                goto usage;
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midiaddindev") && (argc > 1))
        {
            if (sys_nmidiin < 0)
                sys_nmidiin = 0;
            if (sys_nmidiin < MAXMIDIINDEV)
            {
                int devn = sys_mididevnametonumber(0, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI input device: %s\n",
                        argv[1]);
                else sys_midiindevlist[sys_nmidiin++] = devn + 1;
            }
            else fprintf(stderr, "number of MIDI devices limited to %d\n",
                MAXMIDIINDEV);
            argc -= 2; argv += 2;
        }
                else if (!strcmp(*argv, "-midiaddoutdev") && (argc > 1))
        {
            if (sys_nmidiout < 0)
                sys_nmidiout = 0;
            if (sys_nmidiout < MAXMIDIINDEV)
            {
                int devn = sys_mididevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI output device: %s\n",
                        argv[1]);
                else sys_midioutdevlist[sys_nmidiin++] = devn + 1;
            }
            else fprintf(stderr, "number of MIDI devices limited to %d\n",
                MAXMIDIINDEV);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-midiadddev") && (argc > 1))
        {
            if (sys_nmidiin < 0)
                sys_nmidiin = 0;
            if (sys_nmidiout < 0)
                sys_nmidiout = 0;
            if (sys_nmidiin < MAXMIDIINDEV && sys_nmidiout < MAXMIDIINDEV)
            {
                int devn = sys_mididevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI output device: %s\n",
                        argv[1]);
                else sys_midioutdevlist[sys_nmidiin++] = devn + 1;
                devn = sys_mididevnametonumber(1, argv[1]);
                if (devn < 0)
                    fprintf(stderr, "Couldn't find MIDI output device: %s\n",
                        argv[1]);
                else sys_midioutdevlist[sys_nmidiin++] = devn + 1;
            }
            else fprintf(stderr, "number of MIDI devices limited to %d\n",
                MAXMIDIINDEV);
            argc -= 2; argv += 2;
        }
                /* other flags */
        else if (!strcmp(*argv, "-path") && (argc > 1))
        {
            STUFF->st_temppath = namelist_append_files(STUFF->st_temppath, argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-nostdpath"))
        {
            sys_usestdpath = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-stdpath"))
        {
            sys_usestdpath = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-helppath"))
        {
            STUFF->st_helppath = namelist_append_files(STUFF->st_helppath, argv[1]);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-open") && argc > 1)
        {
            sys_openlist = patchlist_append(sys_openlist, argv[1], 0);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-open-with-args"))
        {
            if (argc < 3)
                goto usage;
            sys_openlist = patchlist_append(sys_openlist, argv[1], argv[2]);
            argc -= 3; argv += 3;
        }
        else if (!strcmp(*argv, "-lib") && argc > 1)
        {
            STUFF->st_externlist = namelist_append_files(STUFF->st_externlist, argv[1]);
            argc -= 2; argv += 2;
        }
        else if ((!strcmp(*argv, "-font-size") || !strcmp(*argv, "-font"))
            && argc > 1)
        {
            sys_defaultfont = sys_nearestfontsize(atoi(argv[1]));
            argc -= 2;
            argv += 2;
        }
        else if ((!strcmp(*argv, "-font-face") || !strcmp(*argv, "-typeface"))
            && argc > 1)
        {
            strncpy(sys_font,*(argv+1),sizeof(sys_font)-1);
            sys_font[sizeof(sys_font)-1] = 0;
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-font-weight") && argc > 1)
        {
            strncpy(sys_fontweight,*(argv+1),sizeof(sys_fontweight)-1);
            sys_fontweight[sizeof(sys_fontweight)-1] = 0;
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-verbose"))
        {
            sys_verbose++;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noverbose"))
        {
            sys_verbose=0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-version"))
        {
            sys_version = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-d") && argc > 1 &&
            sscanf(argv[1], "%d", &sys_debuglevel) >= 1)
        {
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-loadbang"))
        {
            sys_noloadbang = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noloadbang"))
        {
            sys_noloadbang = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-gui"))
        {
            sys_nogui = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nogui"))
        {
            sys_nogui = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-console"))
        {
            sys_console = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-k12"))
        {
            sys_k12_mode = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-unique"))
        {
            sys_unique = 1;
            argc -= 1;
            argv += 1;
        }
        else if (!strcmp(*argv, "-legacy"))
        {
            sys_legacy = 1;
            argc -= 1;
            argv += 1;
        }
        else if (!strcmp(*argv, "-legacy-bendin"))
        {
            sys_legacy_bendin = 1;
            argc -= 1;
            argv += 1;
        }
        else if (!strcmp(*argv, "-guiport") && argc > 1 &&
            sscanf(argv[1], "%d", &sys_guisetportnumber) >= 1)
        {
            argc -= 2;
            argv += 2;
        }
        else if (!strcmp(*argv, "-nostderr"))
        {
            sys_printtostderr = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-stderr"))
        {
            sys_printtostderr = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-guicmd") && argc > 1)
        {
            sys_guicmd = argv[1];
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-send") && argc > 1)
        {
            sys_messagelist = namelist_append(sys_messagelist, argv[1], 1);
            argc -= 2; argv += 2;
        }
        else if (!strcmp(*argv, "-schedlib") && argc > 1)
        {
            sys_externalschedlib = 1;
            strncpy(sys_externalschedlibname, argv[1],
                sizeof(sys_externalschedlibname) - 1);
#ifndef  __APPLE__
                /* no audio I/O please, unless overwritten by later args.
                This is to circumvent a problem running pd~ subprocesses
                with -nogui; they would open an audio device before pdsched.c
                could set the API to nothing.  For some reason though, on
                MACOSX this causes Pd-L2Orkto switch to JACK so we just give up
                and suppress the workaround there. */
            as.a_api = 0;
#endif
            argv += 2;
            argc -= 2;
        }
        else if (!strcmp(*argv, "-extraflags") && argc > 1)
        {
            pd_extraflags = argv[1];
            argv += 2;
            argc -= 2;
        }
        else if (!strcmp(*argv, "-batch"))
        {
            sys_batch = 1;
            sys_printtostderr = sys_nogui = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nobatch"))
        {
            sys_batch = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noautopatch"))
        {
            sys_noautopatch = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-autopatch"))
        {
            sys_noautopatch = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-compatibility") && argc > 1)
        {
            float f;
            if (sscanf(argv[1], "%f", &f) < 1)
                goto usage;
            pd_compatibilitylevel = 0.5 + 100. * f; /* e.g., 2.44 --> 244 */
            argv += 2;
            argc -= 2;
        }
#ifdef HAVE_UNISTD_H
        else if (!strcmp(*argv, "-rt") || !strcmp(*argv, "-realtime"))
        {
            sys_hipriority = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nrt") || !strcmp(*argv, "-nort") || !strcmp(*argv, "-norealtime"))
        {
            sys_hipriority = 0;
            argc--; argv++;
        }
#else
        else if (!strcmp(*argv, "-rt") || !strcmp(*argv, "-realtime")
                 || !strcmp(*argv, "-nrt") || !strcmp(*argv, "-nort")
                 || !strcmp(*argv, "-norealtime"))
        {
            fprintf(stderr, "Pd-L2Ork compiled without realtime priority-support, ignoring '%s' flag\n", *argv);
            argc--; argv++;
        }
#endif
        else if (!strcmp(*argv, "-sleep"))
        {
            sys_nosleep = 0;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-nosleep"))
        {
            sys_nosleep = 1;
            argc--; argv++;
        }
        else if (!strcmp(*argv, "-noprefs")) /* did this earlier */
            argc--, argv++;
        else
        {
        usage:
            sys_printusage();
            return (1);
        }
    }
    if (sys_listplease && !sys_printtostderr)
    {
        // if we asked to list devices and are not using stderr output
        // open console to facilitate understanding where devices have 
        // been listed
        sys_console = 1;
    }
    if (sys_batch)  /* if batch, turn off gui, real-time, audio and MIDI */
    {
        sys_nogui = 1;
        sys_hipriority = 0;
        as.a_noutdev = as.a_nchoutdev = as.a_nindev = as.a_nchindev = 0;
        sys_nmidiin = sys_nmidiout = 0;
    }
    if (sys_nogui)
        sys_printtostderr = 1;
#ifdef _WIN32
    if (sys_printtostderr)
        /* we need to tell Windows to output UTF-8 */
        SetConsoleOutputCP(CP_UTF8);
#endif
    if (!sys_defaultfont)
        sys_defaultfont = DEFAULTFONT;
    for (; argc > 0; argc--, argv++) 
        sys_openlist = patchlist_append(sys_openlist, *argv, 0);

    sys_set_audio_settings(&as);
    return (0);
}

int sys_getblksize(void)
{
    return (DEFDACBLKSIZE);
}

void sys_init_audio(void);

    /* stuff to do, once, after calling sys_argparse() -- which may itself
    be called more than once (first from "settings, second from .pdrc, then
    from command-line arguments */
static void sys_afterargparse(void)
{
    char sbuf[MAXPDSTRING];
    int i;
    t_audiosettings as;
    int nmidiindev = 0, midiindev[MAXMIDIINDEV];
    int nmidioutdev = 0, midioutdev[MAXMIDIOUTDEV];
            /* add "extra" library to path */
    strncpy(sbuf, sys_libdir->s_name, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/extra");
    sys_setextrapath(sbuf);
            /* add "doc/5.reference" library to helppath */
    strncpy(sbuf, sys_libdir->s_name, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/doc/5.reference");
    STUFF->st_helppath = namelist_append_files(STUFF->st_helppath, sbuf);

    for (i = 0; i < sys_nmidiin; i++)
        sys_midiindevlist[i]--;
    for (i = 0; i < sys_nmidiout; i++)
        sys_midioutdevlist[i]--;

    if (sys_listplease)
        sys_listdevs();
        
    sys_get_midi_params(&nmidiindev, midiindev, &nmidioutdev, midioutdev);
    if (sys_nmidiin >= 0)
    {
        nmidiindev = sys_nmidiin;
        for (i = 0; i < nmidiindev; i++)
            midiindev[i] = sys_midiindevlist[i];
    }
    if (sys_nmidiout >= 0)
    {
        nmidioutdev = sys_nmidiout;
        for (i = 0; i < nmidioutdev; i++)
            midioutdev[i] = sys_midioutdevlist[i];
    }
    sys_open_midi(nmidiindev, midiindev, nmidioutdev, midioutdev, 0);
    
    sys_init_audio();
}

//static void sys_addreferencepath(void)
//{
//    char sbuf[MAXPDSTRING];
//}
