
#include "shared.smt2"

(assert (= PROP
    (forall ((j Int) (k Int)) (=> (and (< j k) (distinct (select A j) (select A k))) DEF_CANDS))
))
