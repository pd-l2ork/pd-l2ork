
#==============================================================================#
#
# Centralized build system for "externals".  
#
# see README for instructions  <hans@at.or.at>
# http://puredata.org/docs/developer/build
#
#==============================================================================#

CWD := $(shell pwd)

# these are setup to be overridden by the packages/Makefile
cvs_root_dir := $(shell cd $(CWD)/.. && pwd)
DESTDIR = $(CWD)/build/
BUILDLAYOUT_DIR = $(cvs_root_dir)/packages

ifeq ($(macos_target),)
macos_target = 10.9
endif

# turn on weak linking and dlopen support
export MACOSX_DEPLOYMENT_TARGET = $(macos_target)

# default target
default: all

.SUFFIXES: .$(EXTENSION) .$(SHARED_LIB)


include $(BUILDLAYOUT_DIR)/Makefile.buildlayout


# these are sent to all of the various Makefiles so that they all copy their
# output to the same directory tree
DEST_PATHS = BUILDLAYOUT_DIR=$(BUILDLAYOUT_DIR) \
				cvs_root_dir=$(cvs_root_dir) \
				DESTDIR=$(DESTDIR) \
				prefix=$(prefix)

#==============================================================================#
#
# COMPILE TARGETS
#
#==============================================================================#

# this variable is to support old "win" directories, rather than "windows"
BUILDSRC_OS_NAME = $(OS_NAME)

# ico@vt.edu 2025-04-29: for github runner we add CFLAGS_GITHUB_RUNNER=-std=gnu17
# the full command is:
# cd l2ork_addons && CFLAGS_GITHUB_RUNNER=-std=gnu17 inst_dir="/usr" ./tar_em_up.sh -Tk
# see build.yml runner file on the github because after April 2025 github msys runner
# uses gcc 15 which has -std=gnu23. this breaks old code in the 3rd party libraries'
# old style prototype declarations. given this only affects MSYS2/MINGW environment
# it is handled through the runner make. below is by default disabled debug printout
# to make sure all this works as expected.
#$(info    >>>>>>>>>>> $(CFLAGS_GITHUB_RUNNER))
#exit -1

WARN_FLAGS = -Wall -W -Wno-unused-parameter -Wno-incompatible-pointer-types
CFLAGS = -DPD -D__USE_MINGW_ANSI_STDIO=1 -I$(pd_src)/src $(WARN_FLAGS) $(DEBUG_CFLAGS) $(CFLAGS_ADD) -I$(gem_src)/src
LDFLAGS = 
LIBS = -lm

# emscripten
ifeq ($(EMSCRIPTEN),yes)
  set_em_flags = CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)" CC="$(CC)" CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" ALL_CFLAGS="$(CFLAGS)" ALL_LDFLAGS="$(LDFLAGS)" ALL_LIBS="$(LIBS)" SHARED_LIB="" EXTENSION="$(EXTENSION)" STRIP="$(STRIP)"
  define if_emscripten
    $(if $(filter $(EMSCRIPTEN),yes),$1)
  endef
else
  set_em_flags =
endif


ifeq ($(OS_NAME),darwin)
# 10.4 Tiger
#  FAT_FLAGS = -arch ppc -arch ppc64 -arch i386
# 10.5 Leopard
#  FAT_FLAGS = -arch ppc -arch ppc7400 -arch ppc64 -arch i386 -arch x86_64
# Check whether we have Homebrew or MacPorts, prefer the former.
# Homebrew links software into $HOMEBREW_PREFIX. If this isn't defined, we
# check for MacPorts in /opt/local, and finally fall back to /usr/local.
optlocal := $(shell test -n "$$HOMEBREW_PREFIX" && test -d $$HOMEBREW_PREFIX && echo $$HOMEBREW_PREFIX || (test -d /opt/local && echo /opt/local) || echo /usr/local)
usrlocal = $(optlocal)
  CFLAGS += -I$(usrlocal)/include -I$(externals_src)/pdp/include -DMACOSX -DUNIX -Dunix -DDL_OPEN
  LDFLAGS += -bundle -bundle_loader $(pd_src)/bin/pd-l2ork -undefined dynamic_lookup -L$(usrlocal)/lib
  LIBS += -lc 
  DYLIB_LDFLAGS = -dynamiclib -undefined dynamic_lookup -read_only_relocs warning -L$(usrlocal)/lib
  STRIP = strip -x
endif
ifeq ($(OS_NAME),linux)
  CFLAGS += -I$(externals_src)/pdp/include -DUNIX -Dunix -DDL_OPEN -fPIC -O2
  LDFLAGS += -Wl,--export-dynamic  -shared -fPIC
  LIBS += -lc
  DYLIB_LDFLAGS = $(LDFLAGS)
  STRIP = strip --strip-unneeded -R .note -R .comment
  CPU = $(findstring aarch64, $(shell uname -m))
endif
ifeq ($(OS_NAME),windows)
  BUILDSRC_OS_NAME = win
  WINDOWS_HACKS = -DO_NONBLOCK=1
# These don't seem to be needed:
# -D'srand48(n)=srand((n))' \
#    -D'drand48()=((double)rand()/RAND_MAX)' -D'bzero(p,n)=memset(p,0,n)'
#	-D'PROT_READ=1' \
#	-D'MAP_PRIVATE=2' \
#	-D'O_NDELAY=O_NONBLOCK'
  CFLAGS += -mms-bitfields -DMSW -DNT $(WINDOWS_HACKS) $(CFLAGS_GITHUB_RUNNER)
  LDFLAGS += -s -shared
# all of these included libs are part of libc in UNIX platforms.  All except
# libregex are in DLLs, so they get stripped from the external's .dll binary
  LIBS += -L$(pd_src)/src -L$(pd_src)/bin -L$(pd_src)/obj -lpd \
    -lwsock32 -liphlpapi -lpthread -lkernel32 -luser32 -lgdi32 -lregex
  DYLIB_LDFLAGS = -shared
  STRIP = strip --strip-unneeded -R .note -R .comment
endif

CXXFLAGS = $(CFLAGS)

### C++ files
%.$(EXTENSION): %.cpp
%.$(EXTENSION): %.cc
	$(CXX) $(OPT_CFLAGS) $(CXXFLAGS) -o "$*.o" -c "$<"
	$(CXX) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o" $(LIBS)
	chmod a-x "$*.$(EXTENSION)"
#	$(STRIP) $*.$(EXTENSION)
#	rm -f -- $*.o

%.o: %.cpp
%.o: %.cc
	$(CXX) $(OPT_CFLAGS) $(CXXFLAGS) -o "$*.o" -c "$<"


### C files
%.o: %.c
	$(CC) $(OPT_CFLAGS) $(CFLAGS) -o "$*.o" -c "$*.c"

%.$(EXTENSION): %.o
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o"  $(LIBS) \
		`test -f $*.libs && cat $*.libs`	\
		`my_dylib=$(patsubst $(externals_src)/%,%,$(@D)); test -f $(@D)/lib$${my_dylib}.$(DYLIB_EXTENSION) && echo -L$(@D) -l$$my_dylib` \
		`my_obj=$(patsubst $(externals_src)/%,%,$(@D)); test -f $(@D)/shared/$${my_obj}.o && echo $(@D)/shared/$${my_obj}.o` \
		`test -f $(dir $*)../$(BUILDSRC_OS_NAME)/$(notdir $*).libs && \
			cat $(dir $*)../$(BUILDSRC_OS_NAME)/$(notdir $*).libs`
	chmod a-x "$*.$(EXTENSION)"
	$(STRIP) $*.$(EXTENSION)
	rm -f -- $*.o




#------------------------------------------------------------------------------#
# ALL

# if your library isn't included in LIB_TARGETS, it won't be built with
# Pd-extended.  For libraries that build on all platforms, add them directly
# below, otherwise add to the correct platforms below.

#
# WARNING!  this MUST be all on one line because the automatic package
# building scripts rely on it being that way.
ifeq ($(LIGHT),yes)
# minimal set of extensions for the "light" build
lib_targets = loaders-libdir pddp
INCREMENTAL = yes
else
lib_targets = adaptive arraysize autotune bassemu boids bsaylor comport creb cxc disis earplug ekext ext13 fftease flatgui flite fluid freeverb ggee hcs iem_ambi iem_bin_ambi iemlib iemgui iemguts iem_adaptfilt iemmatrix iemxmlrpc iem_delay iem_roomsim iem_spec2 iem_tab jasch_lib js loaders-libdir lyonpotpourri mapping markex maxlib mjlib moocow moonlib motex mrpeach neuraln oscx pan  pd-cyclone pd-else pdcontainer pddp pdlua pdogg plugin pmpd rjlib sigpack smlib tof unauthorized vbap windowing zexy
endif

# Emscriptem needs flatspace for some objects, wheras normal PD has them builtin
# Additionally, the pdjs external is currently not supported in emscripten.
ifeq ($(EMSCRIPTEN),yes)
lib_targets += flatspace
CMAKE = emcmake cmake
else
lib_targets += hidraw
CMAKE = cmake
endif

# NEW (IN-PROGRESS): flext

# this is for libraries that don't compile (yet) on all platforms
ifneq ($(LIGHT),yes)
ifneq ($(EMSCRIPTEN),yes)
ifeq ($(OS_NAME),windows)
  ifneq ($(INCREMENTAL),yes)
    lib_targets += gem
  endif
else
  ifeq ($(OS_NAME),darwin)
# on Mac OS X 10.6/Snow Leopard don't build hid since it needs Carbon
# 2024-10-25 original hid has been completely removed and replaced by the
# cross-platform hidraw
    ifeq ($(INCREMENTAL),yes)
			lib_targets += pdp iem16 apple
    else
			lib_targets += gem pdp iem16 apple
    endif
  else ifneq ($(LIGHT),yes)
    # GNU/Linux, BSD, IRIX, etc. (we use exported variable INCREMENTAL to
    # avoid rebuilding entire Gem lib that takes a long time to compile)
    ifeq ($(INCREMENTAL),yes)
	lib_targets += pdp iem16
    else
	lib_targets += gem pdp iem16
    endif
  endif
endif
endif # ifneq ($(EMSCRIPTEN),yes)
endif # ifneq ($(LIGHT),yes)


# filter out blacklist of all targets NOT to be built
LIB_TARGETS = $(filter-out $(blacklist),$(lib_targets))

#------------------------------------------------------------------------------#

all: $(LIB_TARGETS)
	@echo " "
	@echo "Compiled externals for $(OS_NAME) aka $(UNAME)"


install: $(examplesdir) $(manualsdir) $(objectsdir) \
all $(patsubst %, %_install,$(LIB_TARGETS))
	@echo " "
	@echo "externals install succeeded!"


#==============================================================================#
#
# OLD EXTERNALS BUILD SYSTEM TARGETS
#
# This is all stuff related to the externals/build/src/*.c links.  The idea is
# that they make for a flat namespace, here they are included as a libdir
#
#==============================================================================#

FLATSPACE_NAME=flatspace

