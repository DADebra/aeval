
(declare-fun ANY_INV () Bool)

(assert (= ANY_INV (either_inorder
  (>= _FH_inv_5 0)
  (<= _FH_inv_5 _FH_inv_4)
  (forall ((j Int)) (=> (and (<= 0 j) (< j _FH_inv_5))
    (exists ((k Int)) (and (<= 0 k) (< k _FH_inv_4)
      (= (select _FH_inv_1 j) (select _FH_inv_0 k))
    ))
  ))

  (forall ((j Int)) (=>
    (forall ((k Int)) (=> (and (<= 0 k) (<= k _FH_inv_4))
      (not (= j (select _FH_inv_0 k)))))
    (= (select _FH_inv_2 j) -1)
  ))

  (and
    (=>
      (not (= (select _FH_inv_2 (select _FH_inv_0 _FH_inv_4)) -1))
      (exists ((k Int)) (and (<= 0 k) (< k _FH_inv_5)
        (= (select _FH_inv_0 _FH_inv_4) (select _FH_inv_1 k))
      ))
    )

    (forall ((j Int)) (=> (and (<= 0 j) (< j _FH_inv_4))
      (>= (select _FH_inv_2 (select _FH_inv_0 j)) 0)
    ))

    (forall ((j Int)) (=> (and (<= 0 j) (< j _FH_inv_4))
      (exists ((k Int)) (and (<= 0 k) (< k _FH_inv_5)
        (= (select _FH_inv_0 j) (select _FH_inv_1 k))
      ))
    ))
  )
)))
