

.set IRQ_BASE, 0x20

.section .text

.extern _ZN4myos21hardwarecommunication16InterruptManager15HandleInterruptEhj #interrups.cpp HandleInterrupt function reference


.macro HandleException num
.global _ZN4myos21hardwarecommunication16InterruptManager19HandleException\num\()Ev
_ZN4myos21hardwarecommunication16InterruptManager19HandleException\num\()Ev:
    movb $\num, (interruptnumber)
    jmp int_bottom
.endm

# Macro for implementing HandleInterruptRequest in interrups.cpp
.macro HandleInterruptRequest num
.global _ZN4myos21hardwarecommunication16InterruptManager26HandleInterruptRequest\num\()Ev
_ZN4myos21hardwarecommunication16InterruptManager26HandleInterruptRequest\num\()Ev:
    movb $\num + IRQ_BASE, (interruptnumber)
    pushl $0 # For error register in CPUState
    jmp int_bottom
.endm


HandleException 0x00
HandleException 0x01
HandleException 0x02
HandleException 0x03
HandleException 0x04
HandleException 0x05
HandleException 0x06
HandleException 0x07
HandleException 0x08
HandleException 0x09
HandleException 0x0A
HandleException 0x0B
HandleException 0x0C
HandleException 0x0D
HandleException 0x0E
HandleException 0x0F
HandleException 0x10
HandleException 0x11
HandleException 0x12
HandleException 0x13


# Executes the macro for generating the function with different interrupt numbers
HandleInterruptRequest 0x00
HandleInterruptRequest 0x01
HandleInterruptRequest 0x02
HandleInterruptRequest 0x03
HandleInterruptRequest 0x04
HandleInterruptRequest 0x05
HandleInterruptRequest 0x06
HandleInterruptRequest 0x07
HandleInterruptRequest 0x08
HandleInterruptRequest 0x09
HandleInterruptRequest 0x0A
HandleInterruptRequest 0x0B
HandleInterruptRequest 0x0C
HandleInterruptRequest 0x0D
HandleInterruptRequest 0x0E
HandleInterruptRequest 0x0F
HandleInterruptRequest 0x31

# 0x80: For system calls 
HandleInterruptRequest 0x80 


int_bottom:

    # save registers
    
    ############## TODO: YORUMDAYDI
    #pusha
    #pushl %ds
    #pushl %es
    #pushl %fs
    #pushl %gs
    ##############
    
    # push registers to stack
    # IMPORTANT: The push order is important here. They're in reverse order according to the registers we write in CPUState class.
    #            The stack push order must be reverse according to the written order in the CPUState class. So that when it reads 
    #            from beginning, it reads the correct value. Also, the CPU pushed registers I commented below in CPUState class are
    #            also pushed already before coming here.
    pushl %ebp
    pushl %edi
    pushl %esi

    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    # load ring 0 segment register
    #cld
    #mov $0x10, %eax
    #mov %eax, %eds
    #mov %eax, %ees

    # call C++ Handler
    pushl %esp
    push (interruptnumber)
    call _ZN4myos21hardwarecommunication16InterruptManager15HandleInterruptEhj
    #add %esp, 6
    mov %eax, %esp # switch the stack
    #mov %edx, %eax # TODO: YENI EKLENDI

    # restore registers
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx

    popl %esi
    popl %edi
    popl %ebp
    
    ############## TODO: YORUMDAYDI
    #popl %gs
    #popl %fs
    #popl %es
    #popl %ds
    ##############
    
    #popa
    
    add $4, %esp # TODO: $4'ten $8 yapildi. Cunku eax ayni deger kullanilmayacak.
    
.global _ZN4myos21hardwarecommunication16InterruptManager15InterruptIgnoreEv
_ZN4myos21hardwarecommunication16InterruptManager15InterruptIgnoreEv:

    iret


.data
    interruptnumber: .byte 0
