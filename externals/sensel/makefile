# Sensel requires Sensel API, so the user needs to download it from https://github.com/sensel/sensel-api
# Next go to sensel-install inside the downloaded git repository and install the OSX pkg file
# Only then will this build cleanly

sensel.class.sources = sensel.c
sensel.class.ldlibs = -L./ -lsensel
CPPFLAGS += -I./include

datafiles = sensel-help.pd sensel-led.pd sensel_examples

include Makefile.pdlibbuilder.revised
