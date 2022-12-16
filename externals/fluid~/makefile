# Makefile for mylib

lib.name = fluid~

class.sources = fluid~.c

ldlibs = -lfluidsynth

define forWindows
    ldlibs = -lfluidsynth -lgomp -lpcre
endef

datafiles = fluid~-help.pd fluid.gen2sym.pd midi-input.pd mts-tuning.pd simple_onthego_synth.pd fluid.gen.txt LICENSE README 

include Makefile.pdlibbuilder.revised
