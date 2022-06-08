#ifndef TRAVERSAL__HPP__
#define TRAVERSAL__HPP__

#ifndef GRAMINCLUDED
#error __FILE__ " cannot be included directly. Use '#include \"gram/AllHeaders.hpp\""
#endif

namespace ufo
{

typedef unordered_map<Expr,unordered_set<Expr>> UniqVarMap;

class Traversal
{
  public:

  virtual ~Traversal() {}

  // Returns true if all candidates in the grammar have been enumerated.
  virtual bool IsDone() = 0;

  // Returns true if all candidates at the current recursion depth.
  // have been enumerated.
  virtual bool IsDepthDone() = 0;

  // Get the recursion depth currently used as maximum.
  virtual int GetCurrDepth() = 0;

  // Returns the candidate at the current traversal position.
  virtual ParseTree GetCurrCand() = 0;

  // Returns the set of unique variables that appear in the current candidate.
  // Key: Variable (added with 'Grammar::addUniqueVar')
  // Value: Set of unique FAPPS that Key expands to.
  virtual const UniqVarMap& GetCurrUniqueVars() = 0;

  // Increments the position of this traversal, returning the next candidate
  // (i.e. the candidate at the new position).
  virtual ParseTree Increment() = 0;

  // Restart the traversal, so that it starts from the first candidate again.
  virtual void Restart() = 0;
};

}
#endif
