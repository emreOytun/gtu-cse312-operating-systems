
#include <multitasking.h>

using namespace myos;
using namespace myos::common;

void printf(char* str);
void printfHex(uint8_t);
void printfHex32(uint32_t);
void printInteger(int num);


Task::Task() 
{ 
    ppid = -1;
    forkPid = -1;
    waitingChild = false;
    parentTookInWait = false;
}
    
Task::Task(GlobalDescriptorTable *gdt, void entrypoint())
{
    ppid = -1;
    forkPid = -1;
    waitingChild = false;
    parentTookInWait = false;
    
    Reset(gdt, entrypoint);
}

Task::Task(const Task& task)
{
    ppid = -1;
    forkPid = -1;
    waitingChild = false;
    parentTookInWait = false;
    
    this->Copy(&task);
}

Task::~Task() 
{ 
    
}

void Task::Reset(GlobalDescriptorTable* gdt, void entrypoint()) {
    // Allocate stack memory for the CPUState of this task.
    cpustate = (CPUState*)(stack + MAX_STACK_SIZE - sizeof(CPUState));
    
    // Initialize register of this task/process.
    cpustate -> eax = 0;
    cpustate -> ebx = 0;
    cpustate -> ecx = 0;
    cpustate -> edx = 0;

    cpustate -> esi = 0;
    cpustate -> edi = 0;
    cpustate -> ebp = 0;
    
    cpustate -> eip = (uint32_t)entrypoint;
    cpustate -> cs = gdt->CodeSegmentSelector();
    cpustate -> eflags = 0x202;
}

void Task::Copy(const Task* oth) 
{
    // task.stack + 4096 - task.cpustate = this->stack + 4096 - this->cpustate
    this->cpustate = (CPUState*) (this->stack - (oth->stack - (uint8_t*) oth->cpustate));
    
    for (int i = 0; i < MAX_STACK_SIZE; ++i) {
        this->stack[i] = oth->stack[i];
    }
}

void Task::CopyCpuState(CPUState* cpustate) 
{
    *(this->cpustate) = *cpustate;
}

CPUState* Task::GetCPUState() { return cpustate; }
void Task::SetCPUState(CPUState* cpustate) { this->cpustate = cpustate; }

Priority Task::GetPriority() { return priority; }
void Task::SetPriority(Priority priority) { this->priority = priority; }

common::int32_t Task::GetPid() { return pid; }
void Task::SetPid(common::int32_t pid) { this->pid = pid; }

common::int32_t Task::GetPPid() { return ppid; }
void Task::SetPPid(common::int32_t ppid) { this->ppid = ppid; }

State Task::GetState() { return this->state; }
void Task::SetState(State state) { this->state = state; }

bool Task::GetParentTookInWait() { return this->parentTookInWait; }
void Task::SetParentTookInWait(bool parentTookInWait) { this->parentTookInWait = parentTookInWait; }

common::int32_t Task::GetArrivalOrder() { return this->arrivalOrder; }
void Task::SetArrivalOrder(common::int32_t arrivalOrder) { this->arrivalOrder = arrivalOrder; }






TaskManager::TaskManager(GlobalDescriptorTable *gdt, SchedulerType schedulerType, LifeCycleType lifeCycleType, ProcessTablePrintType processTablePrintType, bool useDelayInPrintingProcessTable)
{
    numTasks = 0;
    currentTask = -1;
    nextArrivalOrder = 1;
    queueLen = 0;
    interruptNumAfterCollatz = -1;
    blockedTaskForCollatz = 0;
    ignoreSchedule = 0;
    
    this->gdt = gdt;
    this->schedulerType = schedulerType;
    this->lifeCycleType = lifeCycleType;
    this->processTablePrintType = processTablePrintType;
    this->useDelayInPrintingProcessTable = useDelayInPrintingProcessTable;
}

TaskManager::~TaskManager()
{
}

Task* TaskManager::AddTask(Task* newTask, Priority priority, common::int32_t ppid)
{
    // Check Tasks array size. Return false, if it is full.
    if(numTasks >= MAX_NUM_TASKS)
        return 0; // null
    
    Task* task = &tasks[numTasks];
    task->Copy(newTask);
    task->SetPriority(priority);
    task->SetPid(numTasks);
    task->SetPPid(ppid);
    task->SetArrivalOrder(nextArrivalOrder);
    
    AddToReadyQueue(task);
    
    ++numTasks;
    ++nextArrivalOrder;

    return task;
}

