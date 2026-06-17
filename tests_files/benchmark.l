%{
#include <stdio.h>
%}

DIGIT       [0-9]
HEXDIGIT    [0-9a-fA-F]
LETTER      [a-zA-Z_]
ALNUM       [a-zA-Z0-9_]

INT         {DIGIT}+
HEXINT      0[xX]{HEXDIGIT}+
OCTINT      0[0-7]+
FLOAT       {DIGIT}+\.{DIGIT}+([eE][+-]?{DIGIT}+)?
IDENT       {LETTER}{ALNUM}*

%%

"auto"          { printf("KW\n"); }
"break"         { printf("KW\n"); }
"case"          { printf("KW\n"); }
"char"          { printf("KW\n"); }
"const"         { printf("KW\n"); }
"continue"      { printf("KW\n"); }
"default"       { printf("KW\n"); }
"do"            { printf("KW\n"); }
"double"        { printf("KW\n"); }
"else"          { printf("KW\n"); }
"enum"          { printf("KW\n"); }
"extern"        { printf("KW\n"); }
"float"         { printf("KW\n"); }
"for"           { printf("KW\n"); }
"goto"          { printf("KW\n"); }
"if"            { printf("KW\n"); }
"inline"        { printf("KW\n"); }
"int"           { printf("KW\n"); }
"long"          { printf("KW\n"); }
"register"      { printf("KW\n"); }
"restrict"      { printf("KW\n"); }
"return"        { printf("KW\n"); }
"short"         { printf("KW\n"); }
"signed"        { printf("KW\n"); }
"sizeof"        { printf("KW\n"); }
"static"        { printf("KW\n"); }
"struct"        { printf("KW\n"); }
"switch"        { printf("KW\n"); }
"typedef"       { printf("KW\n"); }
"union"         { printf("KW\n"); }
"unsigned"      { printf("KW\n"); }
"void"          { printf("KW\n"); }
"volatile"      { printf("KW\n"); }
"while"         { printf("KW\n"); }
"_Bool"         { printf("KW\n"); }
"_Complex"      { printf("KW\n"); }
"_Imaginary"    { printf("KW\n"); }
"NULL"          { printf("KW\n"); }
"true"          { printf("KW\n"); }
"false"         { printf("KW\n"); }

{HEXINT}        { printf("HEX\n"); }
{OCTINT}        { printf("OCT\n"); }
{FLOAT}         { printf("FLOAT\n"); }
{INT}           { printf("INT\n"); }
{IDENT}         { printf("ID\n"); }

"=="            { printf("OP\n"); }
"!="            { printf("OP\n"); }
"<="            { printf("OP\n"); }
">="            { printf("OP\n"); }
"&&"            { printf("OP\n"); }
"||"            { printf("OP\n"); }
"++"            { printf("OP\n"); }
"--"            { printf("OP\n"); }
"->"            { printf("OP\n"); }
"<<"            { printf("OP\n"); }
">>"            { printf("OP\n"); }
"+="            { printf("OP\n"); }
"-="            { printf("OP\n"); }
"*="            { printf("OP\n"); }
"/="            { printf("OP\n"); }
"%="            { printf("OP\n"); }
"&="            { printf("OP\n"); }
"|="            { printf("OP\n"); }
"^="            { printf("OP\n"); }
"<<="           { printf("OP\n"); }
">>="           { printf("OP\n"); }

"="             { printf("OP\n"); }
"+"             { printf("OP\n"); }
"-"             { printf("OP\n"); }
"*"             { printf("OP\n"); }
"/"             { printf("OP\n"); }
"%"             { printf("OP\n"); }
"&"             { printf("OP\n"); }
"|"             { printf("OP\n"); }
"^"             { printf("OP\n"); }
"~"             { printf("OP\n"); }
"!"             { printf("OP\n"); }
"<"             { printf("OP\n"); }
">"             { printf("OP\n"); }
"?"             { printf("OP\n"); }
":"             { printf("OP\n"); }

"("             { printf("PUNCT\n"); }
")"             { printf("PUNCT\n"); }
"{"             { printf("PUNCT\n"); }
"}"             { printf("PUNCT\n"); }
"["             { printf("PUNCT\n"); }
"]"             { printf("PUNCT\n"); }
";"             { printf("PUNCT\n"); }
","             { printf("PUNCT\n"); }
"."             { printf("PUNCT\n"); }
"..."           { printf("PUNCT\n"); }

[ \t\r\n]+      { }
.               { printf("UNK\n"); }

%%