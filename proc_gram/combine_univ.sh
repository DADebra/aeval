#!/bin/sh

if [ $# -lt 5 ]
then
    echo "Usage: $(basename "$0") ours.csv spacer.csv freq.csv prophic.csv out.csv"
    exit 1
fi

us="$1"
z3="$2"
freq="$3"
proph="$4"
out="$5"

printf 'Benchmark,QFix,Spacer,FreqHorn,Prophic\n' > "$out"
paste -d, "$us" "$z3" "$freq" "$proph" | cut -d, -f1,2,3,5,6,8,9,11,12 > "$$.csv"

echo -n "Number QFix solved uniquely: "
awk -F, '{ if ($3 == 0 && $5 == 1 && $7 == 1 && $9 == 1) print $1; }' "$$.csv" | wc -l

echo -n "Number QFix solve quickest: "
awk -F, '
{
    if ($3 == 0) {
        if (($5==1 || $2<$4) && ($7==1 || $2<$6) && ($9==1 || $2<$8))
            print $1;
    }
}' "$$.csv" | wc -l

echo -n "Number QFix in less than a second: "
awk -F, '
{
    if ($3 == 0) {
        if (($5==1 || $2<1) && ($7==1 || $2<1) && ($9==1 || $2<1))
            print $1;
    }
}' "$$.csv" | wc -l

echo -n "Number QFix solves within a second: "
awk -F, '
function abs(v) { return v < 0 ? -v : v; }
{
    if ($3 == 0) {
        if (($5==1 || abs($2-$4) < 1) && ($7==1 || abs($2-$6) < 1) && ($9==1 || abs($2-$8) < 1))
            print $1;
    }
}' "$$.csv" | wc -l

awk -F, 'BEGIN {OFS=",";}
{
    if ($3 == 1) out1="\\cross";
    else out1=$2;
    if ($5 == 1) out2="\\cross";
    else out2=$4;
    if ($7 == 1) out3="\\cross";
    else out3=$6;
    if ($9 == 1) out4="\\cross";
    else out4=$8;
    print $1, out1, out2, out3, out4;
}' "$$.csv" >> "$out"

rm "$$.csv"
