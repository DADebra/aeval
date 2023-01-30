#!/bin/bash

# Change as desired
ncores="$(nproc)"

if [ $# -lt 2 ]
then
    echo "Usage: $(basename "$0") num_of_altern_to_run num_of_univ_to_run"
    exit 1
fi

numaltern="$1"
numuniv="$2"

cd "$(realpath "$(dirname "$0")")"
wd="$(realpath .)"

export LD_LIBRARY_PATH="$wd/libs:$LD_LIBRARY_PATH"
export PATH="$PATH:$wd"

ls "$wd/aeval-altern/bench_horn_rapid/" |
    sed -r 's/array_altern_//; s/_([0-9])\.smt2/& user-conjecture-\1/;' > "$wd/altern-benches.txt"
cat "$wd/aeval-altern/eval_bench/cav19-benches.txt" |
    sed -r 's/\.smt2//;' > "$wd/univ-benches.txt"

shuf -n "$numaltern" "$wd/altern-benches.txt" > "$wd/rand-altern-benches.txt"
shuf -n "$numuniv" "$wd/univ-benches.txt" > "$wd/rand-univ-benches.txt"

run_rapid() (
    cd "$wd/rapid/eval_bench"

    stdsettings="--input_syntax smtlib2 --cores $ncores --mode portfolio --schedule rapid"

    to=600

    trap 'kill -KILL -0' QUIT TERM INT

    nowtime="$(date +%m-%d-%Y_%H:%M)"

    mkdir -p logs
    exec > logs/log-bench-$nowtime.txt 2>&1
    ln -sf log-bench-$nowtime.txt logs/log-bench-latest.txt

    date

    echo "Standard settings: $stdsettings"
    echo "Timeout: $to"

    for bench in ./converted-bench/*
    do
        benchname="$(basename "$bench")"
        if ! grep -q -F "$benchname" "$wd/rand-altern-benches.txt"; then
            continue
        fi
        for conj in $bench/user-conjecture*
        do
        if ! grep -q -F "$benchname $conjname" "$wd/rand-altern-benches.txt"; then
            continue
        fi
            conjname="$(basename -s .smt2 "$conj")"
            echo $conj
            time -p ./vampire-rapid -t $to $stdsettings $conj
            echo Return code: $?
        done
    done
)

run_prophic() (
    cd "$wd/prophic3/eval_bench"

    stdsettings="-no-axiom-reduction -no-eq-uf"

    to=600

    trap 'kill -KILL -0' QUIT TERM INT

    nowtime="$(date +%m-%d-%Y_%H:%M)"

    mkdir -p logs
    exec > logs/log-univ-$nowtime.txt 2>&1
    ln -sf log-univ-$nowtime.txt logs/log-univ-latest.txt

    date

    echo "Standard settings: $stdsettings"
    echo "Timeout: $to"

    for bench in ./bench_univ_vmt/*
    do
        benchname="$(basename -s .vmt "$bench")"
        if ! grep -q -F "$benchname" "$wd/rand-univ-benches.txt"; then
            continue
        fi
        echo "$bench"
        time -p timeout "$to" ./prophic3 $stdsettings "$bench"
        echo "Return code: $?"
    done
)

run_freq_univ() (
    stdsettings="--v3 --altern-ver -1 --to 1500 --no-save-lemmas --inv-templ 0"

    freqhorn="../build/tools/deep/freqhorn"

    to=600

    cd "$wd/aeval-altern/eval_bench"

    trap 'kill -KILL -0' QUIT TERM INT

    # Perl is unhappy with my default locale settings
    export LANGUAGE=en_US LC_ALL=C LC_TIME=C LC_MONETARY=C LC_MEASUREMENT=C LANG=en_US

    nowtime="$(date +%m-%d-%Y_%H:%M)"

    mkdir -p logs
    exec > logs/log-frequniv-$nowtime.txt 2>&1
    ln -sf log-frequniv-$nowtime.txt logs/log-frequniv-latest.txt

    date

    echo "Standard settings: $stdsettings"
    echo "Timeout: $to"
    echo "Freqhorn: $freqhorn"

    # 1: Benchmark name (without prefix 'array_altern'
    dobench() {
        benchpath="$1"
        benchname="${benchpath#../bench_horn/}"
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
        benchname="$(basename -s .smt2 "$bench")"
        if ! grep -q -F "$benchname" "$wd/rand-univ-benches.txt"; then
            continue
        fi
        dobench "$bench"
    done
)

run_freq() (
    stdsettings="--v3 --altern-ver -1 --to 1500 --no-save-lemmas --inv-templ 0"

    freqhorn="../build/tools/deep/freqhorn"

    to=600

    cd "$wd/aeval-altern/eval_bench"

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
        benchname="$(basename -s .smt2 "$bench")"
        benchname="${benchname#array_altern_}"
        if ! grep -q -F "$benchname" "$wd/rand-altern-benches.txt"; then
            continue
        fi
        dobench "$bench"
    done
)

run_z3_univ() (
    stdsettings="fp.spacer.q3.use_qgen=true fp.spacer.ground_pobs=false fp.spacer.mbqi=false fp.spacer.use_euf_gen=true"

    to=600

    cd "$wd/aeval-altern/eval_bench"

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
        benchname="$(basename -s .smt2 "$bench")"
        if ! grep -q -F "$benchname" "$wd/rand-univ-benches.txt"; then
            continue
        fi
        dobench "$bench"
    done
)

run_qfix() (
    stdsettings="--v3 --altern-ver 9 --to 1500 --no-save-lemmas"

    freqhorn="../build/tools/deep/freqhorn"

    to=300

    cd "$wd/aeval-altern/eval_bench"

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
        echo "$benchname"
        time -p timeout "$to" "$freqhorn" $stdsettings "$benchpath"
    }

    for bench in ../bench_horn_rapid/*.smt2
    do
        benchname="$(basename -s .smt2 "$bench")"
        benchname="${benchname#array_altern_}"
        if ! grep -q -F "$benchname" "$wd/rand-altern-benches.txt"; then
            continue
        fi
        dobench "$bench"
    done
)

run_qfix_univ() (
    stdsettings="--v3 --debug --altern-ver 9 --to 1500 --no-save-lemmas --inv-templ 0"

    freqhorn="../build/tools/deep/freqhorn"

    to=300

    cd "$wd/aeval-altern/eval_bench"

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
        benchname2="$(basename -s .smt2 "$bench")"
        if ! grep -q -F "$benchname2" "$wd/rand-univ-benches.txt"; then
            continue
        fi
        dobench "$benchname"
    done
)

run_qfix_ablative() (
    stdsettings="--v3 --to 1500 --no-save-lemmas"

    freqhorn="../build/tools/deep/freqhorn"

    to=300

    cd "$wd/aeval-altern/eval_bench"

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
        ln -sf log-boot-abl-$ver-$nowtime.txt logs/log-boot-abl-$ver-latest.txt
        exec > logs/log-boot-abl-$ver-$nowtime.txt 2>&1

        echo "Standard settings: $stdsettings"
        echo "Timeout: $to"
        echo "Freqhorn: $freqhorn"

        date

        for bench in $(ls ../bench_horn_rapid/array_altern_*.smt2)
        do
            benchname="${bench#../bench_horn_rapid/array_altern_}"
            benchname2="$(basename -s .smt2 "$benchname")"
            if ! grep -q -F "$benchname2" "$wd/rand-altern-benches.txt"; then
                continue
            fi
            dobench "$ver" "$benchname"
        done
        date
    done
)

run_qfix_univ_ablative() (
    stdsettings="--v3 --to 1500 --no-save-lemmas --inv-templ 0"

    freqhorn="../build/tools/deep/freqhorn"

    to=300

    cd "$wd/aeval-altern/eval_bench"

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
        benchpath="../bench_horn/$benchname"
        echo "$benchname"
        time -p timeout "$to" "$freqhorn" --altern-ver "$alternver" $stdsettings "$benchpath"
    }

    for ver in $(cat ./altern-vers.txt)
    do
        ln -sf log-bootuniv-abl-$ver-$nowtime.txt logs/log-bootuniv-abl-$ver-latest.txt
        exec > logs/log-bootuniv-abl-$ver-$nowtime.txt 2>&1

        echo "Standard settings: $stdsettings"
        echo "Timeout: $to"
        echo "Freqhorn: $freqhorn"

        date

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
            benchname2="$(basename -s .smt2 "$bench")"
            if ! grep -q -F "$benchname2" "$wd/rand-univ-benches.txt"; then
                continue
            fi
            benchname="${bench#../bench_horn/}"
            dobench "$ver" "$benchname"
        done
        date
    done
)

rm -f outfifo
mkfifo outfifo
cat outfifo | tee -a artifact-log.txt &
exec >outfifo 2>&1

echo
echo -n "Starting on "
date

echo -n "Running RAPID (altern) (all cores) on "; date
run_rapid

i=0
maybewait() {
    i=$(($i + 1))
    if [ $i -eq $ncores ]; then
        wait -n
        i=$(($i - 1))
    fi
}
echo -n "Running Prophic (univ) (single core) on "; date
run_prophic &
maybewait
echo -n "Running FreqHorn (univ) (single core) on "; date
run_freq_univ &
maybewait
echo -n "Running Spacer (univ) (single core) on "; date
run_z3_univ &
maybewait

echo -n "Running FreqHorn (altern) (single core) on "; date
run_freq &
maybewait

echo -n "Running QFix (altern) (single core) on "; date
run_qfix &
maybewait
echo -n "Running QFix (altern ablative) (single core) on "; date
run_qfix_ablative &
maybewait
echo -n "Running QFix (univ) (single core) on "; date
run_qfix_univ &
maybewait
echo -n "Running QFix (univ ablative) (single core) on "; date
run_qfix_univ_ablative &

for i in $(seq 1 $ncores); do
    wait -n
done
i=0

echo -n "Processing RAPID (altern) logs on "; date
bash "$wd/rapid/eval_bench/process_logs.sh" "$wd/rapid/eval_bench/logs/log-bench-latest.txt" > rapid_altern_results.csv
echo -n "Processing Prophic (univ) logs on "; date
"$wd/prophic3/eval_bench/process_logs.py" "$wd/prophic3/eval_bench/logs/log-univ-latest.txt" > prophic_univ_results.csv
echo -n "Processing FreqHorn (univ) logs on "; date
"$wd/aeval-altern/eval_bench/process_logs.py" "$wd/aeval-altern/eval_bench/logs/log-frequniv-latest.txt" > freqhorn_univ_results.csv
echo -n "Processing FreqHorn (altern) logs on "; date
"$wd/aeval-altern/eval_bench/process_logs.py" "$wd/aeval-altern/eval_bench/logs/log-freq-latest.txt" > freqhorn_altern_results.csv
echo -n "Processing Spacer (univ) logs on "; date
"$wd/aeval-altern/eval_bench/process_logs_z3.py" "$wd/aeval-altern/eval_bench/logs/log-z3univ-latest.txt" > spacer_univ_results.csv

echo -n "Processing QFix (univ) logs on "; date
"$wd/aeval-altern/eval_bench/process_logs.py" "$wd/aeval-altern/eval_bench/logs/log-bootuniv-latest.txt" > qfix_univ_results.csv
echo -n "Processing QFix (altern) logs on "; date
"$wd/aeval-altern/eval_bench/process_logs.py" "$wd/aeval-altern/eval_bench/logs/log-boot-latest.txt" > qfix_altern_results.csv

echo -n "Processing QFix (univ ablative) logs on "; date
bash "$wd/aeval-altern/eval_bench/process_ablative_univ.sh" > qfix_univ_ablative_results.csv
echo -n "Processing QFix (altern ablative) logs on "; date
bash "$wd/aeval-altern/eval_bench/process_ablative.sh" > qfix_altern_ablative_results.csv

echo -n "Producing combined (altern) CSV on "; date
( cd "$wd/aeval-altern/eval_bench" && "$wd/aeval-altern/eval_bench/combine.py" "$wd/freqhorn_altern_results.csv" "$wd/rapid_altern_results.csv" "$wd/qfix_altern_results.csv" "$wd/aeval-altern/eval_bench/forms.csv" "$wd/aeval-altern/eval_bench/minvers.csv" ) > combined_altern_results.csv

echo -n "Producing combined (univ) CSV on "; date
bash "$wd/aeval-altern/eval_bench/combine_univ.sh" qfix_univ_results.csv spacer_univ_results.csv freqhorn_univ_results.csv prophic_univ_results.csv combined_univ_results.csv
echo -n "Number of FreqHorn benchmarks QFix solved: "
cut -d, -f3 qfix_univ_results.csv | grep -c 0
echo -n "Number of FreqHorn benchmarks Spacer solved: "
cut -d, -f3 spacer_univ_results.csv | grep -c 0
echo -n "Number of FreqHorn benchmarks FreqHorn solved: "
cut -d, -f3 freqhorn_univ_results.csv | grep -c 0
echo -n "Number of FreqHorn benchmarks Prophic solved: "
cut -d, -f3 prophic_univ_results.csv | grep -c 0

echo -n "Done on "
date
echo

rm -f outfifo
