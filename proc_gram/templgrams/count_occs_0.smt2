
#include "shared.smt2"

(assert (= PROP
  (forall ((j Int)) (=>
    (forall ((k Int)) (=> (and (>= k 0) (< k _FH_inv_4)) (distinct j (select _FH_inv_0 k))))
    DEF_CANDS
  ))
))
