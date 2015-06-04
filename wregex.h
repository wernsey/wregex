/**
 *	\mainpage WRegex
 *
 *	This is my regular expression engine implementation. It supports a fairly
 *	large language, with curly braces, anchors, sub-match extraction, back
 *	references and lazy (non-greedy) evaluation.
 *
 *	These documents describe the API used for embedding WRegex into an
 *	application. Please refer to the @ref notes for a more detailed
 *	description of the other aspects of \b Wregex.
 *
 *	In general, a regular expression is compiled into a wrx_nfa structure using
 *	wrx_comp().
 *
 *	This wrx_nfa structure is then matched against one or more strings using
 *	the wrx_exec() function.
 *
 *	When everything is done, the wrx_nfa's memory is deallocated with the
 *	wrx_free_nfa() function
 *
 *	If errors happen along the way, their meanings can be found using the
 *	wrx_err() function.
 *
 *	The @ref wgrep.c example demonstrates this process.
 *
 *	\b Wregex is released under the @ref license "MIT-license"
 *
 *	- For more detailed information, see my @ref notes, extracted from the
 *		included \c notes.txt
 *	- The @ref wgrep.c example demonstrates the library through a \c grep-like
 *		utility.
 *	- I also maintain several @ref links to other regex-related information
 */

/** \page license
 *
 * Copyright (c) 2007-2015 Werner Stoop
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 *	\file wregex.h
 *	Import header for \b Wregex.
 */

#ifndef _WREGEX_H
#define _WREGEX_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*
 * A single state in the NFA
 */
typedef struct
{
	char op;	/* opcode */
	short s[2]; /* State transitions */

	union {
		char c;		/* Actual character */
		char *bv;	/* Bit vector for storing ranges of characters */
		short idx;	/* Index if this is a submatch/backreference state (REC, STP, BRF) */
	} data;
} wrx_nfa_state;

/**
 * Structure representing a complete NFA
 */
typedef struct
{
	wrx_nfa_state *states;  /* The states themselves */
	short n_states;	/* The number of states */

	short ns; /* The next available state */

	short	start,	/* The start state */
			stop;	/* The stop state */

	/**
	 *	Number of submatches in the NFA, essentially the number of '('s in
	 *	the regex
	 */
	int	  n_subm;

	/**
	 * Copy of the pattern passed to wrx_comp()
	 */
	char *p;
} wrx_nfa;

/**
 *	Structure used to capture a submatch
 */
typedef struct
{
	/** Beginning of the submatch */
	const char *beg;
	/** End of the submatch */
	const char *end;
} wrx_subm;

/**
 *	NFA Compiler.
 *	It creates a wrx_nfa structure from the pattern
 *	@param pattern The pattern to compile into a wrx_nfa
 *	@param e Pointer to an integer that will contain an error code on error.
 *		it may be \c NULL. Use wrx_err() to get a description of the error
 *	@param ep Pointer to an integer that indicates the position of an error,
 *		if any. In the case of an error, the last character compiled was at
 *		\c pattern[ep]
 *	@return A pointer to the compiled wrx_nfa on success, \c NULL on failure.
 */
wrx_nfa *wrx_comp(char *pattern, int *e, int *ep);

/**
 *	Pattern matching function
 *	Matches the NFA compiled by wrx_comp() against a string 'str'.
 *	@param nfa The wrx_nfa compiled by wrx_comp()
 *	@param str The string to match against the wrx_nfa
 *	@param subm An array of wrx_subm structures that will store the submatches
 *		The i'th submatch is stored in subm[i]. It may be \c NULL, in which case
 *		no submatches will be extracted. Submatch[0] is always the entire
 *		matching part of the string.
 *	@param nsm The number of elements in the \c subm array
 *	@return 1 on a match, 0 on no match, and <0 on a error. Use wrx_err()
 *		to get a message associated with the error.
 */
int wrx_exec(const wrx_nfa *nfa, const char *str, wrx_subm subm[], int nsm);

/**
 *	Deallocates an NFA compiled by wrx_comp()
 *	@param nfa A valid wrx_nfa compiled by wrx_comp()
 */
void wrx_free_nfa(wrx_nfa *nfa);

/**
 *	Returns a description of the error code for wrx_comp()'s \c e parameter or
 *	wrx_exec()'s return value
 *	@param code Either the integer pointed to by wrx_comp()'s \c e parameter or
 *		wrx_exec()'s return value
 *	@return a string describing the error
 */
const char *wrx_err(int code);

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

#endif /* _WREGEX_H */

/**
 *	\page notes
 *	These notes describe the regex language used by WRegex and some details
 *	about the implementation.
 *
 *	They are generated from the \c notes.txt file included in the source.
 *
 *	\verbinclude notes.txt
 */

/**
 *	\page links
 *	In implementing this Regex pattern matcher,  I consulted these sources:
 * -# "Pattern Matching with Regular Expressions in C++" by Oliver Müller
 * 	Published in Issue 27 of Linux Gazette, April 1998
 * 	http://linuxgazette.net/issue27/mueller.html
 * -# "Regular Expression Matching Can Be Simple And Fast
 * 	(but is slow in Java, Perl, PHP, Python, Ruby, ...)" by Russ Cox
 * 	http://swtch.com/~rsc/regexp/regexp1.html
 * -# "The text editor sam" Rob Pike
 * 	http://plan9.bell-labs.com/sys/doc/sam.html
 * 	and the implementation at
 * 	http://plan9.bell-labs.com/sources/plan9/sys/src/libregexp/regexec.c
 * -# "COMPILERS Principles, Techniques and Tools" Aho, Sethi, Ullman
 * 	Addison Wesley 1986
 * -# The webpages at http://www.regular-expressions.info, such as the one at
 * 	"Regular Expression Basic Syntax Reference" provided some insight
 * 	http://www.regular-expressions.info/reference.html
 * -# "How Regexes Work" by Mark-Jason Dominus from The Perl Journal
 * 	http://perl.plover.com/Regex/article.html
 * -# "Writing own regular expression parser" By Amer Gerzic from
 * 	The Code Project
 * 	http://www.codeproject.com/cpp/OwnRegExpressionsParser.asp
 * -# Ozan S. Yigit's implementation at http://www.cse.yorku.ca/~oz/
 * -# "Algorithmic Forays" by Eli Bendersky, accessible from
 * 	http://www.gamedev.net/reference/list.asp?categoryid=25
 * -# "Understanding Regular Expressions" Jeffrey Friedl
 * 	http://www.foo.be/docs/tpj/issues/vol1_2/tpj0102-0006.html
 * -# "Regular Expressions" by Hardeep Singh
 * 	http://seeingwithc.org/topic7html.html
 * -# Henry Spencer's implementation can be found at http://arglist.com/regex/
 * -# The blog at http://www.codinghorror.com/blog/archives/000488.html also
 * 	discusses catastrophic backtracking.
 * -# http://en.wikipedia.org/wiki/Regex
 */

/**
 *	\example wgrep.c
 *	This example demonstrates a simple variant of the well-known \c grep utility
 *
 */
