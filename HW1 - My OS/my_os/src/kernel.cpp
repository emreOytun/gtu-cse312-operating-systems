
#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>

#include <drivers/amd_am79c973.h>


// #define GRAPHICSMODE


using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;

LifeCycleType lifeCycleType = LifeCycleType::LifeCycleA; // A, B1, B2, B3, B4
SchedulerType schedulerType = SchedulerType::RoundRobin; // PreemptivePriority, RoundRobin
ProcessTablePrintType processTablePrintType = ProcessTablePrintType::PrintEverySwitch; // PrintEverySwitch, PrintEveryTimeInterrupt, PrintOnlyTermination, DoNotPrint
bool useDelayInPrintingProcessTable = true;

int collatzInputs[] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
int binarySearchInputs[] = {110, 110, 110, 110, 110, 110, 110, 110, 110, 110};
int linearSearchInputs[] = {175, 110, 80, 175, 175, 175, 175, 175, 175, 175};
int longRunningProgramInputs[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 10};

int collatzNo = 0;
int binarySearchNo = 0;
int linearSearchNo = 0;
int longRunningNo = 0;

void printf(char* str)
{
    // The starting address for video memory. When we put something there, it is printed by graphic card.
    static uint16_t* VideoMemory = (uint16_t*)0xb8000; 

    // Assuming our terminal is 80*25 size, we are writing our buffer logic here.
    static uint8_t x=0,y=0;

    for(int i = 0; str[i] != '\0'; ++i)
    {
        switch(str[i])
        {
            case '\n':
                x = 0;
                y++;
                break;
            default:
                VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | str[i]; 
                x++;
                break;
        }

        if(x >= 80)
        {
            x = 0;
            y++;
        }

        if(y >= 25)
        {
            for(y = 0; y < 25; y++)
                for(x = 0; x < 80; x++)
                    VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | ' ';
            x = 0;
            y = 0;
        }
    }
}

void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}
void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}
void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}

// Converts the given integer number to string and prints it using printf
void printInteger(int num) 
{
    char str[512];
    int i = 0;
    int is_negative = 0;

    // If number is 0, print directly
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        printf(" ");
        printf(str);
        return;
    }

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    else {
        printf(" ");
    }

    while (num != 0) {
        int digit = num % 10;
        str[i++] = digit + '0';
        num /= 10;
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
    
    printf(str);
}





class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char* foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;
        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
    
};


TaskManager* taskManager;
GlobalDescriptorTable* gdtRef;

void sysprintf(char* str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

int sysfork()
{
    asm("int $0x80" : : "a" (57));
}

void sysexecve(void (*entrypoint)())
{
    asm("int $0x80" : : "a" (59), "b" (entrypoint));
}

void sysexit() 
{
    asm("int $0x80" : : "a" (8));
}

uint32_t syswaitpid(uint32_t pid) 
{
    uint32_t result;
    asm("int $0x80" : : "a" (7), "b" (pid));
    asm("" : "=a"(result));
    return result;
}

void sysblockforcollatz() 
{
    asm("int $0x80" : : "a" (9));
}

void syssetlasttaskpriority(Priority priority) 
{
    asm("int $0x80" : : "a" (10), "b" ((uint32_t) priority));
}

uint32_t sysforkpid()
{
    uint32_t forkPid;
    asm("int $0x80" : "=a"(forkPid) : "a" (58));
    return forkPid;
}

void syscollatzadded() 
{
    asm("int $0x80" : : "a" (11));
}

int sysrand() 
{
    uint64_t clockCounter;
    asm("rdtsc": "=A"(clockCounter));
    
    int num = (int) (clockCounter * 345834 + 123456) / 23415;
    if (num < 0) num *= -1;
    return num;
}

/* HOMEWORK TASKS */
void collatz() 
{
    int buf[256];
    int input = collatzInputs[collatzNo++];
    
    for (int j = input; j > 1; --j) {
        int n = j;
        int i = 0;
        while (n > 1) {
            n = (n % 2 == 0) ? n / 2 : 3 * n + 1; 
            buf[i++] = n;
        }
        
        printInteger(j);
        printf(": ");
        for (int z = 0; z < i; ++z) {
            printInteger(buf[z]);
            printf(" ");
        }
        printf("\n");
        Helper::Delay();
    }
    
    sysexit();
}

void longRunningProgram() 
{
    int n = 10;
    int result = longRunningProgramInputs[longRunningNo++];
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            result += i * j;
        }
    }
    printf("Result: ");
    printInteger(result);
    printf("\n");
    Helper::Delay();
    sysexit();
}

