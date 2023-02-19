#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  ;lemma
  ;(=> lemma lemma)
  (forall ((i1 Int)) (=>
    (and (>= i1 0) (< i1 _FH_inv_2))
    (= (select _FH_inv_1 i1) (select _FH_inv_0 (- _FH_inv_3 i1 1)))
  ))
)))
