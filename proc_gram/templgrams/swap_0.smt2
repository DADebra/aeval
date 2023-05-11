
#include "shared.smt2"

(assert (= PROP
  (forall ((favar Int)) (exists ((exvar Int)) DEF_CANDS))
))
