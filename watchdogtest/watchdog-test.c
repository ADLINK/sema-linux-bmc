// Software License Agreement (BSD License)
//
// Copyright (c) 2022, ADLINK Technology, Inc
// All rights reserved.
//
// Redistribution and use of this software in source and binary forms,
// with or without modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Neither the name of ADLINK Technology nor the names of its contributors may be used
//   to endorse or promote products derived from this software without specific
//   prior written permission of ADLINK Technology, Inc.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/watchdog.h>

#define DEFAULT_PING_RATE	1

int fd;
const char v = 'V';
static int stop = 0;
static const char sopts[] = "bdehp:t:Tn:NL:S";
static const struct option lopts[] = {
	{"bootstatus",          no_argument, NULL, 'b'},
	{"disable",             no_argument, NULL, 'd'},
	{"enable",              no_argument, NULL, 'e'},
	{"help",                no_argument, NULL, 'h'},
	{"pingrate",      required_argument, NULL, 'p'},
	{"ping",          	no_argument, NULL, 'P'},
	{"timeout",       required_argument, NULL, 't'},
	{"gettimeout",          no_argument, NULL, 'T'},
	{"pretimeout",    required_argument, NULL, 'n'},
	{"getpretimeout",       no_argument, NULL, 'N'},
	{"gettimeleft",		no_argument, NULL, 'L'},
	{"stop",		no_argument, NULL, 'S'},
	{NULL,                  no_argument, NULL, 0x0}
};

/*
 * This function simply sends an IOCTL to the driver, which in turn ticks
 * the PC Watchdog card to reset its internal timer so it doesn't trigger
 * a computer reset.
 */
static void keep_alive(void)
{
	int dummy;
	int ret;

	ret = ioctl(fd, WDIOC_KEEPALIVE, &dummy);
	if (!ret)
		printf(".");
}

/*
 * The main program.  Run the program with "-d" to disable the card,
 * or "-e" to enable the card.
 */

static void term(int sig)
{
	int ret = 0;

 	if(stop == 0)
		ret = write(fd, &v, 1);

	close(fd);
	if (ret < 0)
		printf("\nStopping watchdog ticks failed (%d)...\n", errno);
	else
		printf("\nStopping watchdog ticks...\n");
	exit(0);
}

static void usage(char *progname)
{
	printf("Usage: %s [options]\n", progname);
	printf(" -b, --bootstatus    Get last boot status (Watchdog/POR)\n");
	printf(" -d, --disable       Turn off the watchdog timer\n");
	printf(" -e, --enable        Turn on the watchdog timer\n");
	printf(" -h, --help          Print the help message\n");
	printf(" -P, --ping    	     Pings with rate of P seconds (default %d)\n", DEFAULT_PING_RATE);
	printf(" -p, --pingrate=P    Set ping rate to P seconds (default %d)\n", DEFAULT_PING_RATE);
	printf(" -t, --timeout=T     Set timeout to T seconds\n");
	printf(" -T, --gettimeout    Get the timeout\n");
	printf(" -n, --pretimeout=T  Set the pretimeout to T seconds\n");
	printf(" -N, --getpretimeout Get the pretimeout\n");
	printf(" -L, --gettimeleft   Get the time left until timer expires\n");
	printf("\n");
	printf("Parameters are parsed left-to-right in real-time.\n");
	printf("Example: %s -d -t 10 -p 5 -e\n", progname);
	printf("Example: %s -t 12 -T -n 7 -N\n", progname);
}

