
#include "shared.smt2"

(assert (= ANY_INV (either_inorder
  lemma
  (forall ((favar Int)) (=>
    ;(and (<= 0 favar) (<= favar (- (* 2 _FH_inv_3) 1)))
    ;(or
    ;  (and (>= favar 0) (<= favar (- (* 2 _FH_inv_3) 2)))
    ;  (and (>= favar 1) (<= favar (- (* 2 _FH_inv_3) 1)))
    ;)
    (and
      (and
        (<= 0 favar)
        (or
          (and (>= favar 0) (<= favar (- (* 2 _FH_inv_3) 2)))
          (and (>= favar 1) (<= favar (- (* 2 _FH_inv_3) 1)))
        )
      )
      (and
        (<= favar _FH_inv_4)
        (or
          (and (>= favar 0) (<= favar (- (* 2 _FH_inv_3) 2)))
          (and (>= favar 1) (<= favar (- (* 2 _FH_inv_3) 1)))
        )
      )
    )
    (exists ((exvar Int)) (or
      (= (select _FH_inv_2 favar) (select _FH_inv_0 exvar))
      (= (select _FH_inv_2 favar) (select _FH_inv_1 exvar))
    ))
  ))
)))
