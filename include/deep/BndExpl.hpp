#ifndef BNDEXPL__HPP__
#define BNDEXPL__HPP__

#include "Horn.hpp"
#include "Distribution.hpp"
#include "ae/AeValSolver.hpp"
#include <limits>

using namespace std;
using namespace boost;
namespace ufo
{
  class BndExpl
  {
    private:

    ExprFactory &m_efac;
    SMTUtils u;
    CHCs& ruleManager;
    Expr extraLemmas;
    EZ3 z3;
    ZSolver<EZ3> constsmt, constsmtopt;

    ExprVector bindVars1;

    int tr_ind; // helper vars
    int pr_ind;
    int k_ind;

    Expr inv;   // 1-inductive proof

    bool debug;

    public:

    BndExpl (CHCs& r, bool d) :
      m_efac(r.m_efac), ruleManager(r), u(m_efac), z3(m_efac), constsmt(z3), constsmtopt(z3), debug(d) {}

    BndExpl (CHCs& r, int to, bool d) :
      m_efac(r.m_efac), ruleManager(r), u(m_efac, to), z3(m_efac), constsmt(z3, to), constsmtopt(z3, to), debug(d) {}

    BndExpl (CHCs& r, Expr lms, bool d) :
      m_efac(r.m_efac), ruleManager(r), u(m_efac), z3(m_efac), constsmt(z3), constsmtopt(z3), extraLemmas(lms), debug(d) {}

    map<Expr, ExprSet> concrInvs;

    void guessRandomTrace(vector<int>& trace)
    {
      std::srand(std::time(0));
      Expr curRel = mk<TRUE>(m_efac);

      while (curRel != ruleManager.failDecl)
      {
        int range = ruleManager.outgs[curRel].size();
        int chosen = guessUniformly(range);
        int chcId = ruleManager.outgs[curRel][chosen];
        curRel = ruleManager.chcs[chcId].dstRelation;
        trace.push_back(chcId);
      }
    }

    void getAllTraces (Expr src, Expr dst, int len, vector<int> trace, vector<vector<int>>& traces)
    {
      if (len == 1)
      {
        for (auto a : ruleManager.outgs[src])
        {
          if (ruleManager.chcs[a].dstRelation == dst)
          {
            vector<int> newtrace = trace;
            newtrace.push_back(a);
            traces.push_back(newtrace);
          }
        }
      }
      else
      {
        for (auto a : ruleManager.outgs[src])
        {
          vector<int> newtrace = trace;
          newtrace.push_back(a);
          getAllTraces(ruleManager.chcs[a].dstRelation, dst, len-1, newtrace, traces);
        }
      }
    }

    Expr compactPrefix (int num, int unr = 0)
    {
      vector<int> pr = ruleManager.prefixes[num];
      if (pr.size() == 0) return mk<TRUE>(m_efac);

      for (int j = pr.size() - 1; j >= 0; j--)
      {
        vector<int>& tmp = ruleManager.getCycleForRel(pr[j]);
        for (int i = 0; i < unr; i++)
          pr.insert(pr.begin() + j, tmp.begin(), tmp.end());
      }

      pr.push_back(ruleManager.cycles[num][0]);   // we are interested in prefixes, s.t.
                                                  // the cycle is reachable
      ExprVector ssa;
      getSSA(pr, ssa);
      if (!(bool)u.isSat(ssa))
      {
        if (unr > 10)
        {
          do {ssa.erase(ssa.begin());}
          while (!(bool)u.isSat(ssa));
        }
        else return compactPrefix(num, unr+1);
      }

      if (ssa.empty()) return mk<TRUE>(m_efac);

      ssa.pop_back();                              // remove the cycle from the formula
      bindVars.pop_back();                         // and its variables
      Expr pref = conjoin(ssa, m_efac);
      pref = rewriteSelectStore(pref);
      pref = keepQuantifiersRepl(pref, bindVars.back());
      return replaceAll(pref, bindVars.back(), ruleManager.chcs[ruleManager.cycles[num][0]].srcVars);
    }

    vector<ExprVector> bindVars;
    unordered_map<Expr,vector<ExprVector>> allBindVars;
    ExprVector bindVarsEnd;

    Expr toExpr(vector<int>& trace)
    {
      ExprVector ssa;
      getSSA(trace, ssa);
      return conjoin(ssa, m_efac);
    }

    void getSSA(vector<int>& trace, ExprVector& ssa)
    {
      ExprVector bindVars2;
      bindVars.clear();
      ExprVector bindVars1 = ruleManager.chcs[trace[0]].srcVars;
      int bindVar_index = 0;
      int locVar_index = 0;

      for (int s = 0; s < trace.size(); s++)
      {
        auto &step = trace[s];
        bindVars2.clear();
        HornRuleExt& hr = ruleManager.chcs[step];
        assert(hr.srcVars.size() == bindVars1.size());

        Expr body = hr.body;
        if (!hr.isFact && extraLemmas != NULL) body = mk<AND>(extraLemmas, body);
        body = replaceAll(body, hr.srcVars, bindVars1);

        for (int i = 0; i < hr.dstVars.size(); i++)
        {
          bool kept = false;
          for (int j = 0; j < hr.srcVars.size(); j++)
          {
            if (hr.dstVars[i] == hr.srcVars[j])
            {
              bindVars2.push_back(bindVars1[i]);
              kept = true;
            }
          }
          if (!kept)
          {
            Expr new_name = mkTerm<string> ("__bnd_var_" + to_string(bindVar_index++), m_efac);
            bindVars2.push_back(cloneVar(hr.dstVars[i],new_name));
          }

          body = replaceAll(body, hr.dstVars[i], bindVars2[i]);
        }

        for (int i = 0; i < hr.locVars.size(); i++)
        {
          Expr new_name = mkTerm<string> ("__loc_var_" + to_string(locVar_index++), m_efac);
          Expr var = cloneVar(hr.locVars[i], new_name);

          body = replaceAll(body, hr.locVars[i], var);
        }

        ssa.push_back(body);
        bindVars.push_back(bindVars2);
        bindVars1 = bindVars2;
      }
    }

