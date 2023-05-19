#ifndef DATALEARNER__HPP__
#define DATALEARNER__HPP__

// currently only polynomials upto degree 2 is supported

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <vector>
#include <climits>
#include <ctime>

#include "armadillo"

#include "Horn.hpp"
#include "BndExpl.hpp"

using namespace std;
using namespace boost;

namespace ufo
{

  const double approxEqualTol = 0.001;
  const char approxEqualMethod[] = "absdiff";

  enum loglevel {NONE, ERROR, INFO, DEBUG};

  unsigned int LOG_LEVEL = INFO;

  template <typename T>
  void printmsg(T t)
  {
    std::cout << t << std::endl;
  }

  template <typename T, typename... Args>
  void printmsg(T t, Args... args)
  {
    std::cout << t << " ";
    printmsg(args...);
  }

  template <typename... Args>
  void printmsg(loglevel level, Args... args)
  {
    if (level <= LOG_LEVEL) {
      printmsg(args...);
    }
  }

  class DataLearner
  {

  private:

    CHCs& ruleManager;
    BndExpl bnd;
    ExprFactory &m_efac;
    unsigned int curPolyDegree;

    arma::mat
    computeMonomial(arma::mat dataMatrix)
    {
      arma::mat monomialMatrix;
      monomialMatrix.fill(0);
      if (curPolyDegree == 1) {
        monomialMatrix.set_size(dataMatrix.n_rows, dataMatrix.n_cols);
        for (int i = 0; i < dataMatrix.n_rows; i++) {
          for (int j = 0; j < dataMatrix.n_cols; j++) {
            monomialMatrix(i,j) = dataMatrix(i,j);
          }
        }
            } else {
            //compute all monomials upto degree 2
        monomialMatrix.set_size(dataMatrix.n_rows, (dataMatrix.n_cols * (dataMatrix.n_cols+1)) / 2);
        for (int i = 0; i < dataMatrix.n_rows; i++) {
          for(int j = 0, dmcol=0; j < dataMatrix.n_cols; j++) {
            for (int k = j; k < dataMatrix.n_cols; k++, dmcol++) {
              monomialMatrix(i, dmcol) = dataMatrix(i,j) * dataMatrix(i, k);
            }
          }
        }
      }

      return monomialMatrix;
    }

    arma::mat
    gaussjordan(arma::mat input)
    {
      unsigned int cur_row = 0;
      unsigned int cur_col = 0;
      arma::vec rowToPivot = arma::vec(input.n_rows);
      const unsigned int UNDEFINED_PIVOT = 100;

      rowToPivot.fill(UNDEFINED_PIVOT);

      printmsg(DEBUG, "Before row\n", input);

      //row echleon form
      while (cur_col < input.n_cols && cur_row < input.n_rows) {

	if (input(cur_row, cur_col) == 0) {
	  unsigned int next_nonzero;
	  for (next_nonzero = cur_row; next_nonzero < input.n_rows; next_nonzero++) {
	    if (input(next_nonzero, cur_col) != 0) {
	      break;
	    }
	  }
	  if (next_nonzero == input.n_rows) {
	    cur_col++;
	    continue;
	  } else {
	    input.swap_rows(cur_row, next_nonzero);
	  }
	}

	if (input(cur_row, cur_col) != 1) {
	  double inverse = 1/input(cur_row, cur_col);
	  for (unsigned int k = cur_col; k < input.n_cols; k++) {
	    input(cur_row, k) = input(cur_row,k)*inverse;
	  }
	}

	for (unsigned int j = cur_row+1; j < input.n_rows; j++) {
	  double f = input(j, cur_col)/input(cur_row, cur_col);
	  for (unsigned int k = 0; k < input.n_cols; k++) {
	    input(j,k) = input(j,k) - input(cur_row, k)*f;
	  }
	  input(j,cur_col) = 0;
	}

	rowToPivot(cur_row) = cur_col;

	cur_col++;
	cur_row++;
      }

      rowToPivot(input.n_rows-1) = input.n_cols-1;

      //reduced row echloen form
      if (cur_row != input.n_rows) {
	//we have found a zero row before we reached last row
	cur_row = cur_row-1;
      } else {
	cur_row = input.n_rows-1;
      }

      cur_col = rowToPivot(cur_row);

      while (cur_row < input.n_rows) {

	cur_col = rowToPivot(cur_row);

	if (cur_col == UNDEFINED_PIVOT || input(cur_row,cur_col) == 0) {
	  cur_row--;
	  continue;
	}

	for (unsigned int j = cur_row-1; j < input.n_rows; j--) {
	  double f = input(j,cur_col)/input(cur_row,cur_col);
	  for (unsigned int k = 0; k < input.n_cols; k++) {
	    input(j,k) = input(j,k) - input(cur_row,k)*f;
	  }
	}
	cur_row--;
      }

      //      printmsg(INFO, "after row reduced\n", input);

      std::vector<unsigned int> independentVars;

      for (unsigned col = 0; col < input.n_cols; col++) {
	if (col < input.n_rows && input(col, col) == 0) {
	  independentVars.push_back(col);
	}
      }

      arma::mat basis(input.n_cols, independentVars.size());
      basis.fill(0);
      unsigned int basis_col = 0;

      for (auto indVar : independentVars) {
	for (unsigned int row = 0; row < input.n_rows; row++) {
	  if (rowToPivot[row] == UNDEFINED_PIVOT) {
	    continue;
	  }
	  //TODO: replace -2 with lcm of column
	  //printmsg(DEBUG, input(row,indVar), row, indVar);
	  basis(rowToPivot(row), basis_col) = -1*input(row, indVar);
	}
	basis(indVar,basis_col)=1;
	basis_col++;
      }

      return basis;
    }