ifneq ($(EMSCRIPTEN),yes)
FLATSPACE_OBJECTS := $(wildcard $(externals_src)/build/src/*.c)
flatspace: $(FLATSPACE_OBJECTS:.c=.$(EXTENSION))

flatspace_install: flatspace $(objectsdir)
	install -d $(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(FLATSPACE_NAME) \
		--author "Numerous" \
		--description "This is a collection of externals in a flat namespace" \
		--license "GNU GPL"
	install -p $(FLATSPACE_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
# all standard objs' help files, it had to be broken up because the list is
# soo long.  They are installed only inside of the libdir since this is so
# messy.  We don't these help patches to come up for other objects
	install -p \
			$(externals_src)/arraysize/*.pd \
			$(externals_src)/beatpipe/*.pd \
			$(externals_src)/bsaylor/help/*.pd \
			$(externals_src)/control/*/*.pd \
			$(externals_src)/bbogart/chaos/tools/*.pd \
			$(externals_src)/bbogart/*/*.pd \
				$(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
	install -p \
			$(externals_src)/ff/*.pd \
			$(externals_src)/hcs/folder_list-help.pd \
			$(externals_src)/hcs/split_path-help.pd \
				$(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
	install -p \
			$(externals_src)/iem/comport/*/*-help.pd \
			$(externals_src)/plugin~/*.pd \
			$(externals_src)/rhythm_estimator/*.p? \
				$(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
else
FLATSPACE_OBJECTS = $(externals_src)/build/src/s2l.c $(externals_src)/build/src/l2i.c $(externals_src)/build/src/l2s.c
flatspace: $(FLATSPACE_OBJECTS:.c=.$(EXTENSION))
$(FLATSPACE_OBJECTS:.c=.$(EXTENSION)): $(FLATSPACE_OBJECTS:.c=.o)
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o" $(LIBS)

$(FLATSPACE_OBJECTS:.c=.o): $(FLATSPACE_OBJECTS)
	$(CC) $(CFLAGS) -o "$*.o" -c "$*.c"

flatspace_install: flatspace $(objectsdir)
	install -d $(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
	install -p $(FLATSPACE_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(FLATSPACE_NAME)
endif



flatspace_clean:
	-rm -f -- $(FLATSPACE_OBJECTS:.c=.o) 
	-rm -f -- $(FLATSPACE_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(externals_src)/build/src/*.*~
	-rm -f -- $(externals_src)/build/src/*.c.bak



#==============================================================================#
#
# PROJECT TARGETS
#
#==============================================================================#

# this is the template for creating new entries:

#------------------------------------------------------------------------------#
# TEMPLATE
TEMPLATE_NAME=template
TEMPLATE_OBJECTS := $(wildcard $(externals_src)/template/*.c)
template: $(TEMPLATE_OBJECTS:.c=.$(EXTENSION))

template_install: template
	install -d $(DESTDIR)$(objectsdir)/$(TEMPLATE_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(TEMPLATE_NAME) \
		--author "" \
		--description "" \
		--license "" \
		--version ""
	install -p $(TEMPLATE_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(TEMPLATE_NAME)
#	install -d $(manualsdir)/$(TEMPLATE_NAME)
#	install -p $(externals_src)/template/manual.txt \
#		$(manualsdir)/$(TEMPLATE_NAME)
	install -d $(DESTDIR)$(examplesdir)/$(TEMPLATE_NAME)
	install -p $(externals_src)/template/examples/*.pd \
		$(DESTDIR)$(examplesdir)/$(TEMPLATE_NAME)

template_clean:
	-rm -f -- $(TEMPLATE_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(TEMPLATE_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/template/*.bak
	-rm -f -- $(externals_src)/template/*.*~




#------------------------------------------------------------------------------#
# AKA.WIIREMOTE
# this is installed into the "io" library
AKA.WIIREMOTE_NAME=io
AKA.WIIREMOTE_SRC := $(wildcard $(externals_src)/io/aka.wiiremote/*.c)

AKA.WIIREMOTE_OBJECTS := $(AKA.WIIREMOTE_SRC:.c=.o)
$(AKA.WIIREMOTE_OBJECTS) : %.o : %.c
	$(CC) $(OPT_CFLAGS) $(CFLAGS) -o "$*.o" -c "$*.c"

$(externals_src)/io/aka.wiiremote/akawiiremote.$(EXTENSION): $(AKA.WIIREMOTE_OBJECTS) 
	$(CC) $(LDFLAGS) -o $(externals_src)/io/aka.wiiremote/akawiiremote.$(EXTENSION) \
		$(AKA.WIIREMOTE_OBJECTS) -weak_framework IOBluetooth \
		-weak_framework CoreFoundation
#	$(STRIP) $(externals_src)/io/aka.wiiremote/aka.wiiremote.$(EXTENSION)

aka.wiiremote: $(externals_src)/io/aka.wiiremote/akawiiremote.$(EXTENSION)

aka.wiiremote_install: aka.wiiremote
	install -d $(DESTDIR)$(objectsdir)/$(AKA.WIIREMOTE_NAME)
	install -p $(externals_src)/io/aka.wiiremote/aka.wiiremote.$(EXTENSION) \
		$(DESTDIR)$(objectsdir)/$(AKA.WIIREMOTE_NAME)
	install -d $(DESTDIR)$(manualsdir)/$(AKA.WIIREMOTE_NAME)
	install -p $(externals_src)/io/aka.wiiremote/*.txt \
		$(DESTDIR)$(manualsdir)/$(AKA.WIIREMOTE_NAME)

aka.wiiremote_clean:
	-rm -f -- $(externals_src)/io/aka.wiiremote/aka.wiiremote.$(EXTENSION)
	-rm -f -- $(AKA.WIIREMOTE_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/io/aka.wiiremote/*.bak
	-rm -f -- $(externals_src)/io/aka.wiiremote/*.*~



#------------------------------------------------------------------------------#
# APPLE
apple:
	make -C $(externals_src)/apple PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" 

apple_install:
	make -C $(externals_src)/apple DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

apple_clean:
	make -C $(externals_src)/apple clean

#------------------------------------------------------------------------------#
# AUTOTUNE
autotune:
	make -C $(externals_src)/autotune PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

autotune_install:
	make -C $(externals_src)/autotune DESTDIR="$(DESTDIR)" \
		objectsdir="$(objectsdir)" $(call set_em_flags) install
	install -p $(externals_src)/autotune/autotune_scale_warp.png \
		$(DESTDIR)/$(objectsdir)/autotune~

autotune_clean:
	make -C $(externals_src)/autotune clean

#------------------------------------------------------------------------------#
# ARRAYSIZE

arraysize:
	make -C $(externals_src)/arraysize PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

arraysize_install:
	make -C $(externals_src)/arraysize DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

arraysize_clean:
	make -C $(externals_src)/arraysize clean


#------------------------------------------------------------------------------#
# BASSEMU
bassemu:
	make -C $(externals_src)/bassemu~ PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

bassemu_install:
	make -C $(externals_src)/bassemu~ DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

bassemu_clean:
	make -C $(externals_src)/bassemu~ clean


#------------------------------------------------------------------------------#
# BOIDS
boids:
	make -C $(externals_src)/boids PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

boids_install:
	make -C $(externals_src)/boids \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

boids_clean:
	make -C $(externals_src)/boids clean


#------------------------------------------------------------------------------#
# BSAYLOR
bsaylor:
	make -C $(externals_src)/bsaylor PD_PATH=$(pd_src) CFLAGS="$(CFLAGS) -fno-strict-aliasing" $(call set_em_flags)

bsaylor_install:
	make -C $(externals_src)/bsaylor \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install
	$(call if_emscripten, -rm -f $(DESTDIR)$(objectsdir)/bsaylor/pvoc~* $(DESTDIR)$(objectsdir)/bsaylor/partconv~*)

bsaylor_clean:
	make -C $(externals_src)/bsaylor clean

#------------------------------------------------------------------------------#
# COMPORT
comport:
	make -C $(externals_src)/iem/comport/comport PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

comport_install:
	make -C $(externals_src)/iem/comport/comport \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

comport_clean:
	make -C $(externals_src)/iem/comport/comport clean

#------------------------------------------------------------------------------#
# CREB
CREB_NAME=creb
CREB_OBJECTS := $(wildcard $(externals_src)/creb/modules/*.c)
CREB_CXXOBJECTS := $(wildcard $(externals_src)/creb/modules++/*.cc)

creb: $(CREB_OBJECTS:.c=.$(EXTENSION)) $(CREB_CXXOBJECTS:.cc=.$(EXTENSION))

creb_install: creb
	install -d $(DESTDIR)$(objectsdir)/$(CREB_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(CREB_NAME) \
		--author "Tom Schouten <tom@zwizwa.be>" \
		--description "This is a collection of pd externals. My bag of tricks." \
		--license "GNU GPL 2" \
		--version "0.9.2"
	install -p $(externals_src)/creb/abs/*.pd $(DESTDIR)$(objectsdir)/$(CREB_NAME)
	install -p $(CREB_OBJECTS:.c=.$(EXTENSION)) \
		$(CREB_CXXOBJECTS:.cc=.$(EXTENSION)) \
		$(DESTDIR)$(objectsdir)/$(CREB_NAME)
	install -p $(externals_src)/creb/doc/*.* $(DESTDIR)$(objectsdir)/$(CREB_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(CREB_NAME)/manual
	install -p $(externals_src)/creb/doc/reference.txt \
		$(DESTDIR)$(objectsdir)/$(CREB_NAME)/manual
	install -d $(DESTDIR)$(objectsdir)/$(CREB_NAME)/examples
	install -p $(externals_src)/creb/doc/examples/*.* \
		$(DESTDIR)$(objectsdir)/$(CREB_NAME)/examples

creb_clean:
	-rm -f -- $(CREB_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(CREB_OBJECTS:.c=.o)
	-rm -f -- $(CREB_CXXOBJECTS:.cc=.$(EXTENSION))
	-rm -f -- $(CREB_CXXOBJECTS:.cc=.o)
	-rm -f -- $(externals_src)/creb/*/*.bak
	-rm -f -- $(externals_src)/creb/*/*.*~


#------------------------------------------------------------------------------#
# CXC
cxc:
	make -C $(externals_src)/cxc PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

cxc_install:
	make -C $(externals_src)/cxc DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

cxc_clean:
	make -C $(externals_src)/cxc clean

#------------------------------------------------------------------------------#
# CYCLONE

SILENCE_PD_GUI := -D\"pdgui_vmess(...)=\" -D\"pdgui_stub_vnew(...)=\" -D\"pdgui_stub_deleteforkey(...)=\"
PD_CYCLONE_PATH := $(externals_src)/pd-cyclone
PD_CYCLONE_FLAGS := $(CFLAGS_GITHUB_RUNNER) $(CFLAGS) $(SILENCE_PD_GUI) -DGLIST_GRAB_COMPAT -DCOLLTHREAD=0

$(PD_CYCLONE_PATH)/.patched:
	cd $(PD_CYCLONE_PATH) && git apply $(externals_src)/patches/cyclone.patch
	touch $(PD_CYCLONE_PATH)/.patched

pd-cyclone: $(PD_CYCLONE_PATH)/.patched
	make -C $(PD_CYCLONE_PATH) PDBINDIR=$(pd_src)/src PDINCLUDEDIR=$(pd_src)/src CFLAGS="$(PD_CYCLONE_FLAGS)" -j extension=$(EXTENSION)

pd-cyclone_install:
	make -C $(PD_CYCLONE_PATH) install objectsdir="$(objectsdir)" extension=$(EXTENSION)

pd-cyclone_clean:
	make -C $(PD_CYCLONE_PATH) clean

PD_ELSE_PATH := $(externals_src)/pd-else
PD_ELSE_FLAGS := $(CFLAGS) $(SILENCE_PD_GUI) -DGLIST_GRAB_COMPAT -DPURR_DATA -DPDL2ORK -Wno-incompatible-function-pointer-types

$(PD_ELSE_PATH)/.patched:
	cd $(PD_ELSE_PATH) && git apply $(externals_src)/patches/else.patch
	touch $(PD_ELSE_PATH)/.patched

ifeq ($(EMSCRIPTEN),yes)
OGG_PATH := $(PD_ELSE_PATH)/.ogg
OGG_LIB := $(OGG_PATH)/build/libogg.a
VORBIS_PATH := $(PD_ELSE_PATH)/.vorbis
VORBIS_LIB := $(VORBIS_PATH)/build/lib/libvorbis.a
OPENSSL_PATH := $(PD_ELSE_PATH)/.openssl
OPENSSL_LIB := $(OPENSSL_PATH)/libssl.a
PD_ELSE_EM_FLAGS := $(PD_ELSE_FLAGS) -msimd128 -U__linux__

$(OGG_PATH)/README.md:
	git clone https://github.com/emscripten-ports/Ogg $(OGG_PATH)

$(VORBIS_PATH)/README.md:
	git clone https://github.com/emscripten-ports/Vorbis $(VORBIS_PATH)

$(OPENSSL_PATH)/README.md:
	git clone https://github.com/openssl/openssl $(OPENSSL_PATH)

$(OGG_LIB): $(OGG_PATH)/README.md $(OPENSSL_LIB)
	$(CMAKE) -B$(OGG_PATH)/build $(OGG_PATH)
	MAKEOVERRIDES= emmake make -j -C $(OGG_PATH)/build

$(VORBIS_LIB): $(VORBIS_PATH)/README.md $(OGG_LIB)
	$(CMAKE) -B$(VORBIS_PATH)/build $(VORBIS_PATH) -DOGG_INCLUDE_DIRS="$(OGG_PATH)/include" -DOGG_LIBRARIES="$(OGG_PATH)/build"
	emmake make -j -C $(VORBIS_PATH)/build

$(OPENSSL_LIB): $(OPENSSL_PATH)/README.md
	cd $(OPENSSL_PATH) && emconfigure ./Configure -no-asm -no-apps gcc
	MAKEFLAGS= emmake make -j -C $(OPENSSL_PATH) LDFLAGS="" WARN_FLAGS="" CXX=`which em++` AR=`which emar` CC=`which emcc` RANLIB=`which emranlib`

$(PD_ELSE_PATH)/build/.built: $(PD_ELSE_PATH)/.patched $(OGG_LIB) $(VORBIS_LIB) $(OPENSSL_LIB)
	$(CMAKE) -B$(PD_ELSE_PATH)/build -Wno-dev -DPD_PATH="$(pd_src)" $(PD_ELSE_PATH) \
		-DOPENSSL_ROOT_DIR=$(OPENSSL_PATH) \
		-DOGG_INCLUDE_DIR="$(OGG_PATH)/include" -DOGG_LIBRARY="$(OGG_PATH)/build" \
		-DVORBIS_INCLUDE_DIR="$(VORBIS_PATH)/include" -DVORBIS_LIBRARY="$(VORBIS_PATH)/build" \
		-DVORBISENC_INCLUDE_DIR="$(VORBIS_PATH)/include" -DVORBISENC_LIBRARY="$(VORBIS_PATH)/build" \
		-DCMAKE_C_FLAGS="$(PD_ELSE_EM_FLAGS)" -DCMAKE_CXX_FLAGS="$(PD_ELSE_EM_FLAGS)"
	env -i PATH=$(PATH) bash -c ' \
		cd $(PD_ELSE_PATH)/build/Source/Shared/ffmpeg/ffmpeg-7.0.1 && \
		emconfigure ./configure --target-os=none --arch=x86_32 --enable-cross-compile --disable-debug --disable-x86asm --disable-programs && \
		emmake make -j \
	'
	$(CMAKE) -B$(PD_ELSE_PATH)/Source/Audio/sfz~/sfizz/library/build $(PD_ELSE_PATH)/Source/Audio/sfz~/sfizz/library \
		-DCMAKE_C_FLAGS="$(PD_ELSE_EM_FLAGS)" -DCMAKE_CXX_FLAGS="$(PD_ELSE_EM_FLAGS)" && \
	emmake make -C $(PD_ELSE_PATH)/build -j
	for f in $(PD_ELSE_PATH)/build/else/*.l_arm64; do \
		emcc $(CFLAGS) "$$f" -o "$${f%.l_arm64}.wasm"; \
		rm $$f; \
	done
	touch $(PD_ELSE_PATH)/build/.built
else
$(PD_ELSE_PATH)/build/.built: $(PD_ELSE_PATH)/.patched
ifeq ($(OS_NAME),windows)
	CMAKE_GENERATOR="Unix Makefiles" $(CMAKE) -B$(PD_ELSE_PATH)/build -Wno-dev -DPD_LIBRARY="$(pd_src)/src/pd.dll" -DPD_PATH="$(pd_src)" $(PD_ELSE_PATH) -DCMAKE_C_FLAGS="$(PD_ELSE_FLAGS)" -DCMAKE_CXX_FLAGS="$(PD_ELSE_FLAGS)"
else
	$(CMAKE) -B$(PD_ELSE_PATH)/build -Wno-dev -DPD_PATH="$(pd_src)" $(PD_ELSE_PATH) -DCMAKE_C_FLAGS="$(PD_ELSE_FLAGS)" -DCMAKE_CXX_FLAGS="$(PD_ELSE_FLAGS)"
endif
	make -C $(PD_ELSE_PATH)/Source/Audio/sfz~/sfizz/library/build CFLAGS="$(PD_ELSE_FLAGS)" CXXFLAGS="$(PD_ELSE_FLAGS)"
	make -j -C $(PD_ELSE_PATH)/build
	touch $(PD_ELSE_PATH)/build/.built
endif

pd-else: $(PD_ELSE_PATH)/build/.built

pd-else_install:
	cp -r $(PD_ELSE_PATH)/build/else $(DESTDIR)/$(objectsdir)
ifeq ($(OS_NAME),darwin)
	for f in $(DESTDIR)/$(objectsdir)/else/*.darwin-fat-32.so; do mv -- "$$f" "$${f%.darwin-fat-32.so}.pd_darwin"; done
else ifeq ($(OS_NAME),windows)
	for f in $(DESTDIR)/$(objectsdir)/else/*.m_amd64; do mv -- "$$f" "$${f%.m_amd64}.pd_linux"; done
else
	for f in $(DESTDIR)/$(objectsdir)/else/*.l_amd64; do mv -- "$$f" "$${f%.l_amd64}.pd_linux"; done
endif

pd-else_clean:
	rm -rf $(PD_ELSE_PATH)/build
ifeq ($(EMSCRIPTEN),yes)
	rm -rf $(OGG_PATH)
	rm -rf $(VORBIS_PATH)
	rm -rf $(OPENSSL_PATH)
	rm -rf $(PD_ELSE_PATH)/Source/Audio/sfz~/sfizz/library/build
endif

#------------------------------------------------------------------------------#
# DISIS
XWIIMOTE_PATH := $(externals_src)/disis/xwiimote/lib
ifneq ($(EMSCRIPTEN),yes)
disis:
ifneq ($(OS_NAME),windows)
ifneq ($(OS_NAME),darwin)
	$(CC) -c -fPIC $(XWIIMOTE_PATH)/core.c -o $(XWIIMOTE_PATH)/core.o -D'XWII__EXPORT=__attribute__((visibility("default")))' 
	$(CC) -c -fPIC $(XWIIMOTE_PATH)/monitor.c -o $(XWIIMOTE_PATH)/monitor.o -D'XWII__EXPORT=__attribute__((visibility("default")))' 
	ar rcs $(XWIIMOTE_PATH)/libxwiimote.a $(XWIIMOTE_PATH)/*.o
endif
endif
	make -C $(externals_src)/disis PD_PATH=$(pd_src) pdbinpath=$(pd_src)/src CFLAGS="$(CFLAGS_GITHUB_RUNNER) $(CFLAGS_ADD)"

disis_flatpak:
ifneq ($(OS_NAME),windows)
ifneq ($(OS_NAME),darwin)
	$(CC) -c -fPIC $(XWIIMOTE_PATH)/core.c -o $(XWIIMOTE_PATH)/core.o -D'XWII__EXPORT=__attribute__((visibility("default")))' 
	$(CC) -c -fPIC $(XWIIMOTE_PATH)/monitor.c -o $(XWIIMOTE_PATH)/monitor.o -D'XWII__EXPORT=__attribute__((visibility("default")))' 
	ar rcs $(XWIIMOTE_PATH)/libxwiimote.a $(XWIIMOTE_PATH)/*.o
endif
endif
	make -C $(externals_src)/disis PD_PATH=$(pd_src) pdbinpath=$(pd_src)/src CFLAGS="$(CFLAGS_GITHUB_RUNNER) $(CFLAGS_ADD)"


disis_install:
	make -C $(externals_src)/disis \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" \
		install

disis_clean:
ifneq ($(OS_NAME),windows)
ifneq ($(OS_NAME),darwin)
	rm -f $(XWIIMOTE_PATH)/libxwiimote.a
	rm -f $(XWIIMOTE_PATH)/core.o
	rm -f $(XWIIMOTE_PATH)/monitor.o
endif
endif
	make -C $(externals_src)/disis clean
else
disis:

disis_install:
	install -d $(DESTDIR)$(objectsdir)/disis
	install -p $(externals_src)/disis/*.pd $(DESTDIR)$(objectsdir)/disis
endif

#------------------------------------------------------------------------------#
# EKEXT
ekext:
	make -C $(externals_src)/ekext PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

ekext_install:
	make -C $(externals_src)/ekext DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

ekext_clean:
	make -C $(externals_src)/ekext clean


#------------------------------------------------------------------------------#
# EXT13
ext13:
	make -C $(externals_src)/ext13 PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

ext13_install:
	make -C $(externals_src)/ext13 DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

ext13_clean:
	make -C $(externals_src)/ext13 clean

#------------------------------------------------------------------------------#
# FFTEASE
FFTEASE_NAME=fftease

fftease:
ifeq ($(EMSCRIPTEN),yes)
	sed -i 's/--enable-new-dtags/-Bsymbolic/g' $(externals_src)/$(FFTEASE_NAME)/Makefile
endif
	make -C $(externals_src)/$(FFTEASE_NAME) $(call set_em_flags) PD_PATH=$(pd_src) CFLAGS="$(CFLAGS_GITHUB_RUNNER) $(CFLAGS_ADD)"
ifeq ($(EMSCRIPTEN),yes)
	sed -i 's/-Bsymbolic/--enable-new-dtags/g' $(externals_src)/$(FFTEASE_NAME)/Makefile
endif

fftease_install:
	#make -C $(externals_src)/$(FFTEASE_NAME) DESTDIR="$(DESTDIR)" \
	#	objectsdir="$(objectsdir)" install
	install -d $(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)/sound
	install -p $(externals_src)/$(FFTEASE_NAME)/*.$(EXTENSION) \
		$(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)
	install -p $(externals_src)/$(FFTEASE_NAME)/fftease32-helpfiles/sound/*.* \
		$(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)/sound
	# copy the meta file and the shared lib
ifneq ($(EMSCRIPTEN),yes)
	install -p $(externals_src)/$(FFTEASE_NAME)/libfftease* \
		$(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)
endif
	# license and readme
	install -p $(externals_src)/$(FFTEASE_NAME)/*.txt \
		$(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)
	# install help files
	install -p $(externals_src)/$(FFTEASE_NAME)/fftease32-helpfiles/*.pd \
		$(DESTDIR)$(objectsdir)/$(FFTEASE_NAME)

fftease_clean:
	make -C $(externals_src)/$(FFTEASE_NAME) clean

#------------------------------------------------------------------------------#
# FLATGUI
#
# Right now we're just building footils/knob and throwing it in the flatgui
# external directory
#
flatgui:
	make -C $(externals_src)/footils/knob CFLAGS="$(CFLAGS)" \
		PD_PATH=$(pd_src) pdbinpath=$(pd_src)/src \
		PD_INCLUDE=$(pd_src)/src $(call set_em_flags)

flatgui_install:
	make -C $(externals_src)/footils/knob STRIP="$(STRIP)" \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

flatgui_clean:
	make -C $(externals_src)/footils/knob clean

#------------------------------------------------------------------------------#
# FLEXT and FLEXT externals
flext:
	cd $(externals_src)/grill/trunk/flext && \
	./bootstrap.sh || ./bootstrap.sh && \
	./build.sh pd gcc build && \
	./build.sh pd gcc build && \
	./build.sh pd gcc build && \
	./build.sh pd gcc install

	#fluid~
#	cd footils/fluid && \
#	../../grill/trunk/flext/build.sh pd gcc build && \
#	../../grill/trunk/flext/build.sh pd gcc build

	#disis_munger~
	cd ../l2ork_addons/disis_munger && \
	../../externals/grill/trunk/flext/build.sh pd gcc build && \
	../../externals/grill/trunk/flext/build.sh pd gcc build

flext_install:
	# we don't install flext but rather link it statically,
	# so we only install statically linked externals

	#fluid~
#	cd footils/fluid && \
#	install -D pd-linux/release-multi/fluid~.pd_linux $(DESTDIR)$(objectsdir)/flext/fluid~.pd_linux && \
#	install -D pd/* $(DESTDIR)$(objectsdir)/flext/

	#disis_munger~
	cd ../l2ork_addons/disis_munger && \
	install -D pd-linux/release-multi/disis_munger~.pd_linux $(DESTDIR)$(objectsdir)/flext/disis_munger~.pd_linux && \
	install -D *help.pd $(DESTDIR)$(objectsdir)/flext/

flext_clean:
	#flext
	cd grill/trunk/flext/ && \
	./build.sh pd gcc clean

	#fluid~
#	cd footils/fluid && \
#	../../grill/trunk/flext/build.sh pd gcc clean

	#fluid~
	cd ../l2ork_addons/disis_munger && \
	../../externals/grill/trunk/flext/build.sh pd gcc clean

#------------------------------------------------------------------------------#
# FLIB
FLIB_NAME=flib
# exclude the flib.c file for the single-file library
# plus ha~.c doesn't compile at the moment.
FLIB_OBJECTS := $(wildcard $(externals_src)/postlude/flib/src/[a-ei-z]*.c)
flib: $(FLIB_OBJECTS:.c=.$(EXTENSION))

flib_install: flib
	install -d $(DESTDIR)$(objectsdir)/$(FLIB_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(FLIB_NAME) \
		--author "Jamie Bullock" \
		--description "library for feature extraction" \
		--license "GNU GPL"
	install -p $(FLIB_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(FLIB_NAME)
	install -p $(externals_src)/postlude/flib/doc/*.pd \
		$(DESTDIR)$(objectsdir)/$(FLIB_NAME)
	install -p $(externals_src)/postlude/flib/README \
		$(DESTDIR)$(objectsdir)/$(FLIB_NAME)/README.txt

flib_clean:
	-rm -f -- $(FLIB_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(FLIB_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/postlude/flib/*/*.bak
	-rm -f -- $(externals_src)/postlude/flib/*/*.*~

#------------------------------------------------------------------------------#
# FLUID~

ifeq ($(EMSCRIPTEN),yes)
FLUID_PATH := $(externals_src)/fluid~
FLUID_LIBPATH := $(FLUID_PATH)/fluidsynth-emscripten/build

fluid: $(FLUID_PATH)/fluid~.wasm

$(FLUID_PATH)/fluid~.wasm: $(FLUID_LIBPATH)/src/libfluidsynth.a $(FLUID_PATH)/fluid~.c
	$(CC) $(CFLAGS) -I$(FLUID_LIBPATH)/include -I$(FLUID_LIBPATH)/../include $(FLUID_LIBPATH)/src/libfluidsynth.a $(FLUID_PATH)/fluid~.c -o $(FLUID_PATH)/fluid~.wasm

$(FLUID_LIBPATH)/src/libfluidsynth.a: $(FLUID_PATH)/fluidsynth-emscripten/README.md
	mkdir -p $(externals_src)/fluid~/fluidsynth-emscripten/build 
	cd $(externals_src)/fluid~/fluidsynth-emscripten/build && $(CMAKE) -Denable-oss=off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=off .. && emmake make

$(FLUID_PATH)/fluidsynth-emscripten/README.md:
	git clone https://github.com/jet2jet/fluidsynth-emscripten $(externals_src)/fluid~/fluidsynth-emscripten

fluid_install: fluid
	install -d $(DESTDIR)$(objectsdir)/fluid~
	install -p $(FLUID_PATH)/fluid~.wasm $(DESTDIR)$(objectsdir)/fluid~/fluid~.wasm
	install -d $(DESTDIR)$(objectsdir)/soundfonts
	install -p $(FLUID_PATH)/soundfonts/* $(DESTDIR)$(objectsdir)/soundfonts

fluid_clean:
	rm -rf $(FLUIDPATH)/fluidsynth-emscripten
	rm -rf $(FLUID_PATH)/fluid~.o
	rm -rf $(FLUID_PATH)/fluid~.wasm

else
fluid:
	make -C $(externals_src)/fluid~ PD_PATH=$(pd_src) \
		pdbinpath=$(pd_src)/src CFLAGS="$(CFLAGS_GITHUB_RUNNER) $(CFLAGS_ADD) -I$(usrlocal)/include" LDFLAGS="$(LDFLAGS)"

fluid_install:
	make -C $(externals_src)/fluid~ DESTDIR="$(DESTDIR)" \
		objectsdir="$(objectsdir)" install
	cd $(externals_src)/fluid~ && cp -rf soundfonts $(DESTDIR)/$(objectsdir)/

fluid_clean:
	make -C $(externals_src)/fluid~ clean
endif

#------------------------------------------------------------------------------#
# FREEVERB
freeverb:
	make -C $(externals_src)/freeverb~ PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

freeverb_install:
	make -C $(externals_src)/freeverb~ DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

freeverb_clean:
	make -C $(externals_src)/freeverb~ clean


#------------------------------------------------------------------------------
# GEM
GEM_NAME = Gem
gem_prep:
	cd $(gem_src) && ./autogen.sh
	cd $(gem_src) && sed 's/Pure\ Data/Pd-L2Ork/g' configure > tmp && mv tmp configure && chmod 755 configure
#	cd $(gem_src)/plugins && sed 's/DECKLINK\ //g' Makefile > tmp && mv tmp Makefile

ifeq ($(MSYSTEM),MINGW32)
msys_prefix = /mingw32
else ifeq ($(MSYSTEM),MINGW64)
msys_prefix = /mingw64
else
msys_prefix = /usr
endif

$(gem_src)/Gem.dll: gem_prep
	cd $(gem_src)/plugins && sed 's/SUBDIRS\ +=\ AVT//g' Makefile.am > tmp && mv tmp Makefile.am
	cd $(gem_src)/plugins && sed 's/SUBDIRS\ +=\ HALCON//g' Makefile.am > tmp && mv tmp Makefile.am
	cd $(gem_src)/plugins && sed 's/SUBDIRS\ +=\ NDI//g' Makefile.am > tmp && mv tmp Makefile.am
	cd $(gem_src)/plugins && sed 's/SUBDIRS\ +=\ PYLON//g' Makefile.am > tmp && mv tmp Makefile.am
	cd $(gem_src) && ./configure \
		CXXFLAGS="-DHAVE_S_STUFF_H $(CFLAGS_ADD)" \
		--without-ALL \
		--with-glfw3 \
		--with-jpeg \
		--with-jpeg-cflags="-I$(msys_prefix)/include" \
		--with-jpeg-libs="-L$(msys_prefix)/lib -ljpeg" \
		--with-ftgl \
		--with-ftgl-cflags="-I$(msys_prefix)/include -I$(msys_prefix)/include/freetype2" \
		--with-ftgl-libs="-L$(msys_prefix)/lib -lftgl" \
		--with-vfw32 \
		--prefix=$(prefix) \
		--libdir=$(objectsdir) \
		--with-pd=$(DESTDIR)
	$(MAKE) $(GEM_MAKEFLAGS) -C $(gem_src)

$(gem_src)/Gem.pd_linux: gem_prep
	test -s $(gem_src)/Gem.pd_linux || \
		cd $(gem_src) && ./configure \
			CXXFLAGS="-DHAVE_S_STUFF_H $(CFLAGS_ADD)" \
			--prefix=$(prefix) \
			--with-pd=$(pd_src) \
			--host=$(shell uname -m)-pc-linux-gnu
	$(MAKE) $(GEM_MAKEFLAGS) -C $(gem_src)

# Mac: This is still a bit experimental. At present, many of the window
# backends seem to be at least half-broken on recent macOS versions, including
# the native gemmacoswindow, which the wiki recommends. For me, the SDL2
# backend works best, on Mojave at least, so that's what we include as the
# default here. NOTES: In MacPorts, libquicktime is unmaintained and doesn't
# build, so it is currently excluded from the MacPorts build. AVF, which is
# supposed to replace libquicktime, is enabled by default on the Mac, but
# right now it only seems to work for video capture, not loading or recording.
# We also exclude ImageMagick, which is needed for improved image support,
# because it currently isn't in our OSX build requirements. But if you have it
# (it's available in both Homebrew and MacPorts), you can enable it below.

ifeq ($(optlocal),/opt/local)
# MacPorts
gem_quicktime = --without-libquicktime
else
# Homebrew
gem_quicktime = --with-libquicktime --with-libquicktime-LIBS="-L$(usrlocal)/lib -lquicktime"
endif

# Uncomment to enable ImageMagick support. This needs imagemagick from Hombrew
# or ImageMagick from MacPorts.
#gem_magick = --with-MagickCore --with-MagickCore-CFLAGS="$(shell pkg-config --cflags MagickCore)" --with-MagickCore-LIBS="$(shell pkg-config --libs MagickCore)"

# Adjust this as needed/wanted. If you comment this out, you'll get the
# default gemmacoswindow which doesn't work for me, YMMV.
gem_window = --with-sdl2 --with-sdl2-LIBS="-L$(usrlocal)/lib -lSDL2" --with-defaultwindow=gemglfw3window

$(gem_src)/Gem.pd_darwin: gem_prep
	cd $(gem_src) && autoreconf -vim
	perl -i -pe 's{^\s*func_fatal_error\s*"specify a tag with .--tag."}{      tagname=\${available_tags%% *}\n      echo "libtool: warning: no tag specified, assuming --tag=\$tagname" >&2;}g' $(gem_src)/ltmain.sh
	perl -i -pe 's/CGRect rect = \{\{0,0\},\{w,h\}\};/CGRect rect = {{0,0},{(double)w,(double)h}};/' $(gem_src)/plugins/imageIO/imageIO.mm
	cd $(gem_src) && ./configure \
		CXXFLAGS="-DHAVE_S_STUFF_H $(CFLAGS_ADD)" \
		--prefix=$(prefix) \
		--libdir=$(objectsdir) \
		--without-ALL \
		--with-glfw3 \
		--without-QuickTime-framework --without-Carbon-framework \
		--with-ftgl --with-ftgl-LIBS="-L$(usrlocal)/lib -lftgl" \
		--with-tiff --with-tiff-LIBS="-L$(usrlocal)/lib -ltiff" \
		--with-jpeg --with-jpeg-LIBS="-L$(usrlocal)/lib -ljpeg" \
		$(gem_quicktime) $(gem_magick) \
		$(gem_window) \
		--without-ndi --with-DeckLink-CFLAGS="-I./plugins/DECKLINK/SDK" \
		--with-pd=$(pd_src)
	$(MAKE) $(GEM_MAKEFLAGS) -C $(gem_src)

gem_configure: gem_prep
	cd $(gem_src) && ./configure \
	CXXFLAGS="-DHAVE_S_STUFF_H $(CFLAGS_ADD)" \
	--prefix=$(prefix) \
	--with-pd=$(pd_src) \
	--host=$(shell uname -m)-pc-linux-gnu

gem: $(gem_src)/Gem.$(EXTENSION)

gem_install:
	$(MAKE) -C $(gem_src) DESTDIR=$(DESTDIR) prefix=$(prefix) \
		libdir=$(objectsdir) pkglibdir=$(objectsdir)/Gem install

gem_clean:
	-$(MAKE) -C $(gem_src) clean


#------------------------------------------------------------------------------#
# GEM2PDP
GEM2PDP_NAME=gem2pdp
GEM2PDP_OBJECTS := $(wildcard $(externals_src)/gem2pdp/*.cpp)
$(externals_src)/gem2pdp/configure: $(externals_src)/gem2pdp/configure.ac
	cd $(externals_src)/gem2pdp && aclocal && autoconf

$(externals_src)/gem2pdp/Makefile: $(externals_src)/gem2pdp/Makefile.in
	cd $(externals_src)/gem2pdp && ./configure --with-pddir=$(pd_src) \
		--with-gemdir=$(gem_src)  --with-pdpdir=$(externals_src)/pdp

gem2pdp: $(externals_src)/gem2pdp/configure $(externals_src)/gem2pdp/Makefile
	$(MAKE) -C $(externals_src)/gem2pdp

gem2pdp_install: gem2pdp
	install -d $(DESTDIR)$(objectsdir)
#	install -d $(DESTDIR)$(objectsdir)/$(GEM2PDP_NAME)
#	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(GEM2PDP_NAME) \
#		--author "Yves Degoyon, Jamie Tittle, Georg Holzmann" \
#		--description "Bridges between Gem and PDP" \
#		--version "0.6"
	install -p $(GEM2PDP_OBJECTS:.cpp=.$(EXTENSION)) $(DESTDIR)$(objectsdir)
	install -p $(externals_src)/gem2pdp/*.pd  $(DESTDIR)$(objectsdir)
	install -p $(externals_src)/gem2pdp/README \
		$(DESTDIR)$(objectsdir)/$(GEM2PDP_NAME)-README.txt

gem2pdp_clean:
	-rm -rf -- $(externals_src)/gem2pdp/autom4te.cache
	-rm -f -- $(externals_src)/gem2pdp/config.status
	-rm -f -- $(externals_src)/gem2pdp/config.log
	-rm -f -- $(externals_src)/gem2pdp/configure
	-rm -f -- $(externals_src)/gem2pdp/Makefile
	-rm -f -- $(GEM2PDP_OBJECTS:.cpp=.$(EXTENSION))
	-rm -f -- $(externals_src)/gem2pdp/*.o
	-rm -f -- $(externals_src)/gem2pdp/*.bak
	-rm -f -- $(externals_src)/gem2pdp/*.*~


#------------------------------------------------------------------------------#
# GGEE
ggee:
	make -C $(externals_src)/ggee PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

ggee_install:
	make -C $(externals_src)/ggee DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install
	cp $(externals_src)/ggee/gui/logo100.gif $(DESTDIR)/$(objectsdir)/ggee
	cp $(externals_src)/ggee/gui/empty_image.png $(DESTDIR)/$(objectsdir)/ggee
	cp $(externals_src)/ggee/gui/source3.gif $(DESTDIR)/$(objectsdir)/ggee
	cp $(externals_src)/ggee/gui/surround-room.jpg $(DESTDIR)/$(objectsdir)/ggee
	cp $(externals_src)/ggee/gui/vu.png $(DESTDIR)/$(objectsdir)/ggee
	cp $(externals_src)/ggee/gui/vu-dial.png $(DESTDIR)/$(objectsdir)/ggee
	cp $(externals_src)/ggee/gui/vu-led.png $(DESTDIR)/$(objectsdir)/ggee

ggee_clean:
	make -C $(externals_src)/ggee clean


#------------------------------------------------------------------------------#
# HCS
hcs:
	make -C $(externals_src)/hcs PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

hcs_install:
	make -C $(externals_src)/hcs DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

hcs_clean:
	make -C $(externals_src)/hcs clean


#------------------------------------------------------------------------------#
# HID
HID_NAME = hid
ifeq ($(OS_NAME),windows)
  HID_CFLAGS = $(OPT_CFLAGS) $(CFLAGS) 
  HID_LIBS = $(LIBS) -lhid -lsetupapi
else
  ifeq ($(OS_NAME),darwin)
    FRAMEWORKS = Carbon IOKit ForceFeedback
    HID_CFLAGS = $(OPT_CFLAGS) $(CFLAGS) -I$(externals_src)/hid/HID\ Utilities\ Source
    HID_UTILITIES_SOURCE = $(externals_src)/hid/HID\ Utilities\ Source
    HID_LIBS = $(LIBS) -L$(HID_UTILITIES_SOURCE)/build \
		-L$(HID_UTILITIES_SOURCE)/build/Default \
      -lHIDUtilities $(patsubst %,-weak_framework %,$(FRAMEWORKS))
  else
    HID_CFLAGS = $(OPT_CFLAGS) $(CFLAGS) 
    HID_LIBS = $(LIBS)
  endif
endif

HID_SRC = input_arrays.c hid_$(OS_NAME).c hid.c
HID_OBJECTS := $(patsubst %.c, $(externals_src)/hid/%.o, $(HID_SRC))
$(HID_OBJECTS) : %.o : %.c
	$(CC) $(HID_CFLAGS) -o "$*.o" -c "$*.c"

$(HID_UTILITIES_SOURCE)/build/Default/libHIDUtilities.a:
# Apple changed the XCode CLI tool's name in xcode2... arg
# if on non-Mac OS X, this target just echos a message
ifeq ($(UNAME),Darwin)
	cd $(HID_UTILITIES_SOURCE) && \
		(test -x /usr/bin/xcodebuild && /usr/bin/xcodebuild) || \
			(test -x /usr/bin/pbxbuild && /usr/bin/pbxbuild) || \
				echo "Not building Apple HID Utilities"
endif

$(externals_src)/hid/hid.$(EXTENSION): $(HID_OBJECTS) \
$(HID_UTILITIES_SOURCE)/build/Default/libHIDUtilities.a
	$(CC) $(LDFLAGS) -o $(externals_src)/hid/hid.$(EXTENSION) \
		$(HID_OBJECTS) $(HID_LIBS)
#	$(STRIP) $(externals_src)/hid/hid.$(EXTENSION)

hid: $(externals_src)/hid/hid.$(EXTENSION)

hid_install: hid
	install -d $(DESTDIR)$(objectsdir)/$(HID_NAME)
	install -p $(externals_src)/hid/hid.$(EXTENSION) \
		$(DESTDIR)$(objectsdir)/$(HID_NAME)
	install -p $(externals_src)/hid/*.pd $(DESTDIR)$(objectsdir)/$(HID_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(HID_NAME)/examples
	install -p $(externals_src)/hid/examples/*.pd  \
		$(DESTDIR)$(objectsdir)/$(HID_NAME)/examples

hid_clean:
	-rm -f -- $(externals_src)/hid/*.o
	-rm -f -- $(externals_src)/hid/hid.$(EXTENSION)
	-rm -f -- $(externals_src)/hid/*.bak
	-rm -f -- $(externals_src)/hid/*.*~
	-rm -f -- $(HID_UTILITIES_SOURCE)/build/libHIDUtilities.a
	-rm -f -- $(HID_UTILITIES_SOURCE)/build/Default/libHIDUtilities.a


#------------------------------------------------------------------------------#
# HIDIN
HIDIN_NAME=hidin
HIDIN_OBJECTS := $(wildcard $(externals_src)/olafmatt/hidin/*.c)
HIDIN_DLL := $(externals_src)/olafmatt/hidin/hidin.$(EXTENSION)

$(HIDIN_DLL): $(HIDIN_OBJECTS:.c=.o)
	$(CC) $(LDFLAGS) -o "$(HIDIN_DLL)" $(HIDIN_OBJECTS:.c=.o) $(LIBS) -lhid \
		-lsetupapi
	chmod a-x "$(HIDIN_DLL)"
	$(STRIP) $(HIDIN_DLL)

hidin: $(HIDIN_DLL)

hidin_install: hidin
	install -d $(DESTDIR)$(objectsdir)/$(HIDIN_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(HIDIN_NAME) \
		--author "Olaf Matthes" \
		--description "HID input for Windows HID" \
		--license "GNU GPL"
	install -p $(HIDIN_DLL) $(DESTDIR)$(objectsdir)/$(HIDIN_NAME)
#	install -d $(DESTDIR)$(manualsdir)/$(HIDIN_NAME)
#	install -p $(externals_src)/olafmatt/hidin/README \
#		$(DESTDIR)$(objectsdir)/$(HIDIN_NAME)
#	install -d $(DESTDIR)$(examplesdir)/$(HIDIN_NAME)
#	install -p $(externals_src)/olafmatt/hidin/examples/*.pd \
#		$(DESTDIR)$(examplesdir)/$(HIDIN_NAME)

hidin_clean:
	-rm -f -- $(HIDIN_DLL)
	-rm -f -- $(HIDIN_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/olafmatt/hidin/*.bak
	-rm -f -- $(externals_src)/olafmatt/hidin/*.*~


#------------------------------------------------------------------------------#
# General IEM flags

#------------------------------------------------------------------------------#
# IEM_AMBI
IEM_AMBI_NAME=iem_ambi
# exclude the files for the single-file library format
IEM_AMBI_OBJECTS := $(wildcard $(externals_src)/iem/iem_ambi/src/[a-hj-z]*.c)
iem_ambi: $(IEM_AMBI_OBJECTS:.c=.$(EXTENSION))

iem_ambi_install: iem_ambi
	install -d $(DESTDIR)$(objectsdir)/$(IEM_AMBI_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEM_AMBI_NAME) \
		--author "IEM/KUG, Graz, Austria" \
		--description "calculate ambisonic encoder matrices rotation matrices and decoder matrices from 1st to 4th order in 2D or 3D." \
		--license "GNU GPL" \
		--version ""
	install -p $(IEM_AMBI_OBJECTS:.c=.$(EXTENSION)) \
		$(DESTDIR)$(objectsdir)/$(IEM_AMBI_NAME)
	install -p $(externals_src)/iem/iem_ambi/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEM_AMBI_NAME)
#	install -d $(DESTDIR)$(manualsdir)/$(IEM_AMBI_NAME)
	install -p $(externals_src)/iem/iem_ambi/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEM_AMBI_NAME)/README.txt
#	install -d $(DESTDIR)$(examplesdir)/$(IEM_AMBI_NAME)
#	install -p $(externals_src)/iem/iem_ambi/examples/*.pd \
#		$(DESTDIR)$(examplesdir)/$(IEM_AMBI_NAME)

iem_ambi_clean:
	-rm -f -- $(IEM_AMBI_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(IEM_AMBI_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/iem/iem_ambi/*/*.bak
	-rm -f -- $(externals_src)/iem/iem_ambi/*/*.*~



#------------------------------------------------------------------------------#
# IEM_BIN_AMBI
IEM_BIN_AMBI_NAME=iem_bin_ambi
# exclude the files for the single-file library format
IEM_BIN_AMBI_OBJECTS := $(wildcard $(externals_src)/iem/iem_bin_ambi/src/[a-hj-z]*.c)
iem_bin_ambi: $(IEM_BIN_AMBI_OBJECTS:.c=.$(EXTENSION))

iem_bin_ambi_install: iem_bin_ambi
	install -d $(DESTDIR)$(objectsdir)/$(IEM_BIN_AMBI_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEM_BIN_AMBI_NAME) \
		--author "IEM/KUG, Graz, Austria" \
		--description "calculate the product of an ambisonic decoder-matrix and the binaural HRIR's (in frequency and in time domain)" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEM_BIN_AMBI_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEM_BIN_AMBI_NAME)
	install -p $(externals_src)/iem/iem_bin_ambi/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEM_BIN_AMBI_NAME)
#	install -d $(DESTDIR)$(manualsdir)/$(IEM_BIN_AMBI_NAME)
	install -p $(externals_src)/iem/iem_bin_ambi/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEM_BIN_AMBI_NAME)/README.txt
#	install -d $(DESTDIR)$(examplesdir)/$(IEM_BIN_AMBI_NAME)
#	install -p $(externals_src)/iem/iem_bin_ambi/examples/*.pd \
#		$(DESTDIR)$(examplesdir)/$(IEM_BIN_AMBI_NAME)

iem_bin_ambi_clean:
	-rm -f -- $(IEM_BIN_AMBI_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(IEM_BIN_AMBI_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/iem/iem_bin_ambi/*/*.bak
	-rm -f -- $(externals_src)/iem/iem_bin_ambi/*/*.*~



#------------------------------------------------------------------------------#
# IEM16
# more externals in one file - so a little bit complicated ... ;)
IEM16_NAME=iem16
IEM16_DIR := $(externals_src)/iem16/src
IEM16_SRC := $(wildcard $(IEM16_DIR)/*.c)

$(IEM16_DIR)/aclocal.m4: $(IEM16_DIR)/acinclude.m4
	cd $(IEM16_DIR) && aclocal

$(IEM16_DIR)/configure: $(IEM16_DIR)/configure.ac $(IEM16_DIR)/aclocal.m4
	cd $(IEM16_DIR) && autoconf

$(IEM16_DIR)/Make.config: $(IEM16_DIR)/Make.config.in \
$(IEM16_DIR)/configure
	cd $(IEM16_DIR) && ./configure --disable-library \
		--with-pd=$(pd_src)

iem16: $(IEM16_DIR)/Make.config
	$(MAKE) -C $(IEM16_DIR)


iem16_install: iem16
	install -d $(DESTDIR)$(objectsdir)/$(IEM16_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEM16_NAME) \
		--author "IOhannes m zmoelnig" \
		--description "16bit table/array objects for low memory usage" \
		--license "GNU GPL" \
		--version "0.1"
	install -p $(IEM16_DIR)/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(IEM16_NAME)
	install -p $(externals_src)/iem16/help/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEM16_NAME)

iem16_clean:
	-$(MAKE) -C $(IEM16_DIR) clean
	-rm -f -- $(IEM16_DIR)/*.$(EXTENSION)
	-rm -f -- $(IEM16_DIR)/*.d
	-rm -rf -- $(IEM16_DIR)/autom4ate
	-rm -rf -- $(IEM16_DIR)/conf[0-9][0-9][0-9]*
	-rm -f -- $(IEM16_DIR)/configure
	-rm -f -- $(IEM16_DIR)/Make.config
	-rm -f -- $(IEM16_DIR)/*.o
	-rm -f -- $(IEM16_DIR)/*.bak
	-rm -f -- $(IEM16_DIR)/*.*~

#------------------------------------------------------------------------------#
# IEMGUTS
IEMGUTS_NAME=iemguts
IEMGUTS_OBJECTS := $(wildcard $(externals_src)/iem/iemguts/src/*.c)
iemguts: $(IEMGUTS_OBJECTS:.c=.$(EXTENSION))

iemguts_install: iemguts
	install -d $(DESTDIR)$(objectsdir)/$(IEMGUTS_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMGUTS_NAME) \
		--author "IOhannes m zmoelnig" \
		--description "IEMguts is a collection of objects that deal with the infrastructure to build better abstractions" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMGUTS_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEMGUTS_NAME)
	install -p $(externals_src)/iem/iemguts/help/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMGUTS_NAME)
	install -p $(externals_src)/iem/iemguts/README.txt \
		$(DESTDIR)$(objectsdir)/$(IEMGUTS_NAME)
	install -d $(DESTDIR)$(examplesdir)/$(IEMGUTS_NAME)
	install -p $(externals_src)/iem/iemguts/examples/*.pd \
		$(DESTDIR)$(examplesdir)/$(IEMGUTS_NAME)

iemguts_clean:
	-rm -f -- $(IEMGUTS_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(IEMGUTS_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/iem/iemguts/*.bak
	-rm -f -- $(externals_src)/iem/iemguts/*.*~



#------------------------------------------------------------------------------#
# IEMLIB
IEMLIB_NAME=iemlib
# omit the lib files and iem_mp3 (for patent reasons :( )
IEMLIB_SRC := $(wildcard $(externals_src)/iemlib/iemlib1/src/*[^1].c) $(wildcard $(externals_src)/iemlib/iemlib2/src/*[^2].c)  $(wildcard $(externals_src)/iemlib/iem_t3_lib/src/t3_*.c)  $(wildcard $(externals_src)/iemlib/alias/*.c)
IEMLIB_OBJECTS := $(IEMLIB_SRC:.c=.o)
IEMLIB_TARGETS := $(IEMLIB_SRC:.c=.$(EXTENSION))

$(IEMLIB_OBJECTS) : %.o : %.c
	$(CC) -I$(externals_src)/iemlib/include $(CFLAGS) -O2 -funroll-loops -fomit-frame-pointer -fno-tree-vectorize -fno-strict-aliasing -o "$*.o" -c "$*.c"

$(IEMLIB_TARGETS): $(IEMLIB_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o "$@" "$(@:.$(EXTENSION)=.o)"

iemlib: $(IEMLIB_TARGETS)

iemlib_install:
	install -d $(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMLIB_NAME) \
		--description "a collection of objects written at IEM/KUG" \
		--license "GNU GPL"
	install -p $(IEMLIB_SRC:.c=.$(EXTENSION)) \
		$(externals_src)/iemlib/*/*.pd \
		$(externals_src)/iemlib/*/*.mp3 \
		$(externals_src)/iemlib/*/*.wav \
		$(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)/examples
	install -p $(externals_src)/iemlib/examples/*.* \
		$(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)/examples
	install -d $(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)/manual
	install -p $(externals_src)/iemlib/*.pdf $(externals_src)/iemlib/*.txt \
		$(externals_src)/iemlib/iemabs/*.txt \
		$(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)/manual
# kludge to get a single working output~ straight in 'extra'
	-rm -f -- $(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)/output~.pd


iemlib_clean: 
	-rm -f -- $(IEMLIB_OBJECTS)
	-rm -f -- $(IEMLIB_SRC:.c=.$(EXTENSION))
	-rmdir -- $(DESTDIR)$(objectsdir)/$(IEMLIB_NAME)
	-rm -f -- $(DESTDIR)$(manualsdir)/$(IEMLIB_NAME)/*.*
	-rmdir -- $(DESTDIR)$(manualsdir)/$(IEMLIB_NAME)



#------------------------------------------------------------------------------#
# IEMMATRIX
## oh this is all a mess. please use iemmatrix's build-system instead
IEMMATRIX_NAME=iemmatrix
IEMMATRIX_ROOT := $(externals_src)/iem/iemmatrix
IEMMATRIX_SRC := $(wildcard $(IEMMATRIX_ROOT)/src/m[at]*.c)
IEMMATRIX_ALIAS := $(wildcard $(IEMMATRIX_ROOT)/alias/*.c)
IEMMATRIX_OBJ := $(IEMMATRIX_SRC:.c=.o) $(IEMMATRIX_ALIAS:.c=.o)
IEMMATRIX_SHARED := $(wildcard $(IEMMATRIX_ROOT)/src/iemmatrix_binops.c) \
                    $(wildcard $(IEMMATRIX_ROOT)/src/iemmatrix_utility.c)

iemmatrix: $(IEMMATRIX_OBJ:.o=.$(EXTENSION))

$(IEMMATRIX_OBJ:.o=.$(EXTENSION)) : %.$(EXTENSION) : $(IEMMATRIX_OBJ) $(IEMMATRIX_SHARED:.c=.o)
	$(CC) $(LDFLAGS) -o $*.$(EXTENSION) "$*.o" $(IEMMATRIX_SHARED:.c=.o) $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 $*.$(EXTENSION)
#	rm -f -- "$*.o"

$(IEMMATRIX_OBJ) $(IEMMATRIX_SHARED:.c=.o) : %.o : %.c
	$(CC) $(OPT_CFLAGS) $(CFLAGS) -o "$*.o" -c "$*.c" -I$(IEMMATRIX_ROOT)/src

iemmatrix_install: iemmatrix
	install -d $(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMMATRIX_NAME) \
		--author "IOhannes m zmoelnig (zmoelnig AT iem DOT at), thomas musil (musil AT iem DOT at), franz zotter (zotter AT iem DOT at)" \
		--description "objects for matrix operations and math" \
		--license "GNU GPL" \
		--version "$(IEMMATRIX_VERSION)"
	install -p $(IEMMATRIX_OBJ:.o=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)
	install -p $(shell ls -1 $(externals_src)/iem/iemmatrix/abs/*.* | \
		grep -v '\-help.pd') $(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)
	install -p $(externals_src)/iem/iemmatrix/doc/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)
	install -p $(externals_src)/iem/iemmatrix/abs/*-help.pd \
		$(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)/manual
	install -p $(externals_src)/iem/iemmatrix/*.txt \
		$(DESTDIR)$(objectsdir)/$(IEMMATRIX_NAME)/manual

iemmatrix_clean:
	-rm -f -- $(IEMMATRIX_OBJ:.o=.$(EXTENSION))
	-rm -f -- $(externals_src)/iem/iemmatrix/src/*.o
	-rm -f -- $(externals_src)/iem/iemmatrix/alias/*.o
	-rm -f -- $(externals_src)/iem/iemmatrix/src/*.bak
	-rm -f -- $(externals_src)/iem/iemmatrix/src/*.*~


#------------------------------------------------------------------------------#
# JASCH_LIB
JASCH_LIB_NAME=jasch_lib
JASCH_LIB_OBJECTS := $(wildcard $(externals_src)/jasch_lib/*/*.c)
jasch_lib: $(JASCH_LIB_OBJECTS:.c=.$(EXTENSION))

jasch_lib_install: jasch_lib
	install -d $(DESTDIR)$(objectsdir)/$(JASCH_LIB_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(JASCH_LIB_NAME) \
		--author "Jasch" \
		--license "GNU GPLv2" \
		--version ""
	install -p $(JASCH_LIB_OBJECTS:.c=.$(EXTENSION)) \
		$(DESTDIR)$(objectsdir)/$(JASCH_LIB_NAME)
	install -p $(externals_src)/jasch_lib/*/*-help.pd \
		$(DESTDIR)$(objectsdir)/$(JASCH_LIB_NAME)
#	install -d $(DESTDIR)$(manualsdir)/$(JASCH_LIB_NAME)
#	install -p $(externals_src)/jasch_lib/manual.txt \
#		$(DESTDIR)$(manualsdir)/$(JASCH_LIB_NAME)
#	install -p $(externals_src)/jasch_lib/README \
#		$(DESTDIR)$(objectsdir)/$(JASCH_LIB_NAME)
#	install -d $(DESTDIR)$(examplesdir)/$(JASCH_LIB_NAME)
#	install -p $(externals_src)/jasch_lib/examples/*.pd \
#		$(DESTDIR)$(examplesdir)/$(JASCH_LIB_NAME)

jasch_lib_clean:
	-rm -f -- $(JASCH_LIB_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(JASCH_LIB_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/jasch_lib/*/*.bak
	-rm -f -- $(externals_src)/jasch_lib/*/*.*~

#------------------------------------------------------------------------------#
# LOADERS-LIBDIR
loaders-libdir:
	make -C $(externals_src)/loaders/libdir PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

loaders-libdir_install:
	make -C $(externals_src)/loaders/libdir DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

loaders-libdir_clean:
	make -C $(externals_src)/loaders/libdir clean

#------------------------------------------------------------------------------#
# LYON
# Ivica named the libdir "lyon" for Pd-l2ork, but the submodule is named
# lyonpotpourri. So we must do extra work to differentiate the two...
LYON_SRC=lyonpotpourri
LYON_DEST=lyonpotpourri

lyonpotpourri:
ifeq ($(EMSCRIPTEN),yes)
	sed -i 's/--enable-new-dtags/-Bsymbolic/g' $(externals_src)/$(LYON_SRC)/Makefile
endif
	make -C $(externals_src)/$(LYON_SRC) $(call set_em_flags) PD_PATH=$(pd_src) CFLAGS="$(CFLAGS_GITHUB_RUNNER) $(CFLAGS_ADD) -Wno-incompatible-pointer-types"
ifeq ($(EMSCRIPTEN),yes)
	sed -i 's/-Bsymbolic/--enable-new-dtags/g' $(externals_src)/$(LYON_SRC)/Makefile
endif

lyonpotpourri_install:
	#make -C $(externals_src)/$(LYON_SRC) DESTDIR="$(DESTDIR)" \
	#	objectsdir="$(objectsdir)" install
	install -d $(DESTDIR)$(objectsdir)/$(LYON_DEST)
	install -p $(wildcard $(externals_src)/$(LYON_SRC)/*.$(EXTENSION)) \
		$(DESTDIR)$(objectsdir)/$(LYON_DEST)
	install -p $(externals_src)/$(LYON_SRC)/*.pd \
		$(DESTDIR)$(objectsdir)/$(LYON_DEST)
	install -d $(DESTDIR)$(objectsdir)/$(LYON_DEST)/sound
	install -p $(externals_src)/$(LYON_SRC)/examples/*.* \
		$(DESTDIR)$(objectsdir)/$(LYON_DEST)/sound
	# some of the help patches expect to find the sounds in the examples
	# subfolder instead of 'sound', so make sure that both are available
ifeq ($(OS_NAME),windows)
	# symbolic links don't work under Windows, just copy the folder instead
	rm -rf $(DESTDIR)$(objectsdir)/$(LYON_DEST)/examples
	cp -r $(DESTDIR)$(objectsdir)/$(LYON_DEST)/sound  $(DESTDIR)$(objectsdir)/$(LYON_DEST)/examples
else
	rm -f $(DESTDIR)$(objectsdir)/$(LYON_DEST)/examples
	ln -sf sound $(DESTDIR)$(objectsdir)/$(LYON_DEST)/examples
endif
	# don't include cartopol/poltocar-- cyclone library already has those
	rm $(DESTDIR)$(objectsdir)/$(LYON_DEST)/cartopol*.$(EXTENSION)
	rm $(DESTDIR)$(objectsdir)/$(LYON_DEST)/poltocar*.$(EXTENSION)
	rm $(DESTDIR)$(objectsdir)/$(LYON_DEST)/cartopol*.pd
	rm $(DESTDIR)$(objectsdir)/$(LYON_DEST)/poltocar*.pd
	# copy the meta file and the shared lib
	install -p $(externals_src)/$(LYON_SRC)/*lyonpotpourri*.* \
		$(DESTDIR)$(objectsdir)/$(LYON_DEST)
	# license and readme
	install -p $(externals_src)/$(LYON_SRC)/*.txt \
		$(DESTDIR)$(objectsdir)/$(LYON_DEST)

lyonpotpourri_clean:
	make -C $(externals_src)/$(LYON_SRC) clean

#------------------------------------------------------------------------------#
# MAPPING
mapping:
	make -C $(externals_src)/mapping PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

mapping_install:
	make -C $(externals_src)/mapping DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

mapping_clean:
	make -C $(externals_src)/mapping clean

#------------------------------------------------------------------------------#
# MARKEX
markex:
	make -C $(externals_src)/markex PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

markex_install:
	make -C $(externals_src)/markex DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

markex_clean:
	make -C $(externals_src)/markex clean

#------------------------------------------------------------------------------#
# MAXLIB
maxlib:
	make -C $(externals_src)/maxlib PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

maxlib_install:
	make -C $(externals_src)/maxlib DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

maxlib_clean:
	make -C $(externals_src)/maxlib clean


#------------------------------------------------------------------------------#
# MJLIB
mjlib:
	make -C $(externals_src)/mjlib PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

mjlib_install:
	make -C $(externals_src)/mjlib DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

mjlib_clean:
	make -C $(externals_src)/mjlib clean



#------------------------------------------------------------------------------#
# MOOCOW
MOOCOW_NAME=moocow
#MOOCOW_OBJECTS := $(shell cat $(externals_src)/moocow/extended/objects)
MOOCOW_DIR=$(externals_src)/moocow/extended
MOOCOW_BUILD=$(MOOCOW_DIR)/build.moo

##-- pass some variables on to sub-make
## + we should probably just use make's "export" for this,
##   maybe even exporting all variables by default...
MOOCOW_MAKEFLAGS = \
	CFLAGS="$(CFLAGS)" \
	pd_src="$(pd_src)" \
	$(MOOCOW_FLATPAK_FLAGS)

$(externals_src)/moocow/extended/build.stamp:
	$(MAKE) -C $(MOOCOW_DIR) $(MOOCOW_MAKEFLAGS) build.stamp \
	  || echo "moocow: WARNING: build failed"

moocow: $(MOOCOW_DIR)/build.stamp

moocow_install:
	install -d $(DESTDIR)$(objectsdir)/$(MOOCOW_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(MOOCOW_NAME) \
		--author "Bryan Jurish <moocow@bbaw.de>" \
		--description "moocow's externals" \
		--license "GNU GPL" \
		--version "CVS.`date +%Y-%m-%d`"
	install -p $(MOOCOW_BUILD)/ext*/*.$(EXTENSION) \
		$(DESTDIR)$(objectsdir)/$(MOOCOW_NAME) \
	  || echo 'moocow_install: WARNING: no library externals to install!'
	install -p $(MOOCOW_BUILD)/ext*/*.pd \
		$(DESTDIR)$(objectsdir)/$(MOOCOW_NAME) \
	  || echo 'moocow_install: WARNING: no library patches to install!'
	install -p $(MOOCOW_BUILD)/doc/5.reference/*.pd \
		$(DESTDIR)$(objectsdir)/$(MOOCOW_NAME) \
	  || echo 'moocow_install: WARNING: no help patches to install!'
#	install -d $(DESTDIR)$(manualsdir)/$(MOOCOW_NAME)
#	install -p $(externals_src)/moocow/manual.txt \
#		$(DESTDIR)$(manualsdir)/$(MOOCOW_NAME) \
#	  || echo 'moocow_install: WARNING: no manuals to install!'
	install -p $(MOOCOW_DIR)/README.txt \
		$(DESTDIR)$(objectsdir)/$(MOOCOW_NAME) \
	  || echo 'moocow_install: WARNING: no README to install!'
#	install -d $(DESTDIR)$(examplesdir)/$(MOOCOW_NAME)
#	install -p $(MOOCOW_BUILD)/examples/*.pd \
#		$(DESTDIR)$(examplesdir)/$(MOOCOW_NAME) \
#	  || echo "moocow_install: WARNING: no examples patches to install!"

moocow_clean:
	$(MAKE) -C $(externals_src)/moocow/extended distclean
	-rm -f -- $(MOOCOW_DIR)/*.bak
	-rm -f -- $(MOOCOW_DIR)/*.*~
	-rm -rf -- $(externals_src)/moocow/*/common
	-rm -rf -- $(externals_src)/moocow/*/a.out.dSYM/*
	cp -f $(MOOCOW_DIR)/../gfsm/gfsm/src/libgfsm/default/* $(MOOCOW_DIR)/../gfsm/gfsm/src/libgfsm/ 


#------------------------------------------------------------------------------#
# MOONLIB
moonlib:
	make -C $(externals_src)/moonlib PD_PATH=$(pd_src) CFLAGS="$(CFLAGS) -fno-strict-aliasing" $(call set_em_flags)

moonlib_install:
	make -C $(externals_src)/moonlib DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

moonlib_clean:
	make -C $(externals_src)/moonlib clean



#------------------------------------------------------------------------------#
# MOTEX
motex:
	make -C $(externals_src)/motex PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

motex_install:
	make -C $(externals_src)/motex DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

motex_clean:
	make -C $(externals_src)/motex clean





#------------------------------------------------------------------------------#
# MRPEACH
MRPEACH_NAME=mrpeach
MRPEACH_OBJECTS := $(wildcard $(externals_src)/mrpeach/*/*.c)
mrpeach: WARN_FLAGS=-Wno-pedantic-ms-format
mrpeach: $(MRPEACH_OBJECTS:.c=.$(EXTENSION))

mrpeach_install: mrpeach
	install -d $(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(MRPEACH_NAME) \
		--author "Martin Peach <martin.peach@sympatico.ca>" \
		--description "" \
		--license "GNU GPL" \
		--version "0.1"
	install -p $(MRPEACH_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)
	install -p $(externals_src)/mrpeach/*/*.pd \
		$(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)
	install -p $(externals_src)/mrpeach/midifile/I_Wanna_Be_Sedated.mid \
		$(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)/examples
	install -p $(externals_src)/mrpeach/net/examples/test.txt \
		$(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)/examples
	$(call if_emscripten, -rm -f $(DESTDIR)$(objectsdir)/$(MRPEACH_NAME)/which*)

mrpeach_clean:
	-rm -f -- $(MRPEACH_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(MRPEACH_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/mrpeach/*.bak
	-rm -f -- $(externals_src)/mrpeach/*.*~


#----------------------------------------------------------------------------
# OSCx
OSCX_NAME=oscx
ifneq ($(EMSCRIPTEN),yes)
$(externals_src)/OSCx/configure: $(externals_src)/OSCx/configure.ac
	cd $(externals_src)/OSCx && autoconf

$(externals_src)/OSCx/Makefile: $(externals_src)/OSCx/Makefile.in
	cd $(externals_src)/OSCx && ./configure $(OSCX_FLATPAK_CONFIGURE_FLAGS)
$(externals_src)/OSCx/libOSC/Makefile: $(externals_src)/OSCx/libOSC/Makefile.in
	cd $(externals_src)/OSCx && ./configure $(OSCX_FLATPAK_CONFIGURE_FLAGS)
$(externals_src)/OSCx/src/Makefile: $(externals_src)/OSCx/src/Makefile.in
	cd $(externals_src)/OSCx && ./configure $(OSCX_FLATPAK_CONFIGURE_FLAGS)

$(externals_src)/OSCx/src/OSCroute.$(EXTENSION):  $(externals_src)/OSCx/configure \
$(externals_src)/OSCx/Makefile
	$(MAKE) -C $(externals_src)/OSCx
$(externals_src)/OSCx/src/dumpOSC.$(EXTENSION):  $(externals_src)/OSCx/configure \
$(externals_src)/OSCx/Makefile
	$(MAKE) -C $(externals_src)/OSCx
$(externals_src)/OSCx/src/sendOSC.$(EXTENSION):  $(externals_src)/OSCx/configure \
$(externals_src)/OSCx/Makefile
	$(MAKE) -C $(externals_src)/OSCx

oscx: $(externals_src)/OSCx/src/OSCroute.$(EXTENSION) \
$(externals_src)/OSCx/src/dumpOSC.$(EXTENSION) \
$(externals_src)/OSCx/src/sendOSC.$(EXTENSION)

oscx_install: oscx
	install -d $(DESTDIR)$(objectsdir)/$(OSCX_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(OSCX_NAME) \
		--author "<jdl@xdv.org>" \
		--license "BSD" \
		--description "objects for working with OpenSoundControl"
	install -p $(externals_src)/OSCx/src/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(OSCX_NAME)
	install -p $(externals_src)/OSCx/doc/*.* $(DESTDIR)$(objectsdir)/$(OSCX_NAME)


oscx_clean:
	-$(MAKE) -C $(externals_src)/OSCx $(DEST_PATHS) clean
	-$(MAKE) -C $(externals_src)/OSCx CC=gcc clean
	-rm $(externals_src)/OSCx/Makefile
	-rm $(externals_src)/OSCx/configure
	-rm -f -- $(cvs_root_dir)/externals/OSCx/*/Makefile
else
OSCX_SRC := $(wildcard $(externals_src)/OSCx/src/*.c)
OSCX_OBJECTS := $(OSCX_SRC:.c=.o)
OSCX_FLAGS := -I$(externals_src)/OSCx/libOSC -D__APPLE__ -I${externals_src}/OSCx/libOSC -I${externals_src}/OSCx/src
OSCX_TARGETS := $(OSCX_SRC:.c=.$(EXTENSION))

oscx: $(OSCX_TARGETS)

$(OSCX_OBJECTS): $(OSCX_SRC)
	@if [ "$(notdir $*)" = "dumpOSC" ]; then \
		$(CC) $(CFLAGS) -I$(externals_src)/OSCx/libOSC -I$(externals_src)/OSCx/libOSC -I$(externals_src)/OSCx/src -o "$*.o" -c "$*.c"; \
	else \
		$(CC) $(CFLAGS) $(OSCX_FLAGS) -o "$*.o" -c "$*.c"; \
	fi

$(OSCX_TARGETS): $(OSCX_OBJECTS)
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o" $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 "$*.$(EXTENSION)"
	
oscx_clean:
	rm -f $(OSCX_TARGETS)
	rm -f $(OSCX_OBJECTS)

oscx_install: $(OSCX_TARGETS)
	install -d $(DESTDIR)$(objectsdir)/$(OSCX_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(OSCX_NAME) \
		--author "<jdl@xdv.org>" \
		--license "BSD" \
		--description "objects for working with OpenSoundControl"
	install -p $(externals_src)/OSCx/src/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(OSCX_NAME)
	install -p $(externals_src)/OSCx/doc/*.* $(DESTDIR)$(objectsdir)/$(OSCX_NAME)
endif 




#------------------------------------------------------------------------------#
# PAN
pan:
	make -C $(externals_src)/pan PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

pan_install:
	make -C $(externals_src)/pan DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

pan_clean:
	make -C $(externals_src)/pan clean


#------------------------------------------------------------------------------#
# PDDP
pddp:
	make -C $(externals_src)/pddp PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

pddp_install:
	make -C $(externals_src)/pddp DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

pddp_clean:
	make -C $(externals_src)/pddp clean


#------------------------------------------------------------------------------#
# pdjs

js:
ifneq ($(EMSCRIPTEN),yes)
ifeq ($(OS_NAME),windows)
	cd $(externals_src)/pdjs-win/v8 && unzip -u lib.zip
ifeq ($(MSYSTEM),MINGW32)
	cd $(externals_src)/pdjs-win \
			&& $(msys_prefix)/bin/cmake -G Ninja -DVERSION=1.0 -DCMAKE_BUILD_TYPE=Debug \
				-B out/build/x86-mingw-Debug/ -S . \
			&& $(msys_prefix)/bin/cmake --build out/build/x86-mingw-Debug -- -v
else
	cd $(externals_src)/pdjs-win \
		&& $(msys_prefix)/bin/cmake -G Ninja -DVERSION=1.0 -DCMAKE_BUILD_TYPE=Debug \
			-B out/build/x64-mingw-Debug/ -S . \
		&& $(msys_prefix)/bin/cmake --build out/build/x64-mingw-Debug -- -v
endif
endif
ifeq ($(OS_NAME),darwin)
	cd $(externals_src)/pdjs/v8 && unzip -u lib.zip
	cd $(externals_src)/pdjs \
		&& cmake -G Ninja -DVERSION=1.0 -DCMAKE_BUILD_TYPE=Debug \
			-B out/build/x64-macos-Debug/ -S . \
		&& cmake --build out/build/x64-macos-Debug -- -v
endif
ifeq ($(OS_NAME),linux)
	cd $(externals_src)/pdjs/v8 && unzip -u lib.zip
ifeq ($(CPU),aarch64)
	cd $(externals_src)/pdjs/v8/lib && ln -sf arm-linux $(shell uname -m)-linux
	cd $(externals_src)/pdjs \
		&& cmake -G Ninja -DVERSION=1.0 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=arm64-linux.cmake \
			-B out/build/x64-linux-Debug/ -S . \
		&& cmake --build out/build/x64-linux-Debug -- -v
else
	cd $(externals_src)/pdjs \
		&& cmake -G Ninja -DVERSION=1.0 -DCMAKE_BUILD_TYPE=Debug \
			-B out/build/x64-linux-Debug/ -S . \
		&& cmake --build out/build/x64-linux-Debug -- -v
endif
endif

js_install:
	install -d $(DESTDIR)$(objectsdir)/js
ifeq ($(OS_NAME),windows)
ifeq ($(MSYSTEM),MINGW32)
	install -p $(externals_src)/pdjs-win/binaries/x86-windows/js.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs-win/binaries/x86-windows/*.pd \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs-win/binaries/x86-windows/*.js \
		$(DESTDIR)/$(objectsdir)/js
else
	install -p $(externals_src)/pdjs-win/binaries/x64-windows/js.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs-win/binaries/x64-windows/*.pd \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs-win/binaries/x64-windows/*.js \
		$(DESTDIR)/$(objectsdir)/js
endif
endif
ifeq ($(OS_NAME),darwin)
ifeq ($(shell uname -m),arm64)
	install -p $(externals_src)/pdjs/binaries/arm64-macos/js.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/arm64-macos/*.pd \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/arm64-macos/*.js \
		$(DESTDIR)/$(objectsdir)/js
else
	install -p $(externals_src)/pdjs/binaries/x64-macos/js.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/x64-macos/*.pd \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/x64-macos/*.js \
		$(DESTDIR)/$(objectsdir)/js
endif
endif
ifeq ($(OS_NAME),linux)
ifeq ($(CPU),aarch64)
	install -p $(externals_src)/pdjs/binaries/arm*-linux/js.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/arm*-linux/*.pd \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/arm*-linux/*.js \
		$(DESTDIR)/$(objectsdir)/js
else
	install -p $(externals_src)/pdjs/binaries/x64-linux/js.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/x64-linux/*.pd \
		$(DESTDIR)/$(objectsdir)/js
	install -p $(externals_src)/pdjs/binaries/x64-linux/*.js \
		$(DESTDIR)/$(objectsdir)/js
endif
endif

js_clean:
ifeq ($(OS_NAME),windows)
	cd $(externals_src)/pdjs-win \
		&& $(msys_prefix)/bin/cmake --build out/build/x86-mingw-Debug --target clean
	cd $(externals_src)/pdjs-win/v8 && rm -rf lib
	cd $(externals_src)/pdjs-win && rm -rf binaries
endif
ifeq ($(OS_NAME),darwin)
	cd $(externals_src)/pdjs \
		&& cmake --build out/build/x64-macos-Debug --target clean
	cd $(externals_src)/pdjs/v8 && rm -rf lib
	cd $(externals_src)/pdjs && rm -rf binaries
	cd $(externals_src)/pdjs/out/build/x64-macos-Debug && rm -rf * && rm -rf ./.ninja*
endif
ifeq ($(OS_NAME),linux)
	cd $(externals_src)/pdjs \
		&& cmake --build out/build/x64-linux-Debug --target clean
	cd $(externals_src)/pdjs/v8 && rm -rf lib
	cd $(externals_src)/pdjs && rm -rf binaries
	cd $(externals_src)/pdjs && rm -rf binaries
	cd $(externals_src)/pdjs/out/build/x64-linux-Debug && rm -rf * && rm -rf ./.ninja*
endif
else
js:

js_install:
	install -d $(DESTDIR)$(objectsdir)/pd-js
	install -p $(externals_src)/pdjs/pdjs/js.pd $(DESTDIR)$(objectsdir)/pd-js
endif

#------------------------------------------------------------------------------#
# PDLUA

LUA_CFLAGS = $(shell (pkg-config --cflags lua5.4 || pkg-config --cflags lua5.3 || pkg-config --cflags lua5.2 || pkg-config --cflags lua) 2> /dev/null)
PDLUA_CFLAGS = -DPURR_DATA -DGLIST_GRAB_COMPAT $(CFLAGS)
LUA_LIBS = $(shell (pkg-config --libs lua5.4 || pkg-config --libs lua5.3 || pkg-config --libs lua5.2 || pkg-config --libs lua) 2> /dev/null)
LUA_TARGET = $(externals_src)/pd-lua/pdlua.$(EXTENSION)

pdlua: $(LUA_TARGET)

$(LUA_TARGET):
	rm -f pd-lua/pdlua.o
ifneq ($(EMSCRIPTEN),yes)
	make -C $(externals_src)/pd-lua LUA_CFLAGS="$(LUA_CFLAGS)" LUA_LIBS="$(LUA_LIBS)" PD_PATH="$(pd_src)" PD_LIB="$(pd_src)/src" CFLAGS="$(PDLUA_CFLAGS)" PDBINDIR=$(pd_src)/src
else
	if [ ! -d $(externals_src)/pd-lua/lua ]; then git clone https://github.com/lua/lua.git $(externals_src)/pd-lua/lua; fi
	$(CC) $(externals_src)/pd-lua/pdlua.c $(externals_src)/pd-lua/lua/onelua.c -o $(LUA_TARGET) $(CFLAGS) $(LDFLAGS) -I$(externals_src)/pd-lua/lua $(PDLUA_CFLAGS)
endif

ifneq ($(EMSCRIPTEN),yes)
pdlua_install: pdlua
	make -C $(externals_src)/pd-lua LUA_CFLAGS="$(LUA_CFLAGS)" LUA_LIBS="$(LUA_LIBS)" PDINCLUDEDIR="$(pd_src)/src" PD_PATH="$(pd_src)" PD_LIB="$(pd_src)/src" CFLAGS="$(CFLAGS)" PDBINDIR=$(pd_src)/src DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

pdlua_clean:
	make -C $(externals_src)/pd-lua clean
else
pdlua_install: pdlua
	install -d $(DESTDIR)$(objectsdir)/pd-lua
	cp -r ${externals_src}/pd-lua/pdlua $(DESTDIR)$(objectsdir)/pd-lua
	cp -r ${externals_src}/pd-lua/*.pd $(DESTDIR)$(objectsdir)/pd-lua
	cp -r ${externals_src}/pd-lua/*.lua $(DESTDIR)$(objectsdir)/pd-lua
	cp -r ${externals_src}/pd-lua/*.${EXTENSION} $(DESTDIR)$(objectsdir)/pd-lua

pdlua_clean:
	rm -f $(LUA_TARGET)
	rm -rf $(externals_src)/pd-lua/lua
endif


#------------------------------------------------------------------------------#
# PDOGG
pdogg:
ifneq ($(EMSCRIPTEN),yes)
	make -C $(externals_src)/pdogg PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
else
	make -C $(externals_src)/pdogg PD_PATH=$(pd_src) CFLAGS="$(CFLAGS) -s USE_OGG=1 -s USE_VORBIS=1" OPT_CFLAGS="-funroll-loops -fomit-frame-pointer" LDFLAGS="$(LDFLAGS) -s USE_OGG=1 -s USE_VORBIS=1 -lvorbis" ALL_LIBS="" LIBS="$(LIBS)" CC="$(CC)" CXX="$(CXX)" EXTENSION="$(EXTENSION)"
endif

pdogg_install:
	make -C $(externals_src)/pdogg DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

pdogg_clean:
	make -C $(externals_src)/pdogg clean



#------------------------------------------------------------------------------#
# PDP
ifeq ($(ARCH),i386)
  PDP_OPTIONS = --enable-quicktime --enable-mmx
else
  PDP_OPTIONS = --enable-quicktime
endif

PDP_NAME=pdp
$(externals_src)/pdp/configure: $(externals_src)/pdp/configure.ac
	cd $(externals_src)/pdp && autoconf

$(externals_src)/pdp/Makefile.config: $(externals_src)/pdp/configure $(externals_src)/pdp/Makefile.config.in
	cd $(externals_src)/pdp && ./configure PD_CPPFLAGS="-I$(pd_src)/src -I$(usrlocal)/include" \
		$(PDP_OPTIONS)

$(externals_src)/pdp/pdp.$(EXTENSION): $(externals_src)/pdp/Makefile.config 
	$(MAKE) -C $(externals_src)/pdp PD_EXECUTABLE="$(pd_src)/bin/pd"

pdp: $(externals_src)/pdp/pdp.$(EXTENSION)

pdp_install: pdp
	install -d $(DESTDIR)$(objectsdir)/$(PDP_NAME)
	install -p $(externals_src)/pdp/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(PDP_NAME)
	install -p $(externals_src)/pdp/abstractions/*.pd $(DESTDIR)$(objectsdir)/$(PDP_NAME)
	install -p $(externals_src)/pdp/doc/objects/*.* $(DESTDIR)$(objectsdir)/$(PDP_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(PDP_NAME)/manual
	install -p $(externals_src)/pdp/doc/reference.txt \
		$(DESTDIR)$(objectsdir)/$(PDP_NAME)/manual
	install -d $(DESTDIR)$(objectsdir)/$(PDP_NAME)/examples
	install -p $(externals_src)/pdp/doc/introduction/*.* \
		$(DESTDIR)$(objectsdir)/$(PDP_NAME)/examples
	install -p $(externals_src)/pdp/doc/examples/*.* \
		$(DESTDIR)$(objectsdir)/$(PDP_NAME)/examples
ifeq ($(OS_NAME),darwin)
# switch Linux-only pdp_v4l to Mac-only pdp_ieee1394
	sed -i.bak 's|pdp_v4l|pdp_ieee1394|' $(DESTDIR)$(objectsdir)/$(PDP_NAME)/*.pd
	rm -f --  $(DESTDIR)$(objectsdir)/$(PDP_NAME)/*.pd.bak
	sed -i.bak 's|pdp_v4l|pdp_ieee1394|' $(DESTDIR)$(objectsdir)/$(PDP_NAME)/examples/*.pd
	rm -f --  $(DESTDIR)$(objectsdir)/$(PDP_NAME)/examples/*.pd.bak
endif

pdp_clean:
	-rm -f -- $(externals_src)/pdp/*.$(EXTENSION)
	-find $(externals_src)/pdp -name '*.o' | xargs rm -f --
	-find $(externals_src)/pdp -name '*.bak' | xargs rm -f --
	-rm -f -- $(externals_src)/pdp/Makefile.config
	-rm -f -- $(externals_src)/pdp/configure
	-rm -f -- $(externals_src)/pdp/include/pdp_config.h


#------------------------------------------------------------------------------#
# PDP_OPENGL
PDP_OPENGL_NAME=3dp
PDP_OPENGL_BINARY := $(externals_src)/pdp/opengl/pdp_opengl.$(EXTENSION)
$(PDP_OPENGL_BINARY): $(externals_src)/pdp/Makefile.config
	make -C $(externals_src)/pdp/opengl

pdp_opengl: $(PDP_OPENGL_BINARY)
	echo $(PDP_OPENGL_BINARY)

pdp_opengl_install: pdp_opengl
	install -d $(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(PDP_OPENGL_NAME) \
		--author "Tom Schouten" \
		--description "opengl extensions for pdp" \
		--license "GPL v2 or later"
	install -p $(PDP_OPENGL_BINARY) $(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)
	install -p $(externals_src)/pdp/opengl/abstractions/*.pd \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)
	install -p $(externals_src)/pdp/opengl/doc/objects/*.pd \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)
	install -p $(externals_src)/pdp/opengl/README \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/README.txt
	install -d $(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/examples
	install -p $(externals_src)/pdp/opengl/doc/examples/*.pd \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/examples
	install -p $(externals_src)/pdp/opengl/test/*.pd \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/examples
ifeq ($(OS_NAME),darwin)
# switch Linux-only pdp_v4l to Mac-only pdp_ieee1394
	sed -i.bak 's|pdp_v4l|pdp_ieee1394|' \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/*.pd
	rm -f --  $(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/*.pd.bak
	sed -i.bak 's|pdp_v4l|pdp_ieee1394|' \
		$(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/examples/*.pd
	rm -f --  $(DESTDIR)$(objectsdir)/$(PDP_OPENGL_NAME)/examples/*.pd.bak
endif

pdp_opengl_clean:
	make -C $(externals_src)/pdp/opengl clean
	-rm -f -- $(PDP_OPENGL_BINARY)
	-rm -f -- $(externals_src)/pdp_opengl/*.bak
	-rm -f -- $(externals_src)/pdp_opengl/*.*~


#------------------------------------------------------------------------------#
# PIDIP
PIDIP_NAME=pidip
$(externals_src)/pidip/configure: $(externals_src)/pidip/configure.ac
	cd $(externals_src)/pidip && autoconf

$(externals_src)/pidip/Makefile: $(externals_src)/pidip/Makefile.in
	-cd $(externals_src)/pidip && ./configure --with-pd=$(pd_src) \
		--with-pdp=$(externals_src)/pdp

$(externals_src)/pidip/pidip.$(EXTENSION):  $(externals_src)/pidip/configure \
$(externals_src)/pidip/Makefile
	-$(MAKE) -C $(externals_src)/pidip

pidip: $(externals_src)/pidip/pidip.$(EXTENSION)

pidip_install: pidip
	install -d $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)
#	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(PIDIP_NAME) \
#		--author "Yves Degoyon" \
#		--description "PiDiP is Definitely in Pieces" \
#		--license "GNU GPLv2"
	install -p $(externals_src)/pidip/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)
	install -p $(externals_src)/pidip/doc/*.pd $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples
	install -p $(externals_src)/pidip/patches/*.* \
		$(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples
	install -p $(externals_src)/pidip/README \
		$(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/README.txt
	install -d $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples/images
	install -p $(externals_src)/pidip/patches/images/*.* \
		$(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples/images
	install -d $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples/morphology
	install -p $(externals_src)/pidip/patches/morphology/*.* \
		$(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples/morphology
ifeq ($(OS_NAME),darwin)
# switch Linux-only pdp_v4l to Mac-only pdp_ieee1394
	sed -i.bak 's|pdp_v4l|pdp_ieee1394|' $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/*.pd
	rm -f --  $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/*.pd.bak
	sed -i.bak 's|pdp_v4l|pdp_ieee1394|' $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples/*.pd
	rm -f --  $(DESTDIR)$(objectsdir)/$(PIDIP_NAME)/examples/*.pd.bak
endif

pidip_clean:
	-rm -f -- $(externals_src)/pidip/*.$(EXTENSION)
	-find $(externals_src)/pidip -name '*.o' | xargs rm -f --
	-rm -f -- $(externals_src)/pidip/*.bak
	-rm -f -- $(externals_src)/pidip/Makefile
	-rm -f -- $(externals_src)/pidip/configure


#------------------------------------------------------------------------------#
# PLUGIN
plugin:
	make -C $(externals_src)/plugin~ PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

plugin_install:
	make -C $(externals_src)/plugin~ DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

plugin_clean:
	make -C $(externals_src)/plugin~ clean


#------------------------------------------------------------------------------#
# PMPD
pmpd:
	make -C $(externals_src)/pmpd PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

pmpd_install:
	make -C $(externals_src)/pmpd DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

pmpd_clean:
	make -C $(externals_src)/pmpd clean


#------------------------------------------------------------------------------#
# POSTLUDE
POSTLUDE_NAME=postlude
# flib is separate, so exclude it here
POSTLUDE_OBJECTS := $(wildcard $(externals_src)/postlude/[a-eg-z]*/src/*.c)
#	$(externals_src)/postlude/psql/psql.c
postlude: $(POSTLUDE_OBJECTS:.c=.$(EXTENSION))

postlude_install: postlude
	install -d $(DESTDIR)$(objectsdir)/$(POSTLUDE_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(POSTLUDE_NAME) \
		--author "Jamie Bullock" \
		--license "GNU GPL"
	install -p $(POSTLUDE_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(POSTLUDE_NAME)
	install -p $(externals_src)/postlude/dssi/doc/output~.pd \
		$(DESTDIR)$(objectsdir)/$(POSTLUDE_NAME)
	install -p $(externals_src)/postlude/dssi/README \
		$(DESTDIR)$(objectsdir)/$(POSTLUDE_NAME)/README-dssi.txt
	install -p $(externals_src)/postlude/psql/README \
		$(DESTDIR)$(objectsdir)/$(POSTLUDE_NAME)README-psql.txt

postlude_clean:
	-rm -f -- $(POSTLUDE_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(POSTLUDE_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/postlude/*/*.bak
	-rm -f -- $(externals_src)/postlude/*/*.*~

#------------------------------------------------------------------------------#
# RJLIB

rjlib:

rjlib_install:
	install -d $(DESTDIR)$(objectsdir)/rjlib
	cp -rf $(externals_src)/rjlib/* $(DESTDIR)/$(objectsdir)/rjlib

rjlib_clean:

#------------------------------------------------------------------------------#
# RTCMIX~

rtcmix:
	cd $(externals_src)/rtcmix-in-pd/RTcmix-pd-4.0.1.6 \
		&& ./configure && make
	make -C $(externals_src)/rtcmix-in-pd \
		PD_PATH=$(pd_src) \
		CFLAGS="$(CFLAGS)" \
		LINUXINCLUDE=-I$(pd_src)/src

rtcmix_install:
	install -d $(DESTDIR)$(objectsdir)/rtcmix
	install -p $(externals_src)/rtcmix-in-pd/rtcmix~.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/rtcmix
	install -p $(externals_src)/rtcmix-in-pd/rtcmix~-help.pd \
		$(DESTDIR)/$(objectsdir)/rtcmix
	cp -rf $(externals_src)/rtcmix-in-pd/lib \
		$(DESTDIR)/$(objectsdir)/rtcmix
	cp -rf $(externals_src)/rtcmix-in-pd/scores \
		$(DESTDIR)/$(objectsdir)/rtcmix

rtcmix_clean:
	make -C $(externals_src)/rtcmix-in-pd clean
	make -C $(externals_src)/rtcmix-in-pd/RTcmix*

#------------------------------------------------------------------------------#
# Sensel

sensel:
ifeq ($(OS_NAME),linux)
ifeq ($(CPU),aarch64)
	cd $(externals_src)/sensel \
		&& cp linux-arm-x86/* ./
else
	cd $(externals_src)/sensel \
		&& cp linux-x64/* ./
endif
	cd $(externals_src)/sensel \
		&& patchelf --force-rpath --set-rpath '$$ORIGIN/' libsensel.so
endif
	cd $(externals_src)/sensel \
		&& make
ifeq ($(OS_NAME),darwin)
	@echo "adjusting linker for the darwin version of the sensel external..."
	cd $(externals_src)/sensel && \
		install_name_tool -change \
		/usr/local/lib/libSensel.dylib @loader_path/libSensel.dylib sensel.pd_darwin
endif

sensel_install:
	install -d $(DESTDIR)$(objectsdir)/sensel
	install -p $(externals_src)/sensel/sensel.$(EXTENSION) \
		$(DESTDIR)/$(objectsdir)/sensel
	install -p $(externals_src)/sensel/*.pd \
		$(DESTDIR)/$(objectsdir)/sensel
	cp -rf $(externals_src)/sensel/sensel_examples \
		$(DESTDIR)/$(objectsdir)/sensel
ifeq ($(OS_NAME),darwin)
	echo "installing darwin sensel libraries..."
	cp -f $(externals_src)/sensel/*dylib \
		$(DESTDIR)/$(objectsdir)/sensel
endif
ifeq ($(OS_NAME),linux)
	echo "installing linux sensel libraries..."
	cp -f $(externals_src)/sensel/*so \
		$(DESTDIR)/$(objectsdir)/sensel
endif
ifeq ($(OS_NAME),windows)
	echo "installing windows sensel libraries..."
	cp -f $(externals_src)/sensel/*dll \
		$(DESTDIR)/$(objectsdir)/sensel
	cp -f $(externals_src)/sensel/win-x86/* \
		$(DESTDIR)/$(objectsdir)/sensel
endif

sensel_clean:
	make -C $(externals_src)/sensel clean

#------------------------------------------------------------------------------#
# SIGPACK
sigpack:
	make -C $(externals_src)/sigpack PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

sigpack_install:
	make -C $(externals_src)/sigpack DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

sigpack_clean:
	make -C $(externals_src)/sigpack clean



#------------------------------------------------------------------------------#
# SMLIB
smlib:
	make -C $(externals_src)/smlib PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

smlib_install:
	make -C $(externals_src)/smlib DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

smlib_clean:
	make -C $(externals_src)/smlib clean


#------------------------------------------------------------------------------#
# TCLPD
TCLPD_NAME=tclpd
TCLPD_LIB := $(externals_src)/tclpd/tcl.$(EXTENSION)

$(TCLPD_LIB):
	cd $(externals_src)/tclpd && make

tclpd: $(TCLPD_LIB)

tclpd_install: tclpd
	install -d $(DESTDIR)$(objectsdir)
	install -p $(TCLPD_LIB) $(DESTDIR)$(objectsdir)
	install -p $(externals_src)/tclpd/README \
		$(DESTDIR)$(objectsdir)/$(TCLPD_NAME)
#	install -d $(DESTDIR)$(examplesdir)/$(TCLPD_NAME)
#	install -p $(externals_src)/tclpd/examples/*.pd \
#		$(DESTDIR)$(examplesdir)/$(TCLPD_NAME)

tclpd_clean:
	make -C $(externals_src)/tclpd clean
	-rm -f -- $(TCLPD_LIB)
	-rm -f -- $(externals_src)/tclpd/*.bak
	-rm -f -- $(externals_src)/tclpd/*.*~


#------------------------------------------------------------------------------#
# TKWIDGETS
tkwidgets:
	make -C $(externals_src)/tkwidgets PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)"

tkwidgets_install:
	make -C $(externals_src)/tkwidgets \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

tkwidgets_clean:
	make -C $(externals_src)/tkwidgets clean

#------------------------------------------------------------------------------#
# TOF
TOF_NAME=tof
TOF_OBJECTS := $(wildcard $(externals_src)/tof/src/*.c)
tof: $(TOF_OBJECTS:.c=.$(EXTENSION))

tof_install: tof
	install -d $(DESTDIR)$(objectsdir)/$(TOF_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(TOF_NAME) \
		--author "Thomas Ouellet Fredericks" \
		--description "Various utilities" \
		--license "Whatever Hans wants" \
		--version "2009-09-22"
	install -p $(TOF_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(TOF_NAME)
	install -p $(externals_src)/tof/tof/*.gif \
		$(DESTDIR)$(objectsdir)/$(TOF_NAME)
	install -p $(externals_src)/tof/tof/*.pd \
		$(DESTDIR)$(objectsdir)/$(TOF_NAME)

tof_clean:
	-rm -f -- $(TOF_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(TOF_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/tof/src/*.bak
	-rm -f -- $(externals_src)/tof/src/*.*~

#------------------------------------------------------------------------------#
# TOXY
TOXY_NAME=toxy
# toxy is compiled straight into $(OUT_DIR)
TOXY_OUT_DIR=$(DESTDIR)$(objectsdir)/$(TOXY_NAME)
toxy: $(bindir)
	$(MAKE) OPT_CFLAGS="-O2 -fno-strict-aliasing -fPIC" \
		-C $(externals_src)/miXed/toxy


toxy_install: toxy
	install -d $(DESTDIR)$(objectsdir)/$(TOXY_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(TOXY_NAME) \
		--author "Kzrysztof Czaja" \
		--license "BSD" \
		--description "objects for working with Tcl and Pd's Tk GUI"
	cd $(externals_src)/miXed/toxy && $(MAKE) OUT_DIR=$(TOXY_OUT_DIR)
	install -p $(externals_src)/miXed/doc/help/toxy/*.*  \
		$(DESTDIR)$(objectsdir)/$(TOXY_NAME)
#	install -d $(DESTDIR)$(manualsdir)/$(TOXY_NAME)
	install -d $(DESTDIR)$(examplesdir)/$(TOXY_NAME)
	install -p $(externals_src)/miXed/test/toxy/*.*  \
		$(DESTDIR)$(examplesdir)/$(TOXY_NAME)


toxy_clean:
	-$(MAKE) -C $(externals_src)/mixed/toxy $(DEST_PATHS) clean


#------------------------------------------------------------------------------#
# UNAUTHORIZED
unauthorized:
	make -C $(externals_src)/unauthorized CFLAGS="$(CFLAGS)" \
		PD_PATH=$(pd_src) PD_INCLUDE=$(pd_src)/src $(call set_em_flags)

unauthorized_install:
	make -C $(externals_src)/unauthorized \
		DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install
	$(call if_emscripten, -rm -f $(DESTDIR)$(objectsdir)/unauthorized/mp3cast~* $(DESTDIR)$(objectsdir)/unauthorized/mp3streamout~* $(DESTDIR)$(objectsdir)/unauthorized/mp3write~* $(DESTDIR)$(objectsdir)/unauthorized/speexin~* $(DESTDIR)$(objectsdir)/unauthorized/speexout~*)

unauthorized_clean:
	make -C $(externals_src)/unauthorized clean


#------------------------------------------------------------------------------#
# VBAP
vbap:
	make -C $(externals_src)/vbap PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)" $(call set_em_flags)

vbap_install:
	make -C $(externals_src)/vbap DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" $(call set_em_flags) install

vbap_clean:
	make -C $(externals_src)/vbap clean



#------------------------------------------------------------------------------#
# VANILLA
vanilla:
	make -C $(externals_src)/vanilla PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)"

vanilla_install:
	make -C $(externals_src)/vanilla DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

vanilla_clean:
	make -C $(externals_src)/vanilla clean


#------------------------------------------------------------------------------#
# WINDOWING
windowing:
	make -C $(externals_src)/windowing PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)"

windowing_install:
	make -C $(externals_src)/windowing DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

windowing_clean:
	make -C $(externals_src)/windowing clean



#------------------------------------------------------------------------------#
# Zexy
ifneq ($(EMSCRIPTEN),yes)
ZEXY_NAME = zexy
zexy_src := $(externals_src)/$(ZEXY_NAME)

zexy: $(zexy_src)/Makefile
	$(MAKE) -C $(zexy_src) PDINCLUDEDIR=$(pd_src)/src PDBINDIR=$(pd_src)/src

zexy_install: zexy
	$(MAKE) -C $(zexy_src) DESTDIR=$(DESTDIR) prefix=$(prefix) \
		libdir=$(objectsdir) pkglibdir=$(objectsdir) install

zexy_clean:
	cd $(externals_src)/zexy
	-$(MAKE) -C $(externals_src)/zexy clean
	-rm -f -- $(externals_src)/zexy/src/*.o
else
ZEXY_NAME = zexy
ZEXY_SRC := $(wildcard $(externals_src)/zexy/src/*.c)
ZEXY_OBJECTS := $(ZEXY_SRC:.c=.o)
ZEXY_TARGETS := $(ZEXY_SRC:.c=.$(EXTENSION))
ZEXY_FLAGS := -DZ_WANT_LPT=0

zexy: $(ZEXY_TARGETS)

$(ZEXY_OBJECTS): $(ZEXY_SRC)
	$(CC) $(CFLAGS) $(ZEXY_FLAGS) -o "$*.o" -c "$*.c";

$(ZEXY_TARGETS): $(ZEXY_OBJECTS)
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o" $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 "$*.$(EXTENSION)"
	
zexy_clean:
	rm -f $(ZEXY_TARGETS)
	rm -f $(ZEXY_OBJECTS)

zexy_install: zexy
	install -d $(DESTDIR)$(objectsdir)/$(ZEXY_NAME)
	install -p $(externals_src)/zexy/src/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(ZEXY_NAME)
endif



#------------------------------------------------------------------------------#
# pdcontainer
PDCONTAINER_NAME = pdcontainer
PDCONTAINER_INCLUDE = -I$(externals_src)/grh/PDContainer
PDCONTAINER_SRC = $(wildcard $(externals_src)/grh/PDContainer/src/[hH]*.cpp) \
                  $(wildcard $(externals_src)/grh/PDContainer/tinyxml/*.cpp)
PDCONTAINER_OBJ = $(PDCONTAINER_SRC:.cpp=.o)
PDCONTAINER_TMP = $(wildcard $(externals_src)/grh/PDContainer/src/h_*.cpp)
PDCONTAINER_TARGETS = $(PDCONTAINER_TMP:.cpp=.$(EXTENSION))
PDC_SHARED = $(wildcard $(externals_src)/grh/PDContainer/tinyxml/*.cpp)

pdcontainer: $(PDCONTAINER_TARGETS)

$(PDCONTAINER_TARGETS) : %.$(EXTENSION) : $(PDCONTAINER_OBJ)
	$(CXX) $(LDFLAGS) -o $*.$(EXTENSION) "$*.o" $(shell ls `echo "$*.o" | sed -e s/"h_[a-z]"/"\?\?"/g -e s/"map"/"Map"/g -e s/"set"/"Set"/g -e s/"queue"/"Queue"/g`) $(PDC_SHARED:.cpp=.o) $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 $*.$(EXTENSION)
#	rm -f -- "$*.o"

$(PDCONTAINER_OBJ) : %.o : %.cpp
ifeq ($(EMSCRIPTEN),yes)
	$(CXX) $(CXXFLAGS) -DPDCONTAINER_SINGLE_OBJECT $(PDCONTAINER_INCLUDE) -std=c++98 -o "$*.o" -c "$*.cpp"
else
	$(CXX) $(CXXFLAGS) -DPDCONTAINER_SINGLE_OBJECT $(PDCONTAINER_INCLUDE) -o "$*.o" -c "$*.cpp"
endif

pdcontainer_install: pdcontainer
	install -d $(DESTDIR)$(objectsdir)/$(PDCONTAINER_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(PDCONTAINER_NAME) \
		--author "Georg Holzmann" \
		--description "container objects of the C++ STL for Pd" \
		--license "GPL" \
		--version ""
	install -p $(PDCONTAINER_TARGETS) $(DESTDIR)$(objectsdir)/$(PDCONTAINER_NAME)
	install -p $(externals_src)/grh/PDContainer/help/*.pd \
		$(DESTDIR)$(objectsdir)/$(PDCONTAINER_NAME)
	install -p $(externals_src)/grh/PDContainer/readme.txt \
		$(DESTDIR)$(objectsdir)/$(PDCONTAINER_NAME)

pdcontainer_clean:
	-rm -f -- $(PDCONTAINER_TARGETS)
	-rm -f -- $(externals_src)/grh/PDContainer/src/*.o
	-rm -f -- $(externals_src)/grh/PDContainer/src/*.bak
	-rm -f -- $(externals_src)/grh/PDContainer/src/*.*~
	-rm -f -- $(externals_src)/grh/PDContainer/tinyxml/*.o
	-rm -f -- $(externals_src)/grh/PDContainer/tinyxml/*.bak
	-rm -f -- $(externals_src)/grh/PDContainer/tinyxml/*.*~



#------------------------------------------------------------------------------#
# adaptive
ADAPTIVE_NAME=adaptive
ADAPTIVE_SRC := $(wildcard $(externals_src)/grh/adaptive/src/*.c)
ADAPTIVE_OBJECTS := $(ADAPTIVE_SRC:.c=.o)
ADAPTIVE_FLAGS := -DADAPTIVE_SINGLE_OBJ
ADAPTIVE_TMP := $(wildcard $(externals_src)/grh/adaptive/src/*lms*.c)
ifeq ($(EMSCRIPTEN),yes)
ADAPTIVE_TARGETS := $(ADAPTIVE_SRC:.c=.$(EXTENSION))

adaptive: $(ADAPTIVE_SRC:.c=.$(EXTENSION))

$(ADAPTIVE_OBJECTS): $(ADAPTIVE_SRC)
	$(CC) $(CFLAGS) $(ADAPTIVE_FLAGS) -o "$*.o" -c "$*.c"

$(ADAPTIVE_TARGETS): $(ADAPTIVE_OBJECTS)
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o"  $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 "$*.$(EXTENSION)"
else
ADAPTIVE_TARGETS := $(ADAPTIVE_TMP:.c=.$(EXTENSION))

adaptive: $(ADAPTIVE_TARGETS)

$(ADAPTIVE_TARGETS) : %.$(EXTENSION) : %.o $(ADAPTIVE_OBJECTS)
	$(CC) $(LDFLAGS) -o $*.$(EXTENSION) "$*.o" $(externals_src)/grh/adaptive/src/adaptive.o $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 $*.$(EXTENSION)
	rm -f -- "$*.o"

$(ADAPTIVE_OBJECTS) : %.o : %.c
	$(CC) $(CFLAGS) $(ADAPTIVE_FLAGS) -o "$*.o" -c "$*.c"
endif

adaptive_install: adaptive
	install -d $(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(ADAPTIVE_NAME) \
		--author "Georg Holzmann, Gerda Strobl" \
		--description "library for adaptive systems and filters" \
		--license "GNU GPL" \
		--version ""
	install -p $(ADAPTIVE_TARGETS) $(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)
	install -p $(externals_src)/grh/adaptive/doc/*.pd \
		$(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)
	install -p $(externals_src)/grh/adaptive/readme \
		$(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)
	install -d $(DESTDIR)$(examplesdir)/$(ADAPTIVE_NAME)
	install -p $(externals_src)/grh/adaptive/examples/*.pd \
		$(externals_src)/grh/adaptive/examples/*.dat \
		$(DESTDIR)$(examplesdir)/$(ADAPTIVE_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)/manual
	install -p $(externals_src)/grh/adaptive/readme \
		$(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)/manual/README.txt
	install -d $(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)/examples
	install -p $(externals_src)/grh/adaptive/examples/*.pd \
		$(externals_src)/grh/adaptive/examples/*.dat \
		$(DESTDIR)$(objectsdir)/$(ADAPTIVE_NAME)/examples

adaptive_clean:
	-rm -f -- $(ADAPTIVE_TARGETS)
	-rm -f -- $(ADAPTIVE_OBJECTS)
	-rm -f -- $(externals_src)/grh/adaptive/src/*.bak
	-rm -f -- $(externals_src)/grh/adaptive/src/*.*~



#------------------------------------------------------------------------------#
# iem_adaptfilt
IEMADAPT_NAME=iem_adaptfilt
IEMADAPT_SRC := $(wildcard $(externals_src)/iem/iem_adaptfilt/src/*~.c)

iem_adaptfilt: $(IEMADAPT_SRC:.c=.$(EXTENSION))

iem_adaptfilt_install: iem_adaptfilt
	install -d $(DESTDIR)$(objectsdir)/$(IEMADAPT_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMADAPT_NAME) \
		--author "Markus Noisternig, Thomas Musil" \
		--description "several algorithms for adaptive filters" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMADAPT_SRC:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEMADAPT_NAME)
	install -p $(externals_src)/iem/iem_adaptfilt/help/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMADAPT_NAME)
	install -d $(DESTDIR)$(objectsdir)/$(IEMADAPT_NAME)/manual
	install -p $(externals_src)/iem/iem_adaptfilt/doc/adapt_filt_lib.pdf \
		$(DESTDIR)$(objectsdir)/$(IEMADAPT_NAME)/manual

iem_adaptfilt_clean:
	-rm -f -- $(IEMADAPT_SRC:.c=.$(EXTENSION))
	-rm -f -- $(IEMADAPT_SRC:.c=.o)
	-rm -f -- $(externals_src)/iem/iem_adaptfilt/src/*.bak
	-rm -f -- $(externals_src)/iem/iem_adaptfilt/src/*.*~



#------------------------------------------------------------------------------#
# iem_delay
IEMDELAY_NAME=iem_delay
IEMDELAY_SRC := $(wildcard $(externals_src)/iem/iem_delay/src/*~.c)

iem_delay: $(IEMDELAY_SRC:.c=.$(EXTENSION))

iem_delay_install: iem_delay
	install -d $(DESTDIR)$(objectsdir)/$(IEMDELAY_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMDELAY_NAME) \
		--author "Thomas Musil" \
		--description "various delay objects" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMDELAY_SRC:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEMDELAY_NAME)
	install -p $(externals_src)/iem/iem_delay/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMDELAY_NAME)
	install -p $(externals_src)/iem/iem_delay/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEMDELAY_NAME)/README.txt

iem_delay_clean:
	-rm -f -- $(IEMDELAY_SRC:.c=.$(EXTENSION))
	-rm -f -- $(IEMDELAY_SRC:.c=.o)
	-rm -f -- $(externals_src)/iem/iem_delay/src/*.bak
	-rm -f -- $(externals_src)/iem/iem_delay/src/*.*~



#------------------------------------------------------------------------------#
# iem_roomsim
IEMROOM_NAME=iem_roomsim
IEMROOM_SRC := $(wildcard $(externals_src)/iem/iem_roomsim/src/*d.c)

iem_roomsim: $(IEMROOM_SRC:.c=.$(EXTENSION))

iem_roomsim_install: iem_roomsim
	install -d $(DESTDIR)$(objectsdir)/$(IEMROOM_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMROOM_NAME) \
		--author "Thomas Musil" \
		--description "objects for room simulation" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMROOM_SRC:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEMROOM_NAME)
	install -p $(externals_src)/iem/iem_roomsim/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMROOM_NAME)
	install -p $(externals_src)/iem/iem_roomsim/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEMROOM_NAME)/README.txt

iem_roomsim_clean:
	-rm -f -- $(IEMROOM_SRC:.c=.$(EXTENSION))
	-rm -f -- $(IEMROOM_SRC:.c=.o)
	-rm -f -- $(externals_src)/iem/iem_roomsim/src/*.bak
	-rm -f -- $(externals_src)/iem/iem_roomsim/src/*.*~



#------------------------------------------------------------------------------#
# iem_spec2
IEMSPEC2_NAME=iem_spec2
IEMSPEC2_SRC := $(wildcard $(externals_src)/iem/iem_spec2/src/*~.c)

iem_spec2: $(IEMSPEC2_SRC:.c=.$(EXTENSION))

iem_spec2_install: iem_spec2
	install -d $(DESTDIR)$(objectsdir)/$(IEMSPEC2_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMSPEC2_NAME) \
		--author "Thomas Musil" \
		--description "special spectral processing objects, which only calculates blocksize/2 + 1 samples of a signal" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMSPEC2_SRC:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(IEMSPEC2_NAME)
	install -p $(externals_src)/iem/iem_spec2/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMSPEC2_NAME)
	install -p $(externals_src)/iem/iem_spec2/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEMSPEC2_NAME)/README.txt

iem_spec2_clean:
	-rm -f -- $(IEMSPEC2_SRC:.c=.$(EXTENSION))
	-rm -f -- $(IEMSPEC2_SRC:.c=.o)
	-rm -f -- $(externals_src)/iem/iem_spec2/src/*.bak
	-rm -f -- $(externals_src)/iem/iem_spec2/src/*.*~



#------------------------------------------------------------------------------#
# iem_tab
IEMTAB_NAME=iem_tab
IEMTAB_SRC := $(wildcard $(externals_src)/iem/iem_tab/src/*.c)
IEMTAB_OBJ := $(IEMTAB_SRC:.c=.o)
IEMTAB_TARTMP := $(wildcard $(externals_src)/iem/iem_tab/src/tab_*.c)
IEMTAB_TARGETS := $(IEMTAB_TARTMP:.c=.$(EXTENSION))
IEMTAB_FLAGS := -DIEMTAB_SINGLE_OBJ

iem_tab: $(IEMTAB_TARGETS)

ifneq ($(EMSCRIPTEN),yes)
$(IEMTAB_TARGETS) : %.$(EXTENSION) : %.o $(IEMTAB_OBJ)
	$(CC) $(LDFLAGS) -o $*.$(EXTENSION) "$*.o" $(externals_src)/iem/iem_tab/src/iem_tab.o $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 $*.$(EXTENSION)
	rm -f -- "$*.o"

$(IEMTAB_OBJ) : %.o : %.c
	$(CC) $(CFLAGS) $(IEMTAB_FLAGS) -o "$*.o" -c "$*.c"
else
$(IEMTAB_OBJ): $(IEMTAB_SRC)
	$(CC) $(CFLAGS) $(IEMTAB_FLAGS) -o "$*.o" -c "$*.c"

$(IEMTAB_TARGETS): $(IEMTAB_OBJ)
	$(CC) $(LDFLAGS) $(IEMTAB_FLAGS) -o $*.$(EXTENSION) $*.o $(LIBS)
endif

iem_tab_install: iem_tab
	install -d $(DESTDIR)$(objectsdir)/$(IEMTAB_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMTAB_NAME) \
		--author "Thomas Musil" \
		--description "library to manipulate tables or arrays" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMTAB_TARGETS) $(DESTDIR)$(objectsdir)/$(IEMTAB_NAME)
	install -p $(externals_src)/iem/iem_tab/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMTAB_NAME)
	install -p $(externals_src)/iem/iem_tab/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEMTAB_NAME)/README.txt

iem_tab_clean:
	-rm -f -- $(IEMTAB_TARGETS)
	-rm -f -- $(IEMTAB_OBJ)
	-rm -f -- $(externals_src)/iem/iem_tab/src/*.bak
	-rm -f -- $(externals_src)/iem/iem_tab/src/*.*~



#------------------------------------------------------------------------------#
# flashserver
FLASHSERVER_NAME=flashserver
FLASHSERVER_OBJECTS := $(wildcard $(externals_src)/olafmatt/flashserver/*.c)
flashserver: $(FLASHSERVER_OBJECTS:.c=.$(EXTENSION))

flashserver_install: flashserver
	install -d $(DESTDIR)$(objectsdir)/$(FLASHSERVER_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(FLASHSERVER_NAME) \
		--author "Olaf Matthes" \
		--description "a flashserver for multiple clients" \
		--license "GNU GPL" \
		--version ""
	install -p $(FLASHSERVER_OBJECTS:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(FLASHSERVER_NAME)
	install -p $(externals_src)/olafmatt/flashserver/*.pd \
		$(DESTDIR)$(objectsdir)/$(FLASHSERVER_NAME)
	install -p $(externals_src)/olafmatt/flashserver/README \
		$(DESTDIR)$(objectsdir)/$(FLASHSERVER_NAME)

flashserver_clean:
	-rm -f -- $(FLASHSERVER_OBJECTS:.c=.$(EXTENSION))
	-rm -f -- $(FLASHSERVER_OBJECTS:.c=.o)
	-rm -f -- $(externals_src)/olafmatt/flashserver/*.bak
	-rm -f -- $(externals_src)/olafmatt/flashserver/*.*~



#------------------------------------------------------------------------------#
# hdspm_mixer
HDSPM_NAME=hdspm_mixer
HDSPM_SRC := $(wildcard $(externals_src)/iem/hdspm_mixer/*.c)
HDSPM_OBJ := $(HDSPM_SRC:.c=.o)
HDSPM_TARGET := $(externals_src)/iem/hdspm_mixer/hdspmmixer.$(EXTENSION)

hdspm_mixer: $(HDSPM_TARGET)

$(HDSPM_TARGET) : $(HDSPM_OBJ)
	$(CC) $(LDFLAGS) -o $(HDSPM_TARGET) $(HDSPM_OBJ) $(LIBS)
	$(STRIP) $(HDSPM_TARGET)
	chmod 755 $(HDSPM_TARGET)
	rm -f -- $(HDSPM_OBJ)

$(HDSPM_OBJ) : %.o : %.c
	$(CC) $(CFLAGS) -o "$*.o" -c "$*.c"

hdspm_mixer_install: hdspm_mixer
	install -d $(DESTDIR)$(objectsdir)/$(HDSPM_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(HDSPM_NAME) \
		--author "Winfried Ritsch" \
		--description "pd-mixer for RME hammerfall devices" \
		--license "GNU GPL" \
		--version ""
	install -p $(HDSPM_TARGET) $(DESTDIR)$(objectsdir)/$(HDSPM_NAME)
	install -p $(externals_src)/iem/hdspm_mixer/*.pd \
		$(DESTDIR)$(objectsdir)/$(HDSPM_NAME)

hdspm_mixer_clean:
	-rm -f -- $(HDSPM_TARGET)
	-rm -f -- $(HDSPM_OBJ)
	-rm -f -- $(externals_src)/iem/hdspm_mixer/*.bak
	-rm -f -- $(externals_src)/iem/hdspm_mixer/*.*~



#------------------------------------------------------------------------------#
# iemgui
IEMGUI_NAME=iemgui
#IEMGUI_SRC := $(wildcard $(externals_src)/iem/iemgui/src/*.c)
IEMGUI_SRC := iem/iemgui/src/iemgui.c iem/iemgui/src/room_sim_2d.c iem/iemgui/src/room_sim_3d.c
IEMGUI_OBJ := $(IEMGUI_SRC:.c=.o)
#IEMGUI_TARTMP := $(wildcard $(externals_src)/iem/iemgui/src/*_*.c)
IEMGUI_TARTMP := iem/iemgui/src/room_sim_2d.c iem/iemgui/src/room_sim_3d.c
IEMGUI_TARGETS := $(IEMGUI_TARTMP:.c=.$(EXTENSION))
IEMGUI_FLAGS := -DIEMGUI_SINGLE_OBJ

iemgui: $(IEMGUI_TARGETS)

ifneq ($(EMSCRIPTEN),yes)
$(IEMGUI_TARGETS) : %.$(EXTENSION) : %.o $(IEMGUI_OBJ)
	$(CC) $(LDFLAGS) $(IEMGUI_FLATPAK_LINKER_FLAGS) -o $*.$(EXTENSION) "$*.o" $(externals_src)/iem/iemgui/src/iemgui.o $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 $*.$(EXTENSION)
	rm -f -- "$*.o"

$(IEMGUI_OBJ) : %.o : %.c
	$(CC) $(CFLAGS) $(IEMGUI_FLAGS) -o "$*.o" -c "$*.c"
else
$(IEMGUI_OBJ): $(IEMGUI_SRC)
	$(CC) $(CFLAGS) $(IEMGUI_FLAGS) -o "$*.o" -c "$*.c"

$(IEMGUI_TARGETS): $(IEMGUI_OBJ)
	$(CC) $(LDFLAGS) $(IEMGUI_FLAGS) -o $*.$(EXTENSION) $*.o $(externals_src)/iem/iemgui/src/iemgui.o $(LIBS)
endif

iemgui_install: iemgui
	install -d $(DESTDIR)$(objectsdir)/$(IEMGUI_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMGUI_NAME) \
		--author "Thomas Musil" \
		--description "some more GUI objects for PD" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMGUI_TARGETS) $(DESTDIR)$(objectsdir)/$(IEMGUI_NAME)
	#install -p $(externals_src)/iem/iemgui/*.pd \
	#	$(DESTDIR)$(objectsdir)/$(IEMGUI_NAME)
	install -p $(externals_src)/iem/iemgui/room_sim*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMGUI_NAME)
	install -p $(externals_src)/iem/iemgui/READ_ME.txt \
		$(DESTDIR)$(objectsdir)/$(IEMGUI_NAME)/README.txt

iemgui_clean:
	-rm -f -- $(IEMGUI_TARGETS)
	-rm -f -- $(IEMGUI_OBJ)
	-rm -f -- $(externals_src)/iem/iemgui/src/*.bak
	-rm -f -- $(externals_src)/iem/iemgui/src/*.*~



#------------------------------------------------------------------------------#
# iemxmlrpc
IEMXMLRPC_NAME=iemxmlrpc
IEMXMLRPC_SRC := $(externals_src)/iem/iemxmlrpc/main.cpp
IEMXMLRPC_TARGET := $(externals_src)/iem/iemxmlrpc/xmlrpc.$(EXTENSION)
IEMXMLRPC_FLAGS := -I$(externals_src)/iem/iemxmlrpc/xmlrpc++/src
XMLRPCPP_SRC := $(wildcard $(externals_src)/iem/iemxmlrpc/xmlrpc++/src/*.cpp)

iemxmlrpc: $(IEMXMLRPC_TARGET)

$(IEMXMLRPC_TARGET) : $(IEMXMLRPC_SRC:.cpp=.o) $(XMLRPCPP_SRC:.cpp=.o)
	$(CXX) $(LDFLAGS) -o $(IEMXMLRPC_TARGET) $(IEMXMLRPC_SRC:.cpp=.o) $(XMLRPCPP_SRC:.cpp=.o) $(LIBS)
	$(STRIP) $(IEMXMLRPC_TARGET)
	chmod 755 $(IEMXMLRPC_TARGET)
	#rm -f -- "$*.o"

$(IEMXMLRPC_SRC:.cpp=.o) $(XMLRPCPP_SRC:.cpp=.o) : %.o : %.cpp
	$(CXX) $(CXXFLAGS) $(IEMXMLRPC_FLAGS) -o "$*.o" -c "$*.cpp"

iemxmlrpc_install: iemxmlrpc
	install -d $(DESTDIR)$(objectsdir)/$(IEMXMLRPC_NAME)
	$(scripts_src)/generate-libdir-metafile.sh $(DESTDIR)$(objectsdir) $(IEMXMLRPC_NAME) \
		--author "Thomas Grill, Winfried Ritsch" \
		--description "XMLRPC external for PD" \
		--license "GNU GPL" \
		--version ""
	install -p $(IEMXMLRPC_TARGET) $(DESTDIR)$(objectsdir)/$(IEMXMLRPC_NAME)
	install -p $(externals_src)/iem/iemxmlrpc/*.pd \
		$(DESTDIR)$(objectsdir)/$(IEMXMLRPC_NAME)
	install -p $(externals_src)/iem/iemxmlrpc/README.txt \
		$(DESTDIR)$(objectsdir)/$(IEMXMLRPC_NAME)
	install -d $(DESTDIR)$(examplesdir)/$(IEMXMLRPC_NAME)
	install -p $(externals_src)/iem/iemxmlrpc/*.pd \
                   $(externals_src)/iem/iemxmlrpc/*.py \
		   $(DESTDIR)$(examplesdir)/$(IEMXMLRPC_NAME)

iemxmlrpc_clean:
	-rm -f -- $(IEMXMLRPC_TARGET)
	-rm -f -- $(externals_src)/iem/iemxmlrpc/xmlrpc++/src/*.o
	-rm -f -- $(externals_src)/iem/iemxmlrpc/*.o
	-rm -f -- $(externals_src)/iem/iemxmlrpc/*.bak
	-rm -f -- $(externals_src)/iem/iemxmlrpc/*.*~



#------------------------------------------------------------------------------#
# EARPLUG
earplug:
	make -C $(externals_src)/earplug~ PD_PATH=$(pd_src) CFLAGS="$(CFLAGS)"

earplug_install:
	make -C $(externals_src)/earplug~ DESTDIR="$(DESTDIR)" objectsdir="$(objectsdir)" install

earplug_clean:
	make -C $(externals_src)/earplug~ clean



#------------------------------------------------------------------------------#
# FLITE
FLITE = $(externals_src)/pd-flite/
ifneq ($(EMSCRIPTEN), yes)
FLITE_OBJECTS := $(wildcard $(FLITE)/*.c)

flite:
	cd $(FLITE) && \
		git submodule update --init --recursive && \
		make PDDIR=$(pd_src) PDBINDIR=$(pd_src)/src/

flite_install: flite
	install -d $(DESTDIR)$(objectsdir)/flite
	install -p $(FLITE_OBJECTS:.c=.$(EXTENSION)) \
		$(DESTDIR)$(objectsdir)/flite/
	install -p $(FLITE)/*.pd \
		$(DESTDIR)$(objectsdir)/flite/

flite_clean:
	cd $(FLITE) && make clean
else
FLITE_NAME = flite
FLITE_DEP = $(FLITE)/deps/flite
BUNDLED_FLITE = $(FLITE_DEP)
FLITE_SRC = $(BUNDLED_FLITE)/lang/cmulex/cmu_lex.c \
	 $(BUNDLED_FLITE)/lang/cmulex/cmu_lex_data.c \
	 $(BUNDLED_FLITE)/lang/cmulex/cmu_lex_entries.c \
	 $(BUNDLED_FLITE)/lang/cmulex/cmu_lts_model.c \
	 $(BUNDLED_FLITE)/lang/cmulex/cmu_lts_rules.c \
	 $(BUNDLED_FLITE)/lang/cmulex/cmu_postlex.c \
	 $(BUNDLED_FLITE)/lang/cmu_grapheme_lang/cmu_grapheme_lang.c \
	 $(BUNDLED_FLITE)/lang/cmu_grapheme_lang/cmu_grapheme_phoneset.c \
	 $(BUNDLED_FLITE)/lang/cmu_grapheme_lang/cmu_grapheme_phrasing_cart.c \
	 $(BUNDLED_FLITE)/lang/cmu_grapheme_lang/graph_gpos.c \
	 $(BUNDLED_FLITE)/lang/cmu_grapheme_lex/cmu_grapheme_lex.c \
	 $(BUNDLED_FLITE)/lang/cmu_grapheme_lex/grapheme_unitran_tables.c \
	 $(BUNDLED_FLITE)/lang/cmu_indic_lang/cmu_indic_lang.c \
	 $(BUNDLED_FLITE)/lang/cmu_indic_lang/cmu_indic_phoneset.c \
	 $(BUNDLED_FLITE)/lang/cmu_indic_lang/cmu_indic_phrasing_cart.c \
	 $(BUNDLED_FLITE)/lang/cmu_indic_lex/cmu_indic_lex.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal16/cmu_us_kal16.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal16/cmu_us_kal16_diphone.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal16/cmu_us_kal16_lpc.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal16/cmu_us_kal16_res.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal16/cmu_us_kal16_residx.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal/cmu_us_kal.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal/cmu_us_kal_diphone.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal/cmu_us_kal_lpc.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal/cmu_us_kal_res.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal/cmu_us_kal_residx.c \
	 $(BUNDLED_FLITE)/lang/cmu_us_kal/cmu_us_kal_ressize.c \
	 $(BUNDLED_FLITE)/lang/usenglish/usenglish.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_aswd.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_durz_cart.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_dur_stats.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_expand.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_f0lr.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_f0_model.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_ffeatures.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_gpos.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_int_accent_cart.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_int_tone_cart.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_nums_cart.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_phoneset.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_phrasing_cart.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_pos_cart.c \
	 $(BUNDLED_FLITE)/lang/usenglish/us_text.c \
	 $(BUNDLED_FLITE)/src/audio/au_none.c \
	 $(BUNDLED_FLITE)/src/audio/audio.c \
	 $(BUNDLED_FLITE)/src/audio/au_streaming.c \
	 $(BUNDLED_FLITE)/src/cg/cst_cg.c \
	 $(BUNDLED_FLITE)/src/cg/cst_cg_dump_voice.c \
	 $(BUNDLED_FLITE)/src/cg/cst_cg_load_voice.c \
	 $(BUNDLED_FLITE)/src/cg/cst_cg_map.c \
	 $(BUNDLED_FLITE)/src/cg/cst_mlpg.c \
	 $(BUNDLED_FLITE)/src/cg/cst_mlsa.c \
	 $(BUNDLED_FLITE)/src/cg/cst_spamf0.c \
	 $(BUNDLED_FLITE)/src/cg/cst_vc.c \
	 $(BUNDLED_FLITE)/src/hrg/cst_ffeature.c \
	 $(BUNDLED_FLITE)/src/hrg/cst_item.c \
	 $(BUNDLED_FLITE)/src/hrg/cst_relation.c \
	 $(BUNDLED_FLITE)/src/hrg/cst_rel_io.c \
	 $(BUNDLED_FLITE)/src/hrg/cst_utterance.c \
	 $(BUNDLED_FLITE)/src/lexicon/cst_lexicon.c \
	 $(BUNDLED_FLITE)/src/lexicon/cst_lts.c \
	 $(BUNDLED_FLITE)/src/lexicon/cst_lts_rewrites.c \
	 $(BUNDLED_FLITE)/src/regex/cst_regex.c \
	 $(BUNDLED_FLITE)/src/regex/regexp.c \
	 $(BUNDLED_FLITE)/src/regex/regsub.c \
	 $(BUNDLED_FLITE)/src/speech/cst_lpcres.c \
	 $(BUNDLED_FLITE)/src/speech/cst_track.c \
	 $(BUNDLED_FLITE)/src/speech/cst_track_io.c \
	 $(BUNDLED_FLITE)/src/speech/cst_wave.c \
	 $(BUNDLED_FLITE)/src/speech/cst_wave_io.c \
	 $(BUNDLED_FLITE)/src/speech/cst_wave_utils.c \
	 $(BUNDLED_FLITE)/src/speech/g721.c \
	 $(BUNDLED_FLITE)/src/speech/g723_24.c \
	 $(BUNDLED_FLITE)/src/speech/g723_40.c \
	 $(BUNDLED_FLITE)/src/speech/g72x.c \
	 $(BUNDLED_FLITE)/src/speech/rateconv.c \
	 $(BUNDLED_FLITE)/src/stats/cst_cart.c \
	 $(BUNDLED_FLITE)/src/stats/cst_ss.c \
	 $(BUNDLED_FLITE)/src/stats/cst_viterbi.c \
	 $(BUNDLED_FLITE)/src/synth/cst_ffeatures.c \
	 $(BUNDLED_FLITE)/src/synth/cst_phoneset.c \
	 $(BUNDLED_FLITE)/src/synth/cst_ssml.c \
	 $(BUNDLED_FLITE)/src/synth/cst_synth.c \
	 $(BUNDLED_FLITE)/src/synth/cst_utt_utils.c \
	 $(BUNDLED_FLITE)/src/synth/cst_voice.c \
	 $(BUNDLED_FLITE)/src/synth/flite.c \
	 $(BUNDLED_FLITE)/src/utils/cst_alloc.c \
	 $(BUNDLED_FLITE)/src/utils/cst_args.c \
	 $(BUNDLED_FLITE)/src/utils/cst_endian.c \
	 $(BUNDLED_FLITE)/src/utils/cst_error.c \
	 $(BUNDLED_FLITE)/src/utils/cst_features.c \
	 $(BUNDLED_FLITE)/src/utils/cst_file_stdio.c \
	 $(BUNDLED_FLITE)/src/utils/cst_mmap_none.c \
	 $(BUNDLED_FLITE)/src/utils/cst_socket.c \
	 $(BUNDLED_FLITE)/src/utils/cst_string.c \
	 $(BUNDLED_FLITE)/src/utils/cst_tokenstream.c \
	 $(BUNDLED_FLITE)/src/utils/cst_url.c \
	 $(BUNDLED_FLITE)/src/utils/cst_val.c \
	 $(BUNDLED_FLITE)/src/utils/cst_val_const.c \
	 $(BUNDLED_FLITE)/src/utils/cst_val_user.c \
	 $(BUNDLED_FLITE)/src/utils/cst_wchar.c \
	 $(BUNDLED_FLITE)/src/wavesynth/cst_clunits.c \
	 $(BUNDLED_FLITE)/src/wavesynth/cst_diphone.c \
	 $(BUNDLED_FLITE)/src/wavesynth/cst_reflpc.c \
	 $(BUNDLED_FLITE)/src/wavesynth/cst_sigpr.c \
	 $(BUNDLED_FLITE)/src/wavesynth/cst_sts.c \
	 $(BUNDLED_FLITE)/src/wavesynth/cst_units.c
FLITE_VOICES := $(FLITE)/cmu_us_awb.flitevox $(FLITE)/cmu_us_rms.flitevox $(FLITE)/cmu_us_slt.flitevox
FLITE_OBJECTS := $(FLITE_SRC:.c=.o)
FLITE_TARGETS := $(FLITE_SRC:.c=.$(EXTENSION))
FLITE_FLAGS := -I$(FLITE_DEP)/include -I$(FLITE_DEP)/lang/usenglish -I$(FLITE_DEP)/lang/cmulex -DCSST_AUDIO_NONE -DVERSION='"0.3.3"'

flite: $(FLITE)/flite.$(EXTENSION) $(FLITE_VOICES)

$(FLITE)/cmu_us_%.flitevox:
	curl --output $@ http://cmuflite.org/packed/flite-2.0/voices/cmu_us_$*.flitevox

$(FLITE)/flite.$(EXTENSION): $(FLITE_OBJECTS)
	cd $(FLITE) && git apply ../patches/flite.patch || true
	$(CC) $(FLITE_OBJECTS) $(FLITE_FLAGS) $(FLITE)/flite.c $(LDFLAGS) $(CFLAGS) -o $(FLITE)/flite.$(EXTENSION) $(LIBS)
	$(STRIP) $(FLITE)/flite.$(EXTENSION)
	chmod 755 $(FLITE)/flite.$(EXTENSION)

$(FLITE_OBJECTS): $(FLITE_SRC)
	$(CC) $(FLITE_FLAGS) $(CFLAGS) -o "$*.o" -c "$*.c";

$(FLITE_SRC):
	git submodule update --init --recursive

flite_install: flite
	install -d $(DESTDIR)$(objectsdir)/$(FLITE_NAME)
	install -p $(FLITE)/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(FLITE_NAME)
	install -p $(FLITE)/*.flitevox $(DESTDIR)$(objectsdir)/$(FLITE_NAME)

flite_clean:
	rm -f $(FLITE)/*.{o,$(EXTENSION)} $(FLITE_DEP)/*/*/*.{o,$(EXTENSION)}
endif



#------------------------------------------------------------------------------#
# HIDRAW
ifneq ($(EMSCRIPTEN), yes)
hidraw:
	cd $(externals_src)/pd-hidraw && \
		rm -f *.o && \
		git submodule update --init --recursive && \
		make PDDIR=$(pd_src) PDBINDIR=$(pd_src)/src/

hidraw_install: hidraw
	install -d $(DESTDIR)$(objectsdir)/hidraw
	install -p $(externals_src)/pd-hidraw/hidraw.${EXTENSION} \
		$(DESTDIR)$(objectsdir)/hidraw/
	install -p $(externals_src)/pd-hidraw/*.pd \
		$(DESTDIR)$(objectsdir)/hidraw/

hidraw_clean:
	cd $(externals_src)/pd-hidraw && make clean
else
# Currently unused, no direct access to hid devices in the browser
# Leaving the build process here, in case anything changes in the future
HIDRAW_NAME = hidraw
HIDRAW = $(externals_src)/pd-hidraw
HIDRAW_SRC := $(wildcard $(HIDRAW)/hidapi/hidapi/*.c)
HIDRAW_OBJECTS := $(HIDRAW_SRC:.c=.o)
HIDRAW_TARGETS := $(HIDRAW_SRC:.c=.$(EXTENSION))
HIDRAW_FLAGS := -I$(externals_src)/pd-hidraw/hidapi/hidapi

hidraw: $(HIDRAW_TARGETS) $(HIDRAW)/hidraw.$(EXTENSION)

$(HIDRAW)/hidraw.$(EXTENSION):
	$(CC) $(HIDRAW_TARGETS) $(HIDRAW_FLAGS) $(externals_src)/pd-hidraw/pd-hidraw.c $(LDFLAGS) $(CFLAGS) -o $(externals_src)/pd-hidraw/hidraw.$(EXTENSION) $(LIBS)
	$(STRIP) $(externals_src)/pd-hidraw/hidraw.$(EXTENSION)
	chmod 755 $(externals_src)/pd-hidraw/hidraw.$(EXTENSION)

$(HIDRAW_OBJECTS): $(HIDRAW_SRC)
	$(CC) $(CFLAGS) -o "$*.o" -c "$*.c";

$(HIDRAW_TARGETS): $(HIDRAW_OBJECTS)
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o" $(LIBS)
	$(STRIP) $*.$(EXTENSION)
	chmod 755 "$*.$(EXTENSION)"
	
hidraw_clean:
	rm -f $(HIDRAW_TARGETS)
	rm -f $(HIDRAW_OBJECTS)

hidraw_install: hidraw
	install -d $(DESTDIR)$(objectsdir)/$(HIDRAW_NAME)
	install -p $(externals_src)/pd-hidraw/*.$(EXTENSION) $(DESTDIR)$(objectsdir)/$(HIDRAW_NAME)
endif


#------------------------------------------------------------------------------#
# NEURALNET

ifneq ($(EMSCRIPTEN), yes)
neuraln:
	cd $(externals_src)/neuralnet && \
		rm -f *.o && \
		git submodule update --init --recursive && \
		make PDDIR=$(pd_src) PDBINDIR=$(pd_src)/src/
else
NEURAL_SRC=$(wildcard $(externals_src)/neuralnet/src/*.c)
NEURAL_OBJ=$(NEURAL_SRC:.c=.o)
NEURAL_TARGETS=$(NEURAL_SRC:.c=.$(EXTENSION))

$(NEURAL_OBJ): $(NEURAL_SRC)
	$(CC) $(CFLAGS) $(LIBS) -c -o "$*.o" "$*.c"

$(NEURAL_TARGETS): $(NEURAL_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $(LIBS) -o "$*.$(EXTENSION)" "$*.o"

neuraln: $(NEURAL_TARGETS)
endif

neuraln_install: neuraln
	install -d $(DESTDIR)$(objectsdir)/neuralnet
ifneq ($(EMSCRIPTEN), yes)
	install -p $(externals_src)/neuralnet/*.${EXTENSION} \
		$(DESTDIR)$(objectsdir)/neuralnet/
else
	install -p $(NEURAL_TARGETS) $(DESTDIR)$(objectsdir)/neuralnet/
endif
	install -p $(externals_src)/neuralnet/*.pd \
		$(DESTDIR)$(objectsdir)/neuralnet/
	cp -rf $(externals_src)/neuralnet/examples \
		$(DESTDIR)$(objectsdir)/neuralnet/

neuraln_clean:
	cd $(externals_src)/neuralnet && make clean



#==============================================================================#
#
# DEVELOPER'S TARGETS
#
#==============================================================================#

TAGS: etags

etags:
	etags $(pd_src)/src/*.[ch] 
	find $(externals_src) -type f -name '*.[ch]' -exec etags -a '{}' \;
	find $(externals_src) -type f -name '*.cc' -exec etags -a '{}' \;
	find $(externals_src) -type f -name '*.cpp' -exec etags -a '{}' \;
	etags --append --language=none --regex="/proc[ \t]+\([^ \t]+\)/\1/" \
		$(externals_src)/unauthorized/*/*.tk
	find /usr/include -type f -name \*.h -exec etags -a '{}' \;
	make etags_`uname -s`

etags_Darwin:
	etags -a $(externals_src)/hcs/hid/HID\ Utilities\ Source/*.[ch]
	find /System/Library/Frameworks  -type f -name \*.h -exec etags -a '{}' \;
	find /Library/Frameworks  -type f -name \*.h -exec etags -a '{}' \;
	find /usr/local/include/ -type f -name \*.h -exec etags -a '{}' \;

etags_Linux:

etags_MINGW:
	find /usr/local/include/ -type f -name \*.h -exec etags -a '{}' \;


#==============================================================================#
#
# CLEAN TARGETS
#
#==============================================================================#

# the destination-specific clean targets are in Makefile.buildlayout
clean:  $(patsubst %, %_clean,$(LIB_TARGETS))


distclean: clean cruft_clean


test_locations:
	@echo "PD_VERSION: $(PD_VERSION)"
	@echo "PACKAGE_VERSION: $(PACKAGE_VERSION)"
	@echo "CWD $(CWD)"
	@echo "DESTDIR $(DESTDIR)"
	@echo "PREFIX $(prefix)"
	@echo "BINDIR  $(bindir)"
	@echo "LIBDIR  $(libdir)"
	@echo "OBJECTSDIR  $(objectsdir)"
	@echo "PDDOCDIR  $(pddocdir)"
	@echo "LIBPDDIR  $(libpddir)"
	@echo "LIBPDBINDIR  $(libpdbindir)"
	@echo "MANUALSDIR  $(manualsdir)"
	@echo "EXAMPLESDIR  $(examplesdir)"
	@echo " "
	@echo "Compiling these libs:"
	@echo "$(LIB_TARGETS)"


.PHONY: all install clean distclean test_locations $(LIB_TARGETS) \
$(patsubst %, %_install,$(LIB_TARGETS)) $(patsubst %, %_clean,$(LIB_TARGETS))


