PURR_DIR = ../../../..
LIBPD_DIR = $(PURR_DIR)/libpd
PD_DIR = $(PURR_DIR)/pd
ABSTR_DIR = $(PURR_DIR)/abstractions
EXTER_DIR = $(PURR_DIR)/externals
EXTRA_DIR = $(PD_DIR)/extra
DOC_DIR = $(PD_DIR)/doc
DST_PD_DIR = purr-data
DST_EXT_DIR = $(DST_PD_DIR)/externals
DST_DOC_DIR = $(DST_PD_DIR)/doc
OUTPUT_FILES = $(TARGET) pdtest.js pdtest.wasm pdtest.data
SRC_FILE = pdtest.cpp
TARGET = pdtest.html

CFLAGS = -I$(PD_DIR)/src -I$(LIBPD_DIR)/libpd_wrapper -DDST_EXT_DIR="\"$(DST_EXT_DIR)\"" -DDST_DOC_DIR="\"$(DST_DOC_DIR)\"" -O3
LDFLAGS = -L$(LIBPD_DIR)/libs -lpd -lm

.PHONY: clean pd build

all: clean pd build

pd: pd-mkdir doc-cp adaptive arraysize autotune~ bassemu~ bob~ bonk~ bsaylor choice comport controctopus creb cxc disis earplug~ ekext ext13 fftease fiddle~ flatgui freeverb~ ggee hcs iem_adaptfilt iem_ambi iem_bin_ambi iem_delay iem_roomsim iem_spec2 iem_tab iemguts iem16 iemlib jasch_lib jmmmp la-kitchen libdir list-abs loop~ lrshift~ mapping markex maxlib memento memento-p mjlib moonlib motex mrpeach nsend pan pd-wavelet pddp pique pixeltango plugin~ purepd rjlib rradical rtc sfruit sigmund~ sigpack smlib stdout timestretch tof vbap windowing zexy pd-clean

pd-mkdir: $(ABSTR_DIR) $(EXTER_DIR) $(EXTRA_DIR) $(DOC_DIR)
	rm -rf $(DST_PD_DIR)
	mkdir -p $(DST_EXT_DIR) $(DST_DOC_DIR)