    void
    computetime(const string & msg, clock_t & start)
    {
      printmsg(DEBUG, msg, (clock() - start)/(CLOCKS_PER_SEC/1000.0));
      start = clock();
    }

    int
    algExprFromBasis(const arma::mat & basis, vector<Expr> & polynomials, map<unsigned int, Expr>& monomialToExpr)
    {

      /*TODO: fix this; not working for 8.c*/
      // if (arma::all(arma::vectorise(basis))) {
      // 	return 1;
      // }

      // create equations of the form a_1*x_1 + a_2*x_2 + ... = 0
      // where a_1, a_2, etc are from basis columns values
      // and x_1, x_2 etc are monomials from corresponding basis' rows
      // to disallow unsound invariants like a_1 = 0 add only candidates with atleast two terms
      Expr zero = mkTerm(mpz_class(0), m_efac);
      for (int col = 0; col < basis.n_cols; col++) {
        int numTerms = 0;
        Expr poly = nullptr;
        bool toCont = false;
        double coef = 1;
        double intpart;
        for (int row = 0; row < basis.n_rows; row++) {
//          //coeffcient is stored in a stream first to avoid incorrect type conversion error
//          std::stringstream coeffStream;
//          coeffStream << std::fixed << basis(row,col);
          double cur = basis(row,col) * coef;
//          cout <<std::fixed << cur << " * v_" << row << " + ";

//          Expr abstractVar = nullptr;
//          if (!allowedPolyCoefficient(basis(row,col), abstractVar)) {

          Expr mult;
          Expr monomialExpr = monomialToExpr[row];

          if (std::modf(cur, &intpart) != 0.0) {
            double c = (1/cur);
            if (std::modf(c, &intpart) == 0.0) {
              cur = 1;
              cpp_int c1 = lexical_cast<cpp_int>(c);
              if (poly != nullptr) poly = mk<MULT>(mkMPZ(c1, m_efac), poly);
              mult = mk<MULT>(mkMPZ(lexical_cast<cpp_int>(cur), m_efac), monomialToExpr[row]);
              coef *= c;
            } else {
              toCont = true;
              poly = nullptr;
              break;
            }
          }
          else
/*        if (abstractVar != nullptr && (isNumericConst(monomialExpr) || curPolyDegree > 1)) {
            if (!isNumericConst(monomialExpr)) {
              mult = mk<MULT>(abstractVar, monomialExpr);
            } else {
              int monomialInt = lexical_cast<int>(monomialExpr);
              //assumption is that abstractVar will be of the form intConst * var or var * intConst
              bool success = true;
              Expr var = nullptr;
              cpp_int varCoeff = 1;
              if (!isOpX<MULT>(abstractVar)) {
                success = false;
              } else {
                for (auto it = abstractVar->args_begin(), end = abstractVar->args_end(); it != end; ++it) {
                  if (isNumericConst(*it)) {
                    varCoeff = lexical_cast<cpp_int>(*it);
                  } else if (bind::isIntConst(*it)) {
                    var = *it;
                  } else {
                    success = false;
                  }
                }
              }
              if (!success || var == nullptr) {
                mult = mk<MULT>(abstractVar, monomialExpr);
              } else {
                mult = mk<MULT>(mkMPZ(varCoeff*monomialInt, m_efac), var);
              }
            }
          } else */
          {
            mult = mk<MULT>(mkMPZ(lexical_cast<cpp_int>(cur), m_efac), monomialToExpr[row]);
          }

          if (poly != nullptr) {
            poly = mk<PLUS>(poly, mult);
          } else {
            poly = mult;
          }
          numTerms++;
        }
        if (toCont) continue;
        if (poly != nullptr && numTerms > 1) {
          poly = mk<EQ>(poly, zero);
          polynomials.push_back(poly);
        }
      }

      return 0;
    }

