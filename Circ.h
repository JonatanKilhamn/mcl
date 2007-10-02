/******************************************************************************************[Circ.h]
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

#ifndef Circ_h
#define Circ_h

#include "Set.h"
#include "CircTypes.h"
#include "SolverTypes.h"

#include <cstdio>

//=================================================================================================
// Circ -- a class for representing combinational circuits.


const uint32_t pair_hash_prime = 148814101;

class Circ
{
    // Types:
    struct Bin { Sig x, y; };
    typedef GMap<Bin> Gates;

    struct Eq { 
        const Gates& gs; 
        Eq(const Gates& gs_) : gs(gs_) {}
        bool operator()(Gate x, Gate y) const { 
            assert(type(x) == gtype_And);
            assert(type(y) == gtype_And);
            //printf("checking %d : %c%d, %c%d\n", index(x), sign(gs[x].x)?' ':'-', index(gate(gs[x].x)), sign(gs[x].y)?' ':'-', index(gate(gs[x].y)));
            //printf("    ---- %d : %c%d, %c%d\n", index(y), sign(gs[y].x)?' ':'-', index(gate(gs[y].x)), sign(gs[y].y)?' ':'-', index(gate(gs[y].y)));
            return gs[x].x == gs[y].x && gs[x].y == gs[y].y; }
    };

    struct Hash { 
        const Gates& gs; 
        Hash(const Gates& gs_) : gs(gs_) {}
        uint32_t operator()(Gate x) const { 
            assert(type(x) == gtype_And);
            return index(gs[x].x) * pair_hash_prime + index(gs[x].y); }
    };

    // Member variables:
    Gates               gates;
    Set<Gate, Hash, Eq> strash;
    unsigned int        next_id;
    vec<unsigned int>   free_ids;
    Gate                tmp_gate;
    unsigned int        n_inps;

    // Private methods:
    unsigned int allocId();
    void         freeId (unsigned int id);

    vec<vec<Sig> > constraints;

 public:
    Circ() : strash(Hash(gates), Eq(gates)), next_id(1), tmp_gate(gate_True), n_inps(0) { gates.growTo(tmp_gate); }

    int nGates() const { return gates.size()- free_ids.size() - n_inps - 1; }
    int nInps () const { return n_inps; }

    // Node constructor functions:
    Sig mkInp    ();
    Sig mkAnd    (Sig x, Sig y);
    Sig mkOr     (Sig x, Sig y);
    Sig mkXorOdd (Sig x, Sig y);
    Sig mkXorEven(Sig x, Sig y);
    Sig mkXor    (Sig x, Sig y);

    // Add extra implications:
    void constrain(const vec<Sig>& xs) { constraints.push(); xs.copyTo(constraints.last()); }
    template<class Solver>
    void addConstraints(Solver& S, GMap<Var>& vmap);
    
    // Node inspection functions:
    Sig lchild(Gate g) const;
    Sig rchild(Gate g) const;
    Sig lchild(Sig x)  const;
    Sig rchild(Sig x)  const;
};

//=================================================================================================
// Circ utility functions:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
bool evaluate(const Circ& c, Sig x, GMap<lbool>& values);


//=================================================================================================
// Implementation of inline methods:

inline unsigned int Circ::allocId()
{
    unsigned int id;
    if (free_ids.size() > 0){
        // There are recyclable indices:
        id = free_ids.last();
        free_ids.pop();
    }else{
        // Must choose a new index, and adjust map-size of 'gates':
        id = next_id++;
        gates.growTo(mkGate(id, /* doesn't matter which type */ gtype_Inp));
    }

    return id;
}


inline void Circ::freeId(unsigned int id){ free_ids.push(id); }
inline Sig  Circ::lchild(Gate g) const   { assert(type(g) == gtype_And); return gates[g].x; }
inline Sig  Circ::rchild(Gate g) const   { assert(type(g) == gtype_And); return gates[g].y; }
inline Sig  Circ::lchild(Sig  x) const   { assert(type(x) == gtype_And); return gates[gate(x)].x; }
inline Sig  Circ::rchild(Sig  x) const   { assert(type(x) == gtype_And); return gates[gate(x)].y; }
inline Sig  Circ::mkInp    ()            { n_inps++; return mkSig(mkGate(allocId(), gtype_Inp), false); }
inline Sig  Circ::mkOr     (Sig x, Sig y){ return ~mkAnd(~x, ~y); }
inline Sig  Circ::mkXorOdd (Sig x, Sig y){ return mkOr (mkAnd(x, ~y), mkAnd(~x, y)); }
inline Sig  Circ::mkXorEven(Sig x, Sig y){ return mkAnd(mkOr(~x, ~y), mkOr ( x, y)); }
inline Sig  Circ::mkXor    (Sig x, Sig y){ return mkXorEven(x, y); }

inline Sig  Circ::mkAnd (Sig x, Sig y){
    // Simplify:
    if      (x == sig_True)  return y;
    else if (y == sig_True)  return x;
    else if (x == y)         return x;
    else if (x == sig_False || y == sig_False || x == ~y) 
        return sig_False;

    // Order:
    if (y < x) { Sig tmp = x; x = y; y = tmp; }

    // Strash-lookup:
    Gate g = tmp_gate;
    gates[g].x = x;
    gates[g].y = y;

    //printf("looking up node: %c%d & %c%d\n", sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));

    if (!strash.peek(g)){
        // New node needs to be created:
        g = mkGate(allocId(), gtype_And);
        gates[g].x = x;
        gates[g].y = y;
        strash.insert(g);
        //printf("created node %3d = %c%d & %c%d\n", index(g), sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));
        //printf(" -- created new node.\n");
    }
    //else
    //    printf(" -- found old node.\n");

    return mkSig(g);
}

template<class Solver>
void Circ::addConstraints(Solver& S, GMap<Var>& vmap)
{
    int cnt = 0;

    for (int i = 0; i < constraints.size(); i++){
        vec<Sig>& constr = constraints[i];
        vec<Lit>  clause;

        for (int j = 0; j < constr.size(); j++){
            vmap.growTo(gate(constr[j]), var_Undef);
            if (vmap[gate(constr[j])] == var_Undef)
                goto undefined;
            else
                clause.push(mkLit(vmap[gate(constr[j])], sign(constr[j])));
        }

        cnt++;
        S.addClause(clause);
        if (i+1 < constraints.size()){
            constraints.last().moveTo(constr);
            constraints.pop();
            i--;
        }

    undefined:
        ;
    }
    printf("Added %d extra constraints (%d left)\n", cnt, constraints.size());
}


//=================================================================================================
// Debug etc:


//=================================================================================================
#endif
