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

#include "wrxcfg.h"

/*
 *	Returns a description of the error code for wrx_comp()'s 'e' parameter or
 *	wrx_exec()'s return value;
 */
const char *wrx_error(int code) {
	if(code >= 0) return "No error";

	switch(code) {
	case WRX_MEMORY			: return "Out of memory";
	case WRX_VALUE			: return "Value expected";
	case WRX_BRACKET		: return "')' expected";
	case WRX_INVALID		: return "Invalid expression";
	case WRX_ANGLEB			: return "']' expected";
	case WRX_SET			: return "Error in [...] set";
	case WRX_RNG_ORDER		: return "v < u in the range [u-v]";
	case WRX_RNG_BADCHAR	: return "Non-alphnumeric character in [u-v]";
	case WRX_RNG_MISMATCH	: return "Mismatch in range [u-v]";
	case WRX_ESCAPE			: return "Invalid escape sequence";
	case WRX_BAD_DOLLAR		: return "'$' not at end of pattern";
	case WRX_CURLYB			: return "'}' expected";
	case WRX_BAD_CURLYB		: return "m > n in expression {m,n}";
	case WRX_BAD_NFA		: return "NFA invalid";
	case WRX_SMALL_NSM		: return "nsm parameter to wrx_exec() is too small";
	case WRX_INV_BREF		: return "Invalid backreference";
	case WRX_MANY_STATES	: return "Too many states in expression";
	case WRX_STACK			: return "Can't grow stack any further";
	case WRX_OPCODE			: return "Unknown opcode";
	}
	return "Unknown error";
}
