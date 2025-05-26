/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* Pd side of the Pd/Pd-gui interface.  Also, some system interface routines
that didn't really belong anywhere. */

#include "config.h"

#include "m_pd.h"
#include "s_stuff.h"
#include "m_imp.h"
#include "g_canvas.h"   /* for GUI queueing stuff */
#include "s_net.h"
#include <errno.h>

/* Use this if you want to point the guidir at a local copy of the
 * repo while developing. Then recompile and copy the pd-l2ork binary
 * to the system path. After that you can make changes to the gui code
 * in pd-l2ork/pd/nw and test them without having to recompile the
 * pd-l2ork binary.
 * If you do this, make sure you have run tar_em_up.sh first to fetch and
 * extract the nw binary to pd-l2ork/pd/nw/nw
 */

#define GUIDIR "" /* "/home/user/pd-l2ork/pd/nw" */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif !defined(_WIN32)
# define isatty(fd) 0
#endif
static int stderr_isatty;

#ifndef _WIN32
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/stat.h>
#include <glob.h>
#else
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
# ifndef USE_UNAME
#  define USE_UNAME 1
# endif
#endif

#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf _snprintf
#endif


#define INTER (pd_this->pd_inter)

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define DEBUG_MESSUP 1      /* messages up from pd to pd-gui */
#define DEBUG_MESSDOWN 2    /* messages down from pd-gui to pd */

#ifndef PDBINDIR
#define PDBINDIR "bin/"
#endif

#ifndef WISH
# if defined _WIN32
#  define WISH "wish85.exe"
# elif defined __APPLE__
   // leave undefined to use dummy search path, otherwise
   // this should be a full path to wish on mac
#else
#  define WISH "wish"
# endif
#endif

#define LOCALHOST "localhost"

#if PDTHREADS
#include "pthread.h"
#endif

#define X_SPECIFIER "x%.6zx"

#if PD_FLOATSIZE == 32
#define FLOAT_SPECIFIER "%.6g"
#elif PD_FLOATSIZE == 64
#define FLOAT_SPECIFIER "%.14g"
#endif

static int stderr_isatty;

/* I don't see any other systems where this header (and backtrace) are
   available. */
#ifdef __linux__
#include <execinfo.h>
#endif

extern char *pd_version;

typedef struct _fdpoll
{
    int fdp_fd;
    t_fdpollfn fdp_fn;
    void *fdp_ptr;
} t_fdpoll;

struct _socketreceiver
{
    char *sr_inbuf;
    int sr_inhead;
    int sr_intail;
    void *sr_owner;
    int sr_udp;
    struct sockaddr_storage *sr_fromaddr; /* optional */
    t_socketnotifier sr_notifier;
    t_socketreceivefn sr_socketreceivefn;
    t_socketfromaddrfn sr_fromaddrfn; /* optional */
};

typedef struct _guiqueue
{
    void *gq_client;
    t_glist *gq_glist;
    t_guicallbackfn gq_fn;
    struct _guiqueue *gq_next;
} t_guiqueue;

struct _instanceinter
{
    int i_havegui;
    int i_nfdpoll;
    t_fdpoll *i_fdpoll;
    int i_maxfd;
    int i_guisock;
    t_socketreceiver *i_socketreceiver;
    t_guiqueue *i_guiqueuehead;
    t_binbuf *i_inbinbuf;
    char *i_guibuf;
    int i_guihead;
    int i_guitail;
    int i_guisize;
    int i_waitingforping;
    int i_bytessincelastping;
    int i_fdschanged; /* flag to break fdpoll loop if fd list changes */

#ifdef _WIN32
    LARGE_INTEGER i_inittime;
    double i_freq;
#endif
#if PDTHREADS
    pthread_mutex_t i_mutex;
#endif

    unsigned char i_recvbuf[NET_MAXPACKETSIZE];
};

extern int sys_guisetportnumber;
extern int sys_addhist(int phase);
void sys_stopgui(void);

/* ----------- functions for timing, signals, priorities, etc  --------- */

#ifdef _WIN32

static void sys_initntclock(void)
{
    LARGE_INTEGER f1;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!QueryPerformanceFrequency(&f1))
    {
          fprintf(stderr, "pd: QueryPerformanceFrequency failed\n");
          f1.QuadPart = 1;
    }
    INTER->i_freq = f1.QuadPart;
    INTER->i_inittime = now;
}

#if 0
    /* this is a version you can call if you did the QueryPerformanceCounter
    call yourself.  Necessary for time tagging incoming MIDI at interrupt
    level, for instance; but we're not doing that just now. */

double nt_tixtotime(LARGE_INTEGER *dumbass)
{
    if (INTER->i_freq == 0) sys_initntclock();
    return (((double)(dumbass->QuadPart - INTER->i_inittime.QuadPart)) / INTER->i_freq);
}
#endif
#endif /* _WIN32 */

    /* get "real time" in seconds; take the
    first time we get called as a reference time of zero. */
double sys_getrealtime(void)
{
#ifndef _WIN32
    static struct timeval then;
    struct timeval now;
    gettimeofday(&now, 0);
    if (then.tv_sec == 0 && then.tv_usec == 0) then = now;
    return ((now.tv_sec - then.tv_sec) +
        (1./1000000.) * (now.tv_usec - then.tv_usec));
#else
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (INTER->i_freq == 0) sys_initntclock();
    return (((double)(now.QuadPart - INTER->i_inittime.QuadPart)) / INTER->i_freq);
#endif
}

/* sleep (but cancel the sleeping if any file descriptors are
ready - in that case, dispatch any resulting Pd messages and return.  Called
with sys_lock() set.  We will temporarily release the lock if we actually
sleep. */
static int sys_domicrosleep(int microsec)
{
    struct timeval timeout;
    int i, didsomething = 0;
    t_fdpoll *fp;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (INTER->i_nfdpoll)
    {
        fd_set readset, writeset;
        FD_ZERO(&writeset);
        FD_ZERO(&readset);
        for (fp = INTER->i_fdpoll, i = INTER->i_nfdpoll; i--; fp++)
            FD_SET(fp->fdp_fd, &readset);
        if(select(INTER->i_maxfd+1, &readset, &writeset, NULL, &timeout))
            perror("microsleep select");
        INTER->i_fdschanged = 0;
        for (i = 0; i < INTER->i_nfdpoll &&
            !INTER->i_fdschanged; i++)
                if (FD_ISSET(INTER->i_fdpoll[i].fdp_fd, &readset))
        {
            (*INTER->i_fdpoll[i].fdp_fn)(INTER->i_fdpoll[i].fdp_ptr, INTER->i_fdpoll[i].fdp_fd);
            didsomething = 1;
        }
        if (didsomething)
            return (1);
    }
    if (microsec)
    {
        sys_unlock();
#ifdef _WIN32
        Sleep(microsec/1000);
#else
        usleep(microsec);
#endif
        sys_lock();
    }
    return (0);
}

    /* sleep (but if any incoming or to-gui sending to do, do that instead.)
    Call with the PD instance lock UNSET - we set it here. */
void sys_microsleep(void)
{
    sys_lock();
    sys_domicrosleep(sched_get_sleepgrain());
    sys_unlock();
}

#if !defined(_WIN32) && !defined(__CYGWIN__)
#if DONT_HAVE_SIG_T
typedef void (*sig_t)(int);
#endif
static void sys_signal(int signo, sig_t sigfun)
{
    struct sigaction action;
    action.sa_flags = 0;
    action.sa_handler = sigfun;
    memset(&action.sa_mask, 0, sizeof(action.sa_mask));
#if 0  /* GG says: don't use that */
    action.sa_restorer = 0;
#endif
    if (sigaction(signo, &action, 0) < 0)
        perror("sigaction");
}

static void sys_exithandler(int n)
{
    static int trouble = 0;
    if (!trouble)
    {
        trouble = 1;
        fprintf(stderr, "Pd: signal %d\n", n);
        sys_bail(1);
    }
    else _exit(1);
}

static void sys_alarmhandler(int n)
{
    fprintf(stderr, "Pd: system call timed out\n");
}

static void sys_huphandler(int n)
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 30000;
    select(1, 0, 0, 0, &timeout);
}

void sys_setalarm(int microsec)
{
    struct itimerval gonzo;
    int sec = (int)(microsec/1000000);
    microsec %= 1000000;
#if 0
    fprintf(stderr, "timer %d:%d\n", sec, microsec);
#endif
    gonzo.it_interval.tv_sec = 0;
    gonzo.it_interval.tv_usec = 0;
    gonzo.it_value.tv_sec = sec;
    gonzo.it_value.tv_usec = microsec;
    if (microsec)
        sys_signal(SIGALRM, sys_alarmhandler);
    else sys_signal(SIGALRM, SIG_IGN);
    setitimer(ITIMER_REAL, &gonzo, 0);
}

#endif /* NOT _WIN32 && NOT __CYGWIN__ */

