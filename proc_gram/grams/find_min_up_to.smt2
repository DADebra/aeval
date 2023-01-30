
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (forall ((favar Int)) (=>
    (and (<= 0 _FH_inv_4) (<= 0 favar) (< favar _FH_inv_2))
    (exists ((exvar Int)) (and (<= 0 exvar) (and (<= exvar favar) (<= exvar _FH_inv_3)) (= (select _FH_inv_1 favar) (select _FH_inv_0 exvar))))
  ))
)))
