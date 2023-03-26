(declare-fun ANY_INV () Bool)
(declare-fun fla () Bool)

(assert (= fla (either_inorder
  (forall ((favar Int)) DEF_CANDS)
  ;(forall ((favar Int)) (=> DEF_CANDS DEF_CANDS))
)))

(assert (= ANY_INV (either_inorder fla fla)))
