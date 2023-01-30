
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (<= 0 _FH_inv_4) (<= 0 favar) (< favar _FH_inv_3) (= (select _FH_inv_0 favar) (select _FH_inv_1 favar)))
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_5) (= (select _FH_inv_2 exvar) favar)))
  ))
)))
