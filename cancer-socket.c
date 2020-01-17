/* This file is licensed under the GPL v2 (http://www.gnu.org/licenses/gpl2.txt) (some parts was originally borrowed from proc events example)

pmon.c

code highlighted with GNU source-highlight 3.1
*/

#define _XOPEN_SOURCE 700
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//#define DISPLAY_STUFF
//#define DIFF_TWO

// configure stuff here

// MY STUFF
static int last_forked  = 0;
static int attempt_output_process_name(int pid) {
	// construct the fucking path
	char proc_cmdline_fp[21];
	strcpy(proc_cmdline_fp, "/proc/");
	sprintf(&(proc_cmdline_fp[6]), "%d", pid);
	strcat(proc_cmdline_fp, "/cmdline");

	// read the procfile /proc/<pic>/cmdline
	FILE *fptr = fopen(proc_cmdline_fp, "r");
	if (fptr == NULL) {
		//printf("Go fuck yourself %d\n", pid);
		return 1;
	} 
	char command[50];
	fread(command, sizeof(char), 50, fptr);
	printf("%s\n", command);
	fflush(stdout);
	fclose(fptr);

	return 0;
}
// END OF MY STUFF


/*
* connect to netlink
* returns netlink socket, or -1 on error
*/
static int nl_connect() {
	int rc;
	int nl_sock;
	struct sockaddr_nl sa_nl;
	
	nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (nl_sock == -1) {
	    perror("socket");
	    return -1;
	}

	sa_nl.nl_family = AF_NETLINK;
	sa_nl.nl_groups = CN_IDX_PROC;
	sa_nl.nl_pid = getpid();
	
	rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
	if (rc == -1) {
	    perror("bind");
	    close(nl_sock);
	    return -1;
	}

	return nl_sock;
}

/*
* subscribe on proc events (process notifications)
*/
static int set_proc_ev_listen(int nl_sock, bool enable) {
	int rc;
	struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
	    struct nlmsghdr nl_hdr;
	    struct __attribute__ ((__packed__)) {
	    struct cn_msg cn_msg;
	    enum proc_cn_mcast_op cn_mcast;
	    };
	} nlcn_msg;

	memset(&nlcn_msg, 0, sizeof(nlcn_msg));
	nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
	nlcn_msg.nl_hdr.nlmsg_pid = getpid();
	nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;
	
	nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
	nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
	nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);
	
	nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;
	
	rc = send(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
	if (rc == -1) {
	    perror("netlink send");
	    return -1;
	}
	
	return 0;
}

