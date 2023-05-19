#ifndef RNDLEARNERV3__HPP__
#define RNDLEARNERV3__HPP__

#include "RndLearner.hpp"

#include "ae/CVCAbduce.hpp"
#include "utils/StdHash.hpp"
#include "utils/PairHash.hpp"

#ifdef HAVE_ARMADILLO
#include "DataLearner.hpp"
#endif

using namespace std;
using namespace boost;
namespace ufo
{
  struct ArrAccessIter
  {
    bool grows;
    Expr iter;
    Expr qv;
    Expr precond;
    Expr postcond;
    Expr range;
    bool closed;
    vector<int> mbp;
  };

  static bool (*weakeningPriorities[])(Expr, tribool) =
  {
    [](Expr cand, tribool val) { return bool(!val); },
    [](Expr cand, tribool val) { return indeterminate(val) && containsOp<FORALL>(cand); },
    [](Expr cand, tribool val) { return indeterminate(val); }
  };

  class RndLearnerV3 : public RndLearnerV2
  {
    private:

    ExprSet checked;
    set<HornRuleExt*> propped;
    map<int, ExprVector> candidates;
    map<int, deque<Expr>> deferredCandidates;
    map<int, ExprSet> bootCands;
    map<int, ExprSet> allDataCands;
    map<int, ExprSet> tmpFailed;
    map<int, ExprTree> mbpDt, strenDt;
    int mut;
    int dat;
    int to;

    // extra options for disjunctive invariants
    bool dDisj;
    bool dAllMbp;
    bool dAddProp;
    bool dAddDat;
    bool dStrenMbp;
    bool dRecycleCands;
    bool dGenerous;
    int dFwd;
    int mbpEqs;
    int alternver;
    bool hasconstarr = false;

    map<int, Expr> postconds;
    map<int, Expr> ssas;
    map<int, Expr> prefs;
    map<int, ExprSet> mbps;

    protected:

    map<int, vector<ArrAccessIter* >> qvits; // per cycle
    map<int, ExprSet> qvars;

    public:
    bool boot = true;

    RndLearnerV3 (ExprFactory &efac, EZ3 &z3, CHCs& r, unsigned to, bool freqs, bool aggp,
                  int _mu, int _da, bool _d, int _m, bool _dAllMbp, bool _dAddProp,
                  bool _dAddDat, bool _dStrenMbp, int _dFwd, bool _dR, bool _dG, int _to, int debug, string fileName, string gramfile, int sw, bool sl, int _aver) :
      RndLearnerV2 (efac, z3, r, to, freqs, aggp, debug, fileName, gramfile, sw, sl),
                  mut(_mu), dat(_da), dDisj(_d), mbpEqs(_m), dAllMbp(_dAllMbp),
                  dAddProp(_dAddProp), dAddDat(_dAddDat), dStrenMbp(_dStrenMbp),
                  dFwd(_dFwd), dRecycleCands(_dR), dGenerous(_dG), to(_to), alternver(_aver)
    {
      for (const auto& hr : ruleManager.chcs)
        if (containsOp<CONST_ARRAY>(hr.body))
        {
          hasconstarr = true;
          break;
        }
    }

    bool checkInit(Expr rel)
    {
      vector<HornRuleExt*> adjacent;
      for (auto &hr: ruleManager.chcs)
      {
        if (hr.isFact && hr.dstRelation == rel)
        {
          adjacent.push_back(&hr);
        }
      }
      if (adjacent.empty()) return true;
      return multiHoudini(adjacent);
    }

    bool checkInductiveness(Expr rel)
    {
      vector<HornRuleExt*> adjacent;
      for (auto &hr: ruleManager.chcs)
      {
        bool checkedSrc = find(checked.begin(), checked.end(), hr.srcRelation) != checked.end();
        bool checkedDst = find(checked.begin(), checked.end(), hr.dstRelation) != checked.end();
        if ((hr.srcRelation == rel && hr.dstRelation == rel) ||
            (hr.srcRelation == rel/* && checkedDst*/) ||
            (hr.dstRelation == rel/* && checkedSrc*/) ||
            (checkedSrc && checkedDst) ||
            (hr.isFact && checkedDst))
        {
          if (!hr.isQuery) adjacent.push_back(&hr);
        }
      }
      if (adjacent.empty()) return true;
      return multiHoudini(adjacent);
    }

    ArrAccessIter* getQvit (int invNum, Expr it)
    {
      for (auto a : qvits[invNum]) if (a->qv == it) return a;
      return NULL;
    }

    Expr actualizeQV (int invNum, Expr cand)
    {
      assert(isOpX<FORALL>(cand));
      Expr ranges = cand->last()->left();
      ExprMap repls;
      for (int i = 0; i < cand->arity()-1; i++)
      {
        auto qv = bind::fapp(cand->arg(i));
        for (auto a : qvits[invNum])
        {
          if (u.implies(ranges, replaceAll(a->range, a->qv, qv)))
          {
            repls[qv->left()] = a->qv->left();
            continue;
          }
        }
      }
      return replaceAll(cand, repls);
    }

    Expr renameCand(Expr newCand, ExprVector& varsRenameFrom, int invNum)
    {
      newCand = replaceAll(newCand, varsRenameFrom, invarVarsShort[invNum]);
      return newCand;
    }

    Expr eliminateQuantifiers(Expr e, ExprVector& varsRenameFrom, int invNum, bool bwd)
    {
      Expr eTmp = keepQuantifiersRepl(e, varsRenameFrom);
      eTmp = weakenForHardVars(eTmp, varsRenameFrom);
      if (bwd) eTmp = mkNeg(eTmp);
      eTmp = simplifyBool(/*simplifyArithm*/(eTmp /*, false, true*/));
      eTmp = unfoldITE(eTmp);
      eTmp = renameCand(eTmp, varsRenameFrom, invNum);
      if (printLog >= 4)
      {
        outs () << "       QE [keep vars  "; pprint(varsRenameFrom); outs () << "]\n"; pprint(e, 9);
        outs () << "       QE res:\n"; pprint(eTmp, 9);
      }
      return eTmp;
    }

    void addPropagatedArrayCands(Expr rel, int invNum, Expr candToProp)
    {
      vector<int>& tmp = ruleManager.getCycleForRel(rel);
      if (tmp.size() != 1) return; // todo: support

      Expr fls = candToProp->last()->right();
      for (auto & q : qvits[invNum])
        fls = replaceAll(fls, q->qv, q->iter);
      Expr bdy = rewriteSelectStore(ruleManager.chcs[tmp[0]].body);

      ExprVector ev;
      for (int h = 0; h < ruleManager.chcs[tmp[0]].dstVars.size(); h++)
      {
        Expr var = ruleManager.chcs[tmp[0]].srcVars[h];
        bool pushed = false;
        for (auto & q : qvits[invNum])
        {
          if (q->iter == var)
          {
            ev.push_back(var);
            pushed = true;
          }
        }
        if (!pushed) ev.push_back(ruleManager.chcs[tmp[0]].dstVars[h]);
      }

      ExprSet cnjs;
      Expr newCand2;
      cnjs.clear();
      getConj(eliminateQuantifiers(mk<AND>(fls, bdy), ev, invNum, false), cnjs);
      for (auto & c : cnjs)
      {
        if (u.isTrue(c) || u.isFalse(c)) continue;
        Expr e = ineqNegReverter(c);
        if (isOp<ComparissonOp>(e))
        {
          for (auto & q : qvits[invNum])
          {
            if (containsOp<ARRAY_TY>(e) && !emptyIntersect(q->iter, e))
            {
              e = replaceAll(e, q->iter, q->qv);
              e = renameCand(e, ruleManager.chcs[tmp[0]].dstVars, invNum);

              // workaround: it works only during bootstrapping now
              arrCands[invNum].insert(e);
            }
          }
        }
      }
    }

    bool getCandForAdjacentRel(Expr candToProp, Expr constraint, Expr relFrom, ExprVector& varsRenameFrom, Expr rel, bool seed, bool fwd)
    {
      if (!containsOp<FORALL>(candToProp) && !containsOp<EXISTS>(candToProp))
        return true;;

      int invNum = getVarIndex(rel, decls);
      int invNumFrom = getVarIndex(relFrom, decls);

      if (containsOp<FORALL>(candToProp))
      {
        // GF: not tested for backward propagation
        if (fwd == false) return true;
        bool fin = finalizeArrCand(candToProp, constraint, invNumFrom);   // enlarge range, if possible
        auto candsTmp = candidates;
        Expr newCand = getForallCnj(eliminateQuantifiers(
                          mk<AND>(candToProp, constraint), varsRenameFrom, invNum, false));
        newCand = actualizeQV(invNum, newCand);
        if (newCand == NULL) return true;
        candidates[invNum].push_back(newCand);

        bool res = checkCand(invNum, false) &&
                   propagateRec(rel, conjoin(candidates[invNum], m_efac), false);

        if (fin && isOpX<FORALL>(candToProp))
        {
          assert (isOpX<FORALL>(candToProp));
          Expr qvar = getQvit(invNumFrom, bind::fapp(candToProp->arg(0)))->qv; // TODO: support nested loops with several qvars

          addPropagatedArrayCands(rel, invNum, candToProp);  // unclear, why?
          if (!candidates[invNum].empty()) return res;

          for (auto & q : qvits[invNum])            // generate regress candidates
          {
            Expr newCand1 = replaceAll(newCand, qvar->last(), q->qv->last());
            ExprVector newCands;
            replaceArrRangeForIndCheck(invNum, newCand1, newCands, /* regress == */ true);
            if (newCands.empty()) continue;
            newCand1 = newCands[0]; // TODO: extend
            candidates = candsTmp;
            candidates[invNum].push_back(newCand1);
            if (checkCand(invNum, false) &&
                propagateRec(rel, conjoin(candidates[invNum], m_efac), false))
              res = true;
          }
        }
        return res;
      }

      Expr newCand = eliminateQuantifiers(
                     (fwd ? mk<AND>(candToProp, constraint) :
                            mk<AND>(constraint, mkNeg(candToProp))),
                                    varsRenameFrom, invNum, !fwd);
      ExprSet cnjs;
      getConj(newCand, cnjs);
      addCandidates(rel, cnjs);

      if (seed)
      {
        if (u.isTrue(newCand)) return true;
        else return propagateRec(rel, newCand, true);
      }
      else
      {
        checkCand(invNum, false);
        return propagateRec(rel, conjoin(candidates[invNum], m_efac), false);
      }
    }

    bool addCandidate(int invNum, Expr cnd)
    {
      if (printLog >= 3) outs () << "Adding candidate [" << invNum << "]: " << cnd << "\n";
      SamplFactory& sf = sfs[invNum].back();
      Expr allLemmas = sf.getAllLemmas();
      if (!sf.gf.initialized && (containsOp<FORALL>(cnd) || containsOp<FORALL>(allLemmas)))
      {
        /*if (containsOp<FORALL>(cnd))
          replaceArrRangeForIndCheck (invNum, cnd, candidates[invNum]);
        else*/
          candidates[invNum].push_back(cnd);
        return true;
      }
      if (!isOpX<TRUE>(allLemmas) && u.implies(allLemmas, cnd)) return false;

      for (auto & a : candidates[invNum])
        if (a == cnd) return false;
      candidates[invNum].push_back(cnd);
      return true;
    }

    void addCandidates(Expr rel, ExprSet& cands)
    {
      int invNum = getVarIndex(rel, decls);
      for (auto & a : cands) addCandidate(invNum, a);
    }

    bool finalizeArrCand(Expr& cand, Expr constraint, int invNum)
    {
      // only forward currently
      if (!isOpX<FORALL>(cand)) return false;
      if (!containsOp<ARRAY_TY>(cand)) return false;

      Expr invs = conjoin(sfs[invNum].back().learnedExprs, m_efac);
      ExprSet cnj;
      for (int i = 0; i < cand->arity()-1; i++)
      {
        Expr qv = bind::fapp(cand->arg(i));
        auto q = getQvit(invNum, qv);
        if (u.isSat(invs, replaceAll(q->range, q->qv, q->iter), constraint))
          return false;
        else
          getConj(q->range, cnj);
      }

      cand = replaceAll(cand, cand->last()->left(), conjoin(cnj, m_efac));
      return true;
    }

    Expr getForallCnj (Expr cands)
    {
      ExprSet cnj;
      getConj(cands, cnj);
      for (auto & cand : cnj)
        if (isOpX<FORALL>(cand)) return cand;
      return NULL;
    }

    void replaceArrRangeForIndCheck(int invNum, Expr cand, ExprVector& cands, bool regress = false)
    {
      assert(isOpX<FORALL>(cand));
      ExprSet cnjs;
      getConj(cand->last()->left(), cnjs);
      for (int i = 0; i < cand->arity()-1; i++)
      {
        auto qv = bind::fapp(cand->arg(i));
        auto str = getQvit(invNum, qv);
        if (!str)
        {
          cands.push_back(simplifyQuants(cand));
          continue;
        }
        Expr itRepl = str->iter;

        if (contains(cand->last()->right(), qv))
        {
          if      (str->grows && regress)   cnjs.insert(mk<GEQ>(qv, itRepl));
          else if (str->grows && !regress)  cnjs.insert(mk<LT>(qv, itRepl));
          else if (!str->grows && regress)  cnjs.insert(mk<LEQ>(qv, itRepl));
          else if (!str->grows && !regress) cnjs.insert(mk<GT>(qv, itRepl));
        }
        else
        {
          for (auto it = cnjs.begin(); it != cnjs.end(); )
          {
            if (contains (*it, qv)) it = cnjs.erase(it);
            else ++it;
          }
        }
        cands.push_back(simplifyQuants(
          replaceAll(cand, cand->last()->left(), conjoin(cnjs, m_efac))));
      }
    }

    bool propagate(int invNum, Expr cand, bool seed) { return propagate(decls[invNum], cand, seed); }

    bool propagate(Expr rel, Expr cand, bool seed)
    {
      propped.clear();
      checked.clear();
      return propagateRec(rel, cand, seed);
    }

    bool propagateRec(Expr rel, Expr cand, bool seed)
    {
      if (printLog >= 3) outs () << "     Propagate:   " << cand << "\n";
      bool res = true;
      checked.insert(rel);
      for (auto & hr : ruleManager.chcs)
      {
        if (hr.isQuery || hr.isFact) continue;

        // forward:
        if (hr.srcRelation == rel && find(propped.begin(), propped.end(), &hr) == propped.end())
        {
          if (hr.isInductive && ruleManager.hasArrays[rel]) continue;     // temporary workarond
          propped.insert(&hr);
          SamplFactory* sf1 = &sfs[getVarIndex(hr.srcRelation, decls)].back();
          Expr replCand = replaceAllRev(cand, sf1->lf.nonlinVars);
          replCand = replaceAll(replCand, ruleManager.invVars[rel], hr.srcVars);
          res = res && getCandForAdjacentRel (replCand, hr.body, rel, hr.dstVars, hr.dstRelation, seed, true);
        }
        // backward (very similarly):
        if (hr.dstRelation == rel && find(propped.begin(), propped.end(), &hr) == propped.end())
        {
          propped.insert(&hr);
          SamplFactory* sf2 = &sfs[getVarIndex(hr.dstRelation, decls)].back();
          Expr replCand = replaceAllRev(cand, sf2->lf.nonlinVars);
          replCand = replaceAll(replCand, ruleManager.invVars[rel], hr.dstVars);
          res = res && getCandForAdjacentRel (replCand, hr.body, rel, hr.srcVars, hr.srcRelation, seed, false);
        }
      }
      return res;
    }

    bool checkCand(int invNum, bool propa = true)
    {
      Expr rel = decls[invNum];
      if (!checkInit(rel)) return false;
      if (!checkInductiveness(rel)) return false;

      return !propa || propagate(invNum, conjoin(candidates[invNum], m_efac), false);
    }

    void addLemma (int invNum, SamplFactory& sf, Expr l)
    {
      if (printLog)
        outs () << "Added lemma for " << decls[invNum] << ": " << l
                << (printLog >= 2 ? " 👍\n" : "\n");
      sf.learnedExprs.insert(l);
      Expr inv = decls[invNum];
      if (_grvFailed.count(inv))
        _grvCache.erase(inv);
    }