    void
    addpolytocands(ExprSet & cands, Expr poly)
    {
      cands.insert(poly);
    }

    void
    addpolytocands(ExprVector & cands, Expr poly)
    {
      cands.push_back(poly);
    }

    int
    initInvVars(Expr invDecl, ExprVector& vars, map<unsigned int, Expr>& monomialToExpr)
    {
      int numVars = 0;
      monomialToExpr.insert(pair<unsigned int, Expr>(0, mkTerm(mpz_class(1), m_efac)));

      for (Expr i : vars) {
        numVars++;
        monomialToExpr.insert(pair<unsigned int, Expr>(numVars, i));
      }

      //degree 2 monomials
      for (unsigned int vIndex1 = 1, mIndex = numVars+1; vIndex1 <= numVars; vIndex1++) {
        for (int vIndex2 = vIndex1; vIndex2 <= numVars; vIndex2++) {
          monomialToExpr.insert(std::pair<unsigned int, Expr>(mIndex,
                        mk<MULT>(monomialToExpr[vIndex1],
                           monomialToExpr[vIndex2])));
          mIndex++;
        }
      }

      return numVars;
    }

    // adds monomial and constant multiples of it if corresponding
    // monomial column in datamatrix is constant
    void
    initLargeCoeffToExpr(arma::mat dataMatrix)
    {
      // first column is 1's
      for (unsigned int col = 1; col < dataMatrix.n_cols; col++) {

        double tmp = dataMatrix(0, col);
        unsigned int row;

        for (row = 1; row < dataMatrix.n_rows; row++) {
          if (!arma::approx_equal(arma::vec(1).fill(dataMatrix(row, col)), arma::vec(1).fill(tmp),
                approxEqualMethod, approxEqualTol)) {
              break;
            }
        }

//        if (row != dataMatrix.n_rows) {
//          continue;
//        }
//
//        Expr var = monomialToExpr[col];
//        for (int multiple = 1; multiple < 4; multiple++) {
//          Expr val1 = mk<MULT>(mkTerm(mpz_class(multiple), m_efac), var);
//          Expr val2 = mk<MULT>(mkTerm(mpz_class(-1*multiple), m_efac), var);
//          largeCoeffToExpr.insert(make_pair(multiple*tmp, val1));
//          largeCoeffToExpr.insert(make_pair(-1*multiple*tmp, val2));
//        }

      }
    }

    // return true only if all the data satisfies basis
    bool
    checkBasisSatisfiesData(arma::mat monomial, arma::vec basis)
    {
      if (monomial.n_cols != basis.n_elem) {
        return false;
      }

      arma::rowvec basisRow = arma::conv_to<arma::rowvec>::from(basis);

      for (int row = 0; row < monomial.n_rows; row++) {
        double sum = 0;
        for (int col = 0; col < monomial.n_cols; col++) {
          sum += basisRow(col) * monomial(row, col);
        }
        if (!arma::approx_equal(arma::vec(1).fill(sum), arma::vec(1).fill(0),
              approxEqualMethod, approxEqualTol)) {
          return false;
        }
      }

      return true;

    }

