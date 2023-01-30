stdsettings="fp.spacer.q3.use_qgen=true fp.spacer.ground_pobs=false fp.spacer.mbqi=false fp.spacer.use_euf_gen=true"

to=600

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-z3univ-$nowtime.txt 2>&1
ln -sf log-z3univ-$nowtime.txt logs/log-z3univ-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchpath="$1"
    benchname="${benchpath#../bench_horn/}"
    echo "$benchname"
    time -p timeout "$to" z3 $stdsettings "$benchpath"
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
    dobench "$bench"
done