    tribool exploreTraces(int cur_bnd, int bnd, bool print = false)
    {
      if (ruleManager.chcs.size() == 0)
      {
        if (debug) outs () << "CHC system is empty\n";
        if (print) outs () << "Success after complete unrolling\n";
        return false;
      }
      if (!ruleManager.hasCycles())
      {
        if (debug) outs () << "CHC system does not have cycles\n";
        bnd = ruleManager.chcs.size();
      }
      tribool res = indeterminate;
      while (cur_bnd <= bnd)
      {
        if (debug)
        {
          outs () << ".";
          outs().flush();
        }
        vector<vector<int>> traces;
        getAllTraces(mk<TRUE>(m_efac), ruleManager.failDecl, cur_bnd++, vector<int>(), traces);
        bool toBreak = false;
        for (auto &a : traces)
        {
          Expr ssa = toExpr(a);
          res = u.isSat(ssa);
          if (res || indeterminate (res))
          {
            if (debug) outs () << "\n";
            toBreak = true;
            break;
          }
        }
        if (toBreak) break;
      }

      if (debug || print)
      {
        if (indeterminate(res)) outs () << "unknown\n";
        else if (res)
        {
          outs () << "Counterexample of length " << (cur_bnd - 1) << " found";
          Expr model = u.getModel();
          if (model)
            outs () << ":\n" << model << "\n";
          else
            outs () << "\n(Model unknown)\n";
        }
        else if (ruleManager.hasCycles())
          outs () << "No counterexample found up to length " << cur_bnd << "\n";
        else
          outs () << "Success after complete unrolling\n";
      }
      return res;
    }

    bool kIndIter(int bnd1, int bnd2)
    {
      assert (bnd1 <= bnd2);
      assert (bnd2 > 1);
      if (!exploreTraces(bnd1, bnd2))
      {
        outs() << "Base check failed at step " << bnd2 << "\n";
        exit(0);
      }

      k_ind = ruleManager.chcs.size(); // == 3

      for (int i = 0; i < k_ind; i++)
      {
        auto & r = ruleManager.chcs[i];
        if (r.isInductive) tr_ind = i;
        if (r.isQuery) pr_ind = i;
      }

      ruleManager.chcs.push_back(HornRuleExt());   // trick for now: a new artificial CHC
      HornRuleExt& hr = ruleManager.chcs[k_ind];
      HornRuleExt& tr = ruleManager.chcs[tr_ind];
      HornRuleExt& pr = ruleManager.chcs[pr_ind];

      hr.srcVars = tr.srcVars;
      hr.dstVars = tr.dstVars;
      hr.locVars = tr.locVars;

      hr.body = mk<AND>(tr.body, mkNeg(pr.body));

      if (extraLemmas != NULL) hr.body = mk<AND>(extraLemmas, hr.body);

      for (int i = 0; i < hr.srcVars.size(); i++)
      {
        hr.body = replaceAll(hr.body, pr.srcVars[i], hr.srcVars[i]);
      }

      vector<int> gen_trace;
      for (int i = 1; i < bnd2; i++) gen_trace.push_back(k_ind);
      gen_trace.push_back(pr_ind);
      Expr q = toExpr(gen_trace);
      bool res = bool(!u.isSat(q));

      if (bnd2 == 2) inv = mkNeg(pr.body);

      // prepare for the next iteration
      ruleManager.chcs.erase (ruleManager.chcs.begin() + k_ind);

      return res;
    }

    Expr getInv() { return inv; }

    Expr getBoundedItp(int k)
    {
      assert(k >= 0);

      int fc_ind;
      for (int i = 0; i < ruleManager.chcs.size(); i++)
      {
        auto & r = ruleManager.chcs[i];
        if (r.isInductive) tr_ind = i;
        if (r.isQuery) pr_ind = i;
        if (r.isFact) fc_ind = i;
      }

      HornRuleExt& fc = ruleManager.chcs[fc_ind];
      HornRuleExt& tr = ruleManager.chcs[tr_ind];
      HornRuleExt& pr = ruleManager.chcs[pr_ind];

      Expr prop = pr.body;
      Expr init = fc.body;
      for (int i = 0; i < tr.srcVars.size(); i++)
      {
        init = replaceAll(init, tr.dstVars[i],  tr.srcVars[i]);
      }

      Expr itp;

      if (k == 0)
      {
        itp = getItp(init, prop);
      }
      else
      {
        vector<int> trace;
        for (int i = 0; i < k; i++) trace.push_back(tr_ind);

        Expr unr = toExpr(trace);
        for (int i = 0; i < pr.srcVars.size(); i++)
        {
          prop = replaceAll(prop, pr.srcVars[i], bindVars.back()[i]);
        }
        itp = getItp(unr, prop);
        if (itp != NULL)
        {
          for (int i = 0; i < pr.srcVars.size(); i++)
          {
            itp = replaceAll(itp, bindVars.back()[i], pr.srcVars[i]);
          }
        }
        else
        {
          itp = getItp(init, mk<AND>(unr, prop));
        }
      }
      return itp;
    }

