
(declare-fun ANY_INV () Bool)
(declare-fun iterm () Int)
(declare-fun lemma () Bool)
(declare-fun exvar () Int)
(declare-fun favar () Int)
(declare-fun exiterm () Int)
(declare-fun exiterm_nofa () Int)
(declare-fun sel () Int)
(declare-fun sel_nofa () Int)
(declare-fun inexpost () Bool)
(declare-fun inexpost_nofa () Bool)

(assert (= iterm (either INT_CONSTS INT_VARS)))

(assert (= lemma (either
  (< iterm iterm)
  (= iterm iterm)
  (not lemma)
)))
(assert (constraint (distinct_under "lemma" iterm)))

(assert (= exiterm (either favar exvar INT_CONSTS INT_VARS)))
(assert (= exiterm_nofa (either exvar INT_CONSTS INT_VARS)))
(assert (= sel (either
  (select $ARRAY_INT_INT$_VARS exvar)
  (select $ARRAY_INT_INT$_VARS favar)
)))
(assert (= sel_nofa
  (select $ARRAY_INT_INT$_VARS exvar)
))

(assert (= inexpost (either
  (= sel exiterm)
  (= sel sel)
  (< sel exiterm)
  (< sel sel)
  (> sel exiterm)
  (not inexpost)
)))
(assert (constraint (distinct_under "inexpost" sel exiterm)))
(assert (constraint (distinct_under "inexpost" sel)))

(assert (= inexpost_nofa (either
  (= sel_nofa exiterm_nofa)
  (= sel_nofa sel_nofa)
  (< sel_nofa exiterm_nofa)
  (< sel_nofa sel_nofa)
  (> sel_nofa exiterm_nofa)
  (not inexpost_nofa)
)))
(assert (constraint (distinct_under "inexpost_nofa" sel_nofa exiterm_nofa)))
(assert (constraint (distinct_under "inexpost_nofa" sel_nofa)))

