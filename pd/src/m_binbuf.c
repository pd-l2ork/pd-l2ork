/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "config.h"

#include <stdlib.h>
#include "m_pd.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <stdarg.h>

#define DOLLARALL -0x7fffffff /* sentinel value for "$@" dollar arg */

/* escape characters for saving */
static char* strnescape(char *dest, const char *src, size_t outlen)
{
    int ptin = 0;
    unsigned ptout = 0;
    for(; ptout < outlen; ptin++, ptout++)
    {
        int c = src[ptin];
        if (c == ' ' || c=='\t')
            dest[ptout++] = '\\';
        dest[ptout] = src[ptin];
        if (c==0) break;
    }

    if(ptout < outlen)
        dest[ptout]=0;
    else
        dest[outlen-1]=0;

    return dest;
}


struct _binbuf
{
    int b_n;
    t_atom *b_vec;
};

t_binbuf *binbuf_new(void)
{
    t_binbuf *x = (t_binbuf *)t_getbytes(sizeof(*x));
    x->b_n = 0;
    x->b_vec = t_getbytes(0);
    return (x);
}

void binbuf_free(t_binbuf *x)
{
    t_freebytes(x->b_vec, x->b_n * sizeof(*x->b_vec));
    t_freebytes(x,  sizeof(*x));
}

t_binbuf *binbuf_duplicate(const t_binbuf *y)
{
    t_binbuf *x = (t_binbuf *)t_getbytes(sizeof(*x));
    x->b_n = y->b_n;
    x->b_vec = t_getbytes(x->b_n * sizeof(*x->b_vec));
    memcpy(x->b_vec, y->b_vec, x->b_n * sizeof(*x->b_vec));
    return (x);
}

void binbuf_clear(t_binbuf *x)
{
    x->b_vec = t_resizebytes(x->b_vec, x->b_n * sizeof(*x->b_vec), 0);
    x->b_n = 0;
}

void binbuf_gettext_from_a_gimme(char **buf, int *len, int ac, t_atom *av)
{
    t_binbuf *b = binbuf_new();
    b->b_n = ac;
    b->b_vec = av;

    binbuf_gettext(b, buf, len);
    //post("binbuf_gettext_from_a_gimme <%s> size=%d", *buf, *len);

    b->b_n = 0;
    b->b_vec = t_getbytes(0);
    binbuf_free(b);
}

void binbuf_setvec(t_binbuf *x, t_atom *vec)
{
    x->b_vec = vec;
}

    /* convert text to a binbuf */
void binbuf_text(t_binbuf *x, const char *text, size_t size)
{
    //post("current text: %d <%s>", size, text);
    char buf[MAXPDSTRING+1], *bufp, *ebuf = buf+MAXPDSTRING;
    const char *textp = text, *etext = text+size;
    t_atom *ap;
    int nalloc = 16, natom = 0;
    t_freebytes(x->b_vec, x->b_n * sizeof(*x->b_vec));
    x->b_vec = t_getbytes(nalloc * sizeof(*x->b_vec));
    ap = x->b_vec;
    x->b_n = 0;
    while (1)
    {
        //int type;
            /* skip leading space */
        while ((textp != etext) && (*textp == ' ' || *textp == '\n'
            || *textp == '\r' || *textp == '\t')) textp++;
        if (textp == etext) break;
        if ((*textp == ';' && *textp == text) ||
            (*textp == ';' && *(textp-1) != '\\'))
        {
            SETSEMI(ap), textp++;
        }
        else if ((*textp == ',' && *textp == text) ||
            (*textp == ',' && *(textp-1) != '\\'))
        {
            //post("comma");
            SETCOMMA(ap), textp++;
        }
        else
        {
                /* it's an atom other than a comma or semi */
            char c;
            int floatstate = 0, slash = 0, lastslash = 0, dollar = 0;
            bufp = buf;
            do
            {
                c = *bufp = *textp++;
                lastslash = slash;
                slash = (c == '\\');

                if (floatstate >= 0)
                {
                    int digit = (c >= '0' && c <= '9'),
                        dot = (c == '.'), minus = (c == '-'),
                        plusminus = (minus || (c == '+')),
                        expon = (c == 'e' || c == 'E');
                    if (floatstate == 0)    /* beginning */
                    {
                        if (minus) floatstate = 1;
                        else if (digit) floatstate = 2;
                        else if (dot) floatstate = 3;
                        else floatstate = -1;
                    }
                    else if (floatstate == 1)   /* got minus */
                    {
                        if (digit) floatstate = 2;
                        else if (dot) floatstate = 3;
                        else floatstate = -1;
                    }
                    else if (floatstate == 2)   /* got digits */
                    {
                        if (dot) floatstate = 4;
                        else if (expon) floatstate = 6;
                        else if (!digit) floatstate = -1;
                    }
                    else if (floatstate == 3)   /* got '.' without digits */
                    {
                        if (digit) floatstate = 5;
                        else floatstate = -1;
                    }
                    else if (floatstate == 4)   /* got '.' after digits */
                    {
                        if (digit) floatstate = 5;
                        else if (expon) floatstate = 6;
                        else floatstate = -1;
                    }
                    else if (floatstate == 5)   /* got digits after . */
                    {
                        if (expon) floatstate = 6;
                        else if (!digit) floatstate = -1;
                    }
                    else if (floatstate == 6)   /* got 'e' */
                    {
                        if (plusminus) floatstate = 7;
                        else if (digit) floatstate = 8;
                        else floatstate = -1;
                    }
                    else if (floatstate == 7)   /* got plus or minus */
                    {
                        if (digit) floatstate = 8;
                        else floatstate = -1;
                    }
                    else if (floatstate == 8)   /* got digits */
                    {
                        if (!digit) floatstate = -1;
                    }
                }
                if (!lastslash && c == '$' && (textp != etext && 
                    ((textp[0] >= '0' && textp[0] <= '9')||
                    textp[0]=='@'))) /* JMZ: $@ and $# expansion */
                        dollar = 1;
                if (!slash) bufp++;
                else if (lastslash)
                {
                    bufp++;
                    slash = 0;
                }
            }
            while (textp != etext && bufp != ebuf && 
                (slash || (*textp != ' ' && *textp != '\n' && *textp != '\r'
                    && *textp != '\t' && *textp != ',' && *textp != ';')));
            *bufp = 0;
#if 0
            post("binbuf_text: buf %s, dollar=%d", buf, dollar);
#endif
            if (floatstate == 2 || floatstate == 4 || floatstate == 5 ||
                floatstate == 8)
                    SETFLOAT(ap, atof(buf));
                /* LATER try to figure out how to mix "$" and "\$" correctly;
                here, the backslashes were already stripped so we assume all
                "$" chars are real dollars.  In fact, we only know at least one
                was. */
            else if (dollar)
            {
                if(buf[1]=='@') /* JMZ: $@ expansion */
                {
                    if(buf[2]==0) /* only expand A_DOLLAR $@ */
                    {
                        ap->a_type = A_DOLLAR;
                        ap->a_w.w_index = DOLLARALL;
                    } 
                    else /* there is no A_DOLLSYM $@ */
                    {
                        SETSYMBOL(ap, gensym(buf));
                    }
                }
                else
                {
                    if (buf[0] != '$') 
                        dollar = 0;
                    for (bufp = buf+1; *bufp; bufp++)
                        if (*bufp < '0' || *bufp > '9')
                            dollar = 0;
                    if (dollar)
                        SETDOLLAR(ap, atoi(buf+1));
                    else SETDOLLSYM(ap, gensym(buf));
                }
            }
            else SETSYMBOL(ap, gensym(buf));
        }
        ap++;
        natom++;
        if (natom == nalloc)
        {
            //fprintf(stderr,"binbuf_text resizing from %d to %d",
            //    nalloc * sizeof(*x->b_vec), nalloc * (2*sizeof(*x->b_vec)));
            x->b_vec = t_resizebytes(x->b_vec, nalloc * sizeof(*x->b_vec),
                nalloc * (2*sizeof(*x->b_vec)));
            nalloc = nalloc * 2;
            ap = x->b_vec + natom;
        }
        if (textp == etext) break;
    }
    /* reallocate the vector to exactly the right size */
    x->b_vec = t_resizebytes(x->b_vec, nalloc * sizeof(*x->b_vec),
        natom * sizeof(*x->b_vec));
    x->b_n = natom;
}

    /* ico@vt.edu 2025-04-22:
       convert text to a binbuf with a single atom
       (thus preserving numbers as string and preventing truncation, 
       e.g. [sprintf %s%s%s%s%s%s%s%s] will get converted into a number
       if every single string is a number, and even more so it may get
       truncated with an exponent)
    */