    // a simple method to generate properties of a larger Array range, given already proven ranges
    void generalizeArrInvars (int invNum, SamplFactory& sf)
    {
      if (sf.learnedExprs.size() > 1)
      {
        ExprVector posts;
        ExprVector pres;
        map<Expr, ExprVector> tmp;
        ExprVector tmpVars;
        ExprVector arrVars;
        Expr tmpIt = bind::intConst(mkTerm<string> ("_tmp_it", m_efac));

        for (auto & a : sf.learnedExprs)
        {
          if (!isOpX<FORALL>(a)) continue;
          Expr learnedQF = a->last();
          if (!isOpX<IMPL>(learnedQF)) continue;
          ExprVector se;
          filter (learnedQF->last(), bind::IsSelect (), inserter(se, se.begin()));

          // filter out accesses via non-quantified indeces
          for (auto it = se.begin(); it != se.end();)
          {
            bool found = false;
            for (int i = 0; i < a->arity(); i++)
            {
              if (contains(se[0]->right(), a->arg(0)))
              {
                found = true;
                break;
              }
            }
            if (!found) it = se.erase(it);
            else ++it;
          }

          bool multipleIndex = false;
          for (auto & s : se) {
            if (se[0]->right() != s->right()) {
              multipleIndex = true;
              break;
            }
          }

          if (se.size() == 0 || multipleIndex) continue;

          if (arrVars.size() == 0) {
            for (int index = 0; index < se.size(); index++) {
              arrVars.push_back(se[index]->left());
              tmpVars.push_back(cloneVar(se[index],
                mkTerm<string> ("_tmp_var_" + lexical_cast<string>(index), m_efac)));
            }
          } else {
            bool toContinue = false;
            for (auto & s : se) {
              if (find(arrVars.begin(), arrVars.end(), s->left()) == arrVars.end()) {
                toContinue = true;
                break;
              }
            }
            if (toContinue) continue;
          }

          Expr postReplaced = learnedQF->right();
          for (int index = 0; index < se.size() && index < tmpVars.size(); index++)
          postReplaced = replaceAll(postReplaced, se[index], tmpVars[index]);

          ExprSet postVars;
          filter(postReplaced, bind::IsConst(), inserter(postVars, postVars.begin()));
          for (auto & fhVar : sf.lf.getVars()) postVars.erase(fhVar);
          for (auto & tmpVar : tmpVars) postVars.erase(tmpVar);
          if (postVars.size() > 0)
          {
            AeValSolver ae(mk<TRUE>(m_efac), mk<AND>(postReplaced, mk<EQ>(tmpIt, se[0]->right())), postVars);
            if (ae.solve())
            {
              Expr pr = ae.getValidSubset();
              ExprSet conjs;
              getConj(pr, conjs);
              if (conjs.size() > 1)
              {
                for (auto conjItr = conjs.begin(); conjItr != conjs.end();)
                {
                  ExprVector conjVars;
                  filter (*conjItr, bind::IsConst(), inserter(conjVars, conjVars.begin()));
                  bool found = false;
                  for (auto & conjVar : conjVars)
                  {
                    if (find (tmpVars.begin(), tmpVars.end(), conjVar) != tmpVars.end())
                    {
                      found = true;
                      break;
                    }
                  }
                  if (!found) conjItr = conjs.erase(conjItr);
                  else ++conjItr;
                }
              }
              postReplaced = conjoin(conjs, m_efac);
            }
          }

          pres.push_back(learnedQF->left());
          posts.push_back(postReplaced);
          tmp[mk<AND>(learnedQF->left(), postReplaced)].push_back(se[0]->right());
        }

        if (tmp.size() > 0)
        {
          for (auto & a : tmp)
          {
            if (a.second.size() > 1)
            {
              Expr disjIts = mk<FALSE>(m_efac);
              for (auto & b : a.second) disjIts = mk<OR>(disjIts, mk<EQ>(tmpIt, b));
              ExprSet elementaryIts;
              filter (disjIts, bind::IsConst (), inserter(elementaryIts, elementaryIts.begin()));
              for (auto & a : sf.lf.getVars()) elementaryIts.erase(a);
              elementaryIts.erase(tmpIt);
              if (elementaryIts.size() == 1)
              {
                AeValSolver ae(mk<TRUE>(m_efac), mk<AND>(disjIts, a.first->left()), elementaryIts);
                if (ae.solve())
                {
                  ExprVector args;
                  Expr it = *elementaryIts.begin();
                  args.push_back(it->left());
                  Expr newPre = replaceAll(ae.getValidSubset(), tmpIt, it);
                  Expr newPost = replaceAll(a.first->right(), tmpIt, it);
                  for (int index = 0; index < tmpVars.size() && index < arrVars.size(); index++)
                  newPost = replaceAll(newPost, tmpVars[index], mk<SELECT>(arrVars[index], it));
                  args.push_back(mk<IMPL>(newPre, newPost));
                  Expr newCand = mknary<FORALL>(args);
                  addLemma(invNum, sf, newCand);
                }
              }
            }
          }
        }
      }
    }

    void assignPrioritiesForLearned()
    {
      for (auto & cand : candidates)
      {
        if (cand.second.size() > 0)
        {
          SamplFactory& sf = sfs[cand.first].back();
          for (auto b : cand.second)
          {
            b = simplifyArithm(b);
            if (!statsInitialized || containsOp<ARRAY_TY>(b)
                    || containsOp<BOOL_TY>(b) || findNonlin(b))
              addLemma(cand.first, sf, b);
            else
            {
              Expr learnedCand = normalizeDisj(b, invarVarsShort[cand.first]);
              Sampl& s = sf.exprToSampl(learnedCand);
              sf.assignPrioritiesForLearned();
              if (!u.implies(sf.getAllLemmas(), learnedCand))
                addLemma(cand.first, sf, learnedCand);
            }
          }
        }
      }
    }

    void deferredPriorities()
    {
      for (auto & dcl: ruleManager.wtoDecls)
      {
        int invNum = getVarIndex(dcl, decls);
        SamplFactory& sf = sfs[invNum].back();
        for (auto & l : sf.learnedExprs)
        {
          if (containsOp<ARRAY_TY>(l) || findNonlin(l) || containsOp<BOOL_TY>(l)) continue;
          Expr learnedCand = normalizeDisj(l, invarVarsShort[invNum]);
          Sampl& s = sf.exprToSampl(learnedCand);
          sf.assignPrioritiesForLearned();
        }
        for (auto & failedCand : tmpFailed[invNum])
        {
          Sampl& s = sf.exprToSampl(failedCand);
          sf.assignPrioritiesForFailed();
        }
      }
    }

    Expr mkImplCnd(Expr pre, Expr cand, bool out = true)
    {
      Expr im;
      if (isOpX<FORALL>(cand))
      {
        Expr tmp = cand->last()->right();
        im = replaceAll(cand, tmp, mkImplCnd(pre, tmp, false));
      }
      else im = simplifyBool(mk<IMPL>(pre, cand));
      if (printLog >= 2 && out)
        outs () << "  - - - Implication candidate: " << im << (printLog >= 3 ? " 😎\n" : "\n");
      return im;
    }

    Expr mkImplCndArr(int invNum, Expr mbp, Expr cnd)
    {
      Expr phaseGuardArr = mbp;
      for (auto & q : qvits[invNum])
        if (contains(cnd, q->qv))
          phaseGuardArr = replaceAll(phaseGuardArr, q->iter, q->qv);
      return mkImplCnd(phaseGuardArr, cnd);
    }

    void internalArrProp(int invNum, Expr ind, Expr mbp, Expr cnd, ExprSet& s, ExprVector& srcVars, ExprVector& dstVars, ExprSet& cands)
    {
      // mainly, one strategy for `--phase-prop`:
      //   * finalizing a candidate for the next phase,
      //   * reusing the regressing one at the next-next phase
      // TODO: support the reasoning for `--phase-data`
      Expr nextMbp = getMbpPost(invNum, conjoin(s, m_efac), srcVars, dstVars);
      Expr newCand = cnd;
      if (!finalizeArrCand(newCand, nextMbp, invNum)) return;
      newCand = getForallCnj(eliminateQuantifiers(
            mk<AND>(newCand, u.simplifiedAnd(ssas[invNum], nextMbp)),
            dstVars, invNum, false));
      if (newCand == NULL) return;
      getConj(newCand, cands);
      ExprVector newCands;
      replaceArrRangeForIndCheck(invNum, newCand, newCands, /* regress == */ true);
      for (auto & newCand : newCands)
      {
        s = {ind, u.simplifiedAnd(ssas[invNum], nextMbp),
            mkNeg(replaceAll(mk<AND>(ind, mbp), srcVars, dstVars))};
        nextMbp = getMbpPost(invNum, conjoin(s, m_efac), srcVars, dstVars);
        Expr candImplReg = mkImplCndArr(invNum, nextMbp, newCand);
        candidates[invNum].push_back(candImplReg);
      }
    }

    void generatePhaseLemmas(int invNum, int cycleNum, Expr rel, Expr cnd)
    {
      if (printLog >= 2) outs () << "  Try finding phase guard for " << cnd << "\n";
      if (isOpX<FORALL>(cnd))
      {
        ExprVector cnds;
        replaceArrRangeForIndCheck (invNum, cnd, cnds);
        if (cnds.empty()) return;
        cnd = cnds[0];              // TODO: extend
      }

      vector<int>& cycle = ruleManager.cycles[cycleNum];
      ExprVector& srcVars = ruleManager.chcs[cycle[0]].srcVars;
      ExprVector& dstVars = ruleManager.chcs[cycle.back()].dstVars;

      vector<pair<int, int>> cur_mbps;
      int curDepth = dFwd ? INT_MAX : 0;
      int curPath = -1;
      int curLen = INT_MAX;

      for (int p = 0; p < mbpDt[invNum].paths.size(); p++)
      {
        auto &path = mbpDt[invNum].paths[p];
        for (int m = 0; m < path.size(); m++)
        {
          Expr fla = mk<AND>(mbpDt[invNum].getExpr(p,m), strenDt[invNum].getExpr(p,m));
          if (m == 0)
            if (!u.implies(mk<AND>(prefs[invNum], fla), cnd))     // initialization filtering
              continue;
          ExprVector filt = {mbpDt[invNum].getExpr(p, m),
                            strenDt[invNum].getExpr(p ,m), ssas[invNum], cnd};
          if (!u.isSat(filt))                                     // consecution filtering
            continue;
          if (m + 1 < path.size())                                // lookahead-based filtering
          {
            filt.push_back(replaceAll(mk<AND>(mbpDt[invNum].getExpr(p, m + 1),
                              strenDt[invNum].getExpr(p, m + 1)), srcVars, dstVars));
            if (!u.isSat(filt))
              continue;
          }

          if (dAllMbp) cur_mbps.push_back({p, m});    // TODO: try other heuristics
          else if (((dFwd && m <= curDepth) || (!dFwd && m >= curDepth)) &&
                    curLen > path.size())
          {
            curPath = p;
            curDepth = m;
            curLen = path.size();
            if (printLog >= 3)
              outs () << "      updating MBP path/depth since its has an earlier occurrence: ["
                      << curPath << ", " << curDepth << "] and shorter path\n";
          }
        }
      }

      if (!dAllMbp && curPath >= 0) cur_mbps.push_back({curPath, curDepth});

      if (printLog >= 2)
      {
        outs () << "  Total " << cur_mbps.size() << " MBP paths to try: ";
        for (auto & c : cur_mbps) outs () << "[ " << c.first << ", " << c.second  << " ], ";
        outs () << "\n";
      }
      for (auto & c : cur_mbps)
        for (bool dir : {true, false})
          if (dFwd == dir || dFwd == 2)
            annotateDT(invNum, rel, cnd, c.first, c.second, srcVars, dstVars, dir);
    }

    void annotateDT(int invNum, Expr rel, Expr cnd,
                    int path, int depth, ExprVector& srcVars, ExprVector& dstVars, bool fwd)
    {
      if (!mbpDt[invNum].isValid(path, depth))
      {
        if (dRecycleCands)
          deferredCandidates[invNum].push_back(cnd);
        return;
      }
      Expr mbp = mbpDt[invNum].getExpr(path, depth);
      if (mbp == NULL) return;

      if (printLog >= 2)
        outs () << "    Next ordered MBP: " << mbp << " [ " << path << ", " << depth << " ] in "
                << (fwd ? "forward" : "backward") << " direction\n";

      Expr ind = strenDt[invNum].getExpr(path, depth);
      if (printLog >= 2)
        outs () << "    Strengthening the guard [ " << path << ", " << depth << " ] with " << ind << "\n";

      Expr candImpl;
      if (isOpX<FORALL>(cnd)) candImpl = mkImplCndArr(invNum, mbp, cnd);
      else candImpl = mkImplCnd(mk<AND>(ind, mbp), cnd);

      if (find(candidates[invNum].begin(), candidates[invNum].end(), candImpl) != candidates[invNum].end()) return;
      candidates[invNum].push_back(candImpl);

      // use multiHoudini here to give more chances to array candidates
      auto candidatesTmp = candidates;
      if (multiHoudini(ruleManager.dwtoCHCs)) assignPrioritiesForLearned();
      else if (dAllMbp) candidates = candidatesTmp;
      else return;

      Expr nextMbp;
      if (fwd)
      {
        if (mbpDt[invNum].isLast(path, depth))
        {
          ExprSet tmp;
          mbpDt[invNum].getBackExprs(path, depth, tmp);
          if (tmp.empty())  return;
          nextMbp = disjoin(tmp, m_efac);
        }
        else nextMbp = mbpDt[invNum].getExpr(path, depth + 1);
      }
      else
      {
        if (depth == 0) return;
        nextMbp = mbpDt[invNum].getExpr(path, depth - 1);
      }

      // GF: if `dFwd == 2` then there is a lot of redundancy
      //     since all the code above this point runs twice
      //     TODO: refactor

      map<Expr, ExprSet> cands;
      if (dAddProp)
      {
        ExprSet s;
        if (fwd) s = { ind, u.simplifiedAnd(ssas[invNum], mbp), replaceAll(nextMbp, srcVars, dstVars) };
        else s = { nextMbp, u.simplifiedAnd(ssas[invNum], mbp), ind };

        Expr newCand;

        if (isOpX<FORALL>(cnd))          // Arrays are supported only if `fwd == 1`; TODO: support more
          internalArrProp(invNum, ind, mbp, cnd, s, srcVars, dstVars, cands[rel]);
        else
        {
          if (fwd)
          {
            s.insert(cnd);                                             // the cand to propagate
            newCand = keepQuantifiers (conjoin(s, m_efac), dstVars);   // (using standard QE)
            newCand = weakenForHardVars(newCand, dstVars);
            newCand = replaceAll(newCand, dstVars, srcVars);
            newCand = u.removeITE(newCand);
          }
          else
          {
            s.insert(replaceAll(cnd, srcVars, dstVars));                       // the cand to propagate
            newCand = keepQuantifiers (mkNeg (conjoin(s, m_efac)), srcVars);   // (using abduction)
            newCand = weakenForHardVars(newCand, srcVars);
            newCand = mkNeg(newCand);
            newCand = u.removeITE(newCand);
          }

          if (isOpX<FALSE>(newCand)) return;
          getConj(newCand, cands[rel]);
          if (printLog >= 3)
            outs () << "  Phase propagation gave " << cands[rel].size() << " candidates to try\n";
        }
      }

      ExprSet qfInvs, candsToDat; //, se;
      if (dAddDat)
      {
        candsToDat = { mbp, ind };
        int sz = cands[rel].size();
//          getArrInds(ssas[invNum], se);
        if (isOpX<FORALL>(cnd))
          qfInvs.insert(cnd->right()->right());              // only the actual inv without the phaseGuard/mbp
        else
          candsToDat.insert(candImpl);
        for (auto & inv : sfs[invNum].back().learnedExprs)   // basically, invs
          if (isOpX<FORALL>(inv))
            qfInvs.insert(inv->right()->right());
          else
            candsToDat.insert(inv);
//          for (auto & s : se)
        {
          Expr candToDat = conjoin(qfInvs, m_efac);
          for (auto & q : qvits[invNum])
            candToDat = replaceAll(candToDat, q->qv, q->iter); // s);
          ExprVector vars2keep;
          for (int i = 0; i < srcVars.size(); i++)
            if (containsOp<ARRAY_TY>(srcVars[i]))
              candToDat = replaceAll(candToDat, srcVars[i], dstVars[i]);
          candsToDat.insert(candToDat);
        }
        getDataCandidates(cands, rel, nextMbp, conjoin(candsToDat, m_efac), fwd);
        if (printLog >= 3)
          outs () << "  Data Learning gave " << (cands[rel].size() - sz)
                  << (dAddProp? "additional " : "") << " candidates to try\n";
      }

      // postprocess behavioral arrCands
      if (ruleManager.hasArrays[rel])
      {
        for (auto &a : arrCands[invNum])
        {
          ExprSet tmpCands;
          getQVcands(invNum, a, tmpCands);
          for (Expr arrCand : tmpCands)
            cands[rel].insert(sfs[invNum].back().af.getQCand(arrCand));
        }
        arrCands[invNum].clear();
      }

      for (auto c : cands[rel])
      {
        if (isOpX<FORALL>(cnd) && !isOpX<FORALL>(c)) continue;
        if (isOpX<FORALL>(c))
        {
          ExprVector cnds;
          replaceArrRangeForIndCheck (invNum, c, cnds);
          if (cnds.empty()) continue;
          c = cnds[0]; //  to extend
        }

        annotateDT(invNum, rel, c, path, depth + (fwd ? 1 : -1), srcVars, dstVars, fwd);
      }
    }