    void fillVars(Expr srcRel, ExprVector& srcVars, ExprVector& vars, int l, int s, vector<int>& mainInds, vector<ExprVector>& versVars, ExprSet& allVars, vector<ExprVector>& versVarsUnp, vector<ExprVector>& versVarsP)
    {
      for (int l1 = l; l1 < bindVars.size(); l1 = l1 + s)
      {
        ExprVector vers, versunp, versp;
        int ai = 0;

        for (int i = 0; i < vars.size(); i++) {
          int var = mainInds[i];
          Expr bvar;
          bool wasselect = false;
          if (var >= 0)
          {
            if (ruleManager.hasArrays[srcRel])
              bvar = bindVars[l1-1][var];
            else
              bvar = bindVars[l1][var];
          }
          else
          {
            bvar = replaceAll(vars[i], srcVars, bindVars[l1-1]);
            if (isOpX<SELECT>(bvar))
            {
              vers.push_back(bvar);
              versp.push_back(bvar);
              wasselect = true;
              allVars.insert(bindVars[l1-1][-var-1]);
              ai++;
              // For final row
              allVars.insert(replaceAll(bvar, bindVars[l1-1], bindVars[l1]));
            }
            bvar = replaceAll(bvar, bindVars[l1-1][-var-1], bindVars[l1][-var-1]);
            allVars.insert(bindVars[l1][-var-1]);
            ai++;
          }
          vers.push_back(bvar);
          if (wasselect)
            versunp.push_back(bvar);
          else
          {
            versunp.push_back(bvar);
            versp.push_back(bvar);
          }
        }
        versVars.push_back(vers);
        versVarsUnp.push_back(versunp);
        versVarsP.push_back(versp);
        allVars.insert(vers.begin(), vers.end());
      }
    }

    void getOptimConstr(vector<ExprVector>& versVars, int vs, ExprVector& srcVars,
                            ExprSet& constr, Expr phaseGuard, ExprVector& diseqs)
    {
      for (auto v : versVars)
        for (int i = 0; i < v.size(); i++)
          for (int j = i + 1; j < v.size(); j++)
            diseqs.push_back(mk<ITE>(mk<NEQ>(v[i], v[j]), mkMPZ(1, m_efac), mkMPZ(0, m_efac)));

      for (int i = 0; i < vs; i++)
        for (int j = 0; j < versVars.size(); j++)
          for (int k = j + 1; k < versVars.size(); k++)
            diseqs.push_back(mk<ITE>(mk<NEQ>(versVars[j][i], versVars[k][i]), mkMPZ(1, m_efac), mkMPZ(0, m_efac)));

      Expr extr = disjoin(constr, m_efac);
      if (debug) outs () << "Adding extra constraints to every iteration: " << extr << "\n";
      for (auto & bv : bindVars)
        diseqs.push_back(mk<ITE>(replaceAll(extr, srcVars, bv), mkMPZ(0, m_efac), mkMPZ(1, m_efac)));
      if (phaseGuard != NULL)
        for (auto & bv : bindVars)
          diseqs.push_back(mk<ITE>(replaceAll(phaseGuard, srcVars, bv), mkMPZ(0, m_efac), mkMPZ(1, m_efac)));
    }

    Expr findSelect(int t, int i)
    {
      Expr tr = ruleManager.chcs[t].body;
      ExprVector& srcVars = ruleManager.chcs[t].srcVars;
      ExprVector& dstVars = ruleManager.chcs[t].dstVars;
      ExprVector st;
      filter (tr, IsStore (), inserter(st, st.begin()));
      for (auto & s : st)
      {
        if (!isSameArray(s->left(), srcVars[i])) continue;
        if (!isOpX<INT_TY>(typeOf(s)->last())) continue;
        //if (!hasOnlyVars(s, srcVars)) continue;
        if (contains(s, dstVars[i])) continue;
        return mk<SELECT>(s->left(), s->right());
      }
      st.clear();
      filter (tr, IsSelect (), inserter(st, st.begin()));
      for (auto & s : st)
      {
        if (!isSameArray(s->left(), srcVars[i])) continue;
        if (!isOpX<INT_TY>(typeOf(s->left())->last())) continue;
        //if (!hasOnlyVars(s, srcVars)) continue;
        if (contains(s, dstVars[i])) continue;
        return s;
      }
      return NULL;
    }