void sys_setsignalhandlers(void)
{
#if !defined(_WIN32) && !defined(__CYGWIN__)
    signal(SIGHUP, sys_huphandler);
    signal(SIGINT, sys_exithandler);
    signal(SIGQUIT, sys_exithandler);
# ifdef SIGIOT
    signal(SIGIOT, sys_exithandler);
# endif
    signal(SIGFPE, SIG_IGN);
    /* signal(SIGILL, sys_exithandler);
    signal(SIGBUS, sys_exithandler);
    signal(SIGSEGV, sys_exithandler); */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
#if 0  /* GG says: don't use that */
    signal(SIGSTKFLT, sys_exithandler);
#endif
#endif /* NOT _WIN32 && NOT __CYGWIN__ */
}

#define MODE_NRT 0
#define MODE_RT 1
#define MODE_WATCHDOG 2
#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__GNU__)


#if defined(_POSIX_PRIORITY_SCHEDULING) || defined(_POSIX_MEMLOCK)
#include <sched.h>
#endif

void sys_set_priority(int mode)
{
#ifdef _POSIX_PRIORITY_SCHEDULING
    struct sched_param par;
    int p1, p2, p3;
    p1 = sched_get_priority_min(SCHED_FIFO);
    p2 = sched_get_priority_max(SCHED_FIFO);
#ifdef USEAPI_JACK
    p3 = (mode == MODE_WATCHDOG ? p1 + 7 : (mode == MODE_RT ? p1 + 5 : 0));
#else
    p3 = (mode == MODE_WATCHDOG ? p2 - 5 : (mode == MODE_RT ? p2 - 7 : 0));
#endif
    par.sched_priority = p3;
    if (sched_setscheduler(0,
        (mode == MODE_NRT ? SCHED_OTHER : SCHED_FIFO), &par) < 0)
    {
        if (mode == MODE_WATCHDOG)
            fprintf(stderr, "priority %d scheduling failed.\n", p3);
        else post("priority %d scheduling failed; running at normal priority",
                p3);
    }
    else if (sys_verbose)
    {
        if (mode == MODE_RT)
            post("priority %d scheduling enabled.\n", p3);
        else post("running at normal (non-real-time) priority.\n");
    }
#endif /* _POSIX_PRIORITY_SCHEDULING */

#if !defined(USEAPI_JACK)
    if (mode != MODE_NRT)
    {
            /* tb: force memlock to physical memory { */
        struct rlimit mlock_limit;
        mlock_limit.rlim_cur=0;
        mlock_limit.rlim_max=0;
        setrlimit(RLIMIT_MEMLOCK,&mlock_limit);
            /* } tb */
        if (mlockall(MCL_FUTURE) != -1 && sys_verbose)
            fprintf(stderr, "memory locking enabled.\n");
    }
    else munlockall();
#endif /* ! USEAPI_JACK */
}

#else /* !__linux__ */
void sys_set_priority(int mode)
{
        /* dummy */
    (void)mode;
}
#endif /* !__linux__ */

#ifdef IRIX             /* hack by <olaf.matthes@gmx.de> at 2003/09/21 */

#if defined(_POSIX_PRIORITY_SCHEDULING) || defined(_POSIX_MEMLOCK)
#include <sched.h>
#endif

void sys_set_priority(int higher)
{
#ifdef _POSIX_PRIORITY_SCHEDULING
    struct sched_param par;
        /* Bearing the table found in 'man realtime' in mind, I found it a */
        /* good idea to use 192 as the priority setting for Pd. Any thoughts? */
    if (higher)
                par.sched_priority = 250;       /* priority for watchdog */
    else
                par.sched_priority = 192;       /* priority for pd (DSP) */

    if (sched_setscheduler(0, SCHED_FIFO, &par) != -1)
        fprintf(stderr, "priority %d scheduling enabled.\n", par.sched_priority);
#endif

#ifdef _POSIX_MEMLOCK
    if (mlockall(MCL_FUTURE) != -1)
        fprintf(stderr, "memory locking enabled.\n");
#endif
}
/* end of hack */
#endif /* IRIX */

/* ------------------ receiving incoming messages over sockets ------------- */

unsigned char *sys_getrecvbuf(unsigned int *size)
{
    if (size)
        *size = NET_MAXPACKETSIZE;
    return INTER->i_recvbuf;
}

void sys_sockerror(const char *s)
{
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == 10054) return;
    else if (err == 10044)
    {
        fprintf(stderr,
            "Warning: you might not have TCP/IP \"networking\" turned on\n");
        fprintf(stderr, "which is needed for Pd to talk to its GUI layer.\n");
    }
#else
    int err = errno;
#endif
    pd_error(0, "%s: %s (%d)", s, strerror(err), err);
}

void sys_addpollfn(int fd, t_fdpollfn fn, void *ptr)
{
    int nfd, size;
    t_fdpoll *fp;
    sys_init_fdpoll();
    nfd = INTER->i_nfdpoll;
    size = nfd * sizeof(t_fdpoll);
    INTER->i_fdpoll = (t_fdpoll *)t_resizebytes(INTER->i_fdpoll, size,
        size + sizeof(t_fdpoll));
    fp = INTER->i_fdpoll + nfd;
    fp->fdp_fd = fd;
    fp->fdp_fn = fn;
    fp->fdp_ptr = ptr;
    INTER->i_nfdpoll = nfd + 1;
    if (fd >= INTER->i_maxfd)
        INTER->i_maxfd = fd + 1;
    INTER->i_fdschanged = 1;
}

void sys_rmpollfn(int fd)
{
    int nfd = INTER->i_nfdpoll;
    int i, size = nfd * sizeof(t_fdpoll);
    t_fdpoll *fp;
    INTER->i_fdschanged = 1;
    for (i = nfd, fp = INTER->i_fdpoll; i--; fp++)
    {
        if (fp->fdp_fd == fd)
        {
            while (i--)
            {
                fp[0] = fp[1];
                fp++;
            }
            INTER->i_fdpoll = (t_fdpoll *)t_resizebytes(INTER->i_fdpoll, size,
                size - sizeof(t_fdpoll));
            INTER->i_nfdpoll = nfd - 1;
            return;
        }
    }
    post("warning: %d removed from poll list but not found", fd);
}

    /* Size of the buffer used for parsing FUDI messages
    received over TCP. Must be a power of two!
    LATER make this settable per socketreceiver instance */
#define INBUFSIZE 4096

t_socketreceiver *socketreceiver_new(void *owner, t_socketnotifier notifier,
    t_socketreceivefn socketreceivefn, int udp)
{
    t_socketreceiver *x = (t_socketreceiver *)getbytes(sizeof(*x));
    x->sr_inhead = x->sr_intail = 0;
    x->sr_owner = owner;
    x->sr_notifier = notifier;
    x->sr_socketreceivefn = socketreceivefn;
    x->sr_udp = udp;
    if (!udp)
    {
        if (!(x->sr_inbuf = malloc(INBUFSIZE)))
            bug("t_socketreceiver");
    }
    else
        x->sr_inbuf = NULL;
    return (x);
}

void socketreceiver_free(t_socketreceiver *x)
{
    free(x->sr_inbuf);
    if (x->sr_inbuf)
        free(x->sr_inbuf);
    freebytes(x, sizeof(*x));
}

    /* this is in a separately called subroutine so that the buffer isn't
    sitting on the stack while the messages are getting passed. */
static int socketreceiver_doread(t_socketreceiver *x)
{
    char messbuf[INBUFSIZE], *bp = messbuf;
    int indx, first = 1;
    int inhead = x->sr_inhead;
    int intail = x->sr_intail;
    char *inbuf = x->sr_inbuf;
    for (indx = intail; first || (indx != inhead);
        first = 0, (indx = (indx+1)&(INBUFSIZE-1)))
    {
            /* if we hit a semi that isn't ded by a \, it's a message
            boundary. LATER we should deal with the possibility that the
            preceding \ might itself be escaped! */
        char c = *bp++ = inbuf[indx];
        if (c == ';' && (!indx || inbuf[indx-1] != '\\'))
        {
            intail = (indx+1)&(INBUFSIZE-1);
            binbuf_text(INTER->i_inbinbuf, messbuf, (int)(bp - messbuf));
            if (sys_debuglevel & DEBUG_MESSDOWN) {
                if (stderr_isatty)
                    fprintf(stderr,"\n<- \e[0;1;36m%.*s\e[0m", (int)(bp - messbuf), messbuf);
                else
                    fprintf(stderr,"\n<- %.*s", (int)(bp - messbuf), messbuf);
            }
            x->sr_inhead = inhead;
            x->sr_intail = intail;
            return (1);
        }
    }
    return (0);
}

