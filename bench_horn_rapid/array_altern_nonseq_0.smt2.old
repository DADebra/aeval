
(declare-var a (Array Int Int))
(declare-var a1 (Array Int Int))
(declare-var b (Array Int Int))
(declare-var b1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var alen Int)

(declare-rel inv ((Array Int Int) (Array Int Int) Int Int))
(declare-rel fail ())

(rule (inv a b 0 alen))

(rule (=> (and (inv a b i alen)
    (< i alen)
    (= b1 (store (store b i (select a i)) (+ i alen) (select a i)))
    (= i1 (+ i 1))
) (inv a b1 i1 alen)))

(rule (=> (and (inv a b i alen) (>= i alen)
    (not (forall ((j Int)) (=>
        (and (>= j 0) (< j alen))
        (exists ((k Int)) (= (select a j) (select b k)))
    )))
) fail))

(query fail :print-certificate true)
