(declare-fun ANY_INV () Bool)

(assert (= ANY_INV (either
  ;DEF_CANDS
  ;(=> DEF_CANDS DEF_CANDS)
  ;(forall ((unused Int)) DEF_CANDS)
  (forall ((unused Int)) (exists ((unused2 Int)) DEF_CANDS))
  ;(forall ((unused Int)) (=> DEF_CANDS DEF_CANDS))
  (forall ((unused Int)) (exists ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> (not DEF_CANDS) DEF_CANDS)))
  ;(exists ((unused Int)) (forall ((unused2 Int)) DEF_CANDS))
  ;(exists ((unused Int)) (forall ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(forall ((unused Int)) (exists ((unused2 Int)) DEF_CANDS))
  ;(forall ((unused Int)) (exists ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
  ;(exists ((unused Int)) (forall ((unused2 Int)) DEF_CANDS))
  ;(exists ((unused Int)) (forall ((unused2 Int)) (=> DEF_CANDS DEF_CANDS)))
)))
