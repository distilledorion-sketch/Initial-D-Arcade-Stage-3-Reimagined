#!/usr/bin/env python3
"""Export the nine original course-conquered banks, preserving authored layout."""
import argparse
from pathlib import Path
from extract_original_loading import export_bank

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='HOSTFS/model/conquer')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    for course in range(9):
        name = f'conquer{course:02d}'
        export_bank(args.source, name, args.out / name, "twiddled")

if __name__ == '__main__':
    main()
