
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (or
    (>= _FH_inv_2 _FH_inv_4)
    (exists ((exvar Int)) (and
      (>= exvar 0) (< exvar _FH_inv_1)
      (= (select _FH_inv_0 exvar) _FH_inv_3)
    ))
  )
)))
