#################################
CPS-Xen 1.0
#################################

What is CPS-Xen?
================

The CPS-Xen project aims at providing a deterministic, dependable and fault tolerant platform for executing safety-critical applications for monitoring, protection or control of cyber-physical systems (CPS). CPS-Xen is based upon the Xen-Hypervisor[1] - a popular open-source Virtual Machine Monitor (VMM). CPS-Xen extends Xen by implementing established real-time scheduling algorithms and provides additional features for the execution and monitoring of the safety-critical applications.

CPS-Xen 1.0 is based upon Xen 4.1.4 version. This is due to the usability issues we experienced with Remus(a high availability extension)[2,3] and its integration into Xen. At that time, we decided to work with the XM-toolstack. However, the scheduling interface is implemented both for the XL- and XM-toolstack.

How to install?
===============
The Xen version inside the xen-4.1.4 directory already contains the current CPS-Xen patches. To install CPS-Xen follow the standard build and installation instructions described in xen-4.1.4/README. For additional information please refer to http://wiki.xenproject.org/wiki/Compiling_Xen_From_Source. All of the applied patches are included in the main directory and divided into core patches - reflecting the changes made to the xen hypervisor code - and tools patches - related to the adjustments in the toolstacks.

Features of CPS-Xen 1.0 
=======================
The current CPS-Xen version allows to switch on the fly between the following three preemptive scheduling policies:

1) Fixed priority scheduling (FP)
2) Rate-monotonic scheduling (RM)
3) Deadline-monotonic scheduling (DM)

How to use?
===========
TODO

Conditions of Use
=================
There is no warranty regarding the correctness of the implementation or the functionality of CPS-Xen.

License
=======
CPS-Xen is licensed under GNU GPL v2.

Project website:
================
http://ess.cs.tu-dortmund.de/EN/Software/CPSXen/index.html

References:
===========
[1] http://www.xenproject.org/
[2] http://remusha.wikidot.com/
[3] http://wiki.xen.org/wiki/Remus
