NAME PWM
PUBLIC _PWMSet, PWMStart

$include (GPIOcfg.inc)

;******************************************
;										   
; Private Marcos:						   
;										   
;******************************************
; timer reset values:
_17_5ms equ -48125
_0_6ms	equ -1650
_10us	equ -27

;******************************************
;
; DATA segment
;
;******************************************
mybits 		segment DATA BITADDRESSABLE
emu_TIMs 	segment DATA

; First, declear some BOOL varibles:
	RSEG mybits
bits:	DS 	4		; We used 32 bits.
allzero	BIT	bits.0	; Show that all PWM channels 
					; had been reset to 0.
; Next, declear 12 emulated TIM coltrol-bit:
IRP emu_TR_num, <0,1,2,3,4,5,6,7,8,9,10,11>
emuTR&emu_TR_num	BIT    allzero + &emu_TR_num
ENDM
; Then, declear 12 emulated TIM counters:
	RSEG emu_TIMs
emuCNTs:	DS	12
; And, declear 12 TIM reload varibles:
emuCCRs:	DS	12
; Finally, declear a MAIN TIM counter:
emuMCNT:	DS	1

										   
;******************************************
;
; PWM interfaces:
;										   
;******************************************
;**										 **
;*	PWMSet(char channel, char degree);	  *
;**										 **
PWMSet?PWM			SEGMENT CODE
	RSEG PWMSet?PWM
_PWMSet:	  
	push PSW
	push ACC
	mov A, R0
	push ACC
	mov A, R5
	mov R0, A
	dec R7
	mov A, #emuCCRs
	add A, R7		; CCR dst address
	xch A, R0
	mov @R0, A
	xch A, R0
	mov A, #emuCNTs
	add A, R7		; CNT dst address
	xch A, R0
	mov @R0, A
	pop ACC
	mov R0, A
	pop ACC
	pop PSW
	ret

;**										 **
;*	PWMStart(void);						  *
;**										 **
PWMStart?PWM		SEGMENT CODE
	RSEG PWMStart?PWM
PWMStart:
	clr tr0
	; reset PWM emulated timers
	setb allzero
	mov emuMCNT, #MAXDEGREE
	; reset t0;17.5ms
	mov tl0, #_17_5ms & 0xff
	mov th0, #_17_5ms >> 8
	setb et0		;enable t0 interrupt
	setb ea			;enable all interrupts
	setb tr0		;start t0
	ret

;******************************************
;
; TIM0 IRQ
;	Update output pins to generate pulses
;										   
;******************************************
CSEG AT 0X000B
	ljmp T0_IRQ
T0_IRQ?PWM		SEGMENT CODE
	RSEG T0_IRQ?PWM
T0_IRQ:
	jbc allzero, reload_all
	jmp gen_PWM
reload_all:
	clr tr0				; stop T0
	orl P0, #p0reload	; reload PWM outputs
	orl P2, #p2reload
	orl P4, #p4reload
	orl P5, #p5reload
	mov tl0, #_0_6ms & 0xff 	;reload T0
	mov th0, #_0_6ms >> 8
	setb tr0					;start T0
	mov tl0, #_10us & 0xff	;Now set the reload
	mov th0, #_10us >> 8	;value: 10us
	RETI
gen_PWM:
	push psw
	; Next, increase and check PWM timers one by one...
IRP PWM_TIM_num, <1,2,3,4,5,6,7,8,9,10,11,12>
	jnb OUT&PWM_TIM_num, PWMCh&PWM_TIM_num
	djnz (emuCNTs + &PWM_TIM_num - 1), PWMCh&PWM_TIM_num
	clr OUT&PWM_TIM_num
PWMCh&PWM_TIM_num:
ENDM
	djnz emuMCNT, IRQRet
	; reload T0 and interrupt after 17.5ms
	clr TR0
	mov tl0, #_17_5ms & 0xff
	mov th0, #_17_5ms >> 8
	setb TR0
	; reload all PWM timers
	mov emuMCNT, #MAXDEGREE
IRP PWM_CNT_num, <0,1,2,3,4,5,6,7,8,9,10,11>
	mov (emuCNTs + &PWM_CNT_num), (emuCCRs + &PWM_CNT_num)
ENDM
	; and then start them all
	mov bits, #0xff
	mov bits+1, #0xff
IRQRet:
	pop psw
	RETI

END