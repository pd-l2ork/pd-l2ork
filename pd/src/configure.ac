dnl Process this file with autoconf to produce a configure script.
AC_INIT(d_arithmetic.c)

AC_CONFIG_HEADER(config.h)

AC_SUBST(oss, yes)
AC_SUBST(alsa, yes)
AC_SUBST(jack, no)
AC_SUBST(portaudio, no)
AC_SUBST(portmidi, no)
AC_SUBST(binarymode, -m755)
AC_SUBST(fftw, no)
AC_SUBST(PDLIB)
AC_SUBST(CPPFLAGS)
AC_SUBST(MORECFLAGS)
AC_SUBST(EXT)
AC_SUBST(USE_DEBUG_CFLAGS, no)
AC_SUBST(SYSSRC)
AC_SUBST(STRIPFLAG)
AC_SUBST(OSNUMBER)
AC_SUBST(EXTERNTARGET)
AC_SUBST(ASIOSRC)
AC_SUBST(PDEXEC)

dnl other defaults

dnl check for features

AC_ARG_ENABLE(oss,      [  --disable-oss           OSS audio driver], 
    oss=$enableval)
AC_ARG_ENABLE(alsa,     [  --enable-alsa           ALSA audio driver], 
    alsa=$enableval)
AC_ARG_ENABLE(jack,     [  --enable-jack           Jack audio server],
    jack=$enableval)
AC_ARG_ENABLE(portaudio,[  --enable-portaudio      PortAudio],
    portaudio=$enableval)
AC_ARG_ENABLE(portmidi, [  --enable-portmidi       PortMidi],
    portmidi=$enableval)
AC_ARG_ENABLE(debug,    [  --enable-debug          debugging support],
    USE_DEBUG_CFLAGS=$enableval)
AC_ARG_ENABLE(static,   [  --enable-static         link statically],
    static=$enableval)
AC_ARG_ENABLE(setuid,   [  --enable-setuid         install as setuid (linux)],
    setuid=$enableval)
AC_ARG_ENABLE(fftw,     [  --enable-fftw           use FFTW package],
    fftw=$enableval)
AC_ARG_ENABLE(fat,      [  --disable-fat           build fat binary on Mac OS X], 
    fat=$enableval, fat="yes")

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CPP

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h malloc.h sys/ioctl.h sys/time.h unistd.h bstring.h io.h)

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(gettimeofday select socket strerror)
AC_FUNC_ALLOCA

dnl Checks for libraries.
dnl Checking for `dlopen' function in -ldl:
AC_CHECK_LIB(dl, dlopen,PDLIB="$PDLIB -ldl",
                 echo "dynamic link support required" || exit 1)

dnl Checking for `sin' function in -lffm:
dnl ffm is the fast math library on the alpha
AC_CHECK_LIB(ffm, sin,PDLIB="$PDLIB -lffm")

dnl Checking for `sin' function in -lm:
AC_CHECK_LIB(m, sin,PDLIB="$PDLIB -lm",
                 echo "math library required" || exit 1)

dnl Checking for `pthread_create' function in -pthread
AC_CHECK_LIB(pthread, pthread_create,PDLIB="$PDLIB -lpthread",
    AC_CHECK_LIB(pthreadGC2, pthread_create, PDLIB="$PDLIB -lpthreadGC2",
        AC_CHECK_LIB(pthreadGC1, pthread_create, PDLIB="$PDLIB -lpthreadGC1",
            AC_CHECK_LIB(pthreadGC, pthread_create, PDLIB="$PDLIB -lpthreadGC",
         echo "pthreads required"; exit 1))))

dnl Check for fftw package
if test x$fftw = "xyes";
then
AC_CHECK_LIB(fftw3f, fftwf_version,PDLIB="$PDLIB -lfftw3f",
    echo "fftw3 package not found - using built-in FFT"; fftw=no)
fi

PDEXEC=pd-l2ork

if test `uname -s` = Linux -o `uname -s` = "GNU/kFreeBSD" -o `uname -s` = "GNU";
then
    dnl Ckecking for ALSA
    echo .................... alsa= $alsa
