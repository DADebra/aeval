
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar (* 2 _FH_inv_1)) (= (select _FH_inv_0 favar) 0))
    (exists ((exvar Int)) (and (> exvar favar) (= (select _FH_inv_0 exvar) 1)))
  ))
)))
