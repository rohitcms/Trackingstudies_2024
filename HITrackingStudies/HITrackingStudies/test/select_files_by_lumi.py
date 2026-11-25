#!/usr/bin/env python3
"""Select the files of a DAS dataset that contain given (run, lumi section) pairs.

Used by run_compare_local.sh for data: the reference sample's files are chosen so that they
cover the run/lumi sections of the first sample, since two productions of the same data split
events into files differently.

Usage:
  select_files_by_lumi.py <run:lumi list file> <dataset> [instance]
Prints the LFNs (/store/...) of the matching files, one per line. Diagnostics go to stderr.
"""
import re
import subprocess
import sys
from collections import defaultdict


def parse_lumis(text):
    """Lumi numbers from a DAS lumi field: '[1, 2, 3]' or ranges '[[1, 5], [7, 9]]'."""
    if '[[' in text:
        lumis = set()
        for a, b in re.findall(r'\[\s*(\d+)\s*,\s*(\d+)\s*\]', text):
            lumis.update(range(int(a), int(b) + 1))
        return lumis
    return set(int(x) for x in re.findall(r'\d+', text))


def parse_das_lines(lines):
    """Map file LFN -> set of lumis from 'dasgoclient file,lumi' output lines."""
    out = defaultdict(set)
    for line in lines:
        line = line.strip()
        if not line.startswith('/store'):
            continue
        lfn, _, rest = line.partition(' ')
        out[lfn] |= parse_lumis(rest)
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    lumi_file, dataset = sys.argv[1], sys.argv[2]
    instance = sys.argv[3] if len(sys.argv) > 3 else 'prod/phys03'

    needed = defaultdict(set)
    with open(lumi_file) as f:
        for line in f:
            parts = line.strip().split(':')
            if len(parts) >= 2 and parts[0].isdigit() and parts[1].isdigit():
                needed[int(parts[0])].add(int(parts[1]))
    if not needed:
        sys.exit('no run:lumi pairs in ' + lumi_file)

    selected = set()
    for run in sorted(needed):
        query = 'file,lumi dataset=%s run=%d instance=%s' % (dataset, run, instance)
        res = subprocess.run(['dasgoclient', '-query=' + query], capture_output=True, text=True)
        if res.returncode != 0:
            sys.exit('dasgoclient failed for run %d: %s' % (run, res.stderr.strip()))
        files = parse_das_lines(res.stdout.splitlines())
        if not files:
            print('WARNING: DAS returned no file/lumi information for run %d' % run, file=sys.stderr)
        n_before = len(selected)
        for lfn, lumis in files.items():
            if lumis & needed[run]:
                selected.add(lfn)
        print('run %d: %d lumi sections needed, %d files selected' % (run, len(needed[run]), len(selected) - n_before),
              file=sys.stderr)
    for lfn in sorted(selected):
        print(lfn)


if __name__ == '__main__':
    main()
