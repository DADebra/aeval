
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (=>
    (and (<= 0 _FH_inv_3) (< _FH_inv_1 _FH_inv_3))
    (exists ((exvar Int)) (= (select _FH_inv_0 exvar) _FH_inv_2))
  )
)))
