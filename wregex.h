/*
 *1	wregex
 *#	This is my regular expression engine implementation. It supports a fairly
 *#	large language, with curly braces, anchors, sub-match extraction, back
 *#	references and lazy (non-greedy) evaluation.\n
 *# 
 *#	This document describes the API used for embedding {*wregex*} into an
 *#	application.
 *# Please refer to the {{README.md}} for a more detailed description of the
 *# syntax, symantics and internal aspects of {*wregex*}.
 *{
 **	In general, a regular expression is compiled into a {{wregex_t}} structure using
 *#	{{wrx_comp()}}.
 **	This {{wregex_t}} structure is then matched against one or more strings using
 *#	the {{wrx_exec()}} function.
 **	When everything is done, the {{wregex_t}}'s memory is deallocated with the
 *#	{{wrx_free()}} function
 *#
 **	If errors happen along the way, their meanings can be found using the
 *#	{{wrx_error()}} function.
 *}
 *#	The file {{wgrep.c}} example program demonstrates this process. It contains an example 
 *# that demonstrates the library by using it in a grep-like utility.\n
 *#
 *# The file {{test.c}} contains a series of unit tests that the engine is supposed to
 *# handle.
 */

#ifndef _WREGEX_H
#define _WREGEX_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*
 * A single state in the NFA
 */
typedef struct _wrx_state
{
	char op;	/* opcode */
	short s[2]; /* State transitions */

	union {
		char c;		/* Actual character */
		char *bv;	/* Bit vector for storing ranges of characters */
		short idx;	/* Index if this is a submatch/backreference state (REC, STP, BRF) */
	} data;
} wrx_state;

/*-
 *@ typedef struct _wregex_t wregex_t
 *# Structure representing a complete NFA of the regular expression.
 */
typedef struct _wregex_t
{
	wrx_state *states;  /* The states themselves */
	short n_states;	/* The number of states */

	short ns; /* The next available state */

	short	start,	/* The start state */
			stop;	/* The stop state */

	/*	Number of submatches in the NFA, essentially the number
	 *	of '('s in the regex */
	int	  n_subm;

	/* Copy of the pattern passed to wrx_comp() */
	char *p;
} wregex_t;

/*@ typedef struct _wregmatch_t wregmatch_t
 *#	Structure used to capture a submatch.
 *[
 *#	typedef struct _wregmatch_t {
 *#		const char *beg;
 *#		const char *end;
 *#	} wregmatch_t;
 *]
 */
typedef struct _wregmatch_t
{
	/* Beginning of the submatch */
	const char *beg;
	/* End of the submatch */
	const char *end;
} wregmatch_t;

/*-
 *@ wregex_t *wrx_comp(const char *pattern, int *e, int *ep)
 *#	Regular expression NFA Compiler.
 *#	It creates a {{wregex_t}} structure from the pattern.\n
 *#	{{pattern}} The pattern to compile into a {{wregex_t}}.\n
 *#	{{e}} Pointer to an integer that will contain an error code on error.
 *#		it may be {{NULL}}. Use {{wrx_error()}} to get a description of the error.
 *#	{{ep}} Pointer to an integer that indicates the position of an error,
 *#		if any. In the case of an error, the last character compiled was at
 *#		{{pattern[ep]}}.\n
 *#	It returns a pointer to the compiled {{wregex_t}} on success, {{NULL}} on failure.
 */
wregex_t *wrx_comp(const char *pattern, int *e, int *ep);

/*@ int wrx_exec(const wregex_t *wreg, const char *str, wregmatch_t subm[], int nsm)
 *#	Pattern matching function.\n
 *#	Matches the regular expression compiled by {{wrx_comp()}} against a string {/'str'/}.\n
 *#	{{wreg}} is the wregex_t compiled by {{wrx_comp()}}
 *#	{{str}} is the string to match against the {{wregex_t}}.\n
 *#	{{subm}} is an array of {{wregmatch_t}} structures that will store the submatches
 *#		The i'th submatch is stored in {{subm[i]}}. It may be {{NULL}}, in which case
 *#		no submatches will be extracted. {{subm[0]}} is always the entire
 *#		matching part of the string.\n
 *#	{{nsm}} The number of elements in the {{subm}} array.\n
 *#	Returns 1 on a match, 0 on no match, and < 0 on a error. Use {{wrx_error()}}
 *#		to get a message associated with the error.
 */
