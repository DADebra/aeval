#ifndef __CVCABDUCE_HPP__
#define __CVCABDUCE_HPP__

#include <fstream>

inline Expr cvcAbduce(Expr goal, Expr assms)
{
  EZ3 z3(goal->efac());
  ExprUSet vars;
  filter(assms, bind::IsConst(), inserter(vars, vars.end()));
  filter(goal, bind::IsConst(), inserter(vars, vars.end()));

  ofstream queryout("abdin.smt2");

  queryout << "(set-logic ALL)\n";
  for (const Expr& v : vars)
    queryout << z3.toSmtLib(v->left()) << "\n";
  queryout << "(assert " << z3.toSmtLib(assms) << ")\n";
  queryout << "(get-abduct C " << z3.toSmtLib(goal) << ")\n";
  queryout << "(exit)\n";
  queryout.flush();

  int res = system("cvc5 --produce-abducts abdin.smt2 > abdout.smt2");
  if (res != 0)
  {
    system("rm -f abdin.smt2 abdout.smt2");
    return NULL;
  }

  ifstream respin("abdout.smt2");
  string line;
  getline(respin, line);
  respin.close();
  system("rm -f abdin.smt2 abdout.smt2");
  stringstream parsein;
  for (const Expr& v : vars)
    parsein << z3.toSmtLib(v->left());
  parsein << line;
  parsein << "(assert C)";
  const string& parsestr = parsein.str();
  return z3_from_smtlib(z3, parsestr);
}

#endif
