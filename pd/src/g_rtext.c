/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* changes by Thomas Musil IEM KUG Graz Austria 2001 */
/* have to insert gui-objects into editor-list */
/* all changes are labeled with      iemlib      */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include "s_utf8.h"

#define LMARGIN 2
#define RMARGIN 2
#define TMARGIN 3
/* for some reason, it draws text 1 pixel lower on Mac OS X */
#ifdef __APPLE__
#define BMARGIN 1
#else
#define BMARGIN 1
#endif

#define SEND_FIRST 1
#define SEND_UPDATE 2
#define SEND_CHECK 0

// selection for shift+arrow selecting
// 0 = none;
// 1 = start;
// 2 = end;
static int last_sel = 0;

struct _rtext
{
    char *x_buf;    /*-- raw byte string, assumed UTF-8 encoded (moo) --*/
    int x_bufsize;  /*-- byte length --*/
    int x_selstart; /*-- byte offset --*/
    int x_selend;   /*-- byte offset --*/
    int x_active;
    int x_active_nlines; /* number of lines that may be different from normal
                            used to update cords as the edited object height
                            changes */
    int x_drawnwidth;
    int x_drawnheight;
    t_text *x_text;
    t_glist *x_glist;
    char x_tag[50];
    struct _rtext *x_next;
};

t_rtext *rtext_new(t_glist *glist, t_text *who)
{
    t_rtext *x = (t_rtext *)getbytes(sizeof *x);
    x->x_text = who;
    x->x_glist = glist;
    x->x_next = glist->gl_editor->e_rtext;
    x->x_selstart = x->x_selend = x->x_active =
        x->x_active_nlines = x->x_drawnwidth = x->x_drawnheight = 0;
    if (who->te_type == T_TEXT)
        binbuf_getrawtext(who->te_binbuf, &x->x_buf, &x->x_bufsize);
    else
        binbuf_gettext(who->te_binbuf, &x->x_buf, &x->x_bufsize);
    glist->gl_editor->e_rtext = x;
    // here we use a more complex tag which will later help us properly
    // select objects inside a gop on its parent that are otherwise not
    // supposed to be there (they don't belong to that canvas). See
    // in pd.tk pdtk_select_all_gop_widgets function and how it affects
    // draw data structures that are displayed via gop (Ico 20140831)
    sprintf(x->x_tag, ".x%zx.t%zx", (t_uint)glist_getcanvas(x->x_glist),
        (t_int)x);
    return (x);
}

static t_rtext *rtext_entered;

void rtext_free(t_rtext *x)
{
    t_editor *e = x->x_glist->gl_editor;
    if (e->e_textedfor == x)
        e->e_textedfor = 0;
    if (e->e_rtext == x)
        e->e_rtext = x->x_next;
    else
    {
        t_rtext *e2;
        for (e2 = e->e_rtext; e2; e2 = e2->x_next)
            if (e2->x_next == x)
        {
            e2->x_next = x->x_next;
            break;
        }
    }
    if (rtext_entered == x) rtext_entered = 0;
    freebytes(x->x_buf, x->x_bufsize);
    freebytes(x, sizeof *x);
}

const char *rtext_gettag(t_rtext *x)
{
    return (x->x_tag);
}

void rtext_gettext(t_rtext *x, char **buf, int *bufsize)
{
    *buf = x->x_buf;
    *bufsize = x->x_bufsize;
}

// ico 2023-08-08: used to get null terminated text
// to prevent reading past the allocated memory
void rtext_getterminatedtext(t_rtext *x, char *result)
{
    if (x)
    {
        char *buf;
        int bufsize;
        rtext_gettext(x, &buf, &bufsize);
        int final_size =
            (bufsize >= FILENAME_MAX ? FILENAME_MAX - 1 : bufsize);
        strncpy(result, buf, final_size);
        result[final_size] = '\0';
    }
}

void rtext_settext(t_rtext *x, char *buf, int bufsize)
{
    if (x->x_bufsize) freebytes(x->x_buf, x->x_bufsize);
    x->x_buf = buf;
    x->x_bufsize = bufsize;
}

void rtext_getseltext(t_rtext *x, char **buf, int *bufsize)
{
    *buf = x->x_buf + x->x_selstart;
    *bufsize = x->x_selend - x->x_selstart;
}

/* convert t_text te_type symbol for use as a Tk tag */
static t_symbol *rtext_gettype(t_rtext *x)
{
    switch (x->x_text->te_type) 
    {
    case T_TEXT: return gensym("text");
    case T_OBJECT: return gensym("obj");
    case T_MESSAGE: return gensym("msg");
    case T_ATOM: return gensym("atom");
    }
    return (&s_);
}

/* LATER deal with tcl-significant characters */

/* firstone(), lastone()
 *  + returns byte offset of (first|last) occurrence of 'c' in 's[0..n-1]', or
 *    -1 if none was found
 *  + 's' is a raw byte string
 *  + 'c' is a byte value
 *  + 'n' is the length (in bytes) of the prefix of 's' to be searched.
 *  + we could make these functions work on logical characters in utf8 strings,
 *    but we don't really need to...
 */
static int firstone(char *s, int c, int n)
{
    char *s2 = s + n;
    int i = 0;
    while (s != s2)
    {
        //fprintf(stderr,"s=<%s> n=%d s=%d c=%d s2=%d\n", s, n, *s, c, *s2);
        if (*s == c)
        {
            //fprintf(stderr,"DONE\n");
            return (i);
        }
        i++;
        s++;
    }
    //fprintf(stderr,"FAILED\n");
    return (-1);
}

static int lastone(char *s, int c, int n)
{
    char *s2 = s + n;
    while (s2 != s)
    {
        s2--;
        n--;
        if (*s2 == c) return (n);
    }
    return (-1);
}

    /* the following routine computes line breaks and carries out
    some action which could be:
        SEND_FIRST - draw the box  for the first time
        SEND_UPDATE - redraw the updated box
        otherwise - don't draw, just calculate.
    Called with *widthp and *heightp as coordinates of
    a test point, the routine reports the index of the character found
    there in *indexp.  *widthp and *heightp are set to the width and height
    of the entire text in pixels.
    */

   /*-- moo: 
    * + some variables from the original version have been renamed
    * + variables with a "_b" suffix are raw byte strings, lengths, or offsets
    * + variables with a "_c" suffix are logical character lengths or offsets
    *   (assuming valid UTF-8 encoded byte string in x->x_buf)
    * + a fair amount of O(n) computations required to convert between raw byte
    *   offsets (needed by the C side) and logical character offsets (needed by
    *   the GUI)
    */

    /* LATER get this and sys_vgui to work together properly,
        breaking up messages as needed.  As of now, there's
        a limit of 1950 characters, imposed by sys_vgui(). */
