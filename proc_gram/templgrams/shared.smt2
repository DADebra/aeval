
(declare-fun ANY_INV () Bool)
(declare-fun PROP () Bool)

(assert (= ANY_INV (either_inorder
  PROP
#include "alterngram_include.smt2"
  PROP
)))