dnl This should be fixed so Pd can use ALSA shared libraries where appropriate.
    if test x$alsa = xyes; then
        AC_CHECK_LIB(asound,snd_pcm_info,PDLIB="$PDLIB -lasound" ; alsa="yes",alsa="no")
    fi

    dnl Checking for JACK

    if test x$jack = xyes; then
        AC_CHECK_LIB(rt,shm_open,LIBS="$LIBS -lrt")
        AC_CHECK_LIB(jack,jack_set_xrun_callback,PDLIB="$PDLIB -ljack";jack=xrun,jack=no)
        AC_CHECK_LIB(jack,jack_set_error_function,PDLIB="$PDLIB -ljack";jack=yes,jack=no)
    fi

    LDFLAGS="$LDFLAGS -Wl,-export-dynamic"
    if test "$static" = "yes"; then 
      LDFLAGS="$LDFLAGS -static"
    fi 
    EXT=pd_linux
    CPPFLAGS="-DPD_INTERNAL -DHAVE_LIBDL -DPA_USE_OSS -DUNIX -DHAVE_UNISTD_H -fno-strict-aliasing"
dnl No OSS on hurd
    if test `uname -s` = "GNU";
    then
        SYSSRC="s_midi_dummy.c"
    else
        if test x$oss = "xyes"; then
            SYSSRC="s_midi_oss.c s_audio_oss.c"
            CPPFLAGS=$CPPFLAGS" -DPA_USE_OSS -DUSEAPI_OSS"
        else
            SYSSRC="s_midi_dummy.c"
        fi
    fi
	
	
    if test x$alsa = "xyes";
    then
        SYSSRC=$SYSSRC" s_audio_alsa.c s_audio_alsamm.c s_midi_alsa.c"
        CPPFLAGS=$CPPFLAGS" -DPA_USE_ALSA -DUSEAPI_ALSA"
        LDFLAGS=$LDFLAGS" -lasound"
    fi

    if test x$portaudio = "xyes";
    then
        CPPFLAGS=$CPPFLAGS" -DUSEAPI_PORTAUDIO -DHAVE_SYS_SOUNDCARD_H \
        -Wno-error \
        -I../portaudio/include -I../portaudio/src/common \
        -I../portaudio/src/os/unix/ \
        -I../portmidi/pm_common \
        -I../portmidi/pm_linux"
        SYSSRC="s_audio_pa.c \
        s_audio_paring.c     \
        ../portaudio/src/common/pa_allocation.c  \
        ../portaudio/src/common/pa_converters.c  \
        ../portaudio/src/common/pa_cpuload.c     \
        ../portaudio/src/common/pa_dither.c      \
        ../portaudio/src/common/pa_front.c       \
        ../portaudio/src/common/pa_process.c     \
        ../portaudio/src/common/pa_stream.c      \
        ../portaudio/src/common/pa_trace.c       \
        ../portaudio/src/common/pa_debugprint.c  \
        ../portaudio/src/common/pa_ringbuffer.c  \
        ../portaudio/src/os/unix/pa_unix_hostapis.c \
        ../portaudio/src/os/unix/pa_unix_util.c     \
        ../portaudio/src/hostapi/oss/pa_unix_oss.c "$SYSSRC
        if test x$alsa = "xyes";
        then
            SYSSRC="../portaudio/src/hostapi/alsa/pa_linux_alsa.c "$SYSSRC
            CPPFLAGS=$CPPFLAGS" -DPA_USE_ALSA"
        fi
        if test x$jack = "xyes";
        then
            SYSSRC="../portaudio/src/hostapi/jack/pa_jack.c "$SYSSRC
            CPPFLAGS=$CPPFLAGS" -DPA_USE_JACK"
        fi
    fi
    if test x$setuid = "xyes";
    then
        binarymode="-m4755"
    fi
    STRIPFLAG=-s
    if test x$USE_DEBUG_CFLAGS = "xyes";
    then
        MORECFLAGS=$MORECFLAGS" -g"
    else
        MORECFLAGS=$MORECFLAGS" -O6 -funroll-loops -fomit-frame-pointer"
    fi
    if test x$jack = "xyes";
    then
        LDFLAGS=$LDFLAGS" -lrt -ljack"
    fi
    if test x$jack = "xrun";
    then
        LDFLAGS=$LDFLAGS" -lrt -ljack"
    fi

    OSNUMBER=0

