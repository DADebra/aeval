
#include "shared.smt2"

(assert (= PROP (either
  (=>
    (exists ((exvar Int)) (and (<= 0 _FH_inv_3) (<= 0 exvar) (< exvar _FH_inv_1) (<= (select _FH_inv_0 exvar) 0))
    )
    (exists ((exvar Int)) DEF_CANDS)
  )
  (=>
    (exists ((exvar Int)) (and (<= 0 _FH_inv_3) (<= 0 exvar) (< exvar _FH_inv_1) (<= (select _FH_inv_0 exvar) 0))
    )
    (exists ((exvar Int)) (=> DEF_CANDS DEF_CANDS))
  )
)))
