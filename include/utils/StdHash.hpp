#ifndef __STDHASH_HPP__
#define __STDHASH_HPP__

#include <vector>

namespace std
{

template <typename T, typename Alloc>
struct hash<vector<T,Alloc>>
{
  size_t operator()(const vector<T,Alloc>& vec) const
  {
    return boost::hash_range(vec.begin(), vec.end());
  }
};

template <typename T, typename Comp, typename Alloc>
struct hash<set<T,Comp,Alloc>>
{
  size_t operator()(const set<T,Comp,Alloc>& s) const
  {
    return boost::hash_range(s.begin(), s.end());
  }
};

}

#endif
