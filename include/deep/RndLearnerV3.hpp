#ifndef RNDLEARNERV3__HPP__
#define RNDLEARNERV3__HPP__

#include "RndLearner.hpp"

#ifdef HAVE_ARMADILLO
#include "DataLearner.hpp"
#endif

using namespace std;
using namespace boost;
namespace ufo
{
  class RndLearnerV3 : public RndLearnerV2
  {
    private:

    ExprSet checked;
    map<int, ExprVector> candidates;
    int updCount = 1;

    map<int, Expr> iterators; // per cycle
    map<int, Expr> preconds;
    map<int, Expr> ssas;
    map<int, ExprSet> qvars;
    map<int, ExprVector> bindvars;

    public:

    RndLearnerV3 (ExprFactory &efac, EZ3 &z3, CHCs& r, bool freqs, bool aggp) :
      RndLearnerV2 (efac, z3, r, freqs, aggp){}

    bool checkInit(int invNum, Expr rel)
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
            (hr.srcRelation == rel && checkedDst) ||
            (hr.dstRelation == rel && checkedSrc) ||
            (checkedSrc && checkedDst) ||
            (hr.isFact && checkedDst))
        {
          if (!hr.isQuery) adjacent.push_back(&hr);
        }
      }
      if (adjacent.empty()) return true;
      return multiHoudini(adjacent);
    }

    Expr eliminateQuantifiers(Expr formula, ExprVector& varsRenameFrom, int invNum)
    {
      ExprSet allVars;
      ExprSet quantified;
      expr::filter (formula, bind::IsConst(), std::inserter (allVars, allVars.begin ()));
      for (auto & v : allVars)
        if (find(varsRenameFrom.begin(), varsRenameFrom.end(), v) == varsRenameFrom.end())
          quantified.insert(v);

      if (findNonlin(formula) || containsOp<IDIV>(formula) || containsOp<MOD>(formula))
      {
        Expr newCand = simpleQE(formula, quantified, true, true);
        for (auto & v : invarVars[invNum]) newCand = replaceAll(newCand, varsRenameFrom[v.first], v.second);
        return newCand;
      }

      AeValSolver ae(mk<TRUE>(m_efac), formula, quantified);
      if (ae.solve())
      {
        Expr newCand = ae.getValidSubset();
        for (auto & v : invarVars[invNum]) newCand = replaceAll(newCand, varsRenameFrom[v.first], v.second);
        return newCand;
      }
      return mk<TRUE>(m_efac);
    }

    bool getCandForAdjacentRel(Expr candToProp, Expr constraint, ExprVector& varsRenameFrom, Expr rel, bool seed)
    {
      Expr formula = mk<AND>(candToProp, constraint);
      if (!u.isSat(formula)) return false; // sometimes, maybe we should return true.

      ExprSet dsjs;
      getDisj(candToProp, dsjs);
      ExprSet newSeedDsjs;
      int invNum = getVarIndex(rel, decls);
      for (auto & d : dsjs)
      {
        Expr r = eliminateQuantifiers(mk<AND>(d, constraint), varsRenameFrom, invNum);
        newSeedDsjs.insert(r);
      }
      Expr newCand = disjoin(newSeedDsjs, m_efac);

      if (seed)
      {
        ExprSet cnjs;
        ExprSet newCnjs;
        getConj(newCand, cnjs);
        for (auto & cnd : cnjs)
        {
          newCnjs.insert(cnd);
          addCandidate(invNum, cnd);
        }

        newCand = conjoin(newCnjs, m_efac);
        if (isOpX<TRUE>(newCand)) return true;
        else return propagate(invNum, newCand, true);
      }
      else
      {
        ExprSet cnjs;
        getConj(newCand, cnjs);
        for (auto & a : cnjs) addCandidate(invNum, a);
        return checkCand(invNum);
      }
    }

    void getArrCandIters(int ind, ExprSet& iters)
    {
      for (auto & a : arrSelects[ind]) iters.insert(a->right());
    }

    bool addCandidate(int invNum, Expr cnd)
    {
      SamplFactory& sf = sfs[invNum].back();
      Expr allLemmas = sf.getAllLemmas();
      if (containsOp<FORALL>(cnd) || containsOp<FORALL>(allLemmas))
      {
        if (containsOp<FORALL>(cnd))
        {
          auto hr = ruleManager.getFirstRuleOutside(decls[invNum]);
          assert(hr != NULL);

          ExprSet cnjs;
          ExprSet newCnjs;
          Expr it = iterators[invNum];
          if (it != NULL)
            cnd = replaceArrRangeForIndCheck (invNum, cnd, hr->body);
        }
        candidates[invNum].push_back(cnd);
        return true;
      }
      if (!isOpX<TRUE>(allLemmas) && u.implies(allLemmas, cnd)) return false;

      for (auto & a : candidates[invNum])
      {
        if (u.isEquiv(a, cnd)) return false;
      }
      candidates[invNum].push_back(cnd);
      return true;
    }

    // hacky helper (to be revisited in the future)
    Expr replaceArrRangeForIndCheck(int invNum, Expr cand, Expr body)
    {
      ExprSet iterVars;
      getArrCandIters(invNum, iterVars);
      Expr precond = preconds[invNum];
      ExprSet cnjs;
      ExprSet newCnjs;
      getConj(cand->last()->left(), cnjs);
      // TODO: support the case when precond has two or more conjuncts
      for (auto & a : iterVars)
        for (auto & b : cnjs)
          if (u.implies(replaceAll(b, a, iterators[invNum]), precond))
            newCnjs.insert(b);

      if (newCnjs.size() != 1) return cand;
      Expr preCand = *newCnjs.begin();
      cnjs.clear();
      newCnjs.clear();
      getConj(body, cnjs);

      for (auto & b : cnjs)
        if (u.isEquiv(mk<NEG>(precond), b)) newCnjs.insert(b);

      Expr bodyRestr = conjoin(newCnjs, m_efac);

      Expr range = mergeIneqs(bodyRestr, preCand);
      if (range == NULL)
      {
        ExprSet qVars;
        ExprSet vars1;
        ExprSet vars2;
        filter (preCand, bind::IsConst (), inserter(vars1, vars1.begin()));
        filter (bodyRestr, bind::IsConst (), inserter(vars2, vars2.begin()));

        for (auto & v1 : vars1)
          if (find(vars2.begin(), vars2.end(), v1) != vars2.end()) qVars.insert(v1);

        if (qVars.empty()) return cand;

        AeValSolver ae(mk<TRUE>(m_efac), mk<AND>(preCand, bodyRestr), qVars);
        if (ae.solve())
        {
          range = ae.getValidSubset();
        }
      }
      cand = replaceAll(cand, preCand, range);
      return cand;
    }

    bool propagate(int invNum, Expr cand, bool seed)
    {
      bool res = true;
      Expr rel = decls[invNum];
      checked.insert(rel);
      for (auto & hr : ruleManager.chcs)
      {
        if (hr.srcRelation == hr.dstRelation || hr.isQuery) continue;
        SamplFactory* sf1;
        SamplFactory* sf2;

        // adding lemmas to the body. GF: not sure if it helps
        Expr constraint = hr.body;
        sf2 = &sfs[getVarIndex(hr.dstRelation, decls)].back();
        Expr lm2 = sf2->getAllLemmas();
        for (auto & v : invarVars[getVarIndex(hr.dstRelation, decls)])
          lm2 = replaceAll(lm2, v.second, hr.dstVars[v.first]);
        constraint = mk<AND>(constraint, lm2);

        if (!hr.isFact)
        {
          sf1 = &sfs[getVarIndex(hr.srcRelation, decls)].back();
          constraint = mk<AND>(constraint, sf1->getAllLemmas());
        }

        // forward:
        if (hr.srcRelation == rel && find(checked.begin(), checked.end(), hr.dstRelation) == checked.end())
        {
          Expr replCand = cand;
          for (int i = 0; i < 3; i++) for (auto & v : sf1->lf.nonlinVars) replCand = replaceAll(replCand, v.second, v.first);
          for (auto & v : invarVars[invNum]) replCand = replaceAll(replCand, v.second, hr.srcVars[v.first]);
          res = res && getCandForAdjacentRel (replCand, constraint, hr.dstVars, hr.dstRelation, seed);
        }

        // backward (very similarly):
        if (hr.dstRelation == rel && find(checked.begin(), checked.end(), hr.srcRelation) == checked.end() && !hr.isFact)
        {
          Expr replCand = cand;
          for (int i = 0; i < 3; i++) for (auto & v : sf2->lf.nonlinVars) replCand = replaceAll(replCand, v.second, v.first);
          for (auto & v : invarVars[invNum]) replCand = replaceAll(replCand, v.second, hr.dstVars[v.first]);
          res = res && getCandForAdjacentRel (replCand, constraint, hr.srcVars, hr.srcRelation, seed);
        }
      }
      return res;
    }

    bool checkCand(int invNum)
    {
      Expr rel = decls[invNum];
      if (!checkInit(invNum, rel)) return false;
      if (!checkInductiveness(rel)) return false;

      return propagate(invNum, conjoin(candidates[invNum], m_efac), false);
    }

    // a simple method to generate properties of a larger Array range, given already proven ranges
    void generalizeArrInvars (SamplFactory& sf)
    {
      if (sf.learnedExprs.size() > 1)
      {
        ExprVector posts;
        ExprVector pres;
        map<Expr, ExprVector> tmp;
        ExprVector tmpVars;
        ExprVector arrVars;
        for (auto & a : sf.learnedExprs)
        {
          if (!isOpX<FORALL>(a)) continue;
          Expr learnedQF = a->last();
          if (!isOpX<IMPL>(learnedQF)) continue;
          ExprVector se;
          filter (learnedQF->last(), bind::IsSelect (), inserter(se, se.begin()));

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
              tmpVars.push_back(bind::intConst(mkTerm<string> ("_tmp_var_" + lexical_cast<string>(index), m_efac)));
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
              Expr tmpIt = bind::intConst(mkTerm<string> ("_tmp_it", m_efac));
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
                  Expr newPost = a.first->right();
                  for (int index = 0; index < tmpVars.size() && index < arrVars.size(); index++)
                    newPost = replaceAll(newPost, tmpVars[index], mk<SELECT>(arrVars[index], it));
                  args.push_back(mk<IMPL>(newPre, newPost));
                  Expr newCand = mknary<FORALL>(args);
                  sf.learnedExprs.insert(newCand);
                }
              }
            }
          }
        }
      }
    }

    void assignPrioritiesForLearned()
    {
//      bool progress = true;
      for (auto & cand : candidates)
      {
        if (cand.second.size() > 0)
        {
          SamplFactory& sf = sfs[cand.first].back();
          for (auto & b : cand.second)
          {
            if (containsOp<FORALL>(b))
            {
              sf.learnedExprs.insert(b);
            }
            else
            {
              ExprVector invVars;
              for (auto & a : invarVars[cand.first]) invVars.push_back(a.second);
              Expr learnedCand = normalizeDisj(b, invVars);
              Sampl& s = sf.exprToSampl(learnedCand);
              sf.assignPrioritiesForLearned();
              if (!u.implies(sf.getAllLemmas(), learnedCand))
                sf.learnedExprs.insert(learnedCand);
            }
//            else progress = false;
          }
        }
      }
//            if (progress) updateGrammars(); // GF: doesn't work great :(
    }

    bool synthesize(int maxAttempts, char * outfile)
    {
      ExprSet cands;
      for (int i = 0; i < maxAttempts; i++)
      {
        // next cand (to be sampled)
        // TODO: find a smarter way to calculate; make parametrizable
        int tmp = ruleManager.cycles[i % ruleManager.cycles.size()][0];
        int invNum = getVarIndex(ruleManager.chcs[tmp].srcRelation, decls);
        checked.clear();
        candidates.clear();
        SamplFactory& sf = sfs[invNum].back();
        Expr cand = sf.getFreshCandidate(i < 25); // try simple array candidates first
        if (cand != NULL && isOpX<FORALL>(cand) && isOpX<IMPL>(cand->last()))
        {
          if (!u.isSat(cand->last()->left())) cand = NULL;
        }
        if (cand == NULL) continue;
//        outs () << " - - - sampled cand: #" << i << ": " << *cand << "\n";

        if (!addCandidate(invNum, cand)) continue;
        if (checkCand(invNum))
        {
          assignPrioritiesForLearned();
          generalizeArrInvars(sf);
          if (checkAllLemmas())
          {
            outs () << "Success after " << (i+1) << " iterations\n";
            return true;
          }
        }
      }
      return false;
    }

    bool splitUnsatSets(ExprVector & src, ExprVector & dst1, ExprVector & dst2)
    {
      if (u.isSat(conjoin(src, m_efac))) return false;

      for (auto & a : src) dst1.push_back(a);

      for (auto it = dst1.begin(); it != dst1.end(); )
      {
        dst2.push_back(*it);
        it = dst1.erase(it);
        if (u.isSat(conjoin(dst1, m_efac))) break;
      }

      // now dst1 is SAT, try to get more things from dst2 back to dst1

      for (auto it = dst2.begin(); it != dst2.end(); )
      {
        if (!u.isSat(conjoin(dst1, m_efac), *it)) { ++it; continue; }
        dst1.push_back(*it);
        it = dst2.erase(it);
      }

      return true;
    }

    bool filterUnsat()
    {
      vector<HornRuleExt*> worklist;
      for (int i = 0; i < invNumber; i++)
      {
        if (!u.isSat(conjoin(candidates[i], m_efac)))
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
        if (!u.isSat(conjoin(candidates[i], m_efac)))
        {
          ExprVector tmp;
          ExprVector stub; // TODO: try greedy search, maybe some lemmas are in stub?
          splitUnsatSets(candidates[i], tmp, stub);
          candidates[i] = tmp;
        }
      }

      return true;
    }

    bool hasQuantifiedCands(map<int, ExprVector>& cands)
    {
      for (auto & a : cands)
      {
        for (auto & b : a.second)
        {
          if (isOpX<FORALL>(b)) return true;
        }
      }
      return false;
    }

    bool multiHoudini(vector<HornRuleExt*> worklist, bool recur = true)
    {
      if (!anyProgress(worklist)) return false;
      map<int, ExprVector> candidatesTmp = candidates;
      bool res1 = true;
      for (auto &h: worklist)
      {
        HornRuleExt& hr = *h;

        if (hr.isQuery) continue;

        if (!checkCHC(hr, candidatesTmp))
        {
          bool res2 = true;
          int ind = getVarIndex(hr.dstRelation, decls);
          Expr model = getModel(hr.dstVars);
          if (model == NULL || hasQuantifiedCands(candidatesTmp))
          {
            // something went wrong with z3. do aggressive weakening (TODO: try bruteforce):
            candidatesTmp[ind].clear();
            res2 = false;
          }
          else
          {
            ExprVector& ev = candidatesTmp[ind];
            ExprVector invVars;
            for (auto & a : invarVars[ind]) invVars.push_back(a.second);
            SamplFactory& sf = sfs[ind].back();

            for (auto it = ev.begin(); it != ev.end(); )
            {
              Expr repl = *it;
              for (auto & v : invarVars[ind]) repl = replaceAll(repl, v.second, hr.dstVars[v.first]);

              if (!u.isSat(model, repl))
              {
                if (hr.isFact)
                {
                  Expr failedCand = normalizeDisj(*it, invVars);
//                outs () << "failed cand for " << *hr.dstRelation << ": " << *failedCand << "\n";
                  Sampl& s = sf.exprToSampl(failedCand);
                  sf.assignPrioritiesForFailed();
                }
                it = ev.erase(it);
                res2 = false;
              }
              else
              {
                ++it;
              }
            }
          }

          if (recur && !res2)
          {
            res1 = false;
            break;
          }
        }
      }
      candidates = candidatesTmp;
      if (!recur) return false;
      if (res1) return anyProgress(worklist);
      else return multiHoudini(worklist);
    }

    // adapted from doSeedMining
    void getSeeds(Expr invRel, map<Expr, ExprSet>& cands, bool analizeCode = true)
    {
      int ind = getVarIndex(invRel, decls);
      SamplFactory& sf = sfs[ind].back();
      ExprSet candsFromCode;
      bool analizedExtras = false;
      bool isFalse = false;

      // for Arrays
      ExprSet tmpArrAccess;
      ExprSet tmpArrSelects;
      ExprSet tmpArrCands;
      ExprSet tmpArrAccessVars;
      for (auto &hr : ruleManager.chcs)
      {
        if (hr.dstRelation != invRel && hr.srcRelation != invRel) continue;

        SeedMiner sm (hr, invRel, invarVars[ind], sf.lf.nonlinVars);
        if (analizeCode) sm.analizeCode();
        else sm.candidates.clear();
        if (!analizedExtras && hr.srcRelation == invRel)
        {
          sm.analizeExtras (cands[invRel]);
          analizedExtras = true;
        }

        for (auto &cand : sm.candidates) candsFromCode.insert(cand);

        // for arrays
        if (analizeCode && ruleManager.hasArrays)
        {
          tmpArrCands.insert(sm.arrCands.begin(), sm.arrCands.end());
          tmpArrSelects.insert(sm.arrSelects.begin(), sm.arrSelects.end());
          tmpArrAccessVars.insert(sm.arrAccessVars.begin(), sm.arrAccessVars.end());
          arrIterRanges[ind].insert(sm.arrIterRanges.begin(), sm.arrIterRanges.end());

          // extra range constraints
          if (sm.arrAccessVars.size() > 0)
          {
            sm.candidates.clear();
            sm.analizeExtras(preconds[ind]);
            for (auto cand : sm.candidates)
            {
              for (auto & iter : tmpArrAccessVars)
              {
                cand = replaceAll(cand, iterators[ind], iter);
                if (!u.implies(conjoin(arrIterRanges[ind], m_efac), cand))
                {
                  arrIterRanges[ind].insert(cand);
                }
              }
            }
          }
        }
      }

      for (auto & a : tmpArrSelects)
      {
        for (auto iter : tmpArrAccessVars)
          arrSelects[ind].insert(replaceAll(a, iterators[ind], iter));
      }
      for (auto & a : tmpArrCands)
      {
        if (!u.isEquiv(a, mk<TRUE>(m_efac)) && !u.isEquiv(a, mk<FALSE>(m_efac)))
          for (auto iter : tmpArrAccessVars)
            arrCands[ind].insert(replaceAll(a, iterators[ind], iter));
      }

      for (auto & cand : candsFromCode)
      {
        checked.clear();
        Expr replCand = cand;
        for (int i = 0; i < 3; i++)
          for (auto & v : sf.lf.nonlinVars)
            replCand = replaceAll(replCand, v.second, v.first);
        if (addCandidate(ind, replCand))
          propagate (ind, replCand, true);
      }
    }

    void refreshCands(map<Expr, ExprSet>& cands)
    {
      cands.clear();
      for (auto & a : candidates)
      {
        cands[decls[a.first]].insert(a.second.begin(), a.second.end());
      }
    }

    bool anyProgress(vector<HornRuleExt*> worklist)
    {
      for (int i = 0; i < invNumber; i++)
      {
        // simple check if there is a non-empty candidate
        for (auto & hr : worklist)
        {
          if (hr->srcRelation == decls[i] || hr->dstRelation == decls[i])
          {
            if (candidates[i].size() > 0)
            {
              return true;
            }
          }
        }
      }
      return false;
    }

