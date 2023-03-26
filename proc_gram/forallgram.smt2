(declare-fun ANY_INV () Bool)

(assert (= ANY_INV (either_inorder
  (forall ((favar Int)) DEF_CANDS)
  (forall ((favar Int)) DEF_CANDS)
  (forall ((favar Int)) (=> DEF_CANDS DEF_CANDS))
  (forall ((favar Int)) (=> DEF_CANDS DEF_CANDS))
)))
