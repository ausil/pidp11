#!/bin/bash
# Attach to the PiDP-11 console (the screen session named "pidp11").
#
# On a Raspberry Pi the session belongs to the logged-in user, so we use
# screen directly and take over the session (-d -r).
#
# When the packaged system service is installed there is a "pidp11" user
# that owns the session. Other users reach it through sudo (see
# packaging/pidp11.sudoers) and share it with -x so several people can
# attach at once.

svc_user=pidp11
if getent passwd "$svc_user" >/dev/null && [ "$(id -un)" != "$svc_user" ]; then
	scr="sudo -n -u $svc_user /usr/bin/screen"
	attach="-x pidp11"
else
	scr="screen"
	attach="-d -r"
fi

procs=$($scr -ls pidp11 | grep -Ec '[0-9]+\.pidp11')
if [ "$procs" -ne 0 ]; then
	exec $scr $attach
fi
echo "PiDP-11 is not running." >&2
exit 1
