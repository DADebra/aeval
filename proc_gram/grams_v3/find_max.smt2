
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (=>
    (not (and
      (<= 0 _FH_inv_3)
      (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_1) (<= 0 (select _FH_inv_0 exvar))))
    ))
    lemma ; (= _FH_inv_2 0)
  )
  (=>
    (and
      (<= 0 _FH_inv_3)
      (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_1) (<= 0 (select _FH_inv_0 exvar))))
    )
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_3) (= _FH_inv_2 (select _FH_inv_0 exvar))))
  )
)))
