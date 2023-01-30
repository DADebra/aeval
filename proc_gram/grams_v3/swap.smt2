
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) (exists ((exvar Int)) (= (select _FH_inv_0 exvar) (select _FH_inv_1 favar))))
)))
