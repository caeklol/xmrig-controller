#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>

// https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
int set_interface_attribs(int fd, int speed, int parity) {
	struct termios tty;
   	if (tcgetattr (fd, &tty) != 0) {
   	    printf("error %d from tcgetattr", errno);
   	    return -1;
   	}

   	cfsetospeed (&tty, speed);
   	cfsetispeed (&tty, speed);

   	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
   	// disable IGNBRK for mismatched speed tests; otherwise receive break
   	// as \000 chars
   	tty.c_iflag &= ~IGNBRK;         // disable break processing
   	tty.c_lflag = 0;                // no signaling chars, no echo,
   	                                // no canonical processing
   	tty.c_oflag = 0;                // no remapping, no delays
   	tty.c_cc[VMIN]  = 0;            // read doesn't block
   	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

   	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

   	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
   	                                // enable reading
   	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
   	tty.c_cflag |= parity;
   	tty.c_cflag &= ~CSTOPB;
   	tty.c_cflag &= ~CRTSCTS;

   	if (tcsetattr (fd, TCSANOW, &tty) != 0) {
   	    printf("error %d from tcsetattr", errno);
   	    return -2;
   	}
   	return 0;
}

int set_blocking(int fd, int should_block) {
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		printf("error %d from tggetattr", errno);
		return -1;
	}

	tty.c_cc[VMIN]  = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	if (tcsetattr (fd, TCSANOW, &tty) != 0) {
		printf("error %d setting term attributes", errno);
		return -2;
	}

	return 0;
}

int connect_to_serial(char* port) {
	int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) return -1;
	if (set_interface_attribs(fd, B9600, 0) < 0) return -1;
	if (set_blocking(fd, 0) < 0) return -1;
	return fd;
}

int start_xmrig(sd_bus** bus, sd_bus_error* error) {
	sd_bus_message *m = NULL;
	return sd_bus_call_method(*bus,
		"org.freedesktop.systemd1",        
		"/org/freedesktop/systemd1",       
		"org.freedesktop.systemd1.Manager",
		"StartUnit",                       
		error,                             
		&m,                                
		"ss",                              
		"xmrig.service",                   
		"replace");
}

int stop_xmrig(sd_bus** bus, sd_bus_error* error) {
	sd_bus_message *m = NULL;
	return sd_bus_call_method(*bus,
		"org.freedesktop.systemd1",           
		"/org/freedesktop/systemd1",          
		"org.freedesktop.systemd1.Manager",   
		"StopUnit",                          
		error,                              
		&m,                                   
		"ss",                                 
		"xmrig.service",                      
		"replace");
}

int status_xmrig(sd_bus** bus, sd_bus_error* error) {
	sd_bus_message *m = NULL;
	const char *status;
	int r = sd_bus_call_method(*bus,
		"org.freedesktop.systemd1",           
		"/org/freedesktop/systemd1",          
		"org.freedesktop.systemd1.Manager",   
		"GetUnit",                          
		error,
		&m,                                   
		"s",                                 
		"xmrig.service");


	if (r < 0) {
		//sd_bus_message_unref(m);
		if (strstr(error->message, "xmrig.service not loaded") != NULL) {
			*error = SD_BUS_ERROR_NULL;
			return 0; // not running
		} else {
			return -1; //actual error;
		}
	} else {
 		r = sd_bus_message_read(m, "o", &status);
		//sd_bus_message_unref(m);
		if (r < 0) return -2;
		return 1; // running
	}
}

int run(char* port) {
	int fd = connect_to_serial(port);
	if (fd < 0) {
		fprintf(stderr, "Error opening `%s`\n", port);
		return fd;
	}
	
	sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus *bus = NULL;
	
	int r;
	r = sd_bus_open_system(&bus);
	if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus\n");
		return r;
    }

	while (1) {
		char buf[1];

		r = read(fd, buf, sizeof buf);
		if (r < 0) {
			fprintf(stderr, "Failed to read serial port\n");
			break;
		}

		int switch_status = buf[0]=='1';
		int xmrig_status = status_xmrig(&bus, &error);
		if (xmrig_status < 0) {
			fprintf(stderr, "Failed to get XMRig status\n");
			r = xmrig_status;
			break;
		}

		char* led_data = xmrig_status ? "1" : "0";

		r = write(fd, led_data, 1);
		if (r < 0) {
			fprintf(stderr, "Failed to write to serial port (is the switch plugged in?)\n");
			break;
		}

		if (switch_status == xmrig_status) continue;
		printf("Syncing! S:%d X:%d\n", switch_status, xmrig_status);

		if (switch_status) {
			r = start_xmrig(&bus, &error);
		} else {
			r = stop_xmrig(&bus, &error);
		}

		if (r < 0) {
			fprintf(stderr, "Sync failed\n");
			break;
		}
	}

	return r;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		printf("Usage: ");
		printf("%s", argv[0]);
		printf(" /dev/ttyUSB1\n");
	} else {
		int res = run(argv[1]);
		printf("Error: %s\n", strerror(res * -1));
		return res;
	}
}
