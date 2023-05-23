(declare-var a (Array Int Int))
(declare-var b (Array Int Int))
(declare-var b1 (Array Int Int))
(declare-var m Int)
(declare-var m1 Int)
(declare-var al Int)
(declare-var bl Int)
(declare-var bl1 Int)
(declare-var i Int)
(declare-var i1 Int)
(declare-var j Int)
(declare-var k Int)
(declare-var l Int)

(declare-rel inv ((Array Int Int) (Array Int Int) Int Int Int Int))
(declare-rel fail ())

(rule (=> (and (= b1 (store b 0 (select a 0))) (= i 1) (= m 0) (= bl 1)) (inv a b1 i m al bl)))

;modified (store b i (select a i)) to (store b bl (select a i)) as i is not the correct index (ex: a = {2,2,3})
(rule (=> (and (inv a b i m al bl) (< i al)
	       (= (ite (< (select a m) (select a i)) (store b bl (select a i)) b) b1)
	       (= (ite (< (select a m) (select a i)) (+ bl 1) bl) bl1)
	       (= (ite (< (select a m) (select a i)) i m) m1)
	       (= i1 (+ i 1))) (inv a b1 i1 m1 al bl1)))

(rule (=> (and (inv a b i m al bl) (>= i al)
	       (not (forall ((k Int)) (=> (and (<= 1 al) (<= 0 k) (< k al) (forall ((l Int)) (=> (and (<= 0 l) (< l k)) (< (select a l) (select a k))))) (exists ((j Int)) (and (<= 0 j) (< j bl) (= (select a k) (select b j))))))))
	  fail))

(query fail)