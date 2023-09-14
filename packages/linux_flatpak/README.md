# Pd-L2Ork packaged for Flatpak

This is a Flatpak for [Pd-L2Ork](http://l2ork.music.vt.edu/main/make-your-own-l2ork/software/).

Pd-L2Ork is a popular fork of the [Pure Data](http://puredata.info/), an open
source visual programming language for multimedia.

## How to build

This Flatpak uses the standard
[flatpak-builder](docs.flatpak.org/en/latest/flatpak-builder-command-reference.html)
tool to build.

Before you try to build pd-l2ork, on systems like Ubuntu 22.04 with 8GB RAM, or less, you will want ot increase your tmpfs drive size, as the compilation may take a lot of memory and may result in compilation mysteriously failing at various points in time for no apparent reason (because the process has run out of memory). To do this, go to /etc/system.d and as a superuser edit logind.conf and find the following line:

    #RuntimeDirectorySize=10%

Change that line to (note the uncommenting of the line by removing # at the beginning of the line):

    RuntimeDirectorySize=30%

Doing so will now allocate more RAM to your tmpfs virtual drive, 30% of it to be exact. Depending on the amount of RAM, you may not need to do this at all, or may make percentage higher. It is not recommended that you exceed 50%, as you could starve your OS of memory, which could result in a crash.

You can run a command like the following to build the package from this repo
and install it in your 'user' Flatpak installation:

    flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
    flatpak install flathub org.freedesktop.Platform//22.08 org.freedesktop.Sdk//22.08
    (press Y to continue)
    git config --global --add protocol.file.allow always
    export FLATPAK_BUILDER_N_JOBS=1 && flatpak-builder --verbose --install ./build net.pdl2ork.PdL2Ork.yml --force-clean --user > build.log 2>&1

During development you can also run a build without installing it, like this:

    flatpak-builder --run build net.pdl2ork.PdL2Ork.yml pd-l2ork

See the [Flatpak manual](http://docs.flatpak.org/en/latest/) for more information.
