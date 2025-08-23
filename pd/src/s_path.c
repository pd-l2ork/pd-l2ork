/* Copyright (c) 1999 Guenter Geiger and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*
 * This file implements the loader for linux, which includes
 * a little bit of path handling.
 *
 * Generalized by MSP to provide an open_via_path function
 * and lists of files for all purposes.
 */ 

/* #define DEBUG(x) x */
#define DEBUG(x)

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/stat.h>
#endif
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#include <string.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "s_utf8.h"
#include <stdio.h>
#include <fcntl.h>

#ifdef _LARGEFILE64_SOURCE
# define open  open64
# define lseek lseek64
# define fstat fstat64
# define stat  stat64
#endif

#include "m_private_utils.h"

    /* change '/' characters to the system's native file separator */
void sys_bashfilename(const char *from, char *to)
{
    char c;
    while ((c = *from++))
    {
#ifdef _WIN32
        if (c == '/') c = '\\';
#endif
        *to++ = c;
    }
    *to = 0;
}

    /* change the system's native file separator to '/' characters  */
void sys_unbashfilename(const char *from, char *to)
{
    char c;
    while((c = *from++))
    {
#ifdef _WIN32
        if (c == '\\') c = '/';
#endif
        *to++ = c;
    }
    *to = 0;
}

/* expand special tags inside path that start with @ */

// utility function to expand paths (see sys_expandpathelems call below for more info)

static void sys_path_replace(
    char const * const original,
    char * returned, 
    char const * const pattern, 
    char const * const replacement
) {
    size_t const replen = strlen(replacement);
    size_t const patlen = strlen(pattern);

    size_t patcnt = 0;
    const char * oriptr;
    const char * patloc;

    // find how many times the pattern occurs in the original string
    for (oriptr = original; patloc = strstr(oriptr, pattern); oriptr = patloc + patlen)
    {
        patcnt++;
    }

    {
        // allocate memory for the new string
        if (returned != NULL)
        {
            // copy the original string, 
            // replacing all the instances of the pattern
            char *retptr = returned;
            for (oriptr = original; patloc = strstr(oriptr, pattern); oriptr = patloc + patlen)
            {
                size_t const skplen = patloc - oriptr;
                // copy the section until the occurence of the pattern
                strncpy(retptr, oriptr, skplen);
                retptr += skplen;
                // copy the replacement 
                strncpy(retptr, replacement, replen);
                retptr += replen;
            }
            // copy the rest of the string.
            strcpy(retptr, oriptr);
        }
    }
}

/* expand env vars and ~ at the beginning of a path and make a copy to return */
void sys_expandpath(const char *from, char *to, int bufsize)
{
    if ((strlen(from) == 1 && from[0] == '~') || (strncmp(from,"~/", 2) == 0))
    {
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
#else
        const char *home = getenv("HOME");
#endif
        if(home) 
        {
            strncpy(to, home, bufsize);
            to[bufsize-1] = 0;
            strncpy(to + strlen(to), from + 1, bufsize - strlen(to));
            to[bufsize-1] = 0;
        }
        else *to = 0;
    }
    else
    {
        strncpy(to, from, bufsize);
        to[bufsize-1] = 0;
    }
#ifdef _WIN32
    {
        char *buf = alloca(bufsize);
        ExpandEnvironmentStrings(to, buf, bufsize-1);
        buf[bufsize-1] = 0;
        strncpy(to, buf, bufsize);
        to[bufsize-1] = 0;
    }
#endif    
}


/* used for expanding paths for various objects */
void sys_expandpathelems(const char *name, char *result)
{
    //check for expandable elements in path (e.g. @pd_extra, ~/) and replace
    //fprintf(stderr,"sys_expandpathelems name=<%s>\n", name);
    char interim[FILENAME_MAX];
    if (strstr(name, "@pd_extra") != NULL)
    {
        t_namelist *path = STUFF->st_staticpath;
        while (path->nl_next)
            path = path->nl_next;
        sys_path_replace(name, interim, "@pd_extra", path->nl_string);
        //fprintf(stderr,"path->nl_string=<%s>\n", path->nl_string);
    }
    else if (strstr(name, "@pd_help") != NULL)
    {
        t_namelist *path = STUFF->st_helppath;
        while (path->nl_next)
            path = path->nl_next;
        sys_path_replace(name, interim, "@pd_help", path->nl_string);
        //fprintf(stderr,"path->nl_string=<%s>\n", path->nl_string);
    }
    else
    {
        strcpy(interim, name);
    }
    //fprintf(stderr,"sys_expandpathelems interim=<%s>\n", interim);
    sys_expandpath(interim, result, FILENAME_MAX);
    //fprintf(stderr,"sys_expandpathelems result=<%s>\n", result);
}

