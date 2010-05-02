/*****************************************************************************************[Main.cc]
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "utils/Options.h"
#include "utils/System.h"
#include "simp/SimpSolver.h"
#include "circ/Circ.h"
#include "circ/Clausify.h"
#include "circ/Aiger.h"
#include "circ/DagShrink.h"
#include "circ/SatSweep.h"

using namespace Minisat;

//=================================================================================================


static
void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsed();
    printf("restarts              : %"PRIu64"\n", solver.starts);
    printf("conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    printf("decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    printf("propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    printf("conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) printf("Memory used           : %.2f MB\n", mem_used);
    printf("CPU time              : %g s\n", cpu_time);
}

SimpSolver* solver;
static void SIGINT_handler(int signum) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    printStats(*solver);
    printf("\n"); printf("*** INTERRUPTED ***\n");
    _exit(1); }


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
    setlinebuf(stdout);
    setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input is in plain or gzipped binary AIGER.\n");
#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    printf("WARNING: for repeatability, setting FPU to use double precision\n");
#endif

    // Extra options:
    //
    IntOption    verb           ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
    BoolOption   pre            ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
    BoolOption   clausify_naive ("MAIN", "clausify-naive", "Use naive clausification", false);
    StringOption aiger          ("MAIN", "aiger",  "If given, stop after preprocessing AIG and write the result to this file.");
    StringOption dimacs         ("MAIN", "dimacs", "If given, stop after producing CNF and write the result to this file.");
    IntOption    dash_iters     ("MAIN", "dash-iters", "Number of DAG Aware Rewriting iterations.", 5);
    BoolOption   split_output   ("MAIN", "split-output", "Split the topmost output conjunctions into multiple outputs.", true);
    BoolOption   sweep          ("MAIN", "sweep", "Perform SAT-sweeping of the AIG.", false);
    
    parseOptions(argc, argv, true);

    SimpSolver S;
    double     initial_time = cpuTime();

    if (!pre) S.eliminate(true);

    S.verbosity = verb;

    solver = &S;
    signal(SIGINT,SIGINT_handler);
    signal(SIGHUP,SIGINT_handler);

    vec<Var> input_vars;

    if (argc < 2 || argc > 3)
        printUsageAndExit(argc, argv);
    else {
        printf("============================[ Problem Statistics ]=============================\n");
        printf("|                                                                             |\n");

        Circ  c;
        Box   b;
        Flops flp;
        readAiger(argv[1], c, b, flp);

        if (flp.size() > 0)
            fprintf(stderr, "ERROR! Sequential circuits not supported!\n"), exit(1);

        //if (c.outputs.size() != 1)
        //    fprintf(stderr, "ERROR! Exactly 1 output expected, found %d!\n", c.outputs.size()), exit(1);
        if (split_output)
            splitOutputs(c, b, flp);

        printf("|  Number of inputs:     %12d                                         |\n", c.nInps());
        printf("|  Number of outputs:    %12d                                         |\n", b.outs.size());
        printf("|  Number of gates:      %12d                                         |\n", c.nGates());

        double parsed_time = cpuTime();
        printf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

        if (sweep){ 
            // Create a list of all references into the circuit that we need to keep:
            vec<Sig> sinks; 
            extractSigs(b,   sinks);
            extractSigs(flp, sinks); // Should be empty.

            // Set up the DAG-shrink environment:
            SatSweeper<Solver> sweeper(c, sinks);
            sweeper.sweep();

            // Map the old reference to point into the shrunk circuit:
            map(sweeper.resultMap(), b);
            map(sweeper.resultMap(), flp); // Should be empty.

            // Copy the shrunk circuit back:
            sweeper.copyResult(c);

            printf("|  Number of inputs:     %12d                                         |\n", c.nInps());
            printf("|  Number of outputs:    %12d                                         |\n", b.outs.size());
            printf("|  Number of gates:      %12d                                         |\n", c.nGates());
        }

        if (dash_iters > 0){ 
            // Create a list of all references into the circuit that we need to keep:
            vec<Sig> sinks; 
            extractSigs(b,   sinks);
            extractSigs(flp, sinks); // Should be empty.

            // Set up the DAG-shrink environment:
            DagShrinker dag(c, sinks);
            dag.shrinkIter(dash_iters);

            // Map the old reference to point into the shrunk circuit:
            map(dag.resultMap(), b);
            map(dag.resultMap(), flp); // Should be empty.

            // Copy the shrunk circuit back:
            dag.copyResult(c);
        }


        // void circInfo (      Circ& c, Gate g, GSet& reachable, int& n_ands, int& n_xors, int& n_muxes, int& tot_ands);
        // {
        //     GSet r; int n_ands = 0, n_xors = 0, n_muxs = 0, tot_ands = 0;
        // 
        //     for (int i = 0; i < c.outputs.size(); i++)
        //         circInfo(c.circ, gate(c.outputs[i]), r, n_ands, n_xors, n_muxs, tot_ands);
        //     fprintf(stderr, " >> ANDS = %d, XORS = %d, MUXES = %d, TOT = %d\n", n_ands, n_xors, n_muxs, tot_ands);
        // }

        if (aiger != NULL){
            printf("==============================[ Writing AIGER ]================================\n");
            writeAiger(aiger, c, b, flp);
            exit(0);
        }

        vec<Lit> unit;
        if (clausify_naive){
            NaiveClausifyer<SimpSolver> cl(c, S);
            for (int i = 0; i < b.outs.size(); i++){
                unit.clear();
                unit.push(cl.clausify(b.outs[i]));
                assert(S.okay());
                assert(S.value(unit.last()) == l_Undef);
                S.addClause(unit);
            }

            for (int i = 0; i < b.inps.size(); i++)
                input_vars.push(cl.clausify(b.inps[i])); 

        }else {
            Clausifyer<SimpSolver> cl(c, S);
            //cl.prepare();

            for (int i = 0; i < b.outs.size(); i++)
                cl.assume(b.outs[i]);

            for (int i = 0; i < b.inps.size(); i++){
                Lit x = cl.clausify(b.inps[i]);
                assert(!sign(x));
                input_vars.push(var(x));
            }
        }

        printf("===========================[ Clausify Statistics ]=============================\n");
        printf("|                                                                             |\n");
        printf("|  Number of variables:  %12d                                         |\n", S.nVars());
        printf("|  Number of clauses:    %12d                                         |\n", S.nClauses());

        double clausify_time = cpuTime();
        printf("|  Clausify time:        %12.2f s                                       |\n", clausify_time - parsed_time);
    }

    // Freeze input vars:
    //for (int i = 0; i < input_vars.size(); i++)
    //    S.setFrozen(input_vars[i], true);

    if (pre){
        double simplified_time_before = cpuTime();
        S.eliminate(true);
        printf("|  Simplification time:  %12.2f s                                       |\n", cpuTime() - simplified_time_before);
    }
    printf("|                                                                             |\n");

    FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

    if (!S.okay()){
        if (res != NULL) fprintf(res, "0\n"), fclose(res);
        printf("===============================================================================\n");
        printf("Solved by simplification\n");
        printStats(S);
        printf("\n");
        printf("UNSATISFIABLE\n");
        exit(20);
    }

    if (dimacs){
        printf("==============================[ Writing DIMACS ]===============================\n");
        S.toDimacs(dimacs);
        printStats(S);
        exit(0);
    }else{
        bool ret = S.solve();
        printStats(S);
        printf("\n");
        printf(ret ? "SATISFIABLE\n" : "UNSATISFIABLE\n");
        if (res != NULL){
            if (ret){
                fprintf(res, "1\n");
                for (int i = 0; i < input_vars.size(); i++){
                    Var inp = input_vars[i];
                    if (S.model[inp] == l_Undef)
                        fprintf(res, "x");
                    else if (S.model[inp] == l_True)
                        fprintf(res, "1");
                    else
                        fprintf(res, "0");
                }
                fprintf(res, "\n");
            }else
                fprintf(res, "0\n");
            fclose(res);
        }
#ifdef NDEBUG
        exit(ret ? 10 : 20);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (ret ? 10 : 20);
#endif
    }
}
