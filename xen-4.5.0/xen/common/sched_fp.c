/* Preemptive RM/DM/Fixed-Priority Scheduler of Xen
 *
 * by Boguslaw Jablkowski, Michael Müller (C)  2014 Technische Universität Dortmund
 * based on code of Credit and SEDF Scheduler
 *
 * This scheduler allows the usage of three scheduling strategies:
 * - Rate-Monotonic 
 * - Deadline-Monotonic
 * - and Fixed Priority.
 * Furthermore it allows the dynamic switching of strategies on runtime.
 */

#include <xen/config.h>
#include <xen/init.h>
#include <xen/lib.h>
#include <xen/sched.h>
#include <xen/domain.h>
#include <xen/delay.h>
#include <xen/event.h>
#include <xen/time.h>
#include <xen/perfc.h>
#include <xen/sched-if.h>
#include <xen/softirq.h>
#include <asm/atomic.h>
#include <xen/errno.h>
#include <xen/keyhandler.h>
#include <xen/guest_access.h>

/*Verbosity level
 * 0 no information
 * 1 print function calls
 * 2 print ingoing and outgoing vcpu in do_schedule()
 * 3 print run queue
  */
#define DLEVEL 0
#define PRINT(_f, _a...)                        \
    do {                                        \
        if ( (_f) <= DLEVEL )                \
            printk(_a );                        \
    } while ( 0 )


/* Macros */
#define FPSCHED_PRIV(_ops)   \
    ((struct fpsched_private *)((_ops)->sched_data))
#define CPU_INFO(cpu)  \
    ((struct fp_cpu *)per_cpu(schedule_data, cpu).sched_priv)
#define RUNQ(cpu)      (&(CPU_INFO(cpu)->runq))
#define WAITQ(cpu)      (&(CPU_INFO(cpu)->waitq))
#define LIST(_vcpu) (&_vcpu->queue_elem)
#define FP_CPUONLINE(_pool)                                             \
    (((_pool) == NULL) ? &cpupool_free_cpus : (_pool)->cpu_valid)
#define FPSCHED_VCPU(_vcpu)  ((struct fp_vcpu *) (_vcpu)->sched_priv)
#define FPSCHED_DOM(_dom)    ((struct fp_dom *) (_dom)->sched_priv)
#define VM_SCHED_PRIO(_prio, _fpv) (FPSCHED_PRIV(ops)->strategy != FP? VM_STANDARD_PRIO((_fpv)) : (_prio))

/* Default parameters for VM, dom0 and Idle Domain */
/* Priorities */
#define VM_DOM0_PRIO 1000
#define VM_IDLE_PRIO 0

/* Slices */
#define VM_STANDARD_SLICE MICROSECS(40)
#define VM_DOM0_SLICE  MICROSECS(100)

/* Periods */
#define VM_STANDARD_PERIOD  MICROSECS(100)
#define VM_DOM0_PERIOD  MICROSECS(100)

/* Strategies */
#define RM 0                    /* rate-monotonic */
#define DM 1                    /* deadline-monotonic */
#define FP 2                    /* fixed-priority */

/*
 * Physical CPU
 */
struct fp_cpu {
    struct list_head runq;
};

/*
 * Virtual CPU
 */
struct fp_vcpu {
    struct vcpu *vcpu;
    //struct fp_dom *sdom;
    struct list_head queue_elem;
    int priority;
    bool_t awake;

    s_time_t period;   /*=(relative deadline)*/
    s_time_t slice;   /*=worst case execution time*/
    s_time_t deadline;  /*=deadline*/

    /*
     * Bookkeeping
     */
    s_time_t period_next;
    s_time_t last_time_scheduled;
    s_time_t cputime;

    unsigned long iterations;
    s_time_t max_cputime;
    s_time_t min_cputime;

    long cputime_log[100];
    int position;               /* position in run queue */
};

/*
 * Domain
 */
struct fp_dom {
    struct domain *domain;
    int priority;
    s_time_t period;
    s_time_t slice;
    s_time_t deadline;
};

/*
 * Configuration structure.
 * It consists of a compare function for vcpus and a priority handler.
 * Each strategy has its own configuration structure instance.
 */
