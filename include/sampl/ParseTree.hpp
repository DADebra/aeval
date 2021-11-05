#ifndef PARSETREE__HPP__
#define PARSETREE__HPP__

namespace ufo
{
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

  ParseTreeNode(Expr _data, const vector<ParseTree>& _children) :
    data(_data), children(_children), parent(NULL) {}

  ParseTreeNode(Expr _data, vector<ParseTree>&& _children) :
    data(_data), children(_children), parent(NULL) {}

  ParseTreeNode(Expr _data) : data(_data), parent(NULL)
  {
    assert(("Must pass a vector of children for non-FAPP Expr with arity != 0",
      data->arity() == 0 || isOpX<FAPP>(data)));
  }

  friend ParseTree;
};

class ParseTree
{
  std::shared_ptr<ParseTreeNode> ptr;

  public:
  ParseTree(Expr _data, const vector<ParseTree>& _children) :
    ptr(new ParseTreeNode(_data, _children)) {}

  ParseTree(Expr _data, vector<ParseTree>&& _children) :
    ptr(new ParseTreeNode(_data, _children)) {}
  ParseTree(const ParseTree& pt) : ptr(pt.ptr) {}
  ParseTree(ParseTreeNode* ptptr) : ptr(ptptr) {}
  ParseTree(const std::shared_ptr<ParseTreeNode>& cp) : ptr(cp) {}
  ParseTree(Expr _data) : ptr(new ParseTreeNode(_data)) {}
  ParseTree() : ptr(NULL) {}

  const Expr& data() const
  {
    return ptr->data;
  }

  const vector<ParseTree>& children() const
  {
    return ptr->children;
  }

  ParseTree parent() const
  {
    return ptr->parent;
  }

  operator bool() const
  {
    return bool(ptr);
  }

  bool operator !() const
  {
    return !bool(ptr);
  }

  bool operator ==(const ParseTree& other) const
  {
    return ptr.get() == other.ptr.get();
  }

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

  /*operator Expr()
  {
    return ptr ? ptr->data : NULL;
  }

  operator cpp_int()
  {
    return lexical_cast<cpp_int>(ptr->data);
  }*/
  friend std::hash<ufo::ParseTree>;
};
}

namespace std
{
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