void binbuf_rawtext(t_binbuf *x, const char *text, size_t size)
{
    //post("current text: %d <%s>", size, text);
    char buf[MAXPDSTRING+1], *bufp, *ebuf = buf+MAXPDSTRING;
    const char *textp = text, *etext = text+size;
    t_atom *ap;
    int nalloc = 16, natom = 0;
    t_freebytes(x->b_vec, x->b_n * sizeof(*x->b_vec));
    x->b_vec = t_getbytes(nalloc * sizeof(*x->b_vec));
    ap = x->b_vec;
    x->b_n = 0;
    while (1)
    {
        //int type;
            /* skip leading space */
        while ((textp != etext) && (*textp == ' ' || *textp == '\n'
            || *textp == '\r' || *textp == '\t')) textp++;
        if (textp == etext) break;

            /* it's an atom other than a comma or semi */
        char c;
        int floatstate = 0, slash = 0, lastslash = 0, dollar = 0;
        bufp = buf;
        do
        {
            c = *bufp = *textp++;
            bufp++;
        }
        while (textp != etext && bufp != ebuf && 
            (slash || (*textp != ' ' && *textp != '\n' && *textp != '\r'
                && *textp != '\t' && *textp != ',' && *textp != ';')));
        *bufp = 0;
#if 0
        post("binbuf_text: buf %s, dollar=%d", buf, dollar);
#endif
        SETSYMBOL(ap, gensym(buf));

        ap++;
        natom++;
        if (natom == nalloc)
        {
            //fprintf(stderr,"binbuf_text resizing from %d to %d",
            //    nalloc * sizeof(*x->b_vec), nalloc * (2*sizeof(*x->b_vec)));
            x->b_vec = t_resizebytes(x->b_vec, nalloc * sizeof(*x->b_vec),
                nalloc * (2*sizeof(*x->b_vec)));
            nalloc = nalloc * 2;
            ap = x->b_vec + natom;
        }
        if (textp == etext) break;
    }
    /* reallocate the vector to exactly the right size */
    x->b_vec = t_resizebytes(x->b_vec, nalloc * sizeof(*x->b_vec),
        natom * sizeof(*x->b_vec));
    x->b_n = natom;
}

    /* convert a binbuf to text; no null termination. */
void binbuf_gettext(const t_binbuf *x, char **bufp, int *lengthp)
{
    char *buf = getbytes(0), *newbuf;
    int length = 0;
    char string[MAXPDSTRING];
    t_atom *ap;
    int indx;
    int newlength;

    for (ap = x->b_vec, indx = x->b_n; indx--; ap++)
    {
        //fprintf(stderr,"=====\n");
        if ((ap->a_type == A_SEMI || ap->a_type == A_COMMA) &&
                length && buf[length-1] == ' ') 
        {
            //fprintf(stderr, "subtracting length\n");
            length--;
        }
        atom_string(ap, string, MAXPDSTRING);
        newlength = length + strlen(string) + 1;
        if (!(newbuf = resizebytes(buf, length, newlength))) break;
        buf = newbuf;
        //post("string=<%s> buf=<%s> length=%d\n", string, buf, length);
        strcpy(buf + length, string);
        length = newlength;
        // this is where we automatically add \n after ;
        // for the time being, we will keep this as-is, even though
        // it is a kludge, mainly to keep backwards compatibility
        if (ap->a_type == A_SEMI) buf[length-1] = '\n';
        else buf[length-1] = ' ';
        //if (ap->a_type == A_COMMA) length--;
    }
    if (length && buf[length-1] == ' ')
    {
        // 2021-11-18 ico@vt.edu: here the new length (3rd arg) is equal to
        // length and not length-1 because realloc in resizebytes
        // adds a terminating character
        if (newbuf = t_resizebytes(buf, length, length))
        {
            buf = newbuf;
            length--;
        }
    }
    //fprintf(stderr,"binbuf_gettext: <%s>\n", buf);
    *bufp = buf;
    *lengthp = length;
}

    /* convert a binbuf to text without adding an \n after semi
       this is used for comments (and later possibly other objects
       that require raw interpretation of the text); no null termination. */
void binbuf_getrawtext(t_binbuf *x, char **bufp, int *lengthp)
{
    char *buf = getbytes(0), *newbuf;
    int length = 0;
    char string[MAXPDSTRING];
    t_atom *ap;
    int indx;
    int newlength;

    for (ap = x->b_vec, indx = x->b_n; indx--; ap++)
    {
        //fprintf(stderr,"=====\n");
        if ((ap->a_type == A_SEMI || ap->a_type == A_COMMA) &&
                length && buf[length-1] == ' ') 
        {
            //fprintf(stderr, "subtracting length\n");
            length--;
        }
        atom_string(ap, string, MAXPDSTRING);
        newlength = length + strlen(string) + 1;
        if (!(newbuf = resizebytes(buf, length, newlength))) break;
        buf = newbuf;
        //post("string=<%s> buf=<%s> length=%d\n", string, buf, length);
        strcpy(buf + length, string);
        length = newlength;
        // this is where we automatically add \n after ;
        // for the time being, we will keep this as-is, even though
        // it is a kludge, mainly to keep backwards compatibility
        ////if (ap->a_type == A_SEMI) buf[length-1] = '\n';
        ////else buf[length-1] = ' ';
        buf[length-1] = ' ';
        ////if (ap->a_type == A_COMMA) length--;
    }
    if (length && buf[length-1] == ' ')
    {
        // 2021-11-18 ico@vt.edu: here the new length (3rd arg) is equal to
        // length and not length-1 because realloc in resizebytes
        // adds a terminating character
        if (newbuf = t_resizebytes(buf, length, length))
        {
            buf = newbuf;
            length--;
        }
    }
    //fprintf(stderr,"binbuf_gettext: <%s>\n", buf);
    *bufp = buf;
    *lengthp = length;
}

/* LATER improve the out-of-space behavior below.  Also fix this so that
writing to file doesn't buffer everything together. */

void binbuf_add(t_binbuf *x, int argc, const t_atom *argv)
{
    //fprintf(stderr,"binbuf_add_started %d\n", argc);
    int newsize = x->b_n + argc, i;
    t_atom *ap;
    if (ap = t_resizebytes(x->b_vec, x->b_n * sizeof(*x->b_vec),
        newsize * sizeof(*x->b_vec)))
            x->b_vec = ap;
    else
    {
        error("binbuf_addmessage: out of space");
        return;
    }
    /*
    for (i=0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL)
        {
            fprintf(stderr, "binbuf_add: %s\n", argv[i].a_w.w_symbol->s_name);
        }
        else if (argv[i].a_type == A_FLOAT)
        {
            fprintf(stderr, "binbuf_add: %g\n", argv[i].a_w.w_float);
        }
    }
    */
#if 0
    startpost("binbuf_add: ");
    postatom(argc, argv);
    endpost();
#endif
    for (ap = x->b_vec + x->b_n, i = argc; i--; ap++)
        *ap = *(argv++);
    x->b_n = newsize;
    //fprintf(stderr,"done binbuf_add\n");
}

#define MAXADDMESSV 100
void binbuf_addv(t_binbuf *x, const char *fmt, ...)
{
    //fprintf(stderr,"binbuf_addv started\n");
    va_list ap;
    t_atom arg[MAXADDMESSV], *at =arg;
    int nargs = 0;
    char *fp = fmt;

    va_start(ap, fmt);
    while (1)
    {
        if (nargs >= MAXADDMESSV)
        {
            error("binbuf_addmessv: only %d allowed", MAXADDMESSV);
            break;
        }
        switch(*fp++)
        {
        case 'i': SETFLOAT(at, va_arg(ap, int));
                    //fprintf(stderr, "i = %f\n", at->a_w.w_float);
                    break;
        case 'f': SETFLOAT(at, va_arg(ap, double)); 
                    //fprintf(stderr, "f = %f\n", at->a_w.w_float);
                    break;
        case 's': SETSYMBOL(at, va_arg(ap, t_symbol *)); 
                    //fprintf(stderr, "s = %s\n", at->a_w.w_symbol->s_name);
                    break;
        case ';': SETSEMI(at); break;
        case ',': SETCOMMA(at); break;
        default: goto done;
        }
        at++;
        nargs++;
    }
done:
    va_end(ap);
    //fprintf(stderr,"done binbuf_addv\n");
    binbuf_add(x, nargs, arg);
}

