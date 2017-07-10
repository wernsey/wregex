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

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <assert.h>

#include "wregex.h"
#include "wrxcfg.h"

#ifdef DEBUG_OUTPUT
#	include <stdio.h> /* To be removed, along with all the printf()s */
#endif

/* States in the NFA per character in the input pattern */
#define DELTA_STATES 4

/* Internal Structures *******************************************************/

/* The compiler works by breaking the regex into smaller regexes which
 * are converted into small NFAs. These NFA segments are combined as
 * we move upwards through the recursion.
 * This structure tracks those NFA segments' states.
 */
typedef struct {
	short 	beg,	/* The state at which this NFA segment begins */
			end;	/* The state at which this NFA segment ends */
	/* Also, due to the nature of the parser, all states between 'beg' and
	 *	'end' will be part of this NFA segment and its sub-segments
	 */
} nfa_segment;

/*
 *	Internal data used while compiling the NFA
 */
typedef struct {
	wregex_t *nfa; /* The NFA being generated */

	const char *pat,	  /* The pattern being compiled */
		 *p;	  /* The position within the pattern */

	jmp_buf jb;   /* Jump buffer for error handling */

	nfa_segment *seg; 	/* Stack of NFA segments */
	short	  seg_s,	/* Size of the stack */
			  seg_sp;	/* Index of the top of the stack */

	char ci;	/* case insensitive flag */
} comp_data;

#define THROW(x) longjmp(cd->jb, x)

/* Helper Functions (and macros) *********************************************/

/*
 *	Gets and initializes the next available state in the wregex_t
 *	realloc()s the NFA's states if there aren't enough available
 */
static short next_state(comp_data *cd) {
	short i;
	int delta;

	if(cd->nfa->ns + 1 >= cd->nfa->n_states) {
		/* We need more states. Guess the number needed from the
			remaining characters in the input pattern */
		delta = (DELTA_STATES * strlen(cd->p));

		if(cd->nfa->n_states == 0x7FFF) {
			/* Too many states: We use shorts to index them, and this
			will result in overflow */
			THROW(WRX_MANY_STATES);
		}

		if(cd->nfa->ns >= 0x7FFF - delta)
			cd->nfa->n_states = 0x7FFF;
		else
			cd->nfa->n_states += delta;

		cd->nfa->states = realloc(cd->nfa->states,
							cd->nfa->n_states * sizeof(wrx_state));

		if(!cd->nfa->states)
			THROW(WRX_MEMORY);
	}

	i = cd->nfa->ns++;

	/* Initialize the state */
	cd->nfa->states[i].op = 0;
	cd->nfa->states[i].data.c = '\0';
	cd->nfa->states[i].s[0] = -1;
	cd->nfa->states[i].s[1] = -1;
	cd->nfa->states[i].data.bv 	= NULL;

	return i;
}

/*
 *	Pushes a NFA segment on the comp_data's stack
 */
static void push_seg(comp_data *cd, short beg, short end) {
	if(cd->seg_sp + 1 >= cd->seg_s) {
		/* Resize the stack */
		cd->seg_s += 10;
		cd->seg = realloc(cd->seg, sizeof(nfa_segment) * cd->seg_s);
		if(!cd->seg) THROW(WRX_MEMORY);
	}

	cd->seg[cd->seg_sp].beg = beg;
	cd->seg[cd->seg_sp].end = end;
	cd->seg_sp++;
}

/*
 *	Pops a NFA segment from the comp_data's stack
 *	Don't call push_seg() if you're still using the returned pointer
 */
static nfa_segment *pop_seg(comp_data *cd) {
	assert(cd->seg_sp > 0);
	cd->seg_sp--;
	return &cd->seg[cd->seg_sp];
}

/*
 *	Has state s1 transition to s2
 */
static void transition(comp_data *cd , short s1, short s2) {
	if(cd->nfa->states[s1].s[0] < 0) {
		cd->nfa->states[s1].s[0] = s2;
	} else {
		/* This assertion must hold because each NFA state has at most
			2 epsilon transitions */
		assert(cd->nfa->states[s1].s[1] < 0);

		cd->nfa->states[s1].s[1] = s2;
	}
}

/*
 *	Function weaken() makes the '*' and '+' (and '?') operators 
 *	"lazy"/"non-greedy" by swapping s[0] and s[1] of the appropriate state
 */
static void weaken(comp_data *cd, short s) {
	short t;
	assert(s >= 0 && s < cd->nfa->n_states);

	t = cd->nfa->states[s].s[0];
	cd->nfa->states[s].s[0] = cd->nfa->states[s].s[1];
	cd->nfa->states[s].s[1] = t;
}

/*
 * 	Creates a duplicate of a particular state j.
 *	It is used with the "{}" operators to convert, say, "A{3}" to "AAA"
 */
