stdsettings="--v3 --data --grammar forallgram.smt2 --to 1500 --nosimpl --no-save-lemmas --inv-templ 0"

[ -z "$freqhorn" ] && freqhorn="../build/tools/deep/freqhorn"

to=30

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs

# 1: Altern-ver
# 2: Benchmark name
dobench() {
    alternver="$1"
    benchname="$2"
    [ "${benchname%.smt2}" = "$benchname" ] && benchname="${benchname}.smt2"
    benchpath="../bench_horn_multiple/$benchname"
    echo "$benchname"
    time -p timeout "$to" "$freqhorn" --altern-ver "$alternver" $stdsettings "$benchpath"
}

for ver in $(cat ./altern-vers.txt)
do
    ln -sf log-bootunivmulti-abl-$ver-$nowtime.txt logs/log-bootunivmulti-abl-$ver-latest.txt
    exec > logs/log-bootunivmulti-abl-$ver-$nowtime.txt 2>&1

    echo "Standard settings: $stdsettings"
    echo "Timeout: $to"
    echo "Freqhorn: $freqhorn"

    date

    for bench in $(ls ../bench_horn_multiple/array_*.smt2)
    do
        if echo "$bench" | grep -q altern
        then
          continue
        fi
        if ! echo "$bench" | grep -q -F -f cav19-benches-multiple.txt
        then
            continue
        fi
        benchname="${bench#../bench_horn/}"
        dobench "$ver" "$benchname"
    done
    date
done
