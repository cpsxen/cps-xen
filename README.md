# CPS-Xen 1.0

### What is CPS-Xen?

The CPS-Xen project aims at providing a deterministic, dependable and fault tolerant platform for executing safety-critical applications for monitoring, protection or control of cyber-physical systems (CPS). CPS-Xen is based upon the Xen-Hypervisor [1] - a popular open-source Virtual Machine Monitor (VMM). CPS-Xen extends Xen by implementing established real-time scheduling algorithms and provides additional features for the execution and monitoring of the safety-critical applications.

CPS-Xen 1.0 is based upon Xen 4.1.4 version. This is due to the usability issues we experienced with Remus(a high availability extension )[2,3] and its integration into Xen. At that time, we decided to work with the XM-toolstack. However, the scheduling interface is implemented both for the XL- and XM-toolstack.

### How to install?

The Xen version inside the xen-4.1.4 directory already contains the current CPS-Xen patches. To install CPS-Xen follow the standard build and installation instructions described in xen-4.1.4/README. For additional information please refer to http://wiki.xenproject.org/wiki/Compiling_Xen_From_Source. All of the applied patches are included in the main directory and divided into core patches - reflecting the changes made to the xen hypervisor code - and tools patches - related to the adjustments in the toolstacks.

Make sure to have "sched=fp" in the grub command line.

### Features of CPS-Xen 1.0 

The current CPS-Xen version allows to switch on the fly between the following three preemptive scheduling policies:

* Fixed priority scheduling (FP)
* Rate-monotonic scheduling (RM)
* Deadline-monotonic scheduling (DM)

### How to use?

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

### Citing CPS-Xen

Please cite the CPS-Xen paper [4] in any research that uses CPS-Xen. 

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

[4] B. Jablkowski and O. Spinczyk. CPS-Xen: A virtual execution environment for cyber-physical applications. In 28th International Conference on Architecture of Computing Systems (ARCS '15), Porto, Portugal, Mar. 2015. Springer-Verlag
