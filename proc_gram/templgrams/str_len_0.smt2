
#include "shared.smt2"

(assert (= PROP
  (=>
    (exists ((exvar Int)) (and (<= 0 exvar) (= (select _FH_inv_0 exvar) 0)))
    (forall ((favar Int)) DEF_CANDS)
  )
))