void TaskManager::CollatzAdded() 
{
    if (numTasks > 0) {
        collatzTask = &tasks[numTasks - 1];
        interruptNumAfterCollatz = 0;
    }
}

void TaskManager::SetLastTaskPriority(common::uint32_t priority) 
{
    Priority newPriority = (Priority) priority;
    if (numTasks > 0) {
        Task* task = &tasks[numTasks - 1];
        RemoveFromReadyQueue(task->GetPid());
        task->SetPriority(newPriority);
        AddToReadyQueue(task);
    }
}

common::uint32_t TaskManager::BlockForCollatz(CPUState* cpustate) 
{
    Task* runningTask = &tasks[currentTask];
    runningTask->SetCPUState(cpustate);
    runningTask->SetState(State::Blocked);
    
    blockedTaskForCollatz = runningTask;

    return (common::uint32_t) Schedule();
}

void TaskManager::Fork(CPUState* cpustate) 
{
    Task* parent = GetCurrentTask();
    
    parent->SetCPUState(cpustate); // Save the current cpustate to the currently running process which is parent
    
    Task* child = AddTask(parent, parent->GetPriority(), parent->GetPid());
    if (child != 0) {
        child->CopyCpuState(cpustate); // Copy the cpustate in any case
    }
    
    // Set the fork pids 
    parent->forkPid = child->GetPid();
    child->forkPid = 0;
    
    cpustate->ecx = child->GetPid();
    child->GetCPUState()->ecx = 0;
}

common::uint32_t TaskManager::Execve(void (*entrypoint)()) 
{
    Task* runningTask = GetCurrentTask();
    runningTask->Reset(gdt, entrypoint);
   
    return (common::uint32_t) runningTask->GetCPUState();
}

common::uint32_t TaskManager::Waitpid(common::uint32_t pid, CPUState* cpustate) 
{
    Task* runningTask = GetCurrentTask();
    runningTask->SetCPUState(cpustate);
    
    // Waiting for any child of the current process
    if (pid == -1) {
        
        // Search through processes to find if there is a child which has been terminated
        uint32_t childId = 0;
        uint32_t childNum = 0;
        bool isFound = false;
        Task* task = 0; // null
        for (int i = 0; i < numTasks && !isFound; ++i) {
            task = &tasks[i];
            if (!task->GetParentTookInWait() && task->GetPPid() == runningTask->GetPid()) {
                ++childNum;
                if (task->GetState() == State::Terminated) {
                    childId = task->GetPid();
                    isFound = true;
                }
            }
        }
        
        // A terminated child is found
        if (isFound) {
            task->SetParentTookInWait(true);
            cpustate->eax = childId;
            return (common::uint32_t) cpustate;
        }
        
        // Not found any child
        if (childNum == 0) {
            cpustate->eax = childId = -1;
            runningTask->waitingChild = false;
            return (common::uint32_t) cpustate;
        }
        
        // Not found any terminated child
        runningTask->SetState(State::Blocked);
        runningTask->waitingChild = true;
        runningTask->waitingChildId = -1;
        
        // Schedule new process since this process is blocked
        return (common::uint32_t) Schedule();
    }
    
    // Waiting for specific pid if it is actually the child of the current process
    if (pid >= numTasks) {
        cpustate->eax = -1;
        runningTask->waitingChild = false;
        return (common::uint32_t) cpustate;
    }

    Task* task = &tasks[pid];
    
    // The process with given pid is not child of this process
    if (task->GetParentTookInWait() || task->GetPPid() != runningTask->GetPid()) {
        cpustate->eax = -1;
        runningTask->waitingChild = false;
        return (common::uint32_t) cpustate;
    }
    
    // The task is the child of the process as we check above, and its state is Terminated.
    if (task->GetState() == State::Terminated) {
        task->SetParentTookInWait(true);
        cpustate->eax = pid;
        return (common::uint32_t) cpustate;
    }
    
    runningTask->SetState(State::Blocked);
    runningTask->waitingChild = true;
    runningTask->waitingChildId = pid;
    
    // Schedule new process since this process is blocked
    return (common::uint32_t) Schedule();
} 

