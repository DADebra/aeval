(declare-fun ANY_INV () Bool)

(assert (= ANY_INV (either_inorder
  (forall ((unused Int)) DEF_CANDS)
  (forall ((unused Int)) (=> DEF_CANDS DEF_CANDS))
)))
