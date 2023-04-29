stdsettings="--v3 --data --altern-ver 9 --inv-templ 0 --nosimpl --grammar forallgram.smt2 --to 1500"

[ -z "$freqhorn" ] && freqhorn="../build/tools/deep/freqhorn"

to=300

cd "$(realpath "$(dirname "$0")")"

trap 'kill -KILL -0' QUIT TERM INT

# Perl is unhappy with my default locale settings
export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

nowtime="$(date +%m-%d-%Y_%H:%M)"

mkdir -p logs
exec > logs/log-boot-$nowtime.txt 2>&1
ln -sf log-boot-$nowtime.txt logs/log-boot-latest.txt

date

echo "Standard settings: $stdsettings"
echo "Timeout: $to"
echo "Freqhorn: $freqhorn"

# 1: Benchmark name (without prefix 'array_altern'
dobench() {
    benchpath="$1"
    benchname="${benchpath#../bench_horn_rapid/}"
    #benchname="${benchname%.smt2}"
    echo "$benchname"
    savelemmas=""
    if [ "${benchname%_*}" = "array_altern_count_occs" ]
    then
      savelemmas="--save-lemmas"
    fi
    time -p timeout "$to" "$freqhorn" $stdsettings $savelemmas "$benchpath"
}

for bench in ../bench_horn_rapid/*.smt2
do
    dobench "$bench"
done
