/*
 * Copyright (c) 2003, 2004, 2005, 2006, 2009 Armin Wolfermann. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pftabled.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>  /* For mode constants */
#include <mqueue.h>

#define PFDEV "/dev/pf"
int pfdev = -1;

int use_syslog = 0;
int default_timeout = 0;

/*struct pftimeout {
        TAILQ_ENTRY(pftimeout)  queue;
        struct	in_addr	ip;
        uint8_t 	mask;
        time_t		timeout;
        char		table[PF_TABLE_NAME_SIZE];
};*/

static void logit(int level, const char *fmt, ...)
{
        va_list ap;
        extern char *__progname;

        va_start(ap, fmt);

        if (use_syslog) {
                vsyslog(LOG_MAKEPRI(LOG_SYSLOG, level), fmt, ap);
        } else {
                fprintf(stderr, "%s: ", __progname);
                vfprintf(stderr, fmt, ap);
                if (strchr(fmt, '\n') == NULL)
                        fprintf(stderr, "\n");
        }

        va_end(ap);
}

static void add_table(pfr_table *table)
{
	struct pfioc_table io;

	bzero(&io, sizeof io);

	io.pfrio_flags = 0;
	io.pfrio_buffer = table;
	io.pfrio_esize = sizeof(*table);
	io.pfrio_size = 1;
	if (ioctl(dev, DIOCRADDTABLES, &io)){
        	perror("ioctl");
		return (-1);
	}
	return 0;
}


static void add(char *tname, struct in_addr *ip, uint8_t mask, uint32_t _timeout, mqd_t mqd)
{
        struct pfioc_table io;
        struct pfr_table table;
        struct pfr_addr addr;
        uint32_t timeout;
        struct pftimeout *t;

        bzero(&io, sizeof io);
        bzero(&table, sizeof(table));
        bzero(&addr, sizeof(addr));

        strncpy(table.pfrt_name, tname, sizeof(table.pfrt_name));

        bcopy(ip, &addr.pfra_ip4addr, 4);
        addr.pfra_af = AF_INET;
        addr.pfra_net = mask;

        io.pfrio_table = table;
        io.pfrio_buffer = &addr;
        io.pfrio_esize = sizeof(addr);
        io.pfrio_size = 1;

        if (ioctl(pfdev, DIOCRADDADDRS, &io)){
		if(errno == ESRCH){
			if(add_table(&table) == 0)
				if(ioctl(pfdev, DIOCRADDADDRS, &io))
	        			perror("ioctl");
		}else{
        		perror("ioctl");
		}
	}

        if (default_timeout != 0 || _timeout != 0) {
                timeout = _timeout != 0 ? _timeout : default_timeout;
                if ((t = malloc(TIMEOUT_MESSAGE_SIZE)) == NULL)
                	err(1, "malloc");
                t->ip = *ip;
                t->mask = mask;
                t->timeout = time(NULL) + timeout;
                strncpy(t->table, tname, sizeof(t->table));
		if (mq_send(mqd, (const char*)t, TIMEOUT_MESSAGE_SIZE, 0) < 0) {
			perror("mq_send");
		}
		if(verbose >= 1)
			logit(LOG_NOTICE, "Included %s/%d in timeout list with timeout %d",inet_ntoa(*ip),mask, timeout);
        }
}

static void del(char *tname, struct in_addr *ip, uint8_t mask)
{
        struct pfioc_table io;
        struct pfr_table table;
        struct pfr_addr addr;

        bzero(&io, sizeof(io));
        bzero(&table, sizeof(table));
        bzero(&addr, sizeof(addr));

        strncpy(table.pfrt_name, tname, sizeof(table.pfrt_name));

        bcopy(ip, &addr.pfra_ip4addr, 4);
        addr.pfra_af = AF_INET;
        addr.pfra_net = mask;

        io.pfrio_table = table;
        io.pfrio_buffer = &addr;
        io.pfrio_esize = sizeof(addr);
        io.pfrio_size = 1;

        if (ioctl(pfdev, DIOCRDELADDRS, &io))
                err(1, "ioctl");
}

static void flush(char *tname)
{
        struct pfioc_table io;
        struct pfr_table table;

        bzero(&io, sizeof io);
        bzero(&table, sizeof(table));

        strncpy(table.pfrt_name, tname, sizeof(table.pfrt_name));

        io.pfrio_table = table;

        if (ioctl(pfdev, DIOCRCLRADDRS, &io))
                err(1, "ioctl");
}

static void usage(int code)
{
        fprintf(stderr,
                        "Usage: pftabled [options...]\n"
                        "-d          Run as daemon in the background\n"
                        "-v          Log all received packets\n"
                        "-a address  Bind to this address (default: 0.0.0.0)\n"
                        "-f table    Force requests to use this table\n"
                        "-k keyfile  Read authentication key from file\n"
                        "-p port     Bind to this port (default: 56789)\n"
                        "-t timeout  Remove IPs from table after timeout seconds\n");
        if (code)
                exit(code);
}

        int
