;--------------------------------------------------------------------------
;  setjmp.s
;
;  Copyright (C) 2011-2014, Philipp Klaus Krause
;  Copyright (C) 2021, Sebastian 'basxto' Riedel (sdcc@basxto.de)
;
;  This library is free software; you can redistribute it and/or modify it
;  under the terms of the GNU General Public License as published by the
;  Free Software Foundation; either version 2, or (at your option) any
;  later version.
;
;  This library is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this library; see the file COPYING. If not, write to the
;  Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
;   MA 02110-1301, USA.
;
;  As a special exception, if you link this library with other files,
;  some of which are compiled with SDCC, to produce an executable,
;  this library does not by itself cause the resulting executable to
;  be covered by the GNU General Public License. This exception does
;  not however invalidate any other reasons why the executable file
;   might be covered by the GNU General Public License.
;--------------------------------------------------------------------------

	.area   _CODE

	.globl ___setjmp

___setjmp:
	ldhl	sp, #0
	; Load sp into de.
	ld	e, l
	ld	d, h
	; Load return address into bc.
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	b, a
	; Load env into hl.
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a

	; Store return address.
	ld	a, c
	ld	(hl+), a
	ld	a, b
	ld	(hl+), a

	; Store stack pointer.
	ld	a, e
	ld	(hl+), a
	ld	(hl), d

	; Return 0.
	ld	de, #0
	ret

.globl _longjmp

_longjmp:
	; We'll never jump back, we can lose this
	pop	af
	pop	hl ; env
	pop	de ; return value

	; Ensure that return value is non-zero.
	ld	a, e
	or	a, d
	jr	NZ, 0001$
	inc	de
0001$:

	; Get return address.
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl+)
	ld	b, a
	; Get stack pointer.
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a

	; Adjust stack pointer.
	ld	sp, hl

	; Set return address.
	ld	l, c
	ld	h, b
	pop	af

	; Return value is in de.

	; Jump.
	jp	(hl)