    vector<deque<Expr>> sfBuffer;
    bool synthesize(unsigned maxAttempts)
    {
      if (printLog) outs () << "\nSAMPLING\n========\n";
      if (printLog >= 3)

        for (auto & a : deferredCandidates)
          for (auto & b : a.second)
            outs () << "  Deferred cand for " << a.first << ": " << b << "\n";

      for (int i = 0; i < bootCands.size(); ++i)
      {
        sfs[i].back().gf.addDeferredCands(bootCands[i]);
        bootCands[i].clear();
      }

      sfBuffer.resize(invNumber);

      map<int, int> defSz;
      for (auto & a : deferredCandidates)
      {
        defSz[a.first] = a.second.size();
        maxAttempts += a.second.size();
      }
      ExprSet cands;
      bool rndStarted = false;
      for (int i = 0; i < maxAttempts; i++)
      {
        // next cand (to be sampled)
        // TODO: find a smarter way to calculate; make parametrizable
        int cycleNum = i % ruleManager.cycles.size();
        int tmp = ruleManager.cycles[cycleNum][0];
        Expr rel = ruleManager.chcs[tmp].srcRelation;
        int invNum = getVarIndex(rel, decls);
        candidates.clear();
        SamplFactory& sf = sfs[invNum].back();
        Expr cand;
        //if (deferredCandidates[invNum].empty())
        {
          if (alternver >= 0 && !sf.gf.initialized)
            break;
          rndStarted = true;
          if (sfBuffer[invNum].empty() && !sf.isdone())
          {
            Expr tmpcand = sf.getFreshCandidate();
            if (tmpcand)
            {
              if (alternver >= 0)
              {
                auto newbuf = alternMatchQuants(tmpcand, invNum);
                //auto newbuf = alternTryQuants(tmpcand, invNum);
                sfBuffer[invNum].insert(sfBuffer[invNum].end(),
                  newbuf.begin(), newbuf.end());
              }
              else
                sfBuffer[invNum].push_back(tmpcand);
            }
          }
          if (!sfBuffer[invNum].empty())
          {
            cand = sfBuffer[invNum].front();
            sfBuffer[invNum].pop_front();
          }
          if (cand != NULL && !sf.gf.initialized && isOpX<FORALL>(cand) && isOpX<IMPL>(cand->last()))
          {
            if (!u.isSat(cand->last()->left())) cand = NULL;
          }
        }
        /*else
        {
          cand = deferredCandidates[invNum].back();
          deferredCandidates[invNum].pop_back();
          if (cand != NULL && isOpX<FORALL>(cand) && isOpX<IMPL>(cand->last()))
          {
            if (!u.isSat(cand->last()->left())) cand = NULL;
          }
        }*/
        bool alldone = true;
        for (int j = 0; j < sfs.size(); ++j)
          if (!sfs[j].back().isdone())
          {
            alldone = false;
            break;
          }
        if (cand == NULL)
        {
          if (alldone) break;
          else continue;
        }
        if (printLog >= 3 && dRecycleCands)
          outs () << "Number of deferred candidates: " << deferredCandidates[invNum].size() << "\n";

        if (rndStarted) cand = getStrenOrWeak(cand, invNum, strenOrWeak);

        if (printLog)
        {
          outs () << " - - - Sampled cand (#" << i << ") for " << decls[invNum] << ": ";
          u.print(cand); outs() << (printLog >= 3 ? " 😎\n" : "\n");
        }
        if (!addCandidate(invNum, cand)) continue;

        bool lemma_found = checkCand(invNum, alternver < 0);
        if (dDisj && (isOp<ComparissonOp>(cand) || isOpX<FORALL>(cand)))
        {
          lemma_found = true;
          generatePhaseLemmas(invNum, cycleNum, rel, cand); //, mk<TRUE>(m_efac));
          multiHoudini(ruleManager.dwtoCHCs);
          if (printLog) outs () << "\n";
        }
        if (lemma_found)
        {
          assignPrioritiesForLearned();
          generalizeArrInvars(invNum, sf);

          /*for (const Expr& newpost : newposts)
          {
            if (!addCandidate(queryInvNum, newpost))
              continue;
            if (checkCand(invNum, alternver < 0))
              assignPrioritiesForLearned();
          }*/

          if (checkAllLemmas())
          {
            outs () << "Success after " << (i+1) << " iterations "
                    << (rndStarted ? "(+ rnd)" :
                       (i > defSz[invNum]) ? "(+ rec)" : "" ) << "\n";

            for (int j = 0; j < invNumber; j++)
              sfs[j].back().gf.finish(true);

            printSolution();
            return true;
          }
        }
        else if (printLog)
          outs() << " - - - bad cand\n";
      }
      for (int j = 0; j < invNumber; j++)
        sfs[j].back().gf.finish(false);
      outs() << "unknown\n";
      return false;
    }

    bool filterUnsat()
    {
      vector<HornRuleExt*> worklist;
      for (int i = 0; i < invNumber; i++)
      {
        if (!u.isSat(candidates[i]))
        {
          for (auto & hr : ruleManager.chcs)
          {
            if (hr.dstRelation == decls[i]) worklist.push_back(&hr);
          }
        }
      }

      // basically, just checks initiation and immediately removes bad candidates
      multiHoudini(worklist, false);

      for (int i = 0; i < invNumber; i++)
      {
        if (!u.isSat(candidates[i]))
        {
          ExprVector cur;
          deque<Expr> def;
          u.splitUnsatSets(candidates[i], cur, def);
          deferredCandidates[i] = def;
          candidates[i] = cur;
        }
      }

      return true;
    }

    bool weaken (int invNum, ExprVector& ev,
                 map<Expr, tribool>& vals, HornRuleExt* hr, SamplFactory& sf,
                 function<bool(Expr, tribool)> cond, bool singleCand)
    {
      bool weakened = false;
      for (auto it = ev.begin(); it != ev.end(); )
      {
        if (cond(*it, vals[*it]))
        {
          weakened = true;
          if (printLog >= 3) outs () << "    Failed cand for " << hr->dstRelation << ": " << *it << " 🔥\n";
          if (hr->isFact && !containsOp<ARRAY_TY>(*it) && !containsOp<BOOL_TY>(*it) && !findNonlin(*it))
          {
            Expr failedCand = normalizeDisj(*it, invarVarsShort[invNum]);
            if (statsInitialized)
            {
              Sampl& s = sf.exprToSampl(failedCand);
              sf.assignPrioritiesForFailed();
            }
            else tmpFailed[invNum].insert(failedCand);
          }
          if (boot)
          {
            if (isOpX<EQ>(*it)) deferredCandidates[invNum].push_back(*it);  //  prioritize equalities
            else
            {
              ExprSet disjs;
              getDisj(*it, disjs);
              for (auto & a : disjs)  // to try in the `--disj` mode
              {
                deferredCandidates[invNum].push_front(a);
                deferredCandidates[invNum].push_front(mkNeg(a));
              }
            }
          }
          it = ev.erase(it);
          if (singleCand) break;
        }
        else
        {
          ++it;
        }
      }
      return weakened;
    }

    bool multiHoudini (vector<HornRuleExt*> worklist, bool recur = true)
    {
      if (printLog >= 3) outs () << "MultiHoudini\n";
      if (printLog >= 4) printCands();

      bool res1 = true;
      for (auto &hr: worklist)
      {
        if (printLog >= 3) outs () << "  Doing CHC check (" << hr->srcRelation << " -> "
                                   << hr->dstRelation << ")\n";
        if (hr->isQuery) continue;
        tribool b = checkCHC(*hr, candidates);
        if (b || indeterminate(b))
        {
          if (printLog >= 3) outs () << "    CHC check failed with " <<
            (indeterminate(b) ? "unknown" : "sat") << "\n";
          int invNum = getVarIndex(hr->dstRelation, decls);
          SamplFactory& sf = sfs[invNum].back();

          map<Expr, tribool> vals;
          for (auto & cand : candidates[invNum])
          {
            Expr repl = cand;
            repl = replaceAll(repl, invarVarsShort[invNum], hr->dstVars);
            vals[cand] = u.eval(repl);
          }

          // first try to remove candidates immediately by their models (i.e., vals)
          // then, invalidate (one-by-one) all candidates for which Z3 failed to find a model

          for (int i = 0; i < 3 /*weakeningPriorities.size() */; i++)
            if (weaken (invNum, candidates[invNum], vals, hr, sf, (*weakeningPriorities[i]), i > 0))
              break;

          if (recur)
          {
            res1 = false;
            break;
          }
        }
        else if (printLog >= 3) outs () << "    CHC check succeeded\n";
      }

      bool haveCand = false;
      for (const auto& kv : candidates)
        for (const auto& cand : kv.second)
        {
          haveCand = true;
          break;
        }
      if (!haveCand)
        return false;

      if (!recur) return false;
      if (res1) return anyProgress(worklist);
      else return multiHoudini(worklist);
    }

    bool findInvVar(int invNum, Expr expr, ExprVector& ve)
    {
      ExprSet vars;
      filter (expr, bind::IsConst (), inserter(vars, vars.begin()));

      for (auto & v : vars)
      {
        bool found = false;
        for (auto & w : invarVars[invNum])
        {
          if (w.second == v)
          {
            found = true;
            break;
          }
        }
        if (!found)
        {
          if (find (ve.begin(), ve.end(), v) == ve.end())
            return false;
        }
      }
      return true;
    }

    void generateMbps(int invNum, Expr ssa, ExprVector& srcVars, ExprVector& dstVars, ExprSet& cands)
    {
      ExprVector vars2keep, prjcts, prjcts1, prjcts2;
      bool hasArray = false;
      for (int i = 0; i < srcVars.size(); i++)
        if (containsOp<ARRAY_TY>(srcVars[i]))
        {
          hasArray = true;
          vars2keep.push_back(dstVars[i]);
        }
        else
          vars2keep.push_back(srcVars[i]);

      if (mbpEqs != 0)
        u.flatten(ssa, prjcts1, false, vars2keep, keepQuantifiersRepl);
      if (mbpEqs != 1)
        u.flatten(ssa, prjcts2, true, vars2keep, keepQuantifiersRepl);

      prjcts1.insert(prjcts1.end(), prjcts2.begin(), prjcts2.end());
      for (auto p : prjcts1)
      {
        if (hasArray)
        {
          getConj(replaceAll(p, dstVars, srcVars), cands);
          p = ufo::eliminateQuantifiers(p, dstVars);
          p = weakenForVars(p, dstVars);
        }
        else
        {
          p = weakenForVars(p, dstVars);
          p = simplifyArithm(normalize(p));
          getConj(p, cands);
        }
        prjcts.push_back(p);
        if (printLog >= 2) outs() << "Generated MBP: " << p << "\n";
      }

      // heuristics: to optimize the range computation
      u.removeRedundantConjunctsVec(prjcts);

      for (auto p = prjcts.begin(); p != prjcts.end(); )
      {
        if (!u.isSat(prefs[invNum], *p) &&
            !u.isSat(mkNeg(*p), ssa, replaceAll(*p, srcVars, dstVars)))
            {
              if (printLog >= 3)
                outs() << "Erasing MBP: " << *p << " since it is negatively inductive\n";
              p = prjcts.erase(p);
            }
        else ++p;
      }

      if (hasArray)
      {
        u.removeRedundantDisjuncts(prjcts); // for array benchmarks, since more expensive
        if (prjcts.empty()) prjcts.push_back(mk<TRUE>(m_efac)); // otherwise, ranges won't be computed
      }

      for (auto p : prjcts)
        mbps[invNum].insert(simplifyArithm(p));
    }

    Expr getMbpPost(int invNum, Expr ssa, ExprVector& srcVars, ExprVector& dstVars)
    {
      // currently, a workaround
      // it ignores the pointers to mbps at qvits and sortedMbps
      // TODO: support
      ExprSet cur_mbps;
      for (auto & m : mbps[invNum])
        if (u.isSat(replaceAll(m, srcVars, dstVars), ssa))
          cur_mbps.insert(m);
      return simplifyBool(distribDisjoin(cur_mbps, m_efac));
    }

    // overloaded, called from `prepareSeeds`
    void doSeedMining(Expr invRel, ExprSet& cands, set<cpp_int>& progConsts, set<cpp_int>& intCoefs)
    {
      int invNum = getVarIndex(invRel, decls);
      ExprVector& srcVars = ruleManager.invVars[invRel];
      ExprVector& dstVars = ruleManager.invVarsPrime[invRel];

      SamplFactory& sf = sfs[invNum].back();
      ExprSet candsFromCode, tmpArrCands;
      bool analyzedExtras, isFalse, hasArrays = false;

      for (auto &hr : ruleManager.chcs)
      {
        if (hr.dstRelation != invRel && hr.srcRelation != invRel) continue;
        SeedMiner sm (hr, invRel, invarVars[invNum], sf.lf.nonlinVars);

        // limit only to queries:
        if (hr.isQuery) sm.analyzeCode();
        if (!analyzedExtras && hr.srcRelation == invRel)
        {
          sm.analyzeExtras (cands);
          sm.analyzeExtras (arrCands[invNum]);
          analyzedExtras = true;
        }
        for (auto &cand : sm.candidates) candsFromCode.insert(cand);
        for (auto &a : sm.intConsts) progConsts.insert(a);
        for (auto &a : sm.intCoefs) intCoefs.insert(a);

        // for arrays
        if (ruleManager.hasArrays[invRel])
        {
          tmpArrCands.insert(sm.arrCands.begin(), sm.arrCands.end());
          hasArrays = true;
        }
      }

      if (hasArrays)
      {
        ExprSet repCands;
        for (auto & a : arrCands[invNum])
        {
          getQVcands(invNum, a, repCands);
        }
        for (auto & a : candidates[invNum])
        {
          getQVcands(invNum, a, repCands);
        }
        arrCands[invNum] = repCands;


        // TODO: revisit handling of nested loops
        /*
        auto nested = ruleManager.getNestedRel(invRel);
        if (nested != NULL)
        {
          int invNumTo = getVarIndex(nested->dstRelation, decls);
          // only one variable for now. TBD: find if we need more
          Expr q->qv2 = bind::intConst(mkTerm<string> ("_FH_nst_it", m_efac));
          arrAccessVars[invNum].push_back(q->qv2);

          Expr range = conjoin (arrIterRanges[invNumTo], m_efac);
          ExprSet quantified;
          for (int i = 0; i < nested->dstVars.size(); i++)
          {
            range = replaceAll(range, invarVars[invNumTo][i], nested->dstVars[i]);
            quantified.insert(nested->dstVars[i]);
          }

          // naive propagation of ranges
          ExprSet cnjs;
          getConj(nested->body, cnjs);
          for (auto & a : cnjs)
          {
            for (auto & b : quantified)
            {
              if (isOpX<EQ>(a) && a->right() == b)
              {
                range = replaceAll(range, b, a->left());
              }
              else if (isOpX<EQ>(a) && a->left() == b)
              {
                range = replaceAll(range, b, a->right());
              }
            }
          }
          arrIterRanges[invNum].insert(
                replaceAll(replaceAll(range, *arrAccessVars[invNumTo].begin(), q->qv2),
                      q->iter, *arrAccessVars[invNum].begin()));
        }
         */

        // process all quantified seeds
        for (auto & a : tmpArrCands)
        {
          if (u.isTrue(a) || u.isFalse(a)) continue;
          Expr replCand = replaceAllRev(a, sf.lf.nonlinVars);
          ExprSet allVars;
          getExtraVars(replCand, srcVars, allVars);
          if (allVars.size() < 2 &&
              !u.isTrue(replCand) && !u.isFalse(replCand))
          {
            if (allVars.empty())
            {
              candsFromCode.insert(replCand);
              getQVcands(invNum, replCand, arrCands[invNum]);
            }
            else if (allVars.size() > 1)
              continue;      // to extend to larger allVars
            else
              for (auto & q : qvits[invNum])
                if (typeOf(*allVars.begin()) == typeOf(q->qv))
                  arrCands[invNum].insert(
                    replaceAll(replCand, *allVars.begin(), q->qv));
          }
        }
      }

      // process all quantifier-free seeds
      cands = candsFromCode;
      for (auto & cand : candsFromCode)
      {
        Expr replCand = replaceAllRev(cand, sf.lf.nonlinVars);
        addCandidate(invNum, replCand);
      }
    }

    void getQVcands(int invNum, Expr replCand, ExprSet& cands)
    {
      ExprVector se;
      //filter (replCand, bind::IsSelect (), inserter(se, se.begin()));
      bool found = false;
      for (auto & q : qvits[invNum])
      {
        if (!contains(replCand, q->iter)) continue;
        found = true;
        Expr replTmp = replaceAll(replCand, q->iter, q->qv);
        cands.insert(replCand);
        getQVcands(invNum, replTmp, cands);
      }
      if (!found)
      {
        cands.insert(replCand);
      }
    }

    bool anyProgress(vector<HornRuleExt*> worklist)
    {
      for (int i = 0; i < invNumber; i++)
        // subsumption check
        for (auto & hr : worklist)
          if (hr->srcRelation == decls[i] || hr->dstRelation == decls[i])
            if (candidates[i].size() > 0)
              if (!u.implies (conjoin(sfs[i].back().learnedExprs, m_efac),
                conjoin(candidates[i], m_efac)))
                  return true;
      return false;
    }

    void getArrRanges(map<Expr, ExprVector>& m)
    {
      for (auto & dcl : decls)
        for (auto q : qvits[getVarIndex(dcl, decls)])
          if (q->closed)
            m[dcl].push_back (q->grows ?
              mk<MINUS>(q->postcond, q->precond) :
              mk<MINUS>(q->precond, q->postcond));
    }

    void filterTrivCands(ExprSet& tmp) // heuristics for incremental DL
    {
      for (auto it = tmp.begin(); it != tmp.end();)
        if (u.hasOneModel(*it))
          it = tmp.erase(it);
        else
          ++it;
    }

    void addDataCand(int invNum, Expr cand, ExprSet& cands)
    {
      int sz = allDataCands[invNum].size();
      allDataCands[invNum].insert(cand);
      bool isNew = allDataCands[invNum].size() > sz;
      if (isNew || dGenerous)
      {
        cands.insert(cand);                               // actually, add it
        if (dRecycleCands && !boot && isOpX<EQ>(cand))     // also, add weaker versions of cand to deferred set
        {
          deferredCandidates[invNum].push_front(mk<GEQ>(cand->left(), cand->right()));
          deferredCandidates[invNum].push_front(mk<LEQ>(cand->left(), cand->right()));
        }
      }
      else if (printLog >= 3 && !boot)
        outs () << "    Data candidate " << cand << " has already appeared before\n";
    }

