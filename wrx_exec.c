/*
 * Copyright (c) 2007 Werner Stoop
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
#include <ctype.h>
#include <string.h>
#include <setjmp.h>

#include <assert.h>

#include "wregex.h"
#include "wrxcfg.h"

#ifdef DEBUG_OUTPUT
#	include <stdio.h>
#endif

typedef enum {
	op_pos,
	op_rbeg,
	op_rend
} stack_op;

/* Element on the stack */
typedef struct
{
	/* Operation associated with this stack element */
	stack_op op;

	/* Operand */
	const char *opr;

	/* State or index */
	short st;
} stack_el;

typedef struct
{
	stack_el *els;	/* Elements on the stack */
	short ns;	 	/* Number of elements on the stack */
	short ts;		/* Top of stack */
} stack;

/*
 *	Returns a stack, whcih is an array of stack_els.
 *	In theory the number of elements required should not exceed the number
 *	of states
 */
static stack *stack_create(int ns)
{
	stack *st;
	st = malloc(sizeof *st);
	if(!st) return NULL;

	st->els = calloc(ns, sizeof *st->els);
	if(!st->els)
	{
		free(st);
		return NULL;
	}

	st->ns = ns;
	st->ts = 0;

	return st;
}

/*
 *	Frees memory allocated to a stack
 */
static void free_stack(stack *stk)
{
	assert(stk);

	free(stk->els);
	free(stk);
}

/*
 *	Pushes an operation on a stack
 */
static int push(stack *stk, stack_op op, const char *opr, short state)
{
	if(stk->ts + 1 >= stk->ns)
	{
		/* Stack overflow: Double its size */
		if(stk->ns < 0x3FFF)
			stk->ns <<= 1;
		else if(stk->ns < 0x7FFF)
			stk->ns = 0x7FFF;
		else
			return -1;

		stk->els = realloc(stk->els, (sizeof *stk->els) * stk->ns);
		if(!stk->els)
			return 0;
	}

	stk->els[stk->ts].op = op;
	stk->els[stk->ts].opr = opr;
	stk->els[stk->ts].st = state;

	stk->ts++;

	return 1;
}

/*
 *	Pops an operation from a stack
 */
static stack_el* pop(stack *stk)
{
	if(--stk->ts < 0) return NULL;
	return &stk->els[stk->ts];
}

/*
 * Matches the string str to the NFA nfa, and stores the submatches in subm[]
 */