doc-cp: $(DOC_DIR)/* $(DST_DOC_DIR)
	cp -rf $^

adaptive: $(EXTER_DIR)/grh/adaptive $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/adaptive
	cp -rf $</doc/* $(DST_EXT_DIR)/adaptive
	cp -rf $</examples $(DST_EXT_DIR)/adaptive

arraysize: $(EXTER_DIR)/arraysize $(DST_EXT_DIR)
	cp -rf $^

autotune~: $(EXTER_DIR)/autotune $(DST_EXT_DIR)
	cp -rf $^
	mv $(DST_EXT_DIR)/autotune $(DST_EXT_DIR)/autotune~
	rm -f $(DST_EXT_DIR)/autotune~/autotune_scale_warp.png

bassemu~: $(EXTER_DIR)/bassemu~ $(DST_EXT_DIR)
	cp -rf $^

bob~: $(EXTRA_DIR)/bob~ $(DST_EXT_DIR)
	cp -rf $^

bonk~: $(EXTRA_DIR)/bonk~ $(DST_EXT_DIR)
	cp -rf $^

bsaylor: $(EXTER_DIR)/bsaylor $(DST_EXT_DIR)
	cp -rf $^

choice: $(EXTRA_DIR)/choice $(DST_EXT_DIR)
	cp -rf $^

comport: $(EXTER_DIR)/iem/comport/comport $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/comport/makefile_win

controctopus: $(ABSTR_DIR)/sfruit/controctopus $(DST_EXT_DIR)
	cp -rf $^

creb: $(EXTER_DIR)/creb $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/creb
	cp -rf $</abs/* $(DST_EXT_DIR)/creb
	cp -rf $</doc/* $(DST_EXT_DIR)/creb
	rm -rf $(DST_EXT_DIR)/creb/reference.txt

cxc: $(EXTER_DIR)/cxc $(DST_EXT_DIR)
	cp -rf $^

disis: $(EXTER_DIR)/disis $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/disis/cwiid

earplug~: $(EXTER_DIR)/earplug~ $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/earplug~/earplug_data.txt
	rm -rf $(DST_EXT_DIR)/earplug~/parse-to-h.pl

ekext: $(EXTER_DIR)/ekext $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/ekext/peakit~_license.txt

ext13: $(EXTER_DIR)/ext13 $(DST_EXT_DIR)
	cp -rf $^

fftease: $(EXTER_DIR)/fftease $(DST_EXT_DIR)
	cp -rf $^
	cp -rf $(DST_EXT_DIR)/fftease/fftease32-helpfiles/* $(DST_EXT_DIR)/fftease
	rm -rf $(DST_EXT_DIR)/fftease/fftease32-helpfiles
	rm -rf $(DST_EXT_DIR)/fftease/collect.pl

fiddle~: $(EXTRA_DIR)/fiddle~ $(DST_EXT_DIR)
	cp -rf $^

flatgui: $(EXTER_DIR)/footils/knob $(DST_EXT_DIR)
	cp -rf $^
	mv $(DST_EXT_DIR)/knob $(DST_EXT_DIR)/flatgui

freeverb~: $(EXTER_DIR)/freeverb~ $(DST_EXT_DIR)
	cp -rf $^

ggee: $(EXTER_DIR)/ggee $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/ggee
	cp -rf $</control/* $(DST_EXT_DIR)/ggee
	cp -rf $</filters/* $(DST_EXT_DIR)/ggee
	cp -rf $</signal/* $(DST_EXT_DIR)/ggee

hcs: $(EXTER_DIR)/hcs $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/hcs/general
	rm -rf $(DST_EXT_DIR)/hcs/grabbag
	rm -rf $(DST_EXT_DIR)/hcs/gui
	rm -rf $(DST_EXT_DIR)/hcs/regression
	rm -rf $(DST_EXT_DIR)/hcs/usbhid
	rm -rf $(DST_EXT_DIR)/hcs/README-ifeel.txt

iem_adaptfilt: $(EXTER_DIR)/iem/iem_adaptfilt/help $(DST_EXT_DIR)
	cp -rf $^
	mv $(DST_EXT_DIR)/help $(DST_EXT_DIR)/iem_adaptfilt

iem_ambi: $(EXTER_DIR)/iem/iem_ambi $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/iem_ambi/src

iem_bin_ambi: $(EXTER_DIR)/iem/iem_bin_ambi $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/iem_bin_ambi/src

iem_delay: $(EXTER_DIR)/iem/iem_delay $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/iem_delay/src

iem_roomsim: $(EXTER_DIR)/iem/iem_roomsim $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/iem_roomsim/src

iem_spec2: $(EXTER_DIR)/iem/iem_spec2 $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/iem_spec2/src

iem_tab: $(EXTER_DIR)/iem/iem_tab $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/iem_tab/src

iemguts: $(EXTER_DIR)/iem/iemguts $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/iemguts
	cp -rf $</examples/* $(DST_EXT_DIR)/iemguts
	cp -rf $</help/* $(DST_EXT_DIR)/iemguts

iem16: $(EXTER_DIR)/iem16/help $(DST_EXT_DIR)
	cp -rf $^
	mv $(DST_EXT_DIR)/help $(DST_EXT_DIR)/iem16

iemlib: $(EXTER_DIR)/iemlib $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/iemlib
	cp -rf $</iemlib1/* $(DST_EXT_DIR)/iemlib
	cp -rf $</iemlib2/* $(DST_EXT_DIR)/iemlib
	rm -rf $(DST_EXT_DIR)/iemlib/src

jasch_lib: $(EXTER_DIR)/jasch_lib $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/jasch_lib
	cp -rf $</detox/* $(DST_EXT_DIR)/jasch_lib
	cp -rf $</memchr/* $(DST_EXT_DIR)/jasch_lib
	cp -rf $</strchr/* $(DST_EXT_DIR)/jasch_lib
	cp -rf $</strcut/* $(DST_EXT_DIR)/jasch_lib
	cp -rf $</strlen/* $(DST_EXT_DIR)/jasch_lib
	cp -rf $</strtok/* $(DST_EXT_DIR)/jasch_lib
	cp -rf $</underscore/* $(DST_EXT_DIR)/jasch_lib

jmmmp: $(ABSTR_DIR)/jmmmp $(DST_EXT_DIR)
	cp -rf $^

la-kitchen: $(ABSTR_DIR)/La-kitchen $(DST_EXT_DIR)
	cp -rf $^
	mv $(DST_EXT_DIR)/La-kitchen $(DST_EXT_DIR)/la-kitchen
	rm -rf $(DST_EXT_DIR)/la-kitchen/readme.txt

libdir: $(EXTER_DIR)/loaders/libdir $(DST_EXT_DIR)
	cp -rf $^

list-abs: $(ABSTR_DIR)/footils/list-abs $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/list-abs/debian
	rm -rf $(DST_EXT_DIR)/list-abs/list-abs-intro.txt

loop~: $(EXTRA_DIR)/loop~ $(DST_EXT_DIR)
	cp -rf $^

lrshift~: $(EXTRA_DIR)/lrshift~ $(DST_EXT_DIR)
	cp -rf $^

mapping: $(EXTER_DIR)/mapping $(DST_EXT_DIR)
	cp -rf $^

markex: $(EXTER_DIR)/markex $(DST_EXT_DIR)
	cp -rf $^

maxlib: $(EXTER_DIR)/maxlib $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/maxlib/average.c.diff
	rm -rf $(DST_EXT_DIR)/maxlib/automata.txt
	rm -rf $(DST_EXT_DIR)/maxlib/examplescore.txt
	rm -rf $(DST_EXT_DIR)/maxlib/HISTORY

memento: $(ABSTR_DIR)/rradical/memento $(DST_EXT_DIR)
	cp -rf $^

memento-p: $(ABSTR_DIR)/sfruit/memento-p $(DST_EXT_DIR)
	cp -rf $^

mjlib: $(EXTER_DIR)/mjlib $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.ilk
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.lib
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.ncb
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.opt
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.pdb
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.plg
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.suo
	rm -rf $(DST_EXT_DIR)/mjlib/mjLib.exp
	rm -rf $(DST_EXT_DIR)/mjlib/vc70.pdb
	rm -rf $(DST_EXT_DIR)/mjlib/VERSION

moonlib: $(EXTER_DIR)/moonlib $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/moonlib/XFS.txt

motex: $(EXTER_DIR)/motex $(DST_EXT_DIR)
	cp -rf $^

mrpeach: $(EXTER_DIR)/mrpeach $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/mrpeach
	cp -rf $</binfile/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</cmos/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</flist2tab/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</life2x/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</midifile/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</net/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</op~/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</osc/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</rc~/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</rcosc~/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</rojo~/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</runningmean/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</serializer/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</slipdec/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</slipenc/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</sqosc~/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</str/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</tab2flist/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</tabfind/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</which/* $(DST_EXT_DIR)/mrpeach
	cp -rf $</xbee/* $(DST_EXT_DIR)/mrpeach

nsend: $(ABSTR_DIR)/sfruit/nsend $(DST_EXT_DIR)
	cp -rf $^

pan: $(EXTER_DIR)/pan $(DST_EXT_DIR)
	cp -rf $^

pd-wavelet: $(ABSTR_DIR)/pd-wavelet $(DST_EXT_DIR)
	cp -rf $^
	cp -rf $(DST_EXT_DIR)/pd-wavelet/abs/* $(DST_EXT_DIR)/pd-wavelet
	rm -rf $(DST_EXT_DIR)/pd-wavelet/abs

pddp: $(EXTER_DIR)/pddp $(DST_EXT_DIR)
	cp -rf $^

pique: $(EXTRA_DIR)/pique $(DST_EXT_DIR)
	cp -rf $^

pixeltango: $(ABSTR_DIR)/pixelTANGO $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/pixeltango
	cp -rf $</abstractions/* $(DST_EXT_DIR)/pixeltango
	cp -rf $</help/* $(DST_EXT_DIR)/pixeltango
	cp -rf $</Example-Patches $(DST_EXT_DIR)/pixeltango
	mv $(DST_EXT_DIR)/pixeltango/Example-Patches $(DST_EXT_DIR)/pixeltango/examples

plugin~: $(EXTER_DIR)/plugin~ $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/plugin~/ChangeLog
	rm -rf $(DST_EXT_DIR)/plugin~/SConstruct

purepd: $(ABSTR_DIR)/purepd $(DST_EXT_DIR)
	cp -rf $^

rjlib: $(EXTER_DIR)/rjlib $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/rjlib/src/makefile_mingw

rradical: $(ABSTR_DIR)/rradical $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/rradical
	cp -rf $</control/* $(DST_EXT_DIR)/rradical
	cp -rf $</effects/* $(DST_EXT_DIR)/rradical
	cp -rf $</instruments/* $(DST_EXT_DIR)/rradical
	cp -rf $</memento/* $(DST_EXT_DIR)/rradical
	cp -rf $</stuff/* $(DST_EXT_DIR)/rradical
	cp -rf $(DST_EXT_DIR)/rradical/mafm/* $(DST_EXT_DIR)/rradical
	cp -rf $(DST_EXT_DIR)/rradical/speakerboxx/* $(DST_EXT_DIR)/rradical
	rm -rf $(DST_EXT_DIR)/rradical/dollars.txt
	rm -rf $(DST_EXT_DIR)/rradical/examples
	rm -rf $(DST_EXT_DIR)/rradical/mafm
	rm -rf $(DST_EXT_DIR)/rradical/speakerboxx
	rm -rf $(DST_EXT_DIR)/rradical/tutorial

rtc: $(ABSTR_DIR)/footils/rtc-lib/rtc $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/rtc/INFROW1024.tab

sfruit: $(ABSTR_DIR)/sfruit/sfruit $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/sfruit/clavier.crikey
	rm -rf $(DST_EXT_DIR)/sfruit/drumpads.crikey

sigmund~: $(EXTRA_DIR)/sigmund~ $(DST_EXT_DIR)
	cp -rf $^

sigpack: $(EXTER_DIR)/sigpack $(DST_EXT_DIR)
	cp -rf $^

smlib: $(EXTER_DIR)/smlib $(DST_EXT_DIR)
	cp -rf $^

stdout: $(EXTRA_DIR)/stdout $(DST_EXT_DIR)
	cp -rf $^

timestretch: $(ABSTR_DIR)/timestretch $(DST_EXT_DIR)
	cp -rf $^

tof: $(EXTER_DIR)/tof/tof $(DST_EXT_DIR)
	cp -rf $^

vbap: $(EXTER_DIR)/vbap $(DST_EXT_DIR)
	cp -rf $^
	rm -rf $(DST_EXT_DIR)/vbap/so_locations

windowing: $(EXTER_DIR)/windowing $(DST_EXT_DIR)
	cp -rf $^

zexy: $(EXTER_DIR)/zexy $(DST_EXT_DIR)
	mkdir -p $(DST_EXT_DIR)/zexy
	cp -rf $</abs/* $(DST_EXT_DIR)/zexy
	cp -rf $</reference/* $(DST_EXT_DIR)/zexy
	rm -rf $(DST_EXT_DIR)/zexy/0.INTRO.txt
	rm -rf $(DST_EXT_DIR)/zexy/alias
	rm -rf $(DST_EXT_DIR)/zexy/test.mtx

pd-clean: $(DST_PD_DIR)
	find $< -type f \( -name \*.c -o -name \*.cc -o -name \*.cpp -o -name \*.h -o -name \*.o -o -name \*.pd_darwin -o -name \*.pd_linux -o -name \*.dll -o -name \*.so -o -name \*.l_arm -o -name \*.bat -o -name \Makefile -o -name \makefile -o -name \Makefile.* -o -name \makefile.* -o -name \GNUmakefile.* -o -name \README -o -name \README.txt -o -name \READ_ME.txt -o -name \LICENSE.txt -o -name \COPYING.txt -o -name \GnuGPL.txt -o -name \REFERENCES -o -name \TODO -o -name \TODO.txt -o -name \CHANGE -o -name \CHANGES -o -name \CHANGE.txt -o -name \CHANGES.txt -o -name \CHANGELOG.txt -o -name \*.dsp -o -name \*.dsw -o -name \*.vcproj -o -name \*.sln \) -delete

build: $(SRC_FILE)
	emcc $(CFLAGS) --bind -o $(TARGET) $(SRC_FILE) --closure 1 \
	-s USE_SDL=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ALLOW_MEMORY_GROWTH=1 \
	-s FORCE_FILESYSTEM=1 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['FS']" \
	--no-heap-copy --preload-file $(DST_PD_DIR) $(LDFLAGS)

clean:
	rm -rf $(OUTPUT_FILES)
	rm -rf $(DST_PD_DIR)