static void socketreceiver_getudp(t_socketreceiver *x, int fd)
{
    char *buf = (char *)sys_getrecvbuf(0);
    socklen_t fromaddrlen = sizeof(struct sockaddr_storage);
    int ret, readbytes = 0;
    while (1)
    {
        ret = (int)recvfrom(fd, buf, NET_MAXPACKETSIZE-1, 0,
            (struct sockaddr *)x->sr_fromaddr, (x->sr_fromaddr ? &fromaddrlen : 0));
        if (ret < 0)
        {
                /* socket_errno_udp() ignores some error codes */
            if (socket_errno_udp())
            {
                sys_sockerror("recv (udp)");
                    /* only notify and shutdown a UDP sender! */
                if (x->sr_notifier)
                {
                    (*x->sr_notifier)(x->sr_owner, fd);
                    sys_rmpollfn(fd);
                    sys_closesocket(fd);
                }
            }
            return;
        }
        else if (ret > 0)
        {
                /* handle too large UDP packets */
            if (ret > NET_MAXPACKETSIZE-1)
            {
                post("warning: incoming UDP packet truncated from %d to %d bytes.",
                    ret, NET_MAXPACKETSIZE-1);
                ret = NET_MAXPACKETSIZE-1;
            }
            buf[ret] = 0;
    #if 0
            post("%s", buf);
    #endif
            if (buf[ret-1] != '\n')
            {
    #if 0
                pd_error(0,"dropped bad buffer %s\n", buf);
    #endif
            }
            else
            {
                char *semi = strchr(buf, ';');
                if (semi)
                    *semi = 0;
                if (x->sr_fromaddrfn)
                    (*x->sr_fromaddrfn)(x->sr_owner, (const void *)x->sr_fromaddr);
                binbuf_text(INTER->i_inbinbuf, buf, strlen(buf));
                outlet_setstacklim();
                if (x->sr_socketreceivefn)
                    (*x->sr_socketreceivefn)(x->sr_owner,
                        INTER->i_inbinbuf);
                else bug("socketreceiver_getudp");
            }
            readbytes += ret;
            /* throttle */
            if (readbytes >= NET_MAXPACKETSIZE)
                return;
            /* check for pending UDP packets */
            if (socket_bytes_available(fd) <= 0)
                return;
        }
    }
}

void socketreceiver_read(t_socketreceiver *x, int fd)
{
    if (x->sr_udp)   /* UDP ("datagram") socket protocol */
        socketreceiver_getudp(x, fd);
    else  /* TCP ("streaming") socket protocol */
    {
        int readto =
            (x->sr_inhead >= x->sr_intail ? INBUFSIZE : x->sr_intail-1);
        int ret;

            /* the input buffer might be full. If so, drop the whole thing */
        if (readto == x->sr_inhead)
        {
            fprintf(stderr, "pd: dropped message from gui\n");
            x->sr_inhead = x->sr_intail = 0;
            readto = INBUFSIZE;
        }
        else
        {
            ret = (int)recv(fd, x->sr_inbuf + x->sr_inhead,
                readto - x->sr_inhead, 0);
            if (ret <= 0)
            {
                if (ret < 0)
                    sys_sockerror("recv");
                if (x == INTER->i_socketreceiver)
                {
                    if (pd_this == &pd_maininstance)
                    {
                        fprintf(stderr, "read from GUI socket: %s; stopping\n",
                            strerror(errno));
                        sys_bail(1);
                    }
                    else
                    {
                        sys_rmpollfn(fd);
                        sys_closesocket(fd);
                        sys_stopgui();
                    }
                }
                else
                {
                    post("EOF on socket %d\n", fd);
                    if (x->sr_notifier) (*x->sr_notifier)(x->sr_owner, fd);
                    sys_rmpollfn(fd);
                    sys_closesocket(fd);
                }
            }
            else
            {
                x->sr_inhead += ret;
                if (x->sr_inhead >= INBUFSIZE) x->sr_inhead = 0;
                while (socketreceiver_doread(x))
                {
                    if (x->sr_fromaddrfn)
                    {
                        socklen_t fromaddrlen = sizeof(struct sockaddr_storage);
                        if(!getpeername(fd,
                                        (struct sockaddr *)x->sr_fromaddr,
                                        &fromaddrlen))
                            (*x->sr_fromaddrfn)(x->sr_owner,
                                (const void *)x->sr_fromaddr);
                    }
                    outlet_setstacklim();
                    if (x->sr_socketreceivefn)
                        (*x->sr_socketreceivefn)(x->sr_owner, INTER->i_inbinbuf);
                    else binbuf_eval(INTER->i_inbinbuf, 0, 0, 0);
                    if (x->sr_inhead == x->sr_intail)
                        break;
                }
            }
        }
    }
}

void socketreceiver_set_fromaddrfn(t_socketreceiver *x,
    t_socketfromaddrfn fromaddrfn)
{
    x->sr_fromaddrfn = fromaddrfn;
    if (fromaddrfn)
    {
        if (!x->sr_fromaddr)
            x->sr_fromaddr = malloc(sizeof(struct sockaddr_storage));
    }
    else if (x->sr_fromaddr)
    {
        free(x->sr_fromaddr);
        x->sr_fromaddr = NULL;
    }
}

void sys_closesocket(int sockfd)
{
    socket_close(sockfd);
}

/* ---------------------- sending messages to the GUI ------------------ */
#define GUI_ALLOCCHUNK 8192
#define GUI_UPDATESLICE 512 /* how much we try to do in one idle period */
#define GUI_BYTESPERPING 1024 /* how much we send up per ping */
//#define GUI_BYTESPERPING 0x7fffffff /* as per Miller's suggestion to disable the flow control */

#ifdef __EMSCRIPTEN__
void __Pd_receiveCommandBuffer(char *buf);
#endif
static int do_send(int sockfd, char *buf, int len, int flags)
{
#ifdef __EMSCRIPTEN__
    __Pd_receiveCommandBuffer(buf);
    return len;
#endif
    return (int)send(sockfd, buf, len, flags);
}

static void sys_trytogetmoreguibuf(int newsize)
{
        /* newsize can be negative if it overflows (at 0x7FFFFFFF)
         * which only happens if we push a huge amount of data to the GUI,
         * such as printing a billion numbers
         *
         * we could fix this by using size_t (or ssize_t), but this will
         * possibly lead to memory exhaustion.
         * as the overflow happens at 2GB which is rather large anyhow,
         * but most machines will still be able to handle this without swapping
         * and crashing, we just use the 2GB limit to trigger a synchronous write
         */
    char *newbuf = (newsize>=0)?realloc(INTER->i_guibuf, newsize):0;
#if 0
    static int sizewas;
    if (newsize > 70000 && sizewas < 70000)
    {
        int i;
        for (i = INTER->i_guitail; i < INTER->i_guihead; i++)
            fputc(INTER->i_guibuf[i], stderr);
    }
    sizewas = newsize;
#endif
#if 0
    fprintf(stderr, "new size %d (head %d, tail %d)\n",
        newsize, INTER->i_guihead, INTER->i_guitail);
#endif

        /* if realloc fails, make a last-ditch attempt to stay alive by
        synchronously writing out the existing contents.  LATER test
        this by intentionally setting newbuf to zero */
    if (!newbuf)
    {
        int bytestowrite = INTER->i_guihead - INTER->i_guitail;
        int written = 0;
        while (1)
        {
            int res = do_send(
                INTER->i_guisock,
                INTER->i_guibuf + INTER->i_guitail + written,
                bytestowrite, 0);
            if (res < 0)
            {
                perror("pd output pipe");
                sys_bail(1);
            }
            else
            {
                written += res;
                if (written >= bytestowrite)
                    break;
            }
        }
        INTER->i_guihead = INTER->i_guitail = 0;
    }
    else
    {
        INTER->i_guisize = newsize;
        INTER->i_guibuf = newbuf;
    }
}

void blargh(void) {
#ifndef __linux__
  fprintf(stderr,"unhandled exception\n");
#else
  int i;
  void *array[25];
  int nSize = backtrace(array, 25);
  char **symbols = backtrace_symbols(array, nSize);
  for (i=0; i<nSize; i++) fprintf(stderr,"%d: %s\n",i,symbols[i]);
  free(symbols);
#endif
}

int sys_havegui(void)
{
    return (INTER->i_havegui);
}

