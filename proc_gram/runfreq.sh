stdsettings="--v3 --altern-ver -1 --to 1500 --no-save-lemmas --inv-templ 0"

freqhorn="./freqhorn-stock"

to=30

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-freq-$nowtime.txt 2>&1
ln -sf log-freq-$nowtime.txt logs/log-freq-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"
echo "Freqhorn: $freqhorn"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchpath="$1"
    benchname="${benchpath#../bench_horn_rapid/}"
    echo "$benchname"
    time -p timeout "$to" "$freqhorn" $stdsettings "$benchpath"
}

for bench in ../bench_horn_rapid/*.smt2
do
    dobench "$bench"
done
