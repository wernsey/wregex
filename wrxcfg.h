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

/* Opcodes in the NFA. Values for wregex_t_state::op  */
typedef enum {
	MTC,	/* Match character */
	MCI,	/* Match character, case insensitive */
	CHC,	/* "CHOICE" used for the '|', '*', '+' and '?' operators */
	MOV,	/* Dummy */
	EOM,	/* End-of-Match */
	SET,	/* Character set */
	REC,	/* Record a submatch */
	STP,	/* Stop recording the submatch */
	BRF,	/* Back reference (case sensitive) */
	BRI,	/* Back reference (case-insensitive) - FIXME */
	BOL,	/* Beginning of line '^' */
	EOL,	/* End of line '$' */
	BOW,	/* Beginning of word '<' */
	EOW,	/* End of word '>' */
	BND,	/* Boundry "\b", like '<' and '>' combined */
	MEV		/* Match everything (causes wrx to always return true) */
} opcode;


#define WRX_MATCH			1
#define WRX_NOMATCH			0

/* Error codes used internally */
#define WRX_SUCCESS			0
#define WRX_MEMORY			-1	/* malloc() failed */
#define WRX_VALUE			-2	/* value expected */
#define WRX_BRACKET			-3	/* ')' expected */
#define WRX_INVALID			-4	/* General invalid expression */
#define WRX_ANGLEB			-5  /* ']' expected */
#define WRX_SET				-6	/* Error in range [] */
#define WRX_RNG_ORDER		-7  /* v < u in the range [u-v] */
#define WRX_RNG_BADCHAR		-8	/* non-alphanumeric char in range [u-v] */
#define WRX_RNG_MISMATCH	-9	/* mismatch in range [u-v] */
#define WRX_ESCAPE			-10	/* Invalid escape sequence */
#define WRX_BAD_DOLLAR		-11	/* '$' not at end of pattern */
#define WRX_CURLYB			-12	/* '}' expected */
#define WRX_BAD_CURLYB		-13	/* m > n in {m,n} */
#define WRX_BAD_NFA			-14	/* NFA invalid */
#define WRX_SMALL_NSM		-15	/* nsm parameter passed to wrx_exec is too small */
#define WRX_INV_BREF		-16	/* Invalid backreference */
#define WRX_MANY_STATES		-17	/* Too many states */
#define WRX_STACK			-18	/* Can't grow stack any further */
#define WRX_OPCODE			-19 /* Unknown opcode */

/* Start of printable characters */
#define START_OF_PRINT 0x20

/* Escape character.
 * '\' for now, but I can think of at least one application
 * where I'd rather use something else, like a '%' or a '/'
 */
#define ESC		'\\'

/*
 *	Macros for manipulating bit-vectors (char[16])
 *	The bit vectors are used for character sets in the [...] in the regexes
 */
/* Sets the bit in bv corresponding to c */
#define BV_SET(bv, c) bv[c>>3] |= 1 << (c & 0x07)

/* Clears the bit in bv corresponding to c */
#define BV_CLR(bv, c) bv[c>>3] &= ~(1 << (c & 0x07))

/* Toggles the bit in bv corresponding to c */
#define BV_TGL(bv, c) bv[c>>3] ^= 1 << (c & 0x07)

/* Tests the bit in bv corresponding to c */
#define BV_TST(bv, c) (bv[c>>3] & 1 << (c & 0x07))

/* Enable my small type of optimization: Remove all nodes marked MOV,
since they're redundant (but useful for debugging) */
#define OPTIMIZE

/* Define DEBUG_OUTPUT to print details about the internals of the
system to stdout */
#if !defined(NDEBUG) && 0
#define DEBUG_OUTPUT
#endif