static int lastend = -1;
void sys_vvgui(const char *fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    int msglen;

    if (!sys_havegui())
        return;
    if (!INTER->i_guibuf)
    {
        if (!(INTER->i_guibuf = malloc(GUI_ALLOCCHUNK)))
        {
            fprintf(stderr, "Pd: couldn't allocate GUI buffer\n");
            sys_bail(1);
        }
        INTER->i_guisize = GUI_ALLOCCHUNK;
        INTER->i_guihead = INTER->i_guitail = 0;
    }
    if (INTER->i_guihead > INTER->i_guisize - (GUI_ALLOCCHUNK/2))
        sys_trytogetmoreguibuf(INTER->i_guisize + GUI_ALLOCCHUNK);
    msglen = pd_vsnprintf(INTER->i_guibuf + INTER->i_guihead,
        INTER->i_guisize - INTER->i_guihead, fmt, ap);
    va_end(ap);
    if (msglen < 0)
    {
        fprintf(stderr, "sys_vgui: pd_snprintf() failed with error code %d\n", errno);
        return;
    }
    if (msglen >= INTER->i_guisize - INTER->i_guihead)
    {
        int msglen2, newsize = INTER->i_guisize + 1 +
            (msglen > GUI_ALLOCCHUNK ? msglen : GUI_ALLOCCHUNK);
        sys_trytogetmoreguibuf(newsize);

        va_copy(ap, aq);
        msglen2 = pd_vsnprintf(INTER->i_guibuf + INTER->i_guihead,
            INTER->i_guisize - INTER->i_guihead, fmt, ap);
        va_end(ap);
        if (msglen2 != msglen)
            bug("sys_vgui");
        if (msglen >= INTER->i_guisize - INTER->i_guihead)
            msglen = INTER->i_guisize - INTER->i_guihead;
    }
    if (sys_debuglevel & DEBUG_MESSUP) {
        //blargh();
        //int begin = lastend=='\n' || lastend=='\r' || lastend==-1;
        int begin = lastend=='\x1f' || lastend==-1;
        if (stderr_isatty)
            fprintf(stderr, "%s\e[0;1;35m%s\e[0m",
                begin ? "\n-> " : "", INTER->i_guibuf + INTER->i_guihead);
        else
            fprintf(stderr, "%s%s",
                begin ? "\n-> " : "", INTER->i_guibuf + INTER->i_guihead);
    }
    INTER->i_guihead += msglen;
    INTER->i_bytessincelastping += msglen;
    if (INTER->i_guihead>0) lastend=INTER->i_guibuf[INTER->i_guihead-1];
    if (INTER->i_guihead>1 && strcmp(INTER->i_guibuf+INTER->i_guihead-2,"\\\n")==0)
        lastend=' ';
}
#undef sys_vgui
#undef sys_gui
void sys_vgui(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sys_vvgui(fmt,ap);
}

/* This was used by the old command interface, but is no longer used. */
void sys_vvguid(const char *file, int line, const char *fmt, va_list ap) {
    if ((sys_debuglevel&4) && /*line>=0 &&*/ (lastend=='\n' || lastend=='\r' || lastend==-1)) {
        sys_vgui("AT %s:%d; ",file,line);
    }
    sys_vvgui(fmt,ap);
}

/* sys_vgui() and sys_gui() are deprecated for externals
   and shouldn't be used directly within Pd.
   however, the we do use them for implementing the high-level
   communication, so we do not want the compiler to shout out loud.
 */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined _MSC_VER
#pragma warning( disable : 4996 )
#endif

void sys_gui(const char *s)
{
    sys_vgui("%s", s);
}

static void escape_double_quotes(const char *src) {
    int dq = 0, len = 0;
    //fprintf(stderr, "escape_double_quotes <%s> %d\n", src, strlen(src));
    const char *s = src;
    while(*s)
    {
        //fprintf(stderr,"<%s><%d> %d\n", s, *s, len);
        len++;
        if (*s == '\"' || *s == '\\')
        {
            dq++;
        }
        s++;
    }
    if (!dq)
        sys_vgui("\"%s\"", src);
    else
    {
        char *dest = (char *)t_getbytes((len+dq+1)*sizeof(*dest));
        char *tmp = dest;
        s = src;
        while(*s)
        {
            if (*s == '\"' || *s == '\\')
            {
                *tmp++ = '\\';
                *tmp++ = *s;
            }
            else
            {
                *tmp++ = *s;
            }
            s++;
        }
        *tmp = '\0'; /* null terminate */
        sys_vgui("\"%s\"", dest);
        t_freebytes(dest, (len+dq+1)*sizeof(*dest));
    }
}

void gui_end_vmess(void)
{
    sys_gui("\x1f"); /* hack: use unit separator to delimit messages */
}

static int gui_array_head;
static int gui_array_tail;
/* this is a bug fix for the edge case of a message to the gui
   with a single array. This is necessary because I'm using a space
   delimiter between the first and second arg, and commas between
   the rest.
   While I think gui_vmess and gui_start_vmess interfaces work well,
   the actual format of the message as sent to the GUI can definitely
   be changed if need be. I threw it together hastily just to get something
   easy to parse in Javascript on the GUI side.
   -jw */
static void set_leading_array(int state) {
    if (state)
    {
        gui_array_head = 1;
        gui_array_tail = 0;
    }
    else
    {
        gui_array_head = 0;
        gui_array_tail = 0;
    }
}

/* quick hack to send a parameter list for use as a function call in nw.js */
void gui_do_vmess(const char *sel, char *fmt, int end, va_list ap)
{
    //fprintf(stderr, "gui_do_vmess <%s>\n", sel);
    //va_list ap;
    int nargs = 0;
    char *fp = fmt;
    //va_start(ap, end);
    sys_vgui("%s ", sel);
    while (*fp)
    {
        // stop-gap-- increase to 20 until we find a way to pass a list or array
        if (nargs >= 20)
        {
            pd_error(0, "sys_gui_vmess: only 10 named parameters allowed");
            break;
        }
        if (nargs > 0) sys_gui(",");
        switch(*fp++)
        {
        case 'f': sys_vgui(FLOAT_SPECIFIER, va_arg(ap, double)); break;
        case 's': escape_double_quotes(va_arg(ap, const char *)); break;
        case 'i': sys_vgui("%d", va_arg(ap, int)); break;
        case 'x': sys_vgui("\"" X_SPECIFIER "\"",
            va_arg(ap, t_uint));
            break;
        //case 'p': SETPOINTER(at, va_arg(ap, t_gpointer *)); break;
        default: goto done;
        }
        nargs++;
    }
done:
    va_end(ap);
    if (nargs) set_leading_array(0);
    if (end)
        gui_end_vmess();
}

void gui_vmess(const char *sel, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    gui_do_vmess(sel, fmt, 1, ap);
}

void gui_start_vmess(const char *sel, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_leading_array(1);
    gui_do_vmess(sel, fmt, 0, ap);
}

void gui_start_array(void)
{
    if (gui_array_head && !gui_array_tail)
        sys_gui("[");
    else
        sys_gui(",[");
    gui_array_head = 1;
    gui_array_tail = 0;
}

void gui_f(t_float f)
{
    if (gui_array_head && !gui_array_tail)
    {
        sys_vgui(FLOAT_SPECIFIER, f);
    }
    else
        sys_vgui("," FLOAT_SPECIFIER, f);
    gui_array_head = 0;
    gui_array_tail = 0;
}

void gui_i(int i)
{
    if (gui_array_head && !gui_array_tail)
    {
        sys_vgui("%d", i);
    }
    else
        sys_vgui(",%d", i);
    gui_array_head = 0;
    gui_array_tail = 0;
}

void gui_s(const char *s)
{
    if (gui_array_head && !gui_array_tail)
    {
        escape_double_quotes(s);
    }
    else
    {
        sys_vgui(",");
        escape_double_quotes(s);
    }
    gui_array_head = 0;
    gui_array_tail = 0;
}

void gui_x(t_uint i)
{
    if (gui_array_head && !gui_array_tail)
        sys_vgui("\"x%.6lx\"", i);
    else
        sys_vgui(",\"x%.6lx\"", i);
    gui_array_head = 0;
    gui_array_tail = 0;
}

void gui_end_array(void)
{
    sys_gui("]");
    gui_array_tail = 1;
}

/* Old GUI command interface... */
void sys_vguid(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    char buf[MAXPDSTRING], *bufp, *endp;
    va_start(ap, fmt);
    pd_vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    /* Just display the first non-empty line, to remove console clutter. -ag */
    for (bufp = buf; *bufp && isspace(*bufp); bufp++) ;
    if ((endp = strchr(bufp, '\n')))
    {
        *endp = 0;
        if (endp[1])
        {
            /* Indicate that more stuff follows with an ellipsis. */
            strncat(bufp, "...", MAXPDSTRING);
        }
    }
    gui_vmess("gui_legacy_tcl_command", "sis",
        file, line, bufp);
    //sys_vvguid(file,line,fmt,ap);
}

int sys_flushtogui(void)
{
    int writesize = INTER->i_guihead - INTER->i_guitail, nwrote = 0;
    if (writesize > 0)
        nwrote = do_send(INTER->i_guisock, INTER->i_guibuf + INTER->i_guitail, writesize, 0);

#if 0
    if (writesize)
        fprintf(stderr, "wrote %d of %d\n", nwrote, writesize);
#endif

    if (nwrote < 0)
    {
        perror("pd-to-gui socket");
        sys_bail(1);
    }
    else if (!nwrote)
        return (0);
    else if (nwrote >= INTER->i_guihead - INTER->i_guitail)
         INTER->i_guihead = INTER->i_guitail = 0;
    else if (nwrote)
    {
        INTER->i_guitail += nwrote;
        if (INTER->i_guitail > (INTER->i_guisize >> 2))
        {
            memmove(INTER->i_guibuf, INTER->i_guibuf + INTER->i_guitail,
                INTER->i_guihead - INTER->i_guitail);
            INTER->i_guihead = INTER->i_guihead - INTER->i_guitail;
            INTER->i_guitail = 0;
        }
    }
    return (1);
}

void glob_ping(t_pd *dummy)
{
    INTER->i_waitingforping = 0;
}

