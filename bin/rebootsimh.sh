#!/bin/sh

# tell simh to exit. The packaged service sets PIDP11_RUNDIR=/run/pidp11;
# the original /opt/pidp11 install keeps using /dev/shm/pidp11.
echo exit > "${PIDP11_RUNDIR:-/dev/shm/pidp11}/tmpsimhcommand.txt"
