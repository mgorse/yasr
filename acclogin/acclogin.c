
/*
 * acclogin is an attempt at an accessible login prompt for Solaris.
 * It should be used in conjunction with yasr (Yet Another Screen Reader)
 * and a text-to-speech synthesizer.
 *
 * This program does the following:
 * - opens "/dev/console" on file descriptors 0, 1 and 2,
 * - adjusts/creates a utmp entry for "console" as a LOGIN_PROCESS.
 * - fork's and exec's yasr with possible command line options.
 * - waits for the child (yasr) propcess to terminate.
 * - adjusts the utmp entry for "console" to be a DEAD_PROCESS.
 *
 * Copyright (C) 2002 by Rich Burridge, Sun Microsystems Inc.
 *
 * acclogin comes with ABSOLUTELY NO WARRANTY.
 *
 * This is free software, placed under the terms of the
 * GNU Lesser General Public License, as published by the Free Software
 * Foundation.  Please see the file COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <utmpx.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#define FAILED      2
#define MAXARGS     20
#define SCPYN(a, b) (void) strncpy(a, b, sizeof (a))

#define CLOSE   (void) close
#define DUP     (void) dup
#define EXECVE  (void) execve

static int yasr_pid;

static char *yasr_argv[MAXARGS];

/* Command line options. */
static char *config_file = NULL;
static char *login_prog = NULL;
static char *port = NULL;
static char *synthesizer = NULL;
static char *yasr_prog = NULL;

extern char **environ;

/* debug.c prototypes */
extern void open_debug(char *);
extern void debug(char *format, ...);
extern void close_debug();


static void
set_utmp_entry(char *user, char *ttyn, int type)
{
    struct utmpx *u = (struct utmpx *) 0;
    struct utmpx utmp;
    char *ttyntail = NULL;
 
    setutxent();
    (void) memset((void *)&utmp, 0, sizeof(utmp));
 
    (void) time(&utmp.ut_tv.tv_sec);
    utmp.ut_pid = getpid();

    (void) memset(utmp.ut_host, 0, sizeof(utmp.ut_host));
 
    (void) strcpy(utmp.ut_user, "root");
 
    if (ttyn) {
        if (strstr(ttyn, "/dev/") != NULL) {
            ttyntail = strdup(ttyn + sizeof ("/dev/") - 1);
        } else {
            ttyntail = strdup(ttyn);
        }
    }
 
    SCPYN(utmp.ut_line, ttyntail);
    utmp.ut_type = type;

    while ((u = getutxent()) != NULL) {
        if ((u->ut_type == INIT_PROCESS ||
             u->ut_type == LOGIN_PROCESS ||
             u->ut_type == USER_PROCESS) &&
             ((ttyntail != NULL &&
             strncmp(u->ut_line, ttyntail, sizeof(u->ut_line)) == 0) ||
             u->ut_pid == utmp.ut_pid)) {
            if (ttyntail) {
                SCPYN(utmp.ut_line, ttyntail);
            }

            (void) pututxline(&utmp);
            break;
        }
    }
    endutxent();        /* Close utmp file */

    if (u == (struct utmpx *) NULL) {
        (void) makeutx(&utmp);
    } else {
        updwtmpx(WTMPX_FILE, &utmp);
    }

    if (ttyntail) {
        free(ttyntail);
    }
}


static void
reset_utmp_entry(int pid, int exitcode)
{
    struct utmpx *up;
    struct utmpx ut;

    setutxent();
    ut.ut_type = DEAD_PROCESS;

    while ((up = getutxent()) != NULL && (up = getutxid(&ut)) != NULL) {
        if (up->ut_pid == pid) {
            if (up->ut_type == DEAD_PROCESS) {
                /* Cleaned up elsewhere. */
                endutxent();
                return;
            }

            up->ut_type = DEAD_PROCESS;
            up->ut_exit.e_termination = exitcode & 0xff;
            up->ut_exit.e_exit = (exitcode >> 8) & 0xff;

            (void) time(&up->ut_tv.tv_sec);

            if (modutx(up) == NULL) {
                /* modutx failed so we write out the new entry ourselves. */
                (void) pututxline(up);
                updwtmpx("wtmpx", up);
            }
 
            endutxent();
            return;
        }
    }    

    endutxent();
}


static void
set_exec_args()
{
    int i = 0;

/* XXX: should really derive the location of the yasr executable from what
 *      the userr gave as --prefix at configuration time.
 */

    yasr_argv[i++] = (yasr_prog != NULL) ? yasr_prog : "/usr/local/bin/yasr";

    if (config_file != NULL) {
        yasr_argv[i++] = "-C";
        yasr_argv[i++] = config_file;
    }

    if (synthesizer != NULL) {
        yasr_argv[i++] = "-s";
        yasr_argv[i++] = synthesizer;
    }

    if (port != NULL) {
        yasr_argv[i++] = "-p";
        yasr_argv[i++] = port;
    }

    yasr_argv[i++] = (login_prog != NULL) ? login_prog : "/bin/login";
    yasr_argv[i] = NULL;
}


/*ARGSUSED*/
int
main(int argc, char *argv[])
{
    int flag = 1;
    int exit_code = FAILED;
    int status, val;

    while (flag) {
        switch (getopt(argc, argv, "C:l:s:p:y:")) {
            case 'C':                          /* Yasr configuration file. */
                config_file = strdup(optarg);
                break;
            case 'l':                          /* Alternative login program. */
                login_prog = strdup(optarg);
                break;
            case 's':                          /* Yasr synthesizer. */
                synthesizer = strdup(optarg);
                break;
            case 'p':                          /* Synthesizer port number. */
                port = strdup(optarg);
                break;
            case 'y':                          /* Alternative yasr program. */
                yasr_prog = strdup(optarg);
                break;
 
            default:
                flag = 0;
        }
    }

    CLOSE(0);
    if (open("/dev/console", O_RDWR) != 0) {
        exit(2);
    }
    CLOSE(1);
    CLOSE(2);
    DUP(0);
    DUP(0);

    set_utmp_entry("", ttyname(0), LOGIN_PROCESS);

    set_exec_args();
    if ((yasr_pid = fork()) == 0) {                      /* Child */
        set_utmp_entry("", ttyname(0), LOGIN_PROCESS);
        EXECVE(yasr_argv[0], yasr_argv, environ);
        perror("execve");
    } else if (yasr_pid != -1) {                               /* Parent. */
        do {
            val = wait(&status);
            if (val == -1) {
                if (errno == EINTR) {
                    continue;
                } else {
                    syslog(LOG_ERR, "wait: %m");   /* Shouldn't happen. */
                    break;
                }
            }
        } while (val != yasr_pid);

        if ((status & 0377) > 0) {
            exit_code = FAILED;                /* Killed by some signal. */
        }      

/* Get the second byte, this is the exit/return code. */

        exit_code = ((status >> 8) & 0377);

    } else {
        perror("fork");
        exit_code = FAILED;
    }

    reset_utmp_entry(getpid(), exit_code);
    exit(exit_code);
}
