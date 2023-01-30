#!/bin/bash

ncores="$(nproc)"

cd "$(realpath "$(dirname "$0")")"
wd="$(realpath .)"

export LD_LIBRARY_PATH="$wd/libs:$LD_LIBRARY_PATH"

rm -f outfifo
mkfifo outfifo
cat outfifo | tee -a artifact-log.txt &
exec >outfifo 2>&1

echo
echo -n "Starting on "
date

echo -n "Running RAPID (altern) (all cores) on "; date
bash "$wd/rapid/eval_bench/run_benches_bench.sh"

i=0
maybewait() {
    i=$(($i + 1))
    if [ $i -eq $ncores ]; then
        wait -n
        i=$(($i - 1))
    fi
}
echo -n "Running Prophic (univ) (single core) on "; date
bash "$wd/prophic3/eval_bench/run_benches_univ.sh" &
maybewait
echo -n "Running FreqHorn (univ) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runfreq_univ.sh" &
maybewait
echo -n "Running Spacer (univ) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runz3_univ.sh" &
maybewait

echo -n "Running FreqHorn (altern) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runfreq.sh" &
maybewait

echo -n "Running QFix (altern) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runboot.sh" &
maybewait
echo -n "Running QFix (altern ablative) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runboot_ablative.sh" &
maybewait
echo -n "Running QFix (univ) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runboot_univ.sh" &
maybewait
echo -n "Running QFix (univ ablative) (single core) on "; date
bash "$wd/aeval-altern/eval_bench/runboot_univ_ablative.sh" &

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
