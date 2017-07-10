# wregex

## Introduction

*Many years ago I wrote my own Regex engine implementation.*

It supports a fairly large language, with curly braces, anchors, sub-match
extraction, back references and lazy (non-greedy) evaluation. It was written
to form the regular expression library of a scripting language I was writing.

Although I've consulted several sources and looked at the sources of a couple of
implementations (See my references below), this implementation is entirely my
own.

It is released under the terms of the MIT license. See the bottom of this file
for details.

Just a small disclaimer:

1. **wregex** tries to be practical, but it does not try to conform to any standards
	(since it was written for use in a scripting language).
2. I have no intention of adding Unicode support (it seems that if I wanted
	Unicode support, I should've designed for it from the beginning).

(now with more than 3200 lines of code, comments included)

### Usage

Like most regular expression engines, **wregex** first requires that the regular
expression first be compiled to a NFA-like data structure (`wregex_t` defined in
`wregex.h`). The compilation happens in the function `wrx_comp()` which takes a
string (`char*`) argument containing the expression, and returns the compiled NFA
structure (its two other parameters are pointers to integers for reporting
errors).

Once the expression has been compiled, the `wregex_t` is used to match a string in
the `wrx_exec()` function. You can reuse the `wregex_t` and don't need to call
`wrx_comp()` again. `wrx_exec()` returns 1 on a match, 0 on a non-match and less
than 0 on a runtime error. It also takes a pointer to an array where the
extracted sub-matches are to be stored.

If `wrx_exec()` returns an error code, the `wrx_error()` function can be used to get
a textual description of the error code.

After the last call to `wrx_exec()` the `wregex_t` structure's memory should be
deallocated using the `wrx_free()` function.

### Debugging

Additionally, two functions, `wrx_print_nfa()` and `wrx_print_dot()` are provided in
wrx_prnt.c that outputs the `wregex_t` structure's states after it has been
compiled. `wrx_print_nfa()` writes the states to stdout while `wrx_print_dot()`
writes a text file that can be used as input to the Graphviz DOT program
(http://www.research.att.com/sw/tools/graphviz/) to draw a graph containing all
the states and transitions in the `wregex_t`. These two functions are intended for
development and debugging.

### Examples

Two example programs are provided to demonstrate the API:

1. `test.c` which simply runs unit tests if no arguments are supplied. When
	arguments are supplied the first is used as a pattern and compiled into a
	`wregex_t` with `wrx_comp()`. The second argument is optional. If it supplied,
	it is used as a string which is matched against the `wregex_t` using
	`wrx_exec()`.
2. `wgrep.c` is a grep-like program that accepts a pattern and a text file as
	input, and outputs all lines in the file which matches that pattern.

## Compiling

The code has been written using only standard C functions, and I've used the
`"-Wall -Werror -pedantic"` flags when compiling, with the goal of portability.

The Makefile works with Linux and the MinGW port of GCC under Windows.

The default build uses `-O2` for optimization and strips debug symbols from the
output file for smaller binaries. Make the debug target to build a debug
version with debug symbols suitable for debugging with GDB and with
optimization turned off, like so:

	$ make debug

To delete the binaries, run

	$ make clean

To delete just the .o object files, run

	$ make wipe

The source files are laid out similar to other regex implementations I've
seen, with each of the major functions in a separate source file.

Here's what they do:

* `wregex.h`	- Header file with the data structures and prototypes. You need to
	`#include` this in any source file that will be using the regex matcher.
* `wrxcfg.h`	- Header file used internally. It contains definitions used by
	wrx_comp.c and wrx_exec.c. You don't need to expose this.
* `wrx_comp.c`	- Contains the `wrx_comp()` function's definition. It compiles a regex
				into an `wregex_t` NFA data structure.
* `wrx_exec.c`	- Contains the `wrx_exec()` function's definition. It matches a string
				to to a compiled NFA.
* `wrx_free.c`	- Contains the `wrx_free()` function's definition. It `free()`'s an NFA
				created by `wrx_comp()`.
* `wrx_error.c`	- Contains the `wrx_error()` function's definition. It describes error
				codes returned by `wrx_comp()` and `wrx_exec()`
* `wrx_prnt.c`	- Contains functions to print the NFA to stdout or to a input file
	for the DOT program (of the Graphviz package). I use these only for testing
	and debugging wrx_comp() and friends.
* `wrx_prnt.h`	- prototypes for the functions in wrx_prnt.c
* `test.c` - The test program.
* `wgrep.c` - Source file for the wgrep example program

## Syntax

The syntax of the pattern matcher is thus the following

	pattern	::= ['^'] [list] ['$']
	list	::= element ["|" list]
	element	::= ("(" [":"] list ")" | value)
				[(("*"|"+"|"?")["?"])|("{" [digit+] ["," [digit+]] "}" ["?"])]
				[element]
	value	::= (A-Za-z0-9!"#%&',-/:;=@\\_`~\r\t\n) | '<' | '>' |
				"[" ["^"] ranges "]" | "." | 'escape sequence'
	ranges 	::= (c ["-" c])+
		where c is any printable ASCII character (>= 0x20)

The syntax has the following features:
* A `'^'` at the beginning of the pattern forces a match at the beginning of
	a line, or after a newline/ carriage return
* A `'$'` at the end of the pattern forces a match at the end of the line
* The `'|'` operator has the lowest precedence. `"abc|def"` matches `"abc"` or
	`"def"`
* Brackets are used for grouping. Expressions enclosed in brackets also
	capture sub-matches. Sub-matches are numbered according to the order in
	which the '('s appear, starting from 1, so for example in `"((abc)(def))"`
	`submatch[1]` would capture `"abcdef"`, `[2]` would capture `"abc"` and `[3]`
	would capture `"def"`
	(`submatch[0]` captures the entire matching part of the string, as if
	there's an invisible `'('` and `')'` around the regex)
* An escape character followed by some digits specify a back reference match
	where the remainder of the input string is matched against a submatch.
	Remember that the submatches are indexed from 1, for example, the
	pattern `"(abc)def\1"` will match against "abcdefabc"
* A opening bracket followed by a `':'` is only used for grouping, but not for
	capturing a submatch, for example in `"((:abc)(def))"` `submatch[0]` would
	capture `"abcdef"` and `[1]` would capture `"def"` since the `(:...)` around
	"abc" don't capture
* A `'*'` following a `"(expr)"` or a "value" means match zero or more, for
	example, `"a*"` will match `""`, `"a"`, `"aa"`, `"aaa"` etc.
* A `'+'` following a `"'('list')'"` or a "value" means match one or more, for
	example, `"a+"` will match `"a"`, `"aa"`, `"aaa"` etc. but not `""`
* A `'?'` following a `"'('list')'"` or a `"value"` means match zero or one, for
	example, `"a?"` will match `""` or `"a"`.
* wregex supports "lazy"/non-greedy evaluation of the `'*'` and `'+'` (and the
	`'?'`) operators by following them with a `'?'`.
* `"a{m}"` means match exactly m "a"s. eg. `"a{3}"` is equivalent to `"aaa"`
* `"a{m,}"` means match at least m "a"s. eg. `"a{3}"` is equivalent to `"aaa+"`
* `"a{,n}"` means match at most n "a"s. eg. `"a{,3}"` is equivalent to `"a?a?a?"`
* `"a{m,n}"` means match at least m but not more than n "a"s. eg. "a{3,5}" is
	equivalent to "aaaa?a?"
* `"a{}"` and `"a{,}"` are treated like `"a*"`
* You can use these escape sequences in an expression:
	- '\.' : literal '.'
	- '\*' : literal '*'
	- '\+' : literal '+'
	- '\?' : literal '?'
	- '\[' : literal '['
	- '\]' : literal ']'
	- '\(' : literal '('
	- '\)' : literal ')'
	- '\{' : literal '{'
	- '\}' : literal '}'
	- '\|' : literal '|'
	- '\^' : literal '^'
	- '\$' : literal '$'
	- '\<' : literal '<'
	- '\>' : literal '>'
	- '\:' : literal ':'
	- '\r' : Carriage return
	- '\n' : Line feed
	- '\t' : tab
	- '\d' : digit, equivalent to [0-9]
	- '\a' : alphabetic, equivalent to [a-zA-Z]
	- '\u' : uppercase character, equivalent to [A-Z]
	- '\l' : lowercase character, equivalent to [a-z]
	- '\s' : space, equivalent to [ \r\n\t]
	- '\w' : 'word' character, equivalent to [a-zA-Z0-9_]
	- '\x' : hex character, equivalent to [0-9a-fA-F]
* `"[...]"` are used to match sets of characters, for example `"[abc]"` is
	equivalent to `"(:a|b|c)"` (although the internal implementation is
	different; `"[abc]"` should be preferred because it is less expensive)
* You can use the `'-'` operator within sets to display ranges of characters,
	for example `"[0-9]"` is equivalent to `"[0123456789]"`
* You can use these escape sequences within sets:
	- '\-' : literal '-'
	- '\^' : literal '^'
	- '\]' : literal ']'
	- '\r', '\n', '\t' : Carriage return, line feed and tab, respectively
	- '\d' : digit, equivalent to '0-9'
	- '\a' : alphabetic, equivalent to 'a-zA-Z'
	- '\u' : uppercase character, equivalent to 'A-Z'
	- '\l' : lowercase character, equivalent to 'a-z'
	- '\s' : space, equivalent to ' \r\n\t'
	- '\w' : 'word' character, equivalent to 'a-zA-Z0-9_'
	- '\x' : hex character, equivalent to '0-9a-fA-F'
* `'<'` matches the beginning of a word
* `'>'` matches the ending of a word
* `'\b'` works like both a `'<'` and a `'>'`: `"\babc\b"` is equivalent to `"<abc>"`
* Everything after `'\i'` will be case-insensitive, and everything after `'\I'`
	will be case-sensitive, so that `"\iabc\Iabc"` will match `"abcabc"`,
	`"ABCabc"`, `"Abcabc"` etc. but not `"abcABC"`. It is case-sensitive by
	default.
* Back references can also be case-insensitive: `"([abc]{3})-\i\0"` will match
	`"abc-ABC"` (but not `"aBc-ABC"` because the first `"[abc]{3}"` is case-
	sensitive)

(The syntax can be obtained from the comments in `wrx_comp.c` by typing
	`$ grep "\*\\$" wrx_comp.c` in the shell)

## Configuration

Some definitions in `wrxcfg.h` can be changed to alter the behaviour of the
engine. In particular:

* Defining `DEBUG_OUTPUT` prints debug information to `stdout` during `wrx_comp()`
	and `wrx_exec()`

* The way in which `wrx_comp()` is implemented requires several states to be added
	which does nothing. They have MOV opcodes which simply means that `wrx_exec()`
	should move to the next state when these are encountered. The optimize()
	function in wrx_comp.c simply runs through each state and changes every
	state transition which leads to a MOV to transition to the MOV's target so
	that this does not need to be done in `wrx_exec()`.
	Undefining `OPTIMIZE` removes this functionality, which is sometimes helpful
	when troubleshooting `wrx_comp.c`.

* Redefining ESC to another character changes the escape character, which is
	currently set to a backslash. My intention is that you can change it to suit
	your particular needs: If you use the functions directly in a C program, you
	can change it to, say '%' (like Lua's), so that you can type for example

		r = wrx_comp("^%d+", &e, &ep);
	instead of

		r = wrx_comp("^\\d+", &e, &ep);

	thus eliminating the need to double the backslash.
	
	I'm planning to use this trick in a scripting language that already uses
	`'\'` as an escape character in string constants. The Lua scripting language
	already does this (see http://www.lua.org/pil/20.2.html)

(redefining ESC should be tested first, as all my development centered arround
using '\' as an escape character)

## My TODO and pitfalls list:

Like all backtracking expression engines, **wregex** is susceptible to problems:
* The expression `"(:a*)*b"` cannot match "aaaaab" and will cause a runtime error.
	In general expressions such as this where there's a `'*'` on both sides of the
	')' should be avoided, or one should use lazy operators `'*?'` and `'+?'` as in
	`"(:a*)*?b"` which will work.
* The expression `"(:x+x+)+y"` will have no trouble matching the string `"xxxxxxy"`,
	but it will run into trouble when detecting that `"xxxxxx"` is not a match. In
	this case the performance will degrade exponentially with each additional
	`'x'`. Once again the problem relates to having a `'+'` on both sides of the `')'`.

[5] refers to these types of problems as "catastrophic backtracking".
Fortunately in most cases it is possible to rewrite the regex in such a way as
to avoid these problems.

(See my references below for more information and tips for how
to avoid these problems, http://www.regular-expressions.info probably being the
best place to start if you're a novice)

Expressions containing curly braces, say "A{m,n}", are implemented by
repeating the preceding expression A `n` times. For example, `A{1,4}` is handled
internally as `AA?A?A?`.

Because of this expressions like `((((a{1,100})){1,100}){1,100}){1,100}` can be
very expensive, this one requiring O(100^4) states internally to store the
`wregex_t`.

In the current version of **wregex** `wrx_comp()` will complain that too many states
are required to store the above expression, and there are currently no plans to
remedy this.

Russ Cox [2] hints that a Thompson-type engine is able to do sub-match
extraction, as well as non-greedy evaluation. Unfortunately it gives no
indication of how this is achieved.

The Thompson scheme's greatest disadvantage is that it cannot handle back
references, but these do not occur that frequently.

Thompson's evaluation method's greatest advantage is that it does not suffer
from the same backtracking problems (since it does not do any backtracking at
all) and it is therefore very attractive.

If I can ever figure out how it works, it might be worth adding it in a
separate function, say `wrx_thom()`, in a separate source file. I can then add
another function that can choose between wrx_exec() and `wrx_thom()` depending on
whether or not the expression uses back references.

Apparently Rob Pike implemented such a scheme in his SAM text editor. I've
looked at some of the SAM code but I don't yet understand how it implements
the sub-match extraction (not that I looked that hard).

Ville Laurikari, the author of the TRE regular expression engine
(http://laurikari.net/tre/), implemented this technique in TRE, and wrote a
thesis on the topic. I just can't get myself to read it at this stage.

I expected that several places where `MOV` states are added in `wrx_comp.c` may be
unnecessary, but removing them resulted in some strange problems.

Fortunately, the `optimize()` function in `wrx_comp.c` gets rid of all these `MOV`
states.

Ideas from http://www.regular-expressions.info/reference.html and
http://www.regular-expressions.info/refadv.html and
http://www.regular-expressions.info/refext.html:

1. Match the escape sequence \xFF where FF are 2 hexadecimal digits: Matches the
	character with the specified ASCII/ANSI value, which depends on the code
	page used. Can be used in character classes eg. \xA9 matches © when using
	the Latin-1 code page. ( On second thought it is not really that useful
	since I'm not supporting unicode, besides I already use \x for something
	else )
2. On second thought, "lookaround" don't seem too complicated to implement,
	although it would require some changes to the structures used to store the
	states within the NFA (it should be recursive - i.e the state structure
	should keep another `wregex_t *` in the union used for the bit-vectors and
	indexes)
3. Some engines also support comments in the form (?#comment). I don't think
	that it is difficult to implement (but is it really useful?)
4. .Net and Python allow for named backreferences: `(?<name>regex)` or
	`(?'name'regex)` capture the text matched by the regex inside them that can
	be referenced by the name between the single quotes. The name may consist
	of letters and digits, and then the backreferences `\k<name>` or `\k'name'`
	substituted with the text matched by the capturing group with the given
	name e.g. `(?<group>abc|def)=\k<group>` matches `abc=abc` or `def=def`, but not
	`abc=def` or `def=abc`.
	
## References:

In implementing this Regex pattern matcher,  I consulted these sources:

* [1] "Pattern Matching with Regular Expressions in C++" by Oliver Müller
	Published in Issue 27 of Linux Gazette, April 1998
	http://linuxgazette.net/issue27/mueller.html
* [2] "Regular Expression Matching Can Be Simple And Fast
	(but is slow in Java, Perl, PHP, Python, Ruby, ...)" by Russ Cox
	http://swtch.com/~rsc/regexp/regexp1.html
* [3] "The text editor sam" Rob Pike
	http://plan9.bell-labs.com/sys/doc/sam.html
	and the implementation at
	http://plan9.bell-labs.com/sources/plan9/sys/src/libregexp/regexec.c
* [4] "COMPILERS Principles, Techniques and Tools" Aho, Sethi, Ullman
	Addison Wesley 1986
* [5] The webpages at http://www.regular-expressions.info, such as the one at
	"Regular Expression Basic Syntax Reference" provided some insight
	http://www.regular-expressions.info/reference.html
* [6] "How Regexes Work" by Mark-Jason Dominus from The Perl Journal
	http://perl.plover.com/Regex/article.html
* [7] "Writing own regular expression parser" By Amer Gerzic from
	The Code Project
	http://www.codeproject.com/cpp/OwnRegExpressionsParser.asp
* [8] Ozan S. Yigit's implementation at http://www.cse.yorku.ca/~oz/
* [9] "Algorithmic Forays" by Eli Bendersky, accessible from
	http://www.gamedev.net/reference/list.asp?categoryid=25
* [10] "Understanding Regular Expressions" Jeffrey Friedl
	http://www.foo.be/docs/tpj/issues/vol1_2/tpj0102-0006.html
* [11] "Regular Expressions" by Hardeep Singh
	http://seeingwithc.org/topic7html.html
* [12] Henry Spencer's implementation can be found at http://arglist.com/regex/
* [13] The blog at http://www.codinghorror.com/blog/archives/000488.html also
	discusses catastrophic backtracking.
* [14] http://en.wikipedia.org/wiki/Regex

> Some people, when confronted with a problem, think "I know, I'll use regular
> expressions." Now they have two problems. - Jamie Zawinski

## License
```
Copyright (c) 2007-2015 Werner Stoop

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
```