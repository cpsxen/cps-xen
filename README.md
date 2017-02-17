# CPS-Xen 1.2

### What is CPS-Xen?

The CPS-Xen project aims at providing a deterministic, dependable and fault tolerant platform for executing safety-critical applications for monitoring, protection or control of cyber-physical systems (CPS). CPS-Xen is based upon the Xen-Hypervisor [1] - a popular open-source Virtual Machine Monitor (VMM). CPS-Xen extends Xen by implementing established real-time scheduling algorithms and provides additional features for the execution and monitoring of the safety-critical applications.

CPS-Xen 1.2 is based upon Xen 4.7.0 version. 

### How to install?

The CPS-Xen version inside the xen-4.7.0 directory already contains the current CPS-Xen patches. To install CPS-Xen follow the standard build and installation instructions described in xen-4.7.0/README. For additional information please refer to http://wiki.xenproject.org/wiki/Compiling_Xen_From_Source. All of the applied patches are included in the main directory and divided into core patches - reflecting the changes made to the xen hypervisor code - and tools patches - related to the adjustments in the toolstacks.

Make sure to have "sched=fp" in the grub command line.

### Features of CPS-Xen 1.2 

The current CPS-Xen version allows to switch on the fly between the following three preemptive scheduling policies:

* Fixed priority scheduling (FP)
* Rate-monotonic scheduling (RM)
* Deadline-monotonic scheduling (DM)
 
CPS-Xen 1.1 also introduces an explicit event-based mechanism for checkpointing of paravirtualized virtual machines (PVM). This allows a PVM itself to determine the moment in time for checkpointing rather than being dependent on a periodical event. This functionality is implemented as an extention to Remus(a high availability extension for Xen)[2,3]. 

### How to use?

####Scheduler

You can set the scheduler parameters on the fly as follows (xl or xm toolstack):

> xl [-v] sched-fp [-d <Domain> [-p[=PRIORITY]|-P[=PERIOD]|-s[=SLICE]]|-D[=DEADLINE]] [-S[=STRATEGY]]

>Options:

>- -d DOMAIN,   --domain=DOMAIN         Domain to modify
- -p PRIORITY, --priority=PRIORITY     Priority of the specified domain (int)
- -P PERIOD,   --period=PERIOD         Period (int)
- -s SLICE,    --slice=SLICE           Slice (int)
- -S STRATEGY, --strategy=STRATEGY     Strategy to be used by the scheduler (int). STRATEGY can either be 0 (rate-monotonic), 1 (deadline-monotonic) or 2 (fixed priority).
- -D DEADLINE, --deadline=DEADLINE     Deadline (int)

The priorities will be (re)calculated automatically:
* if a period respectively a deadline has been changed 
* if a strategy has been switched
* if a new VM has been instantiated

Since Linux Kernel version 3.12 a new netback model has been introduced which utilizes multiple kernel threads for packet processing. Each VM has been given a dedicated process named *vif[Domain ID]*. Through the POSIX interface (*chrt* command) in *Dom0* the scheduling priority of this process can be synchronized with the VMM-scheduler priority of the corresponding VM. This step will provide lower latencies and jitter as well as tighter response time bounds for the given VM. 

####High Availability with Explicit Checkpointing

In order to trigger a checkpoint process the PVM that is supossed to be fail-safe has to write an arbirtraty value into the Xenbus path "/local/domain/[DomID]/data/ha". After starting Remus in Dom0 CPS-Xen registers - for the given domain - a watch on this path. Whenever a value changes on this path a callback function providing the checkpoint functionality is being executed. Further, as the periodic event is dropped in this explicit event version of checkpointing there also has to be an additional way to determine the moment for a fail-safe. This is done through a new timeout which the user is suppose to set starting Remus.

The xl remus functionality has been extended with the following options:

> Usage: xl [-vf] remus [options] \<Domain\> [\<host\>]

>Options:

>- -E                      Use event-driven instead of periodic checkpointing. Needs DomU support.
- -t                      Timeout for heartbeat after which failover shall be triggered.
- -p                      When -E is activated poll for events instead of blocking.

### Building MiniOS stubdomains used to evaluate CPS-Remus

The MiniOS source code - enhanced with the suspend/resume feature via XenStore signaling - can be found in the repository mini-os here on github.com/cpsxen. In order to build it just clone the repository into the *extras* directory in the Xen source tree, change to the *stubdom* directory in the Xen source tree and type *make c-stubdom*. In *stubdom/c* you can find two applications for MiniOS both implementing a simple echo server with the only difference that one of them also triggers CPS-Remus explicit checkpointing via XenStore. Per default the latter echo server with CPS-Remus support is used as application. To use the echo server without CPS-Remus support rename *stubdo/c/main.c* as you like and rename *stubdom/c/main.c.periodic* to *main.c* and rebuild.

### Citing CPS-Xen

Please cite our CPS-Xen papers [4,5] in any research that uses CPS-Xen. 

### Conditions of Use

There is no warranty regarding the correctness of the implementation or the functionality of CPS-Xen.

### License

CPS-Xen is licensed under [GNU GPL v2](http://www.gnu.org/licenses/gpl-2.0.html).

### Project website:

http://ess.cs.tu-dortmund.de/EN/Software/CPSXen/index.html

### References:
[1] http://www.xenproject.org/ 

[2] http://remusha.wikidot.com/ 

[3] http://wiki.xen.org/wiki/Remus 

[4] B. Jablkowski and O. Spinczyk. *CPS-Xen: A virtual execution environment for cyber-physical applications.* In 28th International Conference on Architecture of Computing Systems (ARCS '15), Porto, Portugal, Mar. 2015. Springer-Verlag

[5] B. Jablkowski and O. Spinczyk. CPS-Remus: Eine Hochverfügbarkeitslösung für virtualisierte cyber-physische Anwendungen. In Betriebssysteme und Echtzeit - Echtzeit 2015. Springer-Verlag, Nov. 2015.

### Changlog
from 1.1: - adpated CPS-Remus to new migrate V2
          - more stable hearbeat mechanism
          - fixed several bugs in the scheduler
