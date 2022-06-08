#ifndef __GRAMENUM__HPP__
#define __GRAMENUM__HPP__

#ifndef GRAMINCLUDED
#error __FILE__ " cannot be included directly. Use '#include \"gram/AllHeaders.hpp\""
#endif

#include <set>
#include <unordered_map>
#include <list>

#include "gram/PairHash.hpp"

namespace ufo
{

class GramEnum
{
  // The maximum number of previous candidates we store.
  const int MAXGRAMCANDS = 200;

  // Previously generated candidates from sample grammar
  ExprUSet gramCands;
  deque<Expr> gramCandsOrder;

  // Key: <Non-terminal, Production>
  // Value: number of candidates generated with NT->Prod expansion
  unordered_map<std::pair<Expr,Expr>,mpz_class> candnum;
  // Total number of candidates we've generated (not necessarily returned)
  mpz_class totalcandnum = 0;

  std::list<ParseTree> deferred_cands;
  bool ignoreprios = false;

  // The grammar being used
  Grammar& gram;

  Traversal *traversal = NULL;
  TravParams params;

  ParseTree lastpt;
  ParseTree lastanyconstpt;
  Expr lastexpr;

  tribool lastSolverRes;

  ConstMap constMap;
  // True if we're in constant enumeration mode.
  bool enumeratingConsts = false;
  // Don't recreate a traversal for each time we need to enumerate consts;
  //   instead, cache them and just call 'Restart' when we need them again.
  // K: # of consts to generate, V: Grammar+Traversal for them
  unordered_map<int,pair<Grammar,Traversal*>> constTravs;

  bool shoulddefer(Grammar& gram, const Expr& nt, const Expr& expand)
  {
    //outs() << "shoulddefer(" << nt << ", " << expand << ") = ";
    //outs().flush();
    auto prod = make_pair(nt, expand);
    bool ret;
    if (gram.priomap.at(nt).at(expand) >= 1 || candnum[prod] == 0)
      ret = false;
    else
      ret = candnum[prod] > gram.priomap.at(nt).at(expand)*totalcandnum;
    //outs() << ret << "\n";
    return ret;
  }

  bool ptshoulddefer(const ParseTree &pt)
  {
    bool ret = false;
    pt.foreachPt([&] (const Expr &nt, const ParseTree &expand)
    {
      ret |= shoulddefer(gram, nt, expand.data());
    });
    return ret;
  }

  void gramReset()
  {
    travReset();
    gramCands.clear();
    gramCandsOrder.clear();
  }

  void travReset()
  {
    totalcandnum = 0;
    candnum.clear();
    deferred_cands.clear();
    ignoreprios = false;
    lastpt = NULL;
    lastexpr = NULL;
  }

  void initTraversal()
  {
    if (traversal)
    {
      delete traversal;
      traversal = NULL;
      travReset();
    }
    switch (params.method)
    {
      case TPMethod::RND:
        traversal = new RndTrav(gram, params);
        break;
      case TPMethod::CORO:
        traversal = new CoroTrav(gram, params);
        break;
      case TPMethod::NEWTRAV:
        traversal = new NewTrav(gram, params,
          [&] (const Expr& ei, const Expr& exp)
          { return shoulddefer(gram, ei, exp); });
        break;
      case TPMethod::NONE:
        errs() << "WARNING: Traversal method set to NONE. Segfaults inbound!" << endl;
        break;
    }
  }

  ParseTree getCandidate_Done()
  {
    if (deferred_cands.size() != 0)
    {
      if (debug && !ignoreprios)
      {
        outs() << "Done with normal candidates, using deferred ones" << endl;
      }
      if (!ignoreprios)
        ignoreprios = true;
      ParseTree ret = deferred_cands.front();
      deferred_cands.pop_front();
      return ret;
    }

    //exit(0);
    return NULL;

  }

  // Order of constants (existentially-quantified variables) in traversal's
  // FAPP
  vector<Expr> getConstsOrder()
  {
    const auto& uniqvars = GetCurrUniqueVars();
    // TODO: Support for synthesizing more than one type of constant
    assert(uniqvars.size() == 1);
    const auto& anyconsts = uniqvars.begin()->second;
    return vector<Expr>(anyconsts.begin(), anyconsts.end());
  }