    template <class CONTAINERT>
    int
    getPolynomialsFromData(const arma::mat & data, CONTAINERT & cands, Expr inv, const ExprVector& invVars, map<unsigned int, Expr>& monomialToExpr, Expr assume = nullptr)
    {
      ExprSet polynomialsComputed;
      vector<arma::mat> basisComputed;
      basisComputed.push_back(arma::mat());
      basisComputed.push_back(arma::mat());

      if (data.n_elem == 0) {
        return -1;
      }

      clock_t start = clock();

      arma::mat monomialMatrix = computeMonomial(data);

      arma::mat basis = gaussjordan(monomialMatrix);

      //      printmsg(INFO, "before basis check ", basis);

      if (basis.n_cols == 0) {
        return 0;
      }

      //      cout << endl << basis << endl; //DEBUG

      // computetime("basis computation time ", start);

      // check if column of basis is unique
      if (assume == nullptr) {
        for (int col = 0; col < basis.n_cols; col++) {
          int oldcol;
          for (oldcol = 0; oldcol < basisComputed[curPolyDegree].n_cols; oldcol++) {
            if (arma::approx_equal(basis.col(col), basisComputed[curPolyDegree].col(oldcol),
                 approxEqualMethod, approxEqualTol)) {
              basis.shed_col(col);
              break;
            }
          }
        }

        for (int col = 0; col < basis.n_cols; col++) {
          basisComputed[curPolyDegree].insert_cols(basisComputed[curPolyDegree].n_cols, basis.col(col));
        }
      }

      // computetime("data unique check time ", start);

      // for some reason previous monomialmatrix is overwritten so copy to a different matrix
      arma::mat monomialMatrix2 = computeMonomial(data);
      for (int col = 0; col < basis.n_cols; col++) {
        if (!checkBasisSatisfiesData(monomialMatrix2, basis.col(col))) {
          basis.shed_col(col);
          continue;
        }

      }

      // if (assume != nullptr) {
      // 	printmsg(INFO, "\n dataMatrix \n", dataMatrix);
      // 	printmsg(INFO, "\n data \n", data);
      // 	printmsg(INFO, "\n monomial \n", monomialMatrix);
      // 	printmsg(INFO, "\n basis \n", basis);
      // }

      vector<Expr> polynomials;
      polynomials.reserve(basis.n_cols);

      if (!algExprFromBasis(basis, polynomials, monomialToExpr)) {
        const ExprVector& srcVars = ruleManager.invVars[inv],
                         &dstVars = ruleManager.invVarsPrime[inv];
        for (auto poly : polynomials) {
          Expr cand = (assume == nullptr) ? poly : mk<IMPL>(assume, poly);
          cand = replaceAll(cand, dstVars, srcVars);
          if (polynomialsComputed.find(cand) == polynomialsComputed.end()) {
            addpolytocands(cands, cand);
            polynomialsComputed.insert(cand);
            printmsg(DEBUG, "Adding polynomial: ", cand);
          }
        }

        //getAllEqs(inv, invVars, smtcands);
        //getAllIneqs(inv, invVars, cands);

        computetime("poly conversion time ", start);

        return polynomials.size();
      }

      return 0;

    }

    // Produces templates of the form: c1*v1 + c2*v2 + ... > cc,
    //   then tries to fill them.
    // `inv` is the predicate to find a candidate for.
    // `invVars` is the list of unprimed variables for that predicate.
    // `cands` is the output.
    //
    // Returns the number of candidates found.
    template <class CONTAINERT>
    int getAllIneqs(const Expr& inv, const ExprVector& invVars, CONTAINERT& cands)
    {
      ExprVector cs, templlhs, optcs;
      for (int i = 0; i < invVars.size(); ++i)
      {
        cs.push_back(mkConst(
          mkTerm(string("__FH_c_")+to_string(i), m_efac), mk<INT_TY>(m_efac)));
        templlhs.push_back(mk<MULT>(invVars[i], cs.back()));
      }
      optcs.push_back(mkConst(mkTerm(string("__FH_c_c"), m_efac), mk<INT_TY>(m_efac)));

      Expr templ = mk<GT>(mknary<PLUS>(templlhs), optcs.back());
      return fillTempl2<LEQ>(inv, templ, cs, optcs, cands);
    }

