-- Canonical schema for the MC6809 bench ledger.
--
-- SINGLE SOURCE OF TRUTH, read by BOTH consumers:
--   scripts/create-bench-db.sh         (shell)
--   scripts/run-mc6809-tests ensure_db (python)
--
-- Only idempotent `CREATE ... IF NOT EXISTS` statements live here, so this
-- file is safe to apply to an existing, populated ledger — it never drops,
-- retypes, or rewrites anything.
--
-- WAL mode and the additive `ALTER TABLE ... ADD COLUMN` migrations for
-- legacy ledgers are NOT in this file: both consumers run those migrations
-- *before* applying this schema, so that opt_level exists by the time
-- idx_results_opt references it on a pre-existing ledger.

CREATE TABLE IF NOT EXISTS runs (
    run_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp         TEXT NOT NULL,
    llvm_commit       TEXT,
    picolibc_commit   TEXT,
    usim_commit       TEXT,
    host              TEXT,
    opt_level         TEXT,
    llvm_binary_mtime INTEGER
);

CREATE TABLE IF NOT EXISTS results (
    run_id       INTEGER NOT NULL REFERENCES runs(run_id),
    test_name    TEXT NOT NULL,
    suite        TEXT,
    status       TEXT NOT NULL,
    wall_seconds REAL,
    cycles       INTEGER,
    rc           INTEGER,
    opt_level    TEXT,
    text_bytes   INTEGER,
    PRIMARY KEY (run_id, test_name)
);

CREATE INDEX IF NOT EXISTS idx_results_test ON results(test_name, run_id);
CREATE INDEX IF NOT EXISTS idx_results_opt  ON results(opt_level, test_name);