static int duplicate(comp_data *cd, short j) {
	int k;
	char *bv;

	k = next_state(cd);
	cd->nfa->states[k].op = cd->nfa->states[j].op;
	cd->nfa->states[k].s[0] = cd->nfa->states[j].s[0];
	cd->nfa->states[k].s[1] = cd->nfa->states[j].s[1];

	if(cd->nfa->states[j].op == SET) {
		bv = malloc(16);
		if(!bv) THROW(WRX_MEMORY);
		memcpy(bv, cd->nfa->states[j].data.bv, 16);
		cd->nfa->states[k].data.bv = bv;
	} else if(cd->nfa->states[j].op == REC || cd->nfa->states[j].op == STP || cd->nfa->states[j].op == BRF) {
		cd->nfa->states[k].data.idx = cd->nfa->states[j].data.idx;
	} else
		cd->nfa->states[k].data.c = cd->nfa->states[j].data.c;

	return k;
}

/* Creates a bit vector from the set of characters in s, in a similar way
 *	to how the parser handles character sets. It feels a bit clumsy, but
 *	it was the neatest way i could implement the "set" escape characters;
 *	see value() below.
 *	(It asserts input, so it is not intended for user input)
 */
static char *create_bv(char *s) {
	char *bv, u, v, i;

	assert(s && *s);
	if((bv = malloc(16)) == NULL) return NULL;
	memset(bv, 0, 16);

	do {
		u = s[0];
		if(s[1] == '-')
		{
			s += 2;
			assert(s != '\0');
			v = s[0];
		}
		else
			v = u;

		s++;

		assert((u >= START_OF_PRINT && v >= START_OF_PRINT && v >= u) ||
				(u == '\t' || u == '\r' || u == '\n'));

		for(i = u; i <= v; i++)
			BV_SET(bv, i);
	} while(s[0] != '\0');

	return bv;
}

/*
 * Invert the bits in a range bit-vector
 */
static void invert_bv(char *bv) {
	int i;
	/* Note that I leave the first 4 bytes (containing the bits of the
	non-printable characters) as I found them */
	for(i = 4; i < 16; i++)
		bv[i] = ~bv[i];

	/* These three should be handled separately, since they are lower
	than START_OF_PRINT */
	BV_TGL(bv, '\r');
	BV_TGL(bv, '\n');
	BV_TGL(bv, '\t');
}

/* The Parser ****************************************************************/

static void list(comp_data *cd);
static void element(comp_data *cd);
static void value(comp_data *cd);
static char *sets(comp_data *cd);

/*
 *$ pattern	::= ['^'] [list] ['$']
 */
static void pattern(comp_data *cd) {
	short b, e, bol = 0, hl = 0;
	nfa_segment *m1, *m2;

	if(cd->p[0] == '\0') {
#ifdef DEBUG_OUTPUT
		printf("\"\"");
#endif
		/* empty pattern: Match everything */
		b = next_state(cd);
		/* Initialize the states */
		cd->nfa->states[b].op = MEV;
		push_seg(cd, b, b);
		return;
	}

	if(cd->p[0] == '^') {
		bol = 1;

		/* Create a BOL node */
		b = next_state(cd);
		/* Initialize the states */
		cd->nfa->states[b].op = BOL;
		push_seg(cd, b, b);

#ifdef DEBUG_OUTPUT
		printf(" ^");
#endif
		cd->p++;

		if(!cd->p[0]) return;
	}

	if(cd->p[0] != '$') {
		hl = 1;
		list(cd);
	}

	if(bol && hl) {
		/* concatenate the BOL and the list */
		m2 = pop_seg(cd); /* pop NFA 2 */
		m1 = pop_seg(cd); /* pop NFA 1 */

		/* Attach NFA 1's end to NFA 2's beginning */
		transition(cd, m1->end, m2->beg);
		push_seg(cd, m1->beg, m2->end);
	}

	if(cd->p[0] == '$') {
#ifdef DEBUG_OUTPUT
		printf(" $");
#endif
		if(!bol && !hl) {
			/* Special case: pattern = "$", match everything */
			b = next_state(cd);
			/* Initialize the states */
			cd->nfa->states[b].op = MEV;
			push_seg(cd, b, b);
		}

		cd->p++;
		if(cd->p[0] != '\0')
			THROW(WRX_BAD_DOLLAR);

		/* Create a EOL node */
		b = next_state(cd);
		e = next_state(cd);

		/* Initialize the states */
		cd->nfa->states[b].op = EOL;
		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;

		m1 = pop_seg(cd); /* pop NFA 1 */
		transition(cd, m1->end, b);
		push_seg(cd, m1->beg, e);
	}

	/* Add the REC and STP states for submatch 0,
	which captures the entire matching part of the string */
	m1 = pop_seg(cd); /* pop the NFA */

	b = next_state(cd);
	e = next_state(cd);

	cd->nfa->states[b].op = REC;
	cd->nfa->states[b].data.idx = 0;
	cd->nfa->states[e].op = STP;
	cd->nfa->states[e].data.idx = 0;

	transition(cd, b, m1->beg);
	transition(cd, m1->end, e);

	push_seg(cd, b, e);
}

/*
 *$	list	::= element ["|" list]
 */
