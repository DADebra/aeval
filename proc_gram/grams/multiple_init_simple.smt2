
#include "shared.smt2"

(declare-fun inv1 () Bool)
(declare-fun inv2 () Bool)

(assert (= inv1 (either_inorder
  lemma
  (forall ((i Int)) (=>
    (and (>= i 0) (< i _FH_inv1_1))
    (>= (select _FH_inv1_0 i) 1)
  ))
)))

(assert (= inv2 (either_inorder
  lemma
    (forall ((i Int)) (=>
      (and (>= i 0) (< i _FH_inv2_1))
      (>= (select _FH_inv2_0 i) 1)
    ))
)))
