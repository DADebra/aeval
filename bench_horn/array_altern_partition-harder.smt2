(declare-var a (Array Int Int))
(declare-var b (Array Int Int))
(declare-var b1 (Array Int Int))
(declare-var c (Array Int Int))
(declare-var c1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var al Int)
(declare-var bl Int)
(declare-var bl1 Int)
(declare-var cl Int)
(declare-var cl1 Int)
(declare-var pos Int)

(declare-rel inv ((Array Int Int) (Array Int Int) (Array Int Int) Int Int Int Int))
(declare-rel fail ())

(rule (inv a b c 0 al 0 0))

(rule (=> (and (inv a b c i al bl cl) (< i al)
	       (= c1 (ite (not (>= (select a i) 0)) (store c cl (select a i)) c))
	       (= cl1 (ite (not (>= (select a i) 0)) (+ cl 1) cl))
	       (= b1 (ite (>= (select a i) 0) (store b bl (select a i)) b))
	       (= bl1 (ite (>= (select a i) 0) (+ bl 1) bl))
	       (= i1 (+ i 1))) (inv a b1 c1 i1 al bl1 cl1)))

(rule (=> (and (inv a b c i al bl cl) (>= i al)
	       (not (forall ((pos Int)) (=> (and (<= 0 al) (<= 0 pos) (< pos bl)) (exists ((pos2 Int)) (and (<= 0 pos2) (< pos2 al) (= (select b pos) (select a pos2)))))))) 
	  fail))
(query fail)

	  