static void list(comp_data *cd) {
	short b, e, n1, n2;
	nfa_segment *m;

	element(cd);

	if(cd->p[0] == '|') {
		cd->p++;
#ifdef DEBUG_OUTPUT
		printf(" |");
#endif

		m = pop_seg(cd); /* pop NFA 1 */
		b = m->beg;
		e = m->end;

		list(cd);	/* Compile the second NFA */
		m = pop_seg(cd); /* pop NFA 2 */

		n1 = next_state(cd);
		n2 = next_state(cd);

		cd->nfa->states[n1].op = CHC;
		cd->nfa->states[n2].op = MOV;
		transition(cd, n1, b);
		transition(cd, n1, m->beg);
		transition(cd, e, n2);
		transition(cd, m->end, n2);
		push_seg(cd, n1, n2);
	}
}

/*
 *$	element	::= ("(" [":"] list ")" | value) [(("*"|"+"|"?")["?"])|("{" [digit+] ["," [digit+]] "}" ["?"])] [element]
 */
static void element(comp_data *cd) {
	short b, e;
	char bref;
	nfa_segment *m;

	int boc, eoc, cf; /* begin/end of counting: "{boc,eoc}" */
	short i, j, k,
		ofs, sub1, sub2, sub3;

	sub1 = cd->nfa->ns;

	if(cd->p[0] == '$') return;

	if(cd->p[0] == '(') {
		if(cd->p[1] == ':') {
			/* parenthesis used only for grouping */
			bref = -1;
			cd->p += 2;
#ifdef DEBUG_OUTPUT
			printf(" (:");
#endif
		} else {
			/* parenthesis indicates a submatch capture */
			bref = cd->nfa->n_subm++;
			cd->p++;
#ifdef DEBUG_OUTPUT
			printf(" (");
#endif
		}

		list(cd);
		if(cd->p[0] != ')') THROW(WRX_BRACKET);

		if(bref >= 0) {
			/* back reference: */
			m = pop_seg(cd);	/* Get the NFA within the parens */

			/* Create a recording state */
			b = next_state(cd);
			cd->nfa->states[b].op = REC;
			cd->nfa->states[b].data.idx = bref;
			transition(cd, b, m->beg);

			/* Create a state for stopping the recording */
			e = next_state(cd);
			cd->nfa->states[e].op = STP;
			cd->nfa->states[e].data.idx = bref;
			transition(cd, m->end, e);

			push_seg(cd, b, e);
		}

		cd->p++;
#ifdef DEBUG_OUTPUT
		printf(" )");
#endif
	} else {
		value(cd);
	}

	if(cd->p[0] == '$') return;

	if(cd->p[0] && strchr("*+?", cd->p[0])) {
		m = pop_seg(cd); /* Get the preceding NFA */

		b = next_state(cd);
		e = next_state(cd);

		cd->nfa->states[b].op = CHC;
		cd->nfa->states[e].op = MOV;

		transition(cd, b, m->beg);
		transition(cd, b, e);

		/* The actual differences between the operators are very subtle */
		switch(cd->p[0]) {
			case '*':
				transition(cd, m->end, b);
				push_seg(cd, b, e);
				break;
			case '+':
				transition(cd, m->end, b);
				push_seg(cd, m->beg, e);
				break;
			case '?':
				transition(cd, m->end, e);
				push_seg(cd, b, e);
				break;
		}

#ifdef DEBUG_OUTPUT
		printf(" %c", cd->p[0]);
#endif
		cd->p++;

		/* Lazy evaluation? */
		if(cd->p[0] == '?') {
			cd->p++;
			weaken(cd, b);
#ifdef DEBUG_OUTPUT
			printf(" ?");
#endif
		}
	} else if(cd->p[0] == '{') {
		boc = 0;
		eoc = 0;
		cf = 0;

		cd->p++;
		if(isdigit(cd->p[0])) cf = 1;

		while(isdigit(cd->p[0])) {
			boc = boc * 10 + (cd->p[0] - '0');
			cd->p++;
		}

		if(cd->p[0] == ',') {
			cf |= 2;
			cd->p++;
			if(isdigit(cd->p[0])) cf |= 4;
			while(isdigit(cd->p[0])) {
				eoc = eoc * 10 + (cd->p[0] - '0');
				cd->p++;
			}
		}

		if(cd->p[0] != '}')
			THROW(WRX_CURLYB);

		cd->p++;

		/* So now these values for cf:
		 *	0: {} - treated as '*'
		 *	1: {x} - exactly x
		 *	2: {,} - treated the same as '*'
		 *	3: {x,} - at least x - equivalent to {x,inf}
		 *	4: can't happen
		 *	5: can't happen
		 *	6: {,y} - at most y - equivalent to {0,y}
		 *	7: {x,y} - between x and y
		 */
		assert(cf != 4 && cf != 5);

		/* Treat {x,x} the same as {x} */
		if(cf == 7 && boc == eoc) cf = 1;

		switch(cf)
		{
		case 0:
		case 2: { /* {} or {,} - treat it as we would a '*' */

				m = pop_seg(cd); /* Get the preceding NFA */

				b = next_state(cd);
				e = next_state(cd);

				cd->nfa->states[b].op = CHC;
				cd->nfa->states[e].op = MOV;

				transition(cd, b, m->beg);
				transition(cd, b, e);
				transition(cd, m->end, b);
				push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
				printf(" {}");
#endif

				if(cd->p[0] == '?') { /* weaken? */
					cd->p++;
					weaken(cd, b);
#ifdef DEBUG_OUTPUT
					printf(" ?");
#endif
				}

			} break;
		case 1: { /* {boc} */
			sub2 = cd->nfa->ns;
			m = pop_seg(cd);

			ofs = sub2 - sub1;	/* offset: Number of states that will be added */
			b = m->beg + ofs; 	/* beginning of next NFA segment */
			e = m->end; 		/* end of current NFA segment */

#ifdef DEBUG_OUTPUT
			printf(" {%d}", boc);
#endif

			/* Duplicate the states between sub1 and sub2 boc times */
			for(i = 1; i < boc; i++) {
				/* Create duplicates of all the states between sub1 and sub2 */
				for(j = sub1; j < sub2; j++) {
					/* Create a duplicate of the current state */
					k = duplicate(cd, j);

					/* but alter the state transitions */
					if(cd->nfa->states[k].s[0] >= 0)
						cd->nfa->states[k].s[0] += ofs;

					if(cd->nfa->states[k].s[1] >= 0)
						cd->nfa->states[k].s[1] += ofs;
				}

				/* link the previous NFA segment to the new one */
				cd->nfa->states[e].s[0] = b;

				/* adjust our parameters by the offset */
				b += ofs;
				e += ofs;
				sub1 += ofs;
				sub2 += ofs;
			}

			/* Weaken operator has no meaning here */
			if(cd->p[0] == '?') cd->p++;

			push_seg(cd, m->beg, e);

		} break;
		case 3: { /* {boc,} - at least boc */
			sub2 = cd->nfa->ns;
			m = pop_seg(cd);

			ofs = sub2 - sub1;	/* offset: Number of states that will be added */
			b = m->beg + ofs; /* beginning of next NFA segment */
			e = m->end; /* end of current NFA segment */

#ifdef DEBUG_OUTPUT
			printf(" {%d,}", boc);
#endif

			/* Duplicate the states between sub1 and sub2 boc times */
			for(i = 1; i < boc; i++) {
				/* Create duplicates of all the states between sub1 and sub2 */
				for(j = sub1; j < sub2; j++) {
					/* Create a duplicate of the current state */
					k = duplicate(cd, j);

					/* but alter the state transitions */
					if(cd->nfa->states[k].s[0] >= 0)
						cd->nfa->states[k].s[0] += ofs;

					if(cd->nfa->states[k].s[1] >= 0)
						cd->nfa->states[k].s[1] += ofs;
				}

				/* link the previous NFA segment to the new one */
				cd->nfa->states[e].s[0] = b;

				/* adjust our parameters by the offset */
				b += ofs;
				e += ofs;
				sub1 += ofs;
				sub2 += ofs;
			}

			/*
			 *	We treat "a{3,}" the same as "aaa+"
			 *	so at this stage the "aaa" part is set up, so now we just need
			 *	to do the "+" part:
			 */

			b -= ofs;
			i = next_state(cd);
			j = next_state(cd);
			cd->nfa->states[i].op = CHC;
			cd->nfa->states[j].op = MOV;
			transition(cd, i, b);
			transition(cd, i, j);
			transition(cd, e, i);

			/* weaken? */
			if(cd->p[0] == '?') {
				cd->p++;
				weaken(cd, i);
#ifdef DEBUG_OUTPUT
				printf(" ?");
#endif
			}

			push_seg(cd, m->beg, j);
		} break;
		case 6: { /* {,eoc} - at most eoc */
			/* we treat "A{,3}" as "A?A?A?" */

			m = pop_seg(cd); /* Get the preceding NFA A */

			/* Create the equivalent to A? */
			b = next_state(cd);
			e = next_state(cd);
			cd->nfa->states[b].op = CHC;
			cd->nfa->states[e].op = MOV;
			transition(cd, b, m->beg);
			transition(cd, b, e);
			transition(cd, m->end, e);

#ifdef DEBUG_OUTPUT
			printf(" {,%d}", eoc);
#endif
			/* weaken? */
			if(cd->p[0] == '?') {
				/* Weaken is not really useful here because
					it's like saying ?? */
				cd->p++;
				weaken(cd, b);
#ifdef DEBUG_OUTPUT
				printf(" ?");
#endif
			}

			/* Good! Now create A?A?A?... */
			sub2 = cd->nfa->ns;

			m->beg = b;
			m->end = e;
			ofs = sub2 - sub1;	/* offset: Number of states that will be added */
			b += ofs; 			/* beginning of next NFA segment */

			/* Duplicate the states between sub1 and sub2 boc times */
			for(i = 1; i < eoc; i++) {
				/* Create duplicates of all the states between sub1 and sub2 */
				for(j = sub1; j < sub2; j++) {
					/* Create a duplicate of the current state */
					k = duplicate(cd, j);

					/* but alter the state transitions */
					if(cd->nfa->states[k].s[0] >= 0)
						cd->nfa->states[k].s[0] += ofs;

					if(cd->nfa->states[k].s[1] >= 0)
						cd->nfa->states[k].s[1] += ofs;
				}

				/* link the previous NFA segment to the new one */
				cd->nfa->states[e].s[0] = b;

				/* adjust our parameters by the offset */
				b += ofs;
				e += ofs;
				sub1 += ofs;
				sub2 += ofs;
			}

			push_seg(cd, m->beg, e);

		} break;
		case 7: /* {boc,eoc} - between boc and eoc (inclusive) */
		{
			if(boc > eoc) THROW(WRX_BAD_CURLYB);

			/* I'd like to evaluate "A{2,5}" as "AAA?A?A?" */

			sub2 = cd->nfa->ns;
			m = pop_seg(cd);

			ofs = sub2 - sub1;	/* offset: Number of states that will be added */
			b = m->beg + ofs; /* beginning of next NFA segment */
			e = m->end; /* end of current NFA segment */

#ifdef DEBUG_OUTPUT
			printf(" {%d,%d}", boc, eoc);
#endif

			/* Duplicate the states between sub1 and sub2 boc times */
			for(i = 1; i < boc; i++) {
				/* Create duplicates of all the states between sub1 and sub2 */
				for(j = sub1; j < sub2; j++) {
					/* Create a duplicate of the current state */
					k = duplicate(cd, j);

					/* but alter the state transitions */
					if(cd->nfa->states[k].s[0] >= 0)
						cd->nfa->states[k].s[0] += ofs;

					if(cd->nfa->states[k].s[1] >= 0)
						cd->nfa->states[k].s[1] += ofs;
				}

				/* link the previous NFA segment to the new one */
				cd->nfa->states[e].s[0] = b;

				/* adjust our parameters by the offset */
				b += ofs;
				e += ofs;
				sub1 += ofs;
				sub2 += ofs;
			}

			/* At this stage we have "AA", so now we want too start adding "A?"'s */

			sub3 = cd->nfa->ns; /* Remember the current state, useful later */

			/* Create a new NFA segment identical to "A" */

			/* Create duplicates of all the states between sub1 and sub2 */
			for(j = sub1; j < sub2; j++) {
				/* Create a duplicate of the current state */
				k = duplicate(cd, j);

				/* but alter the state transitions */
				if(cd->nfa->states[k].s[0] >= 0)
					cd->nfa->states[k].s[0] += ofs;

				if(cd->nfa->states[k].s[1] >= 0)
					cd->nfa->states[k].s[1] += ofs;
			}

			/* Convert our duplicate of "A" into a "A?" */
			i = next_state(cd);
			j = next_state(cd);

			cd->nfa->states[i].op = CHC;
			cd->nfa->states[j].op = MOV;

			cd->nfa->states[e].s[0] = i;
			transition(cd, i, b);
			transition(cd, i, j);
			e += ofs;
			transition(cd, e, j);

			if(cd->p[0] == '?') /* weaken? */
				weaken(cd, i);

			/* At this stage we have "AAA?" (only one "A?") */

			/* Recall where our first "A?" lies */
			sub1 = sub3;
			sub2 = cd->nfa->ns;
			ofs = sub2 - sub1;
			b = i;
			e = j;

			/* Duplicate the "A?" states between sub1 and sub2 (eoc - boc - 1) times */
			for(i = boc; i < eoc - 1; i++) {
				/* Create duplicates of all the states between sub1 and sub2 */
				for(j = sub1; j < sub2; j++) {
					/* Create a duplicate of the current state */
					k = duplicate(cd, j);

					/* but alter the state transitions */
					if(cd->nfa->states[k].s[0] >= 0)
						cd->nfa->states[k].s[0] += ofs;

					if(cd->nfa->states[k].s[1] >= 0)
						cd->nfa->states[k].s[1] += ofs;
				}

				/* adjust our parameters by the offset */
				b += ofs;
				/* link the previous NFA segment to the new one */
				cd->nfa->states[e].s[0] = b;

				e += ofs;
				sub1 += ofs;
				sub2 += ofs;
			}

			push_seg(cd, m->beg, e);

			 /* weaken? (we've already taken care of it) */
			if(cd->p[0] == '?') {
				cd->p++;
#ifdef DEBUG_OUTPUT
				printf(" ?");
#endif
			}

		} break;
		} /* switch cf */
	}

	if(cd->p[0] && cd->p[0] != '|' && cd->p[0] != ')' && cd->p[0] != '$') {
		m = pop_seg(cd); /* pop NFA 1 */
		b = m->beg;
		e = m->end;
		element(cd); /* compile NFA 2 */
		m = pop_seg(cd); /* pop NFA 2 */

		/* Attach NFA 1's end to NFA 2's beginning */
		transition(cd, e, m->beg);
		push_seg(cd, b, m->end);
	}
}

