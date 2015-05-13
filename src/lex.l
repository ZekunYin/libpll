/*
    Copyright (C) 2015 Tomas Flouri

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Tomas Flouri <Tomas.Flouri@h-its.org>,
    Heidelberg Institute for Theoretical Studies,
    Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
*/
%{
#include "unrooted.h"
#include "pll.h"

static size_t string_length = 0;

static char * xstrndup(const char * s, size_t len)
{
  char * p = (char *)malloc(len+1);
  if (!p)
  {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Unable to allocate enough memory.");
    return PLL_FAILURE;
  }
  strncpy(p,s,len);
  p[len] = 0;
  return p;
}

static char * append(size_t * dstlen, const char * src, size_t srclen)
{
  char * mem = (char *)realloc(yylval.s, (*dstlen + srclen + 1));
  if (!mem)
  {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Unable to allocate enough memory.");
    return PLL_FAILURE;
  }
  yylval.s =  mem;
  strncpy(&yylval.s[*dstlen], src, srclen);
  yylval.s[*dstlen + srclen] = 0;
  *dstlen += srclen;
  return yylval.s;
}

%}
%option nounput
%option noinput
%s apos
%s quot

%%
<quot>{
\\\"                                         append(&string_length, "\"", 1);
\'                                           append(&string_length, "\'", 1);
\"                                           {BEGIN(INITIAL);return STRING;}
}

<apos>{
\\\'                                         append(&string_length, "\'", 1);
\"                                           append(&string_length, "\"", 1);
\'                                           {BEGIN(INITIAL);return STRING;}
}

<apos,quot>{
\\n                                          append(&string_length, "\n", 1);
\\t                                          append(&string_length, "\t", 1);
\\\\                                         append(&string_length, "\\", 1);
([^\"\'\\]|\n)+                              append(&string_length, yytext, yyleng);
}

\:                                           return COLON;
\;                                           return SEMICOLON;
\)                                           return CPAR;
\(                                           return OPAR;
\,                                           return COMMA;
\"                                           {string_length =0; BEGIN(quot);}
\'                                           {string_length =0; BEGIN(apos);}
[a-zA-Z_][a-zA-Z_0-9]*                       {yylval.s = xstrndup(yytext, yyleng); return STRING;}
[\+|\-]?[0-9]+                               {yylval.d = xstrndup(yytext, yyleng); return NUMBER;}
[\+|\-]?[0-9]+\.[0-9]+([e|E][\+|\-]?[0-9]+)? {yylval.d = xstrndup(yytext, yyleng); return NUMBER;}
[ \t\n\r]                                    ;
.                                            {snprintf(pll_errmsg, 200, "Syntax error (%c)\n", yytext[0]);
                                              pll_errno = PLL_ERROR_NEWICK_SYNTAX; 
                                              return PLL_FAILURE;}
%%