static int sys_flushqueue(void)
{
    int wherestop = INTER->i_bytessincelastping + GUI_UPDATESLICE;
    if (wherestop + (GUI_UPDATESLICE >> 1) > GUI_BYTESPERPING)
        wherestop = 0x7fffffff;
    if (INTER->i_waitingforping)
        return (0);
    if (!INTER->i_guiqueuehead)
        return (0);
    while (1)
    {
        if (INTER->i_bytessincelastping >= GUI_BYTESPERPING)
        {
            //sys_gui("pdtk_ping\n");
            gui_vmess("gui_ping", "");
            INTER->i_bytessincelastping = 0;
            INTER->i_waitingforping = 1;
            return (1);
        }
        if (INTER->i_guiqueuehead)
        {
            t_guiqueue *headwas = INTER->i_guiqueuehead;
            INTER->i_guiqueuehead = headwas->gq_next;
            (*headwas->gq_fn)(headwas->gq_client, headwas->gq_glist);
            t_freebytes(headwas, sizeof(*headwas));
            if (INTER->i_bytessincelastping >= wherestop)
                break;
        }
        else break;
    }
    sys_flushtogui();
    return (1);
}

    /* flush output buffer and update queue to gui in small time slices */
static int sys_poll_togui(void) /* returns 1 if did anything */
{
    if (!sys_havegui())
        return (0);
        /* in case there is stuff still in the buffer, try to flush it. */
    sys_flushtogui();
        /* if the flush wasn't complete, wait. */
    if (INTER->i_guihead > INTER->i_guitail)
        return (0);

        /* check for queued updates */
    if (sys_flushqueue())
        return (1);

    return (0);
}

    /* if some GUI object is having to do heavy computations, it can tell
    us to back off from doing more updates by faking a big one itself. */
void sys_pretendguibytes(int n)
{
    INTER->i_bytessincelastping += n;
}

void sys_queuegui(void *client, t_glist *glist, t_guicallbackfn f)
{
    t_guiqueue **gqnextptr, *gq;
    if (!INTER->i_guiqueuehead)
        gqnextptr = &INTER->i_guiqueuehead;
    else
    {
        for (gq = INTER->i_guiqueuehead; gq->gq_next; gq = gq->gq_next)
            if (gq->gq_client == client)
                return;
        if (gq->gq_client == client)
            return;
        gqnextptr = &gq->gq_next;
    }
    gq = t_getbytes(sizeof(*gq));
    gq->gq_next = 0;
    gq->gq_client = client;
    gq->gq_glist = glist;
    gq->gq_fn = f;
    gq->gq_next = 0;
    *gqnextptr = gq;
}

void sys_unqueuegui(void *client)
{
    t_guiqueue *gq, *gq2;
    while (INTER->i_guiqueuehead && INTER->i_guiqueuehead->gq_client == client)
    {
        gq = INTER->i_guiqueuehead;
        INTER->i_guiqueuehead = INTER->i_guiqueuehead->gq_next;
        t_freebytes(gq, sizeof(*gq));
    }
    if (!INTER->i_guiqueuehead)
        return;
    for (gq = INTER->i_guiqueuehead; gq2 = gq->gq_next; gq = gq2)
        if (gq2->gq_client == client)
    {
        gq->gq_next = gq2->gq_next;
        t_freebytes(gq2, sizeof(*gq2));
        break;
    }
}

    /* poll for any incoming packets, or for GUI updates to send.  call with
    the PD instance lock set. */
int sys_pollgui(void)
{
    static double lasttime = 0;
    double now = 0;
    int didsomething = sys_domicrosleep(0);
    if (!didsomething || (now = sys_getrealtime()) > lasttime + 0.5)
    {
        didsomething |= sys_poll_togui();
        if (now)
            lasttime = now;
    }
    return (didsomething);
}

void sys_init_fdpoll(void)
{
    if (INTER->i_fdpoll)
        return;
    /* create an empty FD poll list */
    INTER->i_fdpoll = (t_fdpoll *)t_getbytes(0);
    INTER->i_nfdpoll = 0;
    INTER->i_inbinbuf = binbuf_new();
}

/* --------------------- starting up the GUI connection ------------- */

static int sys_watchfd = -1;

void glob_watchdog(void *dummy)
{
    if (sys_watchfd < 0)
        return;
    if (write(sys_watchfd, "\n", 1) < 1)
    {
        fprintf(stderr, "pd: watchdog process died\n");
        sys_bail(1);
    }
}

extern t_symbol *pd_getplatform(void);
extern void sys_expandpath(const char *from, char *to, int bufsize);

/* set the datadir for nwjs. We use the published nw.js
   defaults if there's only one instance of the GUI set to
   run. Otherwise, we append the port number so that
   nw.js will spawn a new instance for us.

   Currently, users can give a command line arg to force Pd
   to start with a new GUI instance. A new GUI must also get
   created when a user turns on audio and there is a [pd~] object
   on the canvas. The latter would actually be more usable within
   a single GUI instance, but that would require some way to
   visually distinguish patches that are associated with different Pd
   engine processes.
*/

static void set_datadir(char *buf, int new_gui_instance, int portno)
{
    char dir[FILENAME_MAX];
    t_symbol *platform = pd_getplatform();
    strcpy(buf, "--user-data-dir=");
    /* Let's go ahead and quote the string to handle spaces in
       paths, etc. */
    strcat(buf, "\"");
    if (platform == gensym("darwin"))
        sys_expandpath("~/Library/Application Support/", dir, FILENAME_MAX);
    else if (platform != gensym("win32"))/* bsd and linux */
        sys_expandpath("~/.config/", dir, FILENAME_MAX);
#ifdef _WIN32
        /* win32 has a more robust API for getting the
           value of %LOCALAPPDATA%, but this works on
           Windows 7 and is straightforward... */
        char *env = getenv("LOCALAPPDATA");
        strcpy(dir, env);
        strcat(dir, "\\");
#endif
    strcat(dir, "pd-l2ork");
    if (new_gui_instance)
    {
        char portbuf[10];
        sprintf(portbuf, "-%d", portno);
        strcat(dir, portbuf);
    }
    strcat(buf, dir);
    /* Finally, close quote... */
    strcat(buf, "\"");
}

extern int sys_unique;
// TODO-EXISTING: this is currently unused. should
// revisit later to see if we can do opening of
// new files in a more graceful fashion. For more
// info, see TODO-EXISTING prompts below.
extern t_namelist *sys_openlist;