struct fp_strategy_conf {
    int (*compare) (struct fp_vcpu *, struct fp_vcpu *);
    void (*prio_handler) (struct domain *, int);
};

/* 
 * System-wide scheduler data
 */
struct fpsched_private {
    uint8_t strategy;           /* strategy to use */
    struct fp_strategy_conf *config;
};

/* DEBUG info */
static inline void print_vcpu (struct fp_vcpu *fpv)
{
    PRINT (2, "c.d.v:%d.%d.%d, state: %d, p: %d, idle: %d, time: %ld \n",
           fpv->vcpu->processor, fpv->vcpu->domain->domain_id,
           fpv->vcpu->vcpu_id, fpv->vcpu->runstate.state, fpv->priority,
           is_idle_vcpu (fpv->vcpu), (long int)NOW ());
}

static inline void print_queue (struct list_head *const queue)
{
    struct fp_vcpu *snext;
    struct list_head *iter;

    list_for_each (iter, queue)
    {
        snext = list_entry (iter, struct fp_vcpu, queue_elem);

        print_vcpu (snext);
    }
}

/* List operations */
static inline struct fp_vcpu *__runq_elem (struct list_head *elem)
{
    return list_entry (elem, struct fp_vcpu, queue_elem);
}

static inline int __newvcpu_on_q (struct fp_vcpu *fpv)
{
    return !list_empty (&fpv->queue_elem);
}

static inline void __runq_remove (struct fp_vcpu *fpv)
{
    if (!is_idle_vcpu (fpv->vcpu))
        list_del_init (&fpv->queue_elem);
}

static inline void
__remove_from_queue (struct fp_vcpu *fpv, struct list_head *list)
{
    PRINT (1, "in remove_from_queue\n");
    if (__newvcpu_on_q (fpv))
        list_del_init (&fpv->queue_elem);
}

/* 
 * Insert a vcpu to the run queue of the given cpu using the function
 * compare as sorting criteria.
 */
static inline void
__runq_insert (unsigned int cpu, struct fp_vcpu *fpv,
               int (*compare) (struct fp_vcpu *, struct fp_vcpu *))
{
    struct list_head *const runq = RUNQ (cpu);
    struct list_head *iter;
    int i = 0;

    PRINT (1, "CPU %d in runq_insert: \n", cpu);
    if (cpu != fpv->vcpu->processor)
        return;
    if (is_idle_vcpu (fpv->vcpu))
        return;

    list_for_each (iter, runq)
    {
        struct fp_vcpu *iter_fpv = __runq_elem (iter);

        if (compare (fpv, iter_fpv))
            break;
    }
    list_add_tail (&fpv->queue_elem, iter);

    list_for_each (iter, runq)
    {
        struct fp_vcpu *iter_fpv = __runq_elem (iter);

        iter_fpv->position = i;
        i++;
    }

}

/* Compare functions for the three scheduling strategies. */
static int __runq_rm_compare (struct fp_vcpu *left, struct fp_vcpu *right)
{
    return left->period <= right->period;
}

static int __runq_dm_compare (struct fp_vcpu *left, struct fp_vcpu *right)
{
    return left->deadline <= right->deadline;
}

static int __runq_fp_compare (struct fp_vcpu *left, struct fp_vcpu *right)
{
    return left->priority >= right->priority;
}

/* 
 * Calculating the priority of a domain and vcpu is performed as function
 * of the used strategy. Each strategy has its own priority-handler that
 * describes how the priority of a given domain and its vcpus will be 
 * calculated.
 */
static void __fp_prio_handler (struct domain *dom, int priority)
{
    struct vcpu *v;
    struct fp_dom *fp_dom = FPSCHED_DOM (dom);

    fp_dom->priority = priority;

    for_each_vcpu (dom, v)
    {
        struct fp_vcpu *fpv = FPSCHED_VCPU (v);

        fpv->priority = priority;
    }
    PRINT (1, "Domain: %d Period: %d Deadline %d Priority: %d\n",
           dom->domain_id, (int)fp_dom->period, (int)fp_dom->deadline,
           fp_dom->priority);
}