common::uint32_t TaskManager::Exit() 
{
    Task* runningTask = GetCurrentTask();
    runningTask->SetState(State::Terminated);
    
    // First check if the runningTask has a parent (init process does not have parent)
    if (runningTask->GetPPid() != -1) {
        Task* parent = &tasks[runningTask->GetPPid()];
        
        // Check if parent is waiting for a child, and the pid is either -1 or the pid of this task
        if (parent->waitingChild && (parent->waitingChildId == -1 || parent->waitingChildId == runningTask->GetPid())) 
        {
            runningTask->SetParentTookInWait(true);
            
            parent->waitingChild = false;
            AddToReadyQueue(parent);
            
            CPUState* parentCpuState = parent->GetCPUState();
            parentCpuState->eax = runningTask->GetPid();
        }
    }
    
    // If it is collatz, make ready if there is a blocked process for collatz like init process
    if (collatzTask != 0 && runningTask->GetPid() == collatzTask->GetPid() && blockedTaskForCollatz != 0) {
        AddToReadyQueue(blockedTaskForCollatz);
        collatzTask = 0;
    }
    
    // Print process table if it is set to only in termination
    if (processTablePrintType == ProcessTablePrintType::PrintOnlyTermination) {
        PrintProcessTable();
    }
    
    return (common::uint32_t) Schedule();
}

void TaskManager::AddToReadyQueue(Task* task) 
{
    task->SetState(State::Ready);
    
    if (schedulerType == SchedulerType::RoundRobin) {
        AddToReadyQueueRoundRobin(task);
    }
    else {
        AddToReadyQueuePreemptivePriority(task);
    }
}

void TaskManager::AddToReadyQueueRoundRobin(Task* task) 
{
    // Add the element at the tail
    readyQueue[queueLen] = task;
    ++queueLen;
}

void TaskManager::AddToReadyQueuePreemptivePriority(Task* task) 
{
    // Insertion sort since it is an online sorting algorithm
    readyQueue[queueLen] = task; // Put it at the end firstly
    
    int priority = task->GetPriority();
    int arrivalOrder = task->GetArrivalOrder();
    
    bool isDone = false;
    int i = queueLen - 1;
    while (i >= 0 && !isDone) {
        int othPriority = readyQueue[i] -> GetPriority();
        int othArrivalOrder = readyQueue[i]->GetArrivalOrder();
        
        if (priority < othPriority || (priority == othPriority && arrivalOrder < othArrivalOrder)) {
            readyQueue[i + 1] = readyQueue[i];
            --i;
        }
        else isDone = true;
    }
    readyQueue[i + 1] = task;
    ++queueLen;
}

Task* TaskManager::PopFromReadyQueue() 
{
    // Remove and return head 
    Task* task = readyQueue[0]; // We know there is at least one element before calling this method already, so no need to extra check
    for (int i = 1; i < queueLen; ++i) {
        readyQueue[i - 1] = readyQueue[i];
    }
    --queueLen;
    return task;
}

// Round robin schedule
CPUState* TaskManager::RoundRobinSchedule() 
{
    // Ready process'ler arasindan bulup verilen strategy degiskenine gore next process'i bulup running yapacak
    int oldTask = currentTask;
    
    Task* task = PopFromReadyQueue();
    task->SetState(State::Running);
    currentTask = task->GetPid();
    
    // If there is a context switch, then print the process table
    if (processTablePrintType == ProcessTablePrintType::PrintEveryTimeInterrupt || (oldTask != currentTask && processTablePrintType == ProcessTablePrintType::PrintEverySwitch)) 
    {
        PrintProcessTable();
    }
    
    return tasks[currentTask].cpustate;
}

// Preemptive priority schedule
CPUState* TaskManager::PreemptivePrioritySchedule() 
{
    // Ready process'ler arasindan bulup verilen strategy degiskenine gore next process'i bulup running yapacak
    int oldTask = currentTask;
    
    Task* task = PopFromReadyQueue();
    task->SetState(State::Running);
    currentTask = task->GetPid();
    
    // If there is a context switch, then print the process table
    if (processTablePrintType == ProcessTablePrintType::PrintEveryTimeInterrupt || (oldTask != currentTask && processTablePrintType == ProcessTablePrintType::PrintEverySwitch)) 
    {
        PrintProcessTable();
    }
    
    return tasks[currentTask].cpustate;
}

// Schedules the next process according to the scheduler type
CPUState* TaskManager::Schedule()
{
    if (schedulerType == SchedulerType::RoundRobin) {
        return RoundRobinSchedule();
    }
    return PreemptivePrioritySchedule();
}

