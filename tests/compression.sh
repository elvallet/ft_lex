#!/bin/bash

# ─────────────────────────────────────────────────────────────────────────────
#  ft_lex compression demo — C99 lexer benchmark
# ─────────────────────────────────────────────────────────────────────────────

BINARY="./ft_lex"
LEXER="grammar/benchmark.l"

RED='\033[0;31m'
GRN='\033[0;32m'
BLU='\033[0;34m'
CYN='\033[0;36m'
YLW='\033[0;33m'
BLD='\033[1m'
DIM='\033[2m'
RST='\033[0m'

# ─── Sanity checks ────────────────────────────────────────────────────────────

if [ ! -f "$BINARY" ]; then
    echo -e "${RED}error:${RST} binary '$BINARY' not found. Run make first."
    exit 1
fi

if [ ! -f "$LEXER" ]; then
    echo -e "${RED}error:${RST} lexer file '$LEXER' not found."
    exit 1
fi

# ─── Measure helpers ──────────────────────────────────────────────────────────

bytes_of() {
    "$@" 2>/dev/null | wc -c
}

ratio() {
    local base=$1
    local comp=$2
    awk "BEGIN { printf \"%.2f\", $base / $comp }"
}

bar() {
    local val=$1
    local max=$2
    local width=40
    local filled=$(awk "BEGIN { printf \"%d\", ($val / $max) * $width }")
    local empty=$((width - filled))
    printf "${GRN}"
    printf '█%.0s' $(seq 1 $filled)
    printf "${DIM}"
    printf '░%.0s' $(seq 1 $empty)
    printf "${RST}"
}

fmt_bytes() {
    local b=$1
    awk "BEGIN { printf \"%d KB\", $b / 1024 }"
}

# Pull one "label: value" line out of `ft_lex -cv` stats output.
stat_field() {
    local label="$1"
    echo "$FT_STATS" | grep "$label" | sed -E 's/^[[:space:]]*[a-z ]+: //'
}

# ─── Run measurements ─────────────────────────────────────────────────────────

echo ""
echo -e "${BLD}${CYN}  ft_lex — table compression benchmark${RST}"
echo -e "${DIM}  lexer: $LEXER (C99 full keyword + operator set)${RST}"
echo ""
echo -e "${DIM}  measuring...${RST}"
echo ""

FT_FULL=$(bytes_of  $BINARY "$LEXER" -t)
FT_COMP=$(bytes_of  $BINARY "$LEXER" -t -c)
FT_STATS=$($BINARY "$LEXER" -cv 2>&1 >/dev/null)

HAS_FLEX=false
if command -v flex &>/dev/null; then
    HAS_FLEX=true
    FL_FULL=$(bytes_of flex -t -Cf "$LEXER")
    FL_COMP=$(bytes_of flex -t      "$LEXER")
fi

MAX=$FT_FULL

# ─── Display: generated file size (includes fixed runtime boilerplate) ───────

echo -e "  ${BLD}ft_lex — uncompressed${RST}"
echo -e "  $(bar $FT_FULL $MAX)  $(fmt_bytes $FT_FULL)  ${DIM}(${FT_FULL} bytes)${RST}"
echo ""
echo -e "  ${BLD}ft_lex — compressed ${YLW}(-c)${RST}"
echo -e "  $(bar $FT_COMP $MAX)  $(fmt_bytes $FT_COMP)  ${DIM}(${FT_COMP} bytes)${RST}"
echo ""

FT_RATIO=$(ratio $FT_FULL $FT_COMP)
echo -e "  ${BLD}file-size compression ratio :${RST} ${GRN}${BLD}×${FT_RATIO}${RST}"
echo -e "  ${DIM}(includes the fixed runtime boilerplate -- diluted on small lexers)${RST}"
echo ""

if $HAS_FLEX; then
    echo -e "  ${DIM}────────────────────────────────────────────────────${RST}"
    echo ""
    echo -e "  ${BLD}flex — uncompressed ${DIM}(-Cf)${RST}"
    echo -e "  $(bar $FL_FULL $MAX)  $(fmt_bytes $FL_FULL)  ${DIM}(${FL_FULL} bytes)${RST}"
    echo ""
    echo -e "  ${BLD}flex — compressed   ${DIM}(default)${RST}"
    echo -e "  $(bar $FL_COMP $MAX)  $(fmt_bytes $FL_COMP)  ${DIM}(${FL_COMP} bytes)${RST}"
    echo ""
    FL_RATIO=$(ratio $FL_FULL $FL_COMP)
    echo -e "  ${BLD}flex file-size ratio :${RST}        ${BLU}×${FL_RATIO}${RST}"
    echo ""
fi

# ─── Display: table-only stats (the actual compression metric, no boilerplate) ─

RAW_ENTRIES=$(stat_field "raw size")
PACKED_ENTRIES=$(stat_field "packed size")
TABLE_FACTOR=$(stat_field "compression factor")
DFA_STATES=$(echo "$FT_STATS" | grep -A2 "^  DFA" | grep -E "^[[:space:]]+states:" | sed -E 's/^[[:space:]]*states: //')

if [ -n "$RAW_ENTRIES" ] && [ -n "$PACKED_ENTRIES" ]; then
    echo -e "  ${DIM}────────────────────────────────────────────────────${RST}"
    echo ""
    echo -e "  ${BLD}table-only compression (no boilerplate, the real metric)${RST}"
    echo -e "  ${DIM}DFA states: ${DFA_STATES}${RST}"
    echo ""
    echo -e "  raw table entries     : ${RAW_ENTRIES}"
    echo -e "  packed table entries  : ${PACKED_ENTRIES}  ${DIM}(next[]/check[] size)${RST}"
    echo -e "  ${BLD}table compression factor :${RST} ${GRN}${BLD}${TABLE_FACTOR}${RST}"
    echo ""
fi

echo -e "  ${DIM}────────────────────────────────────────────────────${RST}"
echo ""
echo -e "  ${DIM}algorithm : row displacement packing + default[] chain (Dragon Book §3.9)${RST}"
echo -e "  ${DIM}tables    : yy_base[] / yy_next[] / yy_check[] / yy_default[]${RST}"
echo -e "  ${DIM}default[] : max-spanning-tree over state similarity, diff-only profiles packed${RST}"
echo ""