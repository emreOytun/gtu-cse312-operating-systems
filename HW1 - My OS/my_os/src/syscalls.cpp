
#include <syscalls.h>
 
using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;
 
SyscallHandler::SyscallHandler(InterruptManager* interruptManager, uint8_t InterruptNumber, TaskManager* taskManager)
:    InterruptHandler(interruptManager, InterruptNumber  + interruptManager->HardwareInterruptOffset())
{
    this->taskManager = taskManager;
}

SyscallHandler::~SyscallHandler()
{
}


void printf(char*);
void printfHex32(uint32_t);

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
    CPUState* cpu = (CPUState*)esp;
    

    switch(cpu->eax)
    {
        case 1: 
            // write syscall in linux (sysprintf)
            printf((char*)cpu->ebx);
            break;
            
        case 57: 
            // fork syscall number in linux (sysfork)
            taskManager->Fork(cpu);
            break;
            
        case 58:
            cpu->eax = taskManager->GetCurrentTask()->forkPid;
            break;
            
        case 59: 
            // execve syscall number in linux (sysexecve)
            esp = taskManager->Execve((void (*)()) cpu->ebx);
            break;
            
        case 7:
            // waitpid syscall number in linux (syswaitpid)
            esp = taskManager->Waitpid(cpu->ebx, cpu);
            break;
            
        case 8:
            // exit
            esp = taskManager->Exit();
            break;
            
        case 9:
            esp = taskManager->BlockForCollatz(cpu);
            break;
            
        case 10:
            taskManager->SetLastTaskPriority(cpu->ebx);
            break;
            
        case 11:
            taskManager->CollatzAdded();
            break;
            
        default:
            break;
    }

    
    return esp;
}