/* test if path is absolute or relative, based on leading /, env vars, ~, etc */
int sys_isabsolutepath(const char *dir)
{
    if (dir[0] == '/' || dir[0] == '~'
#ifdef _WIN32
        || dir[0] == '%' || (dir[1] == ':' && dir[2] == '/')
#endif
        )
    {
        return 1;
    }
    else
    {
        return 0;            
    }
}

int sys_relativizepath(const char *from, const char *to, char *result)
// precond: from and to fit into char[MAXPDSTRING]
// postcond: result will fit into char[MAXPDSTRING]
{
    char fromext[MAXPDSTRING];
    sys_unbashfilename(from, fromext);
    char toext[MAXPDSTRING];
    sys_unbashfilename(to, toext);
    // this will be large enough to hold the result in any case (FLW)
    char buf[4*MAXPDSTRING];

    memset(buf, '\0', 4*MAXPDSTRING);

    int i = 0, j;
    while(fromext[i] && toext[i] && fromext[i] == toext[i]) i++;
    if(!i) return 0;

    j = i;
    if(fromext[i])
        while(i > 0 && fromext[i] != '/') i--;
    if(toext[j])
        while(j > 0 && toext[j] != '/') j--;

    if(fromext[i])
    {
        int k = 0;
        while(fromext[i])
        {
            if(fromext[i] == '/')
            {
                if(k == 0)
                {
                    strcpy(buf+k, "..");
                    k += 2;
                }
                else
                {
                    strcpy(buf+k, "/..");
                    k += 3;
                }
            }
            i++;
        }
        if(toext[j])
        {
            buf[k] = '/';
            strcpy(buf+k+1, toext+j+1);
        }
        strncpy(result, buf, MAXPDSTRING);
        result[MAXPDSTRING-1] = '\0';
    }
    else if(!fromext[i] && toext[j])
    {
        strcpy(result, toext+j+1);
    }
    else
    {
        strcpy(result, "");
    }
    return 1;
}


/*******************  Utility functions used below ******************/

/*!
 * \brief copy until delimiter
 * 
 * \arg to destination buffer
 * \arg to_len destination buffer length
 * \arg from source buffer
 * \arg delim string delimiter to stop copying on
 *
 * \return position after delimiter in string.  If it was the last
 *         substring, return NULL.
 */
static const char *strtokcpy(char *to, size_t to_len, const char *from, char delim)
{
    unsigned int i = 0;

        for (; i < (to_len - 1) && from[i] && from[i] != delim; i++)
                to[i] = from[i];
        to[i] = '\0';

        if (i && from[i] != '\0')
                return from + i + 1;

        return NULL;
}

/* add a single item to a namelist.  If "allowdup" is true, duplicates
may be added; otherwise they're dropped.  */

t_namelist *namelist_append(t_namelist *listwas, const char *s, int allowdup)
{
    t_namelist *nl, *nl2;
    nl2 = (t_namelist *)(getbytes(sizeof(*nl)));
    nl2->nl_next = 0;
    nl2->nl_string = (char *)getbytes(strlen(s) + 1);
    strcpy(nl2->nl_string, s);
    sys_unbashfilename(nl2->nl_string, nl2->nl_string);
    if (!listwas)
        return (nl2);
    else
    {
        for (nl = listwas; ;)
        {
            if (!allowdup && !strcmp(nl->nl_string, s))
            {
                freebytes(nl2->nl_string, strlen(nl2->nl_string) + 1);
                return (listwas);
            }
            if (!nl->nl_next)
                break;
            nl = nl->nl_next;
        }
        nl->nl_next = nl2;
    }
    return (listwas);
}

