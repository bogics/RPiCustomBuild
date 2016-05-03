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

int main (int argc, char *argv[])
{

	int file;
	int cmd; 

	if (argc != 3) {
		fprintf(stderr, "ioctl: wrong number of arguments\n");
		exit(1);
	}

	sscanf (argv[2], "%i", &cmd);

  	if ((file = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening file %s\n", argv[1]);
		exit(1);
  	}

	if (ioctl(file, cmd)) {
		fprintf(stderr, "Error sending the ioctl command %d to file %s\n", cmd, argv[2]);
		exit(1);
  	}
		
	close(file);
	return 0;
}
	
