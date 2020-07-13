PURR_DIR = ../../../..
LIBPD_DIR = $(PURR_DIR)/libpd
PD_DIR = $(PURR_DIR)/pd
ABSTR_DIR = $(PURR_DIR)/abstractions
EXTER_DIR = $(PURR_DIR)/externals
EXTRA_DIR = $(PD_DIR)/extra
EXT_DIR = externals
OUTPUT_FILES = $(TARGET) pdtest.js pdtest.wasm pdtest.data

SRC_FILE = pdtest.cpp
TARGET = pdtest.html

CFLAGS = -I$(PD_DIR)/src -I$(LIBPD_DIR)/libpd_wrapper -DEXT_DIR="\"$(EXT_DIR)\"" -O3
LDFLAGS = -L$(LIBPD_DIR)/libs -lpd -lm

.PHONY: clean ext build

all: clean ext build

ext: ext-mkdir adaptive arraysize autotune~ bassemu~ bob~ bonk~ bsaylor choice comport controctopus creb cxc disis earplug~ ekext ext13 fftease fiddle~ flatgui freeverb~ ggee hcs iem_adaptfilt iem_ambi iem_bin_ambi iem_delay iem_roomsim iem_spec2 iem_tab iemguts iem16 iemlib jasch_lib jmmmp la-kitchen libdir list-abs loop~ lrshift~ mapping markex maxlib memento memento-p mjlib moonlib motex mrpeach nsend pan pd-wavelet pddp pique pixeltango plugin~ purepd rjlib rradical rtc sfruit sigmund~ sigpack smlib stdout timestretch tof vbap windowing zexy ext-clean

ext-mkdir: $(EXTER_DIR)
	rm -rf $(EXT_DIR)
	mkdir -p $(EXT_DIR)