/* add a colon-separated list of names to a namelist */

#ifdef _WIN32
#define SEPARATOR ';'   /* in MSW the natural separator is semicolon instead */
#else
#define SEPARATOR ':'
#endif

t_namelist *namelist_append_files(t_namelist *listwas, const char *s)
{
    const char *npos;
    char temp[MAXPDSTRING];
    t_namelist *nl = listwas;
    
    npos = s;
    do
    {
        npos = strtokcpy(temp, sizeof(temp), npos, SEPARATOR);
        if (! *temp) continue;
        nl = namelist_append(nl, temp, 0);
    }
        while (npos);
    return (nl);
}

void namelist_free(t_namelist *listwas)
{
    t_namelist *nl, *nl2;
    for (nl = listwas; nl; nl = nl2)
    {
        nl2 = nl->nl_next;
        t_freebytes(nl->nl_string, strlen(nl->nl_string) + 1);
        t_freebytes(nl, sizeof(*nl));
    }
}

const char *namelist_get(const t_namelist *namelist, int n)
{
    int i;
    const t_namelist *nl;
    for (i = 0, nl = namelist; i < n && nl; i++, nl = nl->nl_next)
        ;
    return (nl ? nl->nl_string : 0);
}

int sys_usestdpath = 1;

void sys_setextrapath(const char *p)
{
    char pathbuf[MAXPDSTRING];
    namelist_free(STUFF->st_staticpath);
    /* add standard place for users to install stuff first */
#ifdef __gnu_linux__
    sys_expandpath("~/.local/lib/pd-l2ork/extra/", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(0, pathbuf, 0);
    sys_expandpath("~/pd-l2ork-externals", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, pathbuf, 0);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath,
        "/usr/local/lib/pd-l2ork-externals", 0);
#endif

#ifdef __APPLE__
    sys_expandpath("~/Library/Pd-l2ork", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(0, pathbuf, 0);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, "/Library/Pd-l2ork", 0);
#endif

#ifdef _WIN32
    sys_expandpath("%AppData%/Pd-l2ork", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(0, pathbuf, 0);
    sys_expandpath("%CommonProgramFiles%/Pd-l2ork", pathbuf, MAXPDSTRING);
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, pathbuf, 0);
#endif
    /* add built-in "extra" path last so its checked last */
    STUFF->st_staticpath = namelist_append(STUFF->st_staticpath, p, 0);
}

    /* try to open a file in the directory "dir", named "name""ext",
    for reading.  "Name" may have slashes.  The directory is copied to
    "dirresult" which must be at least "size" bytes.  "nameresult" is set
    to point to the filename (copied elsewhere into the same buffer). 
    The "bin" flag requests opening for binary (which only makes a difference
    on Windows). */

int sys_trytoopenit(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, int okgui)
{
    int fd;
    char buf[MAXPDSTRING];
    if (strlen(dir) + strlen(name) + strlen(ext) + 4 > size)
        return (-1);
    sys_expandpath(dir, buf, MAXPDSTRING);
    strcpy(dirresult, buf);
    if (*dirresult && dirresult[strlen(dirresult)-1] != '/')
        strcat(dirresult, "/");
    strcat(dirresult, name);
    strcat(dirresult, ext);

    DEBUG(post("looking for %s",dirresult));
        /* see if we can open the file for reading */
    if ((fd = sys_open(dirresult,O_RDONLY)) >= 0)
    {
            /* in unix, further check that it's not a directory */
#ifdef HAVE_UNISTD_H
        struct stat statbuf;
        int ok =  ((fstat(fd, &statbuf) >= 0) &&
            !S_ISDIR(statbuf.st_mode));
        if (!ok)
        {
            if (okgui && sys_verbose)
                post("tried %s; stat failed or directory",
                    dirresult);
            close (fd);
            fd = -1;
        }
        else
#endif
        {
            char *slash;
            if (okgui && sys_verbose) post("tried %s and succeeded", dirresult);
            sys_unbashfilename(dirresult, dirresult);
            slash = strrchr(dirresult, '/');
            if (slash)
            {
                *slash = 0;
                *nameresult = slash + 1;
            }
            else *nameresult = dirresult;

            return (fd);  
        }
    }
    else
    {
        if (okgui && sys_verbose) post("tried %s and failed", dirresult);
    }
    return (-1);
}

