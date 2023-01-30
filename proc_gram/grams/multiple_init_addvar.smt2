
#include "shared.smt2"

(declare-fun inv1 () Bool)
(declare-fun inv2 () Bool)

(assert (= inv1 (either_inorder
  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv1_1))
    (>= (select _FH_inv1_0 favar) (* 2 favar))
  ))
)))

(assert (= inv2 (either_inorder
  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv2_1))
    (>= (select _FH_inv2_0 favar) (* 2 favar))
  ))
)))
