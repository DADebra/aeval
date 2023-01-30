
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (forall ((favar Int)) (exists ((exvar Int)) (=>
    (and (<= 0 favar) (<= 0 _FH_inv_4) (< favar _FH_inv_3))
    (and (>= exvar 0) (< exvar _FH_inv_2)
      (= (select _FH_inv_0 favar) (select _FH_inv_1 exvar))
    ))
  ))
)))
