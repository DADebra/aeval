
#include "shared.smt2"

(assert (= PROP
  (forall ((favar Int)) (=>
    (= (select _FH_inv_0 favar) (select _FH_inv_1 favar))
    (exists ((exvar Int)) DEF_CANDS)
  ))
))
