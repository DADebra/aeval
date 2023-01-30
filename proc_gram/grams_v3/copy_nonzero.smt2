
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (<= 0 favar) (<= 0 _FH_inv_4) (< favar _FH_inv_3))
    (exists ((exvar Int)) (= (select _FH_inv_0 favar) (select _FH_inv_1 exvar)))
  ))
)))
