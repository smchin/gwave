;
; module illustrating how to add your own menus to gwave
;

(define-module (app gwave extra-menus)
  :use-module (gtk gtk)
  :use-module (gtk gdk)
  :use-module (app gwave cmds)
  :use-module (app gwave std-menus)
)

(debug-enable 'debug)
(read-enable 'positions)

(dbprint "extra-menus.scm running\n")

; demonstrating how we can add our own menu to the end
; of the menubar by using append-hook.
(append-hook! 
 new-wavewin-hook
 (lambda ()
;   (display "in new-wavewin-hook") (newline)
   (let* ((mbar (get-wavewin-menubar))
	  (menu (menu-create mbar "SGT")))
       (add-menuitem menu "my menu" #f)
       (add-menuitem menu "garbage collect" gc)
       (add-menuitem menu "list files"
		     (lambda () 
		       (display "wavefile-list:") (newline)
		       (display (wavefile-list))
		       (newline)))
)))

(dbprint "extra-menus.scm done\n")