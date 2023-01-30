(declare-var A (Array Int Int))
(declare-var B (Array Int Int))
(declare-var P (Array Int Int))
(declare-var C (Array Int Int))
(declare-var B1 (Array Int Int))
(declare-var P1 (Array Int Int))
(declare-var C1 (Array Int Int))
(declare-var i Int)
(declare-var i1 Int)
(declare-var len Int)
(declare-var len1 Int)
(declare-var N Int)

(declare-rel inv ((Array Int Int) (Array Int Int) (Array Int Int) (Array Int Int)
  Int Int Int))

(declare-rel fail ())

(rule (=>
   (and
     (>= N 0)
     (= P ((as const (Array Int Int)) -1))
     (= C ((as const (Array Int Int)) 0))
     (= i 0)
     (= len 0))
   (inv A B P C i len N)))

(rule (=>
  (and (inv A B P C i len N)
    (< i N)
    (= (select P (select A i)) -1)
    (= B1 (store B len (select A i)))
    (= P1 (store P (select A i) len))
    (= C1 (store C len 1))
    (= i1 (+ i 1))
    (= len1 (+ len 1)))
  (inv A B1 P1 C1 i1 len1 N)))

(rule (=>
  (and (inv A B P C i len N)
    (< i N)
    (not (= (select P (select A i)) -1))
    (= C1 (store C (select P (select A i))
      (+ (select C (select P (select A i))) 1)))
    (= i1 (+ i 1)))
  (inv A B P C1 i1 len N)))

(rule (=> (and (inv A B P C i len N) (>= i N)
  (not (forall ((j Int))
    (=> (and (<= 0 j) (< j N))
        (exists ((k Int))
          (and (<= 0 k) (< k len)
            (= (select A j) (select B k))))
      )))) fail))

(query fail)
