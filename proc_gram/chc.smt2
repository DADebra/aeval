(declare-fun inv ((Array Int Int) Int Int Int) Bool)

(assert (forall ((a (Array Int Int)) (v Int) (l Int) (|_FH_inv_0'| (Array Int Int)) (|_FH_inv_1'| Int) (|_FH_inv_2'| Int) (|_FH_inv_3'| Int)) (=> (and (= a |_FH_inv_0'|) (= 0 |_FH_inv_1'|) (= v |_FH_inv_2'|) (= l |_FH_inv_3'|)) (inv |_FH_inv_0'| |_FH_inv_1'| |_FH_inv_2'| |_FH_inv_3'|))))

(assert (forall ((a (Array Int Int)) (v Int) (l Int) (i Int) (i1 Int) (_FH_inv_0 (Array Int Int)) (|_FH_inv_0'| (Array Int Int)) (_FH_inv_1 Int) (|_FH_inv_1'| Int) (_FH_inv_2 Int) (|_FH_inv_2'| Int) (_FH_inv_3 Int) (|_FH_inv_3'| Int)) (=> (and (< i l) (= i1 (+ i 1)) (= a |_FH_inv_0'|) (= v |_FH_inv_2'|) (= l |_FH_inv_3'|) (distinct (select a i) v) (= a _FH_inv_0) (= i _FH_inv_1) (= v _FH_inv_2) (= l _FH_inv_3) (= i1 |_FH_inv_1'|) (inv _FH_inv_0 _FH_inv_1 _FH_inv_2 _FH_inv_3)) (inv |_FH_inv_0'| |_FH_inv_1'| |_FH_inv_2'| |_FH_inv_3'|))))

(assert (forall ((a (Array Int Int)) (v Int) (l Int) (i Int) (pos Int) (_FH_inv_0 (Array Int Int)) (_FH_inv_1 Int) (_FH_inv_2 Int) (_FH_inv_3 Int)) (=> (and (= a _FH_inv_0) (= i _FH_inv_1) (= v _FH_inv_2) (= l _FH_inv_3) (not (and (< i l) (distinct (select a i) v))) (not (=> (and (< i l) (<= 0 l)) (exists ((pos Int)) (= (select a pos) v)))) (inv _FH_inv_0 _FH_inv_1 _FH_inv_2 _FH_inv_3)) false)))

(check-sat)
