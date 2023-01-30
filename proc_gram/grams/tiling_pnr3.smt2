
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  ;lemma
  ;(=> lemma lemma)
  ;(forall ((i1 Int)) (or (or (> 0 i1) (and (or (< i1 0) (> (+ i1 (* -3 _FH_inv_1)) -6)) (or (< i1 1) (> (+ i1 (* -3 _FH_inv_1)) -5)) (or (< i1 2) (> (+ i1 (* -3 _FH_inv_1)) -4)))) (or (>= i1 (* 3 _FH_inv_2)) (and (or (< i1 0) (> (+ i1 (* -3 _FH_inv_1)) -6)) (or (< i1 1) (> (+ i1 (* -3 _FH_inv_1)) -5)) (or (< i1 2) (> (+ i1 (* -3 _FH_inv_1)) -4)))) (distinct (select _FH_inv_0 i1) _FH_inv_3)))
    (= (+ _FH_inv_4 (- 10)) 0)
    (= _FH_inv_3 0)
    (forall ((i1 Int)) (or (< i1 0) (and (or (< i1 0) (> (+ i1 (* (- 3) _FH_inv_1)) (- 6))) (or (< i1 1) (> (+ i1 (* (- 3) _FH_inv_1)) (- 5))) (or (< i1 2) (> (+ i1 (* (- 3) _FH_inv_1)) (- 4)))) (>= (+ i1 (* (- 3) _FH_inv_2)) 0) (< (+ _FH_inv_3 (- (select _FH_inv_0 i1))) 0) (> (+ _FH_inv_3 (- (select _FH_inv_0 i1))) 0)))
)))