int wrx_exec(const wregex_t *wreg, const char *str, wregmatch_t subm[], int nsm);

/*@ void wrx_free(wregex_t *wreg)
 *#	Deallocates a {{wregex_t}} object compiled by {{wrx_comp()}}.
 */
void wrx_free(wregex_t *wreg);

/*@ const char *wrx_error(int code)
 *#	Returns a description of an error code.\n
 *#	{{code}} is either the integer pointed to by {{wrx_comp()}}'s {{e}} parameter or
 *#	{{wrx_exec()}}'s return value.\n
 */
const char *wrx_error(int code);

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

#endif /* _WREGEX_H */

/*-
 *2	Links
 *#	In implementing this regex pattern matcher,  I consulted these sources:
 *{
 ** "Pattern Matching with Regular Expressions in C++" by Oliver Müller
 *# 	Published in Issue 27 of Linux Gazette, April 1998
 *# 	http://linuxgazette.net/issue27/mueller.html
 ** "Regular Expression Matching Can Be Simple And Fast
 *# 	(but is slow in Java, Perl, PHP, Python, Ruby, ...)" by Russ Cox
 *# 	http://swtch.com/~rsc/regexp/regexp1.html
 ** "The text editor sam" Rob Pike
 *# 	http://plan9.bell-labs.com/sys/doc/sam.html
 *# 	and the implementation at
 *# 	http://plan9.bell-labs.com/sources/plan9/sys/src/libregexp/regexec.c
 ** "COMPILERS Principles, Techniques and Tools" Aho, Sethi, Ullman
 *# 	Addison Wesley 1986
 ** The webpages at http://www.regular-expressions.info, such as the one at
 *# 	"Regular Expression Basic Syntax Reference" provided some insight
 *# 	http://www.regular-expressions.info/reference.html
 ** "How Regexes Work" by Mark-Jason Dominus from The Perl Journal
 *# 	http://perl.plover.com/Regex/article.html
 ** "Writing own regular expression parser" By Amer Gerzic from
 *# 	The Code Project
 *# 	http://www.codeproject.com/cpp/OwnRegExpressionsParser.asp
 ** Ozan S. Yigit's implementation at http://www.cse.yorku.ca/~oz/
 ** "Algorithmic Forays" by Eli Bendersky, accessible from
 *# 	http://www.gamedev.net/reference/list.asp?categoryid=25
 ** "Understanding Regular Expressions" Jeffrey Friedl
 *# 	http://www.foo.be/docs/tpj/issues/vol1_2/tpj0102-0006.html
 ** "Regular Expressions" by Hardeep Singh
 *# 	http://seeingwithc.org/topic7html.html
 ** Henry Spencer's implementation can be found at http://arglist.com/regex/
 ** The blog at http://www.codinghorror.com/blog/archives/000488.html also
 *# 	discusses catastrophic backtracking.
 ** http://en.wikipedia.org/wiki/Regex
 *}
 *-
 **2 License
 *#	{*wregex*} is released under the {/MIT-license/}:
 *[
 *# Copyright (c) 2007-2015 Werner Stoop
 *#
 *# Permission is hereby granted, free of charge, to any person
 *# obtaining a copy of this software and associated documentation
 *# files (the "Software"), to deal in the Software without
 *# restriction, including without limitation the rights to use,
 *# copy, modify, merge, publish, distribute, sublicense, and/or sell
 *# copies of the Software, and to permit persons to whom the
 *# Software is furnished to do so, subject to the following
 *# conditions:
 *#
 *# The above copyright notice and this permission notice shall be
 *# included in all copies or substantial portions of the Software.
 *#
 *# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *# OTHER DEALINGS IN THE SOFTWARE.
 *]
 */
