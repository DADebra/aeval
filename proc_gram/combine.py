#!/usr/bin/env python3

import sys
from os.path import *
import csv

if len(sys.argv) < 6:
    print(f"Usage: {basename(sys.argv[0])} freq_results.csv rapid_results.csv our_results.csv forms.csv minvers.csv")
    exit(1)

rapidsucceeded = set()
for line in open(f"{dirname(sys.argv[0])}/rapidsucceeded.txt", "r"):
    line = line.rstrip('\n')
    rapidsucceeded.add(line)

freqcsv = csv.reader(open(sys.argv[1], "r"))
for l in freqcsv: break
rapidcsv = csv.reader(open(sys.argv[2], "r"))
for l in rapidcsv: break
ourcsv = csv.reader(open(sys.argv[3], "r"))
for l in ourcsv: break
formscsv = csv.reader(open(sys.argv[4], "r"))
for l in formscsv: break
verscsv = csv.reader(open(sys.argv[5], "r"))
for l in verscsv: break

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

    def __str__(self):
        return "{{ freq:{},{} rapid:{},{} our:{},{} }}".format(
            self.freqtime, self.freqfailed, self.rapidtime, self.rapidfailed,
            self.ourtime, self.ourfailed)

    def __repr__(self):
        return str(self)

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

#print("Benchmark,FreqHornTimeSecs,RAPIDTimeSecs,QFixTimeSecs,Form,MinVers")
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
        elif v.rapidtime != "?":
            rapidres = "\\cross"
        else:
            rapidres = v.rapidtime
    else:
        rapidres = v.rapidtime

    if v.ourfailed == "1":
        ourres = "\\cross"
    else:
        ourres = v.ourtime

    #if not freqres or not rapidres or not ourres:
    #    continue
    if not ourres:
        continue

    print(f"{k},{freqres},{rapidres},{ourres},{v.form}")