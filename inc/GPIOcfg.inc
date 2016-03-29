;******************************************
;										   
; Pin definitions:						   
;										   
;******************************************
p4 data 0xc0	;GPIO P4 for stc15
p5 data 0xc8	;GPIO P5 for stc15

OUT1	BIT P2.5
OUT2	BIT P2.6
OUT3	BIT P2.7
OUT4	BIT P4.5
OUT5	BIT P4.6
OUT6	BIT P0.0
OUT7	BIT P0.1
OUT8	BIT P0.2
OUT9	BIT P0.3
OUT10	BIT P0.4
OUT11	BIT P5.5
OUT12	BIT P2.4

; You must set the following reload marcos
; according to the pins defined above!
p0reload equ 00011111b
p2reload equ 11110000b
p4reload equ 01100000b
p5reload equ 00100000b

;******************************************
;										   
; Maximun servo degree
;										   
;******************************************
MAXDEGREE equ 180