    // Produces templates of the form: c1*v1 + c2*v2 + ... = cc,
    //   then tries to fill them.
    // `inv` is the predicate to find a candidate for.
    // `invVars` is the list of unprimed variables for that predicate.
    // `cands` is the output.
    //
    // Returns the number of candidates found.
    template <class CONTAINERT>
    int getAllEqs(const Expr& inv, const ExprVector& invVars, CONTAINERT& cands)
    {
      ExprVector cs, templlhs, optcs;
      for (int i = 0; i < invVars.size(); ++i)
      {
        cs.push_back(mkConst(
          mkTerm(string("__FH_c_")+to_string(i), m_efac), mk<INT_TY>(m_efac)));
        templlhs.push_back(mk<MULT>(invVars[i], cs.back()));
      }
      optcs.push_back(mkConst(mkTerm(string("__FH_c_c"), m_efac), mk<INT_TY>(m_efac)));

      Expr templ = mk<EQ>(mknary<PLUS>(templlhs), optcs.back());
      return fillTempl<EQ>(inv, templ, cs, optcs, cands);
    }

    // Assumes `vars.back()` is the constant term.
    // `inv` is the predicate to find a candidate for.
    // `templ` is the template to fill.
    // Note that 'vars' means 'coefficients', not program variables.
    // `optvars` is the set of coefs to optimize for, a subset of `vars`.
    // `cands` is the output.
    //
    // Returns the number of candidates found.
    //
    // This version starts by trying to find any candidate, then adds
    // the additional constraint (coef != 0) for each coef which was zero.
    //
    // It doesn't work very well, producing large candidates which overfit.
    template <typename OptimOp, class CONTAINERT>
    int fillTempl(const Expr& inv, const Expr& templ, const ExprVector& vars,
      const ExprVector& optvars, CONTAINERT& cands)
    {
      ExprVector foundconsts;
      ExprUMap consts;
      Expr unptempl = replaceAll(templ,
        ruleManager.invVarsPrime[inv], ruleManager.invVars[inv]);
      Expr zero = mkTerm<mpz_class>(0, m_efac);

      // Find any candidate.
      consts = bnd.findConsts2<OptimOp>(inv, templ, ExprVector(), vars, optvars);
      if (consts.empty())
        return 0; // Nothing found, don't return

      // Verbose way of only returning candidates that aren't 0 > 0.
      bool addcand = false;
      for (const auto& var_val : consts)
        if (find(optvars.begin(), optvars.end(), var_val.first) == optvars.end()&&
            (!isOpX<MPZ>(var_val.second) ||
           getTerm<mpz_class>(var_val.second) != 0))
        { addcand = true; break; }
      if (addcand)
        cands.insert(cands.end(), replaceAll(unptempl, consts));

      // Not doing all perms because I don't think it'll be worth it.
      ExprVector newconstrs;
      ExprUSet newnotzero, failednotzero;
      int found = 1;
      while (true)
      {
        Expr pickedconst;
        // Look through coefs in previous candidate...
        for (const auto& var_val : consts)
          // Picking one that was zero...
          if (find(optvars.begin(), optvars.end(), var_val.first) == optvars.end() &&
              isOpX<MPZ>(var_val.second) &&
              getTerm<mpz_class>(var_val.second) == 0 &&
              failednotzero.count(var_val.first) == 0 &&
              newnotzero.insert(var_val.first).second)
          {
            // And constrain that it must not be zero.
            pickedconst = var_val.first;
            newconstrs.emplace_back(mk<NEQ>(var_val.first, zero));
            break;
          }
        if (!pickedconst)
          break; // All coefs are !0 or we already tried, so break.
        consts = bnd.findNextConst(newconstrs);
        if (!consts.empty())
        {
          ++found;
          cands.insert(cands.end(), replaceAll(unptempl, consts));
        }
        else
        {
          // We failed to find a candidate, remove new constraint.
          newconstrs.pop_back();
          newnotzero.erase(pickedconst);
          failednotzero.insert(pickedconst);
        }
      }
      return found;
    }