    // used for a loop and a phaseGuard
    bool unrollAndExecuteSplitter(
          Expr srcRel,
          ExprVector& invVars,
				  vector<vector<double> >& models,
          Expr phaseGuard, Expr invs, bool fwd, ExprSet& constr, int k = 10)
    {
      assert (phaseGuard != NULL);

      // helper var
      string str = to_string(numeric_limits<double>::max());
      str = str.substr(0, str.find('.'));
      cpp_int max_double = lexical_cast<cpp_int>(str);

      for (int cyc = 0; cyc < ruleManager.cycles.size(); cyc++)
      {
        vector<int> mainInds;
        auto & loop = ruleManager.cycles[cyc];
        ExprVector& srcVars = ruleManager.chcs[loop[0]].srcVars;
        if (srcRel != ruleManager.chcs[loop[0]].srcRelation) continue;
        if (models.size() > 0) continue;

        ExprVector vars, varsMask;
        for (int i = 0; i < srcVars.size(); i++)
        {
          Expr t = typeOf(srcVars[i]);
          if (isOpX<INT_TY>(t))
          {
            mainInds.push_back(i);
            vars.push_back(srcVars[i]);
            varsMask.push_back(srcVars[i]);
          }
          else if (isOpX<ARRAY_TY>(t) && ruleManager.hasArrays[srcRel])
          {
            Expr v = findSelect(loop[0], i);
            if (v != NULL)
            {
              vars.push_back(v);
              mainInds.push_back(-i - 1);  // to be on the negative side
              varsMask.push_back(srcVars[i]);
            }
          }
        }

        if (vars.size() < 2 && cyc == ruleManager.cycles.size() - 1)
          continue; // does not make much sense to run with only one var when it is the last cycle
        invVars = vars;

        auto & prefix = ruleManager.prefixes[cyc];
        vector<int> trace;
        int l = 0;                              // starting index (before the loop)
        if (ruleManager.hasArrays[srcRel]) l++; // first iter is usually useless

        for (int j = 0; j < k; j++)
          for (int m = 0; m < loop.size(); m++)
            trace.push_back(loop[m]);

        ExprVector ssa;
        getSSA(trace, ssa);
        if (fwd)
        {
          ssa.push_back(invs);
          ssa.push_back(replaceAll(phaseGuard, srcVars, bindVars[loop.size() - 1]));
        }
        else
        {
          ssa.push_back(phaseGuard);
          ssa.push_back(replaceAll(invs, srcVars, bindVars[loop.size() - 1]));
        }
        bindVars.pop_back();

        // compute vars for opt constraint
        vector<ExprVector> versVars, unused1, unused2;
        ExprSet allVars;
        ExprVector diseqs;
        fillVars(srcRel, srcVars, vars, l, loop.size(), mainInds, versVars, allVars, unused1, unused2);
        getOptimConstr(versVars, vars.size(), srcVars, constr, phaseGuard, diseqs);

        Expr cntvar = bind::intConst(mkTerm<string> ("_FH_cnt", m_efac));
        allVars.insert(cntvar);
        allVars.insert(bindVars.back().begin(), bindVars.back().end());
        ssa.push_back(mk<EQ>(cntvar, mkplus(diseqs, m_efac)));

        auto res = u.isSat(ssa);
        if (indeterminate(res) || !res)
        {
          if (debug) outs () << "Unable to solve the BMC formula for " <<  srcRel << " and phase guard " << phaseGuard <<"\n";
          continue;
        }
        ExprMap allModels;
        u.getOptModel<GT>(allVars, allModels, cntvar);

        ExprSet phaseGuardVars;
        set<int> phaseGuardVarsIndex; // Get phaseGuard vars here
        filter(phaseGuard, bind::IsConst(), inserter(phaseGuardVars, phaseGuardVars.begin()));
        for (auto & a : phaseGuardVars)
        {
          int i = getVarIndex(a, varsMask);
          assert(i >= 0);
          phaseGuardVarsIndex.insert(i);
        }

        if (debug) outs () << "\nUnroll and execute the cycle for " <<  srcRel << " and phase guard " << phaseGuard <<"\n";

        for (int j = 0; j < versVars.size(); j++)
        {
          vector<double> model;
          if (debug) outs () << "  model for " << j << ": [";
          bool toSkip = false;
          SMTUtils u2(m_efac);
          ExprSet equalities;

          for (auto i: phaseGuardVarsIndex)
          {
            Expr srcVar = varsMask[i];
            Expr bvar = versVars[j][i];
            if (isOpX<SELECT>(bvar)) bvar = bvar->left();
            Expr m = allModels[bvar];
            if (m == NULL) { toSkip = true; break; }
            equalities.insert(mk<EQ>(srcVar, m));
          }
          if (toSkip) continue;
          equalities.insert(phaseGuard);

          if (u2.isSat(equalities)) //exclude models that don't satisfy phaseGuard
          {
            vector<double> model;

            for (int i = 0; i < vars.size(); i++) {
              Expr bvar = versVars[j][i];
              Expr m = allModels[bvar];
              double value;
              if (m != NULL && isOpX<MPZ>(m))
              {
                if (lexical_cast<cpp_int>(m) > max_double ||
                    lexical_cast<cpp_int>(m) < -max_double)
                {
                  toSkip = true;
                  break;
                }
                value = lexical_cast<double>(m);
              }
              else
              {
                toSkip = true;
                break;
              }
              model.push_back(value);
              if (debug) outs () << *bvar << " = " << *m << ", ";
              if (j == 0)
              {
                if (isOpX<SELECT>(bvar))
                  concrInvs[srcRel].insert(mk<EQ>(vars[i]->left(), allModels[bvar->left()]));
                else
                  concrInvs[srcRel].insert(mk<EQ>(vars[i], m));
              }
            }
            if (!toSkip) models.push_back(model);
          }
          else
          {
            if (debug) outs () << "   <  skipping  >      ";
          }
          if (debug) outs () << "\b\b]\n";
        }
      }

      return true;
    }

