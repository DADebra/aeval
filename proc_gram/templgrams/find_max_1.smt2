
#include "shared.smt2"

(assert (= PROP
  (=>
    (and
      (<= 0 _FH_inv_3)
      (exists ((exvar Int)) (and (<= 0 exvar) (< exvar _FH_inv_1) (<= 0 (select _FH_inv_0 exvar))))
    )
    (exists ((exvar Int)) DEF_CANDS)
  )
))
