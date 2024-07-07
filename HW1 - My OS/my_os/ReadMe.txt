PARAMETERS AND INPUTS:


In kernel.cpp, you’ll find parameters at the top of the code.
You should set this parameters according to the scheduling strategy and lifecycle that you want.

LifeCycleType:

LifeCycleType::LifeCycleA: For Part-A
LifeCycleType::LifeCycleB1: For Part-B lifecycle 1
LifeCycleType::LifeCycleB2: For Part-B lifecycle 2
LifeCycleType::LifeCycleB3: For Part-B lifecycle 3
LifeCycleType::LifeCycleB4: For Part-B lifecycle 4

SchedulerType:

SchedulerType::RoundRobin
SchedulerType::PreemptivePriority

ProcessTablePrintType: You can set here to see results well. 

ProcessTablePrintType::PrintEverySwitch: Prints process table in every context switch 
ProcessTablePrintType::PrintEveryTimeInterrupt: Prints process table in every time interrupts
ProcessTablePrintType::PrintOnlyTermination: Prints process table in every process termination
ProcessTablePrintType::DoNotPrint: Does not print process table at all so that you can examine the process results well

useDelayInPrintingProcessTable: You can set this to true if you want to see delay so that you can examine the results and printed values in the screen well. If you want to see the results immediately, set it to false.

NOTE: If you directly want to see the results or if the delay is too much for your machine so that the processes cannot continue then set it to false. You can take record from virtual box to examine the results at least in this case.

NOTE: You should not run RoundRobin with lifecycles B3 and B4 since these lifecycles are designed to work only with PreemptivePriority scheduling.



TASK INPUTS:

Each process can run at max 10 times in a lifecycle. So, you can set the inputs here accordingly by setting the inputs array.



HOW TO COMPILE AND RUN:

You can “make” to compile the project as a whole. You should add the iso to to the virtual box as Engellman describes and run it.
!!!!!!!!! BEFORE THAT SET THE PARAMETERS PROPERLY ACCORDING TO THE SCHEDULING STRATEGY AND LIFECYCLE YOU WANT !!!!!!!!!!!


NOTE: I used the record feature of VirtualBox to take the screenshots and examine the results properly. You can use the same way to examine the results if the results are printed into the screen too quickly or too slowly. If too slow, then set useDelayInPrintingProcessTable to false.