static void __rm_prio_handler (struct domain *dom, int priority)
{
    int posacc = 0;
    int count = 0;
    struct vcpu *v;
    struct fp_dom *fp_dom = FPSCHED_DOM (dom);

    if (dom->domain_id == 0)
    {
        priority = VM_DOM0_PRIO;
    }
    else if (is_idle_domain (dom))
    {
        priority = VM_IDLE_PRIO;
    }
    else
    {

        for_each_vcpu (dom, v)
        {
            struct fp_vcpu *fpv = FPSCHED_VCPU (v);

            posacc += fpv->position;
            count++;
        }
        priority = VM_DOM0_PRIO - posacc / count - 1;
    }

    fp_dom->priority = priority;

    for_each_vcpu (dom, v)
    {
        struct fp_vcpu *fpv = FPSCHED_VCPU (v);

        fpv->priority = priority;
    }
    PRINT (1, "Domain: %d Period: %d Deadline %d Priority: %d\n",
           dom->domain_id, (int)fp_dom->period, (int)fp_dom->deadline,
           fp_dom->priority);
}

static void __dm_prio_handler (struct domain *dom, int priority)
{
    int posacc = 0;
    int count = 0;
    struct vcpu *v;
    struct fp_dom *fp_dom = FPSCHED_DOM (dom);

    if (dom->domain_id == 0)
    {
        priority = VM_DOM0_PRIO;
    }
    else if (is_idle_domain (dom))
    {
        priority = VM_IDLE_PRIO;
    }
    else
    {

        for_each_vcpu (dom, v)
        {
            struct fp_vcpu *fpv = FPSCHED_VCPU (v);

            posacc += fpv->position;
            count++;
        }
        priority = VM_DOM0_PRIO - posacc / count - 1;
    }

    fp_dom->priority = priority;

    for_each_vcpu (dom, v)
    {
        struct fp_vcpu *fpv = FPSCHED_VCPU (v);

        fpv->priority = priority;
    }
    PRINT (1, "Domain: %d Period: %d Deadline %d Priority: %d\n",
           dom->domain_id, (int)fp_dom->period, (int)fp_dom->deadline,
           fp_dom->priority);
}

/* 
 * Calculate the hypothetical worst-case load of the given cpu.
 * This is used as backend to provide the user with a warning when
 * deadlines may be missed due to overloading a cpu.
 */
static uint32_t fp_get_wcload_on_cpu (int cpu)
{
    struct list_head *const runq = RUNQ (cpu);
    struct list_head *iter;
    uint32_t load = 0;

    list_for_each (iter, runq)
    {
        const struct fp_vcpu *const iter_fpv = __runq_elem (iter);

        load += iter_fpv->slice * 100 / iter_fpv->period;
    }
    return load;
}

/* Reinsert a vcpu to a run queue. */
static void
fp_reinsertsort_vcpu (struct vcpu *vc,
                      int (*compare) (struct fp_vcpu *, struct fp_vcpu *))
{
    const int cpu = vc->processor;
    struct fp_vcpu *fpv = vc->sched_priv;
    struct list_head *const runq = RUNQ (cpu);

    __remove_from_queue (fpv, runq);
    __runq_insert (cpu, fpv, compare);
}

/* Get the currently used strategy. */
static int
fp_sched_get (const struct scheduler *ops,
              struct xen_sysctl_fp_schedule *schedule)
{
    struct fpsched_private *prv = FPSCHED_PRIV (ops);

    schedule->strategy = prv->strategy;

    return 0;
}

/* 
 * Set the new strategy and reschedule all domains and
 * recalculate their priorities accordingly. Also set
 * the new compare function and priority handler.
 */
static int
fp_sched_set (const struct scheduler *ops,
              struct xen_sysctl_fp_schedule *schedule)
{
    struct fpsched_private *prv = FPSCHED_PRIV (ops);

    /*
     * Check if given strategy is valid. schedule->strategy is valid if it is either
     * 0 for rate-monotonic, 1 for deadline-monotonic or 2 for fixed_priority 
     */
    if (schedule->strategy < 0 || schedule->strategy > 2)
        return -EINVAL;

