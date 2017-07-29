/* Simple program to send ioctl commands to a device, from the command line
 * Usage: ioctl <device> <cmd>
 * Example: ioctl /dev/foo 3
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>   // incompatible implicit declaration of built-in function ‘exit’
#include <unistd.h>   // incompatible implicit declaration of built-in function ‘close’
#include <string.h>
#include "../example_ioctl.h"

int main (int argc, char *argv[])
{

	int file;
	int cmd; 

	if (argc != 3) {
		fprintf(stderr, "ioctl: wrong number of arguments\n");
		exit(1);
	}

	sscanf (argv[2], "%i", &cmd);
	printf("\ndevice to open: %s", argv[1]);
	printf("\nsend ioctl command: %d\n", cmd);
	printf("\nPress Any Key to Continue...\n");
	getchar();

  	if ((file = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening file %s\n", argv[1]);
		exit(1);
  	}

	if (strcmp(argv[2], "upper") == 0) {
		cmd = EXAMPLE_IOCTL_UPPER;
	}
	else if (strcmp(argv[2], "lower") == 0) {
		cmd = EXAMPLE_IOCTL_LOWER;
	}
	else {
		fprintf(stderr, "Invalid command %s\n", argv[2]);
		exit(1);
	}

	if (ioctl(file, cmd)) {
		fprintf(stderr, "Error sending the ioctl command %d to file %s\n", cmd, argv[1]);
		exit(1);
  	}

	close(file);
	return 0;
}
	