/*
* handle a single process event
*/
static volatile bool need_exit = false;
static int handle_proc_ev(int nl_sock, const int * shell_pids, int shell_pids_count) {
	int rc;
	struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
	    struct nlmsghdr nl_hdr;
	    struct __attribute__ ((__packed__)) {
	    struct cn_msg cn_msg;
	    struct proc_event proc_ev;
	    };
	} nlcn_msg;
	
	while (!need_exit) {
	    rc = recv(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
	    if (rc == 0) {
	    /* shutdown? */
	    	return 0;
	    } else if (rc == -1) {
	    	if (errno == EINTR) continue;
	    	perror("netlink recv");
	    	return -1;
	    }
	    switch (nlcn_msg.proc_ev.what) {
		    case PROC_EVENT_NONE:
		    	#ifdef DISPLAY_STUFF
		        	printf("set mcast listen ok\n");
		        #endif
		        break;
		    case PROC_EVENT_FORK:
		    	; // empty statement ot surpress compiler errors idgaf
		    	int parent_pid = nlcn_msg.proc_ev.event_data.fork.parent_pid;
		    	int found = 0;
		    	for (int i = 0; i < shell_pids_count; ++i) {
		    		if (shell_pids[i] == parent_pid) { 
		    			found = 1;
		    			break;
		    		}
		    	}
		    	if (found) {
		    		#ifdef DISPLAY_STUFF
			        	printf("fork: parent tid=%d pid=%d -> child tid=%d pid=%d\n",
			        	    nlcn_msg.proc_ev.event_data.fork.parent_pid,
			        	    nlcn_msg.proc_ev.event_data.fork.parent_tgid,
			        	    nlcn_msg.proc_ev.event_data.fork.child_pid,
			        	    nlcn_msg.proc_ev.event_data.fork.child_tgid);
		        	#endif
		        	last_forked = nlcn_msg.proc_ev.event_data.fork.child_pid;
		    	}
		        break;

		    case PROC_EVENT_EXEC:
			    #ifdef DISPLAY_STUFF
			        printf("exec: tid=%d pid=%d\n",
			            nlcn_msg.proc_ev.event_data.exec.process_pid,
			            nlcn_msg.proc_ev.event_data.exec.process_tgid);
			    #endif
			        // very nice if, that checks if difference between process ids is 2 :D :D 
			        #ifdef DIFF_TWO
				        if (last_forked + 2 == nlcn_msg.proc_ev.event_data.exec.process_pid) {
					        attempt_output_process_name(nlcn_msg.proc_ev.event_data.exec.process_pid);
				        }
			        #else
			        	attempt_output_process_name(nlcn_msg.proc_ev.event_data.exec.process_pid);
			        #endif
		        break;
		    case PROC_EVENT_UID:
		    	#ifdef DISPLAY_STUFF
			        printf("uid change: tid=%d pid=%d from %d to %d\n",
			            nlcn_msg.proc_ev.event_data.id.process_pid,
			            nlcn_msg.proc_ev.event_data.id.process_tgid,
			            nlcn_msg.proc_ev.event_data.id.r.ruid,
			            nlcn_msg.proc_ev.event_data.id.e.euid);
	            #endif
	    	    break;
	    	case PROC_EVENT_GID:
	    		#ifdef DISPLAY_STUFF
		    	    printf("gid change: tid=%d pid=%d from %d to %d\n",
		    	        nlcn_msg.proc_ev.event_data.id.process_pid,
		    	        nlcn_msg.proc_ev.event_data.id.process_tgid,
		    	        nlcn_msg.proc_ev.event_data.id.r.rgid,
		    	        nlcn_msg.proc_ev.event_data.id.e.egid);
	    	    #endif
	    	    break;
	    	case PROC_EVENT_EXIT:
	    		#ifdef DISPLAY_STUFF
		    	    printf("exit: tid=%d pid=%d exit_code=%d\n",
		    	        nlcn_msg.proc_ev.event_data.exit.process_pid,
		    	        nlcn_msg.proc_ev.event_data.exit.process_tgid,
		    	        nlcn_msg.proc_ev.event_data.exit.exit_code);
	    	    #endif
	    	    break;
	    	default:
	    		#ifdef DISPLAY_STUFF
	    	    	printf("unhandled proc event\n");
	    	    #endif
	    	    break;
    	}
	}
	
	return 0;
}

static void on_sigint(int unused) {
	need_exit = true;
}

int main(int argc, const char *argv[]) {
	int nl_sock;
	int rc = EXIT_SUCCESS;
	
	// MY STUFF
	if (argc == 1) {
		exit(EXIT_FAILURE);
	}
	// parse the arguments here m8
	int pids[argc - 1];
	for (int i = 1; i < argc; ++i) {
		pids[i - 1] = atoi(argv[i]);
	}
	// END OF MY SECTION
	
	signal(SIGINT, &on_sigint);
	siginterrupt(SIGINT, true);
	
	nl_sock = nl_connect();
	if (nl_sock == -1)
	    exit(EXIT_FAILURE);
	
	rc = set_proc_ev_listen(nl_sock, true);
	if (rc == -1) {
	    rc = EXIT_FAILURE;
	    goto out;
	}
	
	rc = handle_proc_ev(nl_sock, pids, argc - 1);
	if (rc == -1) {
	    rc = EXIT_FAILURE;
	    goto out;
	}
	
    set_proc_ev_listen(nl_sock, false);
	
	out:
	close(nl_sock);
	exit(rc);
}
