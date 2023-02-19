(declare-fun ANY_INV () Bool)

(assert (= ANY_INV (either_inorder
  DEF_CANDS
  (=> DEF_CANDS DEF_CANDS)
)))
