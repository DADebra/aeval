#include "ae/AeValSolver.hpp"
#include "ufo/Smt/EZ3.hh"
#include "utils/StdHash.hpp"

using namespace ufo;

vector<ExprVector> findus(Expr e, Expr extra, ExprVector exvars, SMTUtils& u)
{
  vector<ExprVector> ret;
  ret.push_back(ExprVector());
  assert(u.isSat(e, extra));
  for (const Expr& var : exvars)
  {
    ExprVector newexvars;
    newexvars.reserve(exvars.size() - 1);
    for (const Expr& ev : exvars)
      if (ev != var) newexvars.push_back(ev);

    Expr fae = mk<FORALL>(var->left(), e);
    if (u.isSat(fae, extra))
    {
      auto nestret = findus(fae, extra, newexvars, u);
      for (ExprVector& ev : nestret)
      {
        ev.push_back(var);
        ret.push_back(ev);
      }
    }
    else
    {
      auto nestret = findus(e, extra, newexvars, u);
      for (ExprVector& ev : nestret)
        ret.push_back(ev);
    }
  }
  return std::move(ret);
}

ExprVector realabduce(Expr assms, Expr goal, Expr extra, SMTUtils& u)
{
  ExprVector ret;

  ExprVector allvars;
  filter(mk<IMPL>(assms, goal), bind::IsConst(), inserter(allvars, allvars.end()));
  auto us = findus(mk<IMPL>(assms, goal), mk<AND>(extra, assms), allvars, u);
  set<ExprVector> seen;
  for (auto itr = us.rbegin(); itr != us.rend(); ++itr)
  {
    if (!seen.insert(*itr).second)
      continue;
    if (itr->size() == 0)
      continue;
    ret.push_back(simplifyBool(simplifyArithm(mkNeg(eliminateQuantifiers(
      mk<AND>(assms, extra, mkNeg(goal)), *itr)))));
    assert(u.implies(mk<AND>(assms, ret.back()), goal));
  }
  return std::move(ret);
}

int main (int argc, char ** argv)
{

  ExprFactory efac;
  EZ3 z3(efac);
  SMTUtils u(efac);

  if (argc != 2)
  {
    outs() << "Usage: " << argv[0] << " formula.smt2\n";
    return 1;
  }

  Expr asserts = z3_from_smtlib_file(z3, argv[1]);
  assert(isOpX<AND>(asserts) && asserts->arity() >= 2);
  Expr assms = asserts->arg(0),
       goal = asserts->arg(1);
  ExprVector extracnjs;
  for (int i = 2; i < asserts->arity(); ++i) extracnjs.push_back(asserts->arg(i));
  Expr extra = conjoin(extracnjs, efac);

  for (const Expr& out : realabduce(assms, goal, extra, u))
    outs() << z3.toSmtLib(out) << endl;

  return 0;
}