#define UPBUFSIZE 4000
#define BOXWIDTH 60

/* Older (pre-8.3.4) TCL versions handle text selection differently; this
flag is set from the GUI if this happens.  LATER take this out: early 2006? */

extern int sys_oldtclversion;           
extern int is_dropdown(t_text *x);
extern t_class *gatom_class;

// Parse rtf tags. The syntax is [bhisu]|=color, optionally preceded by '/'
// indicating an end tag, and enclosed in '<' and '>'. Here, 'color' indicates
// an HTML color spec which can be either an (alphanumeric) color name, or a
// color triplet beginning with '#'. NOTE: We want to be as specific as
// possible here, since help patches also use <...> as ad-hoc syntax for
// certain meta variables, in which case we want to treat them as literals.
static int is_tag_char(char c, int j)
{
    // simple DFA: j==1 indicates the beginning og the parse; state 0 is at
    // the beginning of the tag, state 1 is when we've read an initial '/',
    // state 2 when we've read the '=' prefix starting a color spec, and state
    // -1 is error (we've reached a final state where we can't accept any more
    // characters).
    static int state = 0;
    int ret = 0;
    if (j == 1) state = 0;
    if (state == 0) {
        ret = strchr("bhisu=/", c) != NULL;
        state = c=='/' ? 1 : c=='=' ? 2 : -1;
    } else if (state == 1) {
        ret = strchr("bhisu=", c) != NULL;
        state = c=='=' ? 2 : -1;
    } else if (state == 2) {
        // here we keep eating away all chars that can be in a color spec
        ret = islower(c) || isdigit(c) || c == '#';
    }
    return ret;
}