    void getDataCandidates(map<Expr, ExprSet>& cands, Expr srcRel = NULL,
                           Expr phaseGuard = NULL, Expr invs = NULL, bool fwd = true){
#ifdef HAVE_ARMADILLO
      if (printLog && phaseGuard == NULL) outs () << "\nDATA LEARNING\n=============\n";
      if (phaseGuard == NULL) assert(invs == NULL && srcRel == NULL);
      DataLearner dl(ruleManager, m_z3, to, printLog);
      vector<map<Expr, ExprSet>> poly;
      if (phaseGuard == NULL)
      {
        // run at the beginning, compute data once
        map<Expr, ExprVector> m;
        //getArrRanges(m);
        map<Expr, ExprSet> constr;
        Expr etrue = mk<TRUE>(m_efac);
        // T: Look at a[i] and a'[i].    F: Look at a'[i] only.
        bool dounp = hasconstarr || invNumber > 1;
        for (int i = 0; i < dat; i++)
        {
          if (!dl.computeData(m, constr)) break;
          poly.push_back(map<Expr, ExprSet>());
          for (int dclnum = 0; dclnum < decls.size(); ++dclnum)
          {
            auto& dcl = decls[dclnum];
            Expr lms = sfs[dclnum].back().getAllLemmas();

            Expr loopbody = mk<TRUE>(m_efac);
            for (const auto& chc : ruleManager.chcs)
              if (chc.isInductive && chc.srcRelation == dcl)
                loopbody = chc.body; // Won't really work if >1 inductive CHC.

            ExprMap sels;
            //ExprMap unp_sels; // Selectors with primed vars removed.
            ExprUSet excl_sels; // Set of arrays not to disable selectors for.
            auto arrranges = getRangesAll(dcl);
            for (auto& kv : arrranges)
            {
              for (Expr& rng : kv.second)
              {
                // Order important: Doing latter replaceAll here would also
                //   replace kv.first.
                rng = replaceAll(rng,
                  ruleManager.invVars[dcl], ruleManager.invVarsPrime[dcl]);
                // Figure out what the expression in the select is and
                //   use it instead of rngvar.
                int i;
                for (i = 0; i < dl.invVarsUnp[dcl].size() && ruleManager.invVars[dcl][i] != kv.first; ++i);
                rng = replaceAll(rng,
                  rngvars[kv.first], dl.invVarsUnp[dcl][i]->right());
              }
              Expr arrsel = conjoin(kv.second, m_efac);
              sels[kv.first] = arrsel;
              // Not sure if this works well enough, disabled for now.
              /*Expr unp_arrsel = ufo::eliminateQuantifiers(
                mk<AND>(loopbody, arrsel), ruleManager.invVarsPrime[dcl]);
              if (!isOpX<FALSE>(unp_arrsel))
                unp_sels[kv.first] = unp_arrsel;*/
              if (!u.isSat(loopbody, lms, mkNeg(arrsel)))
                excl_sels.insert(kv.first);
            }
            ExprVector selsv(sels.size());
            //ExprVector unp_selsv(sels.size());
            auto permbody = [&] (Expr forcesel = NULL)
            {
              int i = 0;
              for (const auto& kv : sels)
              {
                selsv[i] = kv.second;
                /*if (unp_sels.count(kv.first) != 0)
                  unp_selsv[i] = unp_sels[kv.first];
                else
                  unp_selsv[i] = etrue; // TODO: Do something else here.
                */
                ++i;
              }
              Expr conjsels = simplifyBool(simplifyArithm(conjoin(selsv, m_efac)));
              conjsels = forcesel ? forcesel : conjsels;
              if (!u.isSat(loopbody, lms, conjsels)) return;

              ExprSet tmp;
              dl.computePolynomials(dcl, tmp, conjsels, dounp);
              simplify(tmp);
              poly.back()[dcl].insert(tmp.begin(), tmp.end());
              /*for (const Expr& t : tmp)
                poly.back()[dcl].insert(
                  isOpX<TRUE>(conjsels) ? t : mk<IMPL>(conjsels, t));*/
              filterTrivCands(tmp);
              constr[dcl].insert(conjoin(tmp, m_efac));
            };
            std::function<void(ExprMap::iterator)> perm =
              [&] (ExprMap::iterator key)
            {
              if (key == sels.end())
              {
                permbody();
                return;
              }
              else if (excl_sels.count(key->first) != 0)
                return perm(++key);

              auto nextkey = key;
              nextkey++;
              Expr selsbody = key->second;
              perm(nextkey);
              key->second = etrue;
              perm(nextkey);
              key->second = selsbody;
            };
            perm(sels.begin());
            permbody(mk<TRUE>(m_efac));
          }
        }
      }
      else
      {
        // run at `generatePhaseLemmas`
        for (auto & dcl : decls)
        {
          if (srcRel == dcl)
          {
            ExprSet constr;
            for (int i = 0; i < dat; i++)
            {
              poly.push_back(map<Expr, ExprSet>());
              dl.computeDataSplit(srcRel, phaseGuard, invs, fwd, constr);
              ExprSet tmp;
              dl.computePolynomials(dcl, tmp, NULL);
              simplify(tmp);
              poly.back()[dcl] = tmp;
              filterTrivCands(tmp);
              constr.insert(conjoin(tmp, m_efac));
            }
            break;
          }
        }
      }

      if (mut < 2)
      {
        for (auto & c : poly)
          for (auto & p : c)
            for (Expr a : p.second)
            {
              int invNum = getVarIndex(p.first, decls);
              if (containsOp<ARRAY_TY>(a))
                getQVcands(invNum, a, cands[p.first]);
              else
                addDataCand(invNum, a, cands[p.first]);
            }
        return;
      }

      // mutations (if specified)
      for (auto & c : poly)
        for (auto & p : c)
        {
          int invNum = getVarIndex(p.first, decls);
          ExprSet tmp, its;
          if (mut == 3 && ruleManager.hasArrays[p.first])   // heuristic to remove cands irrelevant to counters and arrays
          {
            for (auto q : qvits[invNum]) its.insert(q->iter);
            for (auto it = p.second.begin(); it != p.second.end();)
              if (emptyIntersect(*it, its))
                it = p.second.erase(it);
              else
                ++it;
          }
          mutateHeuristicEq(p.second, tmp, p.first, (phaseGuard == NULL));
          for (auto & c : tmp)
            //addDataCand(invNum, c, cands[p.first]);
            if (containsOp<ARRAY_TY>(c))
              getQVcands(invNum, c, cands[p.first]);
            else
              addDataCand(invNum, c, cands[p.first]);
        }
#else
      if (printLog) outs() << "Skipping learning from data as required library (armadillo) not found\n";
#endif
    }

    void mutateHeuristicEq(ExprSet& src, ExprSet& dst, Expr dcl, bool toProp)
    {
      int invNum = getVarIndex(dcl, decls);
      ExprSet src2;
      map<Expr, ExprVector> substs;
      Expr (*ops[])(Expr, Expr) = {mk<PLUS>, mk<MINUS>};        // operators used in the mutations

      for (auto a1 = src.begin(); a1 != src.end(); ++a1)
        if (isNumericEq(*a1))
        {
          for (auto a2 = std::next(a1); a2 != src.end(); ++a2)    // create various combinations
            if (isNumericEq(*a2))
            {
              const ExprVector m1[] = {{(*a1)->left(), (*a2)->left()}, {(*a1)->left(), (*a2)->right()}};
              const ExprVector m2[] = {{(*a1)->right(), (*a2)->right()}, {(*a1)->right(), (*a2)->left()}};
              for (int i : {0, 1})
                for (Expr (*op) (Expr, Expr) : ops)
                  src2.insert(simplifyArithm(normalize(
                    mk<EQ>((*op)(m1[i][0], m1[i][1]), (*op)(m2[i][0], m2[i][1])))));
            }

          // before pushing them to the cand set, we do some small mutations to get rid of specific consts
          Expr a = simplifyArithm(normalize(*a1));
          if (isOpX<EQ>(a) && isIntConst(a->left()) &&
              isNumericConst(a->right()) && lexical_cast<string>(a->right()) != "0")
            substs[a->right()].push_back(a->left());
          src2.insert(a);
        }

      bool printedAny = false;
      if (printLog >= 2) outs () << "Mutated candidates:\n";
      for (auto a : src2)
        if (!u.isFalse(a) && !u.isTrue(a))
        {
          if (printLog >= 2) { outs () << "  " << a << "\n"; printedAny = true; }

          dst.insert(a);

          if (isNumericConst(a->right()))
          {
            cpp_int i1 = lexical_cast<cpp_int>(a->right());
            for (auto b : substs)
            {
              cpp_int i2 = lexical_cast<cpp_int>(b.first);

              if (i1 % i2 == 0 && i1/i2 != 0)
                for (auto c : b.second)
                {
                  Expr e = simplifyArithm(normalize(mk<EQ>(a->left(), mk<MULT>(mkMPZ(i1/i2, m_efac), c))));
                  if (!u.isSat(mk<NEG>(e))) continue;
                  dst.insert(e);

                  if (printLog >= 2) { outs () << "  " << e << "\n"; printedAny = true; }
                }
            }
          }
        }
      if (printLog >= 2 && !printedAny) outs () << "  none\n";
    }

    int hasRange(Expr e)
    {
      assert(isOpX<AND>(e->last()));
      assert(e->arity() == 2);
      Expr qvar = e->left();
      int rangecount = 0;
      for (int i = 0; i < e->last()->arity(); ++i)
      {
        Expr arg = e->last()->arg(i);
        if (isOp<ComparissonOp>(arg) && (arg->left() == qvar || arg->right() == qvar))
          rangecount++;
      }
      return rangecount;
    }

#if 0
    // Returns all arrays selected by 'qvar' in 'fullpost'
    ExprSet getArrsAccessed(Expr fullpost, Expr qvar)
    {
      ExprSet range_arrs;
      function<bool(Expr)> findqvar = [&] (Expr e) -> bool {
        if (e == qvar)
          return true;
        if (isOpX<SELECT>(e))
        {
          if (findqvar(e->right()))
          {
            range_arrs.insert(e);
            return true;
          }
          return findqvar(e->left());
        }
        bool ret = false;
        for (int i = 0; i < e->arity(); ++i)
          ret |= findqvar(e->arg(i));
        return ret;
      };
      //filter(fullpost, [&] (Expr e) { return isOpX<SELECT>(e) && e->last() == qvar; }, inserter(range_arrs, range_arrs.begin()));
      findqvar(fullpost);
      if (range_arrs.size() == 0)
      {
        outs() << "Bootstrap: Couldn't find array accessed by " << qvar << endl;
        return std::move(range_arrs);
      }

      // Fix up range_arrs to be just the arrays
      ExprSet ret;
      for (const Expr& e : range_arrs) ret.insert(e->first());

      return std::move(ret);
    }
#endif

