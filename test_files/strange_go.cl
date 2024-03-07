(tagbody
  (let ((my-list (list 1 2 3 4)))
    (mapcar #'(lambda (a)
                (if (= a 3)
                  (go out)
                  (format t "Found ~a~%" a)))
            my-list)
    (format t "End Loop~%"))
  out
  (format t "Broke out; shouldn't see the 'End Loop'~%"))