static int sys_do_startgui(const char *guidir)
{
    char cwd[FILENAME_MAX];
    char apibuf[256], apibuf2[256];
    struct addrinfo *ailist = NULL, *ai;
    int sockfd = -1;
    int portno = -1;
    stderr_isatty = isatty(2);
#ifdef _WIN32
    if (GetCurrentDirectory(FILENAME_MAX, cwd) == 0)
        strcpy(cwd, ".");
#endif
#if defined(HAVE_UNISTD_H) || defined(__EMSCRIPTEN__)
    if (!getcwd(cwd, FILENAME_MAX))
        strcpy(cwd, ".");

    int stdinpipe[2];
    pid_t childpid;
#endif /* _WIN32 */

    sys_init_fdpoll();

    if (sys_guisetportnumber)  /* GUI exists and sent us a port number */
    {
        int status;
#ifdef __APPLE__
        /* guisock might be 1 or 2, which will have offensive results
        if somebody writes to stdout or stderr - so we just open a few
            files to try to fill fds 0 through 2.  (I tried using dup()
            instead, which would seem the logical way to do this, but couldn't
            get it to work.) */
        int burnfd1 = open("/dev/null", 0), burnfd2 = open("/dev/null", 0),
            burnfd3 = open("/dev/null", 0);
        if (burnfd1 > 2)
            close(burnfd1);
        if (burnfd2 > 2)
            close(burnfd2);
        if (burnfd3 > 2)
            close(burnfd3);
#endif
        /* get addrinfo list using hostname & port */
        status = addrinfo_get_list(&ailist,
            LOCALHOST, sys_guisetportnumber, SOCK_STREAM);
        if (status != 0)
        {
            fprintf(stderr,
                "localhost not found (inet protocol not installed?)\n%s (%d)",
                gai_strerror(status), status);
            return (1);
        }

        /* Sort to IPv4 for now as the Pd gui uses IPv4. */
        addrinfo_sort_list(&ailist, addrinfo_ipv4_first);

        /* We don't know in advance whether the GUI uses IPv4 or IPv6,
           so we try both and pick the one which works. */
        for (ai = ailist; ai != NULL; ai = ai->ai_next)
        {
            /* create a socket */
            sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (sockfd < 0)
                continue;
#ifndef _WIN32
            if(fcntl(sockfd, F_SETFD, FD_CLOEXEC) < 0)
                perror("close-on-exec");
#endif
        #if 1
            if (socket_set_boolopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
                fprintf(stderr, "setsockopt (TCP_NODELAY) failed");
        #endif
            if (socket_connect(sockfd, ai->ai_addr, ai->ai_addrlen, 10.f) < 0)
            {
                sys_closesocket(sockfd);
                sockfd = -1;
                continue;
            }
            /* this addr worked */
            break;
        }
        freeaddrinfo(ailist);

        /* confirm that we could connect */
        if (sockfd < 0)
            /* try to connect */
        {
            sys_sockerror("connecting stream socket");
            return (1);
        }

        INTER->i_guisock = sockfd;
    }
#ifndef __EMSCRIPTEN__
    else    /* default behavior: start up the GUI ourselves. */
    {
        struct sockaddr_storage addr;
        int status;
#ifdef _WIN32
        char scriptbuf[MAXPDSTRING+30], wishbuf[MAXPDSTRING+30], portbuf[80];
        int spawnret;
#else
        char cmdbuf[4*MAXPDSTRING];
        const char *guicmd;
#endif
        /* get addrinfo list using hostname (get random port from OS) */
        status = addrinfo_get_list(&ailist, LOCALHOST, 0, SOCK_STREAM);
        if (status != 0)
        {
            sys_sockerror("socket");
            fprintf(stderr,
                "localhost not found (inet protocol not installed?)\n%s (%d)",
                gai_strerror(status), status);
            return (1);
        }

        /* we prefer the IPv4 addresses because the GUI might not be IPv6 capable. */
        addrinfo_sort_list(&ailist, addrinfo_ipv4_first);
        /* try each addr until we find one that works */
        for (ai = ailist; ai != NULL; ai = ai->ai_next)
        {
            sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (sockfd < 0)
                continue;
#ifndef _WIN32
            if(fcntl(sockfd, F_SETFD, FD_CLOEXEC) < 0)
                perror("close-on-exec");
#endif
        #if 1
            /* ask OS to allow another process to reopen this port after we close it */
            if (socket_set_boolopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 1) < 0)
                fprintf(stderr, "setsockopt (SO_REUSEADDR) failed\n");
        #endif
        #if 1
            /* stream (TCP) sockets are set NODELAY */
            if (socket_set_boolopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 1) < 0)
                fprintf(stderr, "setsockopt (TCP_NODELAY) failed");
        #endif
            /* name the socket */
            if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
            {
                socket_close(sockfd);
                sockfd = -1;
                continue;
            }
            /* this addr worked */
            memcpy(&addr, ai->ai_addr, ai->ai_addrlen);
            break;
        }
        freeaddrinfo(ailist);

        /* confirm that socket/bind worked */
        if (sockfd < 0)
        {
            sys_sockerror("bind");
            return (1);
        }
        /* get the actual port number */
        portno = socket_get_port(sockfd);
        if (sys_verbose) fprintf(stderr, "port %d\n", portno);

#ifndef _WIN32
        if (sys_guicmd)
        {
            sprintf(cmdbuf, "\"%s\" %d\n", sys_guicmd, portno);
            guicmd = cmdbuf;
        }
        else
        {
#ifdef __APPLE__
            int i;
            struct stat statbuf;
            glob_t glob_buffer;
            char *homedir = getenv("HOME");
            char embed_glob[FILENAME_MAX];
            char embed_filename[FILENAME_MAX], home_filename[FILENAME_MAX];
            char *wish_paths[10] = {
                "(custom wish not defined)",
                "(did not find a home directory)",
                "/Applications/Utilities/Wish.app/Contents/MacOS/Wish",
                "/Applications/Utilities/Wish Shell.app/Contents/MacOS/Wish Shell",
                "/Applications/Wish.app/Contents/MacOS/Wish",
                "/Applications/Wish Shell.app/Contents/MacOS/Wish Shell",
                "/usr/local/bin/wish8.4",
                "/opt/bin/wish8.4"
                "/usr/bin/wish8.4"
                "/usr/local/bin/wish8.4",
                "/usr/local/bin/wish",
                "/usr/bin/wish"
            };
            /* this glob is needed so the Wish executable can have the same
             * filename as the Pd.app, i.e. 'Pd-0.42-3.app' should have a Wish
             * executable called 'Pd-0.42-3.app/Contents/MacOS/Pd-0.42-3' */
            sprintf(embed_glob, "%s/../../MacOS/nwjs", guidir);
            glob_buffer.gl_matchc = 1; /* we only need one match */
            glob(embed_glob, GLOB_LIMIT, NULL, &glob_buffer);
            if (glob_buffer.gl_pathc > 0) {
                strcpy(embed_filename, glob_buffer.gl_pathv[0]);
                wish_paths[0] = embed_filename;
            }
            sprintf(home_filename,
                    "%s/Applications/Wish.app/Contents/MacOS/Wish",homedir);
            wish_paths[1] = home_filename;
            for(i=0;i<10;i++) {
                if (sys_verbose)
                    fprintf(stderr, "Trying Wish at \"%s\"\n", wish_paths[i]);
                if (stat(wish_paths[i], &statbuf) >= 0)
                    break;
            }
            sprintf(cmdbuf,"\"%s\" %s/pd.tk %d\n",wish_paths[i],guidir,portno);
            char data_dir_flag[MAXPDSTRING];
            set_datadir(data_dir_flag, sys_unique, portno);
            /* Make a copy in case the user wants to change to the repo
               dir while developing... */
            char guidir2[MAXPDSTRING];
            /* Let's go ahead and quote the path in case there are spaces
               in it. This won't happen on a sane Linux/BSD setup. But it
               will happen under Windows, so we quote here, too, for
               the sake of consistency. */
            strcpy(guidir2, "\"");
            strcat(guidir2, guidir);
            strcat(guidir2, "\"");
            /* The following line checks if our GUIDIR was defined
               to something other than empty string. If so it will
               use that dir to find the nw binary. */
            if (strcmp(GUIDIR, ""))
                strcpy(guidir2, "\"" GUIDIR "\"");
            //strcpy(guidir2, "\"/home/user/pd-l2ork/pd/nw\"");
            sprintf(cmdbuf,
                "\"%s\" %s %s "
                "%d localhost %s %s " X_SPECIFIER,
                wish_paths[i],
                data_dir_flag,
                guidir2,
                portno,
                (sys_k12_mode ? "pd-l2ork-k12" : "pd-l2ork"),
                guidir2,
                (t_uint)pd_this);
#else
            sprintf(cmdbuf,
                "TCL_LIBRARY=\"%s/tcl/library\" TK_LIBRARY=\"%s/tk/library\" \
                 \"%s/pd-gui\" %d localhost %s\n",
                 sys_libdir->s_name, sys_libdir->s_name, guidir, portno, (sys_k12_mode ? "pd-l2ork-k12" : "pd-l2ork"));

            fprintf(stderr, "guidir is %s\n", guidir);

            /* For some reason, the nw binary doesn't give you access to
               the first argument-- this is the path to the directory where
               package.json lives (or the zip file if it's compressed). So
               we add it again as the last argument to make sure we can fetch
               it on the GUI side. */
            char data_dir_flag[MAXPDSTRING];
            set_datadir(data_dir_flag, sys_unique, portno);

            /* Make a copy in case the user wants to change to the repo
               dir while developing... */
            char guidir2[MAXPDSTRING];
            /* Let's go ahead and quote the path in case there are spaces
               in it. This won't happen on a sane Linux/BSD setup. But it
               will happen under Windows, so we quote here, too, for
               the sake of consistency. */
            strcpy(guidir2, "\"");
            strcat(guidir2, guidir);
            strcat(guidir2, "\"");
            /* The following line checks if our GUIDIR was defined
               to something other than empty string. If so it will
               use that dir to find the nw binary. */
            if (strcmp(GUIDIR, ""))
                strcpy(guidir2, "\"" GUIDIR "\"");

            //strcpy(guidir2, "\"/home/user/pd-l2ork/pd/nw\"");
            sprintf(cmdbuf,
                "%s/nw/nw %s %s "
                "%d localhost %s %s " X_SPECIFIER,
                guidir2,
                data_dir_flag,
                guidir2,
                portno,
                (sys_k12_mode ? "pd-l2ork-k12" : "pd-l2ork"),
                guidir2,
                (t_uint)pd_this);
            // TODO-EXISTING: consider adding files here, see
            // comments below where Windows version is invoked.
            /*
            if (sys_openlist)
            {
                t_namelist *nl;
                sprintf(strchr(cmdbuf, '\0'), " <patches> ");
                for (nl = sys_openlist; nl; nl = nl->nl_next)
                    sprintf(strchr(cmdbuf, '\0'), "%s ", nl->nl_string);
            }
            fprintf(stderr,"final command: %s\n", cmdbuf);
            */
#endif
            guicmd = cmdbuf;
        }

        if (sys_verbose)
            fprintf(stderr, "%s", guicmd);

        childpid = fork();
        if (childpid < 0)
        {
            if (errno) perror("sys_startgui");
            else fprintf(stderr, "sys_startgui failed\n");
            sys_closesocket(sockfd);
            return (1);
        }
        else if (!childpid)                     /* we're the child */
        {
            sys_closesocket(sockfd); /* we're the child */
            sys_set_priority(MODE_NRT);
#ifndef __APPLE__
                /* the wish process in Unix will make a wish shell and
                    read/write standard in and out unless we close the
                    file descriptors.  Somehow this doesn't make the MAC OSX
                        version of Wish happy...*/
            if (pipe(stdinpipe) < 0)
                sys_sockerror("pipe");
            else
            {
                if (stdinpipe[0] != 0)
                {
                    close (0);
                    dup2(stdinpipe[0], 0);
                    close(stdinpipe[0]);
                }
            }

#endif
            execl("/bin/sh", "sh", "-c", guicmd, (char*)0);
            perror("pd: exec");
            _exit(1);
       }
#else
            /* in MSW land "guipath" is unused; we just do everything from
            the libdir. */
        /* fprintf(stderr, "%s\n", sys_libdir->s_name); */

        //strcpy(scriptbuf, "\"");
        //strcat(scriptbuf, sys_libdir->s_name);
        //strcat(scriptbuf, "/" PDBINDIR "pd.tk\"");
        //sys_bashfilename(scriptbuf, scriptbuf);

        char pd_this_string[80];
        sprintf(pd_this_string, X_SPECIFIER, (t_uint)pd_this);
        sprintf(scriptbuf, "\""); /* use quotes in case there are spaces */
        strcat(scriptbuf, sys_libdir->s_name);
        strcat(scriptbuf, "/" PDBINDIR);
        sys_bashfilename(scriptbuf, scriptbuf);
        /* PDBINDIR ends with a "\", which will unfortunately end up
           escaping our trailing double quote. So we replace the "\" with
           our double quote. nw.js seems to find the path in this case,
           so this _should_ work for all cases. */
        scriptbuf[strlen(scriptbuf)-1] = '"';

        sprintf(portbuf, "%d", portno);

        strcpy(wishbuf, sys_libdir->s_name);
        strcat(wishbuf, "/" PDBINDIR "nw/nw");
        sys_bashfilename(wishbuf, wishbuf);

        char data_dir_flag[MAXPDSTRING];
        // at this point we do not know if there is another instance
        // running and sys_unique can only tell us if the user wants
        // to spawn a new instance.
        set_datadir(data_dir_flag, sys_unique, portno);
        //fprintf(stderr,"sys_startgui calling spawnret port=%d\n", portno);
        spawnret = _spawnl(P_NOWAIT, wishbuf, "pd-nw",
            data_dir_flag,
            scriptbuf,
            portbuf,
            "localhost",
            (sys_k12_mode ? "pd-l2ork-k12" : "pd-l2ork"),
            scriptbuf, pd_this_string,
            // TODO-EXISTING: consider appending files, as shown
            // in the line below. openpatches serves as delineator.
            // then, need to figure out a robust way of parsing files
            // possibly by surrounding them with a special character?
            // this is important because files can have spaces, so.
            // the usual parsing methods will not cut it.
            // THEN: do the same for Linux/OSX above (look for the
            // TODO-EXISTING comment). 2.pd is listed as a hardcoded
            // example that should be replaced with the code provided
            // in the TODO-EXISTING prompt above this one.
            //"openpatches", "2.pd",
            NULL);
        if (spawnret < 0)
        {
            perror("spawnl");
            fprintf(stderr, "%s: couldn't load GUI\n", wishbuf);
            return (1);
        }
#endif /* MSW */
        if (sys_verbose)
            fprintf(stderr, "Waiting for connection request... \n");
        if (listen(sockfd, 5) < 0)
        {
            sys_sockerror("listen");
            sys_closesocket(sockfd);
            return (1);
        }

        INTER->i_guisock = accept(sockfd, 0, 0);

        sys_closesocket(sockfd);

        if (INTER->i_guisock < 0)
        {
            sys_sockerror("accept");
            return (1);
        }
        if (sys_verbose)
            fprintf(stderr, "... connected\n");
        INTER->i_guihead = INTER->i_guitail = 0;
    }
