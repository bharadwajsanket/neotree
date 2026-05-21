# ============================================
# neotree Full Edge-Test Suite
# ============================================

set -e

echo "============================================"
echo "BUILD"
echo "============================================"

${CC:-gcc} -std=c99 -Wall -Wextra -Wpedantic -O2 \
main.c tree.c fs.c cli.c utils.c \
-o neotree

echo ""
echo "============================================"
echo "VERSION / HELP"
echo "============================================"

./neotree --version
./neotree --help | head -25

echo ""
echo "============================================"
echo "BASIC"
echo "============================================"

./neotree
./neotree .
./neotree ..

echo ""
echo "============================================"
echo "DEPTH LIMIT"
echo "============================================"

./neotree -L 1
./neotree -L 2

echo ""
echo "============================================"
echo "HIDDEN FILES"
echo "============================================"

./neotree --all

echo ""
echo "============================================"
echo "DIRS ONLY"
echo "============================================"

./neotree --dirs-only
./neotree --dirs-only --all

echo ""
echo "============================================"
echo "SIZE DISPLAY"
echo "============================================"

./neotree --size
./neotree --size --all

echo ""
echo "============================================"
echo "SORTING"
echo "============================================"

./neotree --sort name
./neotree --sort size
./neotree --sort modified
./neotree --sort size --no-dirs-first

echo ""
echo "============================================"
echo "PATTERN MATCHING"
echo "============================================"

./neotree --pattern '*.c'
./neotree --pattern '*.h'
./neotree --pattern '**/*.c'
./neotree --pattern '**/*.h'
./neotree --pattern 'src/**/*.h'

echo ""
echo "============================================"
echo "IGNORE SYSTEM"
echo "============================================"

./neotree --ignore build
./neotree --ignore dist --ignore .cache

echo ""
echo "============================================"
echo "EXTENSION SUMMARY"
echo "============================================"

./neotree --ext-summary
./neotree --pattern '*.c' --ext-summary

echo ""
echo "============================================"
echo "EXPORT TXT"
echo "============================================"

rm -f tree.txt
./neotree --export-txt tree.txt

echo ""
echo "--- tree.txt ---"
cat tree.txt

echo ""
echo "============================================"
echo "EXPORT MARKDOWN"
echo "============================================"

rm -f tree.md
./neotree --export-markdown tree.md

echo ""
echo "--- tree.md ---"
cat tree.md

echo ""
echo "============================================"
echo "DUAL EXPORT"
echo "============================================"

rm -f both.txt both.md

./neotree \
  --export-txt both.txt \
  --export-markdown both.md

echo ""
echo "--- both.txt ---"
head -20 both.txt

echo ""
echo "--- both.md ---"
head -20 both.md

echo ""
echo "============================================"
echo "PIPE SAFETY"
echo "============================================"

./neotree | cat
./neotree --size | tee out.txt

echo ""
echo "============================================"
echo "ERROR HANDLING"
echo "============================================"

./neotree --sort garbage || true
./neotree --export-markdown || true
./neotree --export-txt || true
./neotree /nonexistent || true

echo ""
echo "============================================"
echo "UNICODE TEST"
echo "============================================"

mkdir -p test_unicode
touch "test_unicode/🔥.txt"
touch "test_unicode/hello world.txt"

./neotree test_unicode

rm -rf test_unicode

echo ""
echo "============================================"
echo "SYMLINK TEST"
echo "============================================"

mkdir -p test_link
ln -sf . test_link/loop

./neotree test_link

rm -rf test_link

echo ""
echo "============================================"
echo "STRESS TEST"
echo "============================================"

time ./neotree ~/Developer > /dev/null

echo ""
echo "============================================"
echo "DONE"
echo "============================================"
