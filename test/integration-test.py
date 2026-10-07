#!/usr/bin/env python3
"""
Integration test script for HF Stego Transport.
Run from plugin test directory.
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path

script_dir = Path(__file__).parent.resolve()
plugin_dir = script_dir.parent
raceboat_dir = plugin_dir.parent / "code" / "raceboat"

def run_setup():
    """Run the setup script to prepare plugin artifacts."""
    setup_script = script_dir / "setup.py"
    if setup_script.exists():
        print("Running setup.py...")
        subprocess.run([sys.executable, str(setup_script)], check=True, cwd=script_dir)
    else:
        print("No setup.py found, skipping...")

def clear_logs():
    """Clear log directories."""
    for log_dir_name in ["server-logs", "client-logs", "client2-logs"]:
        log_dir = script_dir / log_dir_name
        if log_dir.exists():
            print(f"Clearing {log_dir_name}...")
            for item in log_dir.iterdir():
                try:
                    if item.is_file():
                        item.unlink()
                    elif item.is_dir():
                        shutil.rmtree(item)
                except Exception as e:
                    print(f"Warning: Could not delete {item}: {e}")

def run_integration_test(wait_time=30, additional_args=None):
    """Run the generic raceboat integration test."""
    additional_args = additional_args or []
    
    # Use the raceboat integration test runner
    test_runner = raceboat_dir / "test" / "integration" / "run-integration-test.py"
    compose_file = script_dir / "docker-compose.yml"
    
    if not test_runner.exists():
        print(f"Error: Test runner not found at {test_runner}")
        return 1
    
    if not compose_file.exists():
        print(f"Error: docker-compose.yml not found at {compose_file}")
        return 1
    
    cmd = [
        sys.executable, str(test_runner),
        "--compose-file", str(compose_file),
        "--wait-time", str(wait_time),
        "--name", "HF Stego Transport Integration Test",
        "--clear-logs"
    ] + additional_args
    
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=script_dir)
    return result.returncode

def main():
    import argparse
    parser = argparse.ArgumentParser(description="HF Stego Transport Integration Test")
    parser.add_argument("--wait-time", type=int, default=30, help="Wait time for channel establishment")
    parser.add_argument("--skip-setup", action="store_true", help="Skip setup.py")
    parser.add_argument("--skip-logs", action="store_true", help="Skip log clearing")
    parser.add_argument("--additional-args", nargs=argparse.REMAINDER, help="Additional args for run-integration-test.py")
    args = parser.parse_args()
    
    if not args.skip_setup:
        run_setup()
    
    if not args.skip_logs:
        clear_logs()
    
    exit_code = run_integration_test(args.wait_time, args.additional_args)
    sys.exit(exit_code)

if __name__ == "__main__":
    main()