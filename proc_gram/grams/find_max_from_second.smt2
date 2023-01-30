
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (=>
    (< 0 _FH_inv_3)
    (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_3) (= (select _FH_inv_0 exvar) _FH_inv_2)))
  )
)))
