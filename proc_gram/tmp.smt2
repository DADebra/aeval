(declare-fun ANY_INV () Bool)
(declare-fun PROP () Bool)
(assert (= ANY_INV (either_inorder
  PROP
  (forall ((unused Int)) (exists ((unused2 Int)) DEF_CANDS))
  ;(forall ((unused Int)) (=> DEF_CANDS DEF_CANDS))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> (not DEF_CANDS) DEF_CANDS)))
  ;(exists ((unused Int)) (forall ((unused2 Int)) DEF_CANDS))
  ;(exists ((unused Int)) (forall ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(forall ((unused Int)) (exists ((unused2 Int)) DEF_CANDS))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(exists ((unused Int)) (forall ((unused2 Int)) DEF_CANDS))
  ;(exists ((unused Int)) (forall ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  PROP
)))
(assert (= PROP
  (=>
    (and (<= 0 _FH_inv_4) (distinct _FH_inv_2 _FH_inv_4))
    (exists ((exvar Int)) DEF_CANDS)
  )
))
