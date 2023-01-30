#!/bin/sh

if [ $# -lt 2 ]
then
  echo "Usage: $(basename "$0") gram.smt2 prog.smt2"
  exit 1
fi

stdsettings="--v1 --maxrecdepth 1 --debug 2 --gen_method newtrav --trav_type striped --trav_priority sfs --to 10000 --no-save-lemmas --inv-templ 0"

thisdir="$(realpath "$(dirname "$0")")"

freqhorn="$thisdir/../build/tools/deep/freqhorn"

to=300

trap 'kill -KILL -0' QUIT TERM INT

gram="$1"
prog="$2"
cpp -P -I"$thisdir/proc_gram" "$gram" | \
    time -p "$freqhorn" --grammar /dev/stdin $stdsettings "$prog"
