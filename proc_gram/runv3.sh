stdsettings="--v3 --maxrecdepth 1 --gen_method newtrav --trav_type striped --trav_priority sfs --to 1000 --no-save-lemmas --inv-templ 0"

freqhorn="../build/tools/deep/freqhorn"

to=300

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-v3-$nowtime.txt 2>&1
ln -sf log-v3-$nowtime.txt logs/log-v3-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"
echo "Freqhorn: $freqhorn"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchname="$1"
    [ "${benchname%.smt2}" = "$benchname" ] && benchname="${benchname}.smt2"
    benchpath="../bench_horn/array_altern_$benchname"
    echo "$benchname"
    cpp -P "grams_v3/$benchname" | \
        time -p timeout "$to" "$freqhorn" --grammar /dev/stdin $stdsettings "$benchpath"
}

for bench in $(ls ../bench_horn/array_altern_*.smt2)
do
    benchname="${bench#../bench_horn/array_altern_}"
    dobench "$benchname"
done
