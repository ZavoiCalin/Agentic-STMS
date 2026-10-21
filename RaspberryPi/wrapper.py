import argparse
import subprocess
import sys
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent


def run_script(script_path: Path, *args):
    subprocess.run([sys.executable, str(script_path), *args], check=True)


def main():
    parser = argparse.ArgumentParser(description="Hardware-first traffic orchestration wrapper")
    parser.add_argument("--route", action="store_true", help="Run the AI routing logic")
    parser.add_argument("--traffic", action="store_true", help="Run the traffic-light decision script")
    parser.add_argument("--sync-cloud", action="store_true", help="Sync route and sensor data to Azure IoT Hub and Azure Blob Storage; MongoDB is legacy compatibility only")
    parser.add_argument("--legacy-mongo", action="store_true", help="Optional legacy MongoDB upload for older web-app compatibility")
    parser.add_argument("--all", action="store_true", help="Run route, traffic, and cloud sync")
    args = parser.parse_args()

    try:
        if args.route or args.all:
            run_script(BASE_DIR / "AI_routing_algorithm" / "ai_routing.py")
        if args.traffic or args.all:
            run_script(BASE_DIR / "Traffic_light_commands" / "main.py")
        if args.sync_cloud or args.all:
            edge_args = ["--sync-cloud"]
            if args.legacy_mongo:
                edge_args.append("--legacy-mongo")
            run_script(BASE_DIR / "edge_pipeline.py", *edge_args)
    except KeyboardInterrupt:
        print("Execution stopped by user.")
        sys.exit(0)


if __name__ == "__main__":
    main()
