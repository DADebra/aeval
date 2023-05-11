
#include "shared.smt2"

(assert (= PROP
    (forall ((j Int) (k Int)) (=> (and (< j k) (distinct (select _FH_inv_0 j) (select _FH_inv_0 k))) DEF_CANDS))
))
