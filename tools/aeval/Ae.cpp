#include "ae/AeValSolver.hpp"
#include "ufo/Smt/EZ3.hh"
#include "ae/CLIParsing.hpp"

using namespace ufo;

/** An AE-VAL wrapper for cmd
 *
 * Usage: specify 2 smt2-files that describe the formula \foral x. S(x) => \exists y . T (x, y)
 *   <s_part.smt2> = S-part (over x)
 *   <t_part.smt2> = T-part (over x, y)
 *   --skol = to print skolem function
 *   --debug = to print more info and perform sanity checks
 *
 * Notably, the tool automatically recognizes x and y based on their appearances in S or T.
 *
 * Example:
 *
 * ./tools/aeval/aeval
 *   ../test/ae/example1_s_part.smt2
 *   ../test/ae/example1_t_part.smt2
 *
 */

void printUsage()
{
  outs() << "Usage: aeval <file1.smt2> [file2.smt2] [options]\n";
  outs() << "  Options:\n";
  outs() << "    <nothing>          just solve for the realizability\n";
  outs() << "    --skol             extract Skolem functions\n";
  outs() << "    --opt              optimize quantifier elimination\n";
  outs() << "    --merge            combina Skolem functions into a single ite-formula\n";
  outs() << "    --compact          attempt to make Skolems more compact\n";
  outs() << "    --debug <lvl>      enable debug logging (higher = more)\n";
}

int main (int argc, char ** argv)
{

  if(getBoolValue("--help", false, argc, argv) || argc == 1)
  {
    printUsage();
    return 0;
  }

  ExprFactory efac;
  EZ3 z3(efac);

  bool skol = getBoolValue("--skol", false, argc, argv);
  bool compact = getBoolValue("--compact", false, argc, argv);
  bool opt = getBoolValue("--opt", false, argc, argv);
  int debug = getIntValue("--debug", 0, argc, argv);
  bool split = !getBoolValue("--merge", false, argc, argv);
  
  Expr s, t = NULL;
  auto name = getSmtFileName(1, argc, argv);
  if (name == NULL)
  {
    outs () << "No filename is given\n";
    return 1;
  }
  s = z3_from_smtlib_file (z3, name);
  name = getSmtFileName(2, argc, argv);
  if (name != NULL)
  {
    t = z3_from_smtlib_file (z3, name);
  }

  if (t != NULL) split = false;    // disable for JSyn
  if (skol && opt) opt = false;    // skolem not supported (yet)

  aeSolveAndSkolemize(s, t, skol, debug, opt, compact, split);

  return 0;
}