#endif /* __EMSCRIPTEN__ */
    if (!sys_nogui)
    {
        char buf[256], buf2[256];
#ifndef __EMSCRIPTEN__
        INTER->i_socketreceiver = socketreceiver_new(0, 0, 0, 0);
        sys_addpollfn(INTER->i_guisock, (t_fdpollfn)socketreceiver_read,
             INTER->i_socketreceiver);
#endif /* __EMSCRIPTEN__ */

            /* here is where we start the pinging. */
#if PD_WATCHDOG
        if (sys_hipriority)
            gui_vmess("gui_watchdog", "");
#endif
        sys_get_audio_apis(buf);
        sys_get_midi_apis(buf2);
                            /* ... and about font, media APIS, etc */
        t_binbuf *aapis = binbuf_new(), *mapis = binbuf_new();
        sys_get_audio_apis2(aapis);
        sys_get_midi_apis2(mapis);
        gui_start_vmess("gui_startup", "sss",
		  pd_version,
		  sys_font,
		  sys_fontweight);

        int i;
        gui_start_array(); // audio apis
        for (i = 0; i < binbuf_getnatom(aapis); i+=2)
        {
            gui_s(atom_getsymbol(binbuf_getvec(aapis)+i)->s_name);
            gui_i(atom_getint(binbuf_getvec(aapis)+i+1));
        }
        gui_end_array();

        gui_start_array(); // midi apis
        for (i = 0; i < binbuf_getnatom(mapis); i+=2)
        {
            gui_s(atom_getsymbol(binbuf_getvec(mapis)+i)->s_name);
            gui_i(atom_getint(binbuf_getvec(mapis)+i+1));
        }
        gui_end_array();

        gui_end_vmess();
        gui_vmess("gui_set_cwd", "xs", 0, cwd);
        binbuf_free(aapis);
        binbuf_free(mapis);
    }
    return (0);

}

void sys_setrealtime(const char *libdir)
{
    char cmdbuf[MAXPDSTRING];
#if PD_WATCHDOG
        /*  promote this process's priority, if we can and want to.
        If sys_hipriority not specified (-1), we assume real-time was wanted.
        Starting in Linux 2.6 one can permit real-time operation of Pd by
        putting lines like:
                @audio - rtprio 99
                @audio - memlock unlimited
        in the system limits file, perhaps /etc/limits.conf or
        /etc/security/limits.conf, and calling Pd from a user in group audio. */
    if (sys_hipriority == -1)
        sys_hipriority = 1;

    pd_snprintf(cmdbuf, MAXPDSTRING, "%s/bin/pd-watchdog", libdir);
    cmdbuf[MAXPDSTRING-1] = 0;
    if (sys_hipriority)
    {
        struct stat statbuf;
        if (stat(cmdbuf, &statbuf) < 0)
        {
            fprintf(stderr,
              "disabling real-time priority due to missing pd-watchdog (%s)\n",
                cmdbuf);
            sys_hipriority = 0;
        }
    }
    if (sys_hipriority)
    {
        int pipe9[2], watchpid;
            /* To prevent lockup, we fork off a watchdog process with
            higher real-time priority than ours.  The GUI has to send
            a stream of ping messages to the watchdog THROUGH the Pd
            process which has to pick them up from the GUI and forward
            them.  If any of these things aren't happening the watchdog
            starts sending "stop" and "cont" signals to the Pd process
            to make it timeshare with the rest of the system.  (Version
            0.33P2 : if there's no GUI, the watchdog pinging is done
            from the scheduler idle routine in this process instead.) */

        if (pipe(pipe9) < 0)
        {
            sys_sockerror("pipe");
            return;
        }
        watchpid = fork();
        if (watchpid < 0)
        {
            if (errno)
                perror("sys_setpriority");
            else fprintf(stderr, "sys_setpriority failed\n");
            return;
        }
        else if (!watchpid)             /* we're the child */
        {
            sys_set_priority(MODE_WATCHDOG);
            if (pipe9[1] != 0)
            {
                dup2(pipe9[0], 0);
                close(pipe9[0]);
            }
            close(pipe9[1]);

            if (sys_verbose) fprintf(stderr, "%s\n", cmdbuf);
            execl(cmdbuf, cmdbuf, (char*)0);
            perror("pd: exec");
            _exit(1);
        }
        else                            /* we're the parent */
        {
            sys_set_priority(MODE_RT);
            close(pipe9[0]);
                /* set close-on-exec so that watchdog will see an EOF when we
                close our copy - otherwise it might hang waiting for some
                stupid child process (as seems to happen if jackd auto-starts
                for us.) */
#ifndef _WIN32
            if(fcntl(pipe9[1], F_SETFD, FD_CLOEXEC) < 0)
              perror("close-on-exec");
#endif
            sys_watchfd = pipe9[1];
                /* We also have to start the ping loop in the GUI;
                this is done later when the socket is open. */
        }
    }
    else if (sys_verbose)
        post("not setting real-time priority");
#endif /* __linux__ */

#ifdef _WIN32
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        fprintf(stderr, "pd: couldn't set high priority class\n");
#endif
#ifdef __APPLE__
    if (sys_hipriority)
    {
        struct sched_param param;
        int policy = SCHED_RR;
        int err;
        param.sched_priority = 80; /* adjust 0 : 100 */

        err = pthread_setschedparam(pthread_self(), policy, &param);
        if (err)
            post("warning: high priority scheduling failed");
    }
#endif /* __APPLE__ */
}

void sys_do_close_audio(void);