/* ico@vt.edu 2022-11-22: a function that can process
   large amounts of data (e.g. saving array contents)
   at a fraction of time. this is necessary for Windows
   where regualr binbuf_addv for a ~3-minute wav file
   saved with the array requires over an hour to save
   takes 5 seconds to do so using this method. TODO:
   figure out how to read those same files without
   running out of memory.
*/
void binbuf_addarray(t_binbuf *x, int length, char *fmt, t_word *wordarray)
{
    //fprintf(stderr,"binbuf_addarray started\n");
    t_atom *arg = (t_atom *)t_getbytes(length * sizeof(*arg));
    t_atom *at = arg;
    char *fp = fmt;
    int i;

    for (i = 0; i < length - 1; i++)
    {
        switch(*fp)
        {
            case 'i': SETFLOAT(at, wordarray[i].w_float);
                        //fprintf(stderr, "i = %f\n", at->a_w.w_float);
                        break;
            case 'f': SETFLOAT(at, wordarray[i].w_float);
                        //fprintf(stderr, "f = %f\n", at->a_w.w_float);
                        break;
            case 's': SETSYMBOL(at, wordarray[i].w_symbol);
                        //fprintf(stderr, "s = %s\n", at->a_w.w_symbol->s_name);
                        break;
        }
        at++;
    }
    SETSEMI(at);

    //fprintf(stderr,"done binbuf_addarray\n");
    binbuf_add(x, length, arg);
    t_freebytes(arg, length * sizeof(t_atom));
}

/* add a binbuf to another one for saving.  Semicolons and commas go to
symbols ";", ",",; We assume here (probably incorrectly) that there's
no symbol whose name is ";" - should we be escaping those?. */

void binbuf_addbinbuf(t_binbuf *x, const t_binbuf *y)
{
    //fprintf(stderr,"starting binbuf_addbinbuf\n");
    t_binbuf *z = binbuf_new();
    int i;
    t_atom *ap;
    char escapedollsym[MAXPDSTRING+2];
    binbuf_add(z, y->b_n, y->b_vec);
    for (i = 0, ap = z->b_vec; i < z->b_n; i++, ap++)
    {
        escapedollsym[0] = '\\';
        escapedollsym[1] = '\0';
        char tbuf[MAXPDSTRING];
        switch (ap->a_type)
        {
        case A_FLOAT:
            //post("addbinbuf: float");
            break;
        case A_SEMI:
            //post("addbinbuf: semi");
            SETSYMBOL(ap, gensym(";"));
            break;
        case A_COMMA:
            //post("addbinbuf: comma");
            SETSYMBOL(ap, gensym(","));
            break;
        case A_DOLLAR:
            //post("addbinbuf: dollar");
            if(ap->a_w.w_index==DOLLARALL){ /* JMZ: $@ expansion */
                SETSYMBOL(ap, gensym("$@"));
            } else {
                sprintf(tbuf, "$%d", ap->a_w.w_index);
                SETSYMBOL(ap, gensym(tbuf));
            }
            break;
        case A_DOLLSYM:
            atom_string(ap, tbuf, MAXPDSTRING);
            SETSYMBOL(ap, gensym(tbuf));
            //post("addbinbuf: dollsym <%s>", ap->a_w.w_symbol->s_name);
            break;
        case A_SYMBOL:
            //post("addbinbuf: symbol %s", ap->a_w.w_symbol->s_name);
            // ico@vt.edu 2020-12-11: Here we escape , ; and $ in case
            // they are interpreted as symbols, which implies they were
            // prepended with a '\'. Doing this will ensure that the patch
            // is reopened the same way it was saved.
            //post("addbinbuf: symbol <%s>", ap->a_w.w_symbol->s_name);
            if (!strcmp(ap->a_w.w_symbol->s_name, ";"))
                SETSYMBOL(ap, gensym("\\\;"));
            else if (!strcmp(ap->a_w.w_symbol->s_name, ","))
                SETSYMBOL(ap, gensym("\\\,"));
            else if (ap->a_w.w_symbol->s_name[0] == '$')
            {
                strcat(escapedollsym, ap->a_w.w_symbol->s_name);
                SETSYMBOL(ap, gensym(escapedollsym));
                //post("...got $: adjusting... <%s>", escapedollsym);
            }
            break;
        default:
            bug("binbuf_addbinbuf");
        }
    }
    
    binbuf_add(x, z->b_n, z->b_vec);
    binbuf_free(z);
    //fprintf(stderr,"done binbuf_addbinbuf\n");
}

void binbuf_addsemi(t_binbuf *x)
{
    t_atom a;
    SETSEMI(&a);
    binbuf_add(x, 1, &a);
    //binbuf_add(x, 1, '\0');
}

/* Supply atoms to a binbuf from a message, making the opposite changes
from binbuf_addbinbuf.  The symbol ";" goes to a semicolon, etc. */

void binbuf_restore(t_binbuf *x, int argc, const t_atom *argv)
{
    int newsize = x->b_n + argc, i;
    t_atom *ap;
    if (ap = t_resizebytes(x->b_vec, x->b_n * sizeof(*x->b_vec),
        newsize * sizeof(*x->b_vec)))
            x->b_vec = ap;
    else
    {
        error("binbuf_addmessage: out of space");
        return;
    }

    for (ap = x->b_vec + x->b_n, i = argc; i--; ap++)
    {
        if (argv->a_type == A_SYMBOL)
        {
            char *str = argv->a_w.w_symbol->s_name, *str2;
            if (!strcmp(str, ";")) SETSEMI(ap);
            else if (!strcmp(str, ",")) SETCOMMA(ap);
            else if (!strcmp(str, "$@")) /* JMZ: $@ expansion */
            {
                ap->a_type = A_DOLLAR;
                ap->a_w.w_index = DOLLARALL;
            }
            else if ((str2 = strchr(str, '$')) && str2[1] >= '0'
                && str2[1] <= '9')
            {
                int dollsym = 0;
                if (*str != '$')
                    dollsym = 1;
                else for (str2 = str + 1; *str2; str2++)
                    if (*str2 < '0' || *str2 > '9')
                {
                    dollsym = 1;
                    break;
                }
                if (dollsym)
                    SETDOLLSYM(ap, gensym(str));
                else
                {
                    int dollar = 0;
                    sscanf(argv->a_w.w_symbol->s_name + 1, "%d", &dollar);
                    SETDOLLAR(ap, dollar);
                }
            }
            else if (strchr(argv->a_w.w_symbol->s_name, '\\'))
            {
                char buf[MAXPDSTRING], *sp1, *sp2;
                int slashed = 0;
                for (sp1 = buf, sp2 = argv->a_w.w_symbol->s_name;
                    *sp2 && sp1 < buf + (MAXPDSTRING-1);
                        sp2++)
                {
                    if (slashed)
                        *sp1++ = *sp2;
                    else if (*sp2 == '\\')
                        slashed = 1;
                    else *sp1++ = *sp2, slashed = 0;
                }
                *sp1 = 0;
                SETSYMBOL(ap, gensym(buf));
            }
            else *ap = *argv;
            argv++;
        }
        else *ap = *(argv++);
    }
    x->b_n = newsize;
}

#define ISSYMBOL(a, b) ((a)->a_type == A_SYMBOL && \
    !strcmp((a)->a_w.w_symbol->s_name, (b)))

void binbuf_print(const t_binbuf *x)
{
    int i, startedpost = 0, newline = 1;
    for (i = 0; i < x->b_n; i++)
    {
        if (newline)
        {
            if (startedpost) endpost();
            startpost("");
            startedpost = 1;
        }
        if (ISSYMBOL(x->b_vec + i, "%hidden%"))
            i = x->b_n - 1;
        postatom(1, x->b_vec + i);
        if (x->b_vec[i].a_type == A_SEMI)
            newline = 1;
        else newline = 0; 
    }
    if (startedpost) endpost();
}

