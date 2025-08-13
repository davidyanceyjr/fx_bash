#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Build
make -s

# Use a fresh bash to avoid residue; require dev headers runtime not needed
BASH_BIN="${BASH:-bash}"

# Load builtins
$BASH_BIN -c "
enable -f ./build/fp_prelude.so fx fp_cut fp_tr fp_grep fp_take fp_find

# 1) fx fused pipeline smoke: cut->tr->grep->take
out=\$(printf 'a,b,c\nb,b,c\n' | fx cut -d , -f2 tr a-z A-Z grep -E '^B' | wc -l)
test \"\$out\" = \"2\" || { echo 'fx fused failed'; exit 1; }

# 2) standalone pipeline should match fused
out2=\$(printf 'a,b,c\nb,b,c\n' | fp_cut -d , -f2 | fp_tr a-z A-Z | fp_grep -E '^B' | wc -l)
test \"\$out2\" = \"2\" || { echo 'standalone chain failed'; exit 1; }

# 3) grep -F -i and -m short-circuit
out3=\$(printf 'foo\nFoo\nbar\n' | fp_grep -F -i foo -m 1 | wc -l)
test \"\$out3\" = \"1\" || { echo 'grep -m failed'; exit 1; }

# 4) take as sink short-circuits
out4=\$(seq 1 100 | fp_take 3 | wc -l)
test \"\$out4\" = \"3\" || { echo 'take failed'; exit 1; }

# 5) multiple sources: emit + cat in one fx plan
tmpfile=$(mktemp)
printf 'x,b,c\n' > "$tmpfile"
out5=$($BASH_BIN -c "enable -f ./build/fp_prelude.so fx fp_cat fp_emit fp_cut fp_tr fp_grep fp_take;
  fx emit 'a,b,c' cat '$tmpfile' cut -d , -f2 tr a-z A-Z grep -E '^B' | wc -l")
test "$out5" = "2" || { echo 'multi-source fx failed'; rm -f "$tmpfile"; exit 1; }
rm -f "$tmpfile"

echo 'OK'
"
