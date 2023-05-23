# README

## Running the Tool
With no extra options the tool defaults to stock FreqHorn,
i.e. none of our bootstrapped candidates or data-learned ones.

For the highest version of the algorithm, run the following:
```
./build/tools/deep/freqhorn --data --altern-ver 9 --grammar ./proc_gram/forallgram.smt2 <input_file>
```

This will enable the full range calculation, data-learned candidates,
and use a basic universally-quantified template.
This is how we evaluate on the benchmarks in `bench_horn`.

We also support the following other constants for `--altern-ver`:

 0. Just tries the property as-is, without lemmas from syntax or data.
    Doesn't need `--grammar` or `--data` option.
 1. Property as-is but with lemmas from syntax.
    Doesn't need `--grammar` or `--data` option.
 2. Lemmas from syntax and data but without the property, using a simpler
    form of the range algorithm which just uses the expression in `store` as-is.
    Needs both `--grammar` and `--data` options.
 3. Lemmas from data only w/o property with full form of range algorithm.
    Needs both `--grammar` and `--data` options.
 4. Lemmas from data and syntax w/o property with full form of range algorithm.
    Needs both `--grammar` and `--data` options.
 5. Lemmas from syntax, data, and property with full range algorithm.
    Needs both `--grammar` and `--data` options.

## Shell scripts
We provide the following shell scripts in `/proc_gram` to ease running
sets of experiments.
They all produce output in `/proc_gram/logs`, named accordingly.
These logs can all be processed by `/proc_gram/process_logs.py`, taking
the log file as the only argument and returning a CSV.

 - `runboot.sh` -- Runs the highest version of the approach on all benchmarks
   in /bench\_horn\_rapid (mostly containing alternating-quantifier properties)
   with `forallgram.smt2`, logs named `log-boot-<date>.txt`.
 - `runboot_ablative.sh` -- Like `runboot.sh` but runs for each version
   listed above, logs named `log-boot-abl-<vers>-<date>.txt`.
   Process logs of all versions at once with `/proc_gram/process_ablative.sh`.
 - `runboot_templ.sh` -- Runs the highest version of the approach on all benchmarks
   in /bench\_horn\_rapid (mostly containing alternating-quantifier properties)
   with a specialized template for each one (in `/proc\_gram/templgrams`),
   logs named `log-boottempl-<date>.txt`.
 - `runboot_templ_ablative.sh` -- Like `runboot_templ.sh` but runs for each version
   listed above, logs named `log-boottempl-abl-<vers>-<date>.txt`.
   Process logs of all versions at once with `/proc_gram/process_ablative_templ.sh`.
 - `runboot_univ.sh` -- Runs the highest version of the approach on the
   benchmarks in /bench\_horn\_rapid (containing single-quantifier properties)
   listed in `proc\_gram/cav19-benches.txt`, logs named `log-bootuniv-<date>.txt`.
 - `runboot_univ_ablative.sh` -- Like `runboot_univ.sh` but runs for each version
   listed above, logs named `log-boottempl-abl-<vers>-<date>.txt`.
   Process logs of all versions at once with `/proc_gram/process_ablative_univ.sh`.
