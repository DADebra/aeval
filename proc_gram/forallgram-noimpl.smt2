(declare-fun ANY_INV () Bool)
(declare-fun fla () Bool)

(assert (= fla (either_inorder
  (forall ((unused Int)) DEF_CANDS)
  ;(forall ((unused Int)) (=> DEF_CANDS DEF_CANDS))
)))

(assert (= ANY_INV (either_inorder fla)))