    unordered_map<Expr,ExprVector> lastssa;
    ExprUMap lastModel;
    unordered_map<Expr,ExprMap> allModels;
    unordered_map<Expr,ExprSet> allVars;
    unordered_map<Expr,bool> noopt;
    typedef vector<vector<double>> DMat;
    //used for multiple loops to unroll inductive clauses k times and collect corresponding models
    //
    //  `invVarsUnp` is {rel -> unprimed variables and selects of arrays}
    //  `invVarsP` is {rel -> unprimed variables and selects of primed arrays}
    //  `modelsUnp` is {rel -> models for vars in `invVarsUnp`}
    //  `modelsP` is {rel -> models for vars in `invVarsP`}
    //  `sels` is {rel -> {variable -> predicate}}
    bool unrollAndExecuteMultiple(
          map<Expr, ExprVector>& invVarsUnp, map<Expr, ExprVector>& invVarsP,
	  //map<Expr, DMat> & modelsUnp, map<Expr, DMat>& modelsP,
          map<Expr, ExprVector>& arrRanges,
          map<Expr, ExprSet>& constr,
          int k = 10)
    {
      map<int, bool> chcsConsidered;
      map<int, Expr> exprModels;
      bool res = false;

      allBindVars.clear();
      allModels.clear();

      for (int cyc = 0; cyc < ruleManager.cycles.size(); cyc++)
      {
        vector<int> mainInds;
        auto & loop = ruleManager.cycles[cyc];
        Expr srcRel = ruleManager.chcs[loop[0]].srcRelation;
        ExprVector& srcVars = ruleManager.chcs[loop[0]].srcVars;
        ExprVector& dstVars = ruleManager.chcs[loop[0]].dstVars;
        if (allBindVars.count(srcRel) != 0)
          continue;

        ExprVector vars, uniqvars, varsUnp, varsP;
        for (int i = 0; i < srcVars.size(); i++)
        {
          Expr t = typeOf(srcVars[i]);
          if (isOpX<INT_TY>(t))
          {
            mainInds.push_back(i);
            vars.push_back(srcVars[i]);
            varsUnp.push_back(srcVars[i]);
            varsP.push_back(srcVars[i]);
            uniqvars.push_back(srcVars[i]);
          }
          else if (isOpX<ARRAY_TY>(t) && ruleManager.hasArrays[srcRel])
          {
            Expr v = findSelect(loop[0], i);
            if (v != NULL)
            {
              vars.push_back(v);
              varsUnp.push_back(v);
              vars.push_back(replaceAll(v, v->left(), dstVars[i]));
              varsP.push_back(vars.back());
              uniqvars.push_back(v);
              mainInds.push_back(-i - 1);  // to be on the negative side
            }
          }
        }

        if (uniqvars.size() < 2 && cyc == ruleManager.cycles.size() - 1)
          continue; // does not make much sense to run with only one var when it is the last cycle
        invVarsUnp[srcRel] = varsUnp;
        invVarsP[srcRel] = varsP;

        auto & prefix = ruleManager.prefixes[cyc];
        vector<int> trace;
        lastModel[srcRel] = mk<TRUE>(m_efac);

        for (int p = 0; p < prefix.size(); p++)
        {
          if (chcsConsidered[prefix[p]] == true)
          {
            Expr lastModelTmp = exprModels[prefix[p]];
            if (lastModelTmp != NULL) lastModel[srcRel] = lastModelTmp;
            trace.clear(); // to avoid CHCs at the beginning
          }
          trace.push_back(prefix[p]);
        }

        int l = trace.size() - 1; // starting index (before the loop)
        if (ruleManager.hasArrays[srcRel]) l++; // first iter is usually useless

        for (int j = 0; j < k; j++)
          for (int m = 0; m < loop.size(); m++)
            trace.push_back(loop[m]);

        int backCHC = -1;
        for (int i = 0; i < ruleManager.chcs.size(); i++)
        {
          auto & r = ruleManager.chcs[i];
          if (i != loop[0] && !r.isQuery && r.srcRelation == srcRel)
          {
            backCHC = i;
            chcsConsidered[i] = true; // entry condition for the next loop
            trace.push_back(i);
            break;
          }
        }

        lastssa[srcRel].clear();
        getSSA(trace, lastssa[srcRel]);

        // compute vars for opt constraint
        vector<ExprVector> versVars, versVarsUnp, versVarsP;
        allVars[srcRel].clear();
        ExprVector diseqs;
        fillVars(srcRel, srcVars, uniqvars, l, loop.size(), mainInds, versVars, allVars[srcRel], versVarsUnp, versVarsP);
        getOptimConstr(versVars, vars.size(), srcVars, constr[srcRel], NULL, diseqs);

        // We need final row (which is special because of how we handle arrays)
        //   in bindVars until here so it shows up in `allVars` from above.
        bindVarsEnd = bindVars.back();
        bindVars.pop_back();
        int traceSz = trace.size();
        assert(bindVars.size() == traceSz - 1);

        Expr cntvar = bind::intConst(mkTerm<string> ("_FH_cnt", m_efac));
        allVars[srcRel].insert(cntvar);
        allVars[srcRel].insert(bindVars.back().begin(), bindVars.back().end());
        allVars[srcRel].insert(bindVarsEnd.begin(), bindVarsEnd.end());
        lastssa[srcRel].insert(lastssa[srcRel].begin(), mk<EQ>(cntvar, mkplus(diseqs, m_efac)));

        // for arrays, make sure the ranges are large enough
        for (auto & v : arrRanges[srcRel])
          lastssa[srcRel].insert(lastssa[srcRel].begin(), replaceAll(mk<GEQ>(v, mkMPZ(k, m_efac)), srcVars, bindVars[0]));

        bool toContinue = false;
        noopt[srcRel] = false;
        while (true)
        {
          if (bindVars.size() <= 1)
          {
            if (debug) outs () << "Unable to find a suitable unrolling for " << *srcRel << "\n";
            toContinue = true;
            break;
          }

          if (u.isSat(lastModel[srcRel], conjoin(lastssa[srcRel], m_efac)))
          {
            if (backCHC != -1 && trace.back() != backCHC &&
                trace.size() != traceSz - 1) // finalizing the unrolling (exit CHC)
            {
              trace.push_back(backCHC);
              lastssa[srcRel].clear();                   // encode from scratch
              getSSA(trace, lastssa[srcRel]);
              bindVars.pop_back();
              noopt[srcRel] = true;   // TODO: support optimization queries
            }
            else break;
          }
          else
          {
            noopt[srcRel] = true;      // TODO: support
            if (trace.size() == traceSz)
            {
              trace.pop_back();
              lastssa[srcRel].pop_back();
              bindVars.pop_back();
            }
            else
            {
              trace.resize(trace.size()-loop.size());
              lastssa[srcRel].resize(lastssa[srcRel].size()-loop.size());
              bindVars.resize(bindVars.size()-loop.size());
            }
          }
        }

        if (toContinue) continue;
        res = true;

        getModel(srcRel);

        /*getMat(srcRel, invVarsUnp[srcRel], modelsUnp[srcRel], NULL);
        getMat(srcRel, invVarsP[srcRel], modelsP[srcRel], NULL);*/

        // although we care only about integer variables for the matrix above,
        // we still keep the entire model to bootstrap the model generation for the next loop
        if (chcsConsidered[trace.back()])
        {
          ExprSet mdls;
          for (auto & a : bindVars.back())
            if (allModels[srcRel][a] != NULL)
              mdls.insert(mk<EQ>(a, allModels[srcRel][a]));
          exprModels[trace.back()] = replaceAll(conjoin(mdls, m_efac),
            bindVars.back(), ruleManager.chcs[trace.back()].srcVars);
        }
        assert(allBindVars.count(srcRel) == 0);
        allBindVars.emplace(srcRel, bindVars);
        if (backCHC == -1)
          allBindVars[srcRel].push_back(bindVarsEnd);
      }

      return res;
    }

