/****************************************************************************
 *
 ****************************************************************************
 *
 *        File: xc_fp.c
 *
 * Description: XC Interface to the fixed priority scheduler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "xc_private.h"

int
xc_sched_fp_domain_set(
    xc_interface *xch,
    uint32_t domid,
    struct xen_domctl_sched_fp *sdom)
{
    DECLARE_DOMCTL;

    domctl.cmd = XEN_DOMCTL_scheduler_op;
    domctl.domain = (domid_t) domid;
    domctl.u.scheduler_op.sched_id = XEN_SCHEDULER_FP;
    domctl.u.scheduler_op.cmd = XEN_DOMCTL_SCHEDOP_putinfo;
    domctl.u.scheduler_op.u.fp = *sdom;

    return do_domctl(xch, &domctl);
}

int
xc_sched_fp_domain_get(
    xc_interface *xch,
    uint32_t domid,
    struct xen_domctl_sched_fp *sdom)
{
    DECLARE_DOMCTL;
    int err;

    domctl.cmd = XEN_DOMCTL_scheduler_op;
    domctl.domain = (domid_t) domid;
    domctl.u.scheduler_op.sched_id = XEN_SCHEDULER_FP;
    domctl.u.scheduler_op.cmd = XEN_DOMCTL_SCHEDOP_getinfo;

    err = do_domctl(xch, &domctl);
    if ( err == 0 )
        *sdom = domctl.u.scheduler_op.u.fp;

    return err;
}

int
xc_sched_fp_schedule_set(
    xc_interface *xch,
    struct xen_sysctl_fp_schedule *schedule)
{
    int rc;
    DECLARE_SYSCTL;
    DECLARE_HYPERCALL_BOUNCE(
        schedule,
        sizeof(*schedule),
        XC_HYPERCALL_BUFFER_BOUNCE_IN);
    
    if ( xc_hypercall_bounce_pre(xch, schedule) )
        return -1;

    sysctl.cmd = XEN_SYSCTL_scheduler_op;
    sysctl.u.scheduler_op.cpupool_id = 0;
    sysctl.u.scheduler_op.sched_id = XEN_SCHEDULER_FP;
    sysctl.u.scheduler_op.cmd = XEN_SYSCTL_SCHEDOP_putinfo;
    set_xen_guest_handle(sysctl.u.scheduler_op.u.sched_fp.schedule, schedule);
    
    rc = do_sysctl(xch, &sysctl);
    xc_hypercall_bounce_post(xch, schedule);
    
    return rc;
}

int
xc_sched_fp_schedule_get(
    xc_interface *xch,
    struct xen_sysctl_fp_schedule *schedule)
{
    int rc;
    DECLARE_SYSCTL;
    DECLARE_HYPERCALL_BOUNCE(
        schedule,
        sizeof(*schedule),
        XC_HYPERCALL_BUFFER_BOUNCE_OUT);

    if ( xc_hypercall_bounce_pre(xch, schedule) )
        return -1;

    sysctl.cmd = XEN_SYSCTL_scheduler_op;
    sysctl.u.scheduler_op.cpupool_id = 0;
    sysctl.u.scheduler_op.sched_id = XEN_SCHEDULER_FP;
    sysctl.u.scheduler_op.cmd = XEN_SYSCTL_SCHEDOP_getinfo;
    set_xen_guest_handle(sysctl.u.scheduler_op.u.sched_fp.schedule,
            schedule);

    rc = do_sysctl(xch, &sysctl);

    xc_hypercall_bounce_post(xch, schedule);
    
    return rc;
}

int xc_sched_fp_get_wcload_on_cpu(
    xc_interface *xch, uint32_t cpu, struct xen_sysctl_fp_schedule *schedule)
{
    int rc;
    DECLARE_SYSCTL;
    DECLARE_HYPERCALL_BOUNCE(
        schedule,
        sizeof(*schedule),
        XC_HYPERCALL_BUFFER_BOUNCE_OUT);

    if ( xc_hypercall_bounce_pre(xch, schedule) ) {
        return -1;
    }

    sysctl.cmd = XEN_SYSCTL_scheduler_op;
    sysctl.u.scheduler_op.cpu_pool_id = 0;
    sysctl.u.scheduler_op.cpu = cpu;
    sysctl.u.scheduler_op.sched_id = XEN_SCHEDULER_FP;
    sysctl.u.scheduler_op.cmd = XEN_SYSCTL_SCHEDOP_getinfo;
    set_xen_guest_handle(sysctl.u.scheduler_op.u.sched_fp.schedule,
        schedule);

    rc = do_sysctl(xch, &sysctl);
    xc_hypercall_bounce_post(xch, schedule);

    return rc;
}
