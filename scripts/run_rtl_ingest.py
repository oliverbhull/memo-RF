#!/usr/bin/env python3
"""
Entrypoint for RTL-SDR 7-channel ingest. Run from repo root:
  python scripts/run_rtl_ingest.py [--config config/rtl_ingest.json] [--device 0]
"""
import sys
from pathlib import Path

# Ensure repo root is on path
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

# Run as package from scripts/ so that rtl_ingest is importable
SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from rtl_ingest.main import main

if __name__ == "__main__":
    main()