adaptive: $(EXTER_DIR)/grh/adaptive $(EXT_DIR)
	mkdir -p $(EXT_DIR)/adaptive
	cp -rf $</doc/* $(EXT_DIR)/adaptive
	cp -rf $</examples $(EXT_DIR)/adaptive

arraysize: $(EXTER_DIR)/arraysize $(EXT_DIR)
	cp -rf $^

autotune~: $(EXTER_DIR)/autotune $(EXT_DIR)
	cp -rf $^
	mv $(EXT_DIR)/autotune $(EXT_DIR)/autotune~
	rm -f $(EXT_DIR)/autotune~/autotune_scale_warp.png

bassemu~: $(EXTER_DIR)/bassemu~ $(EXT_DIR)
	cp -rf $^

bob~: $(EXTRA_DIR)/bob~ $(EXT_DIR)
	cp -rf $^

bonk~: $(EXTRA_DIR)/bonk~ $(EXT_DIR)
	cp -rf $^

bsaylor: $(EXTER_DIR)/bsaylor $(EXT_DIR)
	cp -rf $^

choice: $(EXTRA_DIR)/choice $(EXT_DIR)
	cp -rf $^

comport: $(EXTER_DIR)/iem/comport/comport $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/comport/makefile_win

controctopus: $(ABSTR_DIR)/sfruit/controctopus $(EXT_DIR)
	cp -rf $^

creb: $(EXTER_DIR)/creb $(EXT_DIR)
	mkdir -p $(EXT_DIR)/creb
	cp -rf $</abs/* $(EXT_DIR)/creb
	cp -rf $</doc/* $(EXT_DIR)/creb
	rm -rf $(EXT_DIR)/creb/reference.txt

cxc: $(EXTER_DIR)/cxc $(EXT_DIR)
	cp -rf $^

disis: $(EXTER_DIR)/disis $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/disis/cwiid

earplug~: $(EXTER_DIR)/earplug~ $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/earplug~/earplug_data.txt
	rm -rf $(EXT_DIR)/earplug~/parse-to-h.pl

ekext: $(EXTER_DIR)/ekext $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/ekext/peakit~_license.txt

ext13: $(EXTER_DIR)/ext13 $(EXT_DIR)
	cp -rf $^

fftease: $(EXTER_DIR)/fftease $(EXT_DIR)
	cp -rf $^
	cp -rf $(EXT_DIR)/fftease/fftease32-helpfiles/* $(EXT_DIR)/fftease
	rm -rf $(EXT_DIR)/fftease/fftease32-helpfiles
	rm -rf $(EXT_DIR)/fftease/collect.pl

fiddle~: $(EXTRA_DIR)/fiddle~ $(EXT_DIR)
	cp -rf $^

flatgui: $(EXTER_DIR)/footils/knob $(EXT_DIR)
	cp -rf $^
	mv $(EXT_DIR)/knob $(EXT_DIR)/flatgui

freeverb~: $(EXTER_DIR)/freeverb~ $(EXT_DIR)
	cp -rf $^

ggee: $(EXTER_DIR)/ggee $(EXT_DIR)
	mkdir -p $(EXT_DIR)/ggee
	cp -rf $</control/* $(EXT_DIR)/ggee
	cp -rf $</filters/* $(EXT_DIR)/ggee
	cp -rf $</signal/* $(EXT_DIR)/ggee

hcs: $(EXTER_DIR)/hcs $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/hcs/general
	rm -rf $(EXT_DIR)/hcs/grabbag
	rm -rf $(EXT_DIR)/hcs/gui
	rm -rf $(EXT_DIR)/hcs/regression
	rm -rf $(EXT_DIR)/hcs/usbhid
	rm -rf $(EXT_DIR)/hcs/README-ifeel.txt

iem_adaptfilt: $(EXTER_DIR)/iem/iem_adaptfilt/help $(EXT_DIR)
	cp -rf $^
	mv $(EXT_DIR)/help $(EXT_DIR)/iem_adaptfilt

iem_ambi: $(EXTER_DIR)/iem/iem_ambi $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/iem_ambi/src

iem_bin_ambi: $(EXTER_DIR)/iem/iem_bin_ambi $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/iem_bin_ambi/src

iem_delay: $(EXTER_DIR)/iem/iem_delay $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/iem_delay/src

iem_roomsim: $(EXTER_DIR)/iem/iem_roomsim $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/iem_roomsim/src

iem_spec2: $(EXTER_DIR)/iem/iem_spec2 $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/iem_spec2/src

iem_tab: $(EXTER_DIR)/iem/iem_tab $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/iem_tab/src

iemguts: $(EXTER_DIR)/iem/iemguts $(EXT_DIR)
	mkdir -p $(EXT_DIR)/iemguts
	cp -rf $</examples/* $(EXT_DIR)/iemguts
	cp -rf $</help/* $(EXT_DIR)/iemguts

iem16: $(EXTER_DIR)/iem16/help $(EXT_DIR)
	cp -rf $^
	mv $(EXT_DIR)/help $(EXT_DIR)/iem16

iemlib: $(EXTER_DIR)/iemlib $(EXT_DIR)
	mkdir -p $(EXT_DIR)/iemlib
	cp -rf $</iemlib1/* $(EXT_DIR)/iemlib
	cp -rf $</iemlib2/* $(EXT_DIR)/iemlib
	rm -rf $(EXT_DIR)/iemlib/src

jasch_lib: $(EXTER_DIR)/jasch_lib $(EXT_DIR)
	mkdir -p $(EXT_DIR)/jasch_lib
	cp -rf $</detox/* $(EXT_DIR)/jasch_lib
	cp -rf $</memchr/* $(EXT_DIR)/jasch_lib
	cp -rf $</strchr/* $(EXT_DIR)/jasch_lib
	cp -rf $</strcut/* $(EXT_DIR)/jasch_lib
	cp -rf $</strlen/* $(EXT_DIR)/jasch_lib
	cp -rf $</strtok/* $(EXT_DIR)/jasch_lib
	cp -rf $</underscore/* $(EXT_DIR)/jasch_lib

jmmmp: $(ABSTR_DIR)/jmmmp $(EXT_DIR)
	cp -rf $^

la-kitchen: $(ABSTR_DIR)/La-kitchen $(EXT_DIR)
	cp -rf $^
	mv $(EXT_DIR)/La-kitchen $(EXT_DIR)/la-kitchen
	rm -rf $(EXT_DIR)/la-kitchen/readme.txt

libdir: $(EXTER_DIR)/loaders/libdir $(EXT_DIR)
	cp -rf $^

list-abs: $(ABSTR_DIR)/footils/list-abs $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/list-abs/debian
	rm -rf $(EXT_DIR)/list-abs/list-abs-intro.txt

loop~: $(EXTRA_DIR)/loop~ $(EXT_DIR)
	cp -rf $^

lrshift~: $(EXTRA_DIR)/lrshift~ $(EXT_DIR)
	cp -rf $^

mapping: $(EXTER_DIR)/mapping $(EXT_DIR)
	cp -rf $^

markex: $(EXTER_DIR)/markex $(EXT_DIR)
	cp -rf $^

maxlib: $(EXTER_DIR)/maxlib $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/maxlib/average.c.diff
	rm -rf $(EXT_DIR)/maxlib/automata.txt
	rm -rf $(EXT_DIR)/maxlib/examplescore.txt
	rm -rf $(EXT_DIR)/maxlib/HISTORY

memento: $(ABSTR_DIR)/rradical/memento $(EXT_DIR)
	cp -rf $^

memento-p: $(ABSTR_DIR)/sfruit/memento-p $(EXT_DIR)
	cp -rf $^

mjlib: $(EXTER_DIR)/mjlib $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/mjlib/mjLib.ilk
	rm -rf $(EXT_DIR)/mjlib/mjLib.lib
	rm -rf $(EXT_DIR)/mjlib/mjLib.ncb
	rm -rf $(EXT_DIR)/mjlib/mjLib.opt
	rm -rf $(EXT_DIR)/mjlib/mjLib.pdb
	rm -rf $(EXT_DIR)/mjlib/mjLib.plg
	rm -rf $(EXT_DIR)/mjlib/mjLib.suo
	rm -rf $(EXT_DIR)/mjlib/mjLib.exp
	rm -rf $(EXT_DIR)/mjlib/vc70.pdb
	rm -rf $(EXT_DIR)/mjlib/VERSION

moonlib: $(EXTER_DIR)/moonlib $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/moonlib/XFS.txt

motex: $(EXTER_DIR)/motex $(EXT_DIR)
	cp -rf $^

mrpeach: $(EXTER_DIR)/mrpeach $(EXT_DIR)
	mkdir -p $(EXT_DIR)/mrpeach
	cp -rf $</binfile/* $(EXT_DIR)/mrpeach
	cp -rf $</cmos/* $(EXT_DIR)/mrpeach
	cp -rf $</flist2tab/* $(EXT_DIR)/mrpeach
	cp -rf $</life2x/* $(EXT_DIR)/mrpeach
	cp -rf $</midifile/* $(EXT_DIR)/mrpeach
	cp -rf $</net/* $(EXT_DIR)/mrpeach
	cp -rf $</op~/* $(EXT_DIR)/mrpeach
	cp -rf $</osc/* $(EXT_DIR)/mrpeach
	cp -rf $</rc~/* $(EXT_DIR)/mrpeach
	cp -rf $</rcosc~/* $(EXT_DIR)/mrpeach
	cp -rf $</rojo~/* $(EXT_DIR)/mrpeach
	cp -rf $</runningmean/* $(EXT_DIR)/mrpeach
	cp -rf $</serializer/* $(EXT_DIR)/mrpeach
	cp -rf $</slipdec/* $(EXT_DIR)/mrpeach
	cp -rf $</slipenc/* $(EXT_DIR)/mrpeach
	cp -rf $</sqosc~/* $(EXT_DIR)/mrpeach
	cp -rf $</str/* $(EXT_DIR)/mrpeach
	cp -rf $</tab2flist/* $(EXT_DIR)/mrpeach
	cp -rf $</tabfind/* $(EXT_DIR)/mrpeach
	cp -rf $</which/* $(EXT_DIR)/mrpeach
	cp -rf $</xbee/* $(EXT_DIR)/mrpeach

nsend: $(ABSTR_DIR)/sfruit/nsend $(EXT_DIR)
	cp -rf $^

pan: $(EXTER_DIR)/pan $(EXT_DIR)
	cp -rf $^

pd-wavelet: $(ABSTR_DIR)/pd-wavelet $(EXT_DIR)
	cp -rf $^
	cp -rf $(EXT_DIR)/pd-wavelet/abs/* $(EXT_DIR)/pd-wavelet
	rm -rf $(EXT_DIR)/pd-wavelet/abs

pddp: $(EXTER_DIR)/pddp $(EXT_DIR)
	cp -rf $^

pique: $(EXTRA_DIR)/pique $(EXT_DIR)
	cp -rf $^

pixeltango: $(ABSTR_DIR)/pixelTANGO $(EXT_DIR)
	mkdir -p $(EXT_DIR)/pixeltango
	cp -rf $</abstractions/* $(EXT_DIR)/pixeltango
	cp -rf $</help/* $(EXT_DIR)/pixeltango
	cp -rf $</Example-Patches $(EXT_DIR)/pixeltango
	mv $(EXT_DIR)/pixeltango/Example-Patches $(EXT_DIR)/pixeltango/examples

plugin~: $(EXTER_DIR)/plugin~ $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/plugin~/ChangeLog
	rm -rf $(EXT_DIR)/plugin~/SConstruct

purepd: $(ABSTR_DIR)/purepd $(EXT_DIR)
	cp -rf $^

rjlib: $(EXTER_DIR)/rjlib $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/rjlib/src/makefile_mingw

rradical: $(ABSTR_DIR)/rradical $(EXT_DIR)
	mkdir -p $(EXT_DIR)/rradical
	cp -rf $</control/* $(EXT_DIR)/rradical
	cp -rf $</effects/* $(EXT_DIR)/rradical
	cp -rf $</instruments/* $(EXT_DIR)/rradical
	cp -rf $</memento/* $(EXT_DIR)/rradical
	cp -rf $</stuff/* $(EXT_DIR)/rradical
	cp -rf $(EXT_DIR)/rradical/mafm/* $(EXT_DIR)/rradical
	cp -rf $(EXT_DIR)/rradical/speakerboxx/* $(EXT_DIR)/rradical
	rm -rf $(EXT_DIR)/rradical/dollars.txt
	rm -rf $(EXT_DIR)/rradical/examples
	rm -rf $(EXT_DIR)/rradical/mafm
	rm -rf $(EXT_DIR)/rradical/speakerboxx
	rm -rf $(EXT_DIR)/rradical/tutorial

rtc: $(ABSTR_DIR)/footils/rtc-lib/rtc $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/rtc/INFROW1024.tab

sfruit: $(ABSTR_DIR)/sfruit/sfruit $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/sfruit/clavier.crikey
	rm -rf $(EXT_DIR)/sfruit/drumpads.crikey

sigmund~: $(EXTRA_DIR)/sigmund~ $(EXT_DIR)
	cp -rf $^

sigpack: $(EXTER_DIR)/sigpack $(EXT_DIR)
	cp -rf $^

smlib: $(EXTER_DIR)/smlib $(EXT_DIR)
	cp -rf $^

stdout: $(EXTRA_DIR)/stdout $(EXT_DIR)
	cp -rf $^

timestretch: $(ABSTR_DIR)/timestretch $(EXT_DIR)
	cp -rf $^

tof: $(EXTER_DIR)/tof/tof $(EXT_DIR)
	cp -rf $^

vbap: $(EXTER_DIR)/vbap $(EXT_DIR)
	cp -rf $^
	rm -rf $(EXT_DIR)/vbap/so_locations

windowing: $(EXTER_DIR)/windowing $(EXT_DIR)
	cp -rf $^

zexy: $(EXTER_DIR)/zexy $(EXT_DIR)
	mkdir -p $(EXT_DIR)/zexy
	cp -rf $</abs/* $(EXT_DIR)/zexy
	cp -rf $</reference/* $(EXT_DIR)/zexy
	rm -rf $(EXT_DIR)/zexy/0.INTRO.txt
	rm -rf $(EXT_DIR)/zexy/alias
	rm -rf $(EXT_DIR)/zexy/test.mtx

ext-clean: $(EXT_DIR)
	find $< -type f \( -name \*.c -o -name \*.cc -o -name \*.cpp -o -name \*.h -o -name \*.o -o -name \*.pd_darwin -o -name \*.pd_linux -o -name \*.dll -o -name \*.so -o -name \*.l_arm -o -name \*.bat -o -name \Makefile -o -name \makefile -o -name \Makefile.* -o -name \makefile.* -o -name \GNUmakefile.* -o -name \README -o -name \README.txt -o -name \READ_ME.txt -o -name \LICENSE.txt -o -name \COPYING.txt -o -name \GnuGPL.txt -o -name \REFERENCES -o -name \TODO -o -name \TODO.txt -o -name \CHANGE -o -name \CHANGES -o -name \CHANGE.txt -o -name \CHANGES.txt -o -name \CHANGELOG.txt -o -name \*.dsp -o -name \*.dsw -o -name \*.vcproj -o -name \*.sln \) -delete

build: $(SRC_FILE)
	emcc $(CFLAGS) --bind -o $(TARGET) $(SRC_FILE) --closure 1 \
	-s USE_SDL=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s FORCE_FILESYSTEM=1 \
	-s "EXTRA_EXPORTED_RUNTIME_METHODS=['FS']" --preload-file $(EXT_DIR) $(LDFLAGS)

clean:
	rm -rf $(OUTPUT_FILES)
	rm -rf $(EXT_DIR)