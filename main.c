#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

int find_keyboard_device(char *device_path) {
    DIR *dir;
    struct dirent *ent;
    char path[1024];
    bool found = false;
    printf("Searching:\n");
    if ((dir = opendir("/dev/input/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strncmp(ent->d_name, "event", 5) == 0) {
                sprintf(path, "/dev/input/%s", ent->d_name);
                int fd = open(path, O_RDONLY);
                if (fd >= 0) {
                    struct libevdev *dev = NULL;
                    if (libevdev_new_from_fd(fd, &dev) >= 0) {
                        if (libevdev_has_event_type(dev, EV_KEY) &&
                            libevdev_has_event_code(dev, EV_KEY, KEY_A)) {
                            printf("%s => %s\n", path, libevdev_get_name(dev));
                            if(!found){
                                strcpy(device_path, path);
                                found = true;
                            }

                        }
                        libevdev_free(dev);
                    }
                    close(fd);
                }
            }
        }
        closedir(dir);
    }
    printf("Using: %s\n", device_path);
    return -1;
}
int main(int argc, char** argv) {
    struct libevdev *dev = NULL;
    struct input_event ev;

    char* dev_path = calloc(1024, sizeof(char));
    if (argc < 2) {
        find_keyboard_device(dev_path);
    } else {
        strncpy(dev_path, argv[1], 1024);
    }

    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        exit(1);
    }

    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        perror("Failed to initialize libevdev");
        close(fd);
        exit(1);
    }

    // replace vendor info
    libevdev_set_name(dev, "AltGr Emulator");
    libevdev_set_id_vendor(dev, 0x1453);
    libevdev_set_id_product(dev, 0x1299);

    // enable altgr
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, KEY_RIGHTALT, NULL);

    // Initialize uinput device
    struct libevdev_uinput *uidev;
    int err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (err < 0) {
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-err));
        libevdev_free(dev);
        return -1;
    }

	// sleep 300ms
    usleep(300);

    ioctl(libevdev_get_fd(dev), EVIOCGRAB, 1);

    // define ctrl and alt status
    bool ctrl = false;
    bool alt = false;

    do {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0 && ev.type == EV_KEY) {
        
            if(ev.code == KEY_LEFTALT || ev.code == KEY_LEFTCTRL) {
		        if (ev.code == KEY_LEFTALT) {
		             alt = ev.value;
		        }
		        if (ev.code == KEY_LEFTCTRL) {
		             ctrl = ev.value;
		        }
		        if (ctrl && alt) {
		            libevdev_uinput_write_event(uidev, ev.type, KEY_LEFTALT, 0);
		            libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
		            libevdev_uinput_write_event(uidev, ev.type, KEY_LEFTCTRL, 0);
		            libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
		            libevdev_uinput_write_event(uidev, ev.type, KEY_RIGHTALT, ev.value);
		            libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
		            continue;
		        } else {
		            libevdev_uinput_write_event(uidev, ev.type, KEY_RIGHTALT, 0);
		            libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
		        }
		    }
		    printf("%d\n", ev.code);
            rc = libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
            libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
        }
    } while (rc == 1 || rc == 0 || rc == -EAGAIN);

    libevdev_free(dev);
    close(fd);

    return 0;
}

