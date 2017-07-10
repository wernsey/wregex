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

#include <stdio.h>
#include <assert.h>

#include "wregex.h"
#include "wrxcfg.h"

/*
 *	Returns a mnemonic for the specific opcode (wrx_state's op member)
 */
static const char *mnemonic(char op) {
	switch(op) {
		case MTC: return "MTC";
		case MCI: return "MCI";
		case MOV: return "MOV";
		case CHC: return "CHC";
		case SET: return "SET";
		case EOM: return "EOM";
		case BOL: return "BOL";
		case EOL: return "EOL";
		case BOW: return "BOW";
		case EOW: return "EOW";
		case REC: return "REC";
		case STP: return "STP";
		case BRF: return "BRF";
		case BRI: return "BRI";
		case BND: return "BND";
		case MEV: return "MEV";
	}
	return "UNK";
}

/*
 *	Prints the states in the NFA.
 *	For my own development and debugging purposes.
 */
void wrx_print_nfa(wregex_t *nfa) {
	short i, j;
	printf("start: %d; stop: %d\n", nfa->start, nfa->stop);
	
	assert(nfa->states[nfa->stop].op == EOM);
	
	for(i = 0; i < nfa->ns; i++) {
#ifdef OPTIMIZE
		if(nfa->states[i].op == MOV) continue;
#endif

		printf("%3d ", i);

		printf("%s ", mnemonic(nfa->states[i].op));

		if(nfa->states[i].op == MTC || nfa->states[i].op == MCI) {
			if(nfa->states[i].data.c == '\n')
				printf(" '\\n' ");
			else if(nfa->states[i].data.c == '\r')
				printf(" '\\r' ");
			else if(nfa->states[i].data.c == '\t')
				printf(" '\\t' ");
			else
				printf("'%c'", nfa->states[i].data.c);
		} else if(nfa->states[i].op == SET) {
			printf("[");
			assert(nfa->states[i].data.bv);

			/* These three are handled separately */
			if(BV_TST(nfa->states[i].data.bv, '\r'))
				printf("\\r");
			if(BV_TST(nfa->states[i].data.bv, '\n'))
				printf("\\n");
			if(BV_TST(nfa->states[i].data.bv, '\t'))
				printf("\\t");

			/* Now print all the othe characters in the bit vector */
			for(j = START_OF_PRINT; j < 127; j++)
				if(BV_TST(nfa->states[i].data.bv, j))
					printf("%c", j);

			printf("] ");
		} else if(nfa->states[i].op == REC || nfa->states[i].op == STP || nfa->states[i].op == BRF) {
			printf("<%d>", nfa->states[i].data.idx);
		} else if(nfa->states[i].op == CHC) {
			printf("---");
		}

		if(nfa->states[i].s[0] >= 0) {
			printf("%2d ", nfa->states[i].s[0]);
			if(nfa->states[i].s[1] >= 0)
				printf("%2d ", nfa->states[i].s[1]);
		} else {
			assert(nfa->states[i].s[1] < 0);
			assert(nfa->stop == i);
		}

		printf("\n");
	}
}

/*
 *	Prints an NFA's states in a format that can be used by the
 *	DOT tool to generate a graph
 */
void wrx_print_dot(wregex_t *nfa, const char *filename)
{
	FILE *f;
	int i, j;

	f = fopen(filename, "w");
	fprintf(f, "# Use like so: dot -Tgif -o outfile.gif %s\n", filename);
	fprintf(f, "digraph G {\n");

	fprintf(f, "  rankdir=LR;\n");
	fprintf(f, "  orientation=portrait;\n");
	fprintf(f, "  fontsize=8;\n");

	fprintf(f, "  start [shape=box];\n");

	fprintf(f, "  start -> state%03d;\n", nfa->start);

	for(i = 0; i < nfa->ns; i++) {
		if(nfa->states[i].op == MOV) continue;

		if(nfa->states[i].op == SET) {
			fprintf(f, "  state%03d [label=\"[", i);
			assert(nfa->states[i].data.bv);
			for(j = START_OF_PRINT; j < 127; j++) {
				if(BV_TST(nfa->states[i].data.bv, j)) {
					if(j == '\"')
						fprintf(f, "\\%c", j);
					else
						fprintf(f, "%c", j);
				}
			}
			fprintf(f, "]\",shape=box];\n");
		} else if(nfa->states[i].op == CHC)
			fprintf(f, "  state%03d [label=\"\",shape=point];\n", i);
		else if(nfa->states[i].op == EOM)
			fprintf(f, "  state%03d [label=\"stop\",shape=doublecircle];\n", i);
		else if(nfa->states[i].op == REC)
			fprintf(f, "  state%03d [label=\"%d\",shape=triangle];\n", i, nfa->states[i].data.idx);
		else if(nfa->states[i].op == STP)
			fprintf(f, "  state%03d [label=\"%d\",shape=invtriangle];\n", i, nfa->states[i].data.idx);
		else if(nfa->states[i].op == BRF)
			fprintf(f, "  state%03d [label=\"%d\",shape=diamond];\n", i, nfa->states[i].data.idx);
		else if(nfa->states[i].op == BOL)
			fprintf(f, "  state%03d [label=BOL,shape=circle];\n", i);
		else if(nfa->states[i].op == EOL)
			fprintf(f, "  state%03d [label=EOL,shape=circle];\n", i);
		else if(nfa->states[i].op == BOW)
			fprintf(f, "  state%03d [label=BOW,shape=circle];\n", i);
		else if(nfa->states[i].op == EOW)
			fprintf(f, "  state%03d [label=EOW,shape=circle];\n", i);
		else if(nfa->states[i].op == MTC) {
			if(nfa->states[i].data.c == '\n')
				fprintf(f, "  state%03d [label=\"'\\n\"',shape=circle];\n", i);
			else if(nfa->states[i].data.c == '\r')
				fprintf(f, "  state%03d [label=\"'\\r\"',shape=circle];\n", i);
			else if(nfa->states[i].data.c == '\t')
				fprintf(f, "  state%03d [label=\"'\\t\"',shape=circle];\n", i);
			else
				fprintf(f, "  state%03d [label=\"'%c'\",shape=circle];\n", i, nfa->states[i].data.c);
		}

		if((j = nfa->states[i].s[0]) >= 0)
			fprintf(f, "    state%03d -> state%03d [style=bold];\n", i, j);

		if((j = nfa->states[i].s[1]) >= 0)
			fprintf(f, "    state%03d -> state%03d;\n", i, j);
	}

	fprintf(f, "}\n");
	fclose(f);
}
