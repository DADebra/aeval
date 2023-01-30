
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_2))
    (exists ((exvar Int)) (= (select _FH_inv_0 exvar) (select _FH_inv_1 favar)))
  ))
)))
