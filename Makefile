# Top-level build and install for the PiDP-11 software.
#
#   make                    build everything into ./build
#   make install DESTDIR=/tmp/root PREFIX=/usr
#
# Builds are out of tree (into $(BUILDDIR)); the prebuilt binaries tracked
# in the repository are never touched. CFLAGS/LDFLAGS from the environment
# (e.g. rpmbuild's) are honoured.
#
# Needs: gcc, make, pkg-config, libgpiod-devel, libtirpc-devel,
# readline-devel, libpcap-devel, SDL2-devel; and for sty also
# SDL2_image/SDL2_ttf/SDL2_mixer-devel, libX11/libXft-devel, fontconfig and
# freetype devel.

PREFIX      ?= /usr
BINDIR      ?= $(PREFIX)/bin
LIBEXECDIR  ?= $(PREFIX)/libexec/pidp11
DATADIR     ?= $(PREFIX)/share/pidp11
APPSDIR     ?= $(PREFIX)/share/applications
SYSCONFDIR  ?= /etc
UNITDIR     ?= $(PREFIX)/lib/systemd/system
SYSUSERSDIR ?= $(PREFIX)/lib/sysusers.d
TMPFILESDIR ?= $(PREFIX)/lib/tmpfiles.d
UDEVDIR     ?= $(PREFIX)/lib/udev/rules.d
SUDOERSDIR  ?= $(SYSCONFDIR)/sudoers.d
STATEDIR    ?= /var/lib/pidp11

BUILDDIR    ?= build
# set WITH_STY=no to skip the Teletype terminal (needs several SDL2 libs)
WITH_STY    ?= yes

CC          ?= gcc
CFLAGS      ?= -O2 -g
B           := $(abspath $(BUILDDIR))

SERVERDIR   := src/11_pidp_server
SIMHSRC     := src/02.3_simh/4.x+realcons/src

# Rewrite the install locations baked into packaging files and launchers.
SUBST = sed -e 's|/usr/libexec/pidp11|$(LIBEXECDIR)|g' \
            -e 's|/usr/share/pidp11|$(DATADIR)|g' \
            -e 's|/var/lib/pidp11|$(STATEDIR)|g' \
            -e 's|/etc/pidp11|$(SYSCONFDIR)/pidp11|g'

all: server scansw paneltest simh $(if $(filter yes,$(WITH_STY)),sty)

$(B):
	mkdir -p $(B)

# Blinkenlight RPC server for the front panel
server: | $(B)
	$(MAKE) -C $(SERVERDIR)/pidp11 MAKE_TARGET_ARCH=RPI OBJDIR=$(B) \
	    CC_DBG_FLAGS='$(CFLAGS)' EXTRA_LDFLAGS='$(LDFLAGS)' \
	    OS_CCDEFS='-DPIDP11_LIBEXECDIR=\"$(LIBEXECDIR)\"' \
	    pidp1170_blinkenlightd

# Front panel SR switch reader
scansw: | $(B)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(B)/scansw \
	    $(SERVERDIR)/scanswitch/scansw.c $(SERVERDIR)/gpiohdr/gpiohdr.c \
	    $$(pkg-config --cflags --libs libgpiod)

# Front panel LED/switch tester
paneltest: | $(B)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(B)/paneltest \
	    $(SERVERDIR)/paneltest/paneltest.c $(SERVERDIR)/gpiohdr/gpiohdr.c \
	    $$(pkg-config --cflags --libs libgpiod)

# SIMH with the Blinkenlight console ("realcons")
simh: | $(B)
	$(MAKE) -C '$(SIMHSRC)' -f quickmake BIN=$(B)/ \
	    CC_DBG_FLAGS='$(CFLAGS) -Wno-unused-result' EXTRA_LDFLAGS='$(LDFLAGS)'

# Teletype terminal emulator. Builds in the source tree (st's Makefile).
sty: | $(B)
	$(MAKE) -C src/sty CPPFLAGS='-DPIDP11_SOUNDDIR=\"$(DATADIR)/sounds\"'
	cp src/sty/sty $(B)/sty

# Install what "make" built; deliberately does not rebuild (an RPM build
# runs "make install" in a different environment from "make").
install:
	@test -x $(B)/pidp1170_blinkenlightd -a -x $(B)/pdp11_realcons -a -x $(B)/scansw \
	    -a -x $(B)/paneltest \
	    || { echo "Nothing built in $(B): run 'make' first"; exit 1; }
	install -d $(DESTDIR)$(LIBEXECDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(DATADIR)/sounds \
	    $(DESTDIR)$(DATADIR)/systems/idled $(DESTDIR)$(SYSCONFDIR)/pidp11 \
	    $(DESTDIR)$(UNITDIR) $(DESTDIR)$(SYSUSERSDIR) $(DESTDIR)$(TMPFILESDIR) \
	    $(DESTDIR)$(UDEVDIR) $(DESTDIR)$(SUDOERSDIR)
	install -m 0755 $(B)/pidp1170_blinkenlightd $(B)/pdp11_realcons $(B)/scansw \
	    $(DESTDIR)$(LIBEXECDIR)/
	install -m 0755 $(B)/paneltest $(DESTDIR)$(BINDIR)/pidp11-paneltest
	install -m 0755 bin/rebootsimh.sh $(DESTDIR)$(LIBEXECDIR)/
	$(SUBST) packaging/pidp11-run > $(DESTDIR)$(LIBEXECDIR)/pidp11-run
	chmod 0755 $(DESTDIR)$(LIBEXECDIR)/pidp11-run
	install -m 0755 etc/pdp.sh $(DESTDIR)$(BINDIR)/pdp11
	install -m 0644 bin/sounds/*.wav $(DESTDIR)$(DATADIR)/sounds/
	install -m 0644 systems/selections $(DESTDIR)$(DATADIR)/systems/
	install -m 0644 systems/idled/boot.ini $(DESTDIR)$(DATADIR)/systems/idled/
	install -m 0644 packaging/pidp11.conf $(DESTDIR)$(SYSCONFDIR)/pidp11/pidp11.conf
	for u in pidp11-server pidp11-simh; do \
	    $(SUBST) packaging/$$u.service > $(DESTDIR)$(UNITDIR)/$$u.service; \
	    chmod 0644 $(DESTDIR)$(UNITDIR)/$$u.service; done
	install -m 0644 packaging/pidp11.sysusers $(DESTDIR)$(SYSUSERSDIR)/pidp11.conf
	$(SUBST) packaging/pidp11.tmpfiles > $(DESTDIR)$(TMPFILESDIR)/pidp11.conf
	chmod 0644 $(DESTDIR)$(TMPFILESDIR)/pidp11.conf
	install -m 0644 packaging/70-pidp11-gpio.rules $(DESTDIR)$(UDEVDIR)/
	install -m 0440 packaging/pidp11.sudoers $(DESTDIR)$(SUDOERSDIR)/pidp11
ifeq ($(WITH_STY),yes)
	install -d $(DESTDIR)$(APPSDIR)
	install -m 0755 $(B)/sty $(DESTDIR)$(BINDIR)/pidp11-sty
	sed -e 's|/opt/pidp11/bin/sty|$(BINDIR)/pidp11-sty|' \
	    -e 's|/opt/pidp11/install|$(DATADIR)|' install/tty.desktop \
	    > $(DESTDIR)$(APPSDIR)/pidp11-tty.desktop
	install -m 0644 install/tty.svg $(DESTDIR)$(DATADIR)/
endif

clean:
	rm -rf $(B)
	-$(MAKE) -C src/sty clean

.PHONY: all server scansw paneltest simh sty install clean
