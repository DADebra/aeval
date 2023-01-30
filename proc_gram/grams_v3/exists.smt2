
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  (or (not (< _FH_inv_1 _FH_inv_3)) (= (select _FH_inv_0 _FH_inv_1) _FH_inv_2))
)))
