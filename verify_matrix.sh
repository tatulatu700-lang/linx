#!/bin/bash
set -euo pipefail
cd /home/ron/linx

make clean
make all
taskset -c 0 ./linx_core_engine
echo "[+] SUBSTRATE INVARIANTS SOUND: EXIT $?"
