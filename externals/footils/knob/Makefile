## Pd library template version 1.0
# For instructions on how to use this template, see:
#  http://puredata.info/docs/developer/MakefileTemplate
LIBRARY_NAME = flatgui

# add your .c source files to the SOURCES variable, help files will be
# included automatically
SOURCES = knob.c

# For objects that only build on certain platforms, add those to the SOURCES
# line for the right platforms.
SOURCES_android = 
SOURCES_cygwin = 
SOURCES_macosx = 
SOURCES_iphoneos = 
SOURCES_linux = 
SOURCES_windows = 

# list all pd objects (i.e. myobject.pd) files here, and their helpfiles will
# be included automatically
# PDOBJECTS = mypdobject.pd

# example patches and related files, in the 'examples' subfolder
# EXAMPLES = bothtogether.pd

# manuals and related files, in the 'manual' subfolder
# MANUAL = manual.txt

# if you want to include any other files in the source and binary tarballs,
# list them here.  This can be anything from header files, example patches,
# documentation, etc.  README.txt and LICENSE.txt are required and therefore
# automatically included
# EXTRA_DIST = 



#------------------------------------------------------------------------------#
#
# you shouldn't need to edit anything below here, if we did it right :)
#
#------------------------------------------------------------------------------#

ifeq ($(macos_target),)
macos_target = 10.9
endif

# get library version from meta file
#LIBRARY_VERSION = $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(LIBRARY_NAME)-meta.pd)
LIBRARY_VERSION = 0.9

# where Pd lives
PD_PATH = ../../pd
# where to install the library
prefix = /usr
libdir = $(prefix)/lib
pkglibdir = $(libdir)/extra
#objectsdir = $(pkglibdir)


INSTALL = install
INSTALL_FILE    = $(INSTALL) -p -m 644
INSTALL_DIR     = $(INSTALL) -p -m 755 -d

CFLAGS = -DPD -I ../../pd/src -Wall -W -g -fPIC
LDFLAGS =  
LIBS = 
ALLSOURCES := $(SOURCES) $(SOURCES_android) $(SOURCES_cygwin) $(SOURCES_macosx) \
	         $(SOURCES_iphoneos) $(SOURCES_linux) $(SOURCES_windows)

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  CPU := $(shell uname -p)
  ifeq ($(CPU),arm-iphone) # iPhone/iPod Touch
    SOURCES += $(SOURCES_iphoneos)
    EXTENSION = pd_darwin
    OS = iphoneos
    IPHONE_BASE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin
    CC=$(IPHONE_BASE)/gcc
    CPP=$(IPHONE_BASE)/cpp
    CXX=$(IPHONE_BASE)/g++
    ISYSROOT = -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.0.sdk
    IPHONE_CFLAGS = -miphoneos-version-min=3.0 $(ISYSROOT) -arch armv6
    OPT_CFLAGS = -fast -funroll-loops -fomit-frame-pointer
	CFLAGS := $(IPHONE_CFLAGS) $(OPT_CFLAGS) $(CFLAGS) \
      -I/Applications/Pd-extended.app/Contents/Resources/include
    LDFLAGS += -arch armv6 -bundle -undefined dynamic_lookup $(ISYSROOT)
    LIBS += -lc 
    STRIP = strip -x
    DISTDIR=$(LIBRARY_NAME)-$(LIBRARY_VERSION)
    DISTBINDIR=$(DISTDIR)-$(OS)
  else # Mac OS X
    SOURCES += $(SOURCES_macosx)
    EXTENSION = pd_darwin
    OS = macosx
    OPT_CFLAGS = -ftree-vectorize -ftree-vectorizer-verbose=2 -fast
# build universal 32-bit on 10.4 and 32/64 on newer
    ifeq ($(shell uname -r | sed 's|\([0-9][0-9]*\)\.[0-9][0-9]*\.[0-9][0-9]*|\1|'), 8)
      FAT_FLAGS = -arch i386 -mmacosx-version-min=$(macos_target)
    else
      FAT_FLAGS = -arch i386 -arch x86_64 -mmacosx-version-min=$(macos_target)
      SOURCES += $(SOURCES_iphoneos)
    endif
    CFLAGS += $(FAT_FLAGS) -fPIC \
      -I/Applications/Pd-extended.app/Contents/Resources/include
    LDFLAGS += $(FAT_FLAGS) -bundle -undefined dynamic_lookup
    # if the 'pd' binary exists, check the linking against it to aid with stripping
    LDFLAGS += $(shell test -e $(PD_PATH)/bin/pd && echo -bundle_loader $(PD_PATH)/bin/pd)
    LIBS += -lc 
    STRIP = strip -x
    DISTDIR=$(LIBRARY_NAME)-$(LIBRARY_VERSION)
    DISTBINDIR=$(DISTDIR)-$(OS)
