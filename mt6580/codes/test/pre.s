	.section .vectors
_vectors:
	b _start	/* Reset */
	b _vec_dummy	/* Undefined Instruction */
	b _vec_dummy	/* Supervisor Call */
	b _vec_dummy	/* Prefetch Abort */
	b _vec_dummy	/* Data Abort */
	b .		/* Not Used */
	b _vec_dummy	/* IRQ */
	b _vec_dummy	/* FIQ */

_vec_dummy:
	b .

	.section .text.startup
	.global _start
_start:
	/* set the stack pointer */
	ldr sp, =__sp_system
	
	/* clear bss */
	ldr r0, =_sbss
	ldr r1, =#0
	ldr r2, =_ebss
	sub r2, r0
	blx memset
	
	/* goto main */
	blx main
	
	/* halt */
	b .
