
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (forall ((favar Int)) (=>
    (and (<= 1 _FH_inv_4) (<= 0 favar) (< favar _FH_inv_2) (forall ((favar2 Int)) (=> (and (<= 0 favar2) (< favar2 favar)) (< (select _FH_inv_0 favar2) (select _FH_inv_0 favar)))))
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_5) (= (select _FH_inv_0 favar) (select _FH_inv_1 exvar))))
  ))
)))
