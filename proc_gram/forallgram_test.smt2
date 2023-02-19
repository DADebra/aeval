(declare-fun ANY_INV () Bool)
(declare-fun fla () Bool)

(assert (= fla (either
  (forall ((unused Int)) DEF_CANDS)
  (forall ((unused Int)) (=> DEF_CANDS DEF_CANDS))
  (forall ((unused Int)) (=> (not DEF_CANDS) DEF_CANDS))
)))
(assert (= ANY_INV (either_inorder fla fla)))
