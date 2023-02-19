stdsettings="--v3 --data --altern-ver 9 --inv-templ 0 --to 1500"

[ -z "$freqhorn" ] && freqhorn="../build/tools/deep/freqhorn"

to=100

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-boottempl-$nowtime.txt 2>&1
ln -sf log-boottempl-$nowtime.txt logs/log-boottempl-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"
echo "Freqhorn: $freqhorn"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchpath="$1"
    benchname="${benchpath#../bench_horn_rapid/array_altern}"
    benchname="${benchname%.smt2}"
    echo "$benchname"
    savelemmas=""
    if [ "${benchname%_*}" = "count_occs" ]
    then
      savelemmas="--save-lemmas"
    fi
    cpp -P -I templgrams templgrams/$benchname.smt2 > tmptemplgram.smt2
    time -p timeout "$to" "$freqhorn" --grammar tmptemplgram.smt2 $stdsettings $savelemmas "$benchpath"
}

for bench in ../bench_horn_rapid/*.smt2
do
    dobench "$bench"
done