  void restartConstTrav(const vector<Expr>& consts)
  {
    ExprFactory& efac(consts[0]->efac());
    auto itr = constTravs.find(consts.size());
    if (itr == constTravs.end())
    {
      // Create new traversal
      Grammar cgram;
      NT root = gram.addNt(
        mkConst(mkTerm(string("Root"), efac), mk<BOOL_TY>(efac)));
      cgram.setRoot(root);

      ExprVector rootdeclargs;
      rootdeclargs.push_back(mkTerm(string("All-Constants"), efac));
      for (const Expr& c : consts)
        rootdeclargs.push_back(typeOf(c));
      rootdeclargs.push_back(mk<BOOL_TY>(efac)); // Sort unimportant
      Expr rootdecl = mknary<FDECL>(rootdeclargs);

      ExprVector rootappargs;
      rootappargs.push_back(rootdecl);
      for (const Expr& c : consts)
        rootappargs.push_back(CFGUtils::constsNtName(typeOf(c)));
      Expr rootapp = mknary<FAPP>(rootappargs);

      cgram.addProd(root, rootapp);
      for (const auto& kv : constMap)
        for (const Expr& c : kv.second)
          cgram.addConst(c);

      TravParams paramscopy = params;
      paramscopy.method = TPMethod::NEWTRAV;

      // If we initialize traversal before emplace, it's reference
      // to the Grammar will be wrong.
      auto itr = constTravs.emplace(consts.size(), std::move(make_pair(
        std::move(cgram), nullptr))).first;
      Traversal *ctrav = new NewTrav(itr->second.first, paramscopy,
          [&] (const Expr& ei, const Expr& exp) { return false; });
      itr->second.second = ctrav;
    }
    else
      itr->second.second->Restart();
  }

  // Called when Z3 returned 'unknown' for ANY_*_CONST, so we need to attempt
  //   to eliminate the quantifier ourselves.
  ParseTree getCandidate_Consts()
  {
    vector<Expr> consts = std::move(getConstsOrder());

    Traversal* ctrav = constTravs.at(consts.size()).second;
    ParseTree cand = ctrav->Increment();
    if (ctrav->IsDone())
      enumeratingConsts = false;
    assert(cand.children().size() == 1);
    cand = cand.children()[0];
    assert(cand.children().size() == consts.size() + 1);
    unordered_map<Expr,const ParseTree> replMap;
    for (int i = 1; i < cand.children().size(); ++i)
      replMap.emplace(consts[i - 1], cand.children()[i]);
    return ParseTree::replaceAll(lastanyconstpt, replMap);
  }

  public:

  int debug;
  bool b4simpl = false;

  bool simplify;

  GramEnum(Grammar& _gram, const TravParams* _params = NULL, bool _simplify = false, int _debug = 0) :
    gram(_gram), simplify(_simplify), debug(_debug)
  {
    if (_params)
      SetParams(*_params);
  }
  ~GramEnum()
  {
    if (traversal)
    {
      delete traversal;
      traversal = NULL;
    }
    for (const auto& kv : constTravs)
      if (kv.second.second)
        delete kv.second.second;
  }

  void SetGrammar(Grammar& _gram)
  {
    gram = _gram;
    gramReset();
    Restart();
  }

  void SetConsts(const ConstMap& cmap) { constMap = cmap; }

  void Restart()
  {
    if (params.method != TPMethod::NONE)
      initTraversal();
  }

  const Grammar& GetGrammar() const { return gram; }

  void SetParams(TravParams _params)
  {
    bool needInitTrav = params != _params;
    TPMethod oldmeth = params.method;
    params = _params;
    if (params.iterdeepen && !gram.isInfinite())
    {
      if (debug > 2)
        outs() << "NOTE: Finite grammar but iterative deepening enabled. Disabling iterative deepening (as it does nothing here)" << endl;
      params.iterdeepen = false;
    }
    if (params.maxrecdepth != -2 && !gram.isInfinite())
    {
      params.maxrecdepth = 0;
      if (debug > 2)
        outs() << "NOTE: Finite grammar but maxrecdepth > 0. Setting maxrecdepth = 0 (as it does nothing here)" << endl;
    }
    if (needInitTrav)
    {
      if (oldmeth != TPMethod::NONE && traversal && !traversal->IsDone())
        // Wrap in assert so only printed in debug builds.
        assert(errs() << "WARNING: Re-initializing traversal. Make sure this is what you want!" << endl);
      initTraversal();
    }
  }