    // K: Array, V: Full new range(s)
    typedef unordered_map<Expr,ExprVector> GetRangesRet;
    ExprUMap rngvars; // K: Array Variable, V: Range Variable
    unordered_map<Expr,GetRangesRet> _grvCache;
    unordered_set<Expr> _grvFailed;
    // Returns { array var -> (progress range, regress range, full range) }
    //   for the given relation, for all array variables in that relation.
    //
    // Regress and full ranges only returned if program contains a
    //   const array or multiple loops.
    //
    // Ranges returned are in terms of rngvars[array variable].
    GetRangesRet getRangesAll(Expr inv)
    {
      bool saveret = true;
      int invNum = getVarIndex(inv, decls);
      assert(invNum >= 0);
      const auto &key = inv;
      if (_grvCache.count(key) != 0)
        return _grvCache.at(key);

      GetRangesRet out;

      bool doupper = hasconstarr || invNumber > 1;

      ExprUSet range_arrs_set;
      /*auto arrfilter = [&] (const Expr &e) -> bool
      {return IsConst()(e) && isOpX<ARRAY_TY>(typeOf(e));};*/

      // Find the array the range is using
      Expr loopbody, initbody, querybody, initrel;
      ExprSet loopLocVars;
      RW<function<Expr(Expr)>> quantrw(new function<Expr(Expr)>([&] (const Expr& e)
      {
        if (!isOpX<FORALL>(e))
          return e;
        return mk<TRUE>(m_efac);
      }));

      for (const Expr& v : ruleManager.invVars[inv])
        if (isOpX<ARRAY_TY>(typeOf(v)))
          range_arrs_set.insert(v);

      for (const auto& chc : ruleManager.chcs)
      {
        if (chc.isInductive && chc.srcRelation == inv)
        {
          /*filter(chc.body, arrfilter,
            inserter(range_arrs_set, range_arrs_set.begin()));*/
          loopLocVars.insert(chc.dstVars.begin(), chc.dstVars.end());
          Expr tmpbody = dagVisit(quantrw, chc.body);
          if (loopbody)
            loopbody = mk<OR>(loopbody, tmpbody);
          else
            loopbody = tmpbody;
        }
        else if (chc.dstRelation == inv && chc.srcRelation != inv)
        {
          if (initbody)
          {
            cout << "Bootstrap: Multi-init CHCs not supported for "<<inv<<endl;
            if (saveret) _grvCache[key] = out;
            return std::move(out);
          }
          /*filter(chc.body, arrfilter,
            inserter(range_arrs_set, range_arrs_set.begin()));*/
          initbody = replaceAll(chc.body, chc.dstVars, invarVarsShort[invNum]);
          if (!chc.isFact)
            initbody = ufo::eliminateQuantifiers(initbody, chc.srcVars);
          initrel = chc.srcRelation;
        }
        else if (chc.srcRelation == inv && chc.dstRelation != inv)
        {
          if (querybody)
          {
            cout << "Bootstrap: Multi-query CHCs not supported for "<<inv<<endl;
            if (saveret) _grvCache[key] = out;
            return std::move(out);
          }
          /*filter(chc.body, arrfilter,
            inserter(range_arrs_set, range_arrs_set.begin()));*/
          querybody = ufo::eliminateQuantifiers(chc.body, chc.dstVars);
          querybody = ufo::eliminateQuantifiers(querybody, chc.locVars);
          querybody = removeVars(querybody, chc.locVars);
          if (querybody)
            querybody = simplifyBool(querybody);
        }
      }

      if (range_arrs_set.size() == 0)
      {
        if (saveret) _grvCache[key] = out;
        return std::move(out);
      }
      ExprVector range_arrs(range_arrs_set.begin(), range_arrs_set.end());

      if (alternver < 3)
      {
        vector<ExprSet> alliters(range_arrs.size(), ExprSet());
        for (int i = 0; i < range_arrs.size(); ++i)
          filter(loopbody, [&] (Expr e) {
              return (isOpX<SELECT>(e) || isOpX<STORE>(e)) &&
              isSameArray(e->left(), range_arrs[i]); },
            inserter(alliters[i], alliters[i].begin()));

        for (int i = 0; i < alliters.size(); ++i)
        {
          if (alliters[i].size() > 1)
          {
            cout << "Bootstrap: altern-ver < 3 doesn't support multi-index accesses" << endl;
            //out[range_arrs[i]].push_back(NULL);
            continue;
          }
          if (alliters[i].size() == 0)
            continue;
          Expr iter = (*alliters[i].begin())->right();
          Expr boundhi = iter, boundlo = iter;
          // Still need to find the minimum
          ExprVector itervars;
          ExprUSet allunprimed(invarVarsShort[invNum].begin(),
              invarVarsShort[invNum].end());
          filter(iter, [&](Expr e){return allunprimed.count(e) != 0;},
              inserter(itervars, itervars.begin()));
          if (itervars.size() > 1)
          {
            cout << "Bootstrap: altern-ver < 3 doesn't support multi-var indices" << endl;
            //out[range_arrs[i]].push_back(NULL);
            continue;
          }
          Expr var = itervars[0];
          ExprVector _qvars{var};
          Expr tmpvar = mkConst(mkTerm<string>("tmpqevar", m_efac), typeOf(var));
          Expr varlo_def = ufo::eliminateQuantifiers(
            mk<AND>(initbody, mk<EQ>(tmpvar, var)),
            _qvars);
          if (isOpX<FALSE>(varlo_def))
          {
            cout << "Bootstrap: Failed to find definition for "<<var<<endl;
            if (saveret) _grvCache[key] = out;
            return std::move(out);
          }
          // Assumes eliminateQuantifiers keeps the structure the same
          if (isOpX<AND>(varlo_def))
            varlo_def = varlo_def->last();
          if (isOpX<EQ>(varlo_def) && varlo_def->left() == tmpvar)
            varlo_def = varlo_def->right();
          else
            varlo_def = var; // Non-deterministic variable
          boundlo = replaceAll(boundlo, var, varlo_def);
          boundlo = simplifyArithm(simplifyArithm(boundlo));

          Expr arrtype = typeOf(range_arrs[i])->left();
          if (rngvars.count(range_arrs[i]) == 0)
            rngvars[range_arrs[i]] =
              mkConst(mkTerm(string("rngvar"), arrtype->efac()), arrtype);
          Expr qvar = rngvars[range_arrs[i]];

          out[range_arrs[i]].push_back(mk<AND>(mk<GEQ>(qvar, boundlo), mk<LT>(qvar, boundhi)));
        }
        if (saveret) _grvCache[key] = out;
        return std::move(out);
      }


      ExprVector disjs, vars;
      u.flatten(loopbody, disjs, false, vars,
        [](Expr e, const ExprVector& v){return e;});

      ExprVector ranges(range_arrs.size());
      ExprVector regranges(range_arrs.size());
      ExprVector fullranges(range_arrs.size());
      ExprVector constranges(range_arrs.size());
      for (const auto& loopbody : disjs)
      {
        // Find the all points the array has been written to (for progress)
        vector<ExprSet> alliters(range_arrs.size(), ExprSet());
        vector<ExprSet> alliters_init(range_arrs.size(), ExprSet());
        for (int i = 0; i < range_arrs.size(); ++i) {
          filter(loopbody, [&] (Expr e) {
              return (isOpX<SELECT>(e) || isOpX<STORE>(e)) &&
              isSameArray(e->left(), range_arrs[i]); },
            inserter(alliters[i], alliters[i].begin()));
          filter(initbody, [&] (Expr e) {
              return (isOpX<SELECT>(e) || isOpX<STORE>(e)) &&
              isSameArray(e->left(), range_arrs[i]); },
            inserter(alliters_init[i], alliters_init[i].begin()));
        }

        ExprSet allprimed(ruleManager.invVarsPrime[inv].begin(),
            ruleManager.invVarsPrime[inv].end());
        ExprSet allunprimed(invarVarsShort[invNum].begin(),
            invarVarsShort[invNum].end());
        Expr noprimed_body = ufo::eliminateQuantifiers(loopbody, allprimed);
        Expr lms = sfs[invNum].back().getAllLemmas();
        for (int i = 0; i < alliters.size(); ++i)
        {
          if (alliters[i].size() == 0) continue;
          for (const Expr& iter : alliters[i])
          {
            ExprVector varsp;
            Expr tmpiter = iter->right();
            do
            {
              varsp.clear();
              filter(tmpiter, [&](Expr e){return allprimed.count(e) != 0;},
                  inserter(varsp, varsp.begin()));
              for (const Expr& vp : varsp)
              {
                Expr tmpvar = mkConst(mkTerm<string>("tmpqevar", m_efac), typeOf(vp));
                ExprVector _qvars{vp};
                Expr def = ufo::eliminateQuantifiers(
                  mk<AND>(loopbody, mk<EQ>(tmpvar, vp)),
                  _qvars);
                if (isOpX<FALSE>(def))
                {
                  cout << "Bootstrap: Failed to find definition for "<<vp<<endl;
                  if (saveret) _grvCache[key] = out;
                  return std::move(out);
                }
                if (isOpX<AND>(def))
                  def = def->last();
                def = def->right();
                tmpiter = replaceAll(tmpiter, vp, def);
              }
            } while (varsp.size() != 0);

            ExprVector itervars;
            filter(tmpiter, [&](Expr e){return allunprimed.count(e) != 0;},
                inserter(itervars, itervars.begin()));
            if (itervars.size() == 0)
            {
              filter(tmpiter, [&](Expr e){return loopLocVars.count(e) != 0;},
                  inserter(itervars, itervars.begin()));
              if (itervars.size() == 0)
                continue;
              Expr _newrange = ufo::eliminateQuantifiers(loopbody, allprimed);
              ExprVector newrange(_newrange->args_begin(), _newrange->args_end());
              for (auto itr = newrange.begin(); itr != newrange.end();)
              {
                for (const Expr& var : itervars)
                  if (contains(*itr, var))
                  {
                    ++itr;
                    continue;
                  }
                itr = newrange.erase(itr);
              }
              if (ranges[i])
                ranges[i] = mk<OR>(ranges[i], conjoin(newrange, m_efac));
              else
                ranges[i] = conjoin(newrange, m_efac);
              continue;
            }
            Expr boundlo = tmpiter, boundhi = tmpiter;
            for (const Expr& var : itervars)
            {
              if (isOpX<ARRAY_TY>(typeOf(var)))
                continue;
              Expr varp = replaceAll(var, invarVarsShort[invNum], ruleManager.invVarsPrime[inv]);
              Expr tmpvar = mkConst(mkTerm<string>("tmpqevar", m_efac), typeOf(var));
              Expr tmpvar2 = mkConst(mkTerm<string>("tmpqevar2", m_efac), typeOf(var));
              ExprVector _qvars{varp};
              // Get max
              Expr varp_def = ufo::eliminateQuantifiers(
                mk<AND>(loopbody, mk<EQ>(tmpvar, varp)),
                _qvars, false, true, false);
              if (isOpX<FALSE>(varp_def))
              {
                cout<<"Bootstrap: Failed to find definition for "<<varp<<endl;
                if (saveret) _grvCache[key] = out;
                return std::move(out);
              }
              // Assumes eliminateQuantifiers keeps the structure the same
              if (isOpX<AND>(varp_def))
                varp_def = varp_def->last();
              if (isOpX<EQ>(varp_def) && varp_def->left() == tmpvar)
                varp_def = varp_def->right();
              else if (isOpX<EQ>(varp_def) && isOpX<UN_MINUS>(varp_def->left()) && varp_def->left()->left() == tmpvar)
                varp_def = mk<UN_MINUS>(varp_def->right());
              else
                varp_def = varp; // TODO: Might not be correct
              Expr inddiff = mk<MINUS>(varp_def, var);
              Expr indbound = mk<MINUS>(var, inddiff);

              // Get min
              _qvars[0] = var;
              Expr varlo_def = ufo::eliminateQuantifiers(
                mk<AND>(initbody, mk<EQ>(tmpvar, var)),
                _qvars);
              if (isOpX<FALSE>(varlo_def))
              {
                cout << "Bootstrap: Failed to find definition for "<<var<<endl;
                if (saveret) _grvCache[key] = out;
                return std::move(out);
              }
              // Assumes eliminateQuantifiers keeps the structure the same
              if (isOpX<AND>(varlo_def))
                varlo_def = varlo_def->last();
              if (isOpX<EQ>(varlo_def) && varlo_def->left() == tmpvar)
                varlo_def = varlo_def->right();
              else if (isOpX<EQ>(varlo_def) && isOpX<UN_MINUS>(varlo_def->left()) && varlo_def->left()->left() == tmpvar)
                varlo_def = mk<UN_MINUS>(varlo_def->right());
              else
                varlo_def = var; // Non-deterministic variable

              // We have benchmarks which start with i = 1, so search for
              // a select/store in Init and use that index if < 
              Expr initbound = varlo_def;

              for (const Expr& iter_init : alliters_init[i])
              {
                Expr ltcand = mk<LEQ>(iter_init->right(), initbound);
                /*candidates[0].clear();
                candidates[0].push_back(ltcand);
                if (multiHoudini(ruleManager.dwtoCHCs))*/
                if (u.isTrue(ltcand))
                  initbound = iter_init->right();
              }

              boundlo = replaceAll(boundlo, var, initbound);
              boundlo = simplifyArithm(simplifyArithm(boundlo));
              boundhi = replaceAll(boundhi, var, indbound);
              boundhi = simplifyArithm(simplifyArithm(boundhi));

              /*if (!isOpX<AND>(varup_qe))
                varup_qe = mk<AND>(varup_qe);
              Expr varup_def = NULL;
              for (const Expr& conj : *varup_qe)
              {
                if (isOpX<GEQ>(conj) && conj->left() == tmpvar)
                {
                  varup_def = conj->right();
                  break;
                }
                else if (isOpX<LEQ>(conj) && conj->right() == tmpvar)
                {
                  varup_def = conj->left();
                  break;
                }
              }
              if (!varup_def)
              {
                cout << "Bootstrap: Unrecognized upper bound form: "<<varup_qe<<endl;
                continue;
              }
              boundup = replaceAll(boundup, var, varup_def);
              boundup = simplifyArithm(simplifyArithm(boundup));*/
              //_qvars.push_back(varp);
              //_qvars[0] = tmpvar;
              /*Expr varup_def = ufo::eliminateQuantifiers(
                mk<AND>(loopbody, mk<GT>(tmpvar, var)), _qvars);*/
              /*Expr varupabd = cvcAbduce(mk<GT>(tmpvar, var), noprimed_body);
              if (varupabd)
              {
                if (isOpX<EQ>(varupabd) || isOpX<LEQ>(varupabd) || isOpX<GEQ>(varupabd))
                {
                  Expr varup_def = ufo::eliminateQuantifiers(
                    mk<AND>(varupabd, mk<EQ>(tmpvar2, tmpvar)), _qvars);
                  if (isOpX<AND>(varup_def))
                    varup_def = varup_def->last();
                  assert(isOpX<EQ>(varup_def) || isOpX<LEQ>(varup_def) || isOpX<GEQ>(varup_def));
                  if (varup_def->left() == tmpvar2)
                    varup_def = varup_def->right();
                  else if (varup_def->right() == tmpvar2)
                    varup_def = varup_def->left();
                  else
                    cout << "Bootstrap: Unrecognized abduction form post-qe: "<<varup_def<<endl;
                }
                else
                  cout << "Bootstrap: Unrecognized abduction form: "<<varupabd<<endl;
              }
              else
                cout << "Bootstrap: Failed to abduce variable: "<<var<<endl;*/
            }

            Expr pboundlo = replaceAll(boundlo,
              ruleManager.invVars[inv], ruleManager.invVarsPrime[inv]);

            Expr pboundhi = replaceAll(boundhi,
              ruleManager.invVars[inv], ruleManager.invVarsPrime[inv]);

            Expr lhs = mk<AND>(lms, loopbody);
            bool orderingFailed = false;
            bool rev = false;

            if (!u.isSat(lhs, mk<LT>(pboundhi, boundlo)))
            {}
            else if (!u.isSat(lhs, mk<GT>(pboundhi, boundlo)))
            { swap(boundhi, boundlo); rev = true; }
            else
            {
              cout << "Bootstrap: Failed to order bounds: l=" << boundlo
                << ", h=" << boundhi << endl;
              _grvFailed.insert(inv);
              orderingFailed = true;
            }

            Expr arrtype = typeOf(range_arrs[i])->left();
            if (rngvars.count(range_arrs[i]) == 0)
              rngvars[range_arrs[i]] =
                mkConst(mkTerm(string("rngvar"), arrtype->efac()), arrtype);
            Expr qvar = rngvars[range_arrs[i]];

            Expr boundup = NULL;
            if (doupper)
            {
              if (!rev)
                boundup = ufo::eliminateQuantifiers(
                  mk<AND>(mkNeg(noprimed_body), mk<GEQ>(qvar, tmpiter)),
                  itervars);
              else
                boundup = ufo::eliminateQuantifiers(
                  mk<AND>(mkNeg(noprimed_body), mk<LEQ>(qvar, tmpiter)),
                  itervars);
            }
            if (doupper && (!boundup || isOpX<FALSE>(boundup)))
              cout << "Bootstrap: Failed to find upper for "<<tmpiter<<endl;
            else if (boundup)
              boundup = mkNeg(boundup);

            Expr newrange = orderingFailed ? mk<TRUE>(m_efac) :
              mk<AND>(mk<GEQ>(qvar, boundlo), mk<LEQ>(qvar, boundhi));
            /*if (u.implies(lms, mk<LEQ>(boundlo, boundhi)))
              newrange = mk<AND>(mk<GEQ>(qvar, boundlo), mk<LEQ>(qvar, boundhi));
            else if (u.implies(lms, mk<LEQ>(boundhi, boundlo)))
            {
              swap(boundhi, boundlo);
              newrange = mk<AND>(mk<GEQ>(qvar, boundlo), mk<LEQ>(qvar, boundhi));
            }
            else if (u.isSat(mk<AND>(lms, newrange)))
              newrange = mk<AND>(mk<GEQ>(qvar, boundlo), mk<LEQ>(qvar, boundhi));
            else
            {
              swap(boundhi, boundlo);
              newrange = mk<AND>(mk<GT>(qvar, boundlo), mk<LT>(qvar, boundhi));
              if (u.isSat(mk<AND>(lms, newrange)))
                newrange = mk<AND>(mk<GEQ>(qvar, boundlo), mk<LEQ>(qvar, boundhi));
              else
              {
                swap(boundhi, boundlo);
                cout << "Bootstrap: Failed to order bounds: l=" << boundlo
                  << ", h=" << boundhi << endl;
                newrange = mk<AND>(mk<GEQ>(qvar, boundlo), mk<LEQ>(qvar, boundhi));
              }
            }*/

            if (ranges[i])
              ranges[i] = mk<OR>(ranges[i], newrange);
            else
              ranges[i] = newrange;

            if (boundup && doupper && !orderingFailed)
            {
              Expr newregrange;
              if (!rev)
                newregrange = mk<AND>(mk<GT>(qvar, boundhi), boundup);
              else
                newregrange = mk<AND>(mk<LT>(qvar, boundlo), boundup);

              if (regranges[i])
                regranges[i] = mk<OR>(regranges[i], newregrange);
              else
                regranges[i] = newregrange;

              Expr newfullrange =
                mk<AND>(mk<GEQ>(qvar, boundlo), boundup);

              if (fullranges[i])
                fullranges[i] = mk<OR>(fullranges[i], newfullrange);
              else
                fullranges[i] = newfullrange;
            }

            /*if (hasconstarr)
              if (constranges[i])
                constranges[i] = mk<OR>(constranges[i], mk<GT>(qvar, boundhi));
              else
                constranges[i] = mk<GT>(qvar, boundhi);*/
          }
        }
      }

      for (int i = 0; i < range_arrs.size(); ++i)
      {
        if (ranges[i])
          out[range_arrs[i]].push_back(simplifyBool(simplifyArithm(ranges[i])));
        if (regranges[i])
          out[range_arrs[i]].push_back(simplifyBool(simplifyArithm(regranges[i])));
        if (fullranges[i])
          out[range_arrs[i]].push_back(simplifyBool(simplifyArithm(fullranges[i])));
        /*if (constranges[i])
          out[range_arrs[i]].push_back(simplifyBool(simplifyArithm(constranges[i])));*/
      }

      if (saveret) _grvCache[key] = out;
      return std::move(out);
    }

    // Returns same as `getRangesAll`, but rewrites it so the inequalities
    //   are over `qvar` instead.
    GetRangesRet getRanges(Expr inv, Expr qvar)
    {
      auto out = getRangesAll(inv);
      for (auto& kv : out)
        for (Expr& rng : kv.second)
        {
          assert(typeOf(rngvars[kv.first]) == typeOf(qvar));
          rng = replaceAll(rng, rngvars[kv.first], qvar);
        }
      return std::move(out);
    }

#if 0
    // For 'mutateAERanges'
    Expr getSingleRange(Expr fullpost, Expr qvar, Expr inv)
    {
      GetRangesRet out = getRanges(fullpost, qvar, inv);
      ExprSet iters_set;
      for (const auto &arr_ranges : out)
        for (const Expr& range : arr_ranges.second)
        iters_set.insert(range);

      if (iters_set.size() > 1)
      {
        outs() << "Bootstrap: Multiple array accesses for same quantified variable currently unimplemented" << endl;
        return NULL;
      }
      else if (iters_set.size() == 1)
        return *iters_set.begin();

      outs() << "Bootstrap: Unable to find range for " << qvar << endl;
      return NULL;
    }

    Expr mutateAERanges(Expr fullpost, Expr post, ExprUMap& qvars,
      unordered_map<Expr,int>& hasrange, bool negContext)
    {
      ExprVector newargs;
      if (isLit(post) || isOpX<FDECL>(post))
        return post;

      for (int i = 0; i < post->arity(); ++i)
        newargs.push_back(NULL);

      if (isOpX<FORALL>(post) || isOpX<EXISTS>(post))
      {
        for (int i = 0; i < post->arity() - 1; ++i)
          qvars[mk<FAPP>(post->arg(i))] = post;
        newargs.back() = mutateAERanges(fullpost, post->last(), qvars, hasrange, false);
      }
      else if (isOp<ComparissonOp>(post))
      {
        // Annoying simplification of comparisons
        if (isOpX<PLUS>(post->left()) &&
        isOpX<MPZ>(post->right()) && getTerm<mpz_class>(post->right()) == 0)
          post = mk(post->op(), post->left()->left(), post->left()->right()->left());
        else if (isOpX<PLUS>(post->right()) &&
        isOpX<MPZ>(post->left()) && getTerm<mpz_class>(post->left()) == 0)
          post = mk(post->op(), post->right()->left(), post->right()->right()->left());
        // TODO: Better definition of ranges, detect 2 lower ranges?
        if (qvars.count(post->left())/* && (isOpX<FAPP>(post->right()) || isLit(post->right()))*/)
        {
          hasrange[post->left()] += 1;
          //if (!isLit(post->right())/*&&!isOpX<EXISTS>(qvars[post->left()])*/)
          //{
            Expr newrange = getSingleRange(fullpost, post->left());
            if (newrange)
            {
              if (!negContext)
                post = mk<AND>(post, newrange);
              else
                post = mk<OR>(post, mkNeg(newrange));
            }
          //}
        }
        else if (qvars.count(post->right())/* && (isOpX<FAPP>(post->left()) || isLit(post->left()))*/)
        {
          hasrange[post->right()] += 1;
          //if (!isLit(post->left())/*&&!isOpX<EXISTS>(qvars[post->right()])*/)
          //{
            Expr newrange = getSingleRange(fullpost, post->right());
            if (newrange)
            {
              if (!negContext)
                post = mk<AND>(post, newrange);
              else
                post = mk<OR>(post, mkNeg(newrange));
            }
          //}
        }

        return post;
      }

      if (isOpX<OR>(post))
        negContext = true;

      for (int i = 0; i < post->arity(); ++i)
        if (!newargs[i])
          newargs[i] = mutateAERanges(fullpost, post->arg(i), qvars, hasrange,
            negContext);

      return mknary(post->op(), newargs.begin(), newargs.end());
    }
#endif

    // 'var' should be FAPP
    inline bool quantHasVar(const Expr& quant, const Expr& var)
    {
      assert(isOpX<FORALL>(quant) || isOpX<EXISTS>(quant));
      for (int i = 0; i < quant->arity() - 1; ++i)
        if (quant->arg(i) == var->left())
          return true;
      return false;
    }

