
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  ;lemma
  ;(forall ((favar Int)) (>= (select _FH_inv_0 favar) 0))
  (forall ((favar Int)) (=>
    (and (>= favar _FH_inv_1) (< favar _FH_inv_2))
    (= (select _FH_inv_0 favar) 0)
  ))
  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_1))
    (= (select _FH_inv_0 favar) favar)
  ))
;  (forall ((favar Int)) (=>
;    (and (>= favar 0) (< favar _FH_inv_1))
;    (exists ((exvar Int)) (and (< exvar favar) (< (select _FH_inv_0 exvar) (select _FH_inv_0 favar))))
;  ))
)))