EXTERN int sys_trytoopenone(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin)
{
    // if (PD_VERSION_CODE >= PD_VERSION(0, 56, 0))
    // {
    //     static int warned = 0;
    //     if (!warned)
    //         pd_error(0,
    // "obsolete call to sys_trytoopenone(): some extern needs an update");
    //     warned = 1;
    // }
    return (sys_trytoopenit(dir, name, ext, dirresult, nameresult, size, bin,
        1));
}

    /* check if we were given an absolute pathname, if so try to open it
    and return 1 to signal the caller to cancel any path searches */
int sys_open_absolute(const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin, int *fdp, int okgui)
{
    if (sys_isabsolutepath(name))
    {
        char dirbuf[FILENAME_MAX], *z = strrchr(name, '/');
        int dirlen;
        if (!z)
            return (0);
        dirlen = (int)(z - name);
        if (dirlen > FILENAME_MAX-1) 
            dirlen = FILENAME_MAX-1;
        strncpy(dirbuf, name, dirlen);
        dirbuf[dirlen] = 0;
        *fdp = sys_trytoopenit(dirbuf, name+(dirlen+1), ext,
            dirresult, nameresult, size, bin, okgui);
        return (1);
    }
    else return (0);
}

/* search for a file in a specified directory, then along the globally
defined search path, using ext as filename extension.  The
fd is returned, the directory ends up in the "dirresult" which must be at
least "size" bytes.  "nameresult" is set to point to the filename, which
ends up in the same buffer as dirresult.  Exception:
if the 'name' starts with a slash or a letter, colon, and slash in MSW,
there is no search and instead we just try to open the file literally.  */

/* see also canvas_open() which, in addition, searches down the
canvas-specific path. */

int do_open_via_path(const char *dir, const char *name,
    const char *ext, char *dirresult, char **nameresult, unsigned int size,
    int bin, t_namelist *searchpath, int okgui)
{
    t_namelist *nl;
    int fd = -1;
    char final_name[FILENAME_MAX];

        /* first check for @ and ~ (and later others) and replace */
    sys_expandpathelems(name, final_name);    

        /* first check if "name" is absolute (and if so, try to open) */
    if (sys_open_absolute(final_name, ext, dirresult, nameresult, size, bin, &fd, okgui))
        goto do_open_via_path_end;
    
        /* otherwise "name" is relative; try the directory "dir" first. */
    if ((fd = sys_trytoopenit(dir, final_name, ext,
        dirresult, nameresult, size, bin, okgui)) >= 0)
            goto do_open_via_path_end;

        /* next go through the temp paths from the commandline */
    for (nl = STUFF->st_temppath; nl; nl = nl->nl_next)
        if ((fd = sys_trytoopenit(nl->nl_string, name, ext,
            dirresult, nameresult, size, bin, okgui)) >= 0)
                goto do_open_via_path_end;

        /* next go through the search path */
    for (nl = searchpath; nl; nl = nl->nl_next)
        if ((fd = sys_trytoopenit(nl->nl_string, final_name, ext,
            dirresult, nameresult, size, bin, okgui)) >= 0)
                goto do_open_via_path_end;

        /* next look in built-in paths like "extra" */
    if (sys_usestdpath)
        for (nl = STUFF->st_staticpath; nl; nl = nl->nl_next)
            if ((fd = sys_trytoopenit(nl->nl_string, final_name, ext,
                dirresult, nameresult, size, bin, okgui)) >= 0)
                    goto do_open_via_path_end;

    *dirresult = 0;
    *nameresult = dirresult;
    return (-1);
do_open_via_path_end:
    return (fd);
}

    /* open via path, using the global search path. */
