#!/bin/sh

cd "$(realpath "$(dirname "$0")")"

echo "Version,Solved"
for vers in $(cat altern-vers.txt)
do
    total="$(grep -c Success "./logs/log-boottempl-abl-$vers-latest.txt")"
    echo "$vers,$total"
done
