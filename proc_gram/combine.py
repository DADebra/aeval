#!/usr/bin/env python3

import sys
from os.path import *
import csv

if len(sys.argv) < 6:
    print(f"Usage: {basename(sys.argv[0])} freq_results.csv rapid_results.csv our_results.csv forms.csv minvers.csv")
    exit(1)

rapidsucceeded = set()
for line in open("rapidsucceeded.txt", "r"):
    line = line.rstrip('\n')
    rapidsucceeded.add(line)

freqcsv = csv.reader(open(sys.argv[1], "r"))
rapidcsv = csv.reader(open(sys.argv[2], "r"))
ourcsv = csv.reader(open(sys.argv[3], "r"))
formscsv = csv.reader(open(sys.argv[4], "r"))
verscsv = csv.reader(open(sys.argv[5], "r"))

combined = dict()

class Result:
    def __init__(self):
        self.freqtime = None
        self.freqfailed = None
        self.rapidtime = None
        self.rapidfailed = None
        self.ourtime = None
        self.ourfailed = None
        self.form = None
        self.minvers = None

for line in freqcsv:
    if line[0] not in combined:
        combined[line[0]] = Result()
    combined[line[0]].freqtime = line[1]
    combined[line[0]].freqfailed = line[2]

for line in rapidcsv:
    if line[0] not in combined:
        combined[line[0]] = Result()
    combined[line[0]].rapidtime = line[1]
    combined[line[0]].rapidfailed = line[2]

for line in ourcsv:
    if line[0] not in combined:
        combined[line[0]] = Result()
    combined[line[0]].ourtime = line[1]
    combined[line[0]].ourfailed = line[2]

for line in formscsv:
    if line[0] not in combined:
        combined[line[0]] = Result()
    combined[line[0]].form = line[1]

for line in verscsv:
    if line[0] not in combined:
        combined[line[0]] = Result()
    combined[line[0]].minvers = line[1]

print("Benchmark,FreqHornTimeSecs,RAPIDTimeSecs,QFixTimeSecs,Form,MinVers")
for k, v in combined.items():
    freqres = None
    rapidres = None
    ourres = None
    if v.freqfailed == "1":
        freqres = "\\cross"
    else:
        freqres = v.freqtime

    if v.rapidfailed == "1":
        if k in rapidsucceeded:
            rapidres = "\\checkmark"
        else:
            rapidres = "\\cross"
    else:
        rapidres = v.rapidtime

    if v.ourfailed == "1":
        ourres = "\\cross"
    else:
        ourres = v.ourtime

    if not freqres or not rapidres or not ourres:
        continue

    print(f"{k},{freqres},{rapidres},{ourres},{v.form},{v.minvers}")
