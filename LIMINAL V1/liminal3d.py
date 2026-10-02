#!/usr/bin/env python3
"""Startskript:  python liminal3d.py [--seed N] [--mode ascii|hires|mono] [--debug]"""

import sys

if sys.version_info < (3, 8):
    sys.exit("LIMINAL benoetigt Python 3.8 oder neuer.")

from liminal.game import main

if __name__ == "__main__":
    main()
