 
#ifndef __MYOS__MULTITASKING_H
#define __MYOS__MULTITASKING_H

#include <common/types.h>
#include <gdt.h>

namespace myos
{
    const common::uint32_t MAX_STACK_SIZE = 4096;
    const common::uint32_t MAX_NUM_TASKS = 256;
    
    typedef enum { High, Medium, Low } Priority; // The highest priority has the minimum value
    typedef enum { Ready, Running, Blocked, Terminated } State;
    
    typedef enum { RoundRobin, PreemptivePriority } SchedulerType;
    typedef enum { LifeCycleA, LifeCycleB1, LifeCycleB2, LifeCycleB3, LifeCycleB4 } LifeCycleType;
    typedef enum { PrintEverySwitch, PrintEveryTimeInterrupt, PrintOnlyTermination, DoNotPrint } ProcessTablePrintType;
    
    class Helper {
    public:
        static void Delay() 
        {
            for (int i = 0; i < 750111100; ++i) {}
        }
    };
    
    class Task;
    
    struct CPUState
    {
        /* Pushed by interruptstubs.s */
        common::uint32_t eax;
        common::uint32_t ebx; // base register
        common::uint32_t ecx; // counting register 
        common::uint32_t edx; // data register

        common::uint32_t esi; // stack index 
        common::uint32_t edi; // data index
        common::uint32_t ebp; // stack base pointer

        /*
        ////////////////////
        common::uint32_t gs;
        common::uint32_t fs;
        common::uint32_t es;
        common::uint32_t ds;
        /////////////////////
        */
        
        common::uint32_t error; // for error code

        /* Pushed by processor */
        common::uint32_t eip; // instruction pointer 
        common::uint32_t cs; // code segment 
        common::uint32_t eflags; // flags
        common::uint32_t esp; // stack pointer s
        common::uint32_t ss;  // stack segment
    } __attribute__((packed));
    
    
    class Task
    {
    friend class TaskManager;
    private:
        common::uint8_t stack[MAX_STACK_SIZE]; // 4 KiB
        CPUState* cpustate;
        
        common::int32_t pid;
        common::int32_t ppid;
        
        Priority priority;
        State state;
        
        bool waitingChild; // Indicates if this task is waiting any child 
        int waitingChildId; // Indicates which child this task is waiting (-1 or child pid)
        
        bool parentTookInWait; // If parent has taken this in waitpid already
        
        //common::int32_t forkPid;
        common::int32_t arrivalOrder;
        
    public:
         common::int32_t forkPid;
        
        Task();
        Task(GlobalDescriptorTable *gdt, void entrypoint());
        Task(const Task& task);
        ~Task();
        void Copy(const Task* oth);
        void CopyCpuState(CPUState* cpustate);
        void Reset(GlobalDescriptorTable* gdt, void entrypoint());
        
        CPUState* GetCPUState();
        void SetCPUState(CPUState* cpustate);

        Priority GetPriority();
        void SetPriority(Priority priority);
        
        common::int32_t GetPid();
        void SetPid(common::int32_t pid);
        
        common::int32_t GetPPid();
        void SetPPid(common::int32_t ppid);
        
        State GetState();
        void SetState(State state);
        
        bool GetParentTookInWait();
        void SetParentTookInWait(bool parentTookInWait);
        
        common::int32_t GetArrivalOrder();
        void SetArrivalOrder(common::int32_t arrivalOrder);
        
    };
    
    
    class TaskManager
    {
    private:      
        SchedulerType schedulerType;
        LifeCycleType lifeCycleType;
        ProcessTablePrintType processTablePrintType;
        
        Task tasks[MAX_NUM_TASKS];
        int currentTask;
        int numTasks;
        int nextArrivalOrder;
        
        int interruptNumAfterCollatz;
        Task* collatzTask;
        Task* blockedTaskForCollatz;
        
        Task* readyQueue[MAX_NUM_TASKS];
        int queueLen;
        
        bool ignoreSchedule;
        bool useDelayInPrintingProcessTable;
        
        GlobalDescriptorTable *gdt;
        
        void PrintProcessInfo(Task* task);
        void PrintProcessTable();
        
        CPUState* RoundRobinSchedule();
        CPUState* PreemptivePrioritySchedule(); 
        
        void AddToReadyQueue(Task* task);
        void AddToReadyQueueRoundRobin(Task* task);
        void AddToReadyQueuePreemptivePriority(Task* task);
        Task* PopFromReadyQueue();
        
    public:
        TaskManager(GlobalDescriptorTable* gdt, SchedulerType schedulerType, LifeCycleType lifeCycleType, ProcessTablePrintType processTablePrintType, bool useDelayInPrintingProcessTable);
        ~TaskManager();
        Task* AddTask(Task* newTask, Priority priority, common::int32_t ppid);
        void CollatzAdded();
        CPUState* Schedule(CPUState* cpustate);
        CPUState* Schedule();
        
        void Fork(CPUState* cpustate);
        common::uint32_t Execve(void (*entrypoint)());
        common::uint32_t Waitpid(common::uint32_t pid, CPUState* cpustate);
        common::uint32_t Exit();
        common::uint32_t BlockForCollatz(CPUState* cpustate);
        void RemoveFromReadyQueue(int pid);
        
        Task* GetCurrentTask();
        
        void SetIgnoreSchedule(bool ignoreSchedule);
        
        void SetLastTaskPriority(common::uint32_t priority);
    };
    
}


#endif
