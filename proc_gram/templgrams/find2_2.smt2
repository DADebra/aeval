
#include "shared.smt2"

(assert (= PROP
  (=>
    (and (<= 0 _FH_inv_3) (distinct _FH_inv_1 _FH_inv_3))
    (exists ((exvar Int)) DEF_CANDS)
  )
))
