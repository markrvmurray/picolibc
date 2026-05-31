# Bug #336: split a libm test-vector header into N round-robin slices.
#
# Each vector entry is exactly one line ending in "}," (verified across all
# test-*.h: pow/exp/sin/j0/... formats all share this delimiter, one entry
# per line, no multi-line entries).  Keep entry i only when i % n == part,
# so the N slices together cover every entry exactly once with no per-entry
# markers needed.  Non-entry lines (the trailing licence comment) pass
# through to every slice — they emit no data.
#
# Usage: awk -v part=P -v n=N -f split-vectors.awk <header>   (stdout = slice)

/\},[ \t]*$/ { if (ei % n == part) print; ei++; next }
            { print }
