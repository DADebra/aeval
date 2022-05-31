#ifndef PARSETREE__HPP__
#define PARSETREE__HPP__

#include <iostream>
#include <vector>
#include <memory>
#include <functional>

namespace ufo
{
using namespace std;
class ParseTree;
class ParseTreeNode
{
  private:
  // Will be FAPP or terminal (MPZ, _FH_inv_0, etc.)
  Expr data;
  // Shape will match data; if data has 3 args, children will have 3 args,
  //   even if some of the arguments are e.g. terminals.
  // children[0] == expansion of data.arg(1), etc.
  vector<ParseTree> children;
  std::shared_ptr<ParseTreeNode> parent;
  bool isNt;

  ParseTreeNode(Expr _data, const vector<ParseTree>& _children, bool _isnt) :
    data(_data), children(_children), parent(NULL), isNt(_isnt) {}

  ParseTreeNode(Expr _data, vector<ParseTree>&& _children, bool _isnt) :
    data(_data), children(_children), parent(NULL), isNt(_isnt) {}

  ParseTreeNode(Expr _data) : data(_data), parent(NULL), isNt(false) {}

  friend ParseTree;
};

class ParseTree
{
  std::shared_ptr<ParseTreeNode> ptr;

  public:
  ParseTree(Expr _data, const vector<ParseTree>& _children, bool _isnt) :
    ptr(new ParseTreeNode(_data, _children, _isnt)) {}

  ParseTree(Expr _data, vector<ParseTree>&& _children, bool _isnt) :
    ptr(new ParseTreeNode(_data, _children, _isnt)) {}
  ParseTree(const ParseTree& pt) : ptr(pt.ptr) {}
  ParseTree(ParseTreeNode* ptptr) : ptr(ptptr) {}
  ParseTree(const std::shared_ptr<ParseTreeNode>& cp) : ptr(cp) {}
  ParseTree(Expr _data) : ptr(new ParseTreeNode(_data)) {}
  ParseTree() : ptr(NULL) {}

  const Expr& data() const { return ptr->data; }

  vector<ParseTree>& children() { return ptr->children; }

  const vector<ParseTree>& children() const { return ptr->children; }

  ParseTree parent() const { return ptr->parent; }

  bool isNt() const { return ptr->isNt; }

  operator bool() const { return bool(ptr); }

  bool operator !() const { return !bool(ptr); }

  bool operator ==(const ParseTree& other) const
  {
    if (!ptr || !other.ptr)
      return false;
    if (ptr->data != other.ptr->data)
      return false;
    if (ptr->children.size() != other.ptr->children.size())
      return false;
    for (int i = 0; i < ptr->children.size(); ++i)
      if (ptr->children[i] != other.ptr->children[i])
        return false;
    return true;
  }
  bool operator !=(const ParseTree& other) const
  {
    return !(*this == other);
  }
  bool operator <(const ParseTree&) = delete;
  bool operator >(const ParseTree&) = delete;
  bool operator <=(const ParseTree&) = delete;
  bool operator >=(const ParseTree&) = delete;

  void fixchildren()
  {
    if (!ptr)
      return;
    for (auto &child : ptr->children)
    {
      child.ptr->parent = this->ptr;
      child.fixchildren();
    }
  }

  Expr toExpr() const
  {
    if (children().size() == 0)
      return data();
    else if (children().size() == 1)
    {
      if (!isNt())
        return mk(data()->op(), children()[0].toExpr());
      return children()[0].toExpr();
    }
    else
    {
      ExprVector eargs;
      for (ParseTree subpt : children())
      {
        eargs.push_back(subpt.toExpr());
      }
      return mknary(data()->op(), eargs.begin(), eargs.end());
    }
  }

  operator Expr() const { return toExpr(); }

  void foreachPt(const function<void(const Expr&, const ParseTree&)>& func) const
  {
    if (children().size() == 0)
      return;
    else if (isNt())
    {
      func(data(), children()[0]);

      return children()[0].foreachPt(func);
    }
    else
    {
      for (auto &subpt : children())
        subpt.foreachPt(func);
    }
  }

  // First arg to func: Non-terminal Second arg: Production
  // Follows simple NT renaming (A -> B).
  void foreachExpansion(const function<void(const Expr&, const ParseTree&)>& func) const
  {
    return foreachPt([&] (const Expr& nt, const ParseTree& prod)
    {
      const ParseTree* realexpan = &prod;
      while (realexpan->isNt())
        realexpan = &realexpan->children()[0];

      return func(nt, *realexpan);
    });
  }

  void print(ostream& os, int depth = 0) const
  {
    for (int i = 0; i < depth; ++i) os << "  ";
    os << data() << "\n";
    for (auto &p : children())
      p.print(os, depth + 1);
  }

  /*operator cpp_int()
  {
    return lexical_cast<cpp_int>(ptr->data);
  }*/
  friend std::hash<ufo::ParseTree>;
};
}

namespace std
{
ostream& operator<<(ostream& os, const ufo::ParseTree& pt)
{
  pt.print(os);
  return os;
}

template <>
struct hash<ufo::ParseTree>
{
  size_t operator()(const ufo::ParseTree& pt) const
  {
    return std::hash<std::shared_ptr<ufo::ParseTreeNode>>()(pt.ptr);
  }
};
}
#endif