main(int argc, char *argv[])
{
        struct sockaddr_in laddr;
        struct sockaddr_in raddr;
        socklen_t socklen = sizeof(struct sockaddr_in);
        struct passwd *pw;
        struct pftabled_msg msg;
        int ch, n, s;
        struct timeval tv;
        char *table;
        int keyfile;
	struct mq_attr mqd_att;
	mqd_t mqd;

        /* Options and their defaults */
        char *address = NULL;
        int daemonize = 0;
        char *forced = NULL;
        unsigned char key[SHA1_DIGEST_LENGTH];
        int use_key = 0;
        int port = 56789;
        int verbose = 0;

        /* Process commandline arguments */
        while ((ch = getopt(argc, argv, "a:df:k:p:t:vh")) != -1) {
                switch (ch) {
                        case 'a':
                                address = optarg;
                                break;
                        case 'd':
                                daemonize = 1;
                                break;
                        case 'f':
                                forced = optarg;
                                if (strlen(forced) >= PF_TABLE_NAME_SIZE)
                                        err(1, "table name too long");
                                break;
                        case 'k':
                                use_key = 1;
                                keyfile = open(optarg, O_RDONLY, 0);
                                if (read(keyfile, key, sizeof(key)) != sizeof(key))
                                        err(1, "unable to read authentication key");
                                close(keyfile);
                                break;
                        case 'p':
                                port = strtol(optarg, NULL, 10);
                                break;
                        case 't':
                                default_timeout = strtol(optarg, NULL, 10);
                                break;
                        case 'v':
                                verbose += 1;
                                break;
                        case 'h':
                        default:
                                usage(1);
                }
        }

        /* Prepare and bind our socket */
        bzero((char *)&laddr, sizeof(struct sockaddr_in));
        laddr.sin_family = AF_INET;
        laddr.sin_addr.s_addr = inet_addr(address ? address : "0.0.0.0");
        laddr.sin_port = htons(port);

        if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
                err(1, "socket");

        if (bind(s, (struct sockaddr *)&laddr, socklen) == -1)
                err(1, "bind");

        /* Set receive timeout on socket if using timeouts */
        if (default_timeout) {
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
                        err(1, "setsockopt");
        }

        /* Open PF device while we are root */
        pfdev = open(PFDEV, O_RDWR);
        if (pfdev == -1)
                err(1, "open " PFDEV);

        /* Daemonize if requested */
        if (daemonize) {
                printf("Daemonizing, bye ...\n");
                tzset();

                openlog("pftabled", LOG_PID|LOG_NDELAY, LOG_DAEMON);
                use_syslog = 1;

                if (daemon(0, 0) == -1)
                        err(1, "daemon");
        }

        /* Find less privileged user */
        pw = getpwnam("pftabled");
        if (!pw)
                pw = getpwnam("nobody");

        /* Chroot to /var/empty */
        if (chroot("/var/empty") == -1 || chdir("/") == -1) {
                logit(LOG_ERR, "unable to chroot to /var/empty");
                exit(1);
        }

        /* Drop privileges */
        if (pw) {
                if ((setgroups(1, &pw->pw_gid) == -1) ||
                                (setegid(pw->pw_gid) == -1) ||
                                (setgid(pw->pw_gid) == -1) ||
                                (seteuid(pw->pw_uid) == -1) ||
                                (setuid(pw->pw_uid) == -1)) {
                        logit(LOG_ERR, "unable to drop privileges");
                        exit(1);
                }
        }

	if (mq_unlink("/pftabled") != 0) {
  	   perror("mq_unlink");
        }
	
	mqd_att.mq_flags = O_NONBLOCK;
	mqd_att.mq_maxmsg = 1024;
	mqd_att.mq_msgsize = TIMEOUT_MESSAGE_SIZE;
	mqd_att.mq_curmsgs = 0;

	mqd = mq_open("/pftabled", O_CREAT | O_WRONLY, 0600, NULL);
	if(mqd < 0){
		logit(LOG_ERR, "Message queue creation failed. Returned code was %d", mqd);
	}
	mq_getattr(mqd, &mqd_att);	

        /* Check for timeouts */
        /*if (timeout) {*/
        pid_t pid = fork();
        if (pid == 0)
        {		
		TAILQ_HEAD(pftimeout_head, pftimeout) timeouts;
                TAILQ_INIT(&timeouts);

		if(mq_close(mqd)<0)
			logit(LOG_ERR, "Message queue close failed.");
		mqd = mq_open("/pftabled", O_RDONLY|O_NONBLOCK );

		for(;;){
                        time_t now = time(NULL);
        		struct pftimeout* t;
			int32_t bytes_read = 0;
			int32_t count = 0;
			//char buff[TIMEOUT_MESSAGE_SIZE];

			//if ((t = (struct pftimeout*) malloc(TIMEOUT_MESSAGE_SIZE)) == NULL)
		        //	err(1, "malloc");
			//bytes_read = mq_receive(mqd,(char*)t, TIMEOUT_MESSAGE_SIZE, NULL);
			t = calloc(1, mqd_att.mq_msgsize);
			bytes_read = mq_receive(mqd,(char*)t, mqd_att.mq_msgsize, NULL);
		        //printf("bytes read=%d, sizeof=%ld\n", bytes_read, TIMEOUT_MESSAGE_SIZE);
			//perror("mq_receive");
			//memcpy(t, buff, TIMEOUT_MESSAGE_SIZE);

			while(bytes_read >= 0){
		                TAILQ_INSERT_HEAD(&timeouts, t, queue);
				if(verbose >= 1){
			                logit(LOG_NOTICE, "Included %s/%d in timeout list with timeout %d",
						inet_ntoa(t->ip),
						t->mask,
						t->timeout
					);
				}
				//if ((t = (struct pftimeout*) malloc(TIMEOUT_MESSAGE_SIZE)) == NULL)
		        	//	err(1, "malloc");
				t = calloc(1, mqd_att.mq_msgsize);
			        bytes_read = mq_receive(mqd,(char*)t, mqd_att.mq_msgsize, NULL);
				//bytes_read = mq_receive(mqd, (char*)t, TIMEOUT_MESSAGE_SIZE, NULL);
				//memcpy(t, buff, TIMEOUT_MESSAGE_SIZE);
			}
			free(t);

			TAILQ_FOREACH(t, &timeouts, queue){
                                if (now >= t->timeout){
					del(t->table, &t->ip, t->mask);
                        	        if (verbose>=1)
                        	                logit(LOG_NOTICE, "<%s> timeout %s/%d\n",
                        	                                t->table,
							       	inet_ntoa(t->ip),
                        	                                t->mask
						);
                        	        TAILQ_REMOVE(&timeouts, t, queue);
                        	        free(t);
				}else{
					count++;
				}
			}
			if(verbose>=3)
				logit(LOG_DEBUG, "Hello from timeout daemon. Currently have %d entries in queue.", count);
                        sleep(TIMEOUT_CHECK_INTERVAL);
                }
        }else if (pid > 0){

                /* Main loop: receive packets */
                for(;;) {
                        n = recvfrom(s, &msg, sizeof(msg), 0,
                                        (struct sockaddr *)&raddr, &socklen);

                        /* Drop short packets */
                        if (n != sizeof(msg))
                                continue;

                        /* Check packet version */
                        if (msg.version > PFTABLED_MSG_VERSION) {
                                if (verbose)
                                        logit(LOG_ERR, "wrong protocol version\n");
                                continue;
                        }

                        /* Transform packets from previous versions */
                        if (msg.version < 0x02)
                                msg.mask = 32;
                        if (msg.version < 0x03)
                                msg.timeout = default_timeout;

                        /* Check timestamp */
                        if (abs(time(NULL) - ntohl(msg.timestamp)) > CLOCKDIFF) {
                                if (verbose){
                                        logit(LOG_ERR, "wrong timestamp from %s.",
                                                        inet_ntoa(raddr.sin_addr));
				}
                                continue;
                        }

                        /* Check authentication */
                        if (use_key && hmac_verify(&(key[0]), &msg, sizeof(msg) - sizeof(msg.digest), msg.digest)) {
                                if (verbose){
                                        logit(LOG_ERR, "wrong authentication from %s", inet_ntoa(raddr.sin_addr));
				}
                                continue;
                        }

                        /* Which table to use */
                        table = forced ? forced : (char *)&msg.table;

                        /* Dispatch commands */
                        switch (msg.cmd) {
                                case PFTABLED_CMD_ADD:
                                        cleanmask(&msg.addr, msg.mask);
                                        add(table, &msg.addr, msg.mask, msg.timeout, mqd);
                                        if (verbose)
						if(verbose >= 1)
                                                	logit(LOG_NOTICE, "<%s> add: %s/%d - timeout:%d\n", table,
                                                                inet_ntoa(msg.addr), msg.mask, msg.timeout);
                                        break;
                                case PFTABLED_CMD_DEL:
                                        cleanmask(&msg.addr, msg.mask);
                                        del(table, &msg.addr, msg.mask);
                                        if (verbose >= 1)
                                                logit(LOG_NOTICE, "<%s> del %s/%d\n", table,
                                                                inet_ntoa(msg.addr), msg.mask);
                                        break;
                                case PFTABLED_CMD_FLUSH:
                                        flush(table);
                                        if (verbose >= 1)
                                                logit(LOG_NOTICE, "<%s> flush\n", table);
                                        break;
                                default:
                                        logit(LOG_ERR, "received unknown command\n");
                                        break;
                        }
                }
        }else{
                printf("fork() failed!\n");
                return (1);
        }
        return (0);
}
