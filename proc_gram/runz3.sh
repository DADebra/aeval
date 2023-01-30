stdsettings="fp.spacer.q3.use_qgen=true fp.spacer.ground_pobs=false fp.spacer.mbqi=false fp.spacer.use_euf_gen=true"

to=600

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-z3-$nowtime.txt 2>&1
ln -sf log-z3-$nowtime.txt logs/log-z3-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchpath="$1"
    benchname="${benchpath#../bench_horn_rapid/}"
    echo "$benchname"
    time -p timeout "$to" z3 $stdsettings "$benchpath"
}

for bench in ../bench_horn_rapid/*.smt2
do
    dobench "$bench"
done
