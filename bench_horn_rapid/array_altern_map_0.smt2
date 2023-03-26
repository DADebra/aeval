(declare-var a (Array Int Int))
(declare-var b (Array Int Int))
(declare-var b1 (Array Int Int))
(declare-var c (Array Int Int))
(declare-var c1 (Array Int Int))
(declare-var p (Array Int Int))
(declare-var p1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var len Int)
(declare-var len1 Int)
(declare-var n Int)

(declare-var a_i Int)
(declare-var p_a_i Int)

(declare-rel inv ((Array Int Int) (Array Int Int) (Array Int Int) (Array Int Int) Int Int Int))
(declare-rel fail ())

(rule (inv a b c ((as const (Array Int Int)) -1) 0 0 n))

(rule (=> (and (inv a b c p i len n) (< i n)
    (= a_i (select a i))
    (= p_a_i (select p a_i))
    (= b1 (ite (= p_a_i -1) (store b len a_i) b))
    (= c1 (ite (= p_a_i -1) (store c len 0) (store c p_a_i (+ 1 (select c p_a_i)))))
    (= p1 (ite (= p_a_i -1) (store p a_i 1) p))
    (= len1 (ite (= p_a_i -1) (+ 1 len) len))
    (= i1 (+ 1 i))
  ) (inv a b1 c1 p1 i1 len1 n)))

(rule (=> (and (inv a b c p i len n) (>= i n)
    (not (forall ((i1 Int)) (=>
        (and (>= i1 0) (< i1 n))
        (exists ((j1 Int)) (and (>= j1 0) (< j1 len) (= (select a i1) (select b j1))))
    ))))
    fail
))

(query fail)