static void rtext_senditup(t_rtext *x, int action, int *widthp, int *heightp,
    int *indexp)
{
    //fprintf(stderr,"rtext_senditup <%s>\n", x->x_buf);
    if (x)
    {
        int iscomment = 0;
        int isbroken = 0;
        if (pd_class(&x->x_text->te_pd) == text_class)
        {
            if (x->x_text->te_type == T_TEXT)
                iscomment = 1;
            else
                isbroken = 1;
        }
        //post("iscomment=%d <%s>", iscomment, x->x_buf);
        char smallbuf[200] = { '\0' }, *tempbuf;
        int outchars_b = 0, nlines = 0, ncolumns = 0,
            pixwide, pixhigh, font, fontwidth, fontheight, findx, findy;//, bvfound;
        int reportedindex = 0;
        t_canvas *canvas = glist_getcanvas(x->x_glist);
        int widthspec_c = x->x_text->te_width; // width if any specified
        //post("widthspec_c=%d", widthspec_c);
        // width limit in chars
        int widthlimit_c = (widthspec_c ? widthspec_c : BOXWIDTH);
        int inindex_b = 0; // index location in the buffer
        int inindex_c = 0; // index location in the u8 chars
        int selstart_b = 0, selend_b = 0; // selection start and end
        // buffer size in u8 chars
        //post("\nbuf = <%s> | last 2 chars = %d %d", x->x_buf, x->x_buf[x->x_bufsize-1], x->x_buf[x->x_bufsize]);
        int x_bufsize_c = u8_charnum(x->x_buf, x->x_bufsize);
            /* if we're a GOP (the new, "goprect" style) borrow the font size
            from the inside to preserve the spacing */
        //post("x_bufsize=%d x_bufsize_c=%d", x->x_bufsize, x_bufsize_c);
        if (pd_class(&x->x_text->te_pd) == canvas_class &&
            ((t_glist *)(x->x_text))->gl_isgraph &&
            ((t_glist *)(x->x_text))->gl_goprect)
                font =  glist_getfont((t_glist *)(x->x_text));
        else font = glist_getfont(x->x_glist);
        fontwidth = sys_fontwidth(font);
        fontheight = sys_fontheight(font);
        // calculating x and y in pixels
        findx = (*widthp + (fontwidth/2)) / fontwidth;
        findy = *heightp / fontheight;
        if (x->x_bufsize >= 100)
             tempbuf = (char *)t_getbytes(2 * x->x_bufsize + 1);
        else tempbuf = smallbuf;
        int large = 0;
        while (x_bufsize_c - inindex_c > 0)
        {
            //bvfound = 0;
            int inchars_b  = x->x_bufsize - inindex_b;
            int inchars_c  = x_bufsize_c  - inindex_c;
            int maxindex_c =
                (inchars_c > widthlimit_c ? widthlimit_c : inchars_c);
            int maxindex_b = u8_offset(x->x_buf + inindex_b, maxindex_c,
                x->x_bufsize - inindex_b);
            // deal with rich text tags in the input (comment text)
            int tag_width = 0, extra_width = 0;
             if (x->x_text->te_type == T_TEXT) {
                int extra = 0, in_tag = 0;
                for (int i = inindex_b; i < inindex_b+maxindex_b &&
                         x->x_buf[i] != '\n' && x->x_buf[i] != '\v'; i++) {
                    if (x->x_buf[i] == '<' &&
                        // skip escaped tags
                        i+1 < inindex_b+maxindex_b && x->x_buf[i+1] != '!') {
                        int j;
                        for (j = i+1; j < inindex_b+maxindex_b &&
                                 is_tag_char(x->x_buf[j], j-i); j++) ;
                        if (j < inindex_b+maxindex_b && x->x_buf[j] == '>') {
                            if (strncmp(x->x_buf+i+1, "h", j-i-1) == 0)
                                large = 1;
                            else if (strncmp(x->x_buf+i+1, "/h", j-i-1) == 0)
                                large = 0;
                            tag_width += j-i+1;
                            in_tag = 1;
                        }
                    } else if (in_tag && x->x_buf[i] == '>') {
                        in_tag = 0;
                    } else if (large && !in_tag) {
                        extra++;
                    }
                }
                // ag: This is a rough estimate of extra width needed to
                // accommodate the large font size of headers.
                extra_width = (int)(extra*0.2+0.5);
            }
            int tag_offs = tag_width - extra_width;
            if (tag_offs && maxindex_c == widthlimit_c) {
                // recalculate offsets
                maxindex_c += tag_offs;
                maxindex_b += tag_offs;
            }
            int eatchar = 1;
            //fprintf(stderr, "firstone <%s> inindex_b=%d maxindex_b=%d\n", x->x_buf + inindex_b, inindex_b, maxindex_b);
            // ico@vt.edu 2021-08-06:
            // we add to foundit_b and foundit_bv one extra spot in case
            // the user-entered \n is right at the point of the maximum width
            // since we will eat this char, it is all good
            int foundit_b  = firstone(x->x_buf + inindex_b, '\n', maxindex_b + 1);
            //int oldinindex_b = 0;
            int foundit_c;
            //following deals with \v replacement for \n in multiline comments
            int foundit_bv  = firstone(x->x_buf + inindex_b, '\v', maxindex_b + 1);
            if ((foundit_bv < foundit_b && foundit_bv != -1) ||
                (foundit_b == -1 && foundit_bv != -1))
            {
                foundit_b = foundit_bv;
            }
            // ico@vt.edu 2021-04-16: if we are gatom, we need to pack everything
            // into a single line, so that the nw.js side can do accurate truncating
            if (pd_class(&x->x_text->te_pd) == gatom_class) {
                foundit_b = x_bufsize_c;
            }
            if (foundit_b < 0) //if we did not find an \n or a \v
            {
                //post("no n or v found %d", foundit_b);
                /* too much text to fit in one line? */
                //post("inchars_c=%d widthlimit_c=%d", inchars_c, widthlimit_c);
                if (inchars_c - tag_offs > widthlimit_c)
                {
                    /* is there a space to break the line at?  OK if it's even
                    one byte past the end since in this context we know there's
                    more text */
                    foundit_b =
                        lastone(x->x_buf + inindex_b, ' ', maxindex_b + 1);

                    // ico@vt.edu 2021-05-10: here we also check for hyphen
                    // since HTML line breaks also allow for those to happen
                    // at a point where the hyphen is.
                    foundit_bv =
                        lastone(x->x_buf + inindex_b, '-', maxindex_b);
                    // if '-' is chosen for the linebreak, we add one to the string
                    // size and we make eatchar 0 to ensure '-' is not dropped
                    if (foundit_bv > foundit_b) {
                        foundit_b = foundit_bv + 1;
                        eatchar = 0;
                    }

                    if (foundit_b < 0)
                    {
                        foundit_b = maxindex_b;
                        foundit_c = maxindex_c;
                        eatchar = 0;
                    }
                    else
                        foundit_c = u8_charnum(x->x_buf + inindex_b, foundit_b);
                }
                else
                {
                    foundit_b = inchars_b;
                    foundit_c = inchars_c;
                    eatchar = 0;
                }
            }
            else {
                //post("found b=%d bv=%d", foundit_b, foundbv);
                foundit_c = u8_charnum(x->x_buf + inindex_b, foundit_b);
                //bvfound = 1;
            }
            //post("final foundit_b=%d", foundit_b);

            // this is true when the object takes a single line
            if (nlines == findy)
            {
                int actualx = (findx < 0 ? 0 :
                    (findx > foundit_c ? foundit_c : findx));
                *indexp = inindex_b + u8_offset(x->x_buf + inindex_b, actualx,
                    x->x_bufsize - inindex_b);
                reportedindex = 1;
            }
            strncpy(tempbuf+outchars_b, x->x_buf + inindex_b, foundit_b);
            if (x->x_selstart >= inindex_b &&
                x->x_selstart <= inindex_b + foundit_b + eatchar)
                    selstart_b = x->x_selstart + outchars_b - inindex_b;
            if (x->x_selend >= inindex_b &&
                x->x_selend <= inindex_b + foundit_b + eatchar)
                    selend_b = x->x_selend + outchars_b - inindex_b;
            outchars_b += foundit_b;
            //post("+++ eat=%d %d(%c)", eatchar, x->x_buf[inindex_b+foundit_b], x->x_buf[inindex_b+foundit_b]);
            inindex_b += (foundit_b + eatchar);
            inindex_c += (foundit_c + eatchar);
            //post("...%d(%c) [%d(%c)]", x->x_buf[inindex_b], x->x_buf[inindex_b],
            //    x->x_buf[inindex_c-1], x->x_buf[inindex_c-1]);
            if (inindex_b < x->x_bufsize) // && (!iscomment || tempbuf[outchars_b-1] != '\n'))
            {
                // ico@vt.edu 2021-08-06:
                // really frail way of dealing with \n at the end of the line length, in which
                // case they need to be eaten since the newline will be automatically added
                // given we have exceeded the object width limit. LATER: rework all this, so that
                // the box calculation and mouse detection takes place GUI-side.
                /*post("rtext adding endline: b-1=<%c> b-1=%d foundit_b=%d \
                      eatchar=%d maxindex_b=%d inindex_b=%d",
                    tempbuf[outchars_b-1],
                    tempbuf[outchars_b-1],
                    foundit_b,
                    eatchar,
                    maxindex_b,
                    x->x_buf[inindex_b]);*/
                // ico@vt.edu 2021-08-06:
                // only add a newline in case the line length is not equal the maximum length
                // and the character that follows is not an \n or a \v
                if (foundit_b != maxindex_b ||
                    (x->x_buf[inindex_b] != '\n' && x->x_buf[inindex_b] != '\v')) {
                    //post("...adding");
                    tempbuf[outchars_b++] = '\n';
                } else {
                    // ico@vt.edu 2021-08-06:
                    // here we subtract a line since the throwaway \n or \v character that will
                    // be leftover from a previous line (which was ended at the object's length
                    // by an \n or \v), will generate a ghost extra line causing formatting
                    // problems...
                    nlines--;
                }
            }
            // if we found a row that is longer than previous (total width)
            if (foundit_c - tag_offs > ncolumns) {
                ncolumns = foundit_c - tag_offs; // - (x->x_buf[inindex_c-2] != ';' && bvfound ? 1 : 0);
                /*post("larger ncolumns %d %d %d | %d %d | %d",
                    x->x_buf[inindex_c-2],
                    x->x_buf[inindex_c-1],
                    x->x_buf[inindex_c-0],
                    foundit_b,
                    ncolumns,
                    bvfound);*/
            }
            // ico@vt.edu 2021-03-30:
            // only if we did not find a \v or an \n do we add a line here
            // after extensive testing it appears not doing so adds extra lines
            // and makes comments in particular have extra padded lines at the end
            // this is because foundit_b is always 0 when it finds a \v or an \n
            if (foundit_b >= 0) nlines++;
            //post("////////////// new=%d old=%d lines=%d", inindex_b, oldinindex_b, nlines);
            //oldinindex_b = inindex_b;
            //post("nlines=%d", nlines);
        }

        /*char out[6];
        startpost("text: ");
        for (int i=0; i < x->x_bufsize; i++)
        {
            sprintf(out, "%d(%c)", x->x_buf[i], x->x_buf[i]);
            poststring(&out);
        }
        endpost();*/
        // append new line in case we end our input with an \n
        // 2021-05-10 ico@vt.edu: but do not do so if we have this at the end of
        // a comment, because that is pointless. this also ensures vanilla
        // compatibility.
        if (!iscomment && x_bufsize_c > 0 && 
                (x->x_buf[x->x_bufsize - 1] == '\n' ||
                 x->x_buf[x->x_bufsize - 1] == '\v')
            )
        {
            nlines++;
            tempbuf[outchars_b++] = '\n';
            //post("add endline");
            //tempbuf[outchars_b] = '\0';
            //outchars_b++;
        }
        if (!reportedindex)
            *indexp = outchars_b;
        if (nlines < 1) nlines = 1;
        if (!widthspec_c)
        {
            while (ncolumns < (x->x_text->te_type == T_TEXT ? 1 : 3))
            {
                tempbuf[outchars_b++] = ' ';
                ncolumns++;
            }
        }
        else ncolumns = widthspec_c;

        // add a null character at the end of the string --for u8_charnum
        // _and_ for the new gui_vmess calls which don't use the %.*s syntax.
        // Because of the way binbuf_gettext works, we should always have
        // a space before this null character. But I'm not 100% sure this
        // space is guaranteed to be there, so we'll just filter it out on
        // the GUI side for now.
        tempbuf[outchars_b++] = '\0';

        pixwide = ncolumns * fontwidth + (LMARGIN + RMARGIN);
        pixhigh = (x->x_active_nlines > 0 ? x->x_active_nlines : nlines) *
            fontheight + (TMARGIN + BMARGIN);
        //post("rtext_senditup %d %d", nlines, x->x_active_nlines);
        //printf("outchars_b=%d bufsize=%d %d\n", outchars_b, x->x_bufsize, x->x_buf[outchars_b]);

        if (action && x->x_text->te_width && x->x_text->te_type != T_ATOM)
        {
            /* if our width is specified but the "natural" width is the
               same as the specified width, set specified width to zero
               so future text editing will automatically change width.
               Except atoms whose content changes at runtime. */
            int widthwas = x->x_text->te_width, newwidth = 0, newheight = 0,
                newindex = 0;
            x->x_text->te_width = 0;
            rtext_senditup(x, SEND_CHECK, &newwidth, &newheight, &newindex);
            if (newwidth/fontwidth != widthwas)
                x->x_text->te_width = widthwas;
            else x->x_text->te_width = 0;
        }
        if (action == SEND_FIRST)
        {
            //post("send_first <%s>", tempbuf);
            gui_vmess("gui_text_new", "xssiiisiii",
                canvas,
                x->x_tag,
                (isbroken ? "broken" : rtext_gettype(x)->s_name),
                glist_isselected(x->x_glist,
                ((t_gobj*)x->x_text)),
                LMARGIN,
                fontheight,
                tempbuf,
                sys_hostfontsize(font),
                sys_fontwidth(glist_getfont(x->x_glist)),
                glist_istoplevel(x->x_glist)
            );
        }
        else if (action == SEND_UPDATE)
        {
            // We add the check for T_MESSAGE below so that the box border
            // gets resized correctly using our interim event handling in
            // pd_canvas.html.
            // Additionally we avoid redrawing the border here for the
            // dropdown_class as that has its own special width handling.
            // ico@vt.edu 2021-08-25: this is also where the gatom gets
            // autoresized when its width is specified as 0. This is why
            // we do this first, because the gatom width and consequently
            // its '>' symbol that gets appended on strings larger than
            // the gatom width is now calculated gui-side, so we need to
            // first ensure that the width is correct
            if (glist_isvisible(x->x_glist) && !is_dropdown(x->x_text) &&
                (pixwide != x->x_drawnwidth ||
                pixhigh != x->x_drawnheight ||
                x->x_text->te_type == T_MESSAGE)) 
            {
                //post("rtext text_drawborder");
                text_drawborder(x->x_text, x->x_glist, x->x_tag,
                    pixwide, pixhigh, 0);
            }
            //post("send_update <%s>", tempbuf);
            // note that the type argument comes last here since it is optional
           gui_vmess("gui_text_set", "xsss",
                      canvas, x->x_tag, tempbuf, rtext_gettype(x)->s_name);

            if (x->x_active)
            {
                if (selend_b > selstart_b)
                {
                    //sys_vgui(".x%zx.c select from %s %d\n", canvas, 
                    //    x->x_tag, u8_charnum(tempbuf, selstart_b));
                    //sys_vgui(".x%zx.c select to %s %d\n", canvas, 
                    //    x->x_tag, u8_charnum(tempbuf, selend_b)
                    //      + (sys_oldtclversion ? 0 : -1));
                    //sys_vgui(".x%zx.c focus \"\"\n", canvas);        
                }
                else
                {
                    //sys_vgui(".x%zx.c select clear\n", canvas);
                    //sys_vgui(".x%zx.c icursor %s %d\n", canvas, x->x_tag,
                    //    u8_charnum(tempbuf, selstart_b));
                    //sys_vgui(".x%zx.c focus %s\n", canvas, x->x_tag);        
                }
            }
        }
        x->x_drawnwidth = pixwide;
        x->x_drawnheight = pixhigh;
        
        *widthp = pixwide;
        *heightp = pixhigh;
        if (tempbuf != smallbuf)
            t_freebytes(tempbuf, 2 * x->x_bufsize + 1);
    }
    //printf("END\n");
}

