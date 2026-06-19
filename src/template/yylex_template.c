#include <stdio.h>
#include <stdlib.h>
#include <string.h>

@@PROLOGUE@@

@@COMPRESSION@@

#define @@YYTEXT_MODE@@
#if defined(YYARRAY_MODE)
#define YYLMAX			8192
	char	yytext[YYLMAX];
#else
	char	*yytext = NULL;
#endif

typedef struct {
	int	rule_id;
	int	trailing_len;
	int trailing_dfa_id;
} YYAcceptEntry;

@@TABLES@@

int		yywrap(void);

int		yylineno	= 1;
size_t	yyleng		= 0;
FILE	*yyin		= NULL;
FILE	*yyout		= NULL;

static char		*yybuf				= NULL;
static size_t	yybuf_size			= 0;
static size_t	yybuf_capa			= 0;
static size_t	yybuf_pos			= 0;

static int		yycurrent_state		= 0;
static int		yymore_flag			= 0;
static size_t	yymore_len			= 0;

#define SINK	@@SINK@@

typedef struct {
	int		rule_id;
	size_t	match_end;
	int		trailing_len;
	int		trailing_dfa_id;
} YYCandidate;

#ifndef YYCAND_MAX
#define YYCAND_MAX 15120
#endif

static YYCandidate	yycandidates[YYCAND_MAX];
static int			yyncandidates = 0;

#define YYBUF_INIT_SIZE	256

 #define ECHO fwrite(yytext, yyleng, 1, yyout)

 #define BEGIN yycurrent_state =

#define yyless(n) do {								\
	size_t	_yyless_n	= (size_t)(n);				\
	yybuf_pos			= base_start + _yyless_n;	\
	match_start			= yybuf_pos;				\
	yyleng				= _yyless_n;				\
	yytext[_yyless_n]	= '\0';						\
} while (0)

#define yymore() (yymore_flag = 1)