/*
 *$ value	::= (A-Za-z0-9!"#%&',-/:;=@\\_`~\r\t\n) | '<' | '>' | "[" ["^"] sets "]" | "." | '\i' list | '\I' list | 'escape sequence'
 */
static void value(comp_data *cd) {
	short b, e, inv = 0, i;

	if(isalnum(cd->p[0]) || cd->p[0] == ' ') {
		b = next_state(cd);
		e = next_state(cd);

		/* Initialize the states */
		cd->nfa->states[b].op = cd->ci?MCI:MTC;
		cd->nfa->states[b].data.c = cd->p[0];
		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;
		push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
		printf(" '%c'", cd->p[0]);
#endif
		cd->p++;
	} else if(cd->p[0] == '[') {
		cd->p++;

		b = next_state(cd);
		e = next_state(cd);

#ifdef DEBUG_OUTPUT
		printf(" [");
#endif

		/* Invert the set? */
		if(cd->p[0] == '^') {
#ifdef DEBUG_OUTPUT
			printf(" ^");
#endif
			cd->p++;
			inv = 1;
		}

		/* Initialize the states */
		cd->nfa->states[b].op = SET;

		/* Compile the sets */
		cd->nfa->states[b].data.bv = sets(cd);

		if(inv) /* invert the range */
			invert_bv(cd->nfa->states[b].data.bv);

		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;
		push_seg(cd, b, e);

		if(cd->p[0] == ']')
			cd->p++;
		else
			THROW(WRX_ANGLEB);

#ifdef DEBUG_OUTPUT
		printf(" ]");
#endif
	} else if(cd->p[0] == '.') {
		b = next_state(cd);
		e = next_state(cd);

		/* Initialize the states */
		cd->nfa->states[b].op = SET;

		cd->nfa->states[b].data.bv = malloc(16);
		if(!cd->nfa->states[b].data.bv) THROW(WRX_MEMORY);
		for(i = 0; i < 16; i++)
			cd->nfa->states[b].data.bv[i] = (i < 4)? 0: 0xFF;

		BV_SET(cd->nfa->states[b].data.bv, '\r');
		BV_SET(cd->nfa->states[b].data.bv, '\n');
		BV_SET(cd->nfa->states[b].data.bv, '\t');

		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;
		push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
		printf(" '%c'", cd->p[0]);
#endif
		cd->p++;
	} else if(cd->p[0] == '<') {
		b = next_state(cd);
		e = next_state(cd);

		/* Initialize the states */
		cd->nfa->states[b].op = BOW;
		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;
		push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
		printf(" <");
#endif
		cd->p++;
	} else if(cd->p[0] == '>') {
		b = next_state(cd);
		e = next_state(cd);

		/* Initialize the states */
		cd->nfa->states[b].op = EOW;
		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;
		push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
		printf(" >");
#endif
		cd->p++;
	} else if(cd->p[0] == '$') {
		return;
	} else if(cd->p[0] == ESC) {
		/* 'escape sequence' */
#ifdef DEBUG_OUTPUT
		printf(" %c", cd->p[0]);
#endif
		cd->p++;

		if(!cd->p[0])
			THROW(WRX_ESCAPE);
		if(cd->p[0] == 'i' || cd->p[0] == 'I') {
#ifdef DEBUG_OUTPUT
			printf(" '%c%c'", ESC, cd->p[0]);
#endif
			cd->ci = (cd->p[0] == 'i');
			cd->p++;
			if(cd->p[0] && cd->p[0] != '$') {
				list(cd);
			} else {
				/* Push a state that does nothing,
					(otherwise higher level segments gets messed up) */
				b = next_state(cd);
				cd->nfa->states[b].op = MOV;
				push_seg(cd, b, b);
			}
		} else if(strchr("daulswx", tolower(cd->p[0]))) {
			/* Escape sequence for a set of characters */
			b = next_state(cd);
			e = next_state(cd);

			/* Initialize the states */
			cd->nfa->states[b].op = SET;

			/* select the specific characters in this set */
			switch(tolower(cd->p[0])) {
				case 'd': cd->nfa->states[b].data.bv = create_bv("0-9"); break;
				case 'a': cd->nfa->states[b].data.bv = create_bv("a-zA-Z"); break;
				case 'u': {
					if(cd->ci) /* '\u' has no case insensitive meaning */
						cd->nfa->states[b].data.bv = create_bv("a-zA-Z");
					else
						cd->nfa->states[b].data.bv = create_bv("A-Z");
				} break;
				case 'l': {
					if(cd->ci) /* '\l' has no case insensitive meaning */
						cd->nfa->states[b].data.bv = create_bv("a-zA-Z");
					else
						cd->nfa->states[b].data.bv = create_bv("a-z");
				} break;
				case 's': cd->nfa->states[b].data.bv = create_bv(" \t\r\n"); break;
				case 'w': cd->nfa->states[b].data.bv = create_bv("0-9a-zA-Z_"); break;
				case 'x': cd->nfa->states[b].data.bv = create_bv("a-fA-F0-9"); break;
			}

			/* create the bit-vector representing the character set */
			if(!cd->nfa->states[b].data.bv) THROW(WRX_MEMORY);

			/* If the escaped character is actually uppercase, we invert the
				character set */
			if(isupper(cd->p[0]))
				invert_bv(cd->nfa->states[b].data.bv);

			transition(cd, b, e);
			cd->nfa->states[e].op = MOV;
			push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
			printf("%c", cd->p[0]);
#endif
			cd->p++;
		} else if(strchr("rntb", cd->p[0])) {
			b = next_state(cd);
			e = next_state(cd);

			/* Initialize the states */

			switch(cd->p[0])
			{
			case 'n' : {
				cd->nfa->states[b].op = MTC;
				cd->nfa->states[b].data.c = '\n';
			} break;
			case 'r' : {
				cd->nfa->states[b].op = MTC;
				cd->nfa->states[b].data.c = '\r';
			} break;
			case 't' : {
				cd->nfa->states[b].op = MTC;
				cd->nfa->states[b].data.c = '\t';
			} break;
			case 'b' : cd->nfa->states[b].op = BND; break;
			}

			transition(cd, b, e);
			cd->nfa->states[e].op = MOV;
			push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
			printf(" '%c'", cd->p[0]);
#endif
			cd->p++;
		} else if(strchr(".*+?[](){}|^$<>:", cd->p[0]) || cd->p[0] == ESC) {
			/* Escape of control characters */
			b = next_state(cd);
			e = next_state(cd);

			/* Initialize the states */
			cd->nfa->states[b].op = MTC;
			cd->nfa->states[b].data.c = cd->p[0];
			transition(cd, b, e);
			cd->nfa->states[e].op = MOV;
			push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
			printf("%c", cd->p[0]);
#endif
			cd->p++;
		} else if(isdigit(cd->p[0])) {
			/* Back reference */
			i = 0;
			while (isdigit(cd->p[0])) {
				i = i * 10 + (cd->p[0] - '0');
				cd->p++;
			}

			b = next_state(cd);
			e = next_state(cd);

			/* Initialize the states */
			if(cd->ci)
				cd->nfa->states[b].op = BRI; /* case insensitive backref */
			else
				cd->nfa->states[b].op = BRF; /* case insensitive backref */

			cd->nfa->states[b].data.idx = i;
			transition(cd, b, e);
			cd->nfa->states[e].op = MOV;
			push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
			printf("%c", cd->p[0]);
#endif
		} else {
			cd->p++;
			THROW(WRX_ESCAPE);
		}
	} else if(cd->p[0] && cd->p[0] != ESC && cd->p[0] != ')' && (isgraph(cd->p[0]) || isspace(cd->p[0]))) {
		/* non-alnum characters that don't need to be escaped
		 * (note that I've included '\\' above because the escape character is
		 * reconfigurable in wrxcfg.h, hence the "cd->p[0] != ESC")
		 *
		 * Note also that the '^' and the ':' can be used in escaped or
		 * unescaped form (because of their limited use as special characters)
		 */
		b = next_state(cd);
		e = next_state(cd);

		/* Initialize the states */
		cd->nfa->states[b].op = MTC;
		cd->nfa->states[b].data.c = cd->p[0];
		transition(cd, b, e);
		cd->nfa->states[e].op = MOV;
		push_seg(cd, b, e);

#ifdef DEBUG_OUTPUT
		printf(" '%c'", cd->p[0]);
#endif
		cd->p++;
	} else {
#if 0
		THROW(WRX_VALUE);
#else
		/* This allows statements such as "(a|)", but causes other problems */
		b = next_state(cd);
		cd->nfa->states[b].op = MOV;
		push_seg(cd, b, b);
#endif
	}
}