  TravParams GetParams() const
  {
    return params;
  }

  bool IsDone() const
  {
    if (!traversal)
      return true;
    if (!traversal->IsDone())
      return false;
    return deferred_cands.size() == 0;
  }

  int GetCurrDepth() const
  {
    if (!traversal)
      return -1;
    return traversal->GetCurrDepth();
  }

  const UniqVarMap& GetCurrUniqueVars() const
  { return traversal->GetCurrUniqueVars(); }

  void MarkSolverResult(tribool res)
  {
    lastSolverRes = res;
    if (indeterminate(res))
    {
      if (enumeratingConsts)
        // If solver returned unknown with consts, then we assume any further
        // constants will also do so for efficiency.
        enumeratingConsts = false;
      else
      {
        if (GetCurrUniqueVars().size() == 0)
          return; // Nothing further to enumerate for this candidate

        // Else, we need to try to eliminate the quantifier ourselves.
        restartConstTrav(getConstsOrder());
        enumeratingConsts = true;
        lastanyconstpt = lastpt; // The ParseTree to elim quants from
      }
    }
    else if (res && enumeratingConsts)
      enumeratingConsts = false; // We found the constants we were looking for
  }

  Expr ReplaceConsts(Expr cand, ZSolver<EZ3>::Model* model) const
  {
    assert(model);
    ExprUMap replMap;
    for (const auto& kv : GetCurrUniqueVars())
      for (const auto& expans : kv.second)
        replMap[expans] = model->eval(expans);
    if (replMap.size() == 1)
    {
      const auto itr = replMap.begin();
      return replaceAll(cand, itr->first, itr->second);
    }
    return replaceAll(cand, replMap);
  }

  Expr Increment()
  {
    Expr nextcand = NULL;
    ParseTree nextpt = NULL;

    // Generate a new candidate from the grammar, and simplify
    while (!nextcand)
    {
      if (enumeratingConsts)
        nextpt = getCandidate_Consts();
      else if (traversal->IsDone() && deferred_cands.size() == 0)
        return NULL;
      else if (!traversal->IsDepthDone() || deferred_cands.size() == 0)
      {
        nextpt = traversal->Increment();
        if (debug && traversal->IsDepthDone())
          outs() << "Done with depth " << traversal->GetCurrDepth() << endl;
      }
      else
      {
        nextpt = getCandidate_Done();
        if (!nextpt && traversal->IsDone()) return NULL;
      }
      if (!nextpt) continue;
      nextcand = nextpt;

      // Update candnum and totalcandnum
      nextpt.foreachPt([&] (const Expr& nt, const ParseTree& prod)
        {
          candnum[make_pair(nt, prod.data())]++;
        });
      totalcandnum++;

      if (b4simpl)
        outs() << "Before simplification: " << nextcand << endl;

      if (!gram.satsConstraints(nextpt))
      {
        nextcand = NULL;
        nextpt = NULL;
        if (b4simpl)
          outs() << "Doesn't satisfy constraints" << endl;
        continue;
      }

      if (simplify)
        nextcand = simplifyBool(simplifyArithm(nextcand));

      if (isOpX<TRUE>(nextcand) || isOpX<FALSE>(nextcand))
      {
        nextcand = NULL;
        nextpt = NULL;
        if (b4simpl)
          outs() << "Tautology/Contradiction" << endl;
        continue;
      }

      if (!ignoreprios && ptshoulddefer(nextpt))
      {
        deferred_cands.push_back(nextpt);
        nextpt = NULL;
        nextcand = NULL;
        if (b4simpl)
          outs() << "Need to defer candidate" << endl;
        continue;
      }

      if (!gramCands.insert(nextcand).second)
      {
        nextcand = NULL;
        nextpt = NULL;
        if (b4simpl)
          outs() << "Old candidate" << endl;
        continue;
      }

      if (gramCandsOrder.size() == MAXGRAMCANDS)
      {
        gramCands.erase(gramCandsOrder[0]);
        gramCandsOrder.pop_front();
      }
      gramCandsOrder.push_back(nextcand);
      break;
    }

    lastpt = nextpt;
    lastexpr = nextcand;

    return nextcand;
  }

  // Simplified (if enabled)
  Expr GetCurrCand() const { return lastexpr; }

  // Unsimplified
  ParseTree GetCurrPT() const { return lastpt; }
};

}

#endif
