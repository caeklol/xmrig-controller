#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <libudev.h>
#include <time.h>  
#include <errno.h>    

// https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}
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
		sd_bus_message_unref(m);
		if (strstr(error->message, "xmrig.service not loaded") != NULL) {
			*error = SD_BUS_ERROR_NULL;
			return 0; // not running
		} else {
			return -1; //actual error;
		}
	} else {
		sd_bus_message_unref(m);
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

	clock_t s, n;

	while (1) {
		s = clock();
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

		n = clock();
		if ((n-s) < 500) {
			int r = msleep((long)(n-s));
			if (r < 0) fprintf(stderr, "WARNING: Failed to sleep! systemd may be overloaded - sync delay might be longer\n");
		}
	}

	sd_bus_unref(bus);
	sd_bus_error_free(&error);

	return r;
}

// stolen from somewhere ican't remember
int wait_for_device(char** port) {
	struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;
    const char *path = NULL;
	int res = -6;

    udev = udev_new();
    if (!udev) {
        return -11;
    }

    enumerate = udev_enumerate_new(udev);
    if (!enumerate) {
        udev_unref(udev);
        return -11;
    }

    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_add_match_property(enumerate, "ID_VENDOR_ID", "2341"); // arduino micro
    udev_enumerate_add_match_property(enumerate, "ID_MODEL_ID", "8037");
    udev_enumerate_scan_devices(enumerate);

    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path_tmp;

        path_tmp = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path_tmp);

        path = udev_device_get_devnode(dev);
        if (path) {
			res = 0;
            break;
        }

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

	if (res >= 0) {
		*port = (char*) malloc(strlen(path) + 1);
		strcpy(*port, path);
	}

	return res;
}

int main(int argc, char* argv[]) {
	int firstLaunch = 1;
	while (1) {
		if (!firstLaunch) {
			printf("Retrying in 5s...\n");
			usleep(1000 * 5000);
		} else {
			firstLaunch = 0;
		}

		char* port;

		int res = wait_for_device(&port); // allocates memory for port !!!
		if (res < 0) {
			printf("Error finding arduino: %s\n", strerror(res * -1));
			continue;
		}

		printf("Detected port: %s\n", port);

		res = run(port);
		free(port);

		printf("Error while listening to serial port: %s\n", strerror(res * -1));
	}

	return 0;
}
