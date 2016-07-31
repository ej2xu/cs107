
(define (leap-year? year)
  (or (and (zero? (remainder year 4))
	   (not (zero? (remainder year 100))))
      (zero? (remainder year 400))))


(define (fibonacci n)
  (if (< n 2) n
      (+ (fibonacci (- n 1))
	 (fibonacci (- n 2)))))


(define (fast-fibonacci n)
  (define (fast-fibonacci-helper n base-0 base-1)
    (cond ((zero? n) base-0)
	  ((zero? (- n 1)) base-1)
	  (else (fast-fibonacci-helper (- n 1) base-1 (+ base-0 base-1)))))
  (fast-fibonacci-helper n 0 1))


(define (sum ls)
  (if (null? ls) 0
      (+ (car ls) (sum (cdr ls)))))


(define (dot-product one two)
  (if (null? one) 0
      (+ (* (car one) (car two))
	 (dot-product (cdr one) (cdr two)))))


(define (generate-concatenations strings)
  (generate-concatenations-using strings ""))

(define (generate-concatenations-using strings accumulation)
  (if (null? strings) '()
      (cons (string-append accumulation (car strings))
	    (generate-concatenations-using (cdr strings)
					   (string-append accumulation (car strings))))))


(define (pwr base exponent)
  (if (zero? exponent) 1
      (let ((root (pwr base (quotient exponent 2))))
	(if (zero? (remainder exponent 2))
	    (* root root)
	    (* root root base)))))


