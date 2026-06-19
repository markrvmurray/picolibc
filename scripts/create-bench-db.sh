#!/bin/sh
# create-bench-db.sh — create / ensure the MC6809 bench SQLite ledger.
#
# Idempotent and NON-DESTRUCTIVE: it only ever issues
#   CREATE TABLE  IF NOT EXISTS ...   (from bench-db-schema.sql)
#   CREATE INDEX  IF NOT EXISTS ...   (from bench-db-schema.sql)
#   ALTER TABLE ... ADD COLUMN        (guarded; "duplicate column" ignored)
# so running it against an existing, populated ledger leaves every row and
# every existing column untouched — it only fills in anything missing.
#
# The table/index definitions live in scripts/bench-db-schema.sql, the single
# source of truth shared with the Python recorder (run-mc6809-tests ensure_db).
#
# Usage:
#   create-bench-db.sh [DB_PATH]
# DB_PATH defaults to $MC6809_BENCH_DB, else
#   ~/Documents/mc6809-bench/results.sqlite

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SCHEMA="$SCRIPT_DIR/bench-db-schema.sql"
DB=${1:-${MC6809_BENCH_DB:-$HOME/Documents/mc6809-bench/results.sqlite}}

[ -f "$SCHEMA" ] || { echo "create-bench-db: missing schema file $SCHEMA" >&2; exit 1; }

mkdir -p "$(dirname "$DB")"

# WAL: needed for concurrent tallies; persistent, and a no-op if already set.
sqlite3 "$DB" "PRAGMA journal_mode=WAL;" >/dev/null

# Additive migrations FIRST, so a legacy ledger gains any columns the schema's
# indexes reference (idx_results_opt -> opt_level) before the schema is applied.
# Each ALTER is independent; "duplicate column name" (already present) and "no
# such table" (fresh DB — table made by the schema step below) are expected and
# ignored. No column is ever dropped or retyped.
for spec in "runs    opt_level         TEXT" \
            "runs    llvm_binary_mtime INTEGER" \
            "results opt_level         TEXT" \
            "results text_bytes        INTEGER"; do
  # shellcheck disable=SC2086
  set -- $spec
  sqlite3 "$DB" "ALTER TABLE $1 ADD COLUMN $2 $3;" 2>/dev/null || true
done

# Canonical schema (tables + indexes, all IF NOT EXISTS).
sqlite3 "$DB" < "$SCHEMA"

echo "bench DB ready: $DB"
