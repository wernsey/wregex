#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* for getopt() and path functions - non-portable */
#include <unistd.h>

#include "wregex.h"

/* These are the values for the flags parameter to grep() */
#define INVERT		1
#define SUBMATCHES	2

/*
 *  Prints the usage of the program
 */
void usage(char *s) {
    printf("Usage:\n  %s [options] pattern [infile]\n", s);
    printf("where the following options are allowed:\n");
    printf("  -o outfilename   - Specify output file\n");
    printf("  -v               - Invert matches\n");
    printf("  -s               - Output only submatches\n");
    printf("Under construction...\n");
}

/*
 *  "greps" a file by matching each line in infile to the wregex_t, writes
 *	the results to outfile.
 */
void grep(wregex_t *r, FILE *infile, FILE *outfile, int flags) {
    char buffer[256], *sm;
    wregmatch_t *subm;
    int e, i, len;

    /* Allocate enough memory for all the submatches in the wregex_t */
    if(r->n_subm > 0) {
	    subm = calloc(sizeof *subm, r->n_subm);
	    if(!subm) {
	        fprintf(stderr, "Error: out of memory");
	        wrx_free(r);
	        exit(EXIT_FAILURE);
	    }
	} else
		subm = NULL;

    /* For each line in the file */
    while(!feof(infile)) {
        /* Read the line */
        if(fgets(buffer, sizeof buffer, infile) == buffer) {
            /* Match the line to the wregex_t */
            e = wrx_exec(r, buffer, subm, r->n_subm);

            if(e == 1 && flags & SUBMATCHES) {
	            /* Print only the submatches */
	            for(i = 0; i < r->n_subm; i++) {
		            /* Get the length of the submatch */
		            len = subm[i].end - subm[i].beg;

		            /* Allocate memory for it */
		            sm = malloc(len + 1);
		            if(!sm) {
			            fprintf(stderr, "Error: out of memory");
                		exit(EXIT_FAILURE);
		            }

		            /* Copy it */
		            strncpy(sm, subm[i].beg, len);
		            sm[len] = 0;

		            /* and print */
		            printf("%s ", sm);
		            free(sm);
	            }
	            printf("\n");
            } else if((!(flags & INVERT) && e == 1) || /* The line matched the pattern, or */
                ((flags & INVERT) && e == 0)) {  	/* The line did not match, and we want to invert matches */
                /* print the line */
                fputs(buffer, outfile);
            } else if(e < 0) {
                /* A run-time error occured - print it */
                fprintf(stderr, "Error in match: %s\n",  wrx_error(e));
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    FILE *infile,               /* Input file */
            *outfile = stdout;  /* Output file - defaults to the console */
    char c,
        *ifn,    		/* infile name */
        *ofn = NULL,    /* outfile name */
        *pat;           /* pattern */

    int i, e, ep, flags = 0;

    wregex_t *r; /* Used to store the compiled regular expression */

    /* Parse the command line options */
    while ((c = getopt(argc, argv, "o:vs?")) != EOF) {
      switch (c) {
        case 'o': ofn = optarg; break;
        case 'v': flags |= INVERT; break;
        case 's': flags |= SUBMATCHES; break;
        case '?': usage(argv[0]); return 1;
        }
    }

    if(optind >= argc) {
        /* No pattern/input file */
        usage(argv[0]);
        return 1;
    }

    if(ofn) {
        /* Open the output file */
        outfile = fopen(ofn, "w");
        if(!outfile) {
            fprintf(stderr, "Error: Unable to open %s for input", ofn);
            return 1;
        }
    }

    /* The pattern is the next command line argument */
    pat = argv[optind++];

    /* Compile the regular expression into a wregex_t */
    r = wrx_comp(pat, &e, &ep);
    if(!r) {
        /* Error occured - note how we use ep to indicate the
            position of the error */
        fprintf(stderr, "Error: %d\n%s\n%*c: %s", e, pat, ep, '^', wrx_error(e));
        return 1;
    }

    if(optind < argc) {
        /* For each input file */
        for (i = optind; i< argc; i++) {
            /* Open the input file */
            ifn = argv[i];
            infile = fopen(ifn, "r");
            if(!infile) {
                fprintf(stderr, "Error: Unable to open %s for input", argv[i]);
                wrx_free(r);
                return 1;
            }
            /* "grep" to input file */
            grep(r, infile, outfile, flags);

            /* ...and close it */
            fclose(infile);
        }
    } else {
        /* "grep" stdin */
        grep(r, stdin, outfile, flags);
    }

    /* Deallocate the memory allocated to the wregex_t */
    wrx_free(r);

    return 0;
}