int main(int argc, char *argv[])
{
	int flags, tout, fdd;
	unsigned int ping_rate = DEFAULT_PING_RATE;
	int ret;
	int c;
	int oneshot = 0;
	int ping = 0;

	char sysfile[256];	
	char value[256] = { 0 };
	setbuf(stdout, NULL);

	fd = open("/dev/watchdog", O_WRONLY);

	if (fd == -1) {
		if (errno == ENOENT)
			printf("Watchdog device not enabled.\n");
		else if (errno == EACCES)
			printf("Run watchdog as root.\n");
		else
			printf("Watchdog device open failed %s\n",
				strerror(errno));
		exit(-1);
	}

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'b':
			flags = 0;
			oneshot = 1;
			ret = ioctl(fd, WDIOC_GETBOOTSTATUS, &flags);
			if (!ret)
				printf("Last boot is caused by: %s.\n", (flags != 0) ?
					"Watchdog" : "Power-On-Reset");
			else
				printf("WDIOC_GETBOOTSTATUS error '%s'\n", strerror(errno));
			break;
		case 'd':
			flags = WDIOS_DISABLECARD;
			ret = ioctl(fd, WDIOC_SETOPTIONS, &flags);
			if (!ret)
				printf("Watchdog card disabled.\n");
			else {
				printf("WDIOS_DISABLECARD error '%s'\n", strerror(errno));
				oneshot = 1;
			}
			break;
		case 'e':
			flags = WDIOS_ENABLECARD;
			ret = ioctl(fd, WDIOC_SETOPTIONS, &flags);
			if (!ret)
				printf("Watchdog card enabled.\n");
			else {
				printf("WDIOS_ENABLECARD error '%s'\n", strerror(errno));
				oneshot = 1;
			}
			break;
		case 'p':
			ping_rate = strtoul(optarg, NULL, 0);
			if (!ping_rate)
				ping_rate = DEFAULT_PING_RATE;
			printf("Watchdog ping rate set to %u seconds.\n", ping_rate);
			break;
				
		case 't':
			errno = 0;
			flags = strtoul(optarg, NULL, 0);
			ret = ioctl(fd, WDIOC_SETTIMEOUT, &flags);
			if (!ret)
				printf("Watchdog timeout set to %d seconds.\n", flags);
			else {
				printf("WDIOC_SETTIMEOUT error '%s'\n", strerror(errno));
				oneshot = 1;
			}
			break;
		case 'T':
			oneshot = 1;
			ret = ioctl(fd, WDIOC_GETTIMEOUT, &flags);
			if (!ret)
				printf("WDIOC_GETTIMEOUT returns %d seconds.\n", flags);
			else
				printf("WDIOC_GETTIMEOUT error '%s'\n", strerror(errno));
			break;
		case 'n':
			flags = strtoul(optarg, NULL, 0);
			ret = ioctl(fd, WDIOC_SETPRETIMEOUT, &flags);
			if (!ret)
				printf("Watchdog pretimeout set to %d seconds.\n", flags);
			else {
				printf("WDIOC_SETPRETIMEOUT error '%s'\n", strerror(errno));
				oneshot = 1;
			}
			break;
		case 'N':
			oneshot = 1;
			ret = ioctl(fd, WDIOC_GETPRETIMEOUT, &flags);
			if (!ret)
				printf("WDIOC_GETPRETIMEOUT returns %d seconds.\n", flags);
			else
				printf("WDIOC_GETPRETIMEOUT error '%s'\n", strerror(errno));
			break;
		case 'L':
			oneshot = 1;
			ret = ioctl(fd, WDIOC_GETTIMELEFT, &flags);
			if (!ret)
				printf("WDIOC_GETTIMELEFT returns %d seconds.\n", flags);
			else
				printf("WDIOC_GETTIMELEFT error '%s'\n", strerror(errno));
			break;
		case 'S':
			stop = 1;
			break;
		case 'P':

			sprintf(sysfile, "/sys/class/watchdog/watchdog0/timeout");

			fdd = open(sysfile, O_RDONLY);
			if(fd < 0)
				return -1;

			ret = read(fdd, value, sizeof(value));
			if (ret)
				close(fdd);
			else {
				close(fdd);
				return -1;
			}
			
			tout = atoi(value);

			ret = ioctl(fd, WDIOC_SETTIMEOUT, &tout);
			if (!ret)
				printf("Watchdog timeout set to %d seconds.\n", tout);
			else {
				printf("WDIOC_SETTIMEOUT error '%s'\n", strerror(errno));
				oneshot = 1;
			}
			break;
		default:
			usage(argv[0]);
			goto end;
		}
	}

	if (oneshot)
		goto end;

	printf("Watchdog Ticking Away!\n");

	signal(SIGINT, term);

	while (ping) {
		keep_alive();
		sleep(ping_rate);
	}
end:
	if(stop == 1) {
		ret = write(fd, &v, 1);
		if (ret < 0)
			printf("Stopping watchdog ticks failed (%d)...\n", errno);
	}		
	close(fd);
	return 0;
}
