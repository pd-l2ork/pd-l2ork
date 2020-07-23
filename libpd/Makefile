UNAME = $(shell uname)
SOLIB_PREFIX = lib
LIBPD_IMPLIB =
LIBPD_DEF =

ifeq ($(UNAME), Darwin)  # Mac
  SOLIB_EXT = dylib
  PDNATIVE_SOLIB_EXT = jnilib
  PDNATIVE_PLATFORM = mac
  PDNATIVE_ARCH =
  PLATFORM_CFLAGS = -arch x86_64 -DHAVE_LIBDL \
    -I/System/Library/Frameworks/JavaVM.framework/Headers
  LDFLAGS = -arch x86_64 -dynamiclib -ldl -Wl,-no_compact_unwind
  CSHARP_LDFLAGS = $(LDFLAGS)
  JAVA_LDFLAGS = -framework JavaVM $(LDFLAGS)
  FAT_LIB := $(shell expr `sw_vers -productVersion | cut -f2 -d.` \<= 10.13)
  ifeq ($(FAT_LIB), 1) # macOS 10.14+ does not build i386
    PLATFORM_CFLAGS += -arch i386
    LDFLAGS += -arch i386
  endif
else
  ifeq ($(OS), Windows_NT)  # Windows, use Mingw
    CC ?= gcc
    SOLIB_EXT = dll
    SOLIB_PREFIX =
    LIBPD_IMPLIB = libs/libpd.lib
    LIBPD_DEF = libs/libpd.def
    PDNATIVE_PLATFORM = windows
    PDNATIVE_ARCH = $(shell $(CC) -dumpmachine | sed -e 's,-.*,,' -e 's,i[3456]86,x86,' -e 's,amd64,x86_64,')
    PLATFORM_CFLAGS = -DWINVER=0x502 -DWIN32 -D_WIN32 -DPD_INTERNAL \
      -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/win32"
    MINGW_LDFLAGS = -shared -Wl,--export-all-symbols -lws2_32 -lkernel32 -static-libgcc
    LDFLAGS = $(MINGW_LDFLAGS) -Wl,--output-def=$(LIBPD_DEF) \
      -Wl,--out-implib=$(LIBPD_IMPLIB)
    CSHARP_LDFLAGS = $(MINGW_LDFLAGS) -Wl,--output-def=libs/libpdcsharp.def \
      -Wl,--out-implib=libs/libpdcsharp.lib
    JAVA_LDFLAGS = $(MINGW_LDFLAGS) -Wl,--kill-at
  else  # Linux or *BSD
    SOLIB_EXT = so
    PDNATIVE_ARCH = $(shell $(CC) -dumpmachine | sed -e 's,-.*,,' -e 's,i[3456]86,x86,' -e 's,amd64,x86_64,')
    PLATFORM_CFLAGS = -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -fPIC
    LDFLAGS = -shared -Wl,-Bsymbolic
    ifeq ($(UNAME), Linux)
      PDNATIVE_PLATFORM = linux
      JAVA_HOME ?= /usr/lib/jvm/default-java
      PLATFORM_CFLAGS += -I"$(JAVA_HOME)/include/linux" -DHAVE_LIBDL
      LDFLAGS += -ldl
    else ifeq ($(UNAME), FreeBSD)
      PDNATIVE_PLATFORM = FreeBSD
      JAVA_HOME ?= /usr/local/openjdk8
      PLATFORM_CFLAGS += -I"$(JAVA_HOME)/include/"
    endif
    CSHARP_LDFLAGS = $(LDFLAGS)
    JAVA_LDFLAGS = $(LDFLAGS)
  endif
endif

PDNATIVE_SOLIB_EXT ?= $(SOLIB_EXT)