/* This is called when something bad has happened, like a segfault.
Call glob_quit() below to exit cleanly.
LATER try to save dirty documents even in the bad case. */
void sys_bail(int n)
{
    static int reentered = 0;
    if (!reentered)
    {
        reentered = 1;
#if !defined(__linux__) && !defined(__FreeBSD_kernel__) && !defined(__GNU__) /* sys_close_audio() hangs if you're in a signal? */
        fprintf(stderr ,"gui socket %d - \n", INTER->i_guisock);
        fprintf(stderr, "closing audio...\n");
        sys_do_close_audio();
        fprintf(stderr, "closing MIDI...\n");
        sys_close_midi();
        fprintf(stderr, "... done.\n");
#endif
        exit(n);
    }
    else _exit(1);
}

//extern t_pd *garray_arraytemplatecanvas;
//extern t_pd *garray_floattemplatecanvas;
extern void glob_closeall(void *dummy, t_floatarg fforce);

extern int do_not_redraw;
extern void glob_savepreferences(t_pd *dummy);

void sys_exit(int status);

void glob_quit(void *dummy, t_floatarg status)
{
    if (!sys_havegui())
    {
        // ag: Take the quick way out. Specifically, we do *not* want to clean
        // up a non-existent gui here (which also causes spurious segfaults in
        // gui-less operation on Windows).
        canvas_suspend_dsp();
        sys_bail(status);
        // sys_bail shouldn't return, but just in case:
        return;
    }
    /* If we're going to try to cleanly close everything here, we should
       do the same for all open patches and that is currently not the case,
       so for the time being, let's just leave OS to deal with freeing of all
       memory when the program exits... */

    /* Let's try to cleanly remove invisible template canvases */
    //if (garray_arraytemplatecanvas)
    //    canvas_free((t_canvas *)garray_arraytemplatecanvas);
    //if (garray_floattemplatecanvas)
    //    canvas_free( (t_canvas *)garray_floattemplatecanvas);
    canvas_suspend_dsp();
    do_not_redraw = 1;
    glob_closeall(0, 1);
    // ico@vt.edu 2022-12-14: save preferences on quit
    glob_savepreferences(dummy);
    //sys_vgui("exit\n");
    gui_vmess("app_quit", "");
    sys_close_audio();
    sys_close_midi();
    if (sys_havegui())
    {
        sys_closesocket(INTER->i_guisock);
        sys_rmpollfn(INTER->i_guisock);
    }
    exit(status);
}

    /* recursively descend to all canvases and send them "vis" messages
    if they believe they're visible, to make it really so. */
static void glist_maybevis(t_glist *gl)
{
    t_gobj *g;
    for (g = gl->gl_list; g; g = g->g_next)
        if (pd_class(&g->g_pd) == canvas_class)
            glist_maybevis((t_glist *)g);
    if (gl->gl_havewindow)
    {
        canvas_vis(gl, 0);
        canvas_vis(gl, 1);
    }
}

    /* this is called from main when GUI has given us out font metrics,
    so that we can now draw all "visible" canvases.  These include all
    root canvases and all subcanvases that already believe they're visible. */
void sys_doneglobinit(void)
{
    t_canvas *x;
    for (x = pd_getcanvaslist(); x; x = x->gl_next)
        if (strcmp(x->gl_name->s_name, "_float_template") &&
            strcmp(x->gl_name->s_name, "_float_array_template") &&
                strcmp(x->gl_name->s_name, "_text_template"))
    {
        glist_maybevis(x);
        canvas_vis(x, 1);
    }
}

    /* start the GUI up.  Before we actually draw our "visible" windows
    we have to wait for the GUI to give us our font metrics, see
    glob_initfromgui().  LATER it would be cool to figure out what metrics
    we really need and tell the GUI - that way we can support arbitrary
    zoom with appropriate font sizes.   And/or: if we ever move definitively
    to a vector-based GUI lib we might be able to skip this step altogether. */
int sys_startgui(const char *libdir)
{
    t_canvas *x;
    stderr_isatty = isatty(2);
    for (x = pd_getcanvaslist(); x; x = x->gl_next)
        canvas_vis(x, 0);
    INTER->i_havegui = 1;
    INTER->i_guihead = INTER->i_guitail = 0;
    INTER->i_waitingforping = 0;
    if (sys_do_startgui(libdir))
        return (-1);
    return (0);
}

 /* more work needed here - for some reason we can't restart the gui after
 shutting it down this way.  I think the second 'init' message never makes
 it because the to-gui buffer isn't re-initialized. */
void sys_stopgui(void)
{
    t_canvas *x;
    for (x = pd_getcanvaslist(); x; x = x->gl_next)
        canvas_vis(x, 0);
    // sys_vgui("%s", "exit\n");
    sys_flushtogui();
    sys_flushtogui();
    if (INTER->i_guisock >= 0)
    {
        sys_closesocket(INTER->i_guisock);
        sys_rmpollfn(INTER->i_guisock);
        INTER->i_guisock = -1;
    }
    INTER->i_havegui = 0;
}

/* ----------- mutexes for thread safety --------------- */
#if PDTHREADS
#include "pthread.h"
#endif

void s_inter_newpdinstance(void)
{
    INTER = getbytes(sizeof(*INTER));
#if PDTHREADS
    pthread_mutex_init(&INTER->i_mutex, NULL);
    pd_this->pd_islocked = 0;
#endif
#ifdef _WIN32
    INTER->i_freq = 0;
#endif
    INTER->i_havegui = 0;
    INTER->i_guisock = -1;
}

void s_inter_free(t_instanceinter *inter)
{
    if (inter->i_fdpoll)
    {
        binbuf_free(inter->i_inbinbuf);
        inter->i_inbinbuf = 0;
        t_freebytes(inter->i_fdpoll, inter->i_nfdpoll * sizeof(t_fdpoll));
        inter->i_fdpoll = 0;
        inter->i_nfdpoll = 0;
    }
#if PDTHREADS
    pthread_mutex_destroy(&inter->i_mutex);
#endif
    freebytes(inter, sizeof(*inter));
}

void s_inter_freepdinstance(void)
{
    s_inter_free(INTER);
}

#if PDTHREADS
#ifdef PDINSTANCE
static pthread_rwlock_t sys_rwlock = PTHREAD_RWLOCK_INITIALIZER;
#else /* PDINSTANCE */
static pthread_mutex_t sys_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* PDINSTANCE */
#endif /* PDTHREADS */

#if PDTHREADS

/* routines to lock and unlock Pd's global class structure or list of Pd
instances.  These are called internally within Pd when creating classes, adding
methods to them, or creating or freeing Pd instances.  They should probably
not be called from outside Pd.  They should be called at a point where the
current instance of Pd is currently locked via sys_lock() below; this gains
read access to the class and instance lists which must be released for the
write-lock to be available. */

void pd_globallock(void)
{
#ifdef PDINSTANCE
    if (!pd_this->pd_islocked)
        bug("pd_globallock");
    pthread_rwlock_unlock(&sys_rwlock);
    pthread_rwlock_wrlock(&sys_rwlock);
#endif /* PDINSTANCE */
}

void pd_globalunlock(void)
{
#ifdef PDINSTANCE
    pthread_rwlock_unlock(&sys_rwlock);
    pthread_rwlock_rdlock(&sys_rwlock);
#endif /* PDINSTANCE */
}

/* routines to lock/unlock a Pd instance for thread safety.  Call pd_setinsance
first.  The "pd_this"  variable can be written and read thread-safely as it
is defined as per-thread storage. */
void sys_lock(void)
{
#ifdef PDINSTANCE
    pthread_mutex_lock(&INTER->i_mutex);
    pthread_rwlock_rdlock(&sys_rwlock);
    pd_this->pd_islocked = 1;
#else
    pthread_mutex_lock(&sys_mutex);
#endif
}

void sys_unlock(void)
{
#ifdef PDINSTANCE
    pd_this->pd_islocked = 0;
    pthread_rwlock_unlock(&sys_rwlock);
    pthread_mutex_unlock(&INTER->i_mutex);
#else
    pthread_mutex_unlock(&sys_mutex);
#endif
}

int sys_trylock(void)
{
#ifdef PDINSTANCE
    int ret;
    if (!(ret = pthread_mutex_trylock(&INTER->i_mutex)))
    {
        if (!(ret = pthread_rwlock_tryrdlock(&sys_rwlock)))
            return (0);
        else
        {
            pthread_mutex_unlock(&INTER->i_mutex);
            return (ret);
        }
    }
    else return (ret);
#else
    return pthread_mutex_trylock(&sys_mutex);
#endif
}

#else /* PDTHREADS */

#ifdef TEST_LOCKING /* run standalone Pd with this to find deadlocks */
static int amlocked;
void sys_lock(void)
{
    if (amlocked) bug("duplicate lock");
    amlocked = 1;
}

void sys_unlock(void)
{
    if (!amlocked) bug("duplicate unlock");
    amlocked = 0;
}
#else
void sys_lock(void) {}
void sys_unlock(void) {}
#endif
void pd_globallock(void) {}
void pd_globalunlock(void) {}

#endif /* PDTHREADS */