int open_via_path(const char *dir, const char *name, const char *ext,
    char *dirresult, char **nameresult, unsigned int size, int bin)
{
    return (do_open_via_path(dir, name, ext, dirresult, nameresult,
        size, bin, STUFF->st_searchpath, 1));
}

    /* open a file with a UTF-8 filename
    This is needed because WIN32 does not support UTF-8 filenames, only UCS2.
    Having this function prevents lots of #ifdefs all over the place.
    */
#ifdef _WIN32
int sys_open(const char *path, int oflag, ...)
{
    int i, fd;
    char pathbuf[MAXPDSTRING];
    wchar_t ucs2path[MAXPDSTRING];
    sys_bashfilename(path, pathbuf);
    u8_utf8toucs2(ucs2path, MAXPDSTRING, pathbuf, MAXPDSTRING-1);
    /* For the create mode, Win32 does not have the same possibilities,
     * so we ignore the argument and just hard-code read/write. */
    if (oflag & O_CREAT)
        fd = _wopen(ucs2path, oflag, _S_IREAD | _S_IWRITE);
    else
        fd = _wopen(ucs2path, oflag);
    return fd;
}

FILE *sys_fopen(const char *filename, const char *mode)
{
    char namebuf[MAXPDSTRING];
    wchar_t ucs2buf[MAXPDSTRING];
    wchar_t ucs2mode[MAXPDSTRING];
    sys_bashfilename(filename, namebuf);
    u8_utf8toucs2(ucs2buf, MAXPDSTRING, namebuf, MAXPDSTRING-1);
    /* mode only uses ASCII, so no need for a full conversion, just copy it */
    mbstowcs(ucs2mode, mode, MAXPDSTRING);
    return (_wfopen(ucs2buf, ucs2mode));
}
#else
#include <stdarg.h>
int sys_open(const char *path, int oflag, ...)
{
    int fd;
    char pathbuf[MAXPDSTRING];
    sys_bashfilename(path, pathbuf);
    if (oflag & O_CREAT)
    {
        mode_t mode;
        int imode;
        va_list ap;
        va_start(ap, oflag);

        /* Mac compiler complains if we just set mode = va_arg ... so, even
        though we all know it's just an int, we explicitly va_arg to an int
        and then convert.
           -> http://www.mail-archive.com/bug-gnulib@gnu.org/msg14212.html
           -> http://bugs.debian.org/647345
        */

        imode = va_arg (ap, int);
        mode = (mode_t)imode;
        va_end(ap);
        fd = open(pathbuf, oflag, mode);
    }
    else
        fd = open(pathbuf, oflag);
    return fd;
}

FILE *sys_fopen(const char *filename, const char *mode)
{
  char namebuf[MAXPDSTRING];
  sys_bashfilename(filename, namebuf);
  return fopen(namebuf, mode);
}
#endif /* _WIN32 */

   /* close a previously opened file
   this is needed on platforms where you cannot open/close resources
   across dll-boundaries, but we provide it for other platforms as well */
int sys_close(int fd)
{
#ifdef _WIN32
    return _close(fd);  /* Bill Gates is a big fat hen */
#else
    return close(fd);
#endif
}

int sys_fclose(FILE *stream)
{
    return fclose(stream);
}

    /* Open a help file using the help search path.  We expect the ".pd"
    suffix here, even though we have to tear it back off for one of the
    search attempts. */
void open_via_helppath(const char *name, const char *dir)
{
    char realname[MAXPDSTRING], newname[MAXPDSTRING], dirbuf[MAXPDSTRING], *basename;
        /* make up a silly "dir" if none is supplied */
    const char *usedir = (*dir ? dir : "./");
    int fd;

        /* 1. "objectname-help.pd" */
    strncpy(realname, name, MAXPDSTRING-10);
    realname[MAXPDSTRING-10] = 0;
    if (strlen(realname) > 3 && !strcmp(realname+strlen(realname)-3, ".pd"))
        realname[strlen(realname)-3] = 0;
    strncpy(newname, realname, MAXPDSTRING-10);
    strcat(realname, "-help.pd");
    if ((fd = do_open_via_path(usedir, realname, "", dirbuf, &basename, 
        MAXPDSTRING, 0, STUFF->st_helppath, 1)) >= 0)
            goto gotone;

        /* 2. "help-objectname.pd" */
    strcpy(realname, "help-");
    strncat(realname, name, MAXPDSTRING-10);
    realname[MAXPDSTRING-1] = 0;
    if ((fd = do_open_via_path(usedir, realname, "", dirbuf, &basename, 
        MAXPDSTRING, 0, STUFF->st_helppath, 1)) >= 0)
            goto gotone;

        /* 3. "objectname.pd" */
    //if ((fd = do_open_via_path(usedir, name, "", dirbuf, &basename, 
    //    MAXPDSTRING, 0, STUFF->st_helppath)) >= 0)
    //        goto gotone;
    post("sorry, couldn't find help patch for \"%s\"", newname);
    return;
gotone:
    close (fd);
    glob_evalfile(0, gensym((char*)basename), gensym(dirbuf));
}