int binbuf_getnatom(const t_binbuf *x)
{
    return (x->b_n);
}

t_atom *binbuf_getvec(const t_binbuf *x)
{
    return (x->b_vec);
}

int binbuf_resize(t_binbuf *x, int newsize)
{
    t_atom *new = t_resizebytes(x->b_vec,
        x->b_n * sizeof(*x->b_vec), newsize * sizeof(*x->b_vec));
    if (new)
        x->b_vec = new, x->b_n = newsize;
    return (new != 0);
}

int canvas_getdollarzero(t_pd *x);

/* JMZ:
 * s points to the first character after the $
 * (e.g. if the org.symbol is "$1-bla", then s will point to "1-bla")
 * (e.g. org.symbol="hu-$1mu", s="1mu")
 * LATER: think about more complex $args, like ${$1+3}
 *
 * the return value holds the length of the $arg (in most cases: 1)
 * buf holds the expanded $arg
 *
 * if some error occured, "-1" is returned
 *
 * e.g. "$1-bla" with list "10 20 30"
 * s="1-bla"
 * buf="10"
 * return value = 1; (s+1=="-bla")
 */
int binbuf_expanddollsym(char*s, char*buf,t_atom dollar0, int ac, t_atom *av,
    int tonew)
{
  int argno=atol(s);
  int arglen=0;
  char*cs=s;
  char c=*cs;
  *buf=0;

  while(c && (c >= '0') && (c <= '9'))
  {
    c = *cs++;
    arglen++;
  }

  if (cs==s) { /* invalid $-expansion (like "$bla") */
    sprintf(buf, "$");
    return 0;
  }
  else if (argno < 0 || argno > ac) /* undefined argument */
    {
      if(!tonew)return 0;
      sprintf(buf, "$%d", argno);
    }
  else if (argno == 0){ /* $0 */
    atom_string(&dollar0, buf, MAXPDSTRING/2-1);
  }
  else{ /* fine! */
    atom_string(av+(argno-1), buf, MAXPDSTRING/2-1);
  }
  return (arglen-1);
}

/* LATER remove the dependence on the current canvas for $0; should be another
argument. */
static t_symbol *binbuf_dorealizedollsym(t_pd *target, t_symbol *s, int ac,
    t_atom *av, int tonew)
{
    char buf[MAXPDSTRING];
    char buf2[MAXPDSTRING];
    char*str=s->s_name;
    char*substr;
    int next=0, i=MAXPDSTRING;
    t_atom dollarnull;
    SETFLOAT(&dollarnull, canvas_getdollarzero(target));
    while(i--)buf2[i]=0;

#if 1
    /* JMZ: currently, a symbol is detected to be
     * A_DOLLSYM if it starts with '$'
     * the leading $ is stripped and the rest stored in "s"
     * i would suggest to NOT strip the leading $
     * and make everything a A_DOLLSYM that contains(!) a $
     *
     * whenever this happened, enable this code
     */
    substr=strchr(str, '$');
    /*
    post("binbuf_dorealizedollsym str=<%s> substr=%d", str, substr-str);
    if (substr-str > 0 && substr-str < MAXPDSTRING)
        post("... <%c>", str[substr-str-1]);
    */
    // ico@vt.edu 2021-04-29: added third condition to check if $ is preceded
    // by a backslash, in which case the dollsym should remain unrealized
    if (!substr || substr-str >= MAXPDSTRING ||
        (substr-str > 0 && substr-str < MAXPDSTRING && str[substr-str-1] == '\\'))
        return (s);

    strncat(buf2, str, (substr-str));
    str=substr+1;

#endif

    while((next=binbuf_expanddollsym(str, buf, dollarnull, ac, av, tonew))>=0)
    {
        /*
        * JMZ: i am not sure what this means, so i might have broken it
        * it seems like that if "tonew" is set and the $arg cannot be expanded
        * (or the dollarsym is in reality a A_DOLLAR)
        * 0 is returned from binbuf_dorealizedollsym
        * this happens, when expanding in a message-box, but does not happen
        * when the A_DOLLSYM is the name of a subpatch
        */
        if(!tonew&&(0==next)&&(0==*buf))
        {
            return 0; /* JMZ: this should mimick the original behaviour */
        }

        strncat(buf2, buf, MAXPDSTRING/2-1);
        str+=next;
        substr=strchr(str, '$');
        if(substr)
        {
            strncat(buf2, str, (substr-str));
            str=substr+1;
        } 
        else
        {
            strcat(buf2, str);
            goto done;
        }
    }
done:
    return (gensym(buf2));
}

t_symbol *binbuf_realizedollsym(t_symbol *s, int ac, const t_atom *av, int tonew)
{
    return binbuf_dorealizedollsym(0, s, ac, av, tonew);
}