# install into ~/Library/Pd on Mac OS X since /usr/local isn't used much
    pkglibdir=$(HOME)/Library/Pd
  endif
endif
ifeq ($(UNAME),Linux)
  SOURCES += $(SOURCES_linux)
  EXTENSION = pd_linux
  OS = linux
  OPT_CFLAGS = -O6 -funroll-loops -fomit-frame-pointer
  CFLAGS += -fPIC
  LDFLAGS += -Wl,--export-dynamic  -shared -fPIC
  LIBS += -lc
  STRIP = strip --strip-unneeded -R .note -R .comment
  DISTDIR=$(LIBRARY_NAME)-$(LIBRARY_VERSION)
  DISTBINDIR=$(DISTDIR)-$(OS)-$(shell uname -m)
endif
ifeq (CYGWIN,$(findstring CYGWIN,$(UNAME)))
  SOURCES += $(SOURCES_cygwin)
  EXTENSION = dll
  OS = cygwin
  OPT_CFLAGS = -O6 -funroll-loops -fomit-frame-pointer
  CFLAGS += 
  LDFLAGS += -Wl,--export-dynamic -shared -L$(PD_PATH)/src
  LIBS += -lc -lpd
  STRIP = strip --strip-unneeded -R .note -R .comment
  DISTDIR=$(LIBRARY_NAME)-$(LIBRARY_VERSION)
  DISTBINDIR=$(DISTDIR)-$(OS)
endif
ifeq (MINGW,$(findstring MINGW,$(UNAME)))
  SOURCES += $(SOURCES_windows)
  EXTENSION = dll
  OS = windows
  OPT_CFLAGS = -O3 -funroll-loops -fomit-frame-pointer -march=i686 -mtune=pentium4
  CFLAGS += -mms-bitfields
  LDFLAGS += -s -shared -Wl,--enable-auto-import
  LIBS += -L$(PD_PATH)/src -L$(PD_PATH)/bin -L$(PD_PATH)/obj -lpd -lwsock32 -lkernel32 -luser32 -lgdi32
  STRIP = strip --strip-unneeded -R .note -R .comment
  DISTDIR=$(LIBRARY_NAME)-$(LIBRARY_VERSION)
  DISTBINDIR=$(DISTDIR)-$(OS)
endif

CFLAGS += $(OPT_CFLAGS)


.PHONY = install libdir_install single_install install-doc install-exec install-examples install-manual clean dist etags

all: $(SOURCES:.c=.$(EXTENSION))

%.o: %.c
	$(CC) $(CFLAGS) -o "$*.o" -c "$*.c"

%.$(EXTENSION): %.o
	$(CC) $(LDFLAGS) -o "$*.$(EXTENSION)" "$*.o"  $(LIBS)
	chmod a-x "$*.$(EXTENSION)"

# this links everything into a single binary file
$(LIBRARY_NAME): $(SOURCES:.c=.o) $(LIBRARY_NAME).o
	$(CC) $(LDFLAGS) -o $(LIBRARY_NAME).$(EXTENSION) $(SOURCES:.c=.o) $(LIBRARY_NAME).o $(LIBS)
	chmod a-x $(LIBRARY_NAME).$(EXTENSION)


install: libdir_install

# The meta and help files are explicitly installed to make sure they are
# actually there.  Those files are not optional, then need to be there.
libdir_install: $(SOURCES:.c=.$(EXTENSION)) install-doc install-examples install-manual
	$(INSTALL_DIR) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
#	$(INSTALL_FILE) $(LIBRARY_NAME)-meta.pd \
		$(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
	test -z "$(strip $(SOURCES))" || (\
		$(INSTALL_FILE) $(SOURCES:.c=.$(EXTENSION)) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME) && \
		$(STRIP) $(addprefix $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/,$(SOURCES:.c=.$(EXTENSION))))
	test -z "$(strip $(PDOBJECTS))" || \
		$(INSTALL_FILE) $(PDOBJECTS) \
			$(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)

# install library linked as single binary
single_install: $(LIBRARY_NAME) install-doc install-exec
	$(INSTALL_DIR) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
	$(INSTALL_FILE) $(LIBRARY_NAME).$(EXTENSION) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
	$(STRIP) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/$(LIBRARY_NAME).$(EXTENSION)