static int yyread(void)
{
	if (yybuf_pos < yybuf_size)
		return (unsigned char)yybuf[yybuf_pos++];

	int	c	= fgetc(yyin);
	if (c == EOF)
		return EOF;

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

int input(void)
{
	int c = yyread();

	if (c != EOF && c == '\n')
		yylineno++;

	return c;
}

void unput(int c)
{
	if (yybuf_pos == 0) {
		size_t	new_capa	= yybuf_capa == 0 ? YYBUF_INIT_SIZE : yybuf_capa * 2;
		char	*tmp		= realloc(yybuf, new_capa);

		if (!tmp)
			return;

		memmove(tmp + (new_capa - yybuf_capa), tmp, yybuf_size + 1);
		yybuf_pos	= new_capa - yybuf_capa;
		yybuf_size	+= new_capa - yybuf_capa;
		yybuf		= tmp;
		yybuf_capa	= new_capa;
	}

	yybuf_pos--;
	yybuf[yybuf_pos] = (char)c;

	if (c == '\n' && yylineno > 1)
		yylineno--;
}

static void yy_assign_yytext(size_t start, size_t len)
{
	size_t	total	= yymore_len + len;

#if defined(YYARRAY_MODE)
	if (total >= YYLMAX)
		total = YYLMAX - 1;
	memcpy(yytext + yymore_len, yybuf + start, total - yymore_len);
	yytext[total]	= '\0';
#else
	char	*buf	= malloc(total + 1);
	if (!buf)
		return;
	if (yymore_len > 0 && yytext)
		memcpy(buf, yytext, yymore_len);
	memcpy(buf + yymore_len, yybuf + start, len);
	buf[total]	= '\0';
	free(yytext);
	yytext	= buf;
#endif

	yyleng	= total;
}

void yylex_destroy(void)
{
	if (yybuf)
	{
		free(yybuf);
		yybuf		= NULL;
		yybuf_size	= 0;
		yybuf_capa	= 0;
		yybuf_pos	= 0;
	}
#if !defined(YYARRAY_MODE)
	if (yytext)
	{
		free(yytext);
		yytext	= NULL;
	}
#endif
}

static size_t yy_simulate_trailing(int dfa_id, size_t start, size_t end)
{
	int		size	= yytrailing_sizes[dfa_id];
	int		*accept	= yytrailing_accepts[dfa_id];

#if !defined(MODE_COMPRESSION)
	int	(*table)[256]	= yytrailing_tables[dfa_id];
#else
	int	*base	= yytrailing_bases[dfa_id];
	int	*check	= yytrailing_checks[dfa_id];
	int	*next	= yytrailing_nexts[dfa_id];
#endif

	for (size_t p = start; p <= end; p++) {
		int		state	= 0;
		size_t	pos		= p;

		while (pos < end) {
			unsigned char	c	= (unsigned char)yybuf[pos];

#if !defined(MODE_COMPRESSION)
			state	= table[state][c];
#else
			int offset	= base[state] + c;
			state	= (check[offset] == state) ? next[offset] : -1;
#endif
			if (state < 0)
				break;
			pos++;
		}

		if (pos == end && state >= 0 && state < size && accept[state])
			return p - start;
	}
	return (size_t)-1;
}

int yylex(void)
{
	@@VERBATIM_RULES@@

	static size_t	match_start	= 0;

	static int	initialized	= 0;
	if (!initialized) {
		if (!yyin)	yyin	= stdin;
		if (!yyout)	yyout	= stdout;
		BEGIN(INITIAL);
		initialized	= 1;
	}

	if (yymore_flag) {
		yymore_flag	= 0;
	} else {
#if !defined(YYARRAY_MODE)
		free(yytext);
		yytext	= NULL;
#endif
		yyleng		= 0;
		yymore_len	= 0;
	}
	
	while (1) {
		int yy_at_bol	= (match_start == 0) || (yybuf[match_start - 1] == '\n');

		int state	= yy_at_bol
			? yystart_states[yycurrent_state + YYNB_CONDITIONS]
			: yystart_states[yycurrent_state];

		size_t base_start	= match_start;	
		yybuf_pos			= match_start;
		yyncandidates		= 0;
		while (1) {
			int	c	= yyread();

			if (c == EOF) {
				if (yyncandidates > 0)
					break;
				if (yywrap() == 1)
					return 0;
				continue;
			}

#if !defined(MODE_COMPRESSION)
			state	= yytable[state][(unsigned char)c];
#else
			int offset	= yybase[state] + c;
			if (yycheck[offset] == state)
				state = yynext[offset];
			else
				state = -1;
#endif

			if (state == -1 || state == SINK)
				break;
				
			if (yyaccept_count[state] > 0) {
				int	base	= yyaccept_offset[state];
				int	cnt		= yyaccept_count[state];
				for (int ri = cnt - 1; ri >= 0 && yyncandidates < YYCAND_MAX; ri--) {
					yycandidates[yyncandidates].rule_id			= yyaccept_data[base + ri].rule_id;
					yycandidates[yyncandidates].match_end		= yybuf_pos;
					yycandidates[yyncandidates].trailing_len	= yyaccept_data[base + ri].trailing_len;
					yycandidates[yyncandidates].trailing_dfa_id = yyaccept_data[base + ri].trailing_dfa_id;
					yyncandidates++;
				}
			}
		}
		if (yyncandidates == 0) {
			if (!feof(yyin))
				fputc(yybuf[match_start], yyout);
			match_start++;
			continue;
		}
		int	rule_executed	= 0;
		int ci				= yyncandidates - 1;

		for (; ci >= 0; ci--) {
			YYCandidate *cand	= &yycandidates[ci];

			size_t	full_len	= cand->match_end - base_start;
			size_t	committed_len;

			if (cand->trailing_len >= 0) {
				committed_len = full_len - (size_t)cand->trailing_len;
			} else if (cand->trailing_len == -2) {
				committed_len = yy_simulate_trailing(cand->trailing_dfa_id, match_start, cand->match_end);
				if (committed_len == (size_t)-1)
					continue;
			} else {
				committed_len = full_len;
			}

			yy_assign_yytext(base_start, committed_len);
			yybuf_pos	= base_start + committed_len;

			int saved_yylineno	= yylineno;
			for (size_t i = yymore_len; i < yyleng; i++) {
				if (yytext[i] == '\n')
					yylineno++;
			}

			match_start = yybuf_pos;

			#define REJECT goto yy_try_next_candidate

			switch (cand->rule_id) {
@@RULES@@
			}

			#undef REJECT

			rule_executed = 1;
			break;

yy_try_next_candidate:
			match_start	= base_start;
			yylineno	= saved_yylineno;
		}
		if (!rule_executed) {
			if (!feof(yyin))
				fputc(yybuf[match_start], yyout);
			match_start++;
			continue;
		}
		if (yymore_flag) {
			yymore_len = yyleng;
		} else {
#if !defined(YYARRAY_MODE)
			free(yytext);
			yytext = NULL;
#endif
			yyleng		= 0;
			yymore_len	= 0;
		}
		yymore_flag = 0;
	}
}

@@EPILOGUE@@