void rtext_retext(t_rtext *x)
{
    int w = 0, h = 0, indx;
    t_text *text = x->x_text;
    t_freebytes(x->x_buf, x->x_bufsize);
    binbuf_gettext(text->te_binbuf, &x->x_buf, &x->x_bufsize);
    /* special case: for number boxes, try to pare the number down
       to the specified width of the box. */
    if (text->te_width > 0 && text->te_type == T_ATOM &&
        x->x_bufsize > text->te_width)
    {
        t_atom *atomp = binbuf_getvec(text->te_binbuf);
        int natom = binbuf_getnatom(text->te_binbuf);
        int bufsize = x->x_bufsize;
        if (natom == 1 && atomp->a_type == A_FLOAT)
        {
            /* try to reduce size by dropping decimal digits */
            int wantreduce = bufsize - text->te_width;
            char *decimal = 0, *nextchar, *ebuf = x->x_buf + bufsize,
                *s1, *s2;
            for (decimal = x->x_buf; decimal < ebuf; decimal++)
                if (*decimal == '.')
                    break;
            if (decimal >= ebuf)
                goto giveup;
            for (nextchar = decimal + 1; nextchar < ebuf; nextchar++)
                if (*nextchar < '0' || *nextchar > '9')
                    break;
            if (nextchar - decimal - 1 < wantreduce)
                goto giveup;
            for (s1 = nextchar - wantreduce, s2 = s1 + wantreduce;
                s2 < ebuf; s1++, s2++)
                    *s1 = *s2;
            x->x_buf = t_resizebytes(x->x_buf, bufsize, text->te_width);
            bufsize = text->te_width;
            goto done;
        giveup:
            /* give up and bash it to "+" or "-" */
            x->x_buf[0] = (atomp->a_w.w_float < 0 ? '-' : '+');
            x->x_buf = t_resizebytes(x->x_buf, bufsize, 1);
            bufsize = 1;
        }
        // ico@vt.edu 2021-04-16: we now handle this inside pdgui.js
        /*else if (bufsize > text->te_width)
        {
            x->x_buf[text->te_width - 1] = '>';
            x->x_buf = t_resizebytes(x->x_buf, bufsize, text->te_width);
            bufsize = text->te_width;
        }*/
    done:
        x->x_bufsize = bufsize;
    }
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
}

