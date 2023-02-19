
#include "shared.smt2"

(assert (= PROP
  (forall ((favar Int)) (=>
    (forall ((favar2 Int)) (=> (and (<= 0 favar2) (< favar2 favar)) (< (select _FH_inv_0 favar) (select _FH_inv_0 favar2))))
    (exists ((exvar Int)) DEF_CANDS)
  ))
))
