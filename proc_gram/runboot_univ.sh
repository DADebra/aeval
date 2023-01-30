stdsettings="--v3 --altern-ver 9 --data --to 1500 --no-save-lemmas --inv-templ 0"

[ -z "$freqhorn" ] && freqhorn="../build/tools/deep/freqhorn"

to=30

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-bootuniv-$nowtime.txt 2>&1
ln -sf log-bootuniv-$nowtime.txt logs/log-bootuniv-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"
echo "Freqhorn: $freqhorn"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchname="$1"
    [ "${benchname%.smt2}" = "$benchname" ] && benchname="${benchname}.smt2"
    benchpath="../bench_horn/$benchname"
    echo "$benchname"
    time -p timeout "$to" "$freqhorn" $stdsettings "$benchpath"
}

for bench in $(ls ../bench_horn/array_*.smt2)
do
    if echo "$bench" | grep -q altern
    then
      continue
    fi
    if ! echo "$bench" | grep -q -F -f cav19-benches.txt
    then
        continue
    fi
    benchname="${bench#../bench_horn/}"
    dobench "$benchname"
done
