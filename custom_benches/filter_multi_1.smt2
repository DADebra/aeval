(declare-var a (Array Int Int))
(declare-var b (Array Int Int))
(declare-var c (Array Int Int))
(declare-var c1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var j Int)
(declare-var j1 Int)
(declare-var al Int)
(declare-var bl Int)
(declare-var cl Int)
(declare-var cl1 Int)
(declare-var flag Int)
(declare-var flag1 Int)

(declare-rel inv ((Array Int Int) (Array Int Int) (Array Int Int) Int Int Int Int))
(declare-rel inv2 ((Array Int Int) (Array Int Int) (Array Int Int) Int Int Int Int Int Int))
(declare-rel fail ())

(rule (inv a b c al bl 0 0))

(rule (=> (and (inv a b c al bl i cl) (< i al) (= j 0) (= flag 0)
) (inv2 a b c al bl i cl j flag)))

(rule (=> (and (inv2 a b c al bl i cl j flag) (< j bl)
  (= flag1 (ite (= (select b j) (select a i)) 1 flag))
  (= j1 (+ j 1))
) (inv2 a b c al bl i cl j1 flag1)))

(rule (=> (and (inv2 a b c al bl i cl j flag)
  (= c1 (ite (= flag 1) (store c cl (select a i)) c))
  (= cl1 (ite (= flag 1) (+ cl 1) cl))
  (= i1 (+ i 1))
) (inv a b c1 al bl i cl1)))

(rule (=> (and (inv a b c al bl i cl) (>= i al)
  (not (forall ((pos Int)) (=>
    (and (<= 0 pos) (< pos cl))
    (exists ((pos2 Int)) (= (select c pos) (select a pos2)))))))  
	  fail))
(query fail)