// Finds the process with given pid if any, and removes it from the queue
void TaskManager::RemoveFromReadyQueue(int pid) 
{
    int idx = -1;
    for (int i = 0; i < queueLen && idx == -1; ++i) {
        if (readyQueue[i]->GetPid() == pid) {
            idx = i;
        }
    }
    if (idx != -1) {
        for (int i = idx + 1; i < queueLen; ++i) {
            readyQueue[i - 1] = readyQueue[i];
        }
        --queueLen;
    }
}

CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    if(numTasks <= 0)
        return cpustate;
    
    // In strategy 4, the init task sets the scheduler to ignore schedule so that all processes are added to queue before scheduling
    if (lifeCycleType == LifeCycleType::LifeCycleB4 && ignoreSchedule) {
        return cpustate;
    }
    
    // If collatz task added, then increment the "interruptNumAfterCollatz" counter
    if (interruptNumAfterCollatz != -1) ++interruptNumAfterCollatz;
    
    // In strategy 4, the collatz's priority is set to High after 5th interrupt
    if (lifeCycleType == LifeCycleType::LifeCycleB4 && interruptNumAfterCollatz == 5) {
        printf("\nBEFORE 5th INTERRUPT (SEE TABLE BELOW) : \n");
        --interruptNumAfterCollatz;
        PrintProcessTable();   
        Helper::Delay();
        
        RemoveFromReadyQueue(collatzTask->GetPid());
        collatzTask->SetPriority(Priority::High);
        AddToReadyQueue(collatzTask);
        
        ++interruptNumAfterCollatz;
        PrintProcessTable();
        Helper::Delay();
    }
    
    // If 5th interrupt after collatz tasks started, then unblock the init process which is blocked after adding collatz task to ready queue.
    if (lifeCycleType == LifeCycleType::LifeCycleB3 && interruptNumAfterCollatz != -1) {
        if (interruptNumAfterCollatz == 5 && blockedTaskForCollatz != 0) {
            printf("\nBEFORE 5th INTERRUPT (SEE TABLE BELOW) : \n");
            --interruptNumAfterCollatz;
            PrintProcessTable();
            Helper::Delay();
            
            // Start again the blocked init process
            AddToReadyQueue(blockedTaskForCollatz);
            blockedTaskForCollatz = 0; // null
            
            ++interruptNumAfterCollatz;
        }
    }
    
    if(currentTask >= 0) {
        tasks[currentTask].cpustate = cpustate;
        AddToReadyQueue(&tasks[currentTask]);
    }
    
    return Schedule();
}

Task* TaskManager::GetCurrentTask() 
{
    return &tasks[currentTask];
}


void TaskManager::SetIgnoreSchedule(bool ignoreSchedule) 
{
    this->ignoreSchedule = ignoreSchedule;
}

void TaskManager::PrintProcessInfo(Task* task) 
{
    printInteger(task->GetPid());
    printf("    ");

    printInteger(task->GetPPid());
    printf("   ");

    switch (task->GetState()) {
        case Ready:
            printf("Ready         ");
            break;
            
        case Running:
            printf("Running       ");
            break;
            
        case Blocked:
            printf("Blocked       ");
            break;
            
        case Terminated:
            printf("Terminated    ");
            break;
    }

    printInteger(task->GetPriority());
    
    printf("  ");
    printInteger(task->GetArrivalOrder());
    
    printf("\n");
}

void TaskManager::PrintProcessTable() 
{
    printf("********************************** \n");
    printf("PID PPID   State   Priority   Arrival Order \n");
    
    for (int i = 0; i < numTasks; ++i) {
        PrintProcessInfo(&tasks[i]);
    }

    printf("Ready queue PIDs: ");
    for (int i = 0; i < queueLen; ++i) {
        printInteger(readyQueue[i]->GetPid());
        printf(" ");
    }
    printf("\n");
    
    printf("Interrupt number after collatz: ");
    printInteger(interruptNumAfterCollatz);
    printf("\n");
    
    printf("********************************** \n");
    
    /* REMOVE THIS DELAY IF YOU WANT TO SEE THE WHOLE RESULT IMMEDIATELY */
    // Wait a little to see the result in the screen
    if (useDelayInPrintingProcessTable) {
        Helper::Delay();
    }
    
}    
