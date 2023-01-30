
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (=> lemma lemma)
  ;(=>
  ;  (not (exists ((exvar Int)) (and (<= 0 _FH_inv_3) (<= 0 exvar) (< exvar _FH_inv_1) (<= (select _FH_inv_0 exvar) 0))
  ;  ))
  ;  lemma
  ;)
  (=> (= _FH_inv_2 (select _FH_inv_0 _FH_inv_1)) (>= (select _FH_inv_0 _FH_inv_1) 0))
  (=>
    (exists ((exvar Int)) (and (<= 0 _FH_inv_3) (<= 0 exvar) (< exvar _FH_inv_1) (<= (select _FH_inv_0 exvar) 0))
    )
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_1) (= _FH_inv_2 (select _FH_inv_0 exvar))))
  )
)))