install-doc:
	$(INSTALL_DIR) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
	test -z "$(strip $(SOURCES))" || \
		$(INSTALL_FILE) $(SOURCES:.c=-help.pd) \
			$(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
	test -z "$(strip $(PDOBJECTS))" || \
		$(INSTALL_FILE) $(PDOBJECTS:.pd=-help.pd) \
			$(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)
#	$(INSTALL_FILE) README.txt $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/README.txt
#	$(INSTALL_FILE) LICENSE.txt $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/LICENSE.txt

install-examples:
	test -z "$(strip $(EXAMPLES))" || \
		$(INSTALL_DIR) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/examples && \
		for file in $(EXAMPLES); do \
			$(INSTALL_FILE) examples/$$file $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/examples; \
		done

install-manual:
	test -z "$(strip $(MANUAL))" || \
		$(INSTALL_DIR) $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/manual && \
		for file in $(MANUAL); do \
			$(INSTALL_FILE) manual/$$file $(DESTDIR)$(objectsdir)/$(LIBRARY_NAME)/manual; \
		done


clean:
	-rm -f -- $(SOURCES:.c=.o)
	-rm -f -- $(SOURCES:.c=.$(EXTENSION))
	-rm -f -- $(LIBRARY_NAME).o
	-rm -f -- $(LIBRARY_NAME).$(EXTENSION)

distclean: clean
	-rm -f -- $(DISTBINDIR).tar.gz
	-rm -rf -- $(DISTBINDIR)
	-rm -f -- $(DISTDIR).tar.gz
	-rm -rf -- $(DISTDIR)


$(DISTBINDIR):
	$(INSTALL_DIR) $(DISTBINDIR)

libdir: all $(DISTBINDIR)
#	$(INSTALL_FILE) $(LIBRARY_NAME)-meta.pd  $(DISTBINDIR)
	$(INSTALL_FILE) $(SOURCES)  $(DISTBINDIR)
	$(INSTALL_FILE) $(SOURCES:.c=-help.pd) $(DISTBINDIR)
	test -z "$(strip $(EXTRA_DIST))" || \
		$(INSTALL_FILE) $(EXTRA_DIST)    $(DISTBINDIR)
#	tar --exclude-vcs -czpf $(DISTBINDIR).tar.gz $(DISTBINDIR)

$(DISTDIR):
	$(INSTALL_DIR) $(DISTDIR)

dist: $(DISTDIR)
	$(INSTALL_FILE) Makefile  $(DISTDIR)
	$(INSTALL_FILE) README.txt $(DISTDIR)
	$(INSTALL_FILE) LICENSE.txt $(DISTDIR)
#	$(INSTALL_FILE) $(LIBRARY_NAME)-meta.pd  $(DISTDIR)
	test -z "$(strip $(ALLSOURCES))" || \
		$(INSTALL_FILE) $(ALLSOURCES)  $(DISTDIR)
	test -z "$(strip $(ALLSOURCES))" || \
		$(INSTALL_FILE) $(ALLSOURCES:.c=-help.pd) $(DISTDIR)
	test -z "$(strip $(PDOBJECTS))" || \
		$(INSTALL_FILE) $(PDOBJECTS)  $(DISTDIR)
	test -z "$(strip $(PDOBJECTS))" || \
		$(INSTALL_FILE) $(PDOBJECTS:.pd=-help.pd) $(DISTDIR)
	test -z "$(strip $(EXTRA_DIST))" || \
		$(INSTALL_FILE) $(EXTRA_DIST)    $(DISTDIR)
	test -z "$(strip $(EXAMPLES))" || \
		$(INSTALL_DIR) $(DISTDIR)/examples && \
		for file in $(EXAMPLES); do \
			$(INSTALL_FILE) examples/$$file $(DISTDIR)/examples; \
		done
	test -z "$(strip $(MANUAL))" || \
		$(INSTALL_DIR) $(DISTDIR)/manual && \
		for file in $(MANUAL); do \
			$(INSTALL_FILE) manual/$$file $(DISTDIR)/manual; \
		done
	tar --exclude-vcs -czpf $(DISTDIR).tar.gz $(DISTDIR)


etags:
	etags *.h $(SOURCES) ../../pd/src/*.[ch] /usr/include/*.h /usr/include/*/*.h

showsetup:
	@echo "PD_PATH: $(PD_PATH)"
	@echo "objectsdir: $(objectsdir)"
	@echo "LIBRARY_NAME: $(LIBRARY_NAME)"
	@echo "LIBRARY_VERSION: $(LIBRARY_VERSION)"
	@echo "SOURCES: $(SOURCES)"
	@echo "PDOBJECTS: $(PDOBJECTS)"
	@echo "ALLSOURCES: $(ALLSOURCES)"
	@echo "UNAME: $(UNAME)"
	@echo "CPU: $(CPU)"
	@echo "pkglibdir: $(pkglibdir)"
