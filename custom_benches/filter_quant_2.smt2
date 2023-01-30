(declare-var a (Array Int Int))
(declare-var b (Array Int Int))
(declare-var c (Array Int Int))
(declare-var c1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var al Int)
(declare-var bl Int)
(declare-var cl Int)
(declare-var cl1 Int)

(declare-rel inv ((Array Int Int) (Array Int Int) (Array Int Int) Int Int Int Int))
(declare-rel fail ())

(rule (inv a b c al bl 0 0))

(rule (=> (and (inv a b c al bl i cl) (< i al)
  (= c1 (ite
    (exists ((p Int)) (= (select b p) (select a i)))
    (store c cl (select a i))
    c
  ))
  (= cl1 (ite
    (exists ((p Int)) (= (select b p) (select a i)))
    (+ cl 1)
    cl
  ))
  (= i1 (+ i 1))
) (inv a b c1 al bl i1 cl1)))

(rule (=> (and (inv a b c al bl i cl) (>= i al)
  (not (forall ((pos Int)) (=>
    (and (<= 0 pos) (< pos al) (exists ((p Int)) (= (select b p) (select a pos))))
    (exists ((pos2 Int)) (= (select a pos) (select c pos2)))))))  
	  fail))
(query fail)
