
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (=>
    (and (<= 0 _FH_inv_4) (= _FH_inv_3 1))
    (exists ((exvar Int)) (distinct (select _FH_inv_0 exvar) (select _FH_inv_1 exvar)))
  )
)))
