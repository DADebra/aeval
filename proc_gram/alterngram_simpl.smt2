(declare-fun ANY_INV () Bool)

(assert (= ANY_INV (either_inorder
  ;DEF_CANDS
  ;(=> DEF_CANDS DEF_CANDS)
  (forall ((unused Int)) (exists ((unused2 Int)) DEF_CANDS))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> (not DEF_CANDS) DEF_CANDS)))
)))
