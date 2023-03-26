(declare-var a (Array Int Int))
(declare-var b (Array Int Int))
(declare-var b1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var n Int)
(declare-var bl Int)
(declare-var bl1 Int)
(declare-var lbound Int)
(declare-var ubound Int)

(declare-rel inv ((Array Int Int) (Array Int Int) Int Int Int Int Int))
(declare-rel fail ())

(rule (inv a b n lbound ubound 0 0))

(rule (=> (and (inv a b n lbound ubound i bl) (< i n)
  (= b1 (ite
    (and (>= (select a i) lbound) (<= (select a i) ubound))
    (store b bl (select a i))
    b
  ))
  (= bl1 (ite
    (and (>= (select a i) lbound) (<= (select a i) ubound))
    (+ bl 1)
    bl
  ))
  (= i1 (+ i 1))
) (inv a b1 n lbound ubound i1 bl1)))

(rule (=> (and (inv a b n lbound ubound i bl) (>= i n)
  (not (forall ((pos Int)) (=>
    (and (<= 0 pos) (< pos bl))
    (exists ((pos2 Int)) (= (select b pos) (select a pos2)))))))  
	  fail))
(query fail)
