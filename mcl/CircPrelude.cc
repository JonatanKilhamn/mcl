/**********************************************************************************[CircPrelude.cc]
Copyright (c) 2011, Niklas Sorensson

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

#include "mcl/CircPrelude.h"

using namespace Minisat;

//=================================================================================================
// Circ utility functions:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
//
bool Minisat::evaluate(const Circ& c, Sig x, GMap<lbool>& values)
{
    Gate g = gate(x);
    values.growTo(g, l_Undef);
    if (values[g] == l_Undef){
        assert(type(g) == gtype_And);
        values[g] = lbool(evaluate(c, c.lchild(g), values) && evaluate(c, c.rchild(g), values));
        //printf("%d = %s%d & %s%d ==> %d\n", index(g), sign(c.lchild(g)) ? "-":"", index(gate(c.lchild(g))), sign(c.rchild(g)) ? "-":"", index(gate(c.rchild(g))),
        //       toInt(values[g]));

    }
    assert(values[g] != l_Undef);
    return (values[g] ^ sign(x)) == l_True;
}



//=================================================================================================
// Generate bottomUp topological orders:
//
void Minisat::bottomUpOrder(const Circ& c, Sig  x, GSet& gset) { bottomUpOrder(c, gate(x), gset); }
void Minisat::bottomUpOrder(const Circ& c, Gate g, GSet& gset)
{
    if (gset.has(g) || g == gate_True) return;

    if (type(g) == gtype_And){
        bottomUpOrder(c, gate(c.lchild(g)), gset);
        bottomUpOrder(c, gate(c.rchild(g)), gset);
    }
    gset.insert(g);
}


void Minisat::bottomUpOrder(const Circ& c, const vec<Gate>& gs, GSet& gset)
{
    for (int i = 0; i < gs.size(); i++)
        bottomUpOrder(c, gs[i], gset);
}


void Minisat::bottomUpOrder(const Circ& c, const vec<Sig>& xs, GSet& gset)
{
    for (int i = 0; i < xs.size(); i++)
        bottomUpOrder(c, xs[i], gset);
}


#if 0
// FIXME: remove or update when needed
void Minisat::bottomUpOrder(const Circ& c, const vec<Gate>& latches, const GMap<Sig>& latch_defs, GSet& gset)
{
    bool repeat;
    do {
        repeat = false;
        for (int i = 0; i < latches.size(); i++){
            Gate g = latches[i];
            Gate d = gate(latch_defs[g]);
            
            if (gset.has(g) && !gset.has(d)){
                bottomUpOrder(c, d, gset);
                repeat = true;
            }
        }
    } while (repeat);
}
#endif

//=================================================================================================
// Copy the fan-in of signals, from one circuit to another:
//

static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map);
static inline Sig _copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map){ return _copyGate(src, dst, gate(x), copy_map) ^ sign(x); }
static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map)
{
    if (copy_map[g] == sig_Undef)
        if (g == gate_True)
            copy_map[g] = sig_True;
        else if (type(g) == gtype_Inp)
            copy_map[g] = dst.mkInp();
        else{
            assert(type(g) == gtype_And);
            copy_map[g] = dst.mkAnd(_copySig(src, dst, src.lchild(g), copy_map), 
                                    _copySig(src, dst, src.rchild(g), copy_map));
        }

    return copy_map[g];
}


Sig  Minisat::copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map) { 
    copy_map.growTo(src.lastGate(), sig_Undef); return _copyGate(src, dst, g, copy_map); }
Sig  Minisat::copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map) {
    copy_map.growTo(src.lastGate(), sig_Undef); return _copySig (src, dst, x, copy_map); }
void Minisat::copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map)
{
    copy_map.growTo(src.lastGate(), sig_Undef);
    for (int i = 0; i < xs.size(); i++)
        _copySig(src, dst, xs[i], copy_map);
}


//=================================================================================================
// Copy everything from one circuit to another:
//


void Minisat::copyCirc(const Circ& src, Circ& dst, GMap<Sig>& map)
{
    map.growTo(src.lastGate(), sig_Undef);

    map[gate_True] = sig_True;
    for (Gate g = src.firstGate(); g != gate_Undef; g = src.nextGate(g))
        if (map[g] == sig_Undef)
            if (type(g) == gtype_Inp)
                map[g] = dst.mkInp();
            else {
                assert(type(g) == gtype_And);
                
                Sig ix = src.lchild(g);
                Sig iy = src.rchild(g);
                Sig ux = map[gate(ix)] ^ sign(ix);
                Sig uy = map[gate(iy)] ^ sign(iy);

                map[g] = dst.mkAnd(ux, uy);
            }
}