PD_FILES = \
    ../pd/src/d_arithmetic.c ../pd/src/d_array.c ../pd/src/d_ctl.c \
    ../pd/src/d_dac.c ../pd/src/d_delay.c ../pd/src/d_fft.c \
    ../pd/src/d_fft_fftsg.c ../pd/src/d_fftroutine.c \
    ../pd/src/d_filter.c ../pd/src/d_global.c ../pd/src/d_math.c \
    ../pd/src/d_misc.c ../pd/src/d_osc.c ../pd/src/d_resample.c \
    ../pd/src/d_soundfile.c ../pd/src/d_ugen.c \
    ../pd/src/g_all_guis.c ../pd/src/g_array.c ../pd/src/g_bang.c \
    ../pd/src/g_canvas.c ../pd/src/g_clone.c ../pd/src/g_editor.c \
    ../pd/src/g_graph.c ../pd/src/g_guiconnect.c \
    ../pd/src/g_io.c ../pd/src/g_magicglass.c ../pd/src/g_mycanvas.c \
    ../pd/src/g_numbox.c ../pd/src/g_radio.c ../pd/src/g_readwrite.c \
    ../pd/src/g_rtext.c ../pd/src/g_scalar.c ../pd/src/g_slider.c \
    ../pd/src/g_template.c ../pd/src/g_text.c ../pd/src/g_toggle.c \
    ../pd/src/g_traversal.c ../pd/src/g_undo.c ../pd/src/g_vumeter.c \
    ../pd/src/import.c ../pd/src/m_atom.c ../pd/src/m_binbuf.c \
    ../pd/src/m_class.c ../pd/src/m_conf.c ../pd/src/m_glob.c \
    ../pd/src/m_memory.c ../pd/src/m_obj.c ../pd/src/m_pd.c \
    ../pd/src/m_sched.c ../pd/src/s_audio.c ../pd/src/s_audio_dummy.c \
    ../pd/src/s_inter.c ../pd/src/s_loader.c ../pd/src/s_main.c \
    ../pd/src/s_path.c ../pd/src/s_print.c ../pd/src/s_utf8.c \
    ../pd/src/x_acoustics.c ../pd/src/x_arithmetic.c ../pd/src/x_array.c \
    ../pd/src/x_connective.c ../pd/src/x_gui.c ../pd/src/x_interface.c \
    ../pd/src/x_list.c ../pd/src/x_midi.c ../pd/src/x_misc.c \
    ../pd/src/x_net.c ../pd/src/x_preset.c ../pd/src/x_scalar.c \
    ../pd/src/x_text.c ../pd/src/x_time.c ../pd/src/x_vexp.c \
    ../pd/src/x_vexp_fun.c ../pd/src/x_vexp_if.c \
    libpd_wrapper/s_libpdmidi.c libpd_wrapper/x_libpdreceive.c \
    libpd_wrapper/z_hooks.c libpd_wrapper/z_libpd.c

