#
# This is a grand unifying Makefile for compiling Pd-extended under MinGW
#
all: install

export CC = gcc
CWD := $(shell pwd)

DESTDIR = $(CWD)/build
cvs_root_dir := $(shell cd $(CWD)/../.. && pwd)
BUILDLAYOUT_DIR = $(cvs_root_dir)/packages


include $(BUILDLAYOUT_DIR)/Makefile.buildlayout


# Generic x86 (tune for Pentium III, since that's most common these days)
#OPT_CFLAGS += -mcpu=i586 -mtune=pentium3


# these are sent to all of the various Makefiles so that they all copy their
# output to the same directory tree
DEST_PATHS = BUILDLAYOUT_DIR=$(BUILDLAYOUT_DIR) \
				cvs_root_dir=$(cvs_root_dir) \
				DESTDIR=$(DESTDIR) \
				prefix=$(prefix) \
				OPT_CFLAGS="$(OPT_CFLAGS)" \
				UNAME=$(UNAME)

ifeq ($(LIGHT),yes)
PD_INNO_SETUP = pd-inno-light.iss
else
PD_INNO_SETUP = pd-inno.iss
endif

package:  $(PD_INNO_SETUP)
## this pops up the GUI app
#	start $(PD_INNO_SETUP)
## this compiles the package on the command line. On 64-bit Windows
#  we have to look in a different path, hence the ugly ifeq below...
ifeq (,$(wildcard /c/Program\ Files/Inno\ Setup\ 6/ISCC.exe))
	/c/Program\ Files\ \(x86\)/Inno\ Setup\ 6/ISCC.exe $(PD_INNO_SETUP)
else
	/c/Program\ Files/Inno\ Setup\ 6/ISCC.exe $(PD_INNO_SETUP)
endif
	@echo " "
	@echo "win64_inno install succeeded!"

git_version: $(pd_src)/src/s_version.h.in
	@cd $(pd_src)/src/ && \
	sed 's|^\(#define PD_BUILD_VERSION "\).*"|\1$(PD_BUILD_VERSION)"|' s_version.h.in > s_version.h.new && \
	(test -f s_version.h && diff -q s_version.h s_version.h.new > /dev/null && rm -f s_version.h.new && echo s_version.h unchanged || (rm -f s_version.h && mv s_version.h.new s_version.h))

build_pd: git_version
	cd $(pd_src)/src && $(MAKE) -f makefile.mingw

pd_install: build_pd
# the autoconf/MinGW setup doesn't compile the extras yet
#	$(MAKE) -C $(pd_src)/src $(DEST_PATHS) bin
#	-$(MAKE) -C $(pd_src)/src $(DEST_PATHS) install
	$(MAKE) -C $(pd_src)/src -f makefile.mingw $(DEST_PATHS) install

rpi_install:
	cd $(cvs_root_dir)/l2ork_addons/raspberry_pi/disis_gpio_dummy && make && cd ../ \
		&& cp -f disis_gpio_dummy/disis_gpio.dll $(DESTDIR)/extra && \
		cp -f disis_gpio/disis_gpio-help.pd $(DESTDIR)/extra && \
		cd disis_spi_dummy && make && cd ../ && \
		cp -f disis_spi_dummy/disis_spi.dll $(DESTDIR)/extra && \
		cp -f disis_spi/disis_spi-help.pd $(DESTDIR)/extra

install: pd_install rpi_install prebuilt_install lib_install exe_install
	@echo " "
	@echo "win64_inno install succeeded!"

prebuilt_install:
	$(MAKE) -C $(packages_src) $(DEST_PATHS) extended_install INCREMENTAL=$(INCREMENTAL) LIGHT=$(LIGHT)
	$(MAKE) -C $(packages_src) $(DEST_PATHS) doc_format
ifeq ($(LIGHT),yes)
	install -p pd-settings-light.reg $(DESTDIR)$(prefix)/pd-settings.reg
