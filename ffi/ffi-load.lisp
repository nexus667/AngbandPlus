;;; -*- Mode: Lisp; Syntax: Common-Lisp; Package: cl-user -*-

#|

DESC: ffi/ffi-load.lisp - settings that must be set before foreign build
Copyright (c) 2001 - Stig Erik Sand�

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

|#

(in-package :cl-user)

(eval-when (:execute :load-toplevel :compile-toplevel)
 
  (defun quit-game& ()
    "Tries to quit game.."
    #+cmu
    (cl-user::quit)
    #+allegro
    (excl::exit)
    #-(or cmu allegro)
    (warn "Can't quit yet.. fix me..")
  (values))
  
  
  (defun load-shared-lib (&key
			  (lib "./zterm/liblangband_ui.so")
			  (key :unknown))
    "Loads the necessary shared-lib."
    #-lispworks
    (declare (ignore key))
    
    (let ((is-there (probe-file lib)))
      (unless is-there
	(warn "Unable to locate dynamic library ~a, please run 'make'."
	      lib)
	(quit-game&)))
    
    
    #+allegro
    (load lib)
    #+cmu
    (alien:load-foreign lib)
    #+clisp
    nil
    #+lispworks
    (fli:register-module key :real-name lib :connection-style :manual)
    #-(or cmu allegro clisp lispworks)
    (warn "Did not load shared-library.."))
  
  )

(eval-when (:execute :load-toplevel :compile-toplevel)
  (let ((lib-path "./zterm/"))

    #+unix
    (progn
      (setq lib-path
	    #+langband-development "./zterm/"
	    #-langband-development "/usr/lib/"))
    #+win32
    (progn
      ;; hack
      (setq lib-path "c:/cygwin/home/default/langband/zterm/"))

    #+unix
    (progn
      #-cmu
      (load-shared-lib :key :dc :lib (concatenate 'string lib-path "liblangband_dc.so"))
      ;; everyone
      (load-shared-lib :key :lang-ffi :lib (concatenate 'string lib-path "liblangband_ui.so")))
    #+win32
    (progn
      (load-shared-lib :key :lang-ffi :lib (concatenate 'string lib-path "liblangband_ui.dll"))
      
    )))