    // For each array given in `ranges`:
    //   Surrounds each `select` with the range corresponding to its
    //   array. We consider the smallest boolean expression containing
    //   the select, e.g. 'a[i] == 4 /\ x < 4' will only have the
    //   first conjunct covered with the range for 'a'.
    //   Implications, disjunctions, and varying quantifiers should
    //   be handled properly.
    // Returns <permutations of ranges, unused>.
    std::pair<ExprVector,Expr> coverWithRange(Expr e, const GetRangesRet& ranges,
      Expr qvar, bool isneg = false)
    {
      if (isLit(e) || isOpX<FDECL>(e))
        return make_pair(ExprVector{e}, Expr(NULL));

      if (isOpX<SELECT>(e) && e->right() == qvar)
        return make_pair(ExprVector{e}, e->left());

      vector<ExprVector> newargs;
      ExprVector ret;
      Expr foundsel = NULL;
      ExprVector foundsels;
      vector<bool> nestisneg(e->arity());
      bool apply = false;
      bool thisquant = false;
      for (int i = 0; i < e->arity(); ++i)
      {
        nestisneg[i] = isneg;
        if (isOpX<EXISTS>(e) && quantHasVar(e, qvar))
        {
          thisquant = true;
          nestisneg[i] = false;
        }
        else if (isOpX<FORALL>(e) && quantHasVar(e, qvar))
        {
          thisquant = true;
          nestisneg[i] = true;
        }
        else if (i == 0 && isOpX<IMPL>(e))
          nestisneg[i] = !isneg;

        auto argret = coverWithRange(e->arg(i), ranges, qvar, nestisneg[i]);
        if (argret.first.size() == 0)
            return make_pair(ExprVector(), Expr(NULL));
        if (argret.second)
        {
          if (ranges.count(argret.second) && !ranges.count(foundsel))
            foundsel = argret.second;
          if (foundsel && isOpX<BOOL_TY>(typeOf(e->arg(i))) &&
              ranges.count(foundsel) && ranges.count(argret.second) &&
              ranges.at(foundsel) != ranges.at(argret.second))
          {
            apply = true;
          }
        }
        foundsels.push_back(argret.second);
        newargs.push_back(argret.first);
      }
      if (!apply && (thisquant || isOpX<IMPL>(e))) apply = true;
      if (apply)
      {
        if (isOpX<IMPL>(e) && foundsels[1] && isneg)
        { foundsels[0] = foundsels[1]; foundsels[1] = NULL; }

        if (!isOpX<BOOL_TY>(typeOf(e)))
        {
          outs() << "Bootstrap: Can't synthesize range for multi-array comparison" << endl;
          return make_pair(ExprVector(), Expr(NULL));
        }
        for (int i = 0; i < newargs.size(); ++i)
        {
          if (foundsels[i])
          {
            ExprVector tmpargs;
            for (const Expr& na : newargs[i])
              for (const Expr& range : ranges.at(foundsels[i]))
                if (nestisneg[i])
                  tmpargs.push_back(mk<OR>(mkNeg(range), na));
                else
                  tmpargs.push_back(mk<AND>(range, na));
            assert(tmpargs.size() > 0);
            newargs[i] = tmpargs;
          }
        }
        foundsel = NULL;
      }

      ExprVector permargs(newargs.size());
      function<void(int)> perm = [&] (int pos)
      {
        if (pos == newargs.size())
          ret.push_back(mknary(e->op(), permargs.begin(), permargs.end()));
        else
        {
          for (const Expr& na : newargs[pos])
          {
            permargs[pos] = na;
            perm(pos + 1);
          }
        }
      };
      perm(0);

      /*if (isOpX<BOOL_TY>(typeOf(e)) && foundsel)
      {
        ExprVector newret;
        if (ranges.count(foundsel) != 0)
          for (const Expr& rete : ret)
            for (const Expr& range : ranges.at(foundsel))
              if (isneg)
                newret.push_back(mk<OR>(mkNeg(range), rete));
              else
                newret.push_back(mk<AND>(range, rete));

        // NULL because we already applied range
        return make_pair(std::move(newret), Expr(NULL));
      }*/
      return make_pair(std::move(ret), foundsel);
    }

    // Doesn't work
#if 0
    std::pair<ExprVector,Expr> addRanges(Expr e, const GetRangesRet& ranges, Expr qvar)
    {
      if (isLit(e) || isOpX<FDECL>(e))
        return make_pair(ExprVector{e}, Expr(NULL));

      if (isOpX<SELECT>(e) && e->right() == qvar)
        return make_pair(ExprVector{e}, e->left());

      vector<ExprVector> newargs;
      ExprVector ret;
      Expr foundsel = NULL;
      for (int i = 0; i < e->arity(); ++i)
      {
        auto argret = addRanges(e->arg(i), ranges, qvar);
        if (argret.first.size() == 0)
            return make_pair(ExprVector(), Expr(NULL));
        if (argret.second)
        {
          if (foundsel && ranges.count(foundsel) && ranges.count(argret.second) && ranges.at(foundsel) != ranges.at(argret.second))
          {
            outs() << "Bootstrap: Can't synthesize range for multi-array comparison" << endl;
            return make_pair(ExprVector(), Expr(NULL));
          }
          if (ranges.count(argret.second) && !ranges.count(foundsel))
            foundsel = argret.second;
        }
        newargs.push_back(argret.first);
      }

      bool addrange = false, conj = false;
      if (foundsel && ranges.count(foundsel) != 0)
      {
        if (isOpX<EXISTS>(e) && e->first() == qvar->left())
        { addrange = true; conj = true; }
        else if (isOpX<FORALL>(e) && e->first() == qvar->left())
        { addrange = true; conj = false; }
      }
      if (addrange)
      {
        ExprVector tmpnewargs;
        for (Expr& arg : newargs.back())
          for (const Expr& range : ranges.at(foundsel))
            if (conj)
              tmpnewargs.push_back(mk<AND>(range, arg));
            else
              tmpnewargs.push_back(mk<OR>(mkNeg(range), arg));
        newargs.back() = tmpnewargs;
      }

      ExprVector permargs(newargs.size());
      function<void(int)> perm = [&] (int pos)
      {
        if (pos == newargs.size())
          ret.push_back(mknary(e->op(), permargs.begin(), permargs.end()));
        else
        {
          for (const Expr& na : newargs[pos])
          {
            permargs[pos] = na;
            perm(pos + 1);
          }
        }
      };
      perm(0);

      if (addrange)
        // NULL because we already applied range
        return make_pair(std::move(ret), Expr(NULL));
      return make_pair(std::move(ret), foundsel);
    }
#endif

#if 0
    std::pair<ExprVector,Expr> weakenWithProp(Expr e, Expr prop,
      Expr qvar, bool isneg = false)
    {
      if (isLit(e) || isOpX<FDECL>(e))
        return make_pair(ExprVector{e}, Expr(NULL));

      if (isOpX<SELECT>(e) && e->right() == qvar)
        return make_pair(ExprVector{e}, e->left());

      vector<ExprVector> newargs;
      ExprVector ret;
      Expr foundsel = NULL;
      for (int i = 0; i < e->arity(); ++i)
      {
        bool nestisneg = isneg;
        if (isOpX<EXISTS>(e) && e->first() == qvar->left())
          nestisneg = true;
        else if (isOpX<FORALL>(e) && e->first() == qvar->left())
          nestisneg = false;
        else if (i == 0 && isOpX<IMPL>(e))
          nestisneg = !isneg;

        auto argret = weakenWithProp(e->arg(i), prop, qvar, neststren);
        if (argret.first.size() == 0)
            return make_pair(ExprVector(), Expr(NULL));
        if (argret.second)
        {
          if (foundsel && ranges.count(foundsel) && ranges.count(argret.second) && ranges.at(foundsel) != ranges.at(argret.second))
          {
            outs() << "Bootstrap: Can't synthesize range for multi-array comparison" << endl;
            return make_pair(ExprVector(), Expr(NULL));
          }
          if (ranges.count(argret.second) && !ranges.count(foundsel))
            foundsel = argret.second;
        }
        newargs.push_back(argret.first);
      }
      ExprVector permargs(newargs.size());
      function<void(int)> perm = [&] (int pos)
      {
        if (pos == newargs.size())
          ret.push_back(mknary(e->op(), permargs.begin(), permargs.end()));
        else
        {
          for (const Expr& na : newargs[pos])
          {
            permargs[pos] = na;
            perm(pos + 1);
          }
        }
      };
      perm(0);

      if (isOpX<BOOL_TY>(typeOf(e)) && foundsel)
      {
        ExprVector newret;
        if (ranges.count(foundsel) != 0)
          for (const Expr& rete : ret)
            for (const Expr& range : ranges.at(foundsel))
              if (!stren)
                newret.push_back(mk<OR>(mkNeg(range), rete));
              else
                newret.push_back(mk<AND>(range, rete));

        // NULL because we already applied range
        return make_pair(std::move(newret), Expr(NULL));
      }
      return make_pair(std::move(ret), foundsel);
    }
#endif

    ExprVector synthAERange(Expr post, Expr qvar, Expr inv)
    {
      return coverWithRange(post, getRanges(inv, qvar), qvar).first;
      /*vector<ExprVector> newargs;
      if (isLit(post) || isOpX<FDECL>(post))
        return ExprVector{post};

      for (int i = 0; i < post->arity(); ++i)
        newargs.push_back(synthAERange(fullpost, post->arg(i), qvar));

      if ((isOpX<EXISTS>(post) || isOpX<FORALL>(post)) && newargs.front()[0] == qvar->first())
      {
        ExprVector newback;
        for (const Expr& na : newargs.back())
        {
          auto tmp = coverWithRange(na,
            getRanges(fullpost, qvar), qvar, isOpX<EXISTS>(post)).first;
          newback.insert(newback.end(), tmp.begin(), tmp.end());
        }
        newargs.back() = std::move(newback);
      }

      ExprVector ret;
      ExprVector permargs(newargs.size());
      function<void(int)> perm = [&] (int pos)
      {
        if (pos == newargs.size())
          ret.push_back(mknary(post->op(), permargs.begin(), permargs.end()));
        else
        {
          for (const Expr& na : newargs[pos])
          {
            permargs[pos] = na;
            perm(pos + 1);
          }
        }
      };
      perm(0);
      return std::move(ret);*/
    }

    // Doesn't work
#if 0
    Expr alternRegQuants(const Expr &e)
    {
      if (isOpX<FDECL>(e) || IsConst()(e) || e->arity() == 0)
        return e;
      if (isOpX<EXISTS>(e))
      {
        Expr newlast = alternRegQuants(e->last());
        ExprVector disjs;
        u.flatten(newlast, disjs, false, ExprVector(), keepQuantifiers);
        ExprVector newbody;
        newbody.reserve(disjs.size());
        ExprVector newe(e->args_begin(), e->args_end());
        for (const Expr& dj : disjs)
        {
          newe.back() = dj;
          newbody.push_back(mknary(e->op(), newe.begin(), newe.end()));
        }
        return disjoin(newbody, m_efac);
      }
      // TODO: CNF for forall
      ExprVector newargs(e->arity());
      for (int i = 0; i < e->arity(); ++i)
        newargs[i] = alternRegQuants(e->arg(i));
      return mknary(e->op(), newargs.begin(), newargs.end());
    }
#endif

    // Takes property and performs the core heuristic of range substitution.
    // Returns a vector of potential candidates.
    ExprVector generalizeArrQuery()
    {
      ExprVector ret(1);

      Expr newpost = NULL;
      ExprVector srcbodies;
      const ExprVector *locvars = NULL;
      // Find query body and loops
      Expr queryrel;
      for (const auto& chc : ruleManager.chcs)
        if (chc.isQuery)
        {
          if (chc.origbody)
            newpost = chc.origbody;
          else
            newpost = chc.body;
          locvars = &chc.locVars;
          queryrel = chc.srcRelation;
        }
      for (const auto& chc : ruleManager.chcs)
        if (chc.dstRelation == queryrel)
          srcbodies.push_back(chc.body);

      if (isOpX<AND>(newpost))
      {
        // Strip loop condition
        ExprVector newpost_newargs;
        for (int i = 0; i < newpost->arity(); ++i)
        {
          bool shoulddiscard = false;
          for (const Expr& body : srcbodies)
          {
            if (shoulddiscard) continue;
            if (isOpX<AND>(body))
            {
              for (int j = 0; j < body->arity(); ++j)
                if (u.isEquiv(mkNeg(newpost->arg(i)), body->arg(j)))
                  shoulddiscard = true;
            }
            else if (u.isEquiv(mkNeg(newpost->arg(i)), body))
              shoulddiscard = true;
          }
          if (!shoulddiscard)
            newpost_newargs.push_back(newpost->arg(i));
        }
        newpost = conjoin(newpost_newargs, m_efac);
      }

      if (locvars)
        newpost = getExists(newpost, *locvars);

      newpost = regularizeQF(newpost);

      newpost = mkNeg(newpost);

      //newpost = alternRegQuants(newpost);

      ret.back() = newpost;
      if (alternver < 5)
        return std::move(ret);

      // Rewrite ranges
      /*ExprUMap qvars;
      unordered_map<Expr,int> hasrange;
      newpost = mutateAERanges(newpost, newpost, qvars, hasrange, false);
      ret.back() = newpost;
      if (alternver < 3)
        return std::move(ret);*/

      ret.pop_back();
      ExprVector qvars;
      orderedQVars(newpost, qvars);

      // Rewrite ranges
      function<void(Expr,ExprVector::const_iterator)> perm = [&] (Expr e, ExprVector::const_iterator pos)
      {
        if (pos == qvars.end())
        {
          ret.push_back(e);
          return;
        }
        auto nextpos = pos; ++nextpos;
        //if (hasrange[pos->first] < 2)
          for (const Expr& newe : synthAERange(e, *pos, queryrel))
            perm(newe, nextpos);
        /*else
          return perm(e, nextpos);*/
      };
      perm(newpost, qvars.begin());

      return std::move(ret);
    }

    // Returns all quantified variables within `e` (recursing for nested
    //   quantifiers) in pre-order.
    void orderedQVars(Expr e, ExprVector& qvars)
    {
      if (isOpX<FORALL>(e) || isOpX<EXISTS>(e))
      {
        for (int i = 0; i < e->arity() - 1; ++i)
          qvars.push_back(mk<FAPP>(e->arg(i)));
        return orderedQVars(e->last(), qvars);
      }
      for (int i = 0; i < e->arity(); ++i)
        orderedQVars(e->arg(i), qvars);
    }

    // Add ranges to a candidate `cand` that has none.
    // Returns a vector of candidates: permutations of all possible ranges.
    ExprVector alternAddRanges(Expr cand, Expr inv)
    {
      //ExprUMap qvars;
      //unordered_map<Expr,int> hasrange;
      //Expr ret = mutateAERanges(cand, cand, qvars, hasrange, false);
      //assert(ret == cand); // TODO: Fix bug that makes this false

      ExprVector qvars;
      orderedQVars(cand, qvars);
      ExprVector ret;
      function<void(Expr,int)> perm = [&] (Expr e, int pos)
      {
        if (pos == qvars.size())
          ret.push_back(e);
        else
          for (const Expr& newe : synthAERange(e, qvars[pos], inv))
            perm(newe, pos + 1);
      };
      perm(cand, 0);
      return std::move(ret);
    }

    // Version of CheckCand that we use while for checking the output
    //   of `generalizeArrQuery`.
    // Used to be used for more, probably not needed now.
    bool bootstrapCheckCand(Expr cand, int invNum, bool isnewpost = false)
    {
      if (printLog)
      {
        outs () << "- - - Bootstrapped cand for " << decls[invNum] << ": ";
                u.print(cand); outs() << (printLog >= 3 ? " 😎\n" : "\n");
      }
      candidates[invNum].clear();
      candidates[invNum].push_back(cand);
      if (multiHoudini(ruleManager.dwtoCHCs))
      {
        assignPrioritiesForLearned();
        if (checkAllLemmas())
        {
          outs () << "Success after bootstrapping\n";
          printSolution();
          return true;
        }
        if (!isnewpost && alternver >= 5)
        {
          for (const Expr& np : newposts)
            if (bootstrapCheckCand(np, queryInvNum, true))
              return true;
        }
      }
      return false;
    }

    // Returns all quantified expressions found in `e`, in pre-order,
    //   recursing for nested quantifiers.
    void orderedQuants(const Expr& e, ExprVector &quants)
    {
      if (isOpX<FORALL>(e) || isOpX<EXISTS>(e))
      {
        quants.push_back(e);
        orderedQuants(e->last(), quants);
      }
      else if (!isOpX<FDECL>(e) && !IsConst()(e))
        for (int i = 0; i < e->arity(); ++i)
          orderedQuants(e->arg(i), quants);
    }

