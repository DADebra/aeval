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

(declare-rel fail ())

(declare-rel inv (
  (Array Int Int)
  (Array Int Int)
  (Array Int Int)
  (Array Int Int)
  Int
  Int
  Int))

(define-fun solution_inv (
  (A (Array Int Int))
  (B (Array Int Int))
  (P (Array Int Int))
  (C (Array Int Int))
  (i Int)
  (len Int)
  (N Int)) Bool
    (and

      (>= len 0)
      (<= len i)

      ; each "newly observed" element from A should not be registered in P
      (forall ((j Int))
         (=> (forall ((k Int))
           (=> (and (<= 0 k) (< k i)) (not (= j (select A k)))))
             (= (select P j) -1)))
    ))

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
     (=> (forall ((k Int))
       (=> (and (<= 0 k) (< k i)) (not (= j (select A k)))))
         (= (select P j) -1))))) fail))

(query fail)