/* find the rtext that goes with a text item */
t_rtext *glist_findrtext(t_glist *gl, t_text *who)
{
    t_rtext *x=NULL;
    if (!gl->gl_editor)
        canvas_create_editor(gl);
    if (gl->gl_editor->e_rtext)
        for (x = gl->gl_editor->e_rtext; x && x->x_text != who; x = x->x_next)
            ;
    if (!x) bug("glist_findrtext");
    return (x);
}

/* same as above but without error reporting */
t_rtext *glist_tryfindrtext(t_glist *gl, t_text *who)
{
    t_rtext *x=NULL;
    if (!gl->gl_editor)
        canvas_create_editor(gl);
    if (gl->gl_editor->e_rtext)
        for (x = gl->gl_editor->e_rtext; x && x->x_text != who; x = x->x_next)
            ;
    return (x);
}

int rtext_width(t_rtext *x)
{
    int w = 0, h = 0, indx;
    rtext_senditup(x, SEND_CHECK, &w, &h, &indx);
    return (w);
}

int rtext_height(t_rtext *x)
{
    int w = 0, h = 0, indx;
    rtext_senditup(x, SEND_CHECK, &w, &h, &indx);
    return (h);
}

// used to keep track of the edited area, so that patch cords
// are updated dynamically
void rtext_update_active_nlines(t_rtext *x, int nlines)
{
    if (!x->x_active)
        return;
    if (nlines >= 0)
        x->x_active_nlines = nlines;
}

void rtext_draw(t_rtext *x)
{
    int w = 0, h = 0, indx;
    rtext_senditup(x, SEND_FIRST, &w, &h, &indx);
}

/* Not needed since the rtext gets erased along with the parent gobj group */
void rtext_erase(t_rtext *x)
{
    //if (x && x->x_glist)
    //    sys_vgui(".x%zx.c delete %s\n", glist_getcanvas(x->x_glist), x->x_tag);
}

/* Not needed since the rtext gets displaced along with the parent gobj group */
void rtext_displace(t_rtext *x, int dx, int dy)
{
    //sys_vgui(".x%zx.c move %s %d %d\n", glist_getcanvas(x->x_glist), 
    //    x->x_tag, dx, dy);
}

/* This is no longer used-- we do this with CSS now. But keeping the code
   here until we test a bit more. */
void rtext_select(t_rtext *x, int state)
{
    //t_glist *glist = x->x_glist;
    //t_canvas *canvas = glist_getcanvas(glist);
    //if (glist_istoplevel(glist))
    //    sys_vgui(".x%zx.c itemconfigure %s -fill %s\n", canvas, 
    //        x->x_tag, (state? "$pd_colors(selection)" : "$pd_colors(text)"));
    //if (x->x_text->te_pd->c_wb && x->x_text->te_pd->c_wb->w_displacefnwtag)
    //{
    //    if (state)
    //        sys_vgui(".x%zx.c addtag selected withtag %s\n",
    //               glist_getcanvas(glist), x->x_tag);
    //    else
    //        sys_vgui(".x%zx.c dtag %s selected\n",
    //               glist_getcanvas(glist), x->x_tag);
    //}
    /* Not sure the following is needed anymore either-- commenting it
       out to test what (if any) side-effects there are */
    //canvas_editing = canvas;
}

EXTERN void scrollbar_synchronous_update(t_glist *glist);