fi

if test `uname -s` = Darwin;
then
    LDFLAGS="-Wl -framework CoreAudio \
        -framework AudioUnit -framework AudioToolbox \
        -framework Carbon -framework CoreMIDI"
    EXT=pd_darwin
    CPPFLAGS="-DHAVE_LIBDL -DMACOSX -DHAVE_UNISTD_H -I/usr/X11R6/include \
        -I../portaudio/include -I../portaudio/src/common \
        -I../portaudio/src/os/mac_osx/ \
        -I../portmidi/pm_common -I../portmidi/pm_mac \
        -I../portmidi/porttime \
        -DUSEAPI_PORTAUDIO -DPA19 -DPA_USE_COREAUDIO -DNEWBUFFER"
    dnl for Mac OS X 10.3 (i.e. < 10.4)
    if test `uname -r` = 7.9.0;
    then
        fat="no"
    fi
    if test "x$fat" == "xyes";
    then
        MORECFLAGS="-isysroot /Developer/SDKs/MacOSX10.4u.sdk \
            -arch i386 -arch ppc -Wno-error"
        EXTERNTARGET=d_fat
        LDFLAGS=$LDFLAGS" -arch i386 -arch ppc"
    else
        MORECFLAGS="-DHAVE_ALLOCA_H=1 -DHAVE_MACHINE_ENDIAN_H=1 -DMACOSX3 -DPA_BIG_ENDIAN -Wno-error"
        EXTERNTARGET=d_ppc
    fi
    SYSSRC="s_midi_pm.c s_audio_pa.c \
        s_audio_paring.c     \
        ../portaudio/src/common/pa_allocation.c  \
        ../portaudio/src/common/pa_converters.c  \
        ../portaudio/src/common/pa_cpuload.c     \
        ../portaudio/src/common/pa_dither.c      \
        ../portaudio/src/common/pa_front.c       \
        ../portaudio/src/common/pa_process.c     \
        ../portaudio/src/common/pa_stream.c      \
        ../portaudio/src/common/pa_trace.c       \
        ../portaudio/src/common/pa_debugprint.c       \
        ../portaudio/src/common/pa_ringbuffer.c  \
        ../portaudio/src/os/unix/pa_unix_util.c     \
        ../portaudio/src/os/unix/pa_unix_hostapis.c   \
        ../portaudio/src/hostapi/coreaudio/pa_mac_core.c  \
        ../portaudio/src/hostapi/coreaudio/pa_mac_core_blocking.c  \
        ../portaudio/src/hostapi/coreaudio/pa_mac_core_utilities.c  \
        ../portmidi/pm_mac/pmmac.c         \
        ../portmidi/pm_mac/pmmacosxcm.c    \
        ../portmidi/pm_mac/finddefault.c    \
        ../portmidi/pm_mac/readbinaryplist.c    \
        ../portmidi/pm_common/pmutil.c     \
        ../portmidi/pm_common/portmidi.c   \
        ../portmidi/porttime/ptmacosx_cf.c"
    STRIPFLAG=""

    if test x$USE_DEBUG_CFLAGS = "xyes";
    then
        MORECFLAGS=$MORECFLAGS" -g"
    else
        MORECFLAGS=$MORECFLAGS" -O2"
    fi
    OSNUMBER=2
    if test x$jack = "xyes";
    then
        LDFLAGS=$LDFLAGS" -weak_framework Jackmp"
    fi
    if test x$jack = "xrun";
    then
        LDFLAGS=$LDFLAGS" -weak_framework Jackmp"
    fi
fi

# only Windows uses ASIO, for the rest, set to blank
ASIOSRC=

