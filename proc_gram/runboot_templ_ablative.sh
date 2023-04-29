stdsettings="--v3 --inv-templ 0 --data --to 1500 --nosimpl --no-save-lemmas"

[ -z "$freqhorn" ] && freqhorn="../build/tools/deep/freqhorn"

to=300

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs

# 1: Altern-ver
# 2: Benchmark name (without prefix 'array_altern'
dobench() {
    alternver="$1"
    benchname="$2"
    benchpath="../bench_horn_rapid/array_altern_$benchname.smt2"
    echo "$benchname"
    savelemmas=""
    if [ "${benchname%_*}" = "count_occs" ]
    then
      savelemmas="--save-lemmas"
    fi
    cpp -P -I templgrams templgrams/$benchname.smt2 > tmptemplgram.smt2
    time -p timeout "$to" "$freqhorn" --altern-ver "$alternver" --grammar tmptemplgram.smt2 $stdsettings "$benchpath"
}

dover() {
    ver=$1
    ln -sf log-boottempl-abl-$ver-$nowtime.txt logs/log-boottempl-abl-$ver-latest.txt
    exec > logs/log-boottempl-abl-$ver-$nowtime.txt 2>&1

    echo "Standard settings: $stdsettings"
    echo "Timeout: $to"
    echo "Freqhorn: $freqhorn"

    date

    for bench in $(ls ../bench_horn_rapid/array_altern_*.smt2)
    do
        benchname="${bench#../bench_horn_rapid/array_altern_}"
        benchname="${benchname%.smt2}"
        dobench "$ver" "$benchname"
    done
    date
}

if [ $# -eq 1 ]
then
    dover "$1"
else
  for ver in $(cat ./altern-vers.txt)
  do
      dover "$ver"
  done
fi
