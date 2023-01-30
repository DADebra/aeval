
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (=>
    (exists ((exvar Int)) (and (<= 0 exvar) (= (select _FH_inv_0 exvar) 0)))
    (forall ((favar Int)) (=>
      (and (<= 0 favar) (< favar _FH_inv_1))
      (distinct (select _FH_inv_0 favar) 0)
    ))
  )
)))
