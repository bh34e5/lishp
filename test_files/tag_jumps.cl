(tagbody
    (go a)
  c
    (format t "made it") (go b)
  a
    (format t "~a" (+ 12
                      ((lambda (a)
                         (format t "leaving")
                         (go c)
                         (+ a 3))
                       5)))
  b
    (format t "and to here"))
