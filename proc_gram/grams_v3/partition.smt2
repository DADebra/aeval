
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (<= 0 _FH_inv_4) (<= 0 favar) (< favar _FH_inv_5))
    (exists ((exvar Int)) (= (select _FH_inv_1 favar) (select _FH_inv_0 exvar)))
  ))
)))
