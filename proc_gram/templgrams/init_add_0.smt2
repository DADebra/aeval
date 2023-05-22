
#include "shared.smt2"

(assert (= PROP (either
  (forall ((favar Int))
    (exists ((exvar Int)) DEF_CANDS)
  )
)))
