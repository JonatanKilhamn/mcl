/*****************************************************************************************[Aiger.h]
Copyright (c) 2007, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef Aiger_h
#define Aiger_h

#include "Circ.h"

//=================================================================================================
// Functions for parsing and printing circuits in the AIGER format. See <http://fmv.jku.at/aiger/>
// for specification of this format as well as supporting tools and example circuits.


void readAiger (const char* filename, Circ& c, vec<Sig>& inputs, vec<Def>& latch_defs, vec<Sig>& outputs);
void writeAiger(const char* filename, Circ& c, const vec<Sig>& inputs, const vec<Def>& latch_defs, const vec<Sig>& outputs);

//=================================================================================================
#endif