void binarySearch() 
{
    int arr[] = { 10, 20, 80, 30, 60, 50, 110, 100, 130, 170 };
    int len = sizeof(arr) / sizeof(int);
    int x = binarySearchInputs[binarySearchNo++];
    
    // Insertion sort
    for (int i = 1; i < len; ++i) {
        int item = arr[i];
        int j = i - 1;
        bool isDone = false;
        while (j >= 0 && !isDone) {
            if (item < arr[j]) {
                arr[j + 1] = arr[j];
                --j;
            }
            else isDone = true;
        }
        arr[j + 1] = item;
    }
    
    printf("Sorted Array to Search: ");
    for (int i = 0; i < len; ++i) {
        printInteger(arr[i]);
        printf(" ");
    }
    printf("\n");
    Helper::Delay();
    
    // Binary search
    int li = 0;
    int ri = len - 1;
    int resIdx = -1;
    while (ri >= li && resIdx == -1) {
        int mid = (li + ri) / 2;
        if (arr[mid] == x) {
            resIdx = mid;
        }
        else if (x < arr[mid]) {
            ri = mid - 1;
        }
        else {
            li = mid + 1;
        }
    }
    
    printf("Result: ");
    printInteger(resIdx);
    printf("\n");
    Helper::Delay();
    sysexit();
}

void linearSearch()
{
    int arr[] = { 10, 20, 80, 30, 60, 50, 110, 100, 130, 170 };
    int len = sizeof(arr) / sizeof(int);
    int x = linearSearchInputs[linearSearchNo++];
    
    int resIdx = -1;
    for (int i = 0; i < len && resIdx == -1; ++i) {
        if (arr[i] == x) {
            resIdx = i;
        }
    }
    
    printf("Result: ");
    printInteger(resIdx);
    printf("\n"); 
    Helper::Delay();
    sysexit();
}

void initA()
{
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(collatz);
    }
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(collatz);
    }
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(collatz);
    }
    
    
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(longRunningProgram);
    }
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(longRunningProgram);
    }
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(longRunningProgram);
    }
    
    while (syswaitpid(-1) != -1);
    printf("All programs terminated \n");
    
    while(1);
}

void initB1() 
{
    void (*entryPoints[]) (void) = {collatz, linearSearch, binarySearch, longRunningProgram};
    
    int randNumber = sysrand() % 4;
    printf("Random number: ");
    printInteger(randNumber);
    printf("\n");
    
    void (*entrypoint)() = entryPoints[randNumber];
    for (int i = 0; i < 10; ++i) {
        sysfork();
        if (sysforkpid() == 0) {
            sysexecve(entrypoint);
        }
    }
 
    while (syswaitpid(-1) != -1);
    printf("All programs terminated \n");
    
    while(1);
}

void initB2() 
{
    void (*entryPoints[]) (void) = {collatz, linearSearch, binarySearch, longRunningProgram};
    
    int randNumber1 = sysrand() % 4;
    printf("Random number1: ");
    printInteger(randNumber1);
    printf("\n");
    
    int randNumber2 = randNumber1;
    while (randNumber2 == randNumber1) {
        randNumber2 = sysrand() % 4;
    }
    printf("Random number2: ");
    printInteger(randNumber2);
    printf("\n");
    
    void (*entrypoint1)() = entryPoints[randNumber1];
    void (*entrypoint2)() = entryPoints[randNumber2];

    for (int i = 0; i < 3; ++i) {
        sysfork();
        if (sysforkpid() == 0) {
            sysexecve(entrypoint1);
        }
    }
    
    for (int i = 0; i < 3; ++i) {
        sysfork();
        if (sysforkpid() == 0) {
            sysexecve(entrypoint2);
        }
    }
    
    while (syswaitpid(-1) != -1);
    printf("All programs terminated \n");
    
    while(1);
}