ADAPTIVE_PATH = ../externals/grh/adaptive/src
PD_EXTRA_FILES_ADAPTIVE = $(wildcard $(ADAPTIVE_PATH)/*.c)

BSAYLOR_PATH = ../externals/bsaylor
PD_EXTRA_FILES_BSAYLOR = $(wildcard $(BSAYLOR_PATH)/*.c)

CREB_PATH = ../externals/creb/modules
PD_EXTRA_FILES_CREB = $(wildcard $(CREB_PATH)/*.c)

CXC_PATH = ../externals/cxc
PD_EXTRA_FILES_CXC = $(wildcard $(CXC_PATH)/*.c)

DISIS_PATH = ../externals/disis
PD_EXTRA_FILES_DISIS = $(wildcard $(DISIS_PATH)/*.c)

EKEXT_PATH = ../externals/ekext
PD_EXTRA_FILES_EKEXT = $(wildcard $(EKEXT_PATH)/*.c)

EXT13_PATH = ../externals/ext13
PD_EXTRA_FILES_EXT13 = $(wildcard $(EXT13_PATH)/*.c)

FFTEASE_PATH = ../externals/fftease
PD_EXTRA_FILES_FFTEASE = $(wildcard $(FFTEASE_PATH)/*.c)

GGEE1_PATH = ../externals/ggee/control
PD_EXTRA_FILES_GGEE1 = $(wildcard $(GGEE1_PATH)/*.c)

GGEE2_PATH = ../externals/ggee/experimental
PD_EXTRA_FILES_GGEE2 = $(wildcard $(GGEE2_PATH)/*.c)

GGEE3_PATH = ../externals/ggee/filters
PD_EXTRA_FILES_GGEE3 = $(wildcard $(GGEE3_PATH)/*.c)

GGEE4_PATH = ../externals/ggee/signal
PD_EXTRA_FILES_GGEE4 = $(wildcard $(GGEE4_PATH)/*.c)

HCS_PATH = ../externals/hcs
PD_EXTRA_FILES_HCS = $(wildcard $(HCS_PATH)/*.c)

IEM1_PATH = ../externals/iem/iem_adaptfilt/src
PD_EXTRA_FILES_IEM1 = $(wildcard $(IEM1_PATH)/*.c)

IEM2_PATH = ../externals/iem/iem_ambi/src
PD_EXTRA_FILES_IEM2 = $(wildcard $(IEM2_PATH)/*.c)

IEM3_PATH = ../externals/iem/iem_bin_ambi/src
PD_EXTRA_FILES_IEM3 = $(wildcard $(IEM3_PATH)/*.c)

IEM4_PATH = ../externals/iem/iem_delay/src
PD_EXTRA_FILES_IEM4 = $(wildcard $(IEM4_PATH)/*.c)

IEM5_PATH = ../externals/iem/iem_roomsim/src
PD_EXTRA_FILES_IEM5 = $(wildcard $(IEM5_PATH)/*.c)

IEM6_PATH = ../externals/iem/iem_spec2/src
PD_EXTRA_FILES_IEM6 = $(wildcard $(IEM6_PATH)/*.c)

IEM7_PATH = ../externals/iem/iem_tab/src
PD_EXTRA_FILES_IEM7 = $(wildcard $(IEM7_PATH)/*.c)

IEM8_PATH = ../externals/iem/iemguts/src
PD_EXTRA_FILES_IEM8 = $(wildcard $(IEM8_PATH)/*.c)

IEM16_PATH = ../externals/iem16/src
PD_EXTRA_FILES_IEM16 = $(wildcard $(IEM16_PATH)/*.c)

IEMLIB1_PATH = ../externals/iemlib/iemlib1/src
PD_EXTRA_FILES_IEMLIB1 = $(wildcard $(IEMLIB1_PATH)/*.c)

IEMLIB2_PATH = ../externals/iemlib/iemlib2/src
PD_EXTRA_FILES_IEMLIB2 = $(wildcard $(IEMLIB2_PATH)/*.c)

MARKEX_PATH = ../externals/markex
PD_EXTRA_FILES_MARKEX = $(wildcard $(MARKEX_PATH)/*.c)

MAXLIB_PATH = ../externals/maxlib
PD_EXTRA_FILES_MAXLIB = $(wildcard $(MAXLIB_PATH)/*.c)

MJLIB_PATH = ../externals/mjlib
PD_EXTRA_FILES_MJLIB = $(wildcard $(MJLIB_PATH)/*.c)

MOONLIB_PATH = ../externals/moonlib
PD_EXTRA_FILES_MOONLIB = $(wildcard $(MOONLIB_PATH)/*.c)

MOTEX_PATH = ../externals/motex
PD_EXTRA_FILES_MOTEX = $(wildcard $(MOTEX_PATH)/*.c)

MRPEACH_PATH1 = ../externals/mrpeach/binfile
PD_EXTRA_FILES_MRPEACH1 = $(wildcard $(MRPEACH_PATH1)/*.c)

MRPEACH_PATH2 = ../externals/mrpeach/cmos
PD_EXTRA_FILES_MRPEACH2 = $(wildcard $(MRPEACH_PATH2)/*.c)

MRPEACH_PATH3 = ../externals/mrpeach/flist2tab
PD_EXTRA_FILES_MRPEACH3 = $(wildcard $(MRPEACH_PATH3)/*.c)

MRPEACH_PATH4 = ../externals/mrpeach/life2x
PD_EXTRA_FILES_MRPEACH4 = $(wildcard $(MRPEACH_PATH4)/*.c)

MRPEACH_PATH5 = ../externals/mrpeach/midifile
PD_EXTRA_FILES_MRPEACH5 = $(wildcard $(MRPEACH_PATH5)/*.c)

MRPEACH_PATH6 = ../externals/mrpeach/net
PD_EXTRA_FILES_MRPEACH6 = $(wildcard $(MRPEACH_PATH6)/*.c)

MRPEACH_PATH7 = ../externals/mrpeach/op~
PD_EXTRA_FILES_MRPEACH7 = $(wildcard $(MRPEACH_PATH7)/*.c)

MRPEACH_PATH8 = ../externals/mrpeach/osc
PD_EXTRA_FILES_MRPEACH8 = $(wildcard $(MRPEACH_PATH8)/*.c)

MRPEACH_PATH9 = ../externals/mrpeach/rc~
PD_EXTRA_FILES_MRPEACH9 = $(wildcard $(MRPEACH_PATH9)/*.c)

MRPEACH_PATH10 = ../externals/mrpeach/rcosc~
PD_EXTRA_FILES_MRPEACH10 = $(wildcard $(MRPEACH_PATH10)/*.c)

MRPEACH_PATH11 = ../externals/mrpeach/rojo~
PD_EXTRA_FILES_MRPEACH11 = $(wildcard $(MRPEACH_PATH11)/*.c)

MRPEACH_PATH12 = ../externals/mrpeach/runningmean
PD_EXTRA_FILES_MRPEACH12 = $(wildcard $(MRPEACH_PATH12)/*.c)

MRPEACH_PATH13 = ../externals/mrpeach/serializer
PD_EXTRA_FILES_MRPEACH13 = $(wildcard $(MRPEACH_PATH13)/*.c)

MRPEACH_PATH14 = ../externals/mrpeach/slipdec
PD_EXTRA_FILES_MRPEACH14 = $(wildcard $(MRPEACH_PATH14)/*.c)

MRPEACH_PATH15 = ../externals/mrpeach/slipenc
PD_EXTRA_FILES_MRPEACH15 = $(wildcard $(MRPEACH_PATH15)/*.c)

MRPEACH_PATH16 = ../externals/mrpeach/sqosc~
PD_EXTRA_FILES_MRPEACH16 = $(wildcard $(MRPEACH_PATH16)/*.c)

MRPEACH_PATH17 = ../externals/mrpeach/str
PD_EXTRA_FILES_MRPEACH17 = $(wildcard $(MRPEACH_PATH17)/*.c)

MRPEACH_PATH18 = ../externals/mrpeach/tab2flist
PD_EXTRA_FILES_MRPEACH18 = $(wildcard $(MRPEACH_PATH18)/*.c)

MRPEACH_PATH19 = ../externals/mrpeach/tabfind
PD_EXTRA_FILES_MRPEACH19 = $(wildcard $(MRPEACH_PATH19)/*.c)

MRPEACH_PATH20 = ../externals/mrpeach/which
PD_EXTRA_FILES_MRPEACH20 = $(wildcard $(MRPEACH_PATH20)/*.c)

MRPEACH_PATH21 = ../externals/mrpeach/xbee
PD_EXTRA_FILES_MRPEACH21 = $(wildcard $(MRPEACH_PATH21)/*.c)

PDDP_PATH = ../externals/pddp
PD_EXTRA_FILES_PDDP = $(wildcard $(PDDP_PATH)/*.c)

PLUGIN_PATH = ../externals/plugin~
PD_EXTRA_FILES_PLUGIN = $(wildcard $(PLUGIN_PATH)/*.c)

SIGPACK_PATH = ../externals/sigpack
PD_EXTRA_FILES_SIGPACK = $(wildcard $(SIGPACK_PATH)/*.c)

SMLIB_PATH = ../externals/smlib
PD_EXTRA_FILES_SMLIB = $(wildcard $(SMLIB_PATH)/*.c)

TOF_PATH = ../externals/tof/src
PD_EXTRA_FILES_TOF = $(wildcard $(TOF_PATH)/*.c)

VBAP_PATH = ../externals/vbap
PD_EXTRA_FILES_VBAP = $(wildcard $(VBAP_PATH)/*.c)

WINDOWING_PATH = ../externals/windowing
PD_EXTRA_FILES_WINDOWING = $(wildcard $(WINDOWING_PATH)/*.c)
	
ZEXY_PATH = ../externals/zexy/src
PD_EXTRA_FILES_ZEXY = $(wildcard $(ZEXY_PATH)/*.c)

PD_EXTRA_FILES = \
    ../pd/extra/bob~/bob~.c ../pd/extra/bonk~/bonk~.c \
    ../pd/extra/choice/choice.c \
    ../pd/extra/fiddle~/fiddle~.c ../pd/extra/loop~/loop~.c \
    ../pd/extra/lrshift~/lrshift~.c ../pd/extra/pique/pique.c \
    ../pd/extra/sigmund~/sigmund~.c ../pd/extra/stdout/stdout.c \
    ../externals/arraysize/arraysize.c \
    ../externals/autotune/autotune~.c \
    ../externals/bassemu~/bassemu~.c \
    $(PD_EXTRA_FILES_ADAPTIVE) \
    $(PD_EXTRA_FILES_BSAYLOR) \
    ../externals/iem/comport/comport/comport.c \
    $(PD_EXTRA_FILES_CREB) \
    $(PD_EXTRA_FILES_CXC) \
    $(PD_EXTRA_FILES_DISIS) \
    ../externals/earplug~/earplug~.c \
    $(PD_EXTRA_FILES_EKEXT) \
    $(PD_EXTRA_FILES_EXT13) \
    $(PD_EXTRA_FILES_FFTEASE) \
    ../externals/footils/knob/knob.c \
    ../externals/freeverb~/freeverb~.c \
    $(PD_EXTRA_FILES_GGEE1) \
    $(PD_EXTRA_FILES_GGEE2) \
    $(PD_EXTRA_FILES_GGEE3) \
    $(PD_EXTRA_FILES_GGEE4) \
    $(PD_EXTRA_FILES_HCS) \
    $(PD_EXTRA_FILES_IEM1) \
    $(PD_EXTRA_FILES_IEM2) \
    $(PD_EXTRA_FILES_IEM3) \
    $(PD_EXTRA_FILES_IEM4) \
    $(PD_EXTRA_FILES_IEM5) \
    $(PD_EXTRA_FILES_IEM6) \
    $(PD_EXTRA_FILES_IEM7) \
    $(PD_EXTRA_FILES_IEM8) \
    $(PD_EXTRA_FILES_IEM16) \
    ${PD_EXTRA_FILES_IEMLIB1} \
    ${PD_EXTRA_FILES_IEMLIB2} \
    ../externals/jasch_lib/detox/detox.c \
    ../externals/jasch_lib/memchr/memchr.c \
    ../externals/jasch_lib/strchr/strchr.c \
    ../externals/jasch_lib/strcut/strcut.c \
    ../externals/jasch_lib/strlen/strlen.c \
    ../externals/jasch_lib/strtok/strtok.c \
    ../externals/jasch_lib/underscore/_.c \
    ../externals/loaders/libdir/libdir.c \
    $(PD_EXTRA_FILES_MARKEX) \
    $(PD_EXTRA_FILES_MAXLIB) \
    $(PD_EXTRA_FILES_MJLIB) \
    $(PD_EXTRA_FILES_MOONLIB) \
    $(PD_EXTRA_FILES_MOTEX) \
    $(PD_EXTRA_FILES_MRPEACH1) \
    $(PD_EXTRA_FILES_MRPEACH2) \
    $(PD_EXTRA_FILES_MRPEACH3) \
    $(PD_EXTRA_FILES_MRPEACH4) \
    $(PD_EXTRA_FILES_MRPEACH5) \
    $(PD_EXTRA_FILES_MRPEACH6) \
    $(PD_EXTRA_FILES_MRPEACH7) \
    $(PD_EXTRA_FILES_MRPEACH8) \
    $(PD_EXTRA_FILES_MRPEACH9) \
    $(PD_EXTRA_FILES_MRPEACH10) \
    $(PD_EXTRA_FILES_MRPEACH11) \
    $(PD_EXTRA_FILES_MRPEACH12) \
    $(PD_EXTRA_FILES_MRPEACH13) \
    $(PD_EXTRA_FILES_MRPEACH14) \
    $(PD_EXTRA_FILES_MRPEACH15) \
    $(PD_EXTRA_FILES_MRPEACH16) \
    $(PD_EXTRA_FILES_MRPEACH17) \
    $(PD_EXTRA_FILES_MRPEACH18) \
    $(PD_EXTRA_FILES_MRPEACH19) \
    $(PD_EXTRA_FILES_MRPEACH20) \
    $(PD_EXTRA_FILES_MRPEACH21) \
    $(PD_EXTRA_FILES_PDDP) \
    $(PD_EXTRA_FILES_PLUGIN) \
    $(PD_EXTRA_FILES_SIGPACK) \
    $(PD_EXTRA_FILES_SMLIB) \
    $(PD_EXTRA_FILES_TOF) \
    $(PD_EXTRA_FILES_VBAP) \
    $(PD_EXTRA_FILES_WINDOWING) \
    $(PD_EXTRA_FILES_ZEXY)

LIBPD_UTILS = \
    libpd_wrapper/util/z_print_util.c \
    libpd_wrapper/util/z_queued.c \
    libpd_wrapper/util/ringbuffer.c

PDJAVA_JAR_CLASSES = \
    java/org/puredata/core/PdBase.java \
    java/org/puredata/core/PdBaseloader.java \
    java/org/puredata/core/NativeLoader.java \
    java/org/puredata/core/PdListener.java \
    java/org/puredata/core/PdMidiListener.java \
    java/org/puredata/core/PdMidiReceiver.java \
    java/org/puredata/core/PdReceiver.java \
    java/org/puredata/core/utils/IoUtils.java \
    java/org/puredata/core/utils/PdDispatcher.java

# additional Java source jar files
PDJAVA_SRC_FILES = \
	.classpath \
	.project

JNI_SOUND = jni/z_jni_plain.c

# conditional libpd_wrapper/util compilation
UTIL_FILES = $(LIBPD_UTILS)
ifeq ($(UTIL), false)
    UTIL_FILES =
endif

# conditional ../pd/extra externals compilation
EXTRA_FILES = $(PD_EXTRA_FILES)
EXTRA_CFLAGS = -DLIBPD_EXTRA
ifeq ($(EXTRA), false)
    EXTRA_FILES =
    EXTRA_CFLAGS =
endif

# conditional multi-instance support
MULTI_CFLAGS =
ifeq ($(MULTI), true)
    MULTI_CFLAGS = -DPDINSTANCE -DPDTHREADS
endif

# conditional optimizations or debug settings
OPT_CFLAGS = -ffast-math -funroll-loops -fomit-frame-pointer -O3
ifeq ($(DEBUG), true)
    OPT_CFLAGS = -g -O0
endif

# conditional to set numeric locale to default "C"
LOCALE_CFLAGS =
ifeq ($(SETLOCALE), false)
    LOCALE_CFLAGS = -DLIBPD_NO_NUMERIC
endif

# portaudio backend?
ifeq ($(PORTAUDIO), true)
    JNI_SOUND = jni/z_jni_pa.c
    JAVA_LDFLAGS := $(JAVA_LDFLAGS) -lportaudio
    ifeq ($(UNAME), Darwin)  # Mac
        JAVA_LDFLAGS := $(JAVA_LDFLAGS) \
                        -framework CoreAudio -framework AudioToolbox \
                        -framework AudioUnit -framework CoreServices
    endif
endif

# object files which are somehow generated but not from sources listed above,
# there is probably a better fix but this works for now
PD_EXTRA_OBJS = \
    ../pd/src/d_fft_fft_fftsg.o ../pd/src/d_fft_fftw.o \
    ../pd/src/d_fft_fftsg_h.o ../pd/src/x_qlist.o

# default install location
prefix ?= /usr/local
includedir ?= $(prefix)/include
libdir ?= $(prefix)/lib

JNI_FILE = libpd_wrapper/util/ringbuffer.c libpd_wrapper/util/z_queued.c $(JNI_SOUND)
JNIH_FILE = jni/z_jni.h
JAVA_BASE = java/org/puredata/core/PdBase.java
ifeq ($(OS), Windows_NT)
	LIBPD = libs/pd.$(SOLIB_EXT)
else
	LIBPD = libs/libpd.$(SOLIB_EXT)
endif
PDCSHARP = libs/libpdcsharp.$(SOLIB_EXT)

PDJAVA_BUILD = java-build
PDJAVA_DIR = $(PDJAVA_BUILD)/org/puredata/core/natives/$(PDNATIVE_PLATFORM)/$(PDNATIVE_ARCH)
PDJAVA_NATIVE = $(PDJAVA_DIR)/$(SOLIB_PREFIX)pdnative.$(PDNATIVE_SOLIB_EXT)
PDJAVA_JAR = libs/libpd.jar
PDJAVA_SRC = libs/libpd-sources.jar
PDJAVA_DOC = javadoc

CFLAGS = -DPD -DHAVE_UNISTD_H -DUSEAPI_DUMMY \
         -I./libpd_wrapper -I./libpd_wrapper/util \
         -I./../pd/src \
         $(PLATFORM_CFLAGS) \
         $(OPT_CFLAGS) $(EXTRA_CFLAGS) $(MULTI_CFLAGS) $(LOCALE_CFLAGS) \
         $(ADDITIONAL_CFLAGS)
LDFLAGS += $(ADDITIONAL_LDFLAGS)
CSHARP_LDFLAGS += $(ADDITIONAL_LDFLAGS)
JAVA_LDFLAGS += $(ADDITIONAL_LDFLAGS)

.PHONY: libpd csharplib cpplib javalib javadoc javasrc install uninstall clean clobber

libpd: $(LIBPD)

$(LIBPD): ${PD_FILES:.c=.o} ${UTIL_FILES:.c=.o} ${EXTRA_FILES:.c=.o}
	$(CC) -o $(LIBPD) $^ $(LDFLAGS) -lm -lpthread

javalib: $(JNIH_FILE) $(PDJAVA_JAR)

$(JNIH_FILE): $(JAVA_BASE)
	javac -classpath java $^
	javah -o $@ -classpath java org.puredata.core.PdBase

$(PDJAVA_NATIVE): ${PD_FILES:.c=.o} ${LIBPD_UTILS:.c=.o} ${EXTRA_FILES:.c=.o} ${JNI_FILE:.c=.o}
	mkdir -p $(PDJAVA_DIR)
	$(CC) -o $(PDJAVA_NATIVE) $^ -lm -lpthread $(JAVA_LDFLAGS)
	cp $(PDJAVA_NATIVE) libs/

$(PDJAVA_JAR): $(PDJAVA_NATIVE) $(PDJAVA_JAR_CLASSES)
	javac -classpath java -d $(PDJAVA_BUILD) $(PDJAVA_JAR_CLASSES)
	jar -cvf $(PDJAVA_JAR) -C $(PDJAVA_BUILD) org/puredata/

javadoc: $(PDJAVA_JAR_CLASSES)
	javadoc -d $(PDJAVA_DOC) -sourcepath java org.puredata.core

javasrc: $(PDJAVA_SRC)

$(PDJAVA_SRC): $(PDJAVA_JAR_FILES)
	jar -cvf $(PDJAVA_SRC) $(PDJAVA_SRC_FILES) -C java org

csharplib: $(PDCSHARP)

$(PDCSHARP): ${PD_FILES:.c=.o} ${EXTRA_FILES:.c=.o}
	$(CC) -o $(PDCSHARP) $^ $(CSHARP_LDFLAGS) -lm -lpthread

clean:
	rm -f ${PD_FILES:.c=.o} ${PD_EXTRA_OBJS} ${JNI_FILE:.c=.o}
	rm -f ${UTIL_FILES:.c=.o} ${PD_EXTRA_FILES:.c=.o}

clobber: clean
	rm -f $(LIBPD) $(LIBPD_IMPLIB) $(LIBPD_DEF)
	rm -f $(PDCSHARP) ${PDCSHARP:.$(SOLIB_EXT)=.lib} ${PDCSHARP:.$(SOLIB_EXT)=.def}
	rm -f $(PDJAVA_JAR) $(PDJAVA_NATIVE) libs/`basename $(PDJAVA_NATIVE)`
	rm -rf $(PDJAVA_BUILD) $(PDJAVA_SRC) $(PDJAVA_DOC)

# optional install headers & libs based on build type: UTIL=true and/or windows
install:
	install -d $(includedir)/libpd
	install -m 644 libpd_wrapper/z_libpd.h $(includedir)/libpd
	install -m 644 ../pd/src/m_pd.h $(includedir)/libpd
	if [ -e libpd_wrapper/util/z_queued.o ]; then \
	    install -d $(includedir)/libpd/util; \
	    install -m 644 libpd_wrapper/util/z_print_util.h $(includedir)/libpd/util; \
	    install -m 644 libpd_wrapper/util/z_queued.h $(includedir)/libpd/util; \
		install -m 644 cpp/*hpp $(includedir)/libpd; \
	fi
	install -d $(libdir)
	install -m 755 $(LIBPD) $(libdir)
	if [ -e '$(LIBPD_IMPLIB)' ]; then install -m 755 $(LIBPD_IMPLIB) $(libdir); fi
	if [ -e '$(LIBPD_DEF)' ]; then install -m 755 $(LIBPD_DEF) $(libdir); fi

uninstall:
	rm -rf $(includedir)/libpd
	rm -f $(libdir)/`basename $(LIBPD)` $(libdir)/`basename $(LIBPD_IMPLIB)` $(libdir)/`basename $(LIBPD_DEF)`
