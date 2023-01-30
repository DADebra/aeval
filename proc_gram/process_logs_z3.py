#!/usr/bin/env python3

import sys
from os.path import *

if len(sys.argv) < 2:
    print(f"Usage: {basename(sys.argv[0])} log-file.txt")
    exit(1)

ifile = open(sys.argv[1], "r")

name = None
time = None
res = 1

print("Benchmark,TimeSecs,Failed")
for line in ifile:
    line = line.rstrip('\n')
    if line.find("smt2") >= 0:
        if name:
            print(f"{name},{time},{res}")
        name = line.replace("array_altern_", "").replace(".smt2", "")
        time = None
        res = 1
    elif line.find("unsat") >= 0:
        res = 0
    elif line.find("error") >= 0:
        res = 2
    elif line.find("real") == 0:
        time = line.split(' ')[1]

print(f"{name},{time},{res}")