/*
 *$ sets 	::= (c ["-" c])+
 *$		where c is a printable ASCII character (>= 0x20)
 */
static char *sets(comp_data *cd) {
	char *bv, u, v;
	int i;

	bv = malloc(16);
	if(!bv) THROW(WRX_MEMORY);

	for(i = 0; i < 16; i++)
		bv[i] = 0;

	do {
		if(cd->p[0] == '\0') THROW(WRX_ANGLEB);

		u = cd->p[0];

		if(u == ESC) {
#ifdef DEBUG_OUTPUT
			printf(" '\\%c'", cd->p[1]);
#endif
			switch(cd->p[1])
			{
			case 'r': {
				BV_SET(bv, '\r');
			} break;
			case 'n': {
				BV_SET(bv, '\n');
			} break;
			case 't': {
				BV_SET(bv, '\t');
			} break;
			case ESC:
			case '-':
			case '^':
			case ']': {
				BV_SET(bv, cd->p[1]);
			} break;
			case 'd': {
				for(i = '0'; i <= '9'; i++)
					BV_SET(bv, i);
			} break;
			case 'a': {
				for(i = 'a'; i <= 'z'; i++)
					BV_SET(bv, i);
				for(i = 'A'; i <= 'Z'; i++)
					BV_SET(bv, i);
			} break;
			case 'u': {
				for(i = 'A'; i <= 'Z'; i++)
					BV_SET(bv, i);

				if(cd->ci)
				{
					/* case insensitive */
					for(i = 'a'; i <= 'z'; i++)
						BV_SET(bv, i);
				}
			} break;
			case 'l': {
				for(i = 'a'; i <= 'z'; i++)
					BV_SET(bv, i);

				if(cd->ci)
				{
					/* case insensitive */
					for(i = 'A'; i <= 'Z'; i++)
						BV_SET(bv, i);
				}
			} break;
			case 's': {
				BV_SET(bv, ' ');
				BV_SET(bv, '\t');
				BV_SET(bv, '\r');
				BV_SET(bv, '\n');
			} break;
			case 'w': {
				for(i = 'a'; i <= 'z'; i++)
					BV_SET(bv, i);
				for(i = 'A'; i <= 'Z'; i++)
					BV_SET(bv, i);
				for(i = '0'; i <= '9'; i++)
					BV_SET(bv, i);
				BV_SET(bv, '_');
			} break;
			case 'x': {
				for(i = 'a'; i <= 'f'; i++)
					BV_SET(bv, i);
				for(i = 'A'; i <= 'F'; i++)
					BV_SET(bv, i);
				for(i = '0'; i <= '9'; i++)
					BV_SET(bv, i);
			} break;
			}
			cd->p += 2;
		} else {
			if(cd->p[1] == '-')
			{
				cd->p += 2;
				if(cd->p[0] == '\0') THROW(WRX_SET);
				v = cd->p[0];

				/* On second thought, these restrictions may be unnecessary */
				if(!isalnum(u) || !isalnum(v))
					THROW(WRX_RNG_BADCHAR);
				else if(isupper(u) && !isupper(v)) /* [A-a] is invalid */
					THROW(WRX_RNG_MISMATCH);
				else if(islower(u) && !islower(v)) /* [a-A] is invalid */
					THROW(WRX_RNG_MISMATCH);
				else if(isdigit(u) && !isdigit(v)) /* [0-a] is invalid */
					THROW(WRX_RNG_MISMATCH);
			} else {
				v = u;
			}

			cd->p++;

			if(u < START_OF_PRINT && u != '\r' && u != '\n' && u != '\t')
				THROW(WRX_SET);

			if(v < START_OF_PRINT && v != '\r' && v != '\n' && v != '\t')
				THROW(WRX_SET);

			if(v < u)
				THROW(WRX_RNG_ORDER);

#ifdef DEBUG_OUTPUT
			if (u != v)
				printf(" '%c'-'%c'", u, v);
			else
				printf(" '%c'", u);
#endif
			if(cd->ci) {
				/* case insensitive */
				for(i = u; i <= v; i++) {
					BV_SET(bv, toupper(i));
					BV_SET(bv, tolower(i));
				}
			} else {
				for(i = u; i <= v; i++)
					BV_SET(bv, i);
			}
		}
	}
	while(cd->p[0] != ']');

	return bv;
}