    /*
     * Addopt the new strategy 
     */
    prv->strategy = schedule->strategy;
    switch (prv->strategy)
    {
    case 0:
    {
        prv->config->compare = __runq_rm_compare;
        prv->config->prio_handler = __rm_prio_handler;
        break;
    }
    case 1:
    {
        prv->config->compare = __runq_dm_compare;
        prv->config->prio_handler = __dm_prio_handler;
        break;
    }
    case 2:
    {
        prv->config->compare = __runq_fp_compare;
        prv->config->prio_handler = __fp_prio_handler;
    }
    }
    PRINT (1, "Strategy is now %d\n", prv->strategy);

    return 0;
}

/* Recalculate priorities after updating scheduler or domain parameters. */ 
static void
fp_sched_set_vm_prio (const struct scheduler *ops, struct domain *d, int prio)
{
    struct fp_dom *fpd = FPSCHED_DOM (d);
    int strategy = FPSCHED_PRIV (ops)->strategy;
    struct vcpu *v;

    if (d->domain_id == 0)
    {
        fpd->priority = VM_DOM0_PRIO;
    }
    else if (is_idle_domain (d))
    {
        fpd->priority = VM_IDLE_PRIO;
    }

    for_each_vcpu (d, v)
    {
        struct fp_vcpu *fpv = FPSCHED_VCPU (v);

        if (d->domain_id == 0 || is_idle_domain (d))
        {
            fpv->priority = fpd->priority;
        }
        if (strategy != FP)
        {
            fp_reinsertsort_vcpu (v, FPSCHED_PRIV (ops)->config->compare);
            FPSCHED_PRIV (ops)->config->prio_handler (d, prio);
        }
        else
        {
            FPSCHED_PRIV (ops)->config->prio_handler (d, prio);
            fp_reinsertsort_vcpu (v, FPSCHED_PRIV (ops)->config->compare);
        }
    }
}

static void fp_insert_vcpu (const struct scheduler *ops, struct vcpu *vc)
{
    struct fp_vcpu *fpv = vc->sched_priv;

    PRINT (1, "in fp_insert_vcpu\n");
    if (!__newvcpu_on_q (fpv) && vcpu_runnable (vc) && !vc->is_running)
    {
        __runq_insert (vc->processor, fpv, FPSCHED_PRIV (ops)->config->compare);
        if (FPSCHED_PRIV (ops)->strategy != FP)
            FPSCHED_PRIV (ops)->config->prio_handler (vc->domain, 1);
    }

}

static void *fp_alloc_domdata (const struct scheduler *ops, struct domain *d)
{
    struct fp_dom *fp_dom;

    PRINT (1, "in alloc domdata\n");

    fp_dom = xmalloc (struct fp_dom);

    if (fp_dom == NULL)
        return NULL;
    memset (fp_dom, 0, sizeof (*fp_dom));

    return (void *)fp_dom;
}

static int fp_init_domain (const struct scheduler *ops, struct domain *d)
{
    struct fp_dom *fp_dom;

    PRINT (1, "in init_domain\n");
    if (is_idle_domain (d))
        return 0;

    fp_dom = fp_alloc_domdata (ops, d);
    if (d == NULL)
        return -ENOMEM;

    d->sched_priv = fp_dom;

    fp_dom->domain = d;

    if (d->domain_id == 0)
    {
        fp_dom->priority = VM_DOM0_PRIO;
        fp_dom->slice = VM_DOM0_SLICE;
        fp_dom->period = VM_DOM0_PERIOD;
        fp_dom->deadline = VM_DOM0_PERIOD;      /* assume rate-monotonic scheduling for default */
    }
    else
    {
        fp_dom->period = VM_STANDARD_PERIOD;
        fp_dom->deadline = VM_STANDARD_PERIOD;
        fp_dom->slice = is_idle_domain (d) ? MICROSECS (0) : VM_STANDARD_SLICE;

        if (is_idle_domain (d))
        {
            fp_dom->priority = VM_IDLE_PRIO;
        }
    }

    return 0;
}

static int fp_init (struct scheduler *ops)
{
    struct fpsched_private *prv;
    struct fp_strategy_conf *conf;

    prv = xmalloc (struct fpsched_private);

    if (prv == NULL)
        return -ENOMEM;
    conf = xmalloc (struct fp_strategy_conf);
    if (conf == NULL)
        return -ENOMEM;

    memset (prv, 0, sizeof (*prv));
    memset (prv, 0, sizeof (*conf));
    ops->sched_data = prv;

    prv->strategy = 0;
    prv->config = conf;
    prv->config->compare = __runq_rm_compare;
    prv->config->prio_handler = __rm_prio_handler;

    return 0;
}

