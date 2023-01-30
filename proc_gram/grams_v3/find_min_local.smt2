
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (<= 1 _FH_inv_4) (<= 0 favar) (< favar _FH_inv_5))
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_4) (= (select _FH_inv_1 favar) (select _FH_inv_0 exvar))))
  ))
)))
