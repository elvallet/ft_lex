#!/bin/bash

# ─────────────────────────────────────────────────────────────────────────────
#  ft_lex compression demo — C99 lexer benchmark
# ─────────────────────────────────────────────────────────────────────────────

BINARY="./ft_lex"
LEXER="tests_files/benchmark.l"

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

lines_of() {
    "$@" 2>/dev/null | wc -l
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

# ─── Run measurements ─────────────────────────────────────────────────────────

echo ""
echo -e "${BLD}${CYN}  ft_lex — table compression benchmark${RST}"
echo -e "${DIM}  lexer: $LEXER (C99 full keyword + operator set)${RST}"
echo ""
echo -e "${DIM}  measuring...${RST}"
echo ""

FT_FULL=$(bytes_of  $BINARY "$LEXER" -t)
FT_COMP=$(bytes_of  $BINARY "$LEXER" -t -c)

HAS_FLEX=false
if command -v flex &>/dev/null; then
    HAS_FLEX=true
    FL_FULL=$(bytes_of flex -t -Cf "$LEXER")
    FL_COMP=$(bytes_of flex -t      "$LEXER")
fi

MAX=$FT_FULL

# ─── Display ──────────────────────────────────────────────────────────────────

fmt_bytes() {
    local b=$1
    awk "BEGIN { printf \"%d KB\", $b / 1024 }"
}

echo -e "  ${BLD}ft_lex — uncompressed${RST}"
echo -e "  $(bar $FT_FULL $MAX)  $(fmt_bytes $FT_FULL)  ${DIM}(${FT_FULL} bytes)${RST}"
echo ""
echo -e "  ${BLD}ft_lex — compressed ${YLW}(-c)${RST}"
echo -e "  $(bar $FT_COMP $MAX)  $(fmt_bytes $FT_COMP)  ${DIM}(${FT_COMP} bytes)${RST}"
echo ""

FT_RATIO=$(ratio $FT_FULL $FT_COMP)
echo -e "  ${BLD}compression ratio :${RST} ${GRN}${BLD}×${FT_RATIO}${RST}"
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
    echo -e "  ${BLD}flex ratio :${RST}        ${BLU}×${FL_RATIO}${RST}  ${DIM}(uses default-chain on top of packing)${RST}"
    echo ""
fi

echo -e "  ${DIM}────────────────────────────────────────────────────${RST}"
echo ""
echo -e "  ${DIM}algorithm : row displacement packing (Dragon Book §3.9)${RST}"
echo -e "  ${DIM}tables    : yy_base[] / yy_next[] / yy_check[]${RST}"
echo -e "  ${DIM}note      : flex adds default-chain compression on top${RST}"
echo ""