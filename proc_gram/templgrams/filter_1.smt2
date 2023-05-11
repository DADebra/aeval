
#include "shared.smt2"

(assert (= PROP
    (forall ((j Int)) (=>
      (and (>= (select _FH_inv_0 j) _FH_inv_3) (<= (select _FH_inv_0 j) _FH_inv_4))
      (exists ((k Int)) DEF_CANDS)
    ))
))
