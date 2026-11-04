# Snapshot build: packaging/build-rpm.sh passes gitrev/gitdate for the commit.
%{!?gitrev: %global gitrev unknown}
%{!?gitdate: %global gitdate 00000000}

Name:           pidp11
Version:        0
Release:        0.1.%{gitdate}git%{gitrev}%{?dist}
Summary:        PiDP-11 front panel server and SIMH PDP-11 simulator

# FIXME: unreviewed. The tree mixes SIMH and BlinkenBone (MIT-style), st
# (MIT), and slirp, which has its own permissive notice. The origin and
# licence of the Teletype sound samples is not recorded in the repository.
License:        MIT
URL:            https://github.com/obsolescence/pidp11
Source0:        pidp11.tar.gz

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
%{?systemd_requires}

%description
A PDP-11 simulator (SIMH) together with the Blinkenlight server that drives
the PiDP-11 front panel, packaged as two systemd services:

  pidp11-server  the panel server (works without panel hardware)
  pidp11-simh    the simulator; its console is a screen session

Disk images live in /var/lib/pidp11/systems and are not part of this
package. Start the simulator with "systemctl enable --now pidp11-simh" and
attach to its console with "pdp11" (the user must be in group pidp11).

%package sty
Summary:        Teletype terminal emulator for the PiDP-11
Requires:       %{name} = %{version}-%{release}

%description sty
sty is an X11 terminal emulator styled after a Teletype, with sound. The
desktop entry opens it attached to the PiDP-11 console.

%prep
%autosetup -n pidp11

%build
%set_build_flags
%make_build PREFIX=%{_prefix}

%install
%make_install PREFIX=%{_prefix}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/pidp11-tty.desktop

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
%attr(0440,root,root) %config(noreplace) %{_sysconfdir}/sudoers.d/pidp11
%dir %{_datadir}/pidp11
%{_datadir}/pidp11/systems/
%{_datadir}/pidp11/rsx11bq-boot.ini

%files sty
%{_bindir}/pidp11-sty
%dir %{_datadir}/pidp11
%{_datadir}/pidp11/sounds/
%{_datadir}/pidp11/tty.svg
%{_datadir}/applications/pidp11-tty.desktop

%changelog
* Tue Nov 03 2026 Dennis Gilmore <dennis@ausil.us> - 0-0.1
- Initial snapshot package
