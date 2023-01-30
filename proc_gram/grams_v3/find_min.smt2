
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (=>
    (not (exists ((exvar Int)) (and (<= 0 _FH_inv_3) (<= 0 exvar) (< exvar _FH_inv_1) (<= (select _FH_inv_0 exvar) 0))
    ))
    lemma
  )
  (=>
    (exists ((exvar Int)) (and (<= 0 _FH_inv_3) (<= 0 exvar) (< exvar _FH_inv_1) (<= (select _FH_inv_0 exvar) 0))
    )
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_1) (= _FH_inv_2 (select _FH_inv_0 exvar))))
  )
)))