    // Returns permutations of matches from the variables in `qvits` to
    //   the the quantified variables in `cand`.
    // This is run after we've put the guessed body in the template,
    //   so the variables don't match yet.
    //
    // E.g.: (forall ((x Int)) (exists ((y Int)) (= _FH_it_0 _FH_it_1)))
    // will return:
    //   (forall ((_FH_it_0 Int) (_FH_it_1 Int)) (= _FH_it_0 _FH_it_1)),
    //   (exists ((_FH_it_0 Int) (_FH_it_1 Int)) (= _FH_it_0 _FH_it_1)),
    //   (forall ((_FH_it_0 Int)) (exists ((_FH_it_1 Int)) (= _FH_it_0 _FH_it_1))),
    //   (forall ((_FH_it_1 Int)) (exists ((_FH_it_0 Int)) (= _FH_it_0 _FH_it_1)))
    ExprVector alternMatchQuants(Expr cand, int invNum)
    {
      ExprUSet qiters;
      for (const auto qvptr : qvits[invNum])
        qiters.insert(qvptr->qv);
      ExprVector qvs;
      filter(cand, [&](const Expr& e){return qiters.count(e) != 0;},
        inserter(qvs, qvs.end()));
      if (qvs.size() == 0)
        return ExprVector{cand};
      for (int i = 0; i < qvs.size(); ++i)
        qvs[i] = qvs[i]->left();
      ExprVector quants;
      orderedQuants(cand, quants);
      if (quants.size() == 0)
        return ExprVector{cand};
      vector<ExprVector> quantAssigns(quants.size()), stockAssigns(quants.size());
      Expr emptyqvar;
      for (int i = 0; i < quantAssigns.size(); ++i)
      {
        for (int j = 0; j < max(qvs.size(), quants[i]->arity() - 1); ++j)
          quantAssigns[i].push_back(constDecl(mkTerm(string("empty") + to_string(j), m_efac), typeOf(quants[i]->left())));
        for (int j = 0; j < quants[i]->arity() - 1; ++j)
          quantAssigns[i][j] = quants[i]->arg(j);
        stockAssigns[i] = quantAssigns[i];
      }
      ExprVector ret; ExprUSet seen;
      function<void(int)> perm = [&] (int pos)
      {
        if (pos == qvs.size())
        {
          //ExprUMap newquants;
          Expr qcand = cand;
          for (int i = quantAssigns.size() - 1; i >= 0; --i)
          {
            // I had bugs where quantifiers were invalidally getting skipped,
            //   so just comment out for now.
            /*bool doquant = false;
            for (int j = 0; j < quantAssigns[i].size(); ++j)
              if (quantAssigns[i][j] != stockAssigns[i][j]) { doquant = true; break; }
            if (doquant)
            {*/
              /*Expr tmpcand = mknary(quants[i]->op(),
                quantAssigns[i].begin(), quantAssigns[i].end());
              tmpcand = replaceAll(tmpcand, stockAssigns[i], quantAssigns[i]);
              newquants[quants[i]] = tmpcand;*/
            qcand = replaceAll(qcand, stockAssigns[i], quantAssigns[i]);
            /*}
            else
              newquants[quants[i]] = quantAssigns[i].back();*/
          }
          //Expr qcand = replaceAll(cand, newquants);
          ExprVector rangedqcand = alternAddRanges(qcand, decls[invNum]);
          for (const Expr& e : rangedqcand)
            if (seen.insert(e).second)
              ret.push_back(e);
          return;
        }
        for (int i = 0; i < quants.size(); ++i)
          for (int j = 0; j < quantAssigns[i].size(); ++j)
            if (quantAssigns[i][j] == stockAssigns[i][j])
            {
              quantAssigns[i][j] = qvs[pos];
              perm(pos + 1);
              quantAssigns[i][j] = stockAssigns[i][j];
              if (j >= quants[i]->arity() - 1)
                break;
            }
      };
      perm(0);
      return std::move(ret);
    }

    // Returns all permutations of surrounding `cand` with quantifiers,
    //   where each variable in `qvits` in `cand` is tried both
    //   existentially and universally quantified.
    ExprVector alternTryQuants(Expr cand, int invNum)
    {
      ExprUSet qiters;
      for (const auto qvptr : qvits[invNum])
        qiters.insert(qvptr->qv);
      ExprVector qvs;
      filter(cand, [&](const Expr& e){return qiters.count(e) != 0;},
        inserter(qvs, qvs.end()));
      ExprVector ret; ExprUSet seen;
      if (qvs.size() == 0)
      {
        return ExprVector{cand};
      }
      for (int i = 0; i < qvs.size(); ++i)
        qvs[i] = qvs[i]->left();
      function<void(int,Expr)> perm = [&] (int pos, Expr e)
      {
        if (pos == qvs.size())
        {
          for (const Expr& cand : alternAddRanges(e, decls[invNum]))
            if (seen.insert(cand).second)
              ret.push_back(cand);
          return;
        }
        ExprVector newe(2);
        newe[0] = qvs[pos]; newe[1] = e;
        perm(pos + 1, mknary<FORALL>(newe));
        perm(pos + 1, mknary<EXISTS>(newe));
      };
      perm(0, cand);
      return std::move(ret);
    }

    // Used to do more, currently just copies over `arrCands` into `bootCands`.
    bool alternGenDataCands()
    {
      /*map<Expr,ExprSet> nonArrCands;
      getDataCandidates(nonArrCands);
      for (const auto& kv : nonArrCands)
      {
        int invNum = getVarIndex(kv.first, decls);
        assert(invNum >= 0);
        candidates[invNum].clear();
        candidates[invNum].insert(candidates[invNum].end(),
          kv.second.begin(), kv.second.end());
      }
      if (multiHoudini(ruleManager.dwtoCHCs))
      {
        assignPrioritiesForLearned();
        if (checkAllLemmas())
          return true;
      }*/
      for (int invNum = 0; invNum < arrCands.size(); ++invNum)
        for (const Expr& cand : arrCands[invNum])
        {
          /*if (alternTryQuants(cand, invNum))
            return true;*/
          bootCands[invNum].insert(cand);
        }
      return false;
    }

    // Attempted heuristics for finding candidates.
#if 0
    ExprVector getBoolExprs()
    {
      ExprVector bexprs;
      Expr precond, postcond;
      for (const auto& chc : ruleManager.chcs)
      {
        if (chc.isFact)
        {
          assert(!precond);
          precond = replaceAll(getExists(chc.body, chc.locVars), chc.dstVars, ruleManager.invVars.at(chc.dstRelation));
        }
        if (chc.isQuery)
        {
          assert(!postcond);
          postcond = mk<NEG>(getExists(chc.body, chc.locVars));
        }
        if (chc.isInductive)
        {
          Expr body = isOpX<AND>(chc.body) ?
            chc.body : mk<AND>(chc.body);
          //body = eliminateQuantifiers(body, chc.dstVars, false, false, false);
          for (int i = 0; i < body->arity(); ++i)
          {
            const auto dstbeg = chc.dstVars.begin(),
                       dstend = chc.dstVars.end();
            filter(body->arg(i), [&](const Expr& e){
              return !isLit(e) && !IsConst()(e) && !isOpX<FDECL>(e) &&
                //!(isOpX<EQ>(e) && find(dstbeg, dstend, e->left()) != dstend) &&
                //!(isOpX<EQ>(e) && find(dstbeg, dstend, e->right()) != dstend) &&
                isOpX<BOOL_TY>(typeOf(e)); },
              std::inserter(bexprs, bexprs.end()));
          }
          for (int i = 0; i < bexprs.size(); ++i)
          {
            if (!bexprs[i])
              continue;
            Expr bei = bexprs[i];
            bool found = false;
            for (const Expr& dvar : chc.dstVars)
              if (contains(bei, dvar))
              {
                bexprs[i] = NULL;
                found = true;
                break;
              }
            if (!found && isOpX<EQ>(bei))
            {
              bexprs.push_back(mknary<GEQ>(bei->args_begin(),bei->args_end()));
              bexprs.push_back(mknary<LEQ>(bei->args_begin(),bei->args_end()));
            }
            bei = bexprs[i];
            if (!found && isOpX<NEQ>(bei))
            {
              bexprs.push_back(mknary<GT>(bei->args_begin(),bei->args_end()));
              bexprs.push_back(mknary<LT>(bei->args_begin(),bei->args_end()));
            }
          }
        }
      }

      if (isOpX<AND>(precond))
        for (int i = 0; i < precond->arity(); ++i)
          bexprs.push_back(precond->arg(i));
      else
        bexprs.push_back(precond);

      if (isOpX<AND>(postcond))
        for (int i = 0; i < postcond->arity(); ++i)
          bexprs.push_back(postcond->arg(i));
      else
        bexprs.push_back(postcond);

      return std::move(bexprs);
    }

    ExprVector getPreConds()
    {
      ExprVector ret;
      for (const auto& chc : ruleManager.chcs)
      {
        if (chc.isInductive)
        {
          Expr body = isOpX<AND>(chc.body) ?
            chc.body : mk<AND>(chc.body);
          u.flatten(body, ret, true, chc.srcVars, keepQuantifiersRepl);
        }
      }

      return std::move(ret);
    }
#endif

    // This bootstrapping done before we do `initializeAux`,
    //   which can take a while sometimes.
    // Run before all relations are intialized.
    ExprVector newposts;
    bool bootstrap1()
    {
      if (printLog) outs () << "\nBOOTSTRAPPING\n=============\n";
      // Unquantified lemmas
      if (alternver != 0)
      {
        //filterUnsat();
        for (const auto& kv : candidates)
          bootCands[kv.first].insert(kv.second.begin(), kv.second.end());

        if (multiHoudini(ruleManager.dwtoCHCs))
        {
          assignPrioritiesForLearned();
          if (checkAllLemmas())
          {
            outs () << "Success after bootstrapping 1\n";
            printSolution();
            return true;
          }
        }

        if (!dDisj) simplifyLemmas();
      }

      if (alternver >= 0)
      {
        if (queryInvNum >= 0 && (alternver >= 5 || alternver < 2))
        {
          newposts = generalizeArrQuery();
          for (const Expr& newpost : newposts)
          {
            // Check
            if (bootstrapCheckCand(newpost, queryInvNum, true))
              return true;
          }
        }
      }

      for (auto& kv : candidates) kv.second.clear();

      return false;
    }
   
    // More bootstrapping done after `initializeAux` and all relations
    //   initialized.
    bool bootstrap2()
    {
      for (const auto& kv : candidates)
        bootCands[kv.first].insert(kv.second.begin(), kv.second.end());

      if (alternver != 0)
      {
        if (multiHoudini(ruleManager.dwtoCHCs))
        {
          assignPrioritiesForLearned();
          if (checkAllLemmas())
          {
            outs () << "Success after bootstrapping 2\n";
            printSolution();
            return true;
          }
        }
        for (const auto& kv : deferredCandidates)
        {
          candidates[kv.first].insert(candidates[kv.first].end(),
            kv.second.begin(), kv.second.end());
          bootCands[kv.first].insert(kv.second.begin(), kv.second.end());
        }
        if (multiHoudini(ruleManager.dwtoCHCs))
        {
          assignPrioritiesForLearned();
          if (checkAllLemmas())
          {
            outs () << "Success after bootstrapping 2\n";
            printSolution();
            return true;
          }
        }
      }
      if (alternver >= 0)
      {
        if (queryInvNum >= 0 && (alternver >= 5 || alternver < 2))
        {
          newposts = generalizeArrQuery();
          for (const Expr& newpost : newposts)
          {
            // Check
            if (bootstrapCheckCand(newpost, queryInvNum, true))
              return true;
          }
        }

        if (alternver >= 2 && alternGenDataCands())
          return true;

        // Code that used to compute implicative candidates.
        // I'm now just doing these with a grammar instead.

#if 0
        /*for (int invNum = 0; invNum < invNumber; ++invNum)
        {
          ExprUSet implpartsset;
          Expr lms = sfs[invNum].back().getAllLemmas();
          for (const Expr& defc : deferredCandidates[invNum])
          {
            if (u.implies(lms, defc) || u.implies(lms, mk<NEG>(defc)))
            {}
            else
              implpartsset.insert(defc);
          }
          ExprVector implparts(implpartsset.begin(), implpartsset.end());

          std::sort(implparts.begin(), implparts.end(),
            [&](const Expr& l,const Expr& r){return u.implies(l, r);});

          deferredCandidates[invNum].clear();
          deferredCandidates[invNum].insert(deferredCandidates[invNum].end(),
            implparts.begin(), implparts.end());
        }*/

        if (alternver >= 4)
        {
          //ExprVector bexprs = getBoolExprs();
          for (int invNum = 0; invNum < invNumber; ++invNum)
          {
            ExprVector preconds(mbps[invNum].begin(), mbps[invNum].end());

            Expr lms = sfs[invNum].back().getAllLemmas();
            ExprVector implparts;
            for (const Expr& defc : deferredCandidates[invNum])
            {
              if (u.implies(lms, defc) || u.implies(lms, mk<NEG>(defc)))
              {}
              else
                implparts.push_back(defc);
            }

            std::sort(implparts.begin(), implparts.end(),
              [&](const Expr& l,const Expr& r){return u.implies(l, r);});

            deferredCandidates[invNum] = implparts;

            ExprVector permargs(2);
            function<bool(int)> perm = [&] (int pos) -> bool
            {
              if (pos == permargs.size())
              {
                if (permargs[0] == permargs[1])
                  return false;
                Expr dcand = mknary<IMPL>(permargs.begin(),permargs.end());
                /*if (u.isTrue(dcand) || u.isFalse(dcand))
                  return false;*/
                if (alternTryQuants(dcand, invNum))
                  return true;
              }
              /*else if (pos == 0)
              {
                for (int i = 0; i < preconds.size(); ++i)
                {
                  const Expr& pre = preconds[i];
                  if (!pre)
                    continue;
                  permargs[pos] = pre;
                  if (perm(pos + 1))
                    return true;
                }
              }*/
              else
                for (int i = 0; i < implparts.size(); ++i)
                {
                  const Expr& defc = implparts[i];
                  if (!defc)
                    continue;
                  permargs[pos] = defc;
                  if (perm(pos + 1))
                    return true;
                }
              return false;
            };
            if (perm(0))
              return true;
          }
        }
#endif

        if (alternver < 2)
        {
          outs() << "unknown\n";
          exit(1);
        }
        boot = false;

        return false;
      }

      // try array candidates one-by-one (adapted from `synthesize`)
      for (auto & dcl: ruleManager.wtoDecls)
      {
        if (ruleManager.hasArrays[dcl])
        {
          int invNum = getVarIndex(dcl, decls);
          SamplFactory& sf = sfs[invNum].back();
          for (auto it = arrCands[invNum].begin(); it != arrCands[invNum].end();
               it = arrCands[invNum].erase(it))
          {
            Expr c = *it;
            if (u.isTrue(c) || u.isFalse(c)) continue;

            Expr cand = sf.af.getQCand(c);
            if (cand->arity() <= 1) continue;

            if (printLog)
              outs () << "- - - Bootstrapped cand for " << dcl << ": "
                      << cand << (printLog >= 3 ? " 😎\n" : "\n");

            auto candidatesTmp = candidates[invNum]; // save for later
            if (!addCandidate(invNum, cand)) continue;
            if (checkCand(invNum))
            {
              assignPrioritiesForLearned();
              generalizeArrInvars(invNum, sf);
              if (checkAllLemmas())
              {
                outs () << "Success after bootstrapping 2\n";
                printSolution();
                return true;
              }
            }
            else
            {
              if (dAddProp)                  // for QE-based propagation, the heuristic is disabled
                deferredCandidates[invNum].push_back(cand);
                                             // otherwise, prioritize equality cands with closed ranges
              else if (isOpX<EQ>(c) && getQvit(invNum, bind::fapp(cand->arg(0))) && getQvit(invNum, bind::fapp(cand->arg(0)))->closed)
                deferredCandidates[invNum].push_back(cand);
              else
                deferredCandidates[invNum].push_front(cand);

              if (candidates[invNum].empty()) candidates[invNum] = candidatesTmp;
            }
          }
        }

        // second round of bootstrapping (to be removed after Houdini supports arrays)

        candidates.clear();
        ExprVector empt;
        for (auto &hr: ruleManager.chcs)
        {
          if (hr.isQuery)
          {
            int invNum = getVarIndex(hr.srcRelation, decls);
            ExprSet cnjs;
            getConj(hr.body, cnjs);
            for (auto & a : cnjs)
            {
              if (isOpX<NEG>(a) && findInvVar(invNum, a, empt))
              {
                candidates[invNum].push_back(a->left());
                break;
              }
            }
            break;
          }
        }
        if (multiHoudini(ruleManager.dwtoCHCs))
        {
          assignPrioritiesForLearned();
          if (checkAllLemmas())
          {
            outs () << "Success after bootstrapping 2\n";
            printSolution();
            return true;
          }
        }
      }

      boot = false;

      return false;
    }

    bool checkAllLemmas()
    {
      candidates.clear();
      for (int i = ruleManager.wtoCHCs.size() - 1; i >= 0; i--)
      {
        auto & hr = *ruleManager.wtoCHCs[i];
        tribool b = checkCHC(hr, candidates, true);
        if (b) {
          if (!hr.isQuery)
          {
            if (printLog) outs() << "WARNING: Something went wrong" <<
              (ruleManager.hasArrays[hr.srcRelation] || ruleManager.hasArrays[hr.dstRelation] ?
              " (possibly, due to quantifiers)" : "") <<
              ". Restarting...\n";

            for (int i = 0; i < decls.size(); i++)
            {
              SamplFactory& sf = sfs[i].back();
              for (auto & l : sf.learnedExprs) candidates[i].push_back(l);
              sf.learnedExprs.clear();
            }
            multiHoudini(ruleManager.wtoCHCs);
            assignPrioritiesForLearned();

            return false;
          }
          else
            return false; // TODO: use this fact somehow
        }
        else if (indeterminate(b)) return false;
      }
      return true;
    }

    tribool checkCHC (HornRuleExt& hr, map<int, ExprVector>& annotations, bool checkAll = false)
    {
      int srcNum = getVarIndex(hr.srcRelation, decls);
      int dstNum = getVarIndex(hr.dstRelation, decls);

      if (!hr.isQuery)       // shortcuts
      {
        if (dstNum < 0)
        {
          if (printLog >= 3) outs () << "      Trivially true since " << hr.dstRelation << " is not initialized\n";
          return false;
        }
        if (checkAll && annotations[dstNum].empty())
          return false;
      }

      ExprSet exprs = {hr.body};

      if (!hr.isFact && srcNum >= 0)
      {
        ExprSet lms = sfs[srcNum].back().learnedExprs;
        for (auto & a : annotations[srcNum]) lms.insert(a);
        for (auto a : lms)
        {
          a = replaceAll(a, invarVarsShort[srcNum], hr.srcVars);
          exprs.insert(a);
        }
      }

      /*if (!u.isSat(exprs))
        return true;*/

      if (!hr.isQuery)
      {
        ExprSet lms = sfs[dstNum].back().learnedExprs;
        ExprSet negged;
        for (auto & a : annotations[dstNum]) lms.insert(a);
        for (auto a : lms)
        {
          a = replaceAll(a, invarVarsShort[dstNum], hr.dstVars);
          negged.insert(mkNeg(a));
        }
        exprs.insert(disjoin(negged, m_efac));
      }
      return u.isSat(exprs);
    }

