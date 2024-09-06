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
 **************************************************************
 *
 * This is a simple program to limit the cpu usage of a process
 * If you modify this code, send me a copy please
 *
 * Get the latest version at: http://github.com/opsengine/cpulimit
 *
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "process_group.h"
#include "list.h"
#include "util.h"

#ifndef EPSILON
#define EPSILON 1e-12
#endif

/* control time slot in microseconds */
/* each slot is splitted in a working slice and a sleeping slice */
/* TODO: make it adaptive, based on the actual system load */
#define TIME_SLOT 100000

/* GLOBAL VARIABLES */

/* the "family" */
struct process_group pgroup;
/* pid of cpulimit */
pid_t cpulimit_pid;
/* name of this program (maybe cpulimit...) */
char *program_name;

/* number of cpu */
int NCPU;

/* CONFIGURATION VARIABLES */

/* verbose mode */
int verbose = 0;
/* lazy mode (exits if there is no process) */
int lazy = 0;

/* quit flag for SIGINT and SIGTERM signals */
volatile sig_atomic_t quit_flag = 0;

/* SIGINT and SIGTERM signal handler */
static void sig_handler(int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        quit_flag = 1;
        break;
    default:
        break;
    }
}

static void print_usage(FILE *stream, int exit_code)
{
    fprintf(stream, "Usage: %s [OPTIONS...] TARGET\n", program_name);
    fprintf(stream, "   OPTIONS\n");
    fprintf(stream, "      -l, --limit=N          percentage of cpu allowed from 0 to %d (required)\n", 100 * NCPU);
    fprintf(stream, "      -v, --verbose          show control statistics\n");
    fprintf(stream, "      -z, --lazy             exit if there is no target process, or if it dies\n");
    fprintf(stream, "      -i, --include-children limit also the children processes\n");
    fprintf(stream, "      -h, --help             display this help and exit\n");
    fprintf(stream, "   TARGET must be exactly one of these:\n");
    fprintf(stream, "      -p, --pid=N            pid of the process (implies -z)\n");
    fprintf(stream, "      -e, --exe=FILE         name of the executable program file or path name\n");
    fprintf(stream, "      COMMAND [ARGS]         run this command and limit it (implies -z)\n");
    fprintf(stream, "\nReport bugs to <marlonx80@hotmail.com>.\n");
    exit(exit_code);
}

static double get_dynamic_time_slot(void)
{
    static double time_slot = TIME_SLOT;
    static const double MIN_TIME_SLOT = TIME_SLOT,
                        MAX_TIME_SLOT = TIME_SLOT * 5;
    double load, new_time_slot;
    if (getloadavg(&load, 1) != 1)
    {
        return time_slot;
    }
    new_time_slot = time_slot * load / NCPU / 0.3;
    new_time_slot = MIN(MAX(new_time_slot, MIN_TIME_SLOT), MAX_TIME_SLOT);
    time_slot = time_slot * 0.95 + new_time_slot * 0.05;
    return time_slot;
}

