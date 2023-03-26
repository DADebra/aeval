(declare-fun ANY_INV () Bool)
(declare-fun fla () Bool)

(assert (= fla (either
  (forall ((favar Int)) DEF_CANDS)
  (forall ((favar Int)) (=> DEF_CANDS DEF_CANDS))
  (forall ((favar Int)) (=> (not DEF_CANDS) DEF_CANDS))
)))
(assert (= ANY_INV (either_inorder fla fla)))
