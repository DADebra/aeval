
#include "shared.smt2"

(assert (= PROP
  (forall ((favar Int))
    (<= 0 (select _FH_inv_0 favar))
    (exists ((exvar Int)) DEF_CANDS)
  )
))