(define (flatten sequence)
  (cond ((null? sequence) '())
	((list? (car sequence)) (append (flatten (car sequence)) (flatten (cdr sequence))))
	(else (cons (car sequence) (flatten (cdr sequence))))))


(define (filter sequence pred)
  (if (null? sequence) '()
      (let ((filter-of-rest (filter (cdr sequence) pred)))
	(if (pred (car sequence))
	    (cons (car sequence) filter-of-rest)
	    filter-of-rest))))


(define (partition pivot num-list)
  (if (null? num-list) '(() ())
      (let ((split-of-rest (partition pivot (cdr num-list))))
	(if (< (car num-list) pivot)
	    (list (cons (car num-list) (car split-of-rest)) (cadr split-of-rest))
	    (list (car split-of-rest) (cons (car num-list) (car (cdr split-of-rest))))))))


(define (quicksort num-list)
  (if (<= (length num-list) 1) num-list
      (let ((split (partition (car num-list) (cdr num-list))))
	(append (quicksort (car split)) 
		(list (car num-list)) 
		(quicksort (cadr split))))))


(define (merge list1 list2 comp)
  (cond ((null? list1) list2)
	((null? list2) list1)
	((comp (car list1) (car list2)) (cons (car list1) (merge (cdr list1) list2 comp)))
	(else (cons (car list2) (merge list1 (cdr list2) comp)))))


(define (prefix-of-list ls k)
  (if (or (zero? k) (null? ls)) '()
      (cons (car ls) (prefix-of-list (cdr ls) (- k 1)))))


(define (mergesort ls comp)
  (if (<= (length ls) 1) ls
      (let ((front-length (quotient (length ls) 2))
	    (back-length (- (length ls) (quotient (length ls) 2))))
	(merge (mergesort (prefix-of-list ls front-length) comp)
	       (mergesort (prefix-of-list (reverse ls) back-length) comp)
	       comp))))


(define (unary-map fn seq)
  (if (null? seq) '()
      (cons (fn (car seq)) (unary-map fn (cdr seq)))))


(define (depth tree)
  (if (or (not (list? tree)) (null? tree)) 0
      (+ 1 (apply max (map depth tree)))))


(define (flatten-list ls)
  (cond ((null? ls) '())
	((not (list? ls)) (list ls))
	(else (apply append (map flatten-list ls)))))


(define (translate numbers delta)
  (map (lambda (number) (+ number delta)) numbers))

;;
;; Function: power-set
;; ------------------
;; The powerset of a set is the set of all its subsets.
;; The key recursive breakdown is:
;;
;;       The powerset of {} is {{}}.  That's because the empty 
;;           set is a subset of itself.
;;       The powerset of a non-empty set A with first element a is
;;           equal to the concatenation of two sets:
;;              - the first set is the powerset of A - {a}.  This
;;                recursively gives us all those subsets of A that
;;                exclude a.
;;              - the second set is once again the powerset of A - {a},
;;                except that a has been prepended aka consed to the
;;                front of every subset.
;;

(define (power-set set)
  (if (null? set) '(())
      (let ((power-set-of-rest (power-set (cdr set))))
	(append power-set-of-rest
		(map (lambda (subset) (cons (car set) subset)) 
		     power-set-of-rest)))))

;;
;; Function: k-subsets
;; -------------------
;; Another combinatorics problem, and this one is more difficult
;; than powerset.  k-subsets basically constructs the set of all
;; subsets of a specific size.
;; 
;; Generates the set of all k-subsets of the cdr, and then
;; appends to that the set of all k-1-subsets of the cdr, with the
;; car prepended to each and every one of them.
;;

(define (k-subsets set k)
  (cond ((eq? (length set) k) (list set))
	((zero? k) '(()))
	((or (negative? k) (> k (length set))) '())
	(else (let ((k-subsets-of-rest (k-subsets (cdr set) k))
		    (k-1-subsets-of-rest (k-subsets (cdr set) (- k 1))))
		(append (map (lambda (subset) (cons (car set) subset)) k-1-subsets-of-rest)
			k-subsets-of-rest)))))

;;
;; Function: is-up-down?
;; ---------------------
;; Returns true if and only if the specified sequence
;; is up-down, where the definition of up is dictated by
;; the implementation of comp.  All sequences of length
;; 0 and 1 are trivially considered to the up-down, since
;; it's impossible for it to have any violations.
;; Otherwise, we confirm that the car and the cadr respect
;; the specified predicate, and then confirm that the cdr 
;; respects the invertion of the predicate.
;;
;; Note that the lambda switches the two roles of own and two.
;; Clever, but the drawback is that lambdas layer over lambdas
;; layer over lambdas for long sequences.
;; 

(define (is-up-down? ls comp)
  (or (null? ls)
      (null? (cdr ls))
      (and (comp (car ls) (cadr ls))
	   (is-up-down? (cdr ls) (lambda (one two) (comp two one))))))

;;
;; Function: up-down?
;; ------------------
;; Same is is-up-down?, save for the fact that
;; we recur on the cddr instead of the cdr, do more
;; base case checking, all so we can avoid the layering of
;; the lambdas.
;;

(define (up-down? ls comp)
  (or (null? ls)
      (null? (cdr ls))
      (and (comp (car ls) (cadr ls))
	   (or (null? (cddr ls))
	       (and (comp (caddr ls) (cadr ls))
		    (up-down? (cddr ls) comp))))))

(define (construct-permutation-generator comp inverted-permute ls)
  (lambda (number)
    (apply append
	   (map (lambda (permutation)
		  (if (comp number (car permutation))
		      (list (cons number permutation))
		      '())) 
		(inverted-permute (remove ls number))))))

(define (up-down-permute ls)
  (if (<= (length ls) 1) (list ls)
      (apply append (map (construct-permutation-generator < down-up-permute ls) ls))))

(define (down-up-permute ls)
  (if (<= (length ls) 1) (list ls)
      (apply append (map (construct-permutation-generator > up-down-permute ls) ls))))

;;
;; Function: remove
;; ----------------
;; Generates a copy of the incoming list, except that
;; all elements that match the specified element in the equal?
;; sense are excluded.
;;

(define (remove ls elem)
  (cond ((null? ls) '())
	((equal? (car ls) elem) (remove (cdr ls) elem))
	(else (cons (car ls) (remove (cdr ls) elem)))))

;;
;; Function: permutations
;; ----------------------
;; Generates all of the permutations of the specified list, operating
;; on the understanding that each of the n elements appears as the first
;; element of 1/n of the permutations.  The best approach uses mapping
;; to generate n different sets, where each of these sets are those
;; permutations with the nth element at the front.  We use map to transform
;; each element x into the subset of permutations that have x at the
;; front.  The mapping function that DOES that transformation is itself
;; a call to map, which manages to map an anonymous consing routine
;; over all of the permutations of the list without x.  This is
;; as dense as it gets.
;;

(define (permutations items)
  (if (null? items) '(())
      (apply append
	     (map (lambda (element)
		    (map (lambda (permutation)
			   (cons element permutation))
			 (permutations (remove items element))))
		  items))))

;;
;; Function: construct-permutation-prepender
;; ----------------------------------------
;; Evaluates to a function that takes an arbitrary
;; list (presumably a permutation) and conses the
;; incoming element to the front of it.  
;;
;; Notice that the implementation of the constructed
;; function is framed in terms of whatever data
;; happens to be attached to the incoming element.
;;

(define (construct-permutation-prepender element)
  (lambda (permutation)
    (cons element permutation)))

;;
;; Function: construct-permutations-generator 
;; ------------------------------------------
;; Evaluates to a function (yes, a function) on one argument 
;; (called element) that knows how to generate all of the permutations of the
;; specified list (called items) such that the supplied element
;; argument is at the front.
;;

(define (construct-permutations-generator items)
  (lambda (element)
    (map (construct-permutation-prepender element) (all-permutations (remove items element)))))

;;
;; Function: all-permutations
;; --------------------------
;; Generates all of the permutations of the specified
;; list called items.  Algorithmically identical to
;; the permutations function written above.
;;

(define (all-permutations items)
  (if (null? items) '(())
      (apply append
	     (map (construct-permutations-generator items) items))))

;;
;; Function: generic-map
;; ---------------------
;; Relies on the services of the unary-map function to 
;; gather the cars of all the lists to be fed the supplied
;; fn, and then gathers all of the cdrs for the recursive call.
;;

(define (generic-map fn primary-list . other-lists)
  (if (null? primary-list) '()
      (cons (apply fn (cons (car primary-list) (unary-map car other-lists)))
	    (apply generic-map (cons fn (cons (cdr primary-list) (unary-map cdr other-lists)))))))


(define (marry str-lst)
  (cond ((null? str-lst) '())
	((eq? (length str-lst) 1) str-lst)
	(else (cons (list (car str-lst) (cadr str-lst)) (marry (cddr str-lst))))))

(define (map-ew func structure)
  (map (lambda (elem)
	 (if (list? elem) (map-ew func elem) (func elem))) structure))

(define (lcprefix seq1 seq2)
  (cond ((or (null? seq1) (null? seq2)) '())
	((not (eq? (car seq1) (car seq2))) '())
	(else (cons (car seq1) (lcprefix (cdr seq1) (cdr seq2))))))

(define (mdp func seq)
  (if (null? seq) '()
      (cons (func seq) (mdp func (cdr seq)))))

(define (lc-sublist seq1 seq2)
  (car (quicksort (generate-all-subsets seq1 seq2) list-length>?)))

(define (generate-all-subsets seq1 seq2)
  (apply append (mdp (lambda (suffix1)
		       (mdp (lambda (suffix2) (lc-sublist suffix1 suffix2)) seq2)) seq1)))

(define (list-length>? ls1 ls2)
  (> (length ls1) (length ls2)))
