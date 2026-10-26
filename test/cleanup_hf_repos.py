#!/usr/bin/env python3
"""Delete HF Hub repos created by HF transport integration tests.

Usage:
    set -a && . /home/ubuntu/racegen/.env && set +a
    python3 hf-stego-transport/test/cleanup_hf_repos.py [--prefix transport-]
"""

import argparse
import os
import sys

try:
    from huggingface_hub import HfApi
except ImportError:
    print("huggingface_hub is required: pip install huggingface_hub", file=sys.stderr)
    sys.exit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", default="transport-", help="Repo name prefix to delete")
    parser.add_argument("--dry-run", action="store_true", help="List repos without deleting")
    args = parser.parse_args()

    token = os.environ.get("HF_TOKEN")
    username = os.environ.get("HF_USERNAME")
    if not token or not username:
        print("HF_TOKEN and HF_USERNAME must be set (source .env)", file=sys.stderr)
        return 1

    api = HfApi(token=token)
    deleted = 0
    for repo in api.list_models(author=username):
        name = repo.id.split("/")[-1]
        if name.startswith(args.prefix):
            if args.dry_run:
                print(f"would delete: {repo.id}")
            else:
                try:
                    api.delete_repo(repo.id, repo_type="model")
                    print(f"deleted: {repo.id}")
                    deleted += 1
                except Exception as exc:  # pylint: disable=broad-except
                    print(f"error deleting {repo.id}: {exc}", file=sys.stderr)
    if not args.dry_run:
        print(f"deleted {deleted} repos")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
