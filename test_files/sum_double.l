(labels ((add-one (arg1) (+ arg1 1))
         (double (arg1) (+ arg1 arg1))
         (sum (arg1 arg2) (+ arg1 arg2)))
  (sum (double (add-one 0)) (double (double 2))))
