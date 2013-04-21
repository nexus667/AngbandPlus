;; -*- Mode: Lisp; Syntax: Common-Lisp; Package: org.langband.engine -*-

#|

DESC: level.lisp - describing a level
Copyright (c) 2000-2001 - Stig Erik Sand�

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

|#

(in-package :org.langband.engine)


(defclass level (activatable)
  ((id      :accessor level.id      :initarg :id      :initform 'level)
   (dungeon :accessor level.dungeon :initarg :dungeon :initform nil)
   (rating  :accessor level.rating  :initarg :rating  :initform 0)
   (depth   :accessor level.depth   :initarg :depth   :initform 0))
  (:documentation "A representation of a level.  Meant to be subclassed."))


(defclass random-level (level)
  ((id :initform 'random-level)))


(defclass themed-level (level)
  ((id :initform 'themed-level)))
     

(defgeneric generate-level! (level player)
  (:documentation "Returns the level-object."))
  
(defgeneric create-appropriate-level (variant old-level player depth)
  (:documentation "Returns an appropriate level for the given
variant and player."))
  
(defgeneric level-ready? (level)
  (:documentation "Returns T if the level is ready for use, returns NIL otherwise."))
  
(defgeneric get-otype-table (level var-obj)
  (:documentation "hack, may be updated later."))
  
(defgeneric get-mtype-table (level var-obj)
  (:documentation "hack, may be updated later."))

(defgeneric find-appropriate-monster (level room player)
  (:documentation "Returns an appropriate monster for a given
level/room/player combo.  Allowed to return NIL."))

;; move to better place later
(defgeneric print-depth (level setting)
  (:documentation "fix me later.. currently just prints depth."))


(defgeneric register-level! (var-obj level-key &key object-filter monster-filter &allow-other-keys)
  (:documentation "Registers a level-key in the variant as a later place-hanger for code."))


(defmethod find-appropriate-monster (level room player)
  (declare (ignore room player))
  (error "No proper FIND-APPROPRIATE-MONSTER for ~s" (type-of level)))
 
(defmethod generate-level! (level player)
  (declare (ignore level player))
  (warn "The basic GENERATE-LEVEL is not implemented, please pass
a proper LEVEL object.")
  nil)

;; see generate.lisp and variants



(defmethod create-appropriate-level (variant old-level player depth)
  (declare (ignore old-level player depth))
  (error "CREATE-APPROPRIATE-LEVEL not implemented for variant ~a"
	 (type-of variant)))



(defmethod level-ready? (level)
  (declare (ignore level))
  (error "pass a proper level to LEVEL-READY?"))


;;; random levels (see also generate.lisp)

;; a simple builder, register it in your variant as 'random-level
(defun make-random-level-obj ()
  (make-instance 'random-level :depth 0 :rating 0))

(defmethod level-ready? ((level random-level))
  (when (level.dungeon level)
    t))


(defmethod register-level! (var-obj id  &key object-filter monster-filter &allow-other-keys)
  (assert (not (eq nil var-obj)))
  (assert (symbolp id))
  (assert (hash-table-p (variant.monsters var-obj)))
  (assert (hash-table-p (variant.objects var-obj)))
  (assert (hash-table-p (variant.filters var-obj)))

  (let ((mon-table (make-game-obj-table))
	(obj-table (make-game-obj-table)))
    
    (setf (gobj-table.obj-table mon-table) (make-hash-table :test #'equal))
    (setf (gobj-table.obj-table obj-table) (make-hash-table :test #'equal))

    (setf (gethash id (variant.monsters var-obj)) mon-table)
    (setf (gethash id (variant.objects var-obj))  obj-table)

    ;; fix
    (when object-filter
      (if (not (functionp object-filter))
	  (lang-warn "Object-filter ~s for level ~s is not a function." object-filter id)
	  (pushnew (cons id object-filter)
		   (gethash :objects (variant.filters var-obj))
		   :key #'car)))
    
    (when monster-filter
      (if (not (functionp monster-filter))
	  (lang-warn "Monster-filter ~s for level ~s is not a function." monster-filter id)
	  (pushnew (cons id monster-filter)
		   (gethash :monsters (variant.filters var-obj))
		   :key #'car)))
    
    ))

(defun %get-var-table (var-obj key slot)
  ""
  (let ((id (etypecase key
	      (level (level.id key))
	      (symbol key))))
    (let ((mon-table (slot-value var-obj slot)))
      (when mon-table
	(gethash id mon-table)))))



(defmethod get-otype-table ((level level) var-obj)
  (%get-var-table var-obj level 'objects))

(defmethod get-otype-table ((level (eql 'level)) var-obj)
  (%get-var-table var-obj level 'objects))



(defmethod get-mtype-table ((level level) var-obj)
  (declare (ignore var-obj))
  (error "WRONG MTYPE"))

(defmethod get-mtype-table ((level (eql 'level)) var-obj)
  (declare (ignore var-obj))
  (error "WRONG MTYPE"))


