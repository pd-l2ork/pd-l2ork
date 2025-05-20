/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "s_stuff.h"
#include "m_private_utils.h"

t_printhook sys_printhook = NULL;
t_printhook sys_printhook_error = NULL;
int sys_printtostderr;

#ifdef _WIN32

    /* NB: Unlike vsnprintf(), _vsnprintf() does *not* null-terminate
    the output if the resulting string is too large to fit into the buffer.
    Also, it just returns -1 instead of the required number of bytes.
    Strictly speaking, the UCRT in Windows 10 actually contains a standard-
    conforming vsnprintf() function that is not just an alias for _vsnprintf().
    However, MinGW traditionally links against the old msvcrt.dll runtime library.
    Recent versions of MinGW seem to have their own (standard-conformating)
    implementation of vsnprintf(), but to ensure portability we rather use our
    own implementation for all Windows builds. */
int pd_vsnprintf(char *buf, size_t size, const char *fmt, va_list argptr)
{
    int ret = _vsnprintf(buf, size, fmt, argptr);
    if (ret < 0)
    {
            /* null-terminate the buffer and get the required number of bytes. */
        ret = _vscprintf(fmt, argptr);
        buf[size - 1] = '\0';
    }
    return ret;
}

int pd_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = pd_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

#else

int pd_vsnprintf(char *buf, size_t size, const char *fmt, va_list argptr)
{
    return vsnprintf(buf, size, fmt, argptr);
}

int pd_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

#endif

/* escape characters for tcl/tk */
static char* strnescape(char *dest, const char *src, size_t len)
{
    int ptin = 0;
    unsigned ptout = 0;
    for(; ptout < len; ptin++, ptout++)
    {
        int c = src[ptin];
        if (c == '\\' || c == '{' || c == '}' || c == ';' || c == '[' || c == ']')
            dest[ptout++] = '\\';
        dest[ptout] = src[ptin];
        if (c==0) break;
    }

    if(ptout < len)
        dest[ptout]=0;
    else
        dest[len-1]=0;

    return dest;
}

static char* strnpointerid(char *dest, const void *pointer, size_t len)
{
    *dest=0;
    if (pointer)
        snprintf(dest, len, ".x%zx", (t_uint)pointer);
    return dest;
}

static void doerror(const void *object, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

    if (STUFF->st_printhook || STUFF->st_printhook_error)
    {
        pd_snprintf(upbuf, MAXPDSTRING-1, "error: %s", s);
        if (STUFF->st_printhook_error)
            (*STUFF->st_printhook_error)(upbuf);
        if (STUFF->st_printhook)
            (*STUFF->st_printhook)(upbuf);
    }
    else if (sys_printtostderr || !sys_havegui())
    {
#ifdef _WIN32
    #ifdef _MSC_VER
        fwprintf(stderr, L"error: %S", s);
    #else
        fwprintf(stderr, L"error: %s", s);
    #endif
        fflush(stderr);
#else
        fprintf(stderr, "error: %s", s);
#endif
    }
    else
    {
        char obuf[MAXPDSTRING];
        //sys_vgui("pdtk_posterror {%s} 1 {%s}\n",
        //    strnpointerid(obuf, object, MAXPDSTRING),
        //    strnescape(upbuf, s, MAXPDSTRING));
        gui_vmess("gui_post_error", "sis",
            strnpointerid(obuf, object, MAXPDSTRING),
            1,
            strnescape(upbuf, s, MAXPDSTRING));
    }
}

static void dologpost(const void *object, int level, const char *s)
{
    char upbuf[MAXPDSTRING];
    upbuf[MAXPDSTRING-1]=0;

            /* if it's a verbose message and we aren't set to 'verbose' just do
            nothing */
    if (level >= PD_VERBOSE && !sys_verbose)
        return;
    // what about sys_printhook_verbose ?
    if (STUFF->st_printhook) 
    {
        pd_snprintf(upbuf, MAXPDSTRING-1, "verbose(%d): %s", level, s);
        (*STUFF->st_printhook)(upbuf);
    }
    else if (sys_printtostderr || !sys_havegui()) 
    {
#ifdef _WIN32
    #ifdef _MSC_VER
        fwprintf(stderr, L"verbose(%d): %S", level, s);
    #else
        fwprintf(stderr, L"verbose(%d): %s", level, s);
    #endif
        fflush(stderr);
#else
        fprintf(stderr, "verbose(%d): %s", level, s);
#endif
    }
    else
    {
        //sys_vgui("::pdwindow::logpost {%s} %d {%s}\n", 
                 //strnpointerid(obuf, object, MAXPDSTRING), 
                 //level, strnescape(upbuf, s, MAXPDSTRING));
        //sys_vgui("pdtk_post {%s}\n", 
        //         strnescape(upbuf, s, MAXPDSTRING));
        gui_vmess("gui_post", "s",
                 strnescape(upbuf, s, MAXPDSTRING));
    }
}

