
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
    lemma
    (=> lemma lemma)
    DEF_CANDS
)))