/* Startup file reading for linux and __APPLE__.  As of 0.38 this will be
deprecated in favor of the "settings" mechanism */

int sys_argparse(int argc, const char **argv);

#ifndef _WIN32

#define STARTUPNAME ".pdrc"
#define NUMARGS 1000

int sys_rcfile(void)
{
    FILE* file;
    int i;
    int rcargc;
    char* rcargv[NUMARGS];
    char  fname[FILENAME_MAX], buf[1000], *home = getenv("HOME");
    int retval = 1; /* that's what we will return at the end; for now, let's think it'll be an error */
 
    /* initialize rc-arg-array so we can safely clean up at the end */
    for (i = 1; i < NUMARGS-1; i++)
      rcargv[i]=0;


    /* parse a startup file */
    
    *fname = '\0'; 

    strncat(fname, home? home : ".", FILENAME_MAX-10);
    strcat(fname, "/");

    strcat(fname, STARTUPNAME);

    if (!(file = fopen(fname, "r")))
        return 1;

    post("reading startup file: %s", fname);

    rcargv[0] = ".";    /* this no longer matters to sys_argparse() */

    for (i = 1; i < NUMARGS-1; i++)
    {
        if (fscanf(file, "%998s", buf) < 0)
            break;
        buf[999] = 0;
        if (!(rcargv[i] = malloc(strlen(buf) + 1)))
            goto cleanup;
        strcpy(rcargv[i], buf);
    }
    if (i >= NUMARGS-1)
        fprintf(stderr, "startup file too long; extra args dropped\n");
    rcargv[i] = 0;

    rcargc = i;

    /* parse the options */

    if (sys_verbose)
    {
        if (rcargc)
        {
            post("startup args from RC file:");
            for (i = 1; i < rcargc; i++)
                post("%s", rcargv[i]);
        }
        else post("no RC file arguments found");
    }
    if (sys_argparse(rcargc-1, rcargv+1))
    {
        pd_error(0, "error parsing RC arguments");
        goto cleanup;
    }

    retval=0; /* we made it without an error */


 cleanup: /* prevent memleak */
    fclose(file);

    for (i = 1; i < NUMARGS-1; i++)
      if (rcargv[i]) free(rcargv[i]);
    
    return(retval);
}
#endif /* _WIN32 */

static int string2args(const char * cmd, int * retArgc, const char *** retArgv);
void sys_doflags(void)
{
    int rcargc=0;
    const char**rcargv = NULL;
    int len;
    int rcode = 0;
    if (!sys_flags)
        sys_flags = &s_;
    len = (int)strlen(sys_flags->s_name);
    if (len > MAXPDSTRING)
    {
        pd_error(0, "flags: %s: too long", sys_flags->s_name);
        return;
    }
    rcode = string2args(sys_flags->s_name, &rcargc, &rcargv);
    if(rcode < 0) {
        pd_error(0, "error#%d while parsing flags", rcode);
        return;
    }

    if (sys_argparse(rcargc, rcargv))
        pd_error(0, "error parsing startup arguments");

    for(len=0; len<rcargc; len++)
        free((void*)rcargv[len]);
    free(rcargv);
}

/* undo pdtl_encodedialog.  This allows dialogs to send spaces, commas,
    dollars, and semis down here. */