    void getModel(const Expr& srcRel)
    {
      Expr cntvar = bind::intConst(mkTerm<string> ("_FH_cnt", m_efac));
      ExprMap &allModelsRel = allModels[srcRel];
      if (noopt[srcRel])
        u.getModel(allVars[srcRel], allModelsRel);
      else
        u.getOptModel<GT>(allVars[srcRel], allModelsRel, cntvar, true);
    }

    template <typename OptOp>
    bool getOptModel(const Expr& srcRel, const Expr& constr, 
      const Expr& optvar, const Expr& optstart, unsigned maxIters = -1)
    {
      auto& allBV = allBindVars[srcRel];
      ExprVector& srcVars = ruleManager.invVars[srcRel];
      ExprVector& dstVars = ruleManager.invVarsPrime[srcRel];
      Expr ssa = conjoin(lastssa[srcRel], m_efac);

      ExprVector fullconstr;
      for (int j = 0; j < allBV.size() - 1; j++)
        fullconstr.push_back(replaceAll(replaceAll(constr,
            srcVars, allBV[j]),
          dstVars, allBV[j + 1]));

      if (optstart) allModels[srcRel][optvar] = optstart;

      if (u.isSat(lastModel[srcRel], ssa, conjoin(fullconstr, m_efac)))
        return u.getOptModel<OptOp>(allVars[srcRel], allModels[srcRel],
          optvar, false, maxIters);
      else
        return false;
    }

    Expr lastsels = NULL;
    // Return matrix for previous unrolling (via `unrollAndExecuteMultiple`)
    //   in `outmodel`, using `sels` to filter invalid rows.
    // May automatically regenerate model via `getModel`, but
    //   guaranteed not to if `sels` is same as previous call.
    void getMat(Expr srcRel, ExprVector& invVars, DMat & outmodel, Expr sels)
    {
      ExprVector& srcVars = ruleManager.invVars[srcRel];
      ExprVector& dstVars = ruleManager.invVarsPrime[srcRel];

      // helper var
      string str = to_string(numeric_limits<double>::max());
      str = str.substr(0, str.find('.'));
      cpp_int max_double = lexical_cast<cpp_int>(str);
      map<Expr, ExprSet> ms;
      ExprMap& allModelsRel = allModels[srcRel];
      auto& allBV = allBindVars[srcRel];

      auto replaceBV = [&] (const Expr &e, ExprVector::size_type row)
      {
        return replaceAll(replaceAll(e,
            srcVars, allBV[row]),
          dstVars, allBV[min(allBV.size() - 1, row + 1)]);
      };

      if (sels != lastsels)
      {
        // Cached model is invalid, generate a new one.
        lastsels = sels;
        Expr ssa = conjoin(lastssa[srcRel], m_efac);
        ExprVector fullsels;
        for (int j = 0; j < allBV.size() - 1; j++)
          fullsels.push_back(replaceBV(sels, j));
        if (u.isSat(lastModel[srcRel], ssa, conjoin(fullsels, m_efac)))
          getModel(srcRel);
        else if (u.isSat(lastModel[srcRel], ssa, disjoin(fullsels, m_efac)))
          getModel(srcRel);
        else
          return;
      }

      if (debug) outs () << "\nUnroll and execute the cycle for " <<  srcRel << "\n";
      for (int j = 0; j < allBV.size(); j++)
      {
        vector<double> model;
        bool toSkip = false;
        if (debug) outs () << "  model for " << j << ": [";

        auto dobvar = [&] (Expr& normvar,Expr& bvar)
        {
          Expr m = allModelsRel[bvar];
          double value;
          if (m != NULL && isOpX<MPZ>(m))
          {
            if (lexical_cast<cpp_int>(m) > max_double ||
                lexical_cast<cpp_int>(m) < -max_double)
            {
              toSkip = true;
              return false;
            }
            value = lexical_cast<double>(m);
          }
          else
          {
            toSkip = true;
            return false;
          }
          model.push_back(value);
          if (!containsOp<ARRAY_TY>(bvar))
            ms[normvar].insert(mk<EQ>(normvar, m));
          return true;
        };

        for (int i = 0; i < invVars.size(); i++)
        {
          Expr bvar = replaceBV(invVars[i], j);
          if (!dobvar(invVars[i], bvar))
            break;
          // Filter rows that don't make `sels` true.
          if (sels && !isOpX<TRUE>(sels))
          {
            Expr bsel = replaceBV(sels, j);
            bsel = replaceAll(bsel, allModelsRel);
            bsel = simplifyBool(simplifyArithm(bsel));
            if (isOpX<FALSE>(bsel))
            {
              toSkip = true;
              break;
            }
          }
        }

        if (toSkip)
        {
          if (debug) outs () << "\b\b   <  skipping  >      ]\n";
        }
        else
        {
          outmodel.push_back(model);
          if (debug)
          {
            for (int i = 0; i < invVars.size(); ++i)
            {
              Expr bvar = replaceBV(invVars[i], j);
              Expr m = allModelsRel[bvar];
              outs () << *bvar << " = " << *m << ", ";
            }
            outs () << "\b\b]\n";
          }
        }
      }

      for (auto & a : ms)
        concrInvs[srcRel].insert(simplifyArithm(disjoin(a.second, m_efac)));

    }


