#!/usr/bin/env bash
set -euo pipefail

FROM_COMMIT="6acf44b51df110321a02e3f70ae9f46a911468a4"
TO_COMMIT="8ff3fcbb00076b76920593ede1f456c1ed1166e5"
TARGET_BRANCH=""
DRY_RUN=0
TEST_CMD=""
STOP_ON_FAIL=1

usage() {
  cat <<USAGE
Usage: $0 [options]

Replays only changes under GameServer/ commit-by-commit.

Options:
  --from <commit>     Start commit (exclusive). Default: ${FROM_COMMIT}
  --to <commit>       End commit (inclusive). Default: ${TO_COMMIT}
  --branch <name>     Create/reset and checkout branch from --from before replay.
  --test-cmd <cmd>    Run command after each replayed commit to detect regressions.
  --keep-going        Continue replay even when --test-cmd fails.
  --dry-run           Print replay plan without changing working tree.
  -h, --help          Show this help.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from)
      FROM_COMMIT="$2"
      shift 2
      ;;
    --to)
      TO_COMMIT="$2"
      shift 2
      ;;
    --branch)
      TARGET_BRANCH="$2"
      shift 2
      ;;
    --test-cmd)
      TEST_CMD="$2"
      shift 2
      ;;
    --keep-going)
      STOP_ON_FAIL=0
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if ! git rev-parse --verify "$FROM_COMMIT" >/dev/null 2>&1; then
  echo "Invalid --from commit: $FROM_COMMIT" >&2
  exit 1
fi

if ! git rev-parse --verify "$TO_COMMIT" >/dev/null 2>&1; then
  echo "Invalid --to commit: $TO_COMMIT" >&2
  exit 1
fi

if [[ -n "$TARGET_BRANCH" ]]; then
  if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "[dry-run] Would checkout branch '$TARGET_BRANCH' from $FROM_COMMIT"
  else
    git checkout -B "$TARGET_BRANCH" "$FROM_COMMIT"
  fi
fi

mapfile -t commits < <(git rev-list --reverse --ancestry-path "${FROM_COMMIT}..${TO_COMMIT}" -- GameServer)

if [[ ${#commits[@]} -eq 0 ]]; then
  echo "No commits touching GameServer/ between $FROM_COMMIT..$TO_COMMIT"
  exit 0
fi

echo "Found ${#commits[@]} GameServer commit(s) in range ${FROM_COMMIT}..${TO_COMMIT}."

for commit in "${commits[@]}"; do
  subject=$(git show -s --format=%s "$commit")
  parent=$(git rev-parse "${commit}^")

  echo "---"
  echo "Replaying $commit"
  echo "Subject: $subject"

  if [[ "$DRY_RUN" -eq 1 ]]; then
    continue
  fi

  if ! git diff --binary "$parent" "$commit" -- GameServer | git apply -3 --index; then
    echo "Failed to apply GameServer patch from $commit. Resolve conflicts and continue manually." >&2
    exit 2
  fi

  if git diff --cached --quiet; then
    echo "No effective GameServer changes for $commit after apply (skipped commit)."
    continue
  fi

  git commit -m "[gameserver replay] $subject" -m "Source commit: $commit"

  if [[ -n "$TEST_CMD" ]]; then
    echo "Running test command on $(git rev-parse --short HEAD): $TEST_CMD"
    if ! bash -lc "$TEST_CMD"; then
      echo "Test command failed right after replaying source commit $commit." >&2
      echo "Current HEAD: $(git rev-parse HEAD)" >&2

      if [[ "$STOP_ON_FAIL" -eq 1 ]]; then
        echo "Stopping because --keep-going was not set." >&2
        exit 3
      fi
    fi
  fi
done

echo "Replay complete."