static void fp_deinit (const struct scheduler *ops)
{
    xfree (FPSCHED_PRIV (ops)->config);
    xfree (FPSCHED_PRIV (ops));
}

static void fp_free_domdata (const struct scheduler *ops, void *data)
{
    PRINT (1, "in fp_free_domdata\n");
    xfree (data);
}

static void fp_destroy_domain (const struct scheduler *ops, struct domain *d)
{
    PRINT (1, "in fp_destroy_domain\n");
    fp_free_domdata (ops, d->sched_priv);
}

static void fp_free_vdata (const struct scheduler *ops, void *priv)
{
    struct fp_vcpu *fpv = priv;

    PRINT (1, "in fp_free_vdata\n");
    xfree (fpv);
}

static void *fp_alloc_vdata (const struct scheduler *ops, struct vcpu *vc,
                             void *dd)
{
    struct fp_vcpu *fpv;
    struct fp_dom *fp_dom = FPSCHED_DOM (vc->domain);

    PRINT (1, "in alloc_vdata\n");

    fpv = xmalloc (struct fp_vcpu);

    if (fpv == NULL)
        return NULL;
    memset (fpv, 0, sizeof (*fpv));

    fpv->vcpu = vc;
    fpv->awake = 0;

    if (fp_dom != NULL)
    {
        fpv->slice = fp_dom->slice;
        fpv->period = fp_dom->period;
        fpv->priority = fp_dom->priority;
        fpv->deadline = fp_dom->deadline;
    }
    else
    {
        if (vc->domain->domain_id == 0)
        {
            fpv->priority = VM_DOM0_PRIO;
            fpv->slice = VM_DOM0_SLICE;
            fpv->period = VM_DOM0_PERIOD;
            fpv->deadline = fpv->period;
        }
        else
        {
            fpv->period = VM_STANDARD_PERIOD;
            fpv->deadline = fpv->period;
            if (is_idle_domain (vc->domain))
            {
                fpv->priority = VM_IDLE_PRIO;
            }
            fpv->slice =
                is_idle_domain (vc->domain) ? MICROSECS (0) : VM_STANDARD_SLICE;
        }
    }

    fpv->cputime = 0;
    fpv->last_time_scheduled = 0;
    fpv->period_next = NOW () + fpv->period;
    fpv->iterations = 0;

    INIT_LIST_HEAD (&fpv->queue_elem);
    return fpv;
}

static int fp_pick_cpu (const struct scheduler *ops, struct vcpu *v)
{
    cpumask_t online_affinity;
    cpumask_t *online;

    PRINT (1, "CPU %d in fp_pick_cpu\n", v->processor);

    online = cpupool_scheduler_cpumask(v->domain->cpupool);
    cpumask_and(&online_affinity, v->cpu_hard_affinity, online);
    return cpumask_cycle(v->vcpu_id % cpumask_weight(&online_affinity) - 1,
                        &online_affinity);
}

static int
fp_adjust_global (const struct scheduler *ops,
                  struct xen_sysctl_scheduler_op *sc)
{
    xen_sysctl_fp_schedule_t local_sched;
    int rc = -EINVAL;
    struct domain *d;


    switch (sc->cmd)
    {
    case XEN_SYSCTL_SCHEDOP_putinfo:
        copy_from_guest (&local_sched, sc->u.sched_fp.schedule, 1);
        rc = fp_sched_set (ops, &local_sched);
        break;
    case XEN_SYSCTL_SCHEDOP_getinfo:
        rc = fp_sched_get (ops, &local_sched);
        local_sched.load = fp_get_wcload_on_cpu (sc->cpu);
        copy_to_guest (sc->u.sched_fp.schedule, &local_sched, 1);
        break;
    }

    for_each_domain (d)
    {
        struct fp_dom *const fpd = FPSCHED_DOM (d);

        fp_sched_set_vm_prio (ops, d, fpd->priority);
    }