    // (Maybe) Finds an assignment to the variables listed in `vars`
    //   which satisfy the constraint listed in `constr`,
    //   where `constr` must hold on each step of the unrolling
    //   (i.e. we duplicate `constr` for each row of SSA and conjoin to SSA).
    // 
    // This version additionally tries to optimize the variables given
    //   in `optvars` according to the template parameter `OptimOp`
    //   (an ExprOp). If no optimal candidate is found, returns an
    //   unoptimal one.
    //
    // `inv` is the predicate to consider.
    // `extraconstr` is an additional set of constraints (not duplicated
    // over SSA) to add to the problem, over `vars`.
    //
    // Returns a model (v \in vars -> Expr) if one found, or an empty
    // map otherwise.
    //
    // Must be called after `unrollAndExecuteMultiple()`.
    ExprVector vardst, varsrc;
    template <typename OptimOp, typename Range>
    ExprUMap findConsts2(const Expr& inv, const Expr& constr,
      const ExprVector& extraconstr, const Range& vars, const Range& optvars)
    {
      assert(lastssa.count(inv) != 0);
      vardst.clear(); varsrc.clear();
      const ExprVector& srcVars = ruleManager.invVars[inv];
      const ExprVector& dstVars = ruleManager.invVarsPrime[inv];
      ExprUMap ret;
      assert(allBindVars.count(inv) != 0);
      auto& bvars = allBindVars[inv];
      ExprVector bndconstr(bvars.size() - 1);
      for (int i = 0; i < bvars.size() - 1; ++i)
      {
        bndconstr[i] = replaceAll(constr, srcVars, bvars[i]);
        bndconstr[i] = replaceAll(bndconstr[i], dstVars, bvars[i+1]);
      }
      // All rows of rewritten `constr`.
      Expr allbndconstr = conjoin(bndconstr, m_efac);
      Expr alllastssa = conjoin(lastssa[inv], m_efac);
      // Note lack of universal quantifier.
      Expr tocheck = mk<AND>(allbndconstr, alllastssa);
      ExprVector faArgs;
      ExprVector freshvars;
      for (const Expr& v : vars)
      {
        varsrc.push_back(v);
        vardst.push_back(v);
      }
      for (const Expr& v : optvars)
      {
        faArgs.push_back(v->left());
        // Create a fresh variable for each optimized variable.
        // This will be the one we look for in the model, with the intention
        //   being that it is smaller/bigger than all `v` that satsify
        //   `constr`.
        freshvars.push_back(mkConst(
          mkTerm(getTerm<string>(v->left()->left())+"!", m_efac),
          v->left()->last()));
        varsrc.push_back(freshvars.back());
        vardst.push_back(v);
      }
      ExprVector optconstop;
      for (int i = 0; i < freshvars.size(); ++i)
      {
        // c! < c (for OptimOp == LT)
        optconstop.push_back(mk<OptimOp>(freshvars[i], mk<FAPP>(faArgs[i])));
      }

      // The SMT solver we use for non-optimizing checks.
      constsmt.reset();
      // (all rows of constr) /\ (all rows of SSA)
      constsmt.assertExpr(tocheck);
      // \foreach{v \in optvars}. v! < v
      constsmt.assertExpr(conjoin(optconstop, m_efac));

      faArgs.push_back(mk<IMPL>(allbndconstr, conjoin(optconstop, m_efac)));
      // \foreach{v \in optvars}. \exists{v!}. \forall{v}. constr => v! < v
      Expr fullcheck = mknary<FORALL>(faArgs.begin(), faArgs.end());

      // The SMT solver we use for optimizing checks.
      constsmtopt.reset();
      constsmtopt.assertExpr(tocheck);
      constsmtopt.assertExpr(fullcheck);
      return findNextConst(extraconstr);
    }

