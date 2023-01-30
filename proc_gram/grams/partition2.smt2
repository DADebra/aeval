
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (<= 0 _FH_inv_4) (<= 0 favar) (< favar _FH_inv_6))
    (exists ((exvar Int)) (= (select _FH_inv_2 favar) (select _FH_inv_0 exvar)))
  ))
)))
