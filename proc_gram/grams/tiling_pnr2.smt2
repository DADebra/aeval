
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (=> lemma lemma)
  (forall ((i1 Int)) (or (or (> 0 i1) (and (or (< i1 0) (> (+ i1 (* -2 _FH_inv_1)) -4)) (or (< i1 1) (> (+ i1 (* -2 _FH_inv_1)) -3)))) (distinct (select _FH_inv_0 i1) 0) (or (>= i1 (* 2 _FH_inv_2)) (and (or (< i1 0) (> (+ i1 (* -2 _FH_inv_1)) -4)) (or (< i1 1) (> (+ i1 (* -2 _FH_inv_1)) -3))))))
)))