if test `uname -s` = MINGW32_NT-5.0;
then
    EXT=dll
    MORECFLAGS="-DUSEAPI_PORTAUDIO -DPA19 -DMSW -DPA_NO_DS -DPD_INTERNAL \
        -I../portaudio/include -I../portaudio/src/common \
        -I../portaudio/src/os/win/ \
        -mwindows -mms-bitfields "$MORECFLAGS
    PDLIB=$PDLIB" -lwsock32 -lwinmm -lole32 -lstdc++"

    SYSSRC="s_audio_pa.c s_audio_paring.c \
        s_audio_mmio.c s_midi_mmio.c \
        ../portaudio/src/common/pa_allocation.c  \
        ../portaudio/src/common/pa_converters.c  \
        ../portaudio/src/common/pa_cpuload.c     \
        ../portaudio/src/common/pa_dither.c      \
        ../portaudio/src/common/pa_front.c       \
        ../portaudio/src/common/pa_process.c     \
        ../portaudio/src/common/pa_stream.c      \
        ../portaudio/src/common/pa_trace.c       \
        ../portaudio/src/common/pa_debugprint.c       \
        ../portaudio/src/common/pa_ringbuffer.c  \
        ../portaudio/src/os/win/pa_win_util.c       \
        ../portaudio/src/os/win/pa_win_hostapis.c   \
        ../portaudio/src/os/win/pa_x86_plain_converters.c   \
        ../portaudio/src/hostapi/wmme/pa_win_wmme.c"
         ASIOSRC="../portaudio/src/hostapi/asio/iasiothiscallresolver.cpp \
         ../portaudio/src/hostapi/pa_asio/asio.cpp ../asio/asio.cpp \
         ../asio/asiodrivers.cpp ../asio/asiolist.cpp"
    STRIPFLAG="--strip-unneeded"
fi

# support for jack, on either linux or darwin:

if test x$jack = "xyes";
then
    MORECFLAGS=$MORECFLAGS" -DUSEAPI_JACK"
    SYSSRC=$SYSSRC" s_audio_jack.c"
fi
if test x$jack = "xrun";
then
    MORECFLAGS=$MORECFLAGS" -DUSEAPI_JACK -DJACK_XRUN"
    SYSSRC=$SYSSRC" s_audio_jack.c"
fi

if test x$fftw = "xyes";
then
    SYSSRC=$SYSSRC" d_fft_fftw.c d_fftroutine.c"
    LDFLAGS=$LDFLAGS" -lfftw3"
else
    SYSSRC=$SYSSRC" d_fft_mayer.c d_fftroutine.c"
fi

## JMZ{
##  this does not do very much, but i guess it is a good idea to use it...
AC_SYS_LARGEFILE
## test, whether we can use _LARGEFILE64_SOURCE to 
## enable large-file reading via "open64" (and lseek64");
if test "x$enable_largefile" != "xno"; then
## the following should run on autoconf>2.54 (which pd is not using)
dnl  AC_LINK_IFELSE ( [AC_LANG_PROGRAM([[
dnl                          #define _LARGEFILE64_SOURCE
dnl                          #include <sys/types.h>
dnl                          #include <sys/stat.h>
dnl                          #include <fcntl.h>
dnl                      ]],[[ open64(0,0); ]])], 
dnl                   [ MORECFLAGS=$MORECFLAGS" -D_LARGEFILE64_SOURCE" ], )
## and this is for autoconf<2.50
AC_TRY_LINK( [
                                        #define _LARGEFILE64_SOURCE
                                        #include <sys/types.h>
                                        #include <sys/stat.h>
                                        #include <fcntl.h>
                                       ],[ open64(0,0); ],
                [ MORECFLAGS=$MORECFLAGS" -D_LARGEFILE64_SOURCE" ], )
fi
## }JMZ: end of large-file support section

if test -d ../obj
then echo -n
else mkdir ../obj
fi

if test -d ../obj/tinyfd
then echo -n
else mkdir ../obj/tinyfd
fi

if test -d ../bin
then echo -n
else mkdir ../bin
fi

AST_EXT_LIB_CHECK([SINF],   [m], [sinf])
AST_EXT_LIB_CHECK([COSF],   [m], [cosf])
AST_EXT_LIB_CHECK([ATANF],  [m], [atanf])
AST_EXT_LIB_CHECK([ATAN2F], [m], [atan2f])
AST_EXT_LIB_CHECK([SQRTF],  [m], [sqrtf])
AST_EXT_LIB_CHECK([LOGF],   [m], [logf])
AST_EXT_LIB_CHECK([EXPF],   [m], [expf])
AST_EXT_LIB_CHECK([FABSF],  [m], [fabsf])
AST_EXT_LIB_CHECK([POWF],   [m], [powf])

AC_OUTPUT(makefile)