t_symbol *sys_decodedialog(t_symbol *s)
{
    char buf[MAXPDSTRING];
    const char *sp = s->s_name;
    int i;
    if (*sp != '+')
        bug("sys_decodedialog: %s", sp);
    else sp++;
    for (i = 0; i < MAXPDSTRING-1; i++, sp++)
    {
        if (!sp[0])
            break;
        if (sp[0] == '+')
        {
            if (sp[1] == '_')
                buf[i] = ' ', sp++;
            else if (sp[1] == '+')
                buf[i] = '+', sp++;
            else if (sp[1] == 'c')
                buf[i] = ',', sp++;
            else if (sp[1] == 's')
                buf[i] = ';', sp++;
            else if (sp[1] == 'd')
                buf[i] = '$', sp++;
            else buf[i] = sp[0];
        }
        else buf[i] = sp[0];
    }
    buf[i] = 0;
    return (gensym(buf));
}


    /* start a search path dialog window */
void glob_start_path_dialog(t_pd *dummy)
{
    t_namelist *nl;

    gui_start_vmess("gui_path_properties", "xii",
        dummy,
        sys_usestdpath,
        sys_verbose
    );
    gui_start_array();
    for (nl = STUFF->st_searchpath; nl; nl = nl->nl_next)
    {
        gui_s(nl->nl_string);
    }
    gui_end_array();
    gui_end_vmess();
}

    /* new values from dialog window */
void glob_path_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(STUFF->st_searchpath);
    STUFF->st_searchpath = 0;
    sys_usestdpath = atom_getfloatarg(0, argc, argv);
    sys_verbose = atom_getfloatarg(1, argc, argv);
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            STUFF->st_searchpath = namelist_append_files(STUFF->st_searchpath, s->s_name);
    }
}

    /* AG 20190801: add one item to search path (backported from
       vanilla rev. c917dd19, to make GEM happy). */
void glob_addtopath(t_pd *dummy, t_symbol *path, t_float saveit)
{
  int saveflag = (int)saveit;
  t_symbol *s = sys_decodedialog(path);
  if (*s->s_name)
  {
    if (saveflag < 0)
        STUFF->st_temppath =
            namelist_append_files(STUFF->st_temppath, s->s_name);
    else
        STUFF->st_searchpath =
            namelist_append_files(STUFF->st_searchpath, s->s_name);
    if (saveit > 0) {
      /* AG: We just ignore this flag for now, later maybe save the
         preferences here. GEM doesn't need this, and we don't use
         Deken, so we can do without this. */
    }
  }
}

    /* start a startup dialog window */
void glob_start_startup_dialog(t_pd *dummy)
{
    t_namelist *nl;

    gui_start_vmess("gui_lib_properties", "xis",
        dummy,
        sys_defeatrt,
        sys_flags ? sys_flags->s_name : ""
    );
    gui_start_array();
    for (nl = STUFF->st_externlist; nl; nl = nl->nl_next)
    {
        gui_s(nl->nl_string);
    }
    gui_end_array();
    gui_end_vmess();
}

    /* new values from dialog window */
void glob_startup_dialog(t_pd *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int i;
    namelist_free(STUFF->st_externlist);
    STUFF->st_externlist = 0;
    sys_defeatrt = atom_getfloatarg(0, argc, argv);
    sys_flags = sys_decodedialog(atom_getsymbolarg(1, argc, argv));
    for (i = 0; i < argc-2; i++)
    {
        t_symbol *s = sys_decodedialog(atom_getsymbolarg(i+2, argc, argv));
        if (*s->s_name)
            STUFF->st_externlist = namelist_append_files(STUFF->st_externlist, s->s_name);
    }
}

