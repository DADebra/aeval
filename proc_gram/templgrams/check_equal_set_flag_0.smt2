
#include "shared.smt2"

(assert (= PROP
  (=>
    (and (<= 0 _FH_inv_4) (= _FH_inv_3 1))
    (exists ((exvar Int)) DEF_CANDS)
  )
))
