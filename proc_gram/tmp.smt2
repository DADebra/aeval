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
  (forall ((favar Int)) (=>
    (= (select _FH_inv_0 favar) (select _FH_inv_1 favar))
    (exists ((exvar Int)) DEF_CANDS)
  ))
))
