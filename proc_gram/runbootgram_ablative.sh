stdsettings="--v3 --to 1500 --no-save-lemmas --inv-templ 0 --nosimpl --grammar lemmas-gram.smt2"

freqhorn="../build/tools/deep/freqhorn"

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
    [ "${benchname%.smt2}" = "$benchname" ] && benchname="${benchname}.smt2"
    benchpath="../bench_horn_rapid/array_altern_$benchname"
    echo "$benchname"
    time -p timeout "$to" "$freqhorn" --altern-ver "$alternver" $stdsettings "$benchpath"
}

for ver in $(cat ./altern-vers.txt)
do
    ln -sf log-bootgram-abl-$ver-$nowtime.txt logs/log-bootgram-abl-$ver-latest.txt
    exec > logs/log-bootgram-abl-$ver-$nowtime.txt 2>&1

    echo "Standard settings: $stdsettings"
    echo "Timeout: $to"
    echo "Freqhorn: $freqhorn"

    date

    for bench in $(ls ../bench_horn_rapid/array_altern_*.smt2)
    do
        benchname="${bench#../bench_horn_rapid/array_altern_}"
        dobench "$ver" "$benchname"
    done
    date
done
