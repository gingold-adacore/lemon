#!/usr/bin/env python

import sys

if len(sys.argv) < 2:
    sys.stderr.write('usage: %s file.bin' % sys.argv[0])
    sys.exit(1)

out=sys.stdout
with open(sys.argv[1]) as f:
    s = f.read()
    for i in range(len(s)):
        out.write(' 0x{:02x}'.format(ord(s[i])))
        if i != len(s) - 1:
            out.write(',')
        if (i % 8) == 7:
            out.write('\n')