/*****************************************************************************/

#ifdef OPTIMIZE
/*
 *	Optimizes the NFA slightly by circumventing all states marked MOV
 */
static void optimize(wregex_t *nfa) {
	short i;
	for(i = 0; i < nfa->ns; i++) {
		while(nfa->states[nfa->states[i].s[0]].op == MOV)
			nfa->states[i].s[0] = nfa->states[nfa->states[i].s[0]].s[0];

		while(nfa->states[nfa->states[i].s[1]].op == MOV)
			nfa->states[i].s[1] = nfa->states[nfa->states[i].s[1]].s[0];
	}

	while(nfa->states[nfa->start].op == MOV)
		nfa->start = nfa->states[nfa->start].s[0];
}
#endif

/*
 *	NFA Compiler. It initializes the wregex_t, and wraps around the
 *	parser functions above
 */
wregex_t *wrx_comp(const char *p, int *e, int *ep) {
	comp_data cd;
	int ex;
	short es;
	nfa_segment *m;

	cd.nfa = NULL;

	/* The exception handling: */
	if((ex = setjmp(cd.jb)) != 0) {
		if(cd.nfa) wrx_free(cd.nfa);
		if(e) *e = ex;
		if(ep) *ep = cd.p - cd.pat;
		if(cd.seg) free(cd.seg);
		return NULL;
	}

	/* Initialize the comp_data */
	cd.pat = p;
	cd.p = cd.pat;

	/* We're case sensitive by default */
	cd.ci = 0;

	cd.nfa = malloc(sizeof(wregex_t));
	if(!cd.nfa) longjmp(cd.jb, WRX_MEMORY);

	cd.nfa->states = NULL;

	/* Store a copy of the pattern (I have a good reason for this) */
	cd.nfa->p = strdup(p);
	if(!cd.nfa->p) longjmp(cd.jb, WRX_MEMORY);

	cd.nfa->n_states = (DELTA_STATES * (strlen(cd.p) + 1));
	/* The +1 ensures that we can handle at least 0 length strings (BUGFIX) */
	cd.nfa->states = malloc(cd.nfa->n_states * sizeof(wrx_state));
	if(!cd.nfa->states) longjmp(cd.jb, WRX_MEMORY);

	cd.nfa->ns = 0;
	cd.nfa->n_subm = 1; /* submatch[0] is special */

	/* The stack for NFA segments */
	cd.seg_s = 10;
	cd.seg_sp = 0;
	cd.seg = malloc(sizeof(nfa_segment) * cd.seg_s);
	if(!cd.seg) longjmp(cd.jb, WRX_MEMORY);

	/* Now we can start compiling */
	pattern(&cd);

	if(cd.p[0] != '\0') longjmp(cd.jb, WRX_INVALID);

	m = pop_seg(&cd);

	/* create a final end-om-match state */
	es = next_state(&cd);
	cd.nfa->states[es].op = EOM;
	transition(&cd, m->end, es);

	cd.nfa->start = m->beg;
	cd.nfa->stop = es;

#ifdef OPTIMIZE
	optimize(cd.nfa); /* Get rid of the MOV instructions */
#endif

	/* Done! Clean up and return success */
	if(cd.seg) free(cd.seg);
	if(e) *e = WRX_SUCCESS;
	return cd.nfa;
}
