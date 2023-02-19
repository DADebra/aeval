#!/usr/bin/env python3

import sys
from os.path import *
import csv

if len(sys.argv) < 2:
    print(f"Usage: {basename(sys.argv[0])} combined.csv")
    exit(1)

combinedcsv = csv.reader(open(sys.argv[1], "r"))

out = dict()

class Result:
    def __init__(self):
        self.avgourtime = 0
        self.oursolved = 0
        self.avgrapidtime = 0
        self.rapidsolved = 0
        self.avgfreqtime = 0
        self.freqsolved = 0
        self.total = 0

out["Total"] = Result()
total = out["Total"]
for line in combinedcsv:
    if line[4] not in out:
        out[line[4]] = Result()
    tmp = out[line[4]]
    if line[1] not in ("\\cross", "None"):
        tmp.avgfreqtime = ((tmp.avgfreqtime * tmp.freqsolved) + float(line[1])) / (tmp.freqsolved + 1)
        tmp.freqsolved += 1
        total.avgfreqtime = ((total.avgfreqtime * total.freqsolved) + float(line[1])) / (total.freqsolved + 1)
        total.freqsolved += 1
    if line[2] not in ("\\cross", "\\checkmark", "?"):
        tmp.avgrapidtime = ((tmp.avgrapidtime * tmp.rapidsolved) + float(line[2])) / (tmp.rapidsolved + 1)
        tmp.rapidsolved += 1
        total.avgrapidtime = ((total.avgrapidtime * total.rapidsolved) + float(line[2])) / (total.rapidsolved + 1)
        total.rapidsolved += 1
    if line[3] not in ("\\cross", "None"):
        tmp.avgourtime = ((tmp.avgourtime * tmp.oursolved) + float(line[3])) / (tmp.oursolved + 1)
        tmp.oursolved += 1
        total.avgourtime = ((total.avgourtime * total.oursolved) + float(line[3])) / (total.oursolved + 1)
        total.oursolved += 1
    tmp.total += 1
    total.total += 1

#print("Benchmark,FreqHornTimeSecs,RAPIDTimeSecs,QFixTimeSecs,Form,MinVers")
def printout(k, v):
    #if not freqres or not rapidres or not ourres:
    #    continue

    outstr = f"{k}"
    for itm in [(v.oursolved, v.avgourtime), (v.rapidsolved, v.avgrapidtime), (v.freqsolved, v.avgfreqtime)]:
        outstr += f",{itm[0]} / {v.total},"
        if itm[1] == 0:
            outstr += "---"
        else:
            outstr += str(round(itm[1], 2)) + " s"

    print(outstr)

for k, v in out.items():
    if k == "Total":
        continue
    printout(k, v)

printout("Total", out["Total"])