#define SMALLMSG 5
#define HUGEMSG 1000
#ifdef MSW
#include <malloc.h>
#else
#include <alloca.h>
#endif
#if HAVE_ALLOCA
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < HUGEMSG ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define ATOMS_FREEA(x, n) ( \
    ((n) < HUGEMSG || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

t_pd *pd_mess_from_responder(t_pd *x);
static void binbuf_error(t_pd *x, const char *fmt, ...)
{
    char buf[MAXPDSTRING];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    if (x)
        pd_error(pd_mess_from_responder(x), buf);
    else
        error(buf);
}

void binbuf_eval(const t_binbuf *x, t_pd *target, int argc, const t_atom *argv)
{
    t_atom smallstack[SMALLMSG], *mstack, *msp;
    t_atom *at = x->b_vec;
    int ac = x->b_n;
    int nargs, maxnargs = 0;
    /* initial target for referencing $0 in msg boxes after a semicolon */
    t_pd * init_target = target;

    //first we need to check if the list of arguments has $@
    //fprintf(stderr,"=========\nbinbuf_eval argc:%d ac:%d\n", argc, (int)ac);
    int count, old_ac = ac;
    for (count = 0; count < old_ac; count++)
    {
        //fprintf(stderr, "count %d\n", count);
        if (at[count].a_type == A_DOLLAR &&
            at[count].a_w.w_index==DOLLARALL)
        {
            //fprintf(stderr,"found @ count:%d ac:%d argc:%d ac+argc-1:%d\n",
            //    count, ac, argc, ac+argc-1);
            ac = ac + argc;
            maxnargs = ac;
        }
    }
    if (ac <= SMALLMSG)
        mstack = smallstack;
    else
    {
#if 1
            /* count number of args in biggest message.  The wierd
            treatment of "pd_objectmaker" is because when the message
            goes out to objectmaker, commas and semis are passed
            on as regular args (see below).  We're tacitly assuming here
            that the pd_objectmaker target can't come up via a named
            destination in the message, only because the original "target"
            points there. */
        if (target == &pd_objectmaker) {
            maxnargs = ac;
            //if (at && x->b_n) fprintf(stderr,"pd_objectmaker %s %d %d %d\n",
            //    at[0].a_w.w_symbol->s_name, ac, argc, maxnargs);
        }
        else if (!maxnargs) //we set maxnargs=ac if we encounter $@ arg (see above) so we avoid this place because A_SEMI and A_COMMA tend to be detected at location 220. Why? No idea. Perhaps it is a line delimiter for one line in the saved file?
        {
            int i, j = (target ? 0 : -1);
            for (i = 0; i < ac; i++)
            {
                if (at[i].a_type == A_SEMI) {
                    j = -1;
                    //fprintf(stderr,"A_SEMI %d\n", i);
                } else if (at[i].a_type == A_COMMA) {
                    j = 0;
                    //fprintf(stderr,"A_COMMA %d\n", i);
                } else if (++j > maxnargs) {
                    maxnargs = j;
                }
            }
            //fprintf(stderr,"maxnargs = %d\n", maxnargs);
        }
        if (maxnargs <= SMALLMSG)
            mstack = smallstack;
        else ATOMS_ALLOCA(mstack, maxnargs);
#else
            /* just pessimistically allocate enough to hold everything
            at once.  This turned out to run slower in a simple benchmark
            I tried, perhaps because the extra memory allocation
            hurt the cache hit rate. */
        maxnargs = ac;
        ATOMS_ALLOCA(mstack, maxnargs);
#endif

    }
    msp = mstack;
    //fprintf(stderr,"maxnargs=%d ac=%d argc=%d\n", maxnargs, ac, argc);
    //static t_atom *ems = mstack+MSTACKSIZE;
    while (1)
    {
        t_pd *nexttarget;
            /* get a target. */
        while (!target)
        {
            t_symbol *s;
            while (ac && (at->a_type == A_SEMI || at->a_type == A_COMMA))
                ac--,  at++;
            if (!ac) break;
            if (at->a_type == A_DOLLAR)
            {
                /* would it make sense to consider $@ here? */
                if (at->a_w.w_index <= 0 || at->a_w.w_index > argc)
                {
                    binbuf_error(init_target,
                        "$%d: %s",
                        at->a_w.w_index,
                        (at->a_w.w_index == 0 ?
                            "symbol needed as message destination" :
                            "not enough arguments supplied"));
                    goto cleanup; 
                }
                else if (argv[at->a_w.w_index-1].a_type != A_SYMBOL)
                {
                    binbuf_error(init_target,
                        "$%d: symbol needed as message destination",
                        at->a_w.w_index);
                    goto cleanup; 
                }
                else s = argv[at->a_w.w_index-1].a_w.w_symbol;
            }
            else if (at->a_type == A_DOLLSYM)
            {
                if (!(s = binbuf_dorealizedollsym(init_target, at->a_w.w_symbol,
                    argc, argv, 0)))
                {
                    binbuf_error(init_target,
                        "$%s: not enough arguments supplied",
                        at->a_w.w_symbol->s_name);
                    goto cleanup;
                }
            }
            else s = atom_getsymbol(at);
            if (!(target = s->s_thing))
            {
                binbuf_error(init_target,
                "%s: no such object", s->s_name);
            cleanup:
                do at++, ac--;
                while (ac && at->a_type != A_SEMI);
                    /* LATER eat args until semicolon and continue */
                continue;
            }
            else
            {
                at++, ac--;
                break;
            }
        }
        if (!ac) break;
        nargs = 0;
        nexttarget = target;
        while (1)
        {
            t_symbol *s9;
            if (!ac) goto gotmess;
            switch (at->a_type)
            {
            case A_SEMI:
                    /* semis and commas in new message just get bashed to
                    a symbol.  This is needed so you can pass them to "expr." */
                if (target == &pd_objectmaker)
                {
                    SETSYMBOL(msp, gensym(";"));
                    break;
                }
                else
                {
                    nexttarget = 0;
                    goto gotmess;
                }
            case A_COMMA:
                if (target == &pd_objectmaker)
                {
                    SETSYMBOL(msp, gensym(","));
                    break;
                }
                else goto gotmess;
            case A_FLOAT:
            case A_SYMBOL:
                *msp = *at;
                break;
            case A_DOLLAR:
                if (at->a_w.w_index==DOLLARALL)
                { /* JMZ: $@ expansion */
                    int i;
                    //if(msp+argc >= ems) 
                    //{
                    //    error("message stack overflow");
                    //    goto broken;
                    //}
                    for (i=0; i<argc; i++)
                    {
                        //fprintf(stderr, "@: %d %d\n", i, maxnargs);
                        *msp++=argv[i];
                        nargs++;
                        ac--;
                    }
                    msp--;
                    nargs--;
                    //fprintf(stderr,"x->b_n=%d ac=%d maxnargs=%d "
                    //               "nargs=%d argc=%d\n",
                    //    x->b_n, ac, maxnargs, nargs, argc);
                }
                else if (at->a_w.w_index > 0 && at->a_w.w_index <= argc)
                    *msp = argv[at->a_w.w_index-1];
                else if (at->a_w.w_index == 0)
                    SETFLOAT(msp, canvas_getdollarzero(init_target));
                else
                {
                    if (target == &pd_objectmaker)
                        SETFLOAT(msp, 0);
                    else
                    {
                        binbuf_error(init_target,
                            "$%d: argument number out of range",
                            at->a_w.w_index);
                        SETFLOAT(msp, 0);
                    }
                }
                break;
            case A_DOLLSYM:
                s9 = binbuf_dorealizedollsym(init_target, at->a_w.w_symbol,
                    argc, argv, target == &pd_objectmaker);
                if (!s9)
                {
                    binbuf_error(init_target,
                        "%s: argument number out of range",
                        at->a_w.w_symbol->s_name);
                    SETSYMBOL(msp, at->a_w.w_symbol);
                }
                else SETSYMBOL(msp, s9);
                break;
            default:
                bug("bad item in binbuf");
                goto broken;
            }
            msp++;
            ac--;
            at++;
            nargs++;
        }
    gotmess:
        if (nargs)
        {
            switch (mstack->a_type)
            {
            case A_SYMBOL:
                typedmess(target, mstack->a_w.w_symbol, nargs-1, mstack+1);
                break;
            case A_FLOAT:
                if (nargs == 1) pd_float(target, mstack->a_w.w_float);
                else pd_list(target, 0, nargs, mstack);
                break;
            case A_BLOB: /* MP 20070106 blob type */
                if (nargs == 1) pd_blob(target, mstack->a_w.w_blob);
                else pd_list(target, 0, nargs, mstack);
                break;
            }
        }
        msp = mstack;
        if (!ac) break;
        target = nexttarget;
        at++;
        ac--;
    }
broken: 
    if (maxnargs > SMALLMSG)
         ATOMS_FREEA(mstack, maxnargs);
}

int binbuf_read(t_binbuf *b, const char *filename, const char *dirname, int crflag)
{
    long length;
    int fd;
    int readret;
    char *buf;
    char namebuf[MAXPDSTRING];

    if (*dirname)
        snprintf(namebuf, MAXPDSTRING-1, "%s/%s", dirname, filename);
    else
        snprintf(namebuf, MAXPDSTRING-1, "%s", filename);
    namebuf[MAXPDSTRING-1] = 0;

    if ((fd = sys_open(namebuf, 0)) < 0)
    {
        //fprintf(stderr, "open: ");
        perror(namebuf);
        return (1);
    }
    if ((length = lseek(fd, 0, SEEK_END)) < 0 || lseek(fd, 0, SEEK_SET) < 0 
        || !(buf = t_getbytes(length)))
    {
        //fprintf(stderr, "lseek: ");
        perror(namebuf);
        sys_close(fd);
        return(1);
    }
    if ((readret = read(fd, buf, length)) < length)
    {
        //fprintf(stderr, "read (%d %ld) -> %d\n", fd, length, readret);
        perror(namebuf);
        sys_close(fd);
        t_freebytes(buf, length);
        return(1);
    }
        /* optionally map carriage return to semicolon */
    if (crflag)
    {
        int i;
        for (i = 0; i < length; i++)
            if (buf[i] == '\n')
                buf[i] = ';';
    }
    binbuf_text(b, buf, length);

#if 0
    startpost("binbuf_read "); postatom(b->b_n, b->b_vec); endpost();
#endif

    t_freebytes(buf, length);
    sys_close(fd);
    return (0);
}

    /* read a binbuf from a file, via the search patch of a canvas */
int binbuf_read_via_canvas(t_binbuf *b, const char *filename, const t_canvas *canvas,
    int crflag)
{
    int filedesc;
    char buf[MAXPDSTRING], *bufptr;
    if ((filedesc = canvas_open(canvas, filename, "",
        buf, &bufptr, MAXPDSTRING, 0)) < 0)
    {
        error("%s: can't open", filename);
        return (1);
    }
    else sys_close(filedesc);
    if (binbuf_read(b, bufptr, buf, crflag))
        return (1);
    else return (0);
}

    /* old version */
int binbuf_read_via_path(t_binbuf *b, const char *filename, char *dirname,
    int crflag)
{
    int filedesc;
    char buf[MAXPDSTRING], *bufptr;
    if ((filedesc = open_via_path(
        dirname, filename, "", buf, &bufptr, MAXPDSTRING, 0)) < 0)
    {
        error("%s: can't open", filename);
        return (1);
    }
    else sys_close(filedesc);
    if (binbuf_read(b, bufptr, buf, crflag))
        return (1);
    else return (0);
}

#define WBUFSIZE 4096
static t_binbuf *binbuf_convert(t_binbuf *oldb, int maxtopd);

    /* write a binbuf to a text file.  If "crflag" is set we suppress
    semicolons. */
int binbuf_write(const t_binbuf *x, const char *filename, char *dir, int crflag)
{
    FILE *f = 0;
    char sbuf[WBUFSIZE], fbuf[MAXPDSTRING], *bp = sbuf, *ep = sbuf + WBUFSIZE;
    t_atom *ap;
    int indx, deleteit = 0;
    int ncolumn = 0;

    if (*dir)
        snprintf(fbuf, MAXPDSTRING-1, "%s/%s", dir, filename);
    else
        snprintf(fbuf, MAXPDSTRING-1, "%s", filename);
    fbuf[MAXPDSTRING-1] = 0;

    if (!strcmp(filename + strlen(filename) - 4, ".pat") ||
        !strcmp(filename + strlen(filename) - 4, ".mxt"))
    {
        x = binbuf_convert(x, 0);
        deleteit = 1;
    }
    
    if (!(f = sys_fopen(fbuf, "w")))
        goto fail;
    for (ap = x->b_vec, indx = x->b_n; indx--; ap++)
    {
        int length;
            /* estimate how many characters will be needed.  Printing out
            symbols may need extra characters for inserting backslashes. */
        if (ap->a_type == A_SYMBOL || ap->a_type == A_DOLLSYM)
            length = 80 + strlen(ap->a_w.w_symbol->s_name);
        else length = 40;
        if (ep - bp < length)
        {
            if (fwrite(sbuf, bp-sbuf, 1, f) < 1)
                goto fail;
            bp = sbuf;
        }
        if ((ap->a_type == A_SEMI || ap->a_type == A_COMMA) &&
            bp > sbuf && bp[-1] == ' ') bp--;
        if (!crflag || ap->a_type != A_SEMI)
        {
            char bp2[WBUFSIZE];
            atom_string(ap, bp2, WBUFSIZE);
            strnescape(bp, bp2, (ep-bp)-2);
            //atom_string(ap, bp, (ep-bp)-2);
            length = strlen(bp);
            bp += length;
            ncolumn += length;
        }
        if (ap->a_type == A_SEMI || (!crflag && ncolumn > 65))
        {
            *bp++ = '\n';
            ncolumn = 0;
        }
        else
        {
            *bp++ = ' ';
            ncolumn++;
        }
    }
    if (fwrite(sbuf, bp-sbuf, 1, f) < 1)
        goto fail;


    if (fflush(f) != 0) 
        goto fail;

    if (deleteit)
        binbuf_free(x);
    fclose(f);
    return (0);
fail:
    if (deleteit)
        binbuf_free(x);
    if (f)
        fclose(f);
    return (1);
}

/* The following routine attempts to convert from max to pd or back.  The
max to pd direction is working OK but you will need to make lots of 
abstractions for objects like "gate" which don't exist in Pd.  conversion
from Pd to Max hasn't been tested for patches with subpatches yet!  */

#define MAXSTACK 1000

static t_binbuf *binbuf_convert(t_binbuf *oldb, int maxtopd)
{
    t_binbuf *newb = binbuf_new();
    t_atom *vec = oldb->b_vec;
    t_int n = oldb->b_n, nextindex, stackdepth = 0, stack[MAXSTACK] = {0},
        nobj = 0, i, gotfontsize = 0;
    t_atom outmess[MAXSTACK], *nextmess;
    t_float fontsize = 10;
    if (!maxtopd)
        binbuf_addv(newb, "ss;", gensym("max"), gensym("v2"));
    for (nextindex = 0; nextindex < n; )
    {
        int endmess, natom;
        char *first, *second, *third;
        for (endmess = nextindex; endmess < n && vec[endmess].a_type != A_SEMI;
            endmess++)
                ;
        if (endmess == n) break;
        if (endmess == nextindex || endmess == nextindex + 1
            || vec[nextindex].a_type != A_SYMBOL ||
                vec[nextindex+1].a_type != A_SYMBOL)
        {
            nextindex = endmess + 1;
            continue;
        }
        natom = endmess - nextindex;
        if (natom > MAXSTACK-10) natom = MAXSTACK-10;
        nextmess = vec + nextindex;
        first = nextmess->a_w.w_symbol->s_name;
        second = (nextmess+1)->a_w.w_symbol->s_name;
        if (maxtopd)
        {
                /* case 1: importing a ".pat" file into Pd. */
                
                /* dollar signs in file translate to symbols */
            for (i = 0; i < natom; i++)
            {
                if (nextmess[i].a_type == A_DOLLAR)
                {
                    char buf[100];
                    sprintf(buf, "$%d", nextmess[i].a_w.w_index);
                    SETSYMBOL(nextmess+i, gensym(buf));
                }
                else if (nextmess[i].a_type == A_DOLLSYM)
                {
                    char buf[100];
                    sprintf(buf, "%s", nextmess[i].a_w.w_symbol->s_name);
                    SETSYMBOL(nextmess+i, gensym(buf));
                }
            }
            if (!strcmp(first, "#N"))
            {
                if (!strcmp(second, "vpatcher"))
                {
                    if (stackdepth >= MAXSTACK)
                    {
                        error("stack depth exceeded: too many embedded patches");
                        return (newb);
                    }
                    stack[stackdepth] = nobj;
                    stackdepth++;
                    nobj = 0;
                    binbuf_addv(newb, "ssfffff;", 
                        gensym("#N"), gensym("canvas"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(4, natom, nextmess) -
                                atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess) -
                                atom_getfloatarg(3, natom, nextmess),
                            (t_float)sys_defaultfont);
                }
            }
            if (!strcmp(first, "#P"))
            {
                    /* drop initial "hidden" flag */
                if (!strcmp(second, "hidden"))
                {
                    nextmess++;
                    natom--;
                    second = (nextmess+1)->a_w.w_symbol->s_name;
                }
                if (natom >= 7 && !strcmp(second, "newobj")
                    && (ISSYMBOL(&nextmess[6], "patcher") ||
                        ISSYMBOL(&nextmess[6], "p")))
                {
                    binbuf_addv(newb, "ssffss;",
                        gensym("#X"), gensym("restore"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("pd"), atom_getsymbolarg(7, natom, nextmess));
                    if (stackdepth) stackdepth--;
                    nobj = stack[stackdepth];
                    nobj++;
                }
                else if (!strcmp(second, "newex") || !strcmp(second, "newobj"))
                {
                    t_symbol *classname =
                        atom_getsymbolarg(6, natom, nextmess);
                    if (classname == gensym("trigger") ||
                        classname == gensym("t"))
                    {
                        for (i = 7; i < natom; i++)
                            if (nextmess[i].a_type == A_SYMBOL &&
                                nextmess[i].a_w.w_symbol == gensym("i"))
                                    nextmess[i].a_w.w_symbol = gensym("f");
                    }
                    if (classname == gensym("table"))
                        classname = gensym("TABLE");
                    SETSYMBOL(outmess, gensym("#X"));
                    SETSYMBOL(outmess + 1, gensym("obj"));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    SETSYMBOL(outmess+4, classname);
                    for (i = 7; i < natom; i++)
                        outmess[i-2] = nextmess[i];
                    SETSEMI(outmess + natom - 2);
                    binbuf_add(newb, natom - 1, outmess);
                    nobj++;
                }
                else if (!strcmp(second, "message") || 
                    !strcmp(second, "comment"))
                {
                    SETSYMBOL(outmess, gensym("#X"));
                    SETSYMBOL(outmess + 1, gensym(
                        (strcmp(second, "message") ? "text" : "msg")));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    for (i = 6; i < natom; i++)
                        outmess[i-2] = nextmess[i];
                    SETSEMI(outmess + natom - 2);
                    binbuf_add(newb, natom - 1, outmess);
                    nobj++;
                }
                else if (!strcmp(second, "button"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("bng"));
                    nobj++;
                }
                else if (!strcmp(second, "number") || !strcmp(second, "flonum"))
                {
                    binbuf_addv(newb, "ssff;",
                        gensym("#X"), gensym("floatatom"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess));
                    nobj++;
                }
                else if (!strcmp(second, "slider"))
                {
                    t_float inc = atom_getfloatarg(7, natom, nextmess);
                    if (inc <= 0)
                        inc = 1;
                    binbuf_addv(newb, "ssffsffffffsssfffffffff;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("vsl"),
                        atom_getfloatarg(4, natom, nextmess),
                        atom_getfloatarg(5, natom, nextmess),
                        atom_getfloatarg(6, natom, nextmess),
                        atom_getfloatarg(6, natom, nextmess)
                            + (atom_getfloatarg(5, natom, nextmess) - 1) * inc,
                        0., 0.,
                        gensym("empty"), gensym("empty"), gensym("empty"),
                        0., -8., 0., 8., -262144., -1., -1., 0., 1.);
                    nobj++;
                }
                else if (!strcmp(second, "toggle"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym("tgl"));
                    nobj++;
                }
                else if (!strcmp(second, "inlet"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym((natom > 5 ? "inlet~" : "inlet"))); 
                    nobj++;
                }
                else if (!strcmp(second, "outlet"))
                {
                    binbuf_addv(newb, "ssffs;",
                        gensym("#X"), gensym("obj"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        gensym((natom > 5 ? "outlet~" : "outlet"))); 
                    nobj++;
                }
                else if (!strcmp(second, "user"))
                {
                    third = (nextmess+2)->a_w.w_symbol->s_name;
                    if (!strcmp(third, "hslider"))
                    {
                        t_float range = atom_getfloatarg(7, natom, nextmess);
                        //t_float multiplier = atom_getfloatarg(8, natom, nextmess);
                        t_float offset = atom_getfloatarg(9, natom, nextmess);
                        binbuf_addv(newb, "ssffsffffffsssfffffffff;",
                                    gensym("#X"), gensym("obj"),
                                    atom_getfloatarg(3, natom, nextmess),
                                    atom_getfloatarg(4, natom, nextmess),
                                    gensym("hsl"),
                                    atom_getfloatarg(6, natom, nextmess),
                                    atom_getfloatarg(5, natom, nextmess),
                                    offset,
                                    range + offset,
                                    0., 0.,
                                    gensym("empty"), gensym("empty"), gensym("empty"),
                                    0., -8., 0., 8., -262144., -1., -1., 0., 1.); 
                   }
                    else if (!strcmp(third, "uslider"))
                    {
                        t_float range = atom_getfloatarg(7, natom, nextmess);
                        //t_float multiplier = atom_getfloatarg(8, natom, nextmess);
                        t_float offset = atom_getfloatarg(9, natom, nextmess);
                        binbuf_addv(newb, "ssffsffffffsssfffffffff;",
                                    gensym("#X"), gensym("obj"),
                                    atom_getfloatarg(3, natom, nextmess),
                                    atom_getfloatarg(4, natom, nextmess),
                                    gensym("vsl"),
                                    atom_getfloatarg(5, natom, nextmess),
                                    atom_getfloatarg(6, natom, nextmess),
                                    offset,
                                    range + offset,
                                    0., 0.,
                                    gensym("empty"), gensym("empty"), gensym("empty"),
                                    0., -8., 0., 8., -262144., -1., -1., 0., 1.);
                    }
                    else
                        binbuf_addv(newb, "ssffs;",
                                    gensym("#X"), gensym("obj"),
                                    atom_getfloatarg(3, natom, nextmess),
                                    atom_getfloatarg(4, natom, nextmess),
                                    atom_getsymbolarg(2, natom, nextmess));
                    nobj++;
                }
                else if (!strcmp(second, "connect")||
                    !strcmp(second, "fasten"))
                {
                    binbuf_addv(newb, "ssffff;",
                        gensym("#X"), gensym("connect"),
                        nobj - atom_getfloatarg(2, natom, nextmess) - 1,
                        atom_getfloatarg(3, natom, nextmess),
                        nobj - atom_getfloatarg(4, natom, nextmess) - 1,
                        atom_getfloatarg(5, natom, nextmess)); 
                }
            }
        }
        else        /* Pd to Max */
        {
            if (!strcmp(first, "#N"))
            {
                if (!strcmp(second, "canvas"))
                {
                    if (stackdepth >= MAXSTACK)
                    {
                        post("too many embedded patches");
                        return (newb);
                    }
                    stack[stackdepth] = nobj;
                    stackdepth++;
                    nobj = 0;
                    if(!gotfontsize) { /* only the first canvas sets the font size */
                        fontsize = atom_getfloatarg(6, natom, nextmess);
                        gotfontsize = 1;
                    }
                    t_float x = atom_getfloatarg(2, natom, nextmess);
                    t_float y = atom_getfloatarg(3, natom, nextmess);
                    binbuf_addv(newb, "ssffff;", 
                        gensym("#N"), gensym("vpatcher"),
                            x, y,
                            atom_getfloatarg(4, natom, nextmess) + x,
                            atom_getfloatarg(5, natom, nextmess) + y);
                }
            }
            if (!strcmp(first, "#X"))
            {
                if (natom >= 5 && !strcmp(second, "restore")
                    && (ISSYMBOL (&nextmess[4], "pd")))
                {
                    binbuf_addv(newb, "ss;", gensym("#P"), gensym("pop"));
                    SETSYMBOL(outmess, gensym("#P"));
                    SETSYMBOL(outmess + 1, gensym("newobj"));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    SETFLOAT(outmess + 4, 50.*(natom-5));
                    SETFLOAT(outmess + 5, fontsize);
                    SETSYMBOL(outmess + 6, gensym("p"));
                    for (i = 5; i < natom; i++)
                        outmess[i+2] = nextmess[i];
                    SETSEMI(outmess + natom + 2);
                    binbuf_add(newb, natom + 3, outmess);
                    if (stackdepth) stackdepth--;
                    nobj = stack[stackdepth];
                    nobj++;
                }
                else if (!strcmp(second, "obj"))
                {
                    t_symbol *classname =
                        atom_getsymbolarg(4, natom, nextmess);
                    if (classname == gensym("inlet"))
                        binbuf_addv(newb, "ssfff;", gensym("#P"),
                            gensym("inlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize);
                    else if (classname == gensym("inlet~"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("inlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize, 1.);
                    else if (classname == gensym("outlet"))
                        binbuf_addv(newb, "ssfff;", gensym("#P"),
                            gensym("outlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize);
                    else if (classname == gensym("outlet~"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("outlet"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            10. + fontsize, 1.);
                    else if (classname == gensym("bng"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("button"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess), 0.);
                    else if (classname == gensym("tgl"))
                        binbuf_addv(newb, "ssffff;", gensym("#P"),
                            gensym("toggle"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess), 0.);
                    else if (classname == gensym("vsl"))
                        binbuf_addv(newb, "ssffffff;", gensym("#P"),
                            gensym("slider"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess),
                            atom_getfloatarg(6, natom, nextmess),
                            (atom_getfloatarg(8, natom, nextmess) -
                                atom_getfloatarg(7, natom, nextmess)) /
                                    (atom_getfloatarg(6, natom, nextmess) == 1? 1 :
                                         atom_getfloatarg(6, natom, nextmess) - 1),
                            atom_getfloatarg(7, natom, nextmess));
                    else if (classname == gensym("hsl")) 
                    {
                        t_float slmin = atom_getfloatarg(7, natom, nextmess);
                        t_float slmax = atom_getfloatarg(8, natom, nextmess);
                        binbuf_addv(newb, "sssffffffff;", gensym("#P"),
                            gensym("user"),
                            gensym("hslider"),
                            atom_getfloatarg(2, natom, nextmess),
                            atom_getfloatarg(3, natom, nextmess),
                            atom_getfloatarg(6, natom, nextmess),
                            atom_getfloatarg(5, natom, nextmess),
                            slmax - slmin + 1, /* range */
                            1.,            /* multiplier */
                            slmin,         /* offset */
                            0.);
                    }
                    else if ( (classname == gensym("trigger")) ||
                              (classname == gensym("t")) )
                    {
                        SETSYMBOL(outmess, gensym("#P"));
                        SETSYMBOL(outmess + 1, gensym("newex"));
                        outmess[2] = nextmess[2];
                        outmess[3] = nextmess[3];
                        SETFLOAT(outmess + 4, 50.*(natom-4));
                        SETFLOAT(outmess + 5, fontsize);
                        outmess[6] = nextmess[4];
                        t_symbol *arg;
                        for (i = 5; i < natom; i++) {
                            arg = atom_getsymbolarg(i, natom, nextmess);
                            if (arg == gensym("a"))
                                SETSYMBOL(outmess + i + 2, gensym("l"));
                            else if (arg == gensym("anything"))
                                SETSYMBOL(outmess + i + 2, gensym("l"));
                            else if (arg == gensym("bang"))
                                SETSYMBOL(outmess + i + 2, gensym("b"));
                            else if (arg == gensym("float"))
                                SETSYMBOL(outmess + i + 2, gensym("f"));
                            else if (arg == gensym("list"))
                                SETSYMBOL(outmess + i + 2, gensym("l"));
                            else if (arg == gensym("symbol"))
                                SETSYMBOL(outmess + i + 2, gensym("s"));
                            else 
                                outmess[i+2] = nextmess[i];
                        }
                        SETSEMI(outmess + natom + 2);
                        binbuf_add(newb, natom + 3, outmess);
                    }
                    else
                    {
                        SETSYMBOL(outmess, gensym("#P"));
                        SETSYMBOL(outmess + 1, gensym("newex"));
                        outmess[2] = nextmess[2];
                        outmess[3] = nextmess[3];
                        SETFLOAT(outmess + 4, 50.*(natom-4));
                        SETFLOAT(outmess + 5, fontsize);
                        for (i = 4; i < natom; i++)
                            outmess[i+2] = nextmess[i];
                        if (classname == gensym("osc~"))
                            SETSYMBOL(outmess + 6, gensym("cycle~"));
                        SETSEMI(outmess + natom + 2);
                        binbuf_add(newb, natom + 3, outmess);
                    }
                    nobj++;
                
                }
                else if (!strcmp(second, "msg") || 
                    !strcmp(second, "text"))
                {
                    SETSYMBOL(outmess, gensym("#P"));
                    SETSYMBOL(outmess + 1, gensym(
                        (strcmp(second, "msg") ? "comment" : "message")));
                    outmess[2] = nextmess[2];
                    outmess[3] = nextmess[3];
                    SETFLOAT(outmess + 4, 50.*(natom-4));
                    SETFLOAT(outmess + 5, fontsize);
                    for (i = 4; i < natom; i++)
                        outmess[i+2] = nextmess[i];
                    SETSEMI(outmess + natom + 2);
                    binbuf_add(newb, natom + 3, outmess);
                    nobj++;
                }
                else if (!strcmp(second, "floatatom"))
                {
                    t_float width = atom_getfloatarg(4, natom, nextmess)*fontsize;
                    if(width<8) width = 150; /* if pd width=0, set it big */
                    binbuf_addv(newb, "ssfff;",
                        gensym("#P"), gensym("flonum"),
                        atom_getfloatarg(2, natom, nextmess),
                        atom_getfloatarg(3, natom, nextmess),
                        width);
                    nobj++;
                }
                else if (!strcmp(second, "connect"))
                {
                    binbuf_addv(newb, "ssffff;",
                        gensym("#P"), gensym("connect"),
                        nobj - atom_getfloatarg(2, natom, nextmess) - 1,
                        atom_getfloatarg(3, natom, nextmess),
                        nobj - atom_getfloatarg(4, natom, nextmess) - 1,
                        atom_getfloatarg(5, natom, nextmess)); 
                }
            }
        }
        nextindex = endmess + 1;
    }
    if (!maxtopd)
        binbuf_addv(newb, "ss;", gensym("#P"), gensym("pop"));
#if 0
    binbuf_write(newb, "import-result.pd", "/tmp", 0);
#endif
    return (newb);
}

    /* function to support searching */
int binbuf_match(t_binbuf *inbuf, t_binbuf *searchbuf, int wholeword)
{
    int indexin, nmatched;
    char dollarstr[MAXPDSTRING];
    for (indexin = 0; indexin <= inbuf->b_n - searchbuf->b_n; indexin++)
    {
        for (nmatched = 0; nmatched < searchbuf->b_n; nmatched++)
        {
            t_atom *a1 = &inbuf->b_vec[indexin + nmatched], 
                *a2 = &searchbuf->b_vec[nmatched];
            if (a1->a_type == A_SEMI || a1->a_type == A_COMMA)
            {
                //post("A_SEMI OR A_COMMA");
                if (a2->a_type != a1->a_type)
                    goto nomatch;
            }
            else if (a1->a_type == A_FLOAT)
            {
                //post("A_FLOAT");
                if (a2->a_type != a1->a_type ||
                    a1->a_w.w_float != a2->a_w.w_float)
                        goto nomatch;
            }
            else if (a1->a_type == A_DOLLAR)
            {
                //post("A_DOLLAR");
                if (a2->a_type != a1->a_type || 
                    a1->a_w.w_index != a2->a_w.w_index)
                        goto nomatch;
            }
            else if (a1->a_type == A_SYMBOL || a1->a_type == A_DOLLSYM)
            {
                //post("A_SYMBOL OR A_DOLLSYM");
                if (a2->a_type == A_SYMBOL || a2->a_type == A_DOLLSYM)
                {
                    /*
                    post("strstr=%d A:<%s> B:<%s>", 
                        (strstr(a1->a_w.w_symbol->s_name,
                            a2->a_w.w_symbol->s_name) == NULL ? 0 : 1),
                        a1->a_w.w_symbol->s_name, a2->a_w.w_symbol->s_name);
                    */
                    if ((wholeword && a1->a_w.w_symbol != a2->a_w.w_symbol)
                    || (!wholeword &&  !strstr(a1->a_w.w_symbol->s_name,
                                        a2->a_w.w_symbol->s_name)))
                        goto nomatch;
                } else if (a2->a_type == A_DOLLAR) {
                    sprintf(dollarstr, "$%d", a2->a_w.w_index);
                    //post("resulting dolsym=<%s>", dollarstr);
                    if (!strstr(a1->a_w.w_symbol->s_name,
                                        dollarstr))
                        goto nomatch;
                } else {
                    goto nomatch;
                }
            }           
        }
        return (1);
    nomatch: ;
    }
    return (0);
}

void pd_doloadbang(void);

/* LATER make this evaluate the file on-the-fly. */
/* LATER figure out how to log errors */
void binbuf_evalfile(t_symbol *name, t_symbol *dir)
{
    t_binbuf *b = binbuf_new();
    int import = !strcmp(name->s_name + strlen(name->s_name) - 4, ".pat") ||
        !strcmp(name->s_name + strlen(name->s_name) - 4, ".mxt");
    int dspstate = canvas_suspend_dsp();
        /* set filename so that new canvases can pick them up */
    glob_setfilename(0, name, dir);
    if (binbuf_read(b, name->s_name, dir->s_name, 0))
    {
        error("%s: read failed: %s", name->s_name, strerror(errno));
    }
    else
    {
            /* save bindings of symbols #N, #A (and restore afterward) */
        t_pd *bounda = gensym("#A")->s_thing, *boundn = s__N.s_thing;
        gensym("#A")->s_thing = 0;
        s__N.s_thing = &pd_canvasmaker;
        if (import)
        {
            t_binbuf *newb = binbuf_convert(b, 1);
            binbuf_free(b);
            b = newb;
        }
        binbuf_eval(b, 0, 0, 0);
        if (s__X.s_thing && pd_class(s__X.s_thing) == canvas_class)
            canvas_initbang((t_canvas *)(s__X.s_thing));
        gensym("#A")->s_thing = bounda;
        s__N.s_thing = boundn;
    }
    glob_setfilename(0, &s_, &s_);
    binbuf_free(b);
    canvas_resume_dsp(dspstate);
}

    /* save a text object to a binbuf for a file or copy buf */
void binbuf_savetext(t_binbuf *bfrom, t_binbuf *bto)
{
    int k, n = binbuf_getnatom(bfrom);
    t_atom *ap = binbuf_getvec(bfrom), at;
    for (k = 0; k < n; k++)
    {
        if (ap[k].a_type == A_FLOAT ||
            (ap[k].a_type == A_SYMBOL &&
                !strchr(ap[k].a_w.w_symbol->s_name, ';') &&
                !strchr(ap[k].a_w.w_symbol->s_name, ',') &&
                !strchr(ap[k].a_w.w_symbol->s_name, '$')))
                    binbuf_add(bto, 1, &ap[k]);
        else
        {
            char buf[MAXPDSTRING+1];
            atom_string(&ap[k], buf, MAXPDSTRING);
            SETSYMBOL(&at, gensym(buf));
            binbuf_add(bto, 1, &at);
        }
    }
    binbuf_addsemi(bto);
}
