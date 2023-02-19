
#include "shared.smt2"

(assert (= PROP
  (=>
    (< 0 _FH_inv_3)
    (exists ((exvar Int)) PROP)
  )
))