static void __dopost(const char *s)
{
    if (STUFF->st_printhook)
        (*STUFF->st_printhook)(s);
    else if (sys_printtostderr || !sys_havegui())
    {
#ifdef _WIN32
    #ifdef _MSC_VER
        fwprintf(stderr, L"%S", s);
    #else
        fwprintf(stderr, L"%s", s);
    #endif
        fflush(stderr);
#else
        fprintf(stderr, "%s", s);
#endif
    }
    else
    {
        char upbuf[MAXPDSTRING];
        int ptin = 0, ptout = 0, len = strlen(s);
        //static int heldcr = 0;
        //if (heldcr)
        //    upbuf[ptout++] = '\n', heldcr = 0;
        for (; ptin < len && ptout < MAXPDSTRING-3;
            ptin++, ptout++)
        {
            int c = s[ptin];
            if (c == '\\' || c == '{' || c == '}' || c == ';')
                upbuf[ptout++] = '\\';
            upbuf[ptout] = s[ptin];
        }
        //if (ptout && upbuf[ptout-1] == '\n')
        //    upbuf[--ptout] = 0, heldcr = 1;
        upbuf[ptout] = 0;
//        sys_vgui("pdtk_post {%s}\n", upbuf);
        gui_vmess("gui_post", "s", upbuf);
    }
}

#ifdef __EMSCRIPTEN__
#include <emscripten/threading_legacy.h>
static void dopost(const char *s)
{
    emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VI, __dopost, s);
}
#else
#define dopost __dopost
#endif

void logpost(const void *object, const int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    dologpost(object, level, buf);
}

void startlogpost(const void *object, const int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    if (level > PD_DEBUG && !sys_verbose) return;
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);

    dologpost(object, level, buf);
}

void post(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    dopost(buf);
}

void startpost(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    dopost(buf);
}

void poststring(const char *s)
{
    dopost(" ");
    dopost(s);
}

void postatom(int argc, const t_atom *argv)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        char buf[MAXPDSTRING];
        atom_string(argv+i, buf, MAXPDSTRING);
        poststring(buf);
    }
}

void postfloat(t_float f)
{
    t_atom a;
    SETFLOAT(&a, f);
    postatom(1, &a);
}

void endpost(void)
{
    dopost("\n");
}

  /* keep this in the Pd app for binary extern compatibility but don't
  include in libpd because it conflicts with the posix pd_error(0, ) function. */
#ifdef PD_INTERNAL
EXTERN void error(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    doerror(NULL, buf);
}
#endif

/* deprecated in favor of logpost() */
void verbose(int level, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    if(level>sys_verbose)return;
    
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

        /* log levels for verbose() traditionally start at -3,
        so we have to adjust it before passing it on to dologpost() */
    dologpost(NULL, level + 3, buf);
}

    /* here's the good way to log errors -- keep a pointer to the
    offending or offended object around so the user can search for it
    later. */

static const void *error_object;
static char error_string[256];
void canvas_finderror(const void *object);

void pd_error(const void *object, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    static int saidit = 0;

    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");

    doerror(object, buf);

    if(object)
        error_object = object;
    if (object && !saidit)
    {
        /* move this to a function in the GUI so that we can change the
           message without having to recompile */
        post("... click the link above to track it down, or click the 'Find Last Error' item in the Edit menu.");
        saidit = 1;
    }
}

void glob_finderror(t_pd *dummy)
{
    if (!error_object)
        post("no findable error yet");
    else
    {
        post("last trackable error:");
        post("%s", error_string);
        canvas_finderror(error_object);
    }
}

void glob_findinstance(t_pd *dummy, t_symbol*s)
{
    // revert s to (potential) pointer to object
    t_int obj = 0;
    const char*addr;
    int result = 0;
    if(!s || !s->s_name)
        return;
    addr = s->s_name;
    if (!result)
        result = sscanf(addr, PDGUI_FORMAT__OBJECT, &obj);
    if (!result && (('.' == addr[0]) || ('0' == addr[0])))
        result = sscanf(addr+1, "x%lx", &obj);
    if (!result)
        return;

    if(!obj)
        return;

    canvas_finderror((void *)obj);
}

void bug(const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    dopost("consistency check failed: ");
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    strcat(buf, "\n");
    dopost(buf);
}

    /* don't use these.  They're included for binary compatibility with
    old externs but never worked and now do nothing. */
void sys_logerror(const char *object, const char *s) {}
void sys_unixerror(const char *object) {}
void sys_ouch(void) {}