#ifdef HAVE_ARMADILLO
    void getDataCandidates(map<Expr, ExprSet>& cands, const vector<string> & behaviorfiles){
      int fileIndex = 0;
      for (auto & dcl : decls) {
        DataLearner dl(ruleManager, m_z3);
        dl.initialize(dcl, true /*multipleLoops*/);
        string filename("");
        if (fileIndex < behaviorfiles.size()) {
          filename = behaviorfiles[fileIndex];
          fileIndex++;
        }
        if (!dl.computeData(filename)) continue;
        (void)dl.computePolynomials(cands[dcl]);
      }
    }
#endif

    bool bootstrap()
    {
      filterUnsat();

      if (multiHoudini(ruleManager.wtoCHCs))
      {
        assignPrioritiesForLearned();
        if (checkAllLemmas())
        {
          outs () << "Success after bootstrapping\n";
          return true;
        }
      }

      // try array candidates one-by-one (adapted from `synthesize`)
      // TODO: batching
      if (ruleManager.hasArrays)
      {
        for (int i = 0; i < invNumber; i++)
        {
          SamplFactory& sf = sfs[i].back();
          for (auto & c : arrCands[i])
          {
            Expr cand = sf.af.getSimplCand(c);
//            outs () << " - - - bootstrapped cand: " << *cand << "\n";

            if (!addCandidate(i, cand)) continue;
            if (checkCand(i))
            {
              assignPrioritiesForLearned();
              generalizeArrInvars(sf);
              if (checkAllLemmas())
              {
                outs () << "Success after bootstrapping\n";
                return true;
              }
            }
          }
        }
      }
      return false;
    }

    void updateGrammars()
    {
      // convert candidates to curCandidates and run the method from RndLearner
      for (int ind = 0; ind < invNumber; ind++)
      {
        if (candidates[ind].size() == 0) curCandidates[ind] = mk<TRUE>(m_efac);
        else curCandidates[ind] = conjoin(candidates[ind], m_efac);
      }
      updateRels();
      updCount++;
    }

    bool checkAllLemmas()
    {
      candidates.clear();
      for (int i = ruleManager.wtoCHCs.size() - 1; i >= 0; i--)
      {
        auto & hr = *ruleManager.wtoCHCs[i];
        if (!checkCHC(hr, candidates)) {
          if (!hr.isQuery)
            assert(0);    // only queries are allowed to fail
          else
            return false; // TODO: use this fact somehow
        }
      }
      return true;
    }

    bool checkCHC (HornRuleExt& hr, map<int, ExprVector>& annotations)
    {
      m_smt_solver.reset();
      m_smt_solver.assertExpr (hr.body);

      if (!hr.isFact)
      {
        int ind = getVarIndex(hr.srcRelation, decls);
        SamplFactory& sf = sfs[ind].back();
        ExprSet lms = sf.learnedExprs;
        for (auto & a : annotations[ind]) lms.insert(a);
        for (auto a : lms)
        {
          for (auto & v : invarVars[ind]) a = replaceAll(a, v.second, hr.srcVars[v.first]);
          if (isOpX<FORALL>(a))
          {
            ExprVector varz;
            for (int i = 0; i < a->arity() - 1; i++)
            {
              varz.push_back(bind::fapp(a->arg(i)));
            }
            m_smt_solver.assertForallExpr(varz, a->last());
          }
          else
          {
            m_smt_solver.assertExpr(a);
          }
        }
      }

      if (!hr.isQuery)
      {
        int ind = getVarIndex(hr.dstRelation, decls);
        SamplFactory& sf = sfs[ind].back();
        ExprSet lms = sf.learnedExprs;
        ExprSet negged;
        for (auto & a : annotations[ind]) lms.insert(a);
        for (auto a : lms)
        {
          for (auto & v : invarVars[ind]) a = replaceAll(a, v.second, hr.dstVars[v.first]);
          negged.insert(mkNeg(a));
        }
        m_smt_solver.assertExpr(disjoin(negged, m_efac));
      }
      return !m_smt_solver.solve ();
    }

    void initArrayStuff(BndExpl& bnd)
    {
      for (int invNum = 0; invNum < decls.size(); invNum++)
      {
        vector<int> cycle;
        ruleManager.getCycleForRel(decls[invNum], cycle);
        if (cycle.size() != 1)
        {
          for (int i = 0; i < cycle.size(); i++) outs () << " " << cycle[i] << ",";
          outs () << "Small-step encoding is not supported currently\n";
          exit(0);     // TODO: support longer cycles
        }
        ExprSet ssa;
        ssas[invNum] = bnd.toExpr(cycle);
        bindvars[invNum] = bnd.bindVars.back();
        getConj(ssas[invNum], ssa);

        filter (ssas[invNum], bind::IsConst (), inserter(qvars[invNum], qvars[invNum].begin()));
        preconds[invNum] = ruleManager.getPrecondition(&ruleManager.chcs[cycle[0]]);
        iterators[invNum] = getEvolvingIntVar(preconds[invNum], ssa);
      }
    }
  };

  inline void learnInvariants3(string smt, char * outfile, int maxAttempts, bool freqs, bool aggp, bool enableDataLearning, const vector<string> & behaviorfiles)
  {
    ExprFactory m_efac;
    EZ3 z3(m_efac);

    CHCs ruleManager(m_efac, z3);
    ruleManager.parse(smt);
    BndExpl bnd(ruleManager);

    if (!ruleManager.hasCycles())
    {
      bnd.exploreTraces(1, ruleManager.chcs.size(), true);
      return;
    }

    RndLearnerV3 ds(m_efac, z3, ruleManager, freqs, aggp);
    map<Expr, ExprSet> cands;
    for (auto& dcl: ruleManager.decls) ds.initializeDecl(dcl);
    if (ruleManager.hasArrays) ds.initArrayStuff(bnd);

    if (enableDataLearning) {
#ifdef HAVE_ARMADILLO
      ds.getDataCandidates(cands, behaviorfiles);
#else
      outs() << "Skipping learning from data as required library (armadillo) not found\n";
#endif
    }

    for (int i = 0; i < ruleManager.cycles.size(); i++)
    {
      Expr pref = bnd.compactPrefix(i);
      cands[ruleManager.chcs[ruleManager.cycles[i][0]].srcRelation].insert(pref);
    }

    for (auto& dcl: ruleManager.wtoDecls) ds.getSeeds(dcl, cands);
    ds.refreshCands(cands);
    for (auto& dcl: ruleManager.decls) ds.doSeedMining(dcl->arg(0), cands[dcl->arg(0)], false);
    ds.calculateStatistics();
    if (ds.bootstrap()) return;
    std::srand(std::time(0));
    ds.synthesize(maxAttempts, outfile);
  }
}

#endif