    // Assumes `vars.back()` is the constant term.
    // `inv` is the predicate to find a candidate for.
    // `templ` is the template to fill.
    // Note that 'vars' means 'coefficients', not program variables.
    // `optvars` is the set of coefs to optimize for, a subset of `vars`.
    // `cands` is the output.
    //
    // Returns the number of candidates found.
    //
    // This version starts by looking for all unary (a*v > b) candidates,
    //   then going to higher arities when needed. The hope is that it would
    //   produce more useful candidates than `fillTempl`. It produces smaller
    //   candidates, but because of the way BndExpl::findConsts2 works it
    //   overfits and doesn't produce invariants.
    template <typename OptimOp, class CONTAINERT>
    int fillTempl2(const Expr& inv, const Expr& templ, const ExprVector& vars,
      const ExprVector& optvars, CONTAINERT& cands)
    {
      ExprVector foundconsts;
      ExprUMap consts;
      Expr unptempl = replaceAll(templ,
        ruleManager.invVarsPrime[inv], ruleManager.invVars[inv]);
      int found = 0;
      // Add a candidate to cands if one found
      auto maybeaddcand = [&] ()
      {
        if (!consts.empty())
        {
          cands.insert(cands.end(),
            simplifyArithm(replaceAll(unptempl, consts)));
          ++found;
        }
      };
      Expr zero = mkTerm<mpz_class>(0, m_efac);
      // Find any solution to templ
      consts = bnd.findConsts2<OptimOp>(inv, templ, ExprVector(), vars, optvars);
      maybeaddcand();

      // Find a candidate that has !0 for one coef and 0 for all other coefs.
      // Will be of form a*v > 0 after simplification.

      // The variables for which we haven't found a unary candidate.
      ExprVector unfoundvars;
      for (int i = 0; i < vars.size(); ++i)
      {
        ExprVector zconstr; zconstr.reserve(vars.size());
        for (int j = 0; j < vars.size(); ++j)
          if (j != i)
            zconstr.push_back(mk<EQ>(vars[j], zero));
          else
            zconstr.push_back(mk<NEQ>(vars[j], zero));
        consts = bnd.findNextConst(zconstr);
        if (!consts.empty())
          maybeaddcand();
        else
          unfoundvars.push_back(vars[i]);
      }

      // Find a candidate that has !0 for any coef in 'unfoundvars', with
      //   no other constraints.
      ExprVector zconstr(1);
      for (int i = 0; i < unfoundvars.size(); ++i)
      {
        if (!unfoundvars[i])
          continue;
        zconstr[0] = mk<NEQ>(vars[i], zero);
        // TODO: Try to optimize size of solution?
        consts = bnd.findNextConst(zconstr);
        maybeaddcand();
        for (const auto& var_val : consts)
          if (isOpX<MPZ>(var_val.second) && getTerm<mpz_class>(var_val.second) != 0)
          {
            auto itr = find(unfoundvars.begin(), unfoundvars.end(), var_val.first);
            if (itr != unfoundvars.end())
              // We found a candidate which uses a coef other than the one
              //   we were looking for, mark it as found.
              *itr = NULL;
          }
      }

      return found;
    }

