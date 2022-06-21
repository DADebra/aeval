#ifndef RNDTRAV__HPP__
#define RNDTRAV__HPP__

#ifndef GRAMINCLUDED
#error __FILE__ " cannot be included directly. Use '#include \"gram/AllHeaders.hpp\""
#endif

#include <random>

namespace ufo
{

class RndTrav : public Traversal
{

  private:

  CFGUtils cfgutils;

  Grammar &gram;
  bool grammodified = false;
  TravParams gparams;
  NTParamMap ntparams;

  ParseTree lastcand;

  ExprFactory& efac;

  UniqVarMap uniqvars; // Per-candidate
  mpz_class uniqvarnum = -1; // Per-candidate

  default_random_engine randgenerator;
  // K: NT, V: Distribution of priorities, same order as prods[NT]
  unordered_map<NT, discrete_distribution<int>> distmap;

  void regendistmap()
  {
    distmap.clear();
    for (const auto &nt_prods : gram.prods)
    {
      // each NT
      NT nt = nt_prods.first;
      vector<mpq_class> rweights;
      for (const Expr &p : nt_prods.second)
        // each prod
        rweights.push_back(gram.priomap.at(nt).at(p));

      // Simple GCD, to make sure all priorities convert to integers
      mpz_class gcd = 1;
      for (auto &rw : rweights)
        gcd *= rw.get_den();

      vector<int> iweights;
      for (auto &rw : rweights)
      {
        mpq_class res = rw * gcd;
        assert(res.get_den() == 1);
        mpz_class ires = res.get_num();
        assert(ires.fits_sint_p());
        iweights.push_back(ires.get_si());
      }

      distmap.emplace(nt,
        std::move(discrete_distribution<int>(iweights.begin(),
        iweights.end())));
    }
  }

  // qvars is set of quantifier variables for this expression.
  // Using pointer because we need it to be nullable.
  ParseTree getRandCand(const Expr& root, int currdepth,
      std::shared_ptr<ExprUSet> qvars, Expr currnt)
  {
    if (gram.isVar(root) || bind::isLit(root) || isOpX<FDECL>(root))
      // Root is a symbolic variable or constant; don't expand.
      return ParseTree(root);
    else if (gram.isUniqueVar(root))
    {
      Expr uniqvar = mk<FAPP>(CFGUtils::uniqueVarDecl(typeOf(root)),
        mkTerm(++uniqvarnum, efac));
      uniqvars[root].insert(uniqvar);
      return ParseTree(uniqvar);
    }
    else if (gram.isNt(root))
    {
      if (root != currnt && !gram.pathExists(root, currnt))
      {
        currdepth = 0;
        currnt = root;
      }
      if (gram.prods.at(root).size() == 0)
      {
        CFGUtils::noNtDefError(root, gram.root);
        return NULL; // Unreachable
      }
      ParseTree cand = NULL;
      while (!cand)
      {
        // Randomly select from the available productions.
        // Offset by 1 because arg(0) is the fdecl.
        //int randindex = (rand() % (root->arity() - 1)) + 1;
        int randindex = distmap[root](randgenerator);
        Expr prod = gram.prods.at(root)[randindex];
        if (gram.isRecursive(prod, root) &&
            currdepth == gparams.maxrecdepth)
          continue;
        int newdepth = gram.isRecursive(prod, root) ?
                        currdepth + 1 : currdepth;

        cand = getRandCand(prod, newdepth, qvars, currnt);
      }
      return ParseTree(root, std::move(cand), true);
    }
    else if (isOpX<FAPP>(root))
    {
      if (qvars != NULL && qvars->count(root->first()) != 0)
        // Root is a variable for a surrounding quantifier
        return ParseTree(root);
      else if (root->arity() == 1)
      {
        // There's no definition, we're expanding an empty *_VARS
        CFGUtils::noNtDefError(root, gram.root);
        // We never get here
        return NULL;
      }
    }

    // Root is Z3-defined non-terminal

    vector<ParseTree> expanded_args;
    std::shared_ptr<ExprUSet> localqvars(new ExprUSet());

    if (qvars != NULL)
      for (auto& var : *qvars)
        localqvars->insert(var);

    if (isOpX<FORALL>(root) || isOpX<EXISTS>(root))
    {
      // Add quantifier variables to qvars
      for (int i = 0; i < root->arity() - 1; ++i)
      {
        localqvars->insert(root->arg(i));
      }
    }

    for (auto itr = root->args_begin();
         itr != root->args_end(); ++itr)
    {
      expanded_args.push_back(getRandCand(*itr, currdepth, localqvars, currnt));
    }

    return ParseTree(root, std::move(expanded_args), false);
  }

  void handleGramMod()
  {
    regendistmap();
    grammodified = false;
  }

  void onGramMod(ModClass cl, ModType ty)
  {
    if (cl == ModClass::PRIO || cl == ModClass::PROD)
      grammodified = true;
    lastcand = NULL;
  }
  std::shared_ptr<ModListener> mlp;

  public:

  RndTrav(Grammar &_gram, const TravParams& gp, const NTParamMap& np) :
    gram(_gram), gparams(gp), ntparams(np), efac(gram.root->efac())
  {
    if (gparams.iterdeepen)
    {
      errs() << "Warning: Random traversal doesn't support iterative deepening. Ignoring and starting at maximum recursion depth." << endl;
      if (gparams.maxrecdepth == -1)
      {
        gparams.maxrecdepth = -2;
        gparams.SetDefaults();
      }
    }
    else
      assert(gparams.maxrecdepth >= 0);
    mlp.reset(new ModListener(
      [&] (ModClass cl, ModType ty) { return onGramMod(cl, ty); }));
    bool ret = gram.addModListener(mlp);
    assert(ret);
    regendistmap();
    Increment();
  }

  virtual ~RndTrav()
  {
    bool ret = gram.delModListener(mlp);
    assert(ret);
  }

  virtual bool IsDone() { return false; }

  virtual bool IsDepthDone() { return false; }

  virtual int GetCurrDepth() { return gparams.maxrecdepth; }

  virtual ParseTree GetCurrCand() { return lastcand; }

  virtual const UniqVarMap& GetCurrUniqueVars() { return uniqvars; }

  virtual ParseTree Increment()
  {
    uniqvars.clear();
    uniqvarnum = -1;
    if (grammodified) handleGramMod();
    ParseTree ret = lastcand;
    lastcand = getRandCand(gram.root, 0, NULL, gram.root);
    lastcand.fixchildren();
    return ret;
  }
};

}
#endif