void initB3()
{
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&collatz);
    }
    syssetlasttaskpriority(Priority::Low);
    syscollatzadded();
    sysblockforcollatz();
    
    // Init other tasks like that
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&longRunningProgram);
    }
    syssetlasttaskpriority(Priority::Low);
    
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&binarySearch);
    }
    syssetlasttaskpriority(Priority::Low);
    
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&linearSearch);
    }
    syssetlasttaskpriority(Priority::Low);
    
    while (syswaitpid(-1) != -1);
    printf("All programs terminated \n");
    
    while(1);
}

void initB4()
{
    taskManager->SetIgnoreSchedule(true);
    
    // Critical region: Prepare ready queue before start scheduling
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&collatz);
    }
    syssetlasttaskpriority(Priority::Low);
    syscollatzadded();
    
    
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&longRunningProgram);
    }
    syssetlasttaskpriority(Priority::Medium);
    
    
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&binarySearch);
    }
    syssetlasttaskpriority(Priority::Medium);
    
    
    sysfork();
    if (sysforkpid() == 0) {
        sysexecve(&linearSearch);
    }
    syssetlasttaskpriority(Priority::Medium);
    
    
    taskManager->SetIgnoreSchedule(false);

    while (syswaitpid(-1) != -1);
    printf("All programs terminated \n");
    
    while(1);
}

/* SYSTEM CALL TESTS */
void forkTest() 
{
    printf("Before fork \n");
    sysfork();
    printf("After fork \n");
    while (1);
}

void afterExecve() {
    printf("After execve \n");
    while(1);
}

void afterExecveParent() {
    printf("After execve PARENT\n");
    while(1);
}

void afterExecveChild() {
    printf("After execve CHILD \n");
    while(1);
}

void execveTest() {
    printf("Before execve \n");
    sysexecve(afterExecve);
    printf("THIS LINE SHOULD NOT BE PRINTED AFTER EXECVE \n");
}

void forkExecveTest() {
    printf("\n \n \n \n \n \n \n \n \n");
    
    printf("Before fork \n");
    sysfork();
    printf("After fork. \n");
    
    if (sysforkpid() == 0) {
        printf("Child \n");
        sysexecve(afterExecveChild);
    }
    
    printf("Parent \n");
    while (1);
}

void afterExecveChildExit() 
{
    printf("Child is exiting \n");
    sysexit();
}
    
void forkExecveWaitTest() {
    
    printf("Before fork\n");
    sysfork();
    printf("After fork\n");
    
    if (sysforkpid() == 0) {
        printf("Child \n");
        sysexecve(afterExecveChildExit);
    }
    
    printf("Waiting child\n");
    int res = syswaitpid(-1);
    printf("Res: ");
    printInteger(res);
    printf("\n");
    
    printf("Parent \n");
    while (1);
}

void startInitProcess(GlobalDescriptorTable* gdt) 
{
    if (lifeCycleType == LifeCycleType::LifeCycleA) 
    {
        Task task1(gdt, initA); 
        taskManager->AddTask(&task1, Priority::High, -1);
    }
    else if (lifeCycleType == LifeCycleType::LifeCycleB1) 
    {
        Task task1(gdt, initB1); 
        taskManager->AddTask(&task1, Priority::High, -1);
    }
    else if (lifeCycleType == LifeCycleType::LifeCycleB2) 
    {
        Task task1(gdt, initB2); 
        taskManager->AddTask(&task1, Priority::High, -1);
    }
    else if (lifeCycleType == LifeCycleType::LifeCycleB3) 
    {
        if (schedulerType == SchedulerType::RoundRobin) 
        {
            printf("Scheduler type MUST be PreemptivePriority for lifecycle B3 and B4 \n");
        }
        else 
        {
            Task task1(gdt, initB3); 
            taskManager->AddTask(&task1, Priority::High, -1);
        }
    }
    else if (lifeCycleType == LifeCycleType::LifeCycleB4) 
    {
        if (schedulerType == SchedulerType::RoundRobin) 
        {
            printf("Scheduler type MUST be PreemptivePriority for lifecycle B3 and B4 \n");
        }
        else 
        {
            Task task1(gdt, initB4); 
            taskManager->AddTask(&task1, Priority::High, -1);
        }
    }
}


typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
	// When we have classes which are global variables, then we have their constructor addresses between start and end ctors. We need to call them.
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}


extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("Hello World! --- OS by Emre Oytun \n\n\n\n\n\n\n");

    GlobalDescriptorTable gdt;
    gdtRef = &gdt;
    
    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
    size_t heap = 10*1024*1024;
    MemoryManager memoryManager(heap, (*memupper)*1024 - heap - 10*1024);
    
    printf("heap: 0x");
    printfHex((heap >> 24) & 0xFF);
    printfHex((heap >> 16) & 0xFF);
    printfHex((heap >> 8 ) & 0xFF);
    printfHex((heap      ) & 0xFF);
    
    void* allocated = memoryManager.malloc(1024);
    printf("\nallocated: 0x");
    printfHex(((size_t)allocated >> 24) & 0xFF);
    printfHex(((size_t)allocated >> 16) & 0xFF);
    printfHex(((size_t)allocated >> 8 ) & 0xFF);
    printfHex(((size_t)allocated      ) & 0xFF);
    printf("\n");
    
    taskManager = new TaskManager(&gdt, schedulerType, lifeCycleType, processTablePrintType, useDelayInPrintingProcessTable);
    startInitProcess(&gdt);
    
    InterruptManager interrupts(0x20, &gdt, taskManager);
    SyscallHandler syscalls(&interrupts, 0x80, taskManager);
    
    // Initialize hardware(drivers) before activating interrupt manager
    printf("Initializing Hardware, Stage 1\n");
    
    #ifdef GRAPHICSMODE
        Desktop desktop(320,200, 0x00,0x00,0xA8);
    #endif
    
    DriverManager drvManager;
    
        #ifdef GRAPHICSMODE
            KeyboardDriver keyboard(&interrupts, &desktop);
        #else
            PrintfKeyboardEventHandler kbhandler;
            KeyboardDriver keyboard(&interrupts, &kbhandler);
        #endif
        drvManager.AddDriver(&keyboard);
        
    
        #ifdef GRAPHICSMODE
            MouseDriver mouse(&interrupts, &desktop);
        #else
            MouseToConsole mousehandler;
            MouseDriver mouse(&interrupts, &mousehandler);
        #endif
        drvManager.AddDriver(&mouse);
        
        PeripheralComponentInterconnectController PCIController;
        PCIController.SelectDrivers(&drvManager, &interrupts);

        #ifdef GRAPHICSMODE
            VideoGraphicsArray vga;
        #endif
        
    printf("Initializing Hardware, Stage 2\n");
        drvManager.ActivateAll();
        
    printf("Initializing Hardware, Stage 3\n");

    #ifdef GRAPHICSMODE
        vga.SetMode(320,200,8);
        Window win1(&desktop, 10,10,20,20, 0xA8,0x00,0x00);
        desktop.AddChild(&win1);
        Window win2(&desktop, 40,15,30,30, 0x00,0xA8,0x00);
        desktop.AddChild(&win2);
    #endif


    /*
    printf("\nS-ATA primary master: ");
    AdvancedTechnologyAttachment ata0m(true, 0x1F0);
    ata0m.Identify();
    
    printf("\nS-ATA primary slave: ");
    AdvancedTechnologyAttachment ata0s(false, 0x1F0);
    ata0s.Identify();
    ata0s.Write28(0, (uint8_t*)"http://www.AlgorithMan.de", 25);
    ata0s.Flush();
    ata0s.Read28(0, 25);
    
    printf("\nS-ATA secondary master: ");
    AdvancedTechnologyAttachment ata1m(true, 0x170);
    ata1m.Identify();
    
    printf("\nS-ATA secondary slave: ");
    AdvancedTechnologyAttachment ata1s(false, 0x170);
    ata1s.Identify();
    // third: 0x1E8
    // fourth: 0x168
    */
    
    
    amd_am79c973* eth0 = (amd_am79c973*)(drvManager.drivers[2]);
    eth0->Send((uint8_t*)"Hello Network", 13);
        

    interrupts.Activate();

    while(1)
    {
        #ifdef GRAPHICSMODE
            desktop.Draw(&vga);
        #endif
    }
}
