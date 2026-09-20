#!/usr/bin/env python3
"""Losslessly export the original result screen geometry and texture bank."""
from pathlib import Path
import argparse,json
import extract_original_hud as hud

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();hud.BANKS=[('result','result')]
    print(json.dumps(hud.export(a.hostfs,a.out),indent=2))