void rtext_activate(t_rtext *x, int state)
{
    //post("rtext_activate state=%d", state);
    int w = 0, h = 0, widthspec, heightspec, indx, isgop,
        selstart = -1, selend = -1;
    int xmin, xmax, tmp;
    char *tmpbuf;
    int ni = 0, no = 0, nlet_width = 0;
    t_object *ob;
    t_glist *glist = x->x_glist;
    t_canvas *canvas = glist_getcanvas(glist);
    if (state && x->x_active) {
        //post("duplicate rtext_activate");
        return;
    }

    ob = pd_checkobject(&x->x_text->te_pd);
    if (ob)
    {
        no = obj_noutlets(ob);
        ni = obj_ninlets(ob);
        //post("rtext_activate ni=%d no=%d", ni, no);
    }

    // the following prevents from selecting all when inside an
    // object that is already being texted for... please *test*
    // "fixes" before committing them
    //if (state == x->x_active) return; // avoid excess calls
    if (state)
    {
        //sys_vgui(".x%zx.c focus %s\n", canvas, x->x_tag);
        glist->gl_editor->e_textedfor = x;
        glist->gl_editor->e_textdirty = 0;
        x->x_selstart = 0;
        x->x_selend = x->x_bufsize;
        x->x_active = 1;
        x->x_active_nlines = 0;
    }
    else
    {
        //sys_vgui("selection clear .x%zx.c\n", canvas);
        //sys_vgui(".x%zx.c focus \"\"\n", canvas);
        if (glist->gl_editor->e_textedfor == x)
            glist->gl_editor->e_textedfor = 0;
        x->x_active = 0;
        x->x_active_nlines = 0;
    }

    /* check if it has a window */
    if(!glist->gl_havewindow) return;

    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
    /* hack...
       state = 0 no editing
       state = 1 editing
       state = 2 editing a new object
       State 2 isn't necessary, except that Pd has
       traditionally had this "floating" state for
       new objects where the box text is editable and
       the box position follows the mouse
    */

    /* If we're a gop canvas... */
    if (pd_class((t_pd*)x->x_text) == canvas_class &&
        ((t_canvas *)x->x_text)->gl_isgraph)
    {
        // ico@vt.edu 2021-10-07: we need to get the rectangle in case
        // gop object has more nlets than can fit on the size specified
        // in its properties, so that the activated object size matches
        // its regular size.
        gobj_getrect(&x->x_text->te_g, x->x_glist, &xmin, &tmp, &xmax, &tmp);
        if (xmax - xmin > ((t_canvas *)x->x_text)->gl_pixwidth)
            widthspec = -(xmax-xmin);
        else
            widthspec = ((t_canvas *)x->x_text)->gl_pixwidth;
        heightspec = ((t_canvas *)x->x_text)->gl_pixheight;
        isgop = 1;
    }
    else
    {
        gobj_getrect(&x->x_text->te_g, x->x_glist, &xmin, &tmp, &xmax, &tmp);
            /* width if specified. If not, we send the bounding width as
               a negative number */
        widthspec = (x->x_text->te_width ? x->x_text->te_width : -(xmax-xmin));
            /* signal with negative number that we don't have a heightspec */
        heightspec = -1; // signal that we don't have a heightspec
        isgop = 0;
    }

    if (ni > 1 || no > 1)
    {
        nlet_width = (ni > no ? ni : no);
        //post("...nlet_width=%d", nlet_width);
    }

    if(state & (0b1 << 31)) /* arbitray selection */
    {
        selstart = (state >> 16) & 0x7FFF;
        selend = state & 0xFFFF;
        state = 1; // set to editing state
    }

    /* we need to get scroll to make sure we've got the
       correct bbox for the svg */
    scrollbar_synchronous_update(glist_getcanvas(canvas));
    /* ugly hack to get around the fact that x_buf is not
       null terminated. If this becomes a problem we can revisit
       it later */
    tmpbuf = t_getbytes(x->x_bufsize + 1);
    sprintf(tmpbuf, "%.*s", (int)x->x_bufsize, x->x_buf);
    //post("rtext_activate tmpbuf=<%s>", tmpbuf);
    /* in case x_bufsize is 0... */
    tmpbuf[x->x_bufsize] = '\0';
    gui_vmess("gui_textarea", "xssiiiisiiiiiiii",
        canvas,
        x->x_tag,
        (pd_class((t_pd *)x->x_text) == message_class ? "msg" :
            pd_class((t_pd *)x->x_text) == text_class &&
                x->x_text->te_type == T_TEXT ? "comment" : "obj"),
        x->x_text->te_xpix,
        x->x_text->te_ypix,
        widthspec,
        heightspec,
        tmpbuf,
        sys_hostfontsize(glist_getfont(glist)),
        sys_fontwidth(glist_getfont(glist)),
        sys_fontheight(glist_getfont(glist)),
        isgop,
        state,
        selstart,
        selend,
        nlet_width
        );
    freebytes(tmpbuf, x->x_bufsize + 1);
}

// outputs 1 if found one of the special chars
// this function is used with traversal through rtext below
// using ctrl+left/right and similar shortcuts
static int rtext_compare_special_chars(const char c)
{
        if (c != '\n' && c != '\v' && c != ' ')
            return 0;
        return 1;
}

    /* figure out which atom a click falls into if any; -1 if you
    clicked on a space or something */
int rtext_findatomfor(t_rtext *x, int xpos, int ypos)
{
    int w = xpos, h = ypos, indx, natom = 0, i, gotone = 0;
        /* get byte index of character clicked on */
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
        /* search through for whitespace before that index */
    for (i = 0; i <= indx; i++)
    {
        if (x->x_buf[i] == ';' || x->x_buf[i] == ',')
            natom++, gotone = 0;
        else if (x->x_buf[i] == ' ' || x->x_buf[i] == '\n')
            gotone = 0;
        else
        {
            if (!gotone)
                natom++;
            gotone = 1;
        }
    }
    return (natom-1);
}