    void generateMbpDecisionTree(int invNum, ExprVector& srcVars, ExprVector& dstVars, int f = 0)
    {
      int sz = mbpDt[invNum].sz;
      if (f == 0)
      {
        assert(sz == 0);
        mbpDt[invNum].addEmptyEdge();
        sz++;
      }

      for (auto mbp = mbps[invNum].begin(); mbp != mbps[invNum].end(); ++mbp)
      {
        if (mbpDt[invNum].hasEdge(f, *mbp)) continue;
        ExprSet constr;
        if (f == 0) constr = {*mbp, prefs[invNum]};
        else constr = {mbpDt[invNum].tree_cont[f], ssas[invNum],
                        replaceAll(mk<AND>
                          (mkNeg(mbpDt[invNum].tree_cont[f]), *mbp), srcVars, dstVars)};
        if (u.isSat(constr))
        {
          if (f == 0) mbpDt[invNum].addEdge(f, *mbp);
          else
            if (mbpDt[invNum].getExprInPath(f, *mbp) < 0)    // to avoid cycles
              mbpDt[invNum].addEdge(f, *mbp);
        }
      }
      for (; sz < mbpDt[invNum].sz; sz++)
        generateMbpDecisionTree(invNum, srcVars, dstVars, sz);
    }

    void strengthenMbpDecisionTree(int invNum, ExprVector& srcVars, ExprVector& dstVars)
    {
      strenDt[invNum] = mbpDt[invNum];
      for (int i = 0; i < strenDt[invNum].tree_cont.size(); i++)
        strenDt[invNum].tree_cont[i] = mk<TRUE>(m_efac);
      if (!dStrenMbp) return;     // by default, strenghtening is disabled

      for (auto & s : mbpDt[invNum].tree_edgs)
      {
        if (s.second.size() == 0) continue; // leaves, nothing to strenghten
        Expr rootStr = strenDt[invNum].tree_cont[s.first];

        ExprVector constr;
        if (s.first == 0) constr = {prefs[invNum]};
        else constr = { mbpDt[invNum].tree_cont[s.first], ssas[invNum],
                replaceAll(mkNeg(mbpDt[invNum].tree_cont[s.first]), srcVars, dstVars) };

        for (auto & v : srcVars)
        {
          ExprVector allStrs;
          ExprVector abdVars = {v};
          for (auto & e : s.second)
          {
            Expr mbp = mbpDt[invNum].tree_cont[e];
            Expr abd = simplifyArithm(mkNeg(ufo::keepQuantifiers(
                          mkNeg(mk<IMPL>(conjoin(constr, m_efac), mbp)), abdVars)));
            if (!isOpX<FALSE>(abd) &&
              u.implies(mk<AND>(abd, ssas[invNum]), replaceAll(abd, srcVars, dstVars)))
                allStrs.push_back(abd);
          }
          if (!u.isSat(allStrs))
          {
            for (int i = 0; i < s.second.size(); i++)
            {
              strenDt[invNum].tree_cont[s.second[i]] = allStrs[i];
              if (printLog >= 2)
              {
                auto e = s.second[i];
                outs () << "Found strengthening: " << strenDt[invNum].tree_cont[e] << " for " << mbpDt[invNum].tree_cont[e] << "\n";
              }
            }
            break;
          }
        }
        if (s.first > 0 && !isOpX<TRUE>(rootStr))
        {
          for (auto & e : s.second)
            if (isOpX<TRUE>(strenDt[invNum].tree_cont[e]))
              strenDt[invNum].tree_cont[e] = rootStr;
            else
              strenDt[invNum].tree_cont[e] =
                mk<AND>(rootStr, strenDt[invNum].tree_cont[e]);
        }
      }
    }

    Expr generatePostcondsFromPhases(int invNum, Expr cnt, vector<int>& extMbp,
                      Expr ssa, int p, ExprVector& srcVars, ExprVector& dstVars)
    {
      auto &path = mbpDt[invNum].paths[p];
      Expr pre, phase = NULL;
      for (int i = 0; i < extMbp.size(); i++)
      {
        Expr mbp = mbpDt[invNum].tree_cont[path[extMbp[i]]];
        Expr phaseCur = u.simplifiedAnd(ssa, mbp);
        Expr preCur = keepQuantifiers(phaseCur, srcVars);
        preCur = weakenForHardVars(preCur, srcVars);
        preCur = projectVar(preCur, cnt);
        if (i == 0)
          pre = preCur;
        else if (!u.isSat(pre, phaseCur))
          pre = preCur;
        else
        {
          pre = keepQuantifiers(mk<AND>(pre, phase), dstVars);
          pre = weakenForHardVars(pre, dstVars);
          pre = replaceAll(pre, dstVars, srcVars);
          pre = projectVar(pre, cnt);
          pre = u.removeRedundantConjuncts(mk<AND>(preCur, pre));
        }
        phase = phaseCur;
      }
      return pre;
    }

    bool updateRanges(int invNum, ArrAccessIter* newer)
    {
      for (int i = 0; i < qvits[invNum].size(); i++)
      {
        auto & existing = qvits[invNum][i];
        if (existing->iter != newer->iter) continue;
        Expr newra = replaceAll(newer->range, newer->qv, existing->qv);
        bool toRepl = false;
        if (u.implies(newra, existing->range))
        {
          if (newer->closed == existing->closed)
            return false;
          if (newer->closed && !existing->closed)
            toRepl = true;
        }
        if (toRepl || u.implies(existing->range, newra))           // new one is more general
        {
          if (newer->closed || !existing->closed)
          {
            qvits[invNum][i] = newer;
            if (printLog >= 2) outs () << "Re";     // printing to be continued in `createArrAcc`
            return true;
          }
          if (!newer->closed && existing->closed)
            return false;
        }
      }
      qvits[invNum].push_back(newer);
      return true;
    }

    int qvitNum = 0;
    bool createArrAcc(Expr rel, int invNum, Expr a, bool grows, vector<int>& extMbp, Expr extPref, Expr pre)
    {
      ArrAccessIter* ar = new ArrAccessIter();
      ar->iter = a;
      ar->grows = grows;
      ar->mbp = extMbp;

      ExprSet itersCur = {a};
      AeValSolver ae(mk<TRUE>(m_efac), extPref, itersCur);
      ae.solve();
      Expr skol = u.simplifyITE(ae.getSimpleSkolemFunction(), ae.getValidSubset());
      if (isOpX<TRUE>(skol))
      {
        if (printLog >= 3) outs() << "Preprocessing of counter " << a << " of relation " << rel << " fails  🔥\n";
        return false;
      }
      ar->precond = projectITE (skol, a);
      if (ar->precond == NULL)
      {
        if (printLog >= 3) outs() << "Preprocessing of counter " << a << " of relation " << rel << " fails  🔥\n";
        return false;
      }

      ar->qv = bind::intConst(mkTerm<string>("_FH_arr_it_" + lexical_cast<string>(qvitNum++), m_efac));
      Expr fla = (ar->grows) ? mk<GEQ>(ar->qv, ar->precond) :
                               mk<LEQ>(ar->qv, ar->precond);
      ExprSet tmp;
      getConj(pre, tmp);
      for (auto it = tmp.begin(); it != tmp.end(); )
      {
        if (contains(*it, ar->iter))
        {
          Expr t = ineqSimplifier(ar->iter, *it);     // we need the tightest "opposite" bound; to extend
          if ((ar->grows && (isOpX<LT>(t) || isOpX<LEQ>(t))) ||
             (!ar->grows && (isOpX<GT>(t) || isOpX<GEQ>(t))))
          {
            if (t->left() == ar->iter)
            {
              ar->postcond = t->right();
              ++it;
              continue;
            }
          }
        }
        it = tmp.erase(it);
      }

      ar->closed = tmp.size() > 0;              // flag that the range has lower and upper bounds

      // if ((ar->mbp).size() == mbps[invNum].size())   // GF: TODO, make use of them
      //   ar->mbp.clear();                             // flag that iter keeps growing/shrinking in all phases

      ExprVector invAndIterVarsAll = invarVarsShort[invNum];
      invAndIterVarsAll.push_back(ar->qv);
      ar->range = simplifyArithm(normalizeDisj(mk<AND>(fla,
                 replaceAll(conjoin(tmp, m_efac), ar->iter, ar->qv)), invAndIterVarsAll));

      // keep it, if it is more general than an existing one
      if (updateRanges(invNum, ar))
      {
        arrIterRanges[invNum].insert(ar->range);
        arrAccessVars[invNum].push_back(ar->qv);
        if (printLog >= 2)
          outs () << "Introducing " << ar->qv << " for "
                  << (ar->grows ? "growing" : "shrinking")
                  << " variable " << ar->iter
                  << " with " << (ar->closed ? "closed" : "open")
                  << " range " << ar->range << "\n";
      }
      return true;
    }

    void initializeAux(ExprSet& cands, BndExpl& bnd, int cycleNum, Expr pref)
    {
      vector<int>& cycle = ruleManager.cycles[cycleNum];
      HornRuleExt* hr = &ruleManager.chcs[cycle[0]];
      Expr rel = hr->srcRelation;
      ExprVector& srcVars = hr->srcVars;
      ExprVector& dstVars = ruleManager.chcs[cycle.back()].dstVars;
      assert(srcVars.size() == dstVars.size());

      int invNum = getVarIndex(rel, decls);
      prefs[invNum] = pref;
      Expr ssa = bnd.toExpr(cycle);
      ssa = replaceAll(ssa, bnd.bindVars.back(), dstVars);
      ssas[invNum] = ssa;
      ssa = rewriteSelectStore(ssa);
      retrieveDeltas(ssa, srcVars, dstVars, cands);
      generateMbps(invNum, ssa, srcVars, dstVars, cands);     // collect and add mbps as candidates

      if (dDisj || containsOp<ARRAY_TY>(ssas[invNum]))
      {
        generateMbpDecisionTree(invNum, srcVars, dstVars);
        mbpDt[invNum].deleteIntermPaths();
        strengthenMbpDecisionTree(invNum, srcVars, dstVars);
        if (printLog >= 2)
          outs () << "MBPs are organized as a decision tree (with "
                  << mbpDt[invNum].paths.size() << " possible path(s))\n";
        if (printLog >= 3)
          mbpDt[invNum].print();
      }

      if (qvits[invNum].size() > 0) return;
      if (!containsOp<ARRAY_TY>(ssas[invNum])) return;

      filter (ssas[invNum], bind::IsConst (), inserter(qvars[invNum], qvars[invNum].begin()));
      postconds[invNum] = ruleManager.getPrecondition(hr);
      for (int i = 0; i < dstVars.size(); i++)
      {
        Expr a = srcVars[i];
        Expr b = dstVars[i];
        if (!bind::isIntConst(a) /*|| !contains(postconds[invNum], a)*/) continue;
        if (u.implies(ssa, mk<EQ>(a, b))) continue;

        if (printLog >= 3) outs () << "Considering variable " << a << " as a counter \n";
        for (int p = 0; p < mbpDt[invNum].paths.size(); p++)
        {
          if (printLog >= 3) outs () << "MBP Path # " << p <<"\n";
          Expr extPref = pref;
          vector<int> extMbp;
          tribool grows = indeterminate;
          auto &path = mbpDt[invNum].paths[p];
          for (int m = 0; m < path.size(); m++)
          {
            Expr & mbp = mbpDt[invNum].tree_cont[path[m]];
            tribool growsCur = u.implies(mk<AND>(mbp, ssa), mk<GEQ>(b, a));
            if (growsCur != 1)
              if (!u.implies(mk<AND>(mbp, ssa), mk<GEQ>(a, b)))
                growsCur = indeterminate;

            if (printLog >= 3)
            {
              if (indeterminate(growsCur)) { outs () << "  mbp 👎: " << mbp << "\n"; }
              else outs () << "  mbp 👍 for " << a << ": " << mbp << "\n";
            }
            bool toCont = true;
            if (extMbp.empty())                                       // new meta-phase begins
            {
              grows = growsCur;
              if (!indeterminate(growsCur))
              {
                extMbp = {m};
                if (m > 0)
                {
                  extPref = u.simplifiedAnd(ssa, mbpDt[invNum].tree_cont[path[m-1]]);
                  extPref = keepQuantifiersRepl (extPref, dstVars);
                  extPref = u.removeITE(replaceAll(extPref, dstVars, srcVars));
                }
              }
              toCont = false;
            }
            else if (grows == growsCur && (!indeterminate(growsCur))) // meta-phase continues
            {
              extMbp.push_back(m);
              toCont = false;
            }

            // for the last one, or any where the cnt direction changes:
            if (!extMbp.empty() && (toCont || m == path.size() - 1))
              if (createArrAcc(rel, invNum, a, (bool)grows, extMbp, extPref,
                generatePostcondsFromPhases(invNum, a, extMbp,  ssa,  p, srcVars, dstVars)))
                  extMbp.clear();               // 👍 for the next phase
          }
        }
      }

      if (!qvits[invNum].empty()) ruleManager.hasArrays[rel] = true;
    }

    void postInitGram()
    {
      for (int i = 0; i < invNumber; ++i)
      {
        for (const auto qvptr : qvits[i])
          sfs[i].back().gf.addVar(qvptr->qv);
      }
    }

    void printCands()
    {
      for (auto & c : candidates)
        if (c.second.size() > 0)
        {
          outs() << "  Candidates for " << *decls[c.first] << ":\n";
          pprint(c.second, 4);
        }
    }

    virtual bool checkReadLemmasSafety()
    {
      return checkAllLemmas();
    }

    void clearCache()
    {
      _grvCache.clear();
      _grvFailed.clear();
    }
  };

  inline bool learnInvariants3(string smt, unsigned maxAttempts, unsigned to,
       bool freqs, bool aggp, int dat, int mut, bool doElim, bool doArithm,
       bool doDisj, int doProp, int mbpEqs, bool dAllMbp, bool dAddProp, bool dAddDat,
       bool dStrenMbp, int dFwd, bool dRec, bool dGenerous, bool dSee, bool ser, int debug, bool dBoot, int sw, bool sl, string gramfile, TravParams gramps, bool b4simpl, int alternver)
  {
    ExprFactory m_efac;
    EZ3 z3(m_efac);
    SMTUtils u(m_efac, to);

    CHCs ruleManager(m_efac, z3, debug - 2);
    auto res = ruleManager.parse(smt, doElim, doArithm);
    if (ser)
    {
      ruleManager.serialize();
      return true;
    }
    if (!res) return false;

    if (ruleManager.hasBV)
    {
      outs() << "Bitvectors currently not supported. Try `bnd/expl`.\n";
      return true;
    }

    BndExpl bnd(ruleManager, to, debug);
    if (!ruleManager.hasCycles())
      return bnd.exploreTraces(1, ruleManager.chcs.size(), true) ? 0 : 1;

    RndLearnerV3 ds(m_efac, z3, ruleManager, to, freqs, aggp, mut, dat,
                    doDisj, mbpEqs, dAllMbp, dAddProp, dAddDat, dStrenMbp,
                    dFwd, dRec, dGenerous, to, debug, smt, gramfile, sw, sl, alternver);
    ds.boot = dBoot;

    map<Expr, ExprSet> cands;
    for (int i = 0; i < ruleManager.cycles.size(); i++)
    {
      Expr dcl = ruleManager.chcs[ruleManager.cycles[i][0]].srcRelation;
      if (ds.initializedDecl(dcl)) continue;
      ds.initializeDecl(dcl, gramps, b4simpl);
      if (!dSee) continue;

      Expr pref = bnd.compactPrefix(i);
      ExprSet tmp;
      getConj(pref, tmp);
      for (auto & t : tmp)
        if (hasOnlyVars(t, ruleManager.invVars[dcl]))
          cands[dcl].insert(t);
      ds.addCandidates(dcl, cands[dcl]);
      if (dBoot)
        if (ds.bootstrap1()) return true;

      if (mut > 0) ds.mutateHeuristicEq(cands[dcl], cands[dcl], dcl, true);
      ds.initializeAux(cands[dcl], bnd, i, pref);
    }

    ds.clearCache();

    if (ds.readLemmas())
      return true;

    ds.postInitGram();

    for (auto & dcl: ruleManager.wtoDecls)
    {
      for (int i = 0; i < doProp; i++)
        for (auto & a : cands[dcl]) ds.propagate(dcl, a, true);
      ds.addCandidates(dcl, cands[dcl]);
      ds.prepareSeeds(dcl, cands[dcl]);
    }

    if (dBoot)
      if (ds.bootstrap1()) return true;

    if (dat > 0)
    {
      map<Expr, ExprSet> dcands;
      ds.getDataCandidates(dcands);
      for (auto & dcl: ruleManager.wtoDecls)
        ds.addCandidates(dcl, dcands[dcl]);
    }

    if (dBoot)
      if (ds.bootstrap2()) return true;

    if (alternver < 0)
    {
      ds.calculateStatistics();
      ds.deferredPriorities();
    }
    std::srand(std::time(0));
    bool success = ds.synthesize(maxAttempts);
    if (!success)
      ds.writeAllLemmas();
    return success;
  }
}

#endif
