
#include "shared.smt2"

(assert (= PROP
  (=>
    (and (<= 0 _FH_inv_4) (distinct _FH_inv_2 _FH_inv_4))
    (exists ((exvar Int)) DEF_CANDS)
  )
))
