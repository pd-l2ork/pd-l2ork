# Makefile for mylib

lib.name = disis

class.sources = disis_phasor~.c
disis_munger~.class.sources = disis_munger.c ADSR.c
define forLinux
    class.sources += disis_netsend.c disis_netreceive.c
    disis_wiimote.class.sources = disis_wiimote.c
    disis_wiimote.class.ldlibs = ./xwiimote/lib/libxwiimote.a -lbluetooth -lpthread -ludev
endef
define forDarwin
    class.sources += disis_netsend.c disis_netreceive.c
endef

datafiles = disis_munger~-help.pd disis_phasor~-help.pd disis_netsend-help.pd disis_netreceive-help.pd disis_wiimote-help.pd disis_wiimote_legacy.pd README patch_name-help.pd patch_name.pd disis-meta.pd

include Makefile.pdlibbuilder.revised
