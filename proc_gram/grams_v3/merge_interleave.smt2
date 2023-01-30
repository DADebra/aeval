
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (<= 0 favar) (< favar (* 2 _FH_inv_3)))
    (exists ((exvar Int)) (or
      (= (select _FH_inv_2 favar) (select _FH_inv_0 exvar))
      (= (select _FH_inv_2 favar) (select _FH_inv_1 exvar))
    ))
  ))
)))
