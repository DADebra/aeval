
#include "shared.smt2"

(assert (= PROP
  (or
    (>= _FH_inv_2 _FH_inv_4)
    (exists ((exvar Int))
      DEF_CANDS
    )
  )
))