int wrx_exec(const wrx_nfa *nfa, const char *str, wrx_subm subm[], int nsm)
{
	short st; 			/* current state */
	wrx_nfa_state *sp;	/* state pointer */

	stack *stk;			/* The stack used for backtracking */
	stack_el* sl;		/* last element popped from the stack */

	const char *cp, 	/* Tracks the current character being matched */
				*s;		/* Tracks the beginning of the string */

	char cont;			/* flag to continue */
	
	wrx_subm *spare_sm = NULL;

	/* various indexes and counters*/
	int i, ctr, bol = 0, p;
	const char *b;
	wrx_subm *sm;
	
	int rv;
	jmp_buf ex;	/* Exception handling */

	assert(nfa->start < nfa->ns);
	assert(nfa->stop < nfa->ns);

	/** Initialize **/
	if(!nfa) return WRX_BAD_NFA;

	/* Handle NULL as a valid value for subm */
	if(!subm) nsm = 0;

	if(nsm < 0) return WRX_SMALL_NSM;

	stk = stack_create(nfa->n_states);
	if(!stk) return WRX_MEMORY;
	s = str;

	if(nsm < nfa->n_subm)
	{
		spare_sm = calloc(nfa->n_subm - nsm, sizeof *spare_sm);
		if(!spare_sm)
		{
			free_stack(stk);
			return WRX_MEMORY;
		}
	}

	for(i = 0; i < nsm; i++)
	{
		subm[i].beg = NULL;
		subm[i].end = NULL;
	}
	
	if((rv = setjmp(ex)) != 0) /* Exception handling: Error or Match */
	{		
		free_stack(stk);
		free(spare_sm);
		return rv;
	}
#define THROW(x) longjmp(ex, (x))

	/* Push the first character on top of the stack */
	p = push(stk, op_pos, str, nfa->start);
	if(p == 0 || p == -1)
		THROW(p?WRX_STACK:WRX_MEMORY);

	/** Execute **/
	while((sl = pop(stk)) != NULL)
	{
		if(sl->op == op_rbeg)
		{
			assert(sl->st < nfa->n_subm);

			if(sl->st < nsm)
				subm[sl->st].beg = sl->opr;
			else
			{
				assert(spare_sm);
				spare_sm[sl->st - nsm].beg = sl->opr;
			}

#ifdef DEBUG_OUTPUT
			printf("popped subm[%d].beg\n", sl->st);
#endif
			continue; /* Pop the next character */
		}
		else if(sl->op == op_rend)
		{
			assert(sl->st < nfa->n_subm);

			if(sl->st < nsm)
				subm[sl->st].end = sl->opr;
			else
			{
				assert(spare_sm);
				spare_sm[sl->st - nsm].end = sl->opr;
			}

#ifdef DEBUG_OUTPUT
			printf("popped subm[%d].end\n", sl->st);
#endif
			continue; /* Pop the next character */
		}

		cp = sl->opr;

		st = sl->st;
		assert(st < nfa->n_states && st >= 0);
		sp = &nfa->states[st];

#ifdef DEBUG_OUTPUT
		printf("popped %d", st);
		if(sp->op > START_OF_PRINT)
			printf(": '%c'\n", sp->op);
		else
			printf(": %d\n", sp->op);
#endif
		do
		{
			cont = 0;
			switch(sp->op)
			{
			case CHC:
			{
#ifdef DEBUG_OUTPUT
				printf("CHC @ %d\n", st);
#endif
				/* Push the alternatice route onto the stack */
				p = push(stk, op_pos, cp, sp->s[1]);
				if(p == 0 || p == -1)
					THROW(p?WRX_STACK:WRX_MEMORY);
					
				/* and continue along the current route */
				cont = 1;
			} break;
			case MOV:
			{
#ifdef DEBUG_OUTPUT
				printf("MOV @ %d\n", st);
#endif
				cont = 1;
			} break;
			case EOM:
			{
#ifdef DEBUG_OUTPUT
				printf("EOM @ %d\n", st);
#endif
				/* If we get here, the we found a path through the graph */
				THROW(WRX_MATCH);
			}
			case SET:
			{
#ifdef DEBUG_OUTPUT
				printf("SET @ %d ('%c')\n", st, cp[0]);
#endif
				if(BV_TST(sp->data.bv, cp[0]))
				{
					cont = 1;
					/* get the next character in the input string */
					cp++;
				}
			} break;
			case REC: /* Start recording a submatch */
			{
#ifdef DEBUG_OUTPUT
				printf("REC @ %d (%d)\n", st, sp->data.idx);
#endif
				/* Store the current submatch beginning in case we backtrack through here again */
				if(sp->data.idx < nsm)
					p = push(stk, op_rbeg, subm[sp->data.idx].beg, sp->data.idx);
				else
					p = push(stk, op_rbeg, spare_sm[sp->data.idx - nsm].beg, sp->data.idx);

				if(p == 0 || p == -1)
					THROW(p?WRX_STACK:WRX_MEMORY);
					
				/* Record the beginning of the submatch */
				if(sp->data.idx < nsm)
					subm[sp->data.idx].beg = cp;
				else
					spare_sm[sp->data.idx - nsm].beg = cp;

				cont = 1;
			} break;
			case STP:/* Stop recording a submatch */
			{
#ifdef DEBUG_OUTPUT
				printf("STP @ %d (%d)\n", st, sp->data.idx);
#endif
				/* Store the current submatch ending in case we backtrack through here again */
				if(sp->data.idx < nsm)
					p = push(stk, op_rend, subm[sp->data.idx].end, sp->data.idx);
				else
					p = push(stk, op_rend, spare_sm[sp->data.idx - nsm].end, sp->data.idx);

				if(p == 0 || p == -1)
					THROW(p?WRX_STACK:WRX_MEMORY);
					
				/* Record the ending of the submatch */
				if(sp->data.idx < nsm)
					subm[sp->data.idx].end = cp;
				else
					spare_sm[sp->data.idx - nsm].end = cp;
				cont = 1;
			} break;
			case BRF:/* Match a backreference */
			{
#ifdef DEBUG_OUTPUT
				printf("BRF @ %d (%d)\n", st, sp->data.idx);
#endif
				i = sp->data.idx;
				
				if(i >= nfa->n_subm) /* The specified backreference does not exist */
					THROW(WRX_INV_BREF);

				if(i < nsm)		
					sm = &subm[i];
				else
					sm = &spare_sm[i];
				
				if(!sm->beg || !sm->end) /* The specified backreference or has not been matched */					
					THROW(WRX_INV_BREF);
				
				for(cont = 1, b = sm->beg; b < sm->end; b++, cp++)
					if(b[0] != cp[0])
					{
						cont = 0;
						break;
					}
					
			} break;
			case BRI:/* Match a (case insensitive) backreference */
			{
#ifdef DEBUG_OUTPUT
				printf("BRI @ %d (%d)\n", st, sp->data.idx);
#endif
				i = sp->data.idx;
				
				if(i >= nfa->n_subm) /* The specified backreference does not exist */
					THROW(WRX_INV_BREF);

				if(i < nsm)		
					sm = &subm[i];
				else
					sm = &spare_sm[i];
				
				if(!sm->beg || !sm->end) /* The specified backreference or has not been matched */					
					THROW(WRX_INV_BREF);
				
				for(cont = 1, b = sm->beg; b < sm->end; b++, cp++)
					if(tolower(b[0]) != tolower(cp[0]))
					{
						cont = 0;
						break;
					}		
								
			} break;
			case BOL: /* beginning of line */
			{
#ifdef DEBUG_OUTPUT
				printf("BOL @ %d\n", st);
#endif
				bol = 1;
				if(cp == str)
					cont = 1;
				else
				{
					assert(cp > str);
					if(cp[-1] == '\r' || cp[-1] == '\n')
						cont = 1;
				}
			} break;
			case EOL: /* end of line */
			{
#ifdef DEBUG_OUTPUT
				printf("EOL @ %d\n", st);
#endif
				if(cp[0] == '\r' || cp[0] == '\n' || cp[0] == '\0')
					cont = 1;					
			} break;
			case BOW: /* beginning of word */
			{
#ifdef DEBUG_OUTPUT
				printf("BOW @ %d\n", st);
#endif
				if(cp == str)
				{
					if(isalnum(cp[0]))
						cont = 1; /* first char in string */
				}
				else
				{
					assert(cp > str);
					if(isalnum(cp[0]) && !isalnum(cp[-1]))
						cont = 1;
				}
			} break;
			case EOW: /* end of word */
			{
#ifdef DEBUG_OUTPUT
				printf("EOW @ %d\n", st);
#endif
				if(cp > str && isalnum(cp[-1]))
					if(!isalnum(cp[0]))
						cont = 1;
			} break;
			case BND:
			{
#ifdef DEBUG_OUTPUT
				printf("BND @ %d\n", st);
#endif
				if(cp == str)
				{
					if(isalnum(cp[0]))
						cont = 1; /* first char in string */
				}
				else
				{
					assert(cp > str);
					if(isalnum(cp[0]) ^ isalnum(cp[-1]))
						cont = 1;
				}
			} break;
			case MEV:
				/* Special case: Match everything (used with empty patterns) */
				THROW(WRX_MATCH);
			case MTC:
			{
#ifdef DEBUG_OUTPUT
				printf("MTC %c ?= %c @ %d\n", cp[0], sp->data.c, st);
#endif
				if(cp[0] == sp->data.c)
				{
					cont = 1;
					/* get the next character in the input string */
					cp++;
				}
			} break;
			case MCI: /* match case insensitive */
			{
#ifdef DEBUG_OUTPUT
				printf("MCI %c ?= %c @ %d\n", cp[0], sp->data.c, st);
#endif
				if(tolower(cp[0]) == tolower(sp->data.c))
				{
					cont = 1;
					/* get the next character in the input string */
					cp++;
				}
			} break;
			default: THROW(WRX_OPCODE);
			} /* switch sp->op */

			/* Continue along this path? */
			if(cont)
			{
				/* move to the next state */
				st = sp->s[0];
#ifdef DEBUG_OUTPUT
				printf("moving to state %d ('%c')\n", st, cp[0]);
#endif
				assert(st < nfa->n_states);
				sp = &nfa->states[st];
			}

			/*
			 *	count the number of op_pos operators on the stack for what
			 *	we're going to do next:
			 */
			ctr = 0;
			for(i = 0; i < stk->ts; i++)
				if(stk->els[i].op == op_pos) ctr++;

			/*
			 *	If we don't have a beginning-of-line '^' (BOL) and if our
			 *	stack will be empty after the next pop(),
			 *	we push the next starting character as a start state onto the
			 *	stack so that a pattern like "abc" can match against "xasxabc"
			 */
#ifdef DEBUG_OUTPUT
			printf("bol %d; ctr %d; s[1] %d\n", bol, ctr, s[1]);
#endif
			if(bol)
			{
				/* We have an '^' anchor, push every character that follows
				a newline to the stack */
				if(bol == 1 && ctr == 0)
				{
					while(s[0])
					{
						if((s[0] == '\r' || s[0] == '\n') && s[1])
						{
#ifdef DEBUG_OUTPUT
							printf("pushing '%c' start\n", s[1]);
#endif
							p = push(stk, op_pos, ++s, nfa->start);
							if(p == 0 || p == -1)
								THROW(p?WRX_STACK:WRX_MEMORY);
						}
						s++;
					}

					bol++;
				}
			}
			else if(ctr == 0 && s[1])
			{
#ifdef DEBUG_OUTPUT
				printf("pushing '%c' start\n", s[1]);
#endif
				p = push(stk, op_pos, ++s, nfa->start);
				if(p == 0 || p == -1)
					THROW(p?WRX_STACK:WRX_MEMORY);
			}
		}
		while(cont);
	}

	/* Error or No match */
	free_stack(stk);
	free(spare_sm);
	return rv;
	
#undef THROW
}