    // (Maybe) Finds an assignment to the variables listed in `vars`
    //   which satisfy the constraint listed in `constr`,
    //   where `constr` must hold on each step of the unrolling
    //   (i.e. we duplicate `constr` for each row of SSA and conjoin to SSA).
    //
    // `inv` is the predicate to consider.
    // `extraconstr` is an additional set of constraints (not duplicated
    // over SSA) to add to the problem, over `vars`.
    //
    // Returns a model (v \in vars -> Expr) if one found, or an empty
    // map otherwise.
    //
    // Must be called after `unrollAndExecuteMultiple()`.
    template <typename Range>
    ExprUMap findConsts(const Expr& inv, const Expr& constr,
      const ExprVector& extraconstr, const Range& vars)
    {
      assert(lastssa.count(inv) != 0);
      vardst.clear(); vardst.insert(vardst.end(), vars.begin(), vars.end());
      varsrc = vardst;
      const ExprVector& srcVars = ruleManager.invVars[inv];
      const ExprVector& dstVars = ruleManager.invVarsPrime[inv];
      ExprUMap ret;
      assert(allBindVars.count(inv) != 0);
      auto& bvars = allBindVars[inv];
      ExprVector bndconstr(bvars.size());
      ExprVector faArgs;
      for (int i = 0; i < bvars.size(); ++i) {
        for (int j = 0; j < bvars[i].size(); ++j)
          // Universally-quantify all SSA variables.
          faArgs.push_back(bvars[i][j]->left());
        // Rewrite and add `constr` for each row of SSA.
        bndconstr[i] = replaceAll(constr, srcVars, bvars[i]);
        bndconstr[i] = replaceAll(bndconstr[i], dstVars, bvars[i+1]);
      }

      Expr lastssa_conj = conjoin(lastssa[inv], m_efac);
      Expr bndconstr_conj = conjoin(bndconstr, m_efac);
      faArgs.push_back(mk<IMPL>(lastssa_conj, bndconstr_conj));
      // \forall{SSA vars}. SSA => (rewrite of constr)
      Expr tocheck = mknary<FORALL>(faArgs.begin(), faArgs.end());
      constsmt.reset();
      constsmt.assertExpr(tocheck);
      constsmt.assertExpr(lastssa_conj);
      constsmt.assertExpr(bndconstr_conj);
      return findNextConst(extraconstr);
    }

    // Uses the `inv`, `constr`, and `vars` of the previous call to
    //   `findConsts` or `findConsts2` to find a model involving the
    //   additional set of constraints `newconstr`. If `newconstr` is empty,
    //   will find the same model.
    //
    // Ignores any `extraconstr` and any prior `newconstr` passed.
    //
    // Returns model of same format as `findConsts[2]`, empty if none.
    ExprUMap findNextConst(const ExprVector& newconstr, bool donoopt=true)
    {
      ExprUMap ret;
      ZSolver<EZ3>& smt = donoopt ? constsmt : constsmtopt;
      bool didpush = false;
      if (newconstr.size() > 0)
      {
        didpush = true;
        smt.push();
        smt.assertExpr(conjoin(newconstr, m_efac));
      }
      tribool solveret = smt.solve();
      if (indeterminate(solveret))
      {
        //outs() << "Warning: unknown in findConsts" << endl;
        if (didpush) smt.pop();
        if (donoopt)
          return findNextConst(newconstr, false);
        else
          return ret;
      }
      else if (!solveret)
      {
        if (didpush) smt.pop();
        if (donoopt)
          return findNextConst(newconstr, false);
        else
          return ret;
      }
      auto m = smt.getModelPtr();
      if (!m)
      {
        if (didpush) smt.pop();
        if (donoopt)
          return findNextConst(newconstr, false);
        else
          return ret;
      }

      assert(vardst.size() == varsrc.size());
      for (int i = 0; i < varsrc.size(); ++i)
        ret[vardst[i]] = m->eval(varsrc[i]);
      free(m);
      if (didpush) smt.pop();
      return ret;
    }
  };

  inline void unrollAndCheck(string smt, int bnd1, int bnd2, int to, bool skip_elim, int debug)
  {
    ExprFactory m_efac;
    EZ3 z3(m_efac);
    CHCs ruleManager(m_efac, z3, debug);
    if (!ruleManager.parse(smt, !skip_elim)) return;
    BndExpl bnd(ruleManager, to, debug);
    bnd.exploreTraces(bnd1, bnd2, true);
  };

  inline bool kInduction(CHCs& ruleManager, int bnd)
  {
    if (ruleManager.chcs.size() != 3)
    {
      outs () << "currently not supported\n";
      return false;
    }

    BndExpl ds(ruleManager, false);

    bool success = false;
    int i;
    for (i = 2; i < bnd; i++)
    {
      if (ds.kIndIter(i, i))
      {
        success = true;
        break;
      }
    }

    outs () << "\n" <<
      (success ? "K-induction succeeded " : "Unknown result ") <<
      "after " << (i-1) << " iterations\n";

    return success;
  };

  inline void kInduction(string smt, int bnd)
  {
    ExprFactory m_efac;
    EZ3 z3(m_efac);
    CHCs ruleManager(m_efac, z3);
    ruleManager.parse(smt);
    kInduction(ruleManager, bnd);
  };
}

#endif