    return rc;
}

static void fp_free_pdata (const struct scheduler *ops, void *spc, int cpu)
{
    PRINT (1, "in fp_free_pdata\n");
    if (spc == NULL)
        return;
    xfree (spc);
}

static void *fp_alloc_pdata (const struct scheduler *ops, int cpu)
{
    struct fp_cpu *fpc;

    PRINT (1, "CPU %d in alloc_pdata\n", cpu);

    fpc = xmalloc (struct fp_cpu);

    memset (fpc, 0, sizeof (*fpc));

    INIT_LIST_HEAD (&fpc->runq);
    return fpc;
}

static void fp_sleep (const struct scheduler *ops, struct vcpu *vc)
{
    struct fp_vcpu *const fpv = FPSCHED_VCPU (vc);
    const unsigned int cpu = vc->processor;

    PRINT (1, "CPU %d in fp_sleep\n", cpu);

    if (is_idle_vcpu (vc))
        return;
    fpv->awake = 0;
    if (per_cpu (schedule_data, cpu).curr == vc)
    {
        cpu_raise_softirq (cpu, SCHEDULE_SOFTIRQ);
    }
    else if (__newvcpu_on_q (fpv))
        __runq_remove (fpv);
}

static void fp_vcpu_wake (const struct scheduler *ops, struct vcpu *vc)
{
    struct fp_vcpu *const fpv = FPSCHED_VCPU (vc);
    const unsigned int cpu = vc->processor;
    struct fp_vcpu *const cur =
        FPSCHED_VCPU (per_cpu (schedule_data, cpu).curr);

    PRINT (1, "in fp_vcpu_wake ");
    fpv->awake = 1;
    if (unlikely (per_cpu (schedule_data, vc->processor).curr == vc))
        return;
    if (unlikely (is_idle_vcpu (vc)))
        return;
    if (unlikely (__newvcpu_on_q (fpv)))
    {
        return;
    }
    if (!__newvcpu_on_q (fpv))
    {
        __runq_insert (cpu, fpv, FPSCHED_PRIV (ops)->config->compare);
        FPSCHED_PRIV (ops)->config->prio_handler (vc->domain, fpv->priority);
    }
    if (FPSCHED_PRIV (ops)->config->compare (fpv, cur))
    {
        PRINT (2, "Raising SOFTIRQ\n");
        cpu_raise_softirq (cpu, SCHEDULE_SOFTIRQ);
    }
}

static void fp_vcpu_remove (const struct scheduler *ops, struct vcpu *vc)
{
    struct fp_vcpu *const fpv = FPSCHED_VCPU (vc);
    const unsigned int cpu = vc->processor;
    struct list_head *const runq = RUNQ (cpu);

    PRINT (1, "CPU: %d in fp_vcpu_remove\n", cpu);
    __remove_from_queue (fpv, runq);
}

static int
fp_adjust (const struct scheduler *ops, struct domain *d,
           struct xen_domctl_scheduler_op *op)
{
    struct fp_dom *const fp_dom = FPSCHED_DOM (d);
    struct vcpu *v;

    PRINT (1, "in fp_adjust\n");
    if (op->cmd == XEN_DOMCTL_SCHEDOP_getinfo)
    {
        op->u.fp.priority = fp_dom->priority;
        op->u.fp.slice = fp_dom->slice;
        op->u.fp.period = fp_dom->period;
        op->u.fp.deadline = fp_dom->deadline;
    }
    else
    {
        if (op->u.fp.period > 0)
        {
            fp_dom->period = op->u.fp.period * 1000;
            if (FPSCHED_PRIV (ops)->strategy == RM)
                fp_dom->deadline = fp_dom->period;
            for_each_vcpu (d, v)
            {
                struct fp_vcpu *fpv = FPSCHED_VCPU (v);

                fpv->period = op->u.fp.period * 1000;
                if (FPSCHED_PRIV (ops)->strategy == RM)
                    fpv->deadline = fpv->period;
            }
        }
        if (op->u.fp.slice > 0)
        {
            fp_dom->slice = op->u.fp.slice * 1000;
            for_each_vcpu (d, v)
            {
                FPSCHED_VCPU (v)->slice = op->u.fp.slice * 1000;
            }
        }
        if (op->u.fp.deadline > 0)
        {
            fp_dom->deadline = op->u.fp.deadline * 1000;
            for_each_vcpu (d, v)
            {
                FPSCHED_VCPU (v)->deadline = op->u.fp.deadline * 1000;
            }
        }
        /*
         * Schedule must be updated always now. Otherwise changing period or deadline does not cause a change in priority when using
         * rate-monotonic or deadline-monotonic scheduling. 
         */
        fp_sched_set_vm_prio (ops, d,
                              op->u.fp.priority >
                              0 ? op->u.fp.priority : fp_dom->priority);

    }
    return 0;
}