    Operator *vcops[8] = { NULL, new LT(), new GT(), new EQ(),
                           new EQ(), new LEQ(), new GEQ(), new EQ() };
    template <class CONTAINERT>
    int getSimplIneqs(const Expr& inv, const ExprVector& invVars,
        const arma::mat & data, CONTAINERT& cands, const Expr& sels)
    {
      const static int MAX_OPT = 10; // Max iters to try to optimize consts
      if (data.n_rows == 1)
        return 0; // Can't do analysis with only one iter.

      int oldsize = cands.size();
      const int ivs = invVars.size();
      const ExprVector& srcVars = ruleManager.invVars[inv],
                       &dstVars = ruleManager.invVarsPrime[inv];

      auto addCand = [&] (const Expr& e)
      {
        cands.insert(replaceAll(e, dstVars, srcVars));
      };

      // viableCombos[vca(var1, var2)] is bitfield with:
      //   4 if (ever) var1 == var2.
      //   2 if (always) var1 > var2.
      //   1 if (always) var1 < var2.
      //   0 if sometimes both so no inequality.
      vector<char> viableCombos(ivs * ivs, 3);
      vector<double> vmin(ivs), vmax(ivs);
      auto vda = [=] (int v1, int v2) { return v1 * ivs + v2; };
      vector<double> vdmin(ivs * ivs), vdmax(ivs * ivs);

      for (int var1 = 0; var1 < ivs; ++var1)
      {
        double dv1 = data(0, var1 + 1);
        vmin[var1] = dv1; vmax[var1] = vmin[var1];
        for (int var2 = var1 + 1; var2 < ivs; ++var2)
        {
          double diff = dv1 - data(0, var2 + 1);
          vdmin[vda(var1, var2)] = diff;
          vdmax[vda(var1, var2)] = diff;
        }
      }
      for (int row = 1; row < data.n_rows; ++row)
      {
        for (int var1 = 0; var1 < ivs; ++var1)
        {
          double dv1 = data(row, var1 + 1);
          vmin[var1] = min(vmin[var1], dv1);
          vmax[var1] = max(vmax[var1], dv1);
          for (int var2 = var1 + 1; var2 < ivs; ++var2)
          {
            double dv2 = data(row, var2 + 1);
            if (dv1 < dv2)
              viableCombos[vda(var1, var2)] &= 1;
            else if (dv1 > dv2)
              viableCombos[vda(var1, var2)] &= 2;
            else //if (dv1 == dv2)
              viableCombos[vda(var1, var2)] |= 4;

            /*double &vdmn = vdmin[vda(var1, var2)];
            vdmn = min(vdmn, dv1 - dv2);
            double &vdmx = vdmax[vda(var1, var2)];
            vdmx = max(vdmx, dv1 - dv2);*/
          }
        }
      }

      Expr c = bind::intConst(mkTerm<string>("_FH_c", m_efac));
      for (int var1 = 0; var1 < ivs; ++var1)
      {
        if (vmin[var1] == vmax[var1])
        {
          // We already found equalities.
          //addCand(mk<EQ>(invVars[var1], mkTerm<mpz_class>(vmin[var1], m_efac)));
        }
        else
        {
          Expr mincand = mk<LEQ>(invVars[var1], c);
          if (bnd.getOptModel<LT>(inv, mk<AND>(mincand, sels), c,
                mkTerm<mpz_class>(vmin[var1], m_efac), MAX_OPT))
            addCand(mk<GEQ>(mincand->left(), bnd.allModels[inv][c]));
          else
          {
            Expr maxcand = mk<GEQ>(invVars[var1], c);
            if (bnd.getOptModel<GT>(inv, mk<AND>(maxcand, sels), c,
                  mkTerm<mpz_class>(vmax[var1], m_efac), MAX_OPT))
              addCand(mk<LEQ>(maxcand->left(), bnd.allModels[inv][c]));
          }
        }
        for (int var2 = var1 + 1; var2 < ivs; ++var2)
        {
          auto vc12 = viableCombos[vda(var1, var2)];
          if (vc12)
            addCand(mk(*vcops[vc12], invVars[var1], invVars[var2]));
        }
#if 0
        for (int var2 = var1 + 1; var2 < ivs; ++var2)
        {
          Expr diff12 = mk<MINUS>(invVars[var1], invVars[var2]);
          double vdmn = vdmin[vda(var1, var2)], vdmx = vdmax[vda(var1, var2)];
          if (vdmn == vdmx)
          {
            //addCand(mk<EQ>(diff12, mkTerm<mpz_class>(vdmn, m_efac)));
          }
          else
          {
            /*
            Expr mincand = mk<GEQ>(diff12, mkTerm<mpz_class>(vdmn, m_efac));
            Expr maxcand = mk<LEQ>(diff12, mkTerm<mpz_class>(vdmx, m_efac));
            addCand(maxcand);
            addCand(mincand);*/
            Expr mincand = mk<GEQ>(diff12, c);
            if (bnd.getOptModel<LT>(inv, mk<AND>(mincand, sels), c,
                  mkTerm<mpz_class>(vdmn, m_efac), MAX_OPT))
              addCand(mk<GEQ>(mincand->left(), bnd.allModels[inv][c]));
            else
            {
              Expr maxcand = mk<LEQ>(diff12, c);
              if (bnd.getOptModel<GT>(inv, mk<AND>(maxcand, sels), c,
                    mkTerm<mpz_class>(vdmx, m_efac), MAX_OPT))
                addCand(mk<LEQ>(maxcand->left(), bnd.allModels[inv][c]));
            }
          }
        }
#endif
      }

      return cands.size() - oldsize;
    }

