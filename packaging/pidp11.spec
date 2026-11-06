# CalVer: Version is the YYYY.MM release, srcdate the snapshot within it.
# Both segments compare numerically, so 2026.12 > 2026.11 > 2026.9 with no
# padding tricks. Cut a release by bumping Version and srcdate.
#
# Nothing here is derived from git. The commit that records a value is never
# the commit whose value it is, so a git-derived version, release or archive
# name is stale the moment it is committed, and a plain "rpmbuild -bs" then
# goes looking for a tarball nobody ever made. The upstream commit a release
# snapshots belongs in %%changelog, where being wrong is only a stale
# comment.
%global srcdate 20261105

Name:           pidp11
Version:        2026.11
Release:        0.1.%{srcdate}%{?dist}
Summary:        PiDP-11 front panel server and SIMH PDP-11 simulator

# FIXME: unreviewed. The tree mixes SIMH and BlinkenBone (MIT-style), st
# (MIT), and slirp, which has its own permissive notice. The origin and
# licence of the Teletype sound samples is not recorded in the repository.
License:        MIT
URL:            https://github.com/obsolescence/pidp11
# Produced by "make dist" (git archive of the tree, --prefix=%%{srcname}/),
# which takes this name out of the spec with "rpmspec -P" instead of inventing
# its own, so the two cannot drift apart and no --define is needed to build
# the source RPM somewhere that has no checkout. rpmbuild takes the file from
# %%{_sourcedir}; nothing here is ever downloaded.
%global srcname %{name}-%{version}-%{srcdate}
Source0:        %{srcname}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  file
BuildRequires:  pkgconfig
BuildRequires:  libgpiod-devel >= 2.0
BuildRequires:  libtirpc-devel
BuildRequires:  readline-devel
BuildRequires:  libpcap-devel
BuildRequires:  SDL2-devel
BuildRequires:  SDL2_image-devel
BuildRequires:  SDL2_ttf-devel
BuildRequires:  SDL2_mixer-devel
BuildRequires:  libX11-devel
BuildRequires:  libXft-devel
BuildRequires:  fontconfig-devel
BuildRequires:  freetype-devel
BuildRequires:  desktop-file-utils
BuildRequires:  systemd-rpm-macros

# The Blinkenlight server registers with the portmapper.
Requires:       rpcbind
# The console runs in a screen session; pdp11 attaches through sudo.
Requires:       screen
Requires:       sudo
# The Teletype emulator and its desktop entry. Its own subpackage only so
# that the SDL2/X11 stack and the sampled Teletype noise can be left out;
# a panel server with no screen is the case where it is not wanted.
Recommends:     %{name}-sty = %{version}-%{release}
# Period terminals, one for the console ("vt05 pdp11") and -- this is a
# timesharing machine -- more for its other users over the serial lines.
# Their own upstreams and their own RPMs, hence weak deps: on by default,
# because a PDP-11 wants a period-correct terminal, and skippable with
# --setopt=install_weak_deps=False by whoever only drives the front panel.
Recommends:     vt05
Recommends:     tek4010
%{?systemd_requires}

%description
A PDP-11 simulator (SIMH) together with the Blinkenlight server that drives
the PiDP-11 front panel. Two services are installed: the panel server
(pidp11-server, which works even without panel hardware) and the
simulator, whose console is a screen session.

Disk images live in /var/lib/pidp11/systems and are not part of this
package; fetch them with "pidp11-install-systems". Enable and start the
simulator, then attach to its console with "pdp11" (the user must be in
group pidp11).

The console works in any terminal. The recommended vt05 and tek4010
packages give it a period-correct one, and can be pointed at the serial
lines as extra terminals for other users.

%package sty
Summary:        Teletype terminal emulator for the PiDP-11
Requires:       %{name} = %{version}-%{release}

%description sty
sty is an X11 terminal emulator styled after a Teletype, with sampled
Teletype sound. The desktop entry opens it attached to the PiDP-11
console. For other period terminals on the console, or extra terminals
for more users of this multi-user machine, install the vt05 or tek4010
packages.

%prep
%autosetup -n %{srcname}

%build
%set_build_flags
%make_build PREFIX=%{_prefix}

%install
%make_install PREFIX=%{_prefix}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/pidp11-tty.desktop \
    %{buildroot}%{_datadir}/applications/pidp11-vt52.desktop \
    %{buildroot}%{_datadir}/applications/pidp11-vt52-fullscreen.desktop \
    %{buildroot}%{_datadir}/applications/pidp11-tek4010.desktop

%post
systemd-tmpfiles --create %{_tmpfilesdir}/%{name}.conf || :
%systemd_post pidp11-server.service pidp11-simh.service

%preun
%systemd_preun pidp11-simh.service pidp11-server.service

%postun
%systemd_postun_with_restart pidp11-server.service pidp11-simh.service

%files
%doc README.md
%{_bindir}/pdp11
%{_bindir}/pidp11-paneltest
%{_bindir}/pidp11-install-systems
%{_libexecdir}/pidp11/
%{_unitdir}/pidp11-server.service
%{_unitdir}/pidp11-simh.service
%{_sysusersdir}/%{name}.conf
%{_tmpfilesdir}/%{name}.conf
%{_udevrulesdir}/99-pidp11-gpio.rules
%ghost %dir %attr(0755,pidp11,pidp11) /run/pidp11
%ghost %dir %attr(0755,pidp11,pidp11) %{_sharedstatedir}/pidp11
%ghost %dir %attr(0755,pidp11,pidp11) %{_sharedstatedir}/pidp11/systems
%dir %{_sysconfdir}/pidp11
%config(noreplace) %{_sysconfdir}/pidp11/pidp11.conf
# 0440 is what sudo(5) requires for sudoers.d files; see rpmlint.toml for
# the matching "non-readable" filter.
%attr(0440,root,root) %config(noreplace) %{_sysconfdir}/sudoers.d/pidp11
%dir %{_datadir}/pidp11
%{_datadir}/pidp11/systems/
%{_datadir}/pidp11/rsx11bq-boot.ini
%{_datadir}/pidp11/vt52.svg
%{_datadir}/pidp11/tek.svg
# Console launchers for the recommended terminals, which is why they live
# here rather than with pidp11-tty.desktop in the sty subpackage: they need
# vt05 and tek4010, not sty. The TryExec keys hide them from the menu when
# those packages were skipped.
%{_datadir}/applications/pidp11-vt52.desktop
%{_datadir}/applications/pidp11-vt52-fullscreen.desktop
%{_datadir}/applications/pidp11-tek4010.desktop
# brp-compress gzips man pages, so match both spellings.
%{_mandir}/man1/pdp11.1*
%{_mandir}/man1/pidp11-install-systems.1*
%{_mandir}/man1/pidp11-paneltest.1*

%files sty
%doc README.md
%{_bindir}/pidp11-sty
%dir %{_datadir}/pidp11
%{_datadir}/pidp11/sounds/
%{_datadir}/pidp11/tty.svg
%{_datadir}/applications/pidp11-tty.desktop
%{_mandir}/man1/pidp11-sty.1*

%changelog
* Thu Nov 05 2026 Dennis Gilmore <dennis@ausil.us> - 2026.11-0.1.20261105
- Snapshot of upstream 44a98d6.
- Recommend the period terminal packages (vt05, tek4010) and the sty
  subpackage, rather than leaving them to be discovered in the README
- CalVer Version (YYYY.MM) with a date in Release telling the snapshots of
  one month apart, and nothing in the spec derived from git, so the source
  RPM builds outside the checkout with no macros passed
