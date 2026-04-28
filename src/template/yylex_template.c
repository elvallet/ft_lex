/* -----------------------------------------------------------------------
 * Generated scanner runtime - do not edit by hand.
 * Produced by ft_lex. Template embedded via xxd -i at build time.
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

@@PROLOGUE@@

/* -----------------------------------------------------------------------
 * yytext declaration - YYTEXT_MODE is replaced by the codegen:
 * - %pointer (default) -> "pointer"
 * - %array				-> "array"
 * ----------------------------------------------------------------------- */
#define @@YYTEXT_MODE@@
#if defined(YYARRAY_MODE)
	char	yytext[YYLMAX];	/* %array: fixed buffer, no heap allocation */
#else
	char	*yytext = NULL;	/* %pointer (default): heap-allocated		*/
#endif

/* -----------------------------------------------------------------------
 * 
 * ----------------------------------------------------------------------- */
@@TABLES@@

/* -----------------------------------------------------------------------
 * Global scanner state
 * ----------------------------------------------------------------------- */

int		yywrap(void);			/* User-supplied or provided by libl.		*/

int		yylineno	= 1;		/* Current source line number (1-based).	*/
size_t	yyleng		= 0;		/* Length of current token.					*/
FILE	*yyin		= NULL;		/* Input stream (defaults to stdin in libl).*/
FILE	*yyout		= NULL;		/* Output stream (defaults to stdout).		*/


/* -----------------------------------------------------------------------
 * Internal scanner state
 * ----------------------------------------------------------------------- */

static char		*yybuf			= NULL;	/* Raw input buffer.				*/
static size_t	yybuf_size		= 0;	/* Bytes currently in the buffer.	*/
static size_t	yybuf_capa		= 0;	/* Allocated capacity.				*/
static size_t	yybuf_pos		= 0;	/* Read cursor inside the buffer.	*/

static int		yycurrent_state	= 0;	/* Active start condition index.	*/
static int		yy_at_bol		= 1;	/* Non-zero at beginning of line.	*/

static int		yymore_flag		= 0;	/* Set by yymore().					*/
static size_t	yymore_len		= 0;	/* Accumulated length from yymore(). */

#define YYBUF_INIT_SIZE 256

/* -----------------------------------------------------------------------
 * Public API macros
 * ----------------------------------------------------------------------- */

/* Copy yytext to yyout. */
#define ECHO fwrite(yytext, yyleng, 1, yyout)

/* Switch start condition. Indexes into yystart_states[]. */
#define BEGIN(x) (yycurrent_state = (x))

/* Rewind: keep only the first n bytes of the current match. */
#define yyless(n) do {									\
	size_t	yyless_n	= (size_t)(n);					\
	yybuf_pos	= match_start - yyleng + yyless_n;		\
	match_start	= match_start - yyleng + yyless_n;		\
	yyleng		= yyless_n;								\
	yytext[yyless_n]	= '\0';							\
} while (0)

/* Append the next match to the current yytext instead of replacing it. */
#define yymore() (yymore_flag = 1)

/* Maximum yytext length when compiled in %array mode. */
#define YYLMAX 8192


/* -----------------------------------------------------------------------
 * yyread - fille the buffer one character at a time, with lazy growth.
 * Returns the next unsigned byte, or EOF.
 * ----------------------------------------------------------------------- */
static int yyread(void)
{
	/* Serve from buffer if bytes are still pending. */
	if (yybuf_pos < yybuf_size)
		return (unsigned char)yybuf[yybuf_pos++];
	
	int c = fgetc(yyin);
	if (c == EOF)
		return EOF;
	
	/* Grow buffer of needed (keep one extra byte for the NUL sentinel). */
	if (yybuf_size + 1 >= yybuf_capa) {
		size_t	new_capa	= yybuf_capa == 0 ? YYBUF_INIT_SIZE : yybuf_capa * 2;
		char	*tmp		= realloc(yybuf, new_capa);
		if (!tmp)
			return EOF;
		yybuf		= tmp;
		yybuf_capa	= new_capa;
	}

	yybuf[yybuf_size++]	= (char)c;
	yybuf[yybuf_size]	= '\0';
	yybuf_pos++;
	return (unsigned char)c;
}

/* -----------------------------------------------------------------------
 * yy_assign_yytext - write the matched slice into yytext.
 * Handles both %pointer (heap) and %array (fixed buffer) modes.
 * In yymore() mode, prepends the previously accumulated text.
 * ----------------------------------------------------------------------- */