static void update_queue (s_time_t now, struct list_head *runq)
{
    struct list_head *iter;

    if (!list_empty (runq))
    {
        list_for_each (iter, runq)
        {
            struct fp_vcpu *fpv = __runq_elem (iter);

            if (now > fpv->period_next)
            {
                fpv->iterations = fpv->iterations + 1;
                /*
                 * printk("core.dom.vcpu:%d.%d.%d, cputime: %ld, max_ct: %ld, last_schedule: %ld, period_next: %ld, time: %ld, period: %ld, slice: %ld\n",
                 * fpv->vcpu->processor,fpv->vcpu->domain->domain_id, fpv->vcpu->vcpu_id,
                 * fpv->cputime, fpv->max_cputime, fpv->last_time_scheduled, fpv->period_next, now, fpv->period, fpv->slice);
                 */
                fpv->cputime = 0;
                fpv->period_next = now + fpv->period;
            }
        }
    }

}


static struct task_slice
fp_do_schedule (const struct scheduler *ops, s_time_t now,
                bool_t tasklet_work_scheduled)
{
    const int cpu = smp_processor_id ();
    struct list_head *const runq = RUNQ (cpu);
    struct fp_vcpu *cur = FPSCHED_VCPU (current);
    struct fp_vcpu *snext = NULL;
    struct task_slice ret;
    struct list_head *iter;

    if (!is_idle_vcpu (current))
    {
        cur->cputime += now - cur->last_time_scheduled;
    }
    update_queue (now, runq);

    /* Get next runnable vcpu */
    if (!list_empty (runq))
    {
        list_for_each (iter, runq)
        {
            const struct fp_vcpu *const iter_fpv = __runq_elem (iter);

            if (vcpu_runnable (iter_fpv->vcpu) && iter_fpv->awake
                && (iter_fpv->cputime < iter_fpv->slice))
            {
                snext = __runq_elem (iter);
                snext->last_time_scheduled = now;
                ret.migrated = 0;
                break;
            }
        }
    }
    if (snext == NULL)
        snext = FPSCHED_VCPU (idle_vcpu[cpu]);

    if (tasklet_work_scheduled)
    {
        PRINT (1, "Tasklet work:\n");
        snext = FPSCHED_VCPU (idle_vcpu[cpu]);
    }

    ret.time = MICROSECS (1);   //MILLISECS(1);
    ret.task = snext->vcpu;
    return ret;
}

const struct fpsched_private _fpsched_private;

const struct scheduler sched_fp_def = {
    .name = "Fixed Priority Scheduler",
    .opt_name = "fp",
    .sched_id = XEN_SCHEDULER_FP,
    .sched_data = NULL,

    .init = fp_init,
    .deinit = fp_deinit,

    .init_domain = fp_init_domain,
    .destroy_domain = fp_destroy_domain,

    .insert_vcpu = fp_insert_vcpu,
    .remove_vcpu = fp_vcpu_remove,

    .alloc_vdata = fp_alloc_vdata,
    .free_vdata = fp_free_vdata,
    .alloc_pdata = fp_alloc_pdata,
    .free_pdata = fp_free_pdata,
    .alloc_domdata = fp_alloc_domdata,
    .free_domdata = fp_free_domdata,

    .do_schedule = fp_do_schedule,
    .pick_cpu = fp_pick_cpu,
    .dump_cpu_state = NULL,
    .sleep = fp_sleep,
    .wake = fp_vcpu_wake,
    .adjust = fp_adjust,
    .adjust_global = fp_adjust_global
};