t_symbol *pd_getdirname(void)
{
    char buf[MAXPDSTRING], buf2[MAXPDSTRING];
    int len;
#ifdef _WIN32
    if ((len = GetModuleFileName(NULL, buf, sizeof(buf))) == 0)
        strcpy(buf, ".");
    else
        buf[len] = '\0';
    sys_unbashfilename(buf, buf);
#elif defined(__APPLE__)
    int ret;
    len = sizeof(buf); buf[0] = '\0';
    ret = _NSGetExecutablePath(buf, &len);
    if (ret) len = -1;
#elif defined(__FreeBSD__)
    len = (ssize_t)(readlink("/proc/curproc/file", buf, sizeof(buf)-1));
    if (len != -1) buf[len] = '\0';
#else
    len = (ssize_t)(readlink("/proc/self/exe", buf, sizeof(buf)-1));
    if (len != -1) buf[len] = '\0';
#endif
    if (len != -1)
    {
        char *lastslash;
        lastslash = strrchr(buf, '/');
        if (lastslash)
        {
            lastslash++;
            *lastslash= '\0';
            strncpy(buf2, buf, lastslash-buf);
            buf2[lastslash-buf] = '\0';
        }
        else strcpy(buf2, ".");
    }
    else
    {
        return 0;
    }
    t_symbol *foo = gensym(buf2);
   return foo;
}

/*
 * the following string2args function is based on from sash-3.8 (the StandAlone SHell)
 * Copyright (c) 2014 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 */
#define	isBlank(ch)	(((ch) == ' ') || ((ch) == '\t'))
int string2args(const char * cmd, int * retArgc, const char *** retArgv)
{
    int errCode = 1;
    int len = strlen(cmd), argCount = 0;
    char strings[MAXPDSTRING], *cp;
    const char **argTable = 0, **newArgTable;

    if(retArgc) *retArgc = 0;
    if(retArgv) *retArgv = NULL;

        /*
         * Copy the command string into a buffer that we can modify,
         * reallocating it if necessary.
         */
    if(len >= MAXPDSTRING) {
        errCode = 1; goto ouch;
    }
    memset(strings, 0, MAXPDSTRING);
    memcpy(strings, cmd, len);
    cp = strings;

        /* Keep parsing the command string as long as there are any arguments left. */
    while (*cp) {
        const char *cpIn = cp;
        char *cpOut = cp, *argument;
        int quote = '\0';

            /*
             * Loop over the string collecting the next argument while
             * looking for quoted strings or quoted characters.
             */
        while (*cp) {
            int ch = *cp++;

                /* If we are not in a quote and we see a blank then this argument is done. */
            if (isBlank(ch) && (quote == '\0'))
                break;

                /* If we see a backslash then accept the next character no matter what it is. */
            if (ch == '\\') {
                ch = *cp++;
                if (ch == '\0') { /* but only if there is a next char */
                    errCode = 10; goto ouch;
                }
                *cpOut++ = ch;
                continue;
            }

                /* If we were in a quote and we saw the same quote character again then the quote is done. */
            if (ch == quote) {
                quote = '\0';
                continue;
            }

                /* If we weren't in a quote and we see either type of quote character,
                 * then remember that we are now inside of a quote. */
            if ((quote == '\0') && ((ch == '\'') || (ch == '"')))  {
                quote = ch;
                continue;
            }

                /* Store the character. */
            *cpOut++ = ch;
        }

        if (quote) { /* Unmatched quote character */
            errCode = 11; goto ouch;
        }

            /*
             * Null terminate the argument if it had shrunk, and then
             * skip over all blanks to the next argument, nulling them
             * out too.
             */
        if (cp != cpOut)
            *cpOut = '\0';
        while (isBlank(*cp))
            *cp++ = '\0';

        if (!(argument = calloc(1+cpOut-cpIn, 1))) {
            errCode = 22; goto ouch;
        }
        memcpy(argument, cpIn, cpOut-cpIn);

            /* Now reallocate the argument table to hold the argument, add add it. */
        if (!(newArgTable = (const char **) realloc(argTable, (sizeof(const char *) * (argCount + 1))))) {
            free(argument);
            errCode= 23; goto ouch;
        } else argTable = newArgTable;

        argTable[argCount] = argument;

        argCount++;
    }

        /*
         * Null terminate the argument list and return it.
         */
    if (!(newArgTable = (const char **) realloc(argTable, (sizeof(const char *) * (argCount + 1))))) {
        errCode = 23; goto ouch;
    } else argTable = newArgTable;

    argTable[argCount] = NULL;

    if(retArgc) *retArgc = argCount;
    if(retArgv)
        *retArgv = argTable;
    else
        free(argTable);
    return argCount;

 ouch:
    free(argTable);
    return -errCode;
}
