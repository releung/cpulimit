/**
 *
 * cpulimit - a CPU limiter for Linux
 *
 * Copyright (C) 2005-2012, by:  Angelo Marletta <angelo dot marletta at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Author: Simon Sigurdhsson
 *
 */

#ifdef __APPLE__

#ifndef __PROCESS_ITERATOR_APPLE_C
#define __PROCESS_ITERATOR_APPLE_C

#include <errno.h>
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "process_iterator.h"

int init_process_iterator(struct process_iterator *it, struct process_filter *filter)
{
    int bufsize;
    it->i = 0;
    /* Find out how much to allocate for it->pidlist */
    if ((bufsize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0)) <= 0)
    {
        fprintf(stderr, "proc_listpids: %s\n", strerror(errno));
        return -1;
    }
    /* Allocate and populate it->pidlist */
    if ((it->pidlist = (pid_t *)malloc(bufsize)) == NULL)
    {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        exit(-1);
    }
    if ((bufsize = proc_listpids(PROC_ALL_PIDS, 0, it->pidlist, bufsize)) <= 0)
    {
        fprintf(stderr, "proc_listpids: %s\n", strerror(errno));
        free(it->pidlist);
        return -1;
    }
    /* bufsize / sizeof(pid_t) gives the number of processes */
    it->count = bufsize / sizeof(pid_t);
    it->filter = filter;
    return 0;
}

static int pti2proc(struct proc_taskallinfo *ti, struct process *process)
{
    process->pid = ti->pbsd.pbi_pid;
    process->ppid = ti->pbsd.pbi_ppid;
    process->cputime = ti->ptinfo.pti_total_user / 1e6 + ti->ptinfo.pti_total_system / 1e6;
    if (proc_pidpath(ti->pbsd.pbi_pid, process->command, sizeof(process->command)) <= 0)
        return -1;
    return 0;
}

static int get_process_pti(pid_t pid, struct proc_taskallinfo *ti)
{
    int bytes;
    bytes = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, ti, sizeof(*ti));
    if (bytes <= 0)
    {
        if (!(errno & (EPERM | ESRCH)))
        {
            fprintf(stderr, "proc_pidinfo: %s\n", strerror(errno));
        }
        return -1;
    }
    else if (bytes < (int)sizeof(*ti))
    {
        fprintf(stderr, "proc_pidinfo: too few bytes; expected %lu, got %d\n", (unsigned long)sizeof(*ti), bytes);
        return -1;
    }
    return 0;
}

pid_t getppid_of(pid_t pid)
{
    struct proc_taskallinfo ti;
    if (get_process_pti(pid, &ti) == 0)
    {
        return ti.pbsd.pbi_ppid;
    }
    return (pid_t)(-1);
}

int is_child_of(pid_t child_pid, pid_t parent_pid)
{
    if (child_pid <= 1 || parent_pid <= 0 || child_pid == parent_pid)
        return 0;
    if (parent_pid == 1)
        return 1;
    while (child_pid > 1 && child_pid != parent_pid)
    {
        child_pid = getppid_of(child_pid);
    }
    return child_pid == parent_pid;
}

int get_next_process(struct process_iterator *it, struct process *p)
{
    if (it->i >= it->count)
        return -1;
    if (it->filter->pid != 0 && !it->filter->include_children)
    {
        struct proc_taskallinfo ti;
        if (get_process_pti(it->filter->pid, &ti) == 0 && pti2proc(&ti, p) == 0)
        {
            it->i = it->count = 1;
            return 0;
        }
        it->i = it->count = 0;
        return -1;
    }
    while (it->i < it->count)
    {
        struct proc_taskallinfo ti;
        if (get_process_pti(it->pidlist[it->i], &ti) != 0)
        {
            it->i++;
            continue;
        }
        if (ti.pbsd.pbi_flags & PROC_FLAG_SYSTEM)
        {
            it->i++;
            continue;
        }
        if (it->filter->pid != 0 && it->filter->include_children)
        {
            it->i++;
            if (pti2proc(&ti, p) != 0)
                continue;
            if (p->pid != it->filter->pid && !is_child_of(p->pid, it->filter->pid))
                continue;
            return 0;
        }
        else if (it->filter->pid == 0)
        {
            it->i++;
            if (pti2proc(&ti, p) != 0)
                continue;
            return 0;
        }
    }
    return -1;
}

int close_process_iterator(struct process_iterator *it)
{
    free(it->pidlist);
    it->pidlist = NULL;
    it->filter = NULL;
    it->count = 0;
    it->i = 0;
    return 0;
}

#endif
#endif