static void limit_process(pid_t pid, double limit, int include_children)
{
    /* slice of the slot in which the process is allowed to run */
    struct timespec twork;
    /* slice of the slot in which the process is stopped */
    struct timespec tsleep;
    /* generic list item */
    struct list_node *node;
    /* counter */
    int c = 0;

    /* rate at which we are keeping active the processes (range 0-1) */
    /* 1 means that the process are using all the twork slice */
    double workingrate = -1;

    /* get a better priority */
    increase_priority();

    /* build the family */
    init_process_group(&pgroup, pid, include_children);

    if (verbose)
        printf("Members in the process group owned by %ld: %d\n",
               (long)pgroup.target_pid, pgroup.proclist->count);

    while (!quit_flag)
    {
        /* total cpu actual usage (range 0-1) */
        /* 1 means that the processes are using 100% cpu */
        double pcpu = -1;

        double twork_total_nsec, tsleep_total_nsec;

        double time_slot;

        update_process_group(&pgroup);

        if (pgroup.proclist->count == 0)
        {
            if (verbose)
                printf("No more processes.\n");
            break;
        }

        /* estimate how much the controlled processes are using the cpu in the working interval */
        for (node = pgroup.proclist->first; node != NULL; node = node->next)
        {
            const struct process *proc = (const struct process *)(node->data);
            if (proc->cpu_usage < 0)
            {
                continue;
            }
            if (pcpu < 0)
                pcpu = 0;
            pcpu += proc->cpu_usage;
        }

        /* adjust work and sleep time slices */
        if (pcpu < 0)
        {
            /* it's the 1st cycle, initialize workingrate */
            pcpu = limit;
            workingrate = limit;
        }
        else
        {
            /* adjust workingrate */
            workingrate = workingrate * limit / MAX(pcpu, EPSILON);
        }
        workingrate = MAX(MIN(workingrate, 1 - EPSILON), EPSILON);

        time_slot = get_dynamic_time_slot();

        twork_total_nsec = time_slot * 1000 * workingrate;
        nsec2timespec(twork_total_nsec, &twork);

        tsleep_total_nsec = time_slot * 1000 - twork_total_nsec;
        nsec2timespec(tsleep_total_nsec, &tsleep);

        if (verbose)
        {
            if (c % 200 == 0)
                printf("\n    %%CPU    work quantum    sleep quantum    active rate\n");
            if (c % 10 == 0 && c > 0)
                printf("%7.2f%%    %9.0f us    %10.0f us    %10.2f%%\n", pcpu * 100, twork_total_nsec / 1000, tsleep_total_nsec / 1000, workingrate * 100);
        }

        /* resume processes */
        node = pgroup.proclist->first;
        while (node != NULL)
        {
            struct list_node *next_node = node->next;
            struct process *proc = (struct process *)(node->data);
            if (kill(proc->pid, SIGCONT) != 0)
            {
                /* process is dead, remove it from family */
                if (verbose)
                    fprintf(stderr, "SIGCONT failed. Process %ld dead!\n", (long)proc->pid);
                /* remove process from group */
                delete_node(pgroup.proclist, node);
                remove_process(&pgroup, proc->pid);
            }
            node = next_node;
        }

        /* now processes are free to run (same working slice for all) */
        sleep_timespec(&twork);

        if (tsleep.tv_nsec > 0 || tsleep.tv_sec > 0)
        {
            /* stop processes only if tsleep>0 */
            node = pgroup.proclist->first;
            while (node != NULL)
            {
                struct list_node *next_node = node->next;
                struct process *proc = (struct process *)(node->data);
                if (kill(proc->pid, SIGSTOP) != 0)
                {
                    /* process is dead, remove it from family */
                    if (verbose)
                        fprintf(stderr, "SIGSTOP failed. Process %ld dead!\n", (long)proc->pid);
                    /* remove process from group */
                    delete_node(pgroup.proclist, node);
                    remove_process(&pgroup, proc->pid);
                }
                node = next_node;
            }
            /* now the processes are sleeping */
            sleep_timespec(&tsleep);
        }
        c = (c + 1) % 200;
    }

    if (quit_flag)
    {
        for (node = pgroup.proclist->first; node != NULL; node = node->next)
        {
            const struct process *p = (const struct process *)(node->data);
            kill(p->pid, SIGCONT);
        }
    }

    close_process_group(&pgroup);
}

static void quit_handler(void)
{
    if (quit_flag)
    {
        /* fix ^C little problem */
        printf("\r");
    }
}