  public:

    DataLearner(CHCs& r, EZ3 &z3, int to, bool debug) :
      ruleManager(r), bnd(ruleManager, to, debug), m_efac(r.m_efac), curPolyDegree(1) {}

    void
    setLogLevel(unsigned int l)
    {
      LOG_LEVEL = l;
    }

    map <Expr, vector< vector<double> > > exprToModelsUnp, exprToModelsP;
    map <Expr, ExprVector> invVarsUnp, invVarsP;

    bool computeData(map<Expr, ExprVector>& arrRanges, map<Expr, ExprSet>& constr)
    {
      invVarsUnp.clear();
      invVarsP.clear();
      return bnd.unrollAndExecuteMultiple(invVarsUnp, invVarsP, arrRanges, constr);
    }

    void computeDataSplit(Expr srcRel, Expr splitter, Expr invs, bool fwd, ExprSet& constr)
    {
      exprToModelsUnp.clear();
      exprToModelsP.clear();
      invVarsUnp.clear();
      invVarsP.clear();
      bnd.unrollAndExecuteSplitter(srcRel, invVarsP[srcRel], exprToModelsP[srcRel], splitter, invs, fwd, constr);
    }

    ExprSet& getConcrInvs(Expr rel) { return bnd.concrInvs[rel]; }

    // Implementation of "A Data Driven Approach for Algebraic Loop Invariants", Sharma et al.
    // return number of candidate polynomials added (< 0 in case of error)
    //
    // `sels` is {variable -> range for that variable}.
    //    Note that ranges for arrays are expected to use the array as the
    //    thing to range (e.g. 0 <= a < a_len) even though it won't typecheck.
    template <class CONTAINERT> void
    computePolynomials(Expr inv, CONTAINERT & cands, Expr sels, bool dounp = false)
    {
      CONTAINERT tmp;
      const int maxIsUnp = (invVarsUnp != invVarsP && dounp) ? 2 : 1;
      for (int isUnp = 0; isUnp < maxIsUnp; ++isUnp)
      {
        auto& exprToModels = isUnp ? exprToModelsUnp[inv] : exprToModelsP[inv];
        auto& invVars = isUnp ? invVarsUnp[inv] : invVarsP[inv];

        exprToModels.clear();
        // Extra call to `getMat` to make sure we have the best matrix for
        //   the current `sels`.
        bnd.getMat(inv, invVars, exprToModels, sels);
        while (!exprToModels.empty())
        {
          arma::mat dataMatrix;
          for (auto model : exprToModels) {
            arma::rowvec row = arma::conv_to<arma::rowvec>::from(model);
            row.insert_cols(0, arma::rowvec(1, arma::fill::ones));
            dataMatrix.insert_rows(dataMatrix.n_rows, row);
          }

          map<unsigned int, Expr> monomialToExpr;
          if (0 == initInvVars(inv, invVars, monomialToExpr)) return;
          initLargeCoeffToExpr(dataMatrix);
          getPolynomialsFromData(dataMatrix, tmp, inv, invVars, monomialToExpr);
          getSimplIneqs(inv, invVars, dataMatrix, tmp, sels);
          if (tmp.size() > 0) break;
          else exprToModels.pop_back();
        }
        cands.insert(tmp.begin(), tmp.end());
      }
    }
  };
}


#endif