else
	install -p pd-settings.reg $(DESTDIR)$(prefix)
	@echo "Adding apps folder..."
	cp -rf $(cvs_root_dir)/apps $(DESTDIR)$(prefix)/
	@echo "Adding K12 abstractions..."
	cp -rf $(cvs_root_dir)/l2ork_addons/K12 $(DESTDIR)$(prefix)/extra/
	@echo "Adding K12 menu icons..."
	install -d $(DESTDIR)$(bindir)/K12-icons/
	cp -rf $(cvs_root_dir)/l2ork_addons/K12/icons-large/* $(DESTDIR)$(bindir)/K12-icons/
	@echo "Adding arduino abstractions..."
	cp -rf $(cvs_root_dir)/externals/hardware/arduino $(DESTDIR)$(libpddir)/extra/
endif

#==============================================================================#
#
## setup Pd binaries
#
#==============================================================================#

#bin_src = /usr/local/bin
bin_src = /mingw64/bin
system32 = /c/WINDOWS/system32
lib_install:
# ultimately, the DLLs should be installed in %SystemRoot%\system32 by
# InnoSetup since they are 'officially' named and versioned DLLs, and can be
# shared between apps
# GUI port note: Pd doesn't seem to be able to find the following libs
# unless they are in $(DESTDIR)$(bindir) instead of $(DESTDIR)$(prefix).
# I figured that out by trial and error. If a Windows expert wants to figure
# out a better way to do it, that's fine-- I'm just trying to get it to build
# at all at this point...
	install -d $(DESTDIR)$(prefix)/
	install -d $(DESTDIR)$(bindir)
	install -p $(bin_src)/libfftw3-3.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libfftw3f-3.dll  $(DESTDIR)$(bindir)/
	#install -p $(bin_src)/libgnurx-0.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libogg-0.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libsndfile-1.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libopus-0.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libvorbis-0.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libvorbisenc-2.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libvorbisfile-3.dll  $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libmp3lame-0.dll  $(DESTDIR)$(bindir)/
ifneq ($(LIGHT),yes)
	# I think these two are needed for zexy/matchbox and zexy/regex
	install -p $(bin_src)/libsystre-0.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libtre-5.dll $(DESTDIR)$(bindir)/
	# For fluid~
	install -p $(bin_src)/libfluidsynth-*.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/SDL2.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libgmodule-2.0-0.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libreadline8.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libtermcap-0.dll $(DESTDIR)$(bindir)/
	# For other external libs
	install -p $(bin_src)/lua*.dll          $(DESTDIR)$(bindir)/
	#install -p $(bin_src)/pthreadGC2.dll     $(DESTDIR)$(bindir)/
	#install -p $(bin_src)/libdl.dll     $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libportaudio*.dll     $(DESTDIR)$(bindir)/
	# ico@vt.edu 2022-08-17: added both versions of libFLAC because
	# libFLAC version 1.3.4-2 does not have "-8" appended to the dll
	-install -p $(bin_src)/libFLAC-8.dll     $(DESTDIR)$(bindir)/
	-install -p $(bin_src)/libFLAC.dll     $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libspeex-1.dll     $(DESTDIR)$(bindir)/
	# These are for Gem
	install -p $(bin_src)/libftgl-2.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libfreetype-6.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libbz2-1.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libharfbuzz-0.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libglib-2.0-0.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libintl-8.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libiconv-2.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libpng16-16.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/zlib1.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libpcre-1.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libgraphite2.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libjpeg-8.dll $(DESTDIR)$(bindir)/
	# ico@vt.edu 2022-12-15: added libraries that are needed for
	# fluid~ even though there is no mention of them anywhere, and
	# had to be identified by copying the entire /mingw32/bin/ folder
	# and then pruning...
	install -p $(bin_src)/libgomp-1.dll $(DESTDIR)$(bindir)/
	install -p $(bin_src)/libpcre2-8-0.dll $(DESTDIR)$(bindir)/
	# ico 2023-08-24: more libraries needed (msys keeps moving the goal posts)
	install -p $(bin_src)/libmpg123-0.dll $(DESTDIR)$(bindir)/
	# New: lossless compression algorithm. Gem.dll won't load without it.
	# Cf.: https://github.com/bagder/libbrotli. Looks like this recently
	# got added to MSYS2. Apparently Gem needs only the decoder , but we
	# include the entire shebang just in case.
	install -p $(bin_src)/libbrotli*.dll $(DESTDIR)$(bindir)/
	# Apparently these Gem dependencies must be in the bindir
	# to be found on Windows
	-install -p $(DESTDIR)/extra/Gem/gem_film*.dll $(DESTDIR)$(bindir)/
	-install -p $(DESTDIR)/extra/Gem/gem_image*.dll $(DESTDIR)$(bindir)/
	-install -p $(DESTDIR)/extra/Gem/gem_model*.dll $(DESTDIR)$(bindir)/
	-install -p $(DESTDIR)/extra/Gem/gem_record*.dll $(DESTDIR)$(bindir)/
	-install -p $(DESTDIR)/extra/Gem/gem_video*.dll $(DESTDIR)$(bindir)/
endif
# these stay with pd.exe
# if these are installed, include them in the build root
	-install -p $(system32)/msvcp71.dll   $(DESTDIR)$(bindir)
	-install -p $(system32)/msvcr71.dll   $(DESTDIR)$(bindir)
	#-install -p $(system32)/pthreadVC.dll $(DESTDIR)$(bindir)
	install -p $(bin_src)/libwinpthread-1.dll $(DESTDIR)$(bindir)
	install -p $(bin_src)/libgcc_s_seh-1.dll $(DESTDIR)$(bindir)
	install -p $(bin_src)/libstdc++-6.dll $(DESTDIR)$(bindir)
ifneq ($(LIGHT),yes)
# lyonpotpourri compiles its own shared lib. On Windows it needs to be
# in the path of the pd executable in order to get loaded. (There's probably
# also a way to add the external lib's path but I don't know how to do that.)
	install -p $(DESTDIR)/extra/lyonpotpourri/liblyonpotpourri.dll $(DESTDIR)$(bindir)
# fftease also uses a shared lib...
	install -p $(DESTDIR)/extra/fftease/libfftease.dll $(DESTDIR)$(bindir)
# install dummy disis_wiimote since that object does not compile on Windows
	install -p $(cvs_root_dir)/externals/disis/non-linux/disis_wiimote.pd $(DESTDIR)/extra/
endif


exe_install:
	install -d $(DESTDIR)$(bindir)

$(PD_INNO_SETUP): $(PD_INNO_SETUP).in
	@echo "Making Inno Setup file: $(PD_INNO_SETUP)"
	cat "$(PD_INNO_SETUP).in" | \
		sed 's/PACKAGE_NAME/$(PACKAGE_NAME)/g' | \
		sed 's/PD_VERSION/$(PD_VERSION)/g' | \
		sed 's/PD_BUILD_VERSION/$(PD_BUILD_VERSION)/g' > \
		$(PD_INNO_SETUP)
# in incremental builds the lib directory may not exist, in that case we need
# to edit the setup file to prevent an inno-setup error
	test -d $(DESTDIR)$(libdir) || sed -i -e 's/^Source: build\\lib/;Source: build\\lib/' $(PD_INNO_SETUP)
#	start $(PD_INNO_SETUP)


#==============================================================================#
#
## CLEAN TARGETS
#
#==============================================================================#

distclean: package_clean
	$(MAKE) -C $(packages_src) $(DEST_PATHS) distclean


package_clean: 	
	-rm $(PD_INNO_SETUP)
	-rm -rf $(DESTDIR)
	-rm -rf Output


clean: package_clean
	$(MAKE) -C $(packages_src) $(DEST_PATHS) clean

#==============================================================================#
#
## TEST TARGETS
#
#==============================================================================#

test_package:
	echo "No tests here yet"


test_locations:
	@echo --------------------------------------------------
	@echo "PD_VERSION: $(PD_VERSION)"
	@echo "PD_BUILD_VERSION: $(PD_BUILD_VERSION)"
	@echo "CWD $(CWD)"
	@echo "DESTDIR $(DESTDIR)"
	@echo "PREFIX $(prefix)"
	@echo "BINDIR  $(bindir)"
	@echo "LIBDIR  $(libdir)"
	@echo "OBJECTSDIR  $(objectsdir)"
	@echo "PDDOCDIR  $(pddocdir)"
	@echo "LIBPDDIR  $(libpddir)"
	@echo "LIBPDBINDIR  $(libpdbindir)"
	@echo "HELPDIR  $(helpdir)"
	@echo "MANUALSDIR  $(manualsdir)"
	@echo "EXAMPLESDIR  $(examplesdir)"
	@echo --------------------------------------------------
	autoconf --version
	@echo --------------------------------------------------
	make --version
	@echo --------------------------------------------------
	gcc --version


.PHONY: all install package clean distclean test_locations test_package exe_install
