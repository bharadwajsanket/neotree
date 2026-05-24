#!/usr/bin/env bash
# ============================================
# neotree / ntree Full Edge-Test Suite
# ============================================

set -e

echo "============================================"
echo "BUILD"
echo "============================================"

${CC:-gcc} -std=c99 -Wall -Wextra -Wpedantic -O2 \
  main.c tree.c fs.c cli.c utils.c find.c \
  -o neotree

# Create local symlink for alias testing
ln -sf neotree ntree

echo ""
echo "============================================"
echo "VERSION / HELP (neotree and ntree)"
echo "============================================"

./neotree --version
./ntree --version
./neotree --help | head -25
./ntree --help | head -25

echo ""
echo "============================================"
echo "BASIC"
echo "============================================"

./ntree
./ntree .
./ntree ..

echo ""
echo "============================================"
echo "DEPTH LIMIT"
echo "============================================"

./ntree -L 1
./ntree -L 2

echo ""
echo "============================================"
echo "HIDDEN FILES"
echo "============================================"

./ntree --all

echo ""
echo "============================================"
echo "DIRS ONLY"
echo "============================================"

./ntree --dirs-only
./ntree --dirs-only --all

echo ""
echo "============================================"
echo "SIZE DISPLAY"
echo "============================================"

./ntree --size
./ntree --size --all

echo ""
echo "============================================"
echo "SORTING & REVERSING"
echo "============================================"

./ntree --sort name
./ntree --sort size
./ntree --sort modified
./ntree --sort size --no-dirs-first
./ntree --sort size --reverse
./ntree --sort modified --reverse

echo ""
echo "============================================"
echo "STATS SUMMARY"
echo "============================================"

./ntree --stats
./ntree --dirs-only --stats
./ntree --pattern '*.c' --stats

echo ""
echo "============================================"
echo "COMMA-SEPARATED IGNORES"
echo "============================================"

./ntree --ignore 'cli.c,cli.h'
./ntree --ignore 'cli.c,cli.h,tree.c'

echo ""
echo "============================================"
echo "FIND MODE (--find and --find-dir)"
echo "============================================"

mkdir -p test_find_sandbox/dir1/dir2
dd if=/dev/zero of=test_find_sandbox/dir1/file1.c bs=1024 count=1 2>/dev/null
dd if=/dev/zero of=test_find_sandbox/dir1/dir2/file2.h bs=1024 count=1 2>/dev/null
touch test_find_sandbox/dir1/dir2/file3.txt

echo "--- Find exact name ---"
./ntree --find 'file1.c' test_find_sandbox
echo "--- Find relative path ---"
./ntree --find 'dir1/dir2/file2.h' test_find_sandbox
echo "--- Find slash anchor ---"
./ntree --find '/*.c' test_find_sandbox
echo "--- Find wildcard ---"
./ntree --find '**/*.h' test_find_sandbox
echo "--- Find comma-separated ---"
./ntree --find 'file1.c,**/*.txt' test_find_sandbox
echo "--- Find directories ---"
./ntree --find-dir 'dir1,dir2' test_find_sandbox

rm -rf test_find_sandbox

echo ""
echo "============================================"
echo "EXPORT TXT & MARKDOWN"
echo "============================================"

rm -f tree.txt tree.md both.txt both.md

./ntree --export-txt tree.txt
echo "--- tree.txt ---"
cat tree.txt

./ntree --export-markdown tree.md
echo "--- tree.md ---"
cat tree.md

./ntree --export-txt both.txt --export-markdown both.md
echo "--- both.txt ---"
head -15 both.txt
echo "--- both.md ---"
head -15 both.md

rm -f tree.txt tree.md both.txt both.md

echo ""
echo "============================================"
echo "PIPE SAFETY"
echo "============================================"

./ntree | cat
./ntree --size | tee out.txt
rm -f out.txt

echo ""
echo "============================================"
echo "ERROR HANDLING"
echo "============================================"

./ntree --sort garbage || echo "Failed as expected"
./ntree --reverse || echo "Failed as expected: --reverse without --sort"
./ntree --export-markdown || echo "Failed as expected"
./ntree --export-txt || echo "Failed as expected"
./ntree /nonexistent || echo "Failed as expected"

echo ""
echo "============================================"
echo "MULTI-ROOT DIRECTORY SUPPORT"
echo "============================================"

# Prepare test directories
mkdir -p test_multi_1 test_multi_2
touch test_multi_1/fileA.c test_multi_1/fileB.h
touch test_multi_2/fileC.py test_multi_2/fileD.txt

echo "--- Basic rendering of multiple roots ---"
./ntree test_multi_1 test_multi_2

echo "--- Multiple roots with stats ---"
./ntree test_multi_1 test_multi_2 --stats

echo "--- Multiple roots with export-txt ---"
rm -f multi_export.txt
./ntree test_multi_1 test_multi_2 --export-txt multi_export.txt
cat multi_export.txt
rm -f multi_export.txt

echo "--- Multiple roots in find mode ---"
./ntree test_multi_1 test_multi_2 --find '*.c,*.py'

echo "--- Mixed existence / nonexistent roots ---"
./ntree test_multi_1 /nonexistent test_multi_2 || echo "Set exit code 1 as expected"

# Cleanup
rm -rf test_multi_1 test_multi_2

echo ""
echo "============================================"
echo "UNICODE TEST"
echo "============================================"

mkdir -p test_unicode
touch "test_unicode/🔥.txt"
touch "test_unicode/hello world.txt"

./ntree test_unicode

rm -rf test_unicode

echo ""
echo "============================================"
echo "SYMLINK TEST"
echo "============================================"

mkdir -p test_link
ln -sf . test_link/loop

./ntree test_link

rm -rf test_link

# Clean up local alias link
rm -f ntree

echo ""
echo "============================================"
echo "DONE"
echo "============================================"
