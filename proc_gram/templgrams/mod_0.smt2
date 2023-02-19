
#include "shared.smt2"

(assert (= PROP
  (forall ((favar Int)) (=>
    (= (select _FH_inv_0 favar) 0)
    (exists ((exvar Int)) DEF_CANDS)
  ))
))