static void yy_assign_yytext(size_t start, size_t len)
{
	size_t	total	= yymore_len + len;

#if defined(YYARRAY_MODE)
	if (total >= YYLMAX)
		total = YYLMAX - 1;
	if (yymore_len > 0)
		/* text already sits at yytext[0..yymore_len-1] from previous round */;
	memcpy(yytext + yymore_len, yybuf + start, total - yymore_len);
	yytext[total]	= '\0';
#else
	char *buf	= malloc(total + 1);
	if (!buf)
		return;
	/* Prepend accumulated yymore() text if any. */
	if (yymore_len > 0 && yytext)
		memcpy(buf, yytext, yymore_len);
	memcpy(buf + yymore_len, yybuf + start, len);
	buf[total] = '\0';
	free(yytext);
	yytext = buf;
#endif

	yyleng	= total;
}

/* -----------------------------------------------------------------------
 * yylex - main scanner entry point.
 * ----------------------------------------------------------------------- */
int yylex(void)
{
	/* Verbatim code from the rules section (indented lines in the .l file). */
	@@VERBATIM_RULES@@

	/* Persistent across calls: tracks where the next token starts. */
	static size_t match_start	= 0;

	/* One-time initialisation: open stdin/stdout and enter INITIAL. */
	static int initialized = 0;
	if (!initialized) {
		if (!yyin) yyin 	= stdin;
		if (!yyout) yyout	= stdout;
		BEGIN(INITIAL);
		initialized = 1;
	}

	/* Reset yytext for this call unless yymore was requested. */
	if (yymore_flag) {
		yymore_flag = 0;
		/* match_start stays put; yymore_len retains accumulated length. */
	} else {
#if !defined(YYARRAY_MODE)
		free(yytext);
		yytext = NULL;
#endif
		yyleng		= 0;
		yymore_len	= 0;
	}

	/* -------------------------------------------------------------------
	 * Outer loop: scan one token per iteration.
	 * ------------------------------------------------------------------- */
	while (1) {

		/* Select DFA entry point: BOL variant when at beginning of line. */
		int state = yy_at_bol
			? yystart_states[yycurrent_state + YYNB_CONDITIONS]
			: yystart_states[yycurrent_state];

		int		last_match		= -1;
		size_t	last_match_pos	= match_start;

		yybuf_pos	= match_start;

		/* ----------------------------------------------------------------
		 * Inner loop: drive the DFA until it dies or hits EOF.
		 * ---------------------------------------------------------------- */
		while (1) {
			int	c = yyread();

			if (c == EOF) {
				/* If we already found a match, commit it. */
				if (last_match != -1)
					break;
				/* Otherwise, ask the user whether to continue. */
				if (yywrap() == 1)
					return 0;
				continue;
			}

			state = yytable[state][(unsigned char)c];
			if (state == -1)
				break;

			if (yyaccept[state] != -1) {
				last_match		= yyaccept[state];
				last_match_pos	= yybuf_pos;
			}
		}

		/* ----------------------------------------------------------------
		 * No rule matched: echo the unmatched character (POSIX default).
		 * ---------------------------------------------------------------- */
		if (last_match == -1) {
			if (yyin != stdin || !feof(yyin)) {
				fputc(yybuf[match_start], yyout);
			}
			match_start++;
			continue;
		}

		/* ----------------------------------------------------------------
		 * Commit the longest match.
		 * ---------------------------------------------------------------- */
		size_t	 match_len	= last_match_pos - match_start;
		yy_assign_yytext(match_start, match_len);

		/* Update line counter before advancing match_start. */
		for (size_t i = 0; i < match_len; ++i) {
			if (yybuf[match_start + i] == '\n')
				yylineno++;
		}

		match_start	= last_match_pos;
		yybuf_pos	= last_match_pos;

		/* BOL flag: true is the token ends with a newline. */
		yy_at_bol = (yyleng > 0 && yytext[yyleng - 1] == '\n');

		/* Dispatch to user action. */
		switch (last_match) {
@@RULES@@
		}

		/* After the action: handle yymore() or release yytext. */
		if (yymore_flag) {
			yymore_len = yyleng;
			/* yytext is kept alive; next call will prepend it. */
		} else {
#if !defined(YYARRAY_MODE)
			free(yytext);
			yytext = NULL;
#endif
			yyleng		= 0;
			yymore_len	= 0;
		}
		yymore_flag	= 0;
	}
}

@@EPILOGUE@@