/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wregex.h"
#include "wrx_prnt.h"

#define match(p, s)   _match(p, s, __FILE__, __LINE__)

static int _match(const char *p, const char *s, const char *file, int line) {
	int e, ep;
	wregex_t *r;
	wregmatch_t *subm;

	r = wrx_comp(p, &e, &ep);
	if(!r) {
		fprintf(stderr, "\n[%s:%d] ERROR......: %s\n%s\n%*c\n", file, line, wrx_error(e), p, ep + 1, '^');
		exit(EXIT_FAILURE);
	}

	if(r->n_subm > 0) {
		subm = calloc(sizeof *subm, r->n_subm);
		if(!subm) {
			fprintf(stderr, "Error: out of memory (submatches)\n");
			wrx_free(r);
			exit(EXIT_FAILURE);
		}
	} else
		subm = NULL;

	e = wrx_exec(r, s, subm, r->n_subm);

	if(e < 0) fprintf(stderr, "Error: %s\n", wrx_error(e));

	free(subm);
	wrx_free(r);

	return e;
}

/* Macro to test patterns that should match strings */
#define MATCH(x,y)  do{\
					total++;\
					if(match(x,y) == 1) \
					{\
						success++;\
						printf("[%s:%3d] SUCCESS....: \"%s\" =~ \"%s\"\n", __FILE__, __LINE__, x, y);\
					}\
					else\
					{\
						printf("[%s:%3d] FAIL.......: \"%s\" !~ \"%s\"\n", __FILE__, __LINE__, x, y);\
					}\
                    fflush(stdout);\
					} while(0)


/* Macro to test patterns that should not match strings */
#define NOMATCH(x,y)  do{\
					total++;\
					if(match(x,y) != 1) \
					{\
						success++;\
						printf("[%s:%3d] SUCCESS....: \"%s\" !~ \"%s\"\n", __FILE__, __LINE__, x, y);\
					}\
					else\
					{\
						printf("[%s:%3d] FAIL.......: \"%s\" =~ \"%s\"\n", __FILE__, __LINE__, x, y);\
					}\
                    fflush(stdout);\
					} while(0)

int main(int argc, char *argv[]) {
	int i, e, ep, len, nsm;

	int total = 0, success = 0;

	wregex_t *r;
	char *pat, *str = NULL;
	wregmatch_t *subm;

	char *buf;

	if(argc < 2) {
		/* Run unit tests */
		MATCH("def", "abcdefghi");
		NOMATCH("def", "abcdfghi");

		/* Match only at start of line */
		MATCH("^abc", "abcdef");
		MATCH("^abc", "\rabcdef");
		MATCH("^abc", "\nabcdef");
		MATCH("^def", "abc\ndef\nghi");
		MATCH("^ghi", "abc\ndef\rghi");
		NOMATCH("^def", "abcdef");
		NOMATCH("^def", "ab\ncdef\nghi");
		NOMATCH("^ghi", "abc\ndefg\rhi");

		/* Match only at end of line */
		MATCH("def$", "abcdef");
		MATCH("def$", "abcdef\n");
		MATCH("def$", "abcdef\r");
		NOMATCH("abc$", "abcdef");

		/* Match zero or more b's */
		MATCH("ab*c", "ac");
		MATCH("ab*c", "abbbbbbbbc");
		NOMATCH("ab*c", "abbbbbbbb");

		/* Match one or more b's */
		NOMATCH("ab+c", "ac");
		MATCH("ab+c", "abbbbbbbbc");
		NOMATCH("ab+c", "abbbbbbbb");

		/* Match 0 or one b's */
		MATCH("ab?c", "abc");
		MATCH("ab?c", "ac");
		NOMATCH("ab?c", "abbc");

		/* Match either "ab", "cd" or "ef"  */
		MATCH("ab|cd|ef", "abc");
		MATCH("ab|cd|ef", "acd");
		MATCH("ab|cd|ef", "aef");
		NOMATCH("ab|cd|ef", "ace");
		
		MATCH("a(b|c)d", "abd");
		MATCH("a(b|c)d", "acd");
		NOMATCH("a(b|c)d", "aed");
		
		MATCH("a(b|)d", "abd");
		MATCH("a(b|)d", "ad");
		NOMATCH("a(b|)d", "aed");		
		
		/* Non capturing groups */
		MATCH("a(:b|c)d", "abd");
		MATCH("a(:b|c)d", "acd");
		NOMATCH("a(:b|c)d", "aed");
		
		MATCH("a(:b|)d", "abd");
		MATCH("a(:b|)d", "ad");
		NOMATCH("a(:b|)d", "aed");		

		/* Match exactly 2 */
		MATCH("ab{2}c", "abbc");
		NOMATCH("ab{2}c", "abbbc");
		NOMATCH("ab{2}c", "abc");

		/* Match at most 2 */
		MATCH("ab{,2}c", "ac");
		MATCH("ab{,2}c", "abc");
		MATCH("ab{,2}c", "abbc");
		NOMATCH("ab{,2}c", "abbbc");

		/* match at least 2 */
		NOMATCH("ab{2,}c", "ac");
		NOMATCH("ab{2,}c", "abc");
		MATCH("ab{2,}c", "abbc");
		MATCH("ab{2,}c", "abbbc");

		/* match at least 2 and at most 4 */
		NOMATCH("ab{2,4}c", "ac");
		NOMATCH("ab{2,4}c", "abc");
		MATCH("ab{2,4}c", "abbc");
		MATCH("ab{2,4}c", "abbbc");
		MATCH("ab{2,4}c", "abbbbc");
		NOMATCH("ab{2,4}c", "abbbbbc");

		/* Character sets */
		MATCH("[abc]{3}", "abc");
		MATCH("[a-c]{3}", "abc");
		NOMATCH("[abc]{3}", "dbc");
		NOMATCH("[a-c]{3}", "dbc");
		MATCH("[a\\-c]{3}", "ac-");
		NOMATCH("[a\\-c]{3}", "abc");
		MATCH("[\\a]{3}", "abc");
		NOMATCH("[^abc]{3}", "abc");
		NOMATCH("[^a-c]{3}", "abc");
		MATCH("[^abc]{3}", "def");
		MATCH("[^a-c]{3}", "def");
		MATCH("[\\^ac]{3}", "ac^");
		NOMATCH("[\\^ac]{3}", "abc");
		MATCH("[\\]ac]{3}", "ac]");
		NOMATCH("[\\]ac]{3}", "abc");
		MATCH("[\\r\\n\\t]{3}", "\r\n\t");
		MATCH("[\r\n\t]{3}", "\r\n\t");
		MATCH("[\\d]{3}", "123");
		NOMATCH("[\\d]{3}", "abc");
		MATCH("[\\a]{3}", "abc");
		NOMATCH("[\\a]{3}", "123");
		MATCH("[\\u]{3}", "ABC");
		NOMATCH("[\\u]{3}", "abc");
		MATCH("[\\l]{3}", "abc");
		NOMATCH("[\\l]{3}", "ABC");
		MATCH("[\\w]{4}", "aA0_");
		NOMATCH("[\\w]{4}", "aA0*");
		MATCH("[\\x]{4}", "a0B9");
		NOMATCH("[\\x]{4}", "a0z9");

		/* Case insensitive tests */
		MATCH("\\iabc\\Iabc", "abcabc");
		MATCH("\\iabc\\Iabc", "AbCabc");
		NOMATCH("\\iabc\\Iabc", "defAbc");
		NOMATCH("\\iabc\\Iabc", "AbCAbc");

		MATCH("\\i[a-c]{3}\\I[a-c]{3}", "abcabc");
		MATCH("\\i[a-c]{3}\\I[a-c]{3}", "AbCabc");
		NOMATCH("\\i[a-c]{3}\\I[a-c]{3}", "AbCAbC");
		NOMATCH("\\i[a-c]{3}\\I[a-c]{3}", "AbCdef");
		MATCH("\\i[^a-c]{3}\\I[a-c]{3}", "dEfabc");
		NOMATCH("\\i[^a-c]{3}\\I[a-c]{3}", "abcabc");
		NOMATCH("\\i[^a-c]{3}\\I[a-c]{3}", "ABCabc");

		MATCH("\\i\\I", "abc");
		NOMATCH("^\\i\\I$", "abc");
		MATCH("^\\i\\I$", "");

		/* Submatches/Backreferences */
		MATCH("(abc) \\1", "abc abc");
		NOMATCH("(abc) \\1", "abc bbc");
		MATCH("((abc) \\2)-\\1", "abc abc-abc abc");
		NOMATCH("((abc) \\2)-\\1", "abc-abc abc abc");

		MATCH("([abc]{3})-\\i\\1", "abc-abc");
		MATCH("([abc]{3})-\\i\\1", "abc-ABC");
		NOMATCH("([abc]{3})-\\i\\1", "aBc-AbC");
		MATCH("([abcABC]{3})-\\i\\1", "aBc-AbC");
		MATCH("\\i([abc]{3})-\\1", "aBc-AbC");

		/* Escape sequences */
		MATCH("\\.", ".");
		NOMATCH("\\.", "a");
		MATCH("\\*", "*");
		NOMATCH("\\*", "a");
		MATCH("\\+", "+");
		NOMATCH("\\+", "a");
		MATCH("\\?", "?");
		NOMATCH("\\?", "a");
		MATCH("\\[", "[");
		NOMATCH("\\[", "a");
		MATCH("\\]", "]");
		NOMATCH("\\]", "a");
		MATCH("\\(", "(");
		NOMATCH("\\(", "a");
		MATCH("\\)", ")");
		NOMATCH("\\)", "a");
		MATCH("\\{", "{");
		NOMATCH("\\{", "a");
		MATCH("\\}", "}");
		NOMATCH("\\}", "a");
		MATCH("\\|", "|");
		NOMATCH("\\|", "a");
		MATCH("\\^", "^");
		NOMATCH("\\^", "a");
		MATCH("\\$", "$");
		NOMATCH("\\$", "a");
		MATCH("\\<", "<");
		NOMATCH("\\<", "a");
		MATCH("\\>", ">");
		NOMATCH("\\>", "a");
		MATCH("\\:", ":");
		NOMATCH("\\:", "a");
		MATCH("\\r", "\r");
		NOMATCH("\\r", "a");
		MATCH("\\n", "\n");
		NOMATCH("\\n", "a");
		MATCH("\\t", "\t");
		NOMATCH("\\t", "a");
		MATCH("\r", "\r");
		NOMATCH("\r", "a");
		MATCH("\n", "\n");
		NOMATCH("\n", "a");
		MATCH("\t", "\t");
		NOMATCH("\t", "a");
		MATCH("\\d{3}", "123");
		NOMATCH("\\d{3}", "abc");
		NOMATCH("\\d{3}", "ABC");
		NOMATCH("\\d{3}", "@#$");
		MATCH("\\a{3}", "abc");
		NOMATCH("\\a{3}", "123");
		NOMATCH("\\a{3}", "@#$");
		MATCH("\\u{3}", "ABC");
		NOMATCH("\\u{3}", "abc");
		NOMATCH("\\u{3}", "123");
		MATCH("\\l{3}", "abc");
		NOMATCH("\\l{3}", "ABC");
		NOMATCH("\\l{3}", "123");
		MATCH("\\s{4}", " \r\n\t");
		NOMATCH("\\s{4}", "12ab");
		MATCH("\\w{4}", "0aA_");
		NOMATCH("\\w{4}", "@#$%");
		MATCH("\\x{4}", "09aF");
		NOMATCH("\\x{4}", "123Z");

		/* Match beginning of word */
		MATCH("<abc", "abcdef");
		MATCH("<abc", "def abcdef");
		NOMATCH("<abc", "defabcdef");

		/* Match ending of word */
		MATCH("abc>", "abc def");
		MATCH("abc>", "def abc");
		NOMATCH("abc>", "abcdef");

		/* Match boundaries */
		MATCH("\\babc\\b", "abc");
		MATCH("\\babc\\b", "def abc");
		MATCH("\\babc\\b", "abc def");
		NOMATCH("\\babc\\b", "defabc");
		NOMATCH("\\babc\\b", "abcdef");

		/* Tests for whitespace */
		MATCH("a {4}b", "a    b");
		MATCH("a\t{4}b", "a\t\t\t\tb");
		MATCH("a\\t{4}b", "a\t\t\t\tb");
		NOMATCH("a {4}b", "a   b");
		NOMATCH("a\t{4}b", "a\t\t\tb");
		NOMATCH("a\\t{4}b", "a\t\t\tb");
		MATCH("a\\s{4}b", "a \r\n\tb");

		/* These tests are to check that previously fixed bugs don't reoccur */

		/* There was a bug with how the [abc] was pushed to the stack following
		a '^' anchor, which caused an assertion to fail */
		MATCH("^[abc]", "aef");

		/* These are all special in their own way: */
		MATCH("", "");    /* "" should match everything */
		MATCH("", "abc");
		MATCH("^", "");	  /* "^" should match everything */
		MATCH("^", "abc");
		MATCH("$", "");	  /* "$" should match everything */
		MATCH("$", "abc");
		MATCH("^$", "");	  /* "^$" should match only an empty line */

		/* "^$" should match between the two '\n's in "abc\n\ndef" */
		MATCH("^$", "abc\n\ndef");
		NOMATCH("^$", "abc\ndef"); /* but not here */

		NOMATCH("^$", "abc");
		
		/* An unescaped ']' can be treated as a literal: \[x\] and \[x] should be equivalent. */
		MATCH("^\\[x*\\]$", "[xxxxxxxxxxxx]");
		MATCH("^\\[x*]$", "[xxxxxxxxxxxx]");
		NOMATCH("^\\[x*]$", "[xxxxxxxxxxxx");
		MATCH("]+", "]]]]]]]");
		NOMATCH("]+", "[[[[[[[[[");
		
		/* A '\(' MUST be matched with a '\)';
		It is a bit inconsistent with the behaviour of the unescaped ']' in the
		previous tests.
		*/
		MATCH("^\\(x*\\)$", "(xxxxxxxxxxxx)");
		NOMATCH("^\\(x*\\)$", "(xxxxxxxxxxxx");
		NOMATCH("^\\(x*\\)$", "(xxxxxxxxxxxx");
		MATCH("\\)+", ")))))))");
		NOMATCH("\\)+", "((((((((");

		printf("\n______________\nSuccess: %d/%d\n", success, total);

		if(success != total)
			fprintf(stderr, "Some tests failed!\n");

		return 0;
	}

	/* else compile argv[1] and match argv[2] */

	pat = argv[1];

	if(argc > 2) str = argv[2];

	r = wrx_comp(pat, &e, &ep);
	if(r) {
		printf("\n---------------\n");
		wrx_print_nfa(r);
		printf("---------------\n");

		if(str) {
			/*
			 *	Allocate memory for the submatches
			 */
#if 0		/* This tests that nsm could be less than r->n_subm */
			nsm = r->n_subm / 2;
#else		/* This will allocate enough memory for all submatches */
			nsm = r->n_subm;
#endif
			if(nsm > 0) {
				subm = calloc(sizeof *subm, nsm);
				if(!subm) {
					fprintf(stderr, "Error: out of memory (submatches)");
					wrx_free(r);
					return 1;
				}
			}
			else
				subm = NULL;

			/*
			 *	Match the expression against the string
			 */
			e = wrx_exec(r, str, subm, nsm);
			printf("wrx_exec() returned %d\n", e);

			/* If successful, print all the submatches */
			if(e == 1) {
				printf("Match!\n");
				for(i = 0; i < nsm; i++)
					if(subm[i].beg && subm[i].end) {
						len = subm[i].end - subm[i].beg;
						buf = malloc(len + 1);
						if(!buf) {
							fprintf(stderr, "Error: Out of memory\n");
							return 1;
						}

						strncpy(buf, subm[i].beg, len);
						buf[len] = '\0';
						printf("subm[%d] = \"%s\"\n", i, buf);
						free(buf);
					}
			} else if(e == 0)
				printf("No match\n");
			else
				printf("Error in match: %s\n",  wrx_error(e));

			free(subm);
		}

		/*
		 *	Turn the DOT document into a JPEG like so:
		 *	dot -Tjpg -onfa.jpg nfa.dot
		 */
		wrx_print_dot(r, "nfa.dot");

		wrx_free(r);
	} else {
		fprintf(stderr, "\nError: %d\n%s\n%*c: %s", e, pat, ep, '^', wrx_error(e));
		return 1;
	}

	return 0;
}
