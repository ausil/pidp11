#!/bin/sh
# Called by the PiDP-11 server when the front panel power switch is turned
# off with HALT up. Cleanly powers the host off: systemd stops
# pidp11-simh (asking SIMH to quit) and the server before powering down.
# The server runs as an unprivileged user; /etc/sudoers.d/pidp11 allows
# exactly this command.
exec sudo -n /usr/bin/systemctl poweroff