void rtext_key(t_rtext *x, int keynum, t_symbol *keysym)
{
    //post("rtext_key %d %s", keynum, keysym->s_name);
    int w = 0, h = 0, indx, i, newsize, ndel;
    if (keynum)
    {
        int n = keynum;
        if (n == '\r' || n == '\v') n = '\n';
        if (n == '\b') /* backspace */
        {
            if (x->x_selstart && (x->x_selstart == x->x_selend))
            {
                u8_dec(x->x_buf, &x->x_selstart);
                if (glist_isvisible(glist_getcanvas(x->x_glist)))
                    canvas_getscroll(glist_getcanvas(x->x_glist));
            }
            
        }
        else if (n == 127) /* delete */
        {
            if (x->x_selend < x->x_bufsize && (x->x_selstart == x->x_selend))
                u8_inc(x->x_buf, &x->x_selend);
            if (glist_isvisible(glist_getcanvas(x->x_glist)))
                canvas_getscroll(glist_getcanvas(x->x_glist));
        }
        
        ndel = x->x_selend - x->x_selstart;
        if (ndel)
        {
            for (i = x->x_selend; i < x->x_bufsize; i++)
                x->x_buf[i- ndel] = x->x_buf[i];
            newsize = x->x_bufsize - ndel;
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize);
            x->x_bufsize = newsize;
        }

/* at Guenter's suggestion, use 'n>31' to test wither a character might
be printable in whatever 8-bit character set we find ourselves. */

/*-- moo:
  ... but test with "<" rather than "!=" in order to accomodate unicode
  codepoints for n (which we get since Tk is sending the "%A" substitution
  for bind <Key>), effectively reducing the coverage of this clause to 7
  bits.  Case n>127 is covered by the next clause.
*/
        if (n == '\n' || (n > 31 && n < 127))
        {
            newsize = x->x_bufsize+1;
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize);
            for (i = x->x_bufsize; i > x->x_selstart; i--)
                x->x_buf[i] = x->x_buf[i-1];
            x->x_buf[x->x_selstart] = n;
            x->x_bufsize = newsize;
            x->x_selstart = x->x_selstart + 1;
            if (glist_isvisible(glist_getcanvas(x->x_glist)))
                canvas_getscroll(glist_getcanvas(x->x_glist));
        }
        /*--moo: check for unicode codepoints beyond 7-bit ASCII --*/
        else if (n > 127)
        {
            int ch_nbytes = u8_wc_nbytes(n);
            newsize = x->x_bufsize + ch_nbytes;
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize);
            //fprintf(stderr,"x->x_bufsize=%d newsize=%d\n", x->x_bufsize, newsize);
            //for (i = newsize-1; i >= x->x_selstart; i--)
            //{
                //fprintf(stderr,"%d-%d <%d>\n", i, i-ch_nbytes, x->x_buf[i-ch_nbytes]);
                //x->x_buf[i] = '\0';
            //}
            x->x_bufsize = newsize;
            /*-- moo: assume canvas_key() has encoded keysym as UTF-8 */
            strncpy(x->x_buf+x->x_selstart, keysym->s_name, ch_nbytes);
            x->x_selstart = x->x_selstart + ch_nbytes;
        }
        x->x_selend = x->x_selstart;
        x->x_glist->gl_editor->e_textdirty = 1;
    }
    else if (!strcmp(keysym->s_name, "Right"))
    {
        if (x->x_selend == x->x_selstart && x->x_selstart < x->x_bufsize)
        {
            u8_inc(x->x_buf, &x->x_selstart);
            x->x_selend = x->x_selstart;
        }
        else
            x->x_selstart = x->x_selend;
        last_sel = 0;        
    }
    else if (!strcmp(keysym->s_name, "Left"))
    {
        if (x->x_selend == x->x_selstart && x->x_selstart > 0)
        {
            u8_dec(x->x_buf, &x->x_selstart);
            x->x_selend = x->x_selstart;
        }
        else
            x->x_selend = x->x_selstart;
        last_sel = 0;
    }
    else if (!strcmp(keysym->s_name, "ShiftRight"))
    {
        if (!last_sel) last_sel = 2;
        if (last_sel == 1 && x->x_selstart < x->x_selend)
        {
            if (x->x_selstart < x->x_bufsize)
                u8_inc(x->x_buf, &x->x_selstart);        
        }
        else
        {
            last_sel = 2;
            if (x->x_selend < x->x_bufsize)
                u8_inc(x->x_buf, &x->x_selend);
        }
    }
    else if (!strcmp(keysym->s_name, "ShiftLeft"))
    {
        if (!last_sel) last_sel = 1;
        if (last_sel == 2 && x->x_selend > x->x_selstart)
        {
            x->x_selend = x->x_selend - 1;
        }
        else
        {
            last_sel = 1;
            if (x->x_selstart > 0)
                u8_dec(x->x_buf, &x->x_selstart);
        }
    }
    else if (!strcmp(keysym->s_name, "Up"))
    {
        if (x->x_selstart != x->x_selend)
        {
            x->x_selend = x->x_selstart;
            last_sel = 0;
        }
        else
        {
            // we do this twice and then move to the right
            // as many spots as we had before, this will
            // allow us to go visually above where we used
            // to be in multiline situations (e.g. comments)
            int right = 0;
            //printf("start: selstart=%d x->x_bufsize=%d\n", x->x_selstart, x->x_bufsize);
            if (x->x_selstart > 0 &&
                   (x->x_selstart == x->x_bufsize ||
                    x->x_buf[x->x_selstart] == '\n' ||
                    x->x_buf[x->x_selstart] == '\v'))
            {
                //printf("found break\n");
                u8_dec(x->x_buf, &x->x_selstart);
                right++;
            }
            while (x->x_selstart > 0 &&
                    (x->x_buf[x->x_selstart-1] != '\n' &&
                        x->x_buf[x->x_selstart-1] != '\v'))
            {
                u8_dec(x->x_buf, &x->x_selstart);
                right++;
            }
            if (x->x_selstart == 0)
                right = 0;
            //printf("first linebreak: right=%d selstart=%d\n", right, x->x_selstart);
            if (x->x_selstart > 0)
                u8_dec(x->x_buf, &x->x_selstart);
            //printf("decrease by 1: selstart=%d\n", x->x_selstart);
            while (x->x_selstart > 0 &&
                (x->x_buf[x->x_selstart-1] != '\n' &&
                    x->x_buf[x->x_selstart-1] != '\v'))
                u8_dec(x->x_buf, &x->x_selstart);
            //printf("second linebreak: selstart=%d\n", x->x_selstart);
            if (x->x_selstart < x->x_bufsize && right > 0)
            {
                u8_inc(x->x_buf, &x->x_selstart);
                right--;
            }
            //printf("increase by 1: selstart=%d\n", x->x_selstart);
            while (right > 0 && 
                (x->x_buf[x->x_selstart] != '\n' &&
                    x->x_buf[x->x_selstart] != '\v'))
            {
                u8_inc(x->x_buf, &x->x_selstart);
                right--;
            }
            //printf("final: selstart=%d\n", x->x_selstart);
            x->x_selend = x->x_selstart;
            last_sel = 0;
        }
    }
    else if (!strcmp(keysym->s_name, "Down"))
    {
        if (x->x_selstart != x->x_selend)
        {
            x->x_selstart = x->x_selend;
            last_sel = 0;
        }
        else
        {
            // we do this twice and then move to the right
            // as many spots as we had before, this will
            // allow us to go visually below where we used
            // to be in multiline situations (e.g. comments)
            int right = 0;
            if (x->x_selstart > 0 &&
                    (x->x_buf[x->x_selstart] != '\n' ||
                        x->x_buf[x->x_selstart] != '\v'))
            {
                while (x->x_selstart > 0 &&
                        (x->x_buf[x->x_selstart-1] != '\n' &&
                            x->x_buf[x->x_selstart-1] != '\v'))
                {
                    x->x_selstart--;
                    right++;
                }
            }
            //printf("start: right=%d selstart=%d selend=%d\n", right, x->x_selstart, x->x_selend);
            if (x->x_selend < x->x_bufsize &&
                    (x->x_buf[x->x_selend] == '\n' ||
                        x->x_buf[x->x_selend] == '\v'))
            {
                //printf("found break\n");
                u8_inc(x->x_buf, &x->x_selend);
                right--;
            }
            else while (x->x_selend < x->x_bufsize &&
                (x->x_buf[x->x_selend] != '\n' && x->x_buf[x->x_selend] != '\v'))
            {
                u8_inc(x->x_buf, &x->x_selend);
            }
            //printf("first linebreak: selend=%d\n", x->x_selend);
            if (x->x_selend+1 < x->x_bufsize)
            {
                u8_inc(x->x_buf, &x->x_selend);
            }
            //printf("increase by 1: selend=%d\n", x->x_selend);
            while (right > 0 && x->x_selend < x->x_bufsize &&
                (x->x_buf[x->x_selend] != '\n' &&
                    x->x_buf[x->x_selend] != '\v'))
            {
                u8_inc(x->x_buf, &x->x_selend);
                right--;
            }
            //printf("final: selend=%d\n", x->x_selend);
            x->x_selstart = x->x_selend;
            last_sel = 0;
        }
    }
    else if (!strcmp(keysym->s_name, "Home"))
    {
        if (x->x_selstart)
            u8_dec(x->x_buf, &x->x_selstart);
        while (x->x_selstart > 0 && x->x_buf[x->x_selstart] != '\n')
            u8_dec(x->x_buf, &x->x_selstart);
        x->x_selend = x->x_selstart;
        last_sel = 0;
    }
    else if (!strcmp(keysym->s_name, "ShiftHome"))
    {
        if (x->x_selstart)
        {
            if (last_sel == 2)
                x->x_selend = x->x_selstart;
            u8_dec(x->x_buf, &x->x_selstart);
            last_sel = 1;
        }
        while (x->x_selstart > 0 && x->x_buf[x->x_selstart] != '\n')
            u8_dec(x->x_buf, &x->x_selstart);
        //x->x_selend = x->x_selstart;
        //last_sel = 1;
    }
    else if (!strcmp(keysym->s_name, "End"))
    {
        while (x->x_selend < x->x_bufsize &&
            x->x_buf[x->x_selend] != '\n')
            u8_inc(x->x_buf, &x->x_selend);
        if (x->x_selend < x->x_bufsize)
            u8_inc(x->x_buf, &x->x_selend);
        x->x_selstart = x->x_selend;
        last_sel = 0;
    }
    else if (!strcmp(keysym->s_name, "ShiftEnd"))
    {
        if (last_sel == 1)
            x->x_selstart = x->x_selend;
        while (x->x_selend < x->x_bufsize &&
            x->x_buf[x->x_selend] != '\n')
            u8_inc(x->x_buf, &x->x_selend);
        if (x->x_selend < x->x_bufsize)
        {
            u8_inc(x->x_buf, &x->x_selend);
        }
        //x->x_selstart = x->x_selend;
        last_sel = 2;
    }
    else if (!strcmp(keysym->s_name, "CtrlLeft"))
    {
        /* first find first non-space char going back */
        while (x->x_selstart > 0 &&
            rtext_compare_special_chars(x->x_buf[x->x_selstart-1]))
        {
            u8_dec(x->x_buf, &x->x_selstart);
        }
        /* now go back until you find another space or
           the beginning of the buffer */
        while (x->x_selstart > 0 &&
          !rtext_compare_special_chars(x->x_buf[x->x_selstart-1]))
        {
            u8_dec(x->x_buf, &x->x_selstart);
        }
        if (x->x_buf[x->x_selstart+1] == ' ' &&
            x->x_buf[x->x_selstart] == ' ')
        {
            u8_inc(x->x_buf, &x->x_selstart);
        }
        x->x_selend = x->x_selstart;
    }
    else if (!strcmp(keysym->s_name, "CtrlRight"))
    {
        /* now go forward until you find another space
           or the end of the buffer */
        if (x->x_selend < x->x_bufsize - 1)
            u8_inc(x->x_buf, &x->x_selend);
        while (x->x_selend < x->x_bufsize &&
          !rtext_compare_special_chars(x->x_buf[x->x_selend]))
            u8_inc(x->x_buf, &x->x_selend);
        x->x_selstart = x->x_selend;
    }
    else if (!strcmp(keysym->s_name, "CtrlShiftLeft"))
    {
        int swap = 0;
        int *target;
        if (!last_sel) last_sel = 1;
        if (last_sel == 2 && x->x_selend > x->x_selstart)
            target = &x->x_selend;
        else
        {
            last_sel = 1;
            target = &x->x_selstart;
        }
        /* first find first non-space char going back */
        while (*target > 0 &&
            rtext_compare_special_chars(x->x_buf[*target-1]))
        {
            u8_dec(x->x_buf, target);
        }
        /* now go back until you find another space or
           the beginning of the buffer */
        while (*target > 0 &&
            !rtext_compare_special_chars(x->x_buf[*target-1]))
        {
            u8_dec(x->x_buf, target);
        }
        if (x->x_buf[*target+1] == ' ' &&
            x->x_buf[x->x_selstart] == ' ')
        {
            u8_inc(x->x_buf, target);
        }
        if (x->x_selstart > x->x_selend)
        {
            swap = x->x_selend;
            x->x_selend = x->x_selstart;
            x->x_selstart = swap;
            last_sel = 1;
        }
    }
    else if (!strcmp(keysym->s_name, "CtrlShiftRight"))
    {
        int swap = 0;
        int *target;
        if (!last_sel) last_sel = 2;
        if (last_sel == 1 && x->x_selstart < x->x_selend)
            target = &x->x_selstart;
        else
        {
            last_sel = 2;
            target = &x->x_selend;
        }
        /* now go forward until you find another space or
           the end of the buffer */
        if (*target < x->x_bufsize - 1)
        {
            u8_inc(x->x_buf, target);
        }
        while (*target < x->x_bufsize &&
            !rtext_compare_special_chars(x->x_buf[*target]))
        {
            u8_inc(x->x_buf, target);
        }
        if (x->x_selstart > x->x_selend)
        {
            swap = x->x_selend;
            x->x_selend = x->x_selstart;
            x->x_selstart = swap;
            last_sel = 2;
        }
    }
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
}
