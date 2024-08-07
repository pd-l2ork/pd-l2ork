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
    ../pd/src/s_midi.c ../pd/src/s_midi_dummy.c \
    ../pd/src/s_path.c ../pd/src/s_print.c ../pd/src/s_utf8.c \
    ../pd/src/x_acoustics.c ../pd/src/x_arithmetic.c ../pd/src/x_array.c \
    ../pd/src/x_connective.c ../pd/src/x_gui.c ../pd/src/tinyfd/tinyfiledialogs.c ../pd/src/x_interface.c \
    ../pd/src/x_list.c ../pd/src/x_midi.c ../pd/src/x_misc.c \
    ../pd/src/x_net.c ../pd/src/x_preset.c ../pd/src/x_scalar.c \
    ../pd/src/x_text.c ../pd/src/x_time.c ../pd/src/x_vexp.c \
    ../pd/src/x_vexp_fun.c ../pd/src/x_vexp_if.c ../pd/src/x_file.c \
    libpd_wrapper/s_libpdmidi.c libpd_wrapper/x_libpdreceive.c \
    libpd_wrapper/z_hooks.c libpd_wrapper/z_libpd.c

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
         $(OPT_CFLAGS) $(MULTI_CFLAGS) $(LOCALE_CFLAGS) \
         $(ADDITIONAL_CFLAGS)
LDFLAGS += $(ADDITIONAL_LDFLAGS)
CSHARP_LDFLAGS += $(ADDITIONAL_LDFLAGS)
JAVA_LDFLAGS += $(ADDITIONAL_LDFLAGS)

.PHONY: libpd csharplib cpplib javalib javadoc javasrc install uninstall clean clobber

libpd: $(LIBPD)

$(LIBPD): ${PD_FILES:.c=.o} ${UTIL_FILES:.c=.o}
	$(CC) -o $(LIBPD) $^ $(LDFLAGS) -lm -lpthread

javalib: $(JNIH_FILE) $(PDJAVA_JAR)

$(JNIH_FILE): $(JAVA_BASE)
	javac -classpath java $^
	javah -o $@ -classpath java org.puredata.core.PdBase

$(PDJAVA_NATIVE): ${PD_FILES:.c=.o} ${LIBPD_UTILS:.c=.o} ${JNI_FILE:.c=.o}
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

$(PDCSHARP): ${PD_FILES:.c=.o}
	$(CC) -o $(PDCSHARP) $^ $(CSHARP_LDFLAGS) -lm -lpthread

clean:
	rm -f ${PD_FILES:.c=.o} ${PD_EXTRA_OBJS} ${JNI_FILE:.c=.o}
	rm -rf build libs

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