int main(int argc, char *argv[])
{
    /* argument variables */
    char *exe = NULL;
    int perclimit = 0;
    int exe_ok = 0;
    int pid_ok = 0;
    int limit_ok = 0;
    pid_t pid = 0;
    int include_children = 0;
    int command_mode;

    /* parse arguments */
    int next_option;
    int option_index = 0;
    /* A string listing valid short options letters */
    const char *short_options = "+p:e:l:vzih";
    /* An array describing valid long options */
    const struct option long_options[] = {
        {"pid", required_argument, NULL, 'p'},
        {"exe", required_argument, NULL, 'e'},
        {"limit", required_argument, NULL, 'l'},
        {"verbose", no_argument, NULL, 'v'},
        {"lazy", no_argument, NULL, 'z'},
        {"include-children", no_argument, NULL, 'i'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}};

    double limit;

    struct timespec wait_time = {2, 0};

    struct sigaction sa;

    static char program_base_name[PATH_MAX + 1];

    atexit(quit_handler);

    /* get program name */
    strncpy(program_base_name, basename(argv[0]), sizeof(program_base_name) - 1);
    program_base_name[sizeof(program_base_name) - 1] = '\0';
    program_name = program_base_name;
    /* get current pid */
    cpulimit_pid = getpid();
    /* get cpu count */
    NCPU = get_ncpu();

    do
    {
        next_option = getopt_long(argc, argv, short_options, long_options, &option_index);
        switch (next_option)
        {
        case 'p':
            pid = (pid_t)atol(optarg);
            pid_ok = 1;
            break;
        case 'e':
            exe = optarg;
            exe_ok = 1;
            break;
        case 'l':
            perclimit = atoi(optarg);
            limit_ok = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'z':
            lazy = 1;
            break;
        case 'i':
            include_children = 1;
            break;
        case 'h':
            print_usage(stdout, 1);
            break;
        case '?':
            print_usage(stderr, 1);
            break;
        case -1:
            break;
        default:
            abort();
        }
    } while (next_option != -1);

    if (pid_ok && (pid <= 1 || pid >= get_pid_max()))
    {
        fprintf(stderr, "Error: Invalid value for argument PID\n");
        print_usage(stderr, 1);
        exit(1);
    }
    if (pid != 0)
    {
        lazy = 1;
    }

    if (!limit_ok)
    {
        fprintf(stderr, "Error: You must specify a cpu limit percentage\n");
        print_usage(stderr, 1);
        exit(1);
    }
    limit = perclimit / 100.0;
    if (limit < 0 || limit > NCPU)
    {
        fprintf(stderr, "Error: limit must be in the range 0-%d00\n", NCPU);
        print_usage(stderr, 1);
        exit(1);
    }

    command_mode = optind < argc;
    if (exe_ok + pid_ok + command_mode == 0)
    {
        fprintf(stderr, "Error: You must specify one target process, either by name, pid, or command line\n");
        print_usage(stderr, 1);
        exit(1);
    }

    if (exe_ok + pid_ok + command_mode > 1)
    {
        fprintf(stderr, "Error: You must specify exactly one target process, either by name, pid, or command line\n");
        print_usage(stderr, 1);
        exit(1);
    }

    /* all arguments are ok! */
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* print the number of available cpu */
    if (verbose)
        printf("%d cpu detected\n", NCPU);

    if (command_mode)
    {
        int i;
        pid_t child;
        /* executable file */
        const char *cmd = argv[optind];
        /* command line arguments */
        char **cmd_args = (char **)malloc((argc - optind + 1) * sizeof(char *));
        if (cmd_args == NULL)
            exit(2);
        for (i = 0; i < argc - optind; i++)
        {
            cmd_args[i] = argv[i + optind];
        }
        cmd_args[i] = NULL;

        if (verbose)
        {
            printf("Running command: '%s", cmd);
            for (i = 1; i < argc - optind; i++)
            {
                printf(" %s", cmd_args[i]);
            }
            printf("'\n");
        }

        child = fork();
        if (child < 0)
        {
            exit(EXIT_FAILURE);
        }
        else if (child == 0)
        {
            /* target process code */
            int ret = execvp(cmd, cmd_args);
            /* if we are here there was an error, show it */
            perror("Error");
            free(cmd_args);
            exit(ret);
        }
        else
        {
            /* parent code */
            pid_t limiter;
            free(cmd_args);
            limiter = fork();
            if (limiter < 0)
            {
                exit(EXIT_FAILURE);
            }
            else if (limiter > 0)
            {
                /* parent */
                int status_process;
                int status_limiter;
                waitpid(child, &status_process, 0);
                waitpid(limiter, &status_limiter, 0);
                if (WIFEXITED(status_process))
                {
                    if (verbose)
                        printf("Process %ld terminated with exit status %d\n",
                               (long)child, (int)WEXITSTATUS(status_process));
                    exit(WEXITSTATUS(status_process));
                }
                printf("Process %ld terminated abnormally\n", (long)child);
                exit(status_process);
            }
            else
            {
                /* limiter code */
                if (verbose)
                    printf("Limiting process %ld\n", (long)child);
                limit_process(child, limit, include_children);
                exit(0);
            }
        }
    }

    while (!quit_flag)
    {
        /* look for the target process..or wait for it */
        pid_t ret = 0;
        if (pid_ok)
        {
            /* search by pid */
            ret = find_process_by_pid(pid);
            if (ret == 0)
            {
                printf("No process found\n");
            }
            else if (ret < 0)
            {
                printf("Process found but you aren't allowed to control it\n");
            }
        }
        else
        {
            /* search by file or path name */
            ret = find_process_by_name(exe);
            if (ret == 0)
            {
                printf("No process found\n");
            }
            else if (ret < 0)
            {
                printf("Process found but you aren't allowed to control it\n");
            }
            else
            {
                pid = ret;
            }
        }
        if (ret > 0)
        {
            if (ret == cpulimit_pid)
            {
                printf("Target process %ld is cpulimit itself! Aborting because it makes no sense\n",
                       (long)ret);
                exit(1);
            }
            printf("Process %ld found\n", (long)pid);
            /* control */
            limit_process(pid, limit, include_children);
        }
        if (lazy || quit_flag)
            break;
        /* wait for 2 seconds before next search */
        sleep_timespec(&wait_time);
    }

    return 0;
}
