
#include "shared.smt2"

(declare-fun sel_sel () Int)

;(assert (= sel_sel (select $ARRAY_INT_INT$_VARS (select 

(assert (= ANY_INV (either_inorder
  lemma
  ;(=> lemma lemma)
  ;(forall ((favar Int)) (=>
  ;  (and (>= favar 0) (< favar _FH_inv_4))
  ;  (exists ((exvar Int)) (and (>= exvar 0) (< exvar _FH_inv_5)
  ;    ;(= (select _FH_inv_0 favar) (select _FH_inv_1 exvar))
  ;    inexpost
  ;  ))
  ;))
  ;(forall ((favar Int)) (=>
  ;  (and (>= favar 0) (< favar _FH_inv_4))
  ;  (exists ((exvar Int)) (and (>= exvar 0) (< exvar _FH_inv_5)
  ;    (= (select _FH_inv_0 favar) (select _FH_inv_1 exvar))
  ;  ))
  ;))

  ;(forall ((exvar Int)) (=>
  ;  (and (>= exvar 0) (< exvar INT_VARS))
  ;  inexpost_nofa
  ;))

  ;(forall ((favar Int)) (=>
  ;  (and (>= favar _FH_inv_4) (< favar _FH_inv_6))
  ;  (= (select _FH_inv_3 (select _FH_inv_0 favar)) -1)
  ;)) ; Invalid: 'A' might have duplicates

  ;(=>
  ;  (= _FH_inv_4 0)
  ;  (= _FH_inv_3 ((as const (Array Int Int)) -1))
  ;) ; Valid but useless

  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_4))
    (=>
      (distinct (select _FH_inv_3 (select _FH_inv_0 favar)) -1)
      (exists ((exvar Int)) (and (>= exvar 0) (< exvar _FH_inv_4)
        (= (select _FH_inv_0 exvar) (select _FH_inv_0 favar))
      ))
    )
  ))

  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_5))
    (=>
      (distinct (select _FH_inv_3 (select _FH_inv_1 favar)) -1)
      (exists ((exvar Int)) (and (>= exvar 0) (< exvar _FH_inv_5)
        (= (select _FH_inv_1 exvar) (select _FH_inv_1 favar))
      ))
    )
  ))

  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_4))
    (distinct (select _FH_inv_3 (select _FH_inv_0 favar)) -1)
  ))

  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_5))
    (distinct (select _FH_inv_3 (select _FH_inv_1 favar)) -1)
  ))

  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_4))
    (=>
      (distinct (select _FH_inv_3 (select _FH_inv_0 favar)) -1)
      (exists ((exvar Int)) (and (>= exvar 0) (< exvar _FH_inv_5)
        (= (select _FH_inv_1 exvar) (select _FH_inv_0 favar))
      ))
    )
  )) ; Valid (I believe) but Z3 cant prove

  (forall ((favar Int)) (=>
    (and (>= favar 0) (< favar _FH_inv_4))
    (exists ((exvar Int)) (and (>= exvar 0) (< exvar _FH_inv_5)
      (= (select _FH_inv_0 favar) (select _FH_inv_1 exvar))
    ))
  ))
)))