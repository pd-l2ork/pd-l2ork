#include <m_pd.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include "xwiimote/lib/xwiimote.h"

/* Button flags */
#define WIIMOTE_BTN_2		0x0001
#define WIIMOTE_BTN_1		0x0002
#define WIIMOTE_BTN_B		0x0004
#define WIIMOTE_BTN_A		0x0008
#define WIIMOTE_BTN_MINUS	0x0010
#define WIIMOTE_BTN_HOME	0x0080
#define WIIMOTE_BTN_LEFT	0x0100
#define WIIMOTE_BTN_RIGHT	0x0200
#define WIIMOTE_BTN_DOWN	0x0400
#define WIIMOTE_BTN_UP		0x0800
#define WIIMOTE_BTN_PLUS	0x1000

#define WIIMOTE_NUNCHUK_BTN_Z	0x01
#define WIIMOTE_NUNCHUK_BTN_C	0x02

static t_class *pd_wiimote_class;
typedef struct _wiimote {
    t_object x_obj; // standard pd object (must be first in struct)

    pthread_t polling_thread;
    pthread_mutex_t polling_mutex;
    struct xwii_iface *xwii_interface;

    // We store atom list for each data type so we don't waste time
	// allocating memory at every callback:
	t_atom btn_atoms[2];				// core stuff
	t_atom old_btn_atoms[2];
	t_atom acc_atoms[3];
	t_atom acc_calibration_atoms[3];
	t_atom ir_atoms[4][4];
	t_atom nc_btn_atoms[1];				// nunchuk
	t_atom old_nc_btn_atoms[1];
	t_atom nc_acc_atoms[3];
	t_atom nc_acc_calibration_atoms[3];
	t_atom nc_stick_atoms[2];
	t_atom mp_ar_atoms[3];				// motionplus
	t_atom mp_ar_calibration_atoms[3];
	t_atom cl_l_stick_atoms[2];			// classic
	t_atom cl_r_stick_atoms[2];
	t_atom old_cl_buttons_atoms[4];
	t_atom cl_buttons_atoms[4];			// l r button-a button-b
	t_atom balance_atoms[4];			// balance board rt,rb,lt,lb

    // outlets:
    t_outlet *outlet_data;
    t_outlet *outlet_status;

    float report_acceleration;
    float report_ir;
    float report_expansion;

    float is_connected;

} t_wiimote;

static void pd_wiimote_free(t_wiimote* x) {

    pthread_join(x->polling_thread, NULL);
    pthread_mutex_destroy(&x->polling_mutex);

}

static void pd_wiimote_debug(t_wiimote *x) {
    post("\n======================");
	if (x->is_connected) post("Wiimote is connected.");
	else post("Wiimote is NOT connected.");
	if (x->report_acceleration) post("Acceleration: ON");
	else 			   post("Acceleration: OFF");
	if (x->report_ir)  post("IR:           ON");
	else 			   post("IR:           OFF");
	if (x->report_expansion)  post("Extension:    ON");
	else 			   post("Extension:    OFF");
}

static void pd_wiimote_polling_thread(t_wiimote *x) {
    int fds_num = 2, ret;
    struct pollfd fds[2];
    struct xwii_event event;

    memset(fds, 0, sizeof(fds));
    fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[1].fd = xwii_iface_get_fd(x->xwii_interface);
	fds[1].events = POLLIN;
	fds_num = 2;

	if(xwii_iface_watch(x->xwii_interface, true)) {
		post("Error: Cannot initialize hotplug watch descriptor\n");
    }

    while(true) {
        ret = poll(fds, fds_num, -1);
		if (ret < 0) {
			if (errno != EINTR) {
				ret = -errno;
				post("Error: Cannot poll fds\n");
				break;
			}
		}

		if (pthread_mutex_lock(&x->polling_mutex)) {
			post("Mutex lock error (polling mutex)\n");
			break;
		}
		
		ret = xwii_iface_dispatch(x->xwii_interface, &event, sizeof(event));
		if (ret) {
			if (ret != -EAGAIN) {
				post("Error: Read failed\n");
				break;
			}
		} else  {
			int mask = 0, buttonsBitfield = 0;
			switch (event.type) {
			case XWII_EVENT_GONE:
				post("Info: Device gone\n");
				fds[1].fd = -1;
				fds[1].events = 0;
				fds_num = 1;
				break;
			case XWII_EVENT_WATCH:
				xwii_iface_close(x->xwii_interface, xwii_iface_opened(x->xwii_interface));
				xwii_iface_open(x->xwii_interface, xwii_iface_available(x->xwii_interface));
				break;
			case XWII_EVENT_KEY:
				switch(event.v.key.code) {
					case XWII_KEY_LEFT:
						mask = WIIMOTE_BTN_LEFT;
						break;
					case XWII_KEY_RIGHT:
						mask = WIIMOTE_BTN_RIGHT;
						break;
					case XWII_KEY_UP:
						mask = WIIMOTE_BTN_UP;
						break;
					case XWII_KEY_DOWN:
						mask = WIIMOTE_BTN_DOWN;
						break;
					case XWII_KEY_A:
						mask = WIIMOTE_BTN_A;
						break;
					case XWII_KEY_B:
						mask = WIIMOTE_BTN_B;
						break;
					case XWII_KEY_HOME:
						mask = WIIMOTE_BTN_HOME;
						break;
					case XWII_KEY_MINUS:
						mask = WIIMOTE_BTN_MINUS;
						break;
					case XWII_KEY_PLUS:
						mask = WIIMOTE_BTN_PLUS;
						break;
					case XWII_KEY_ONE:
						mask = WIIMOTE_BTN_1;
						break;
					case XWII_KEY_TWO:
						mask = WIIMOTE_BTN_2;
						break;
				}

				buttonsBitfield = (atom_getfloat(x->btn_atoms) * 256) + (atom_getfloat(x->btn_atoms + 1));
				buttonsBitfield |= mask; // Set the bit to 1 if it isnt already
				buttonsBitfield ^= mask; // Flip the bit to 0
				buttonsBitfield |= event.v.key.state ? mask : 0; // Set the bit to 1 if required

				SETFLOAT(x->btn_atoms+0, (buttonsBitfield & 0xFF00)>>8);
				SETFLOAT(x->btn_atoms+1, buttonsBitfield & 0x00FF);

				break;
			case XWII_EVENT_ACCEL:

				SETFLOAT(x->acc_atoms, event.v.abs->x);
				SETFLOAT(x->acc_atoms + 1, event.v.abs->y);
				SETFLOAT(x->acc_atoms + 2, event.v.abs->z);
                break;
			case XWII_EVENT_IR:
				for(int i=0; i<4; i++) {
					SETFLOAT(x->ir_atoms[i], i);
					SETFLOAT(x->ir_atoms[i] + 1, event.v.abs[0].x);
					SETFLOAT(x->ir_atoms[i] + 2, event.v.abs[0].y);
					SETFLOAT(x->ir_atoms[i] + 3, 1024);
				}
				break;
			case XWII_EVENT_MOTION_PLUS:
				SETFLOAT(x->mp_ar_atoms, event.v.abs[0].x);
				SETFLOAT(x->mp_ar_atoms + 1, event.v.abs[0].y);
				SETFLOAT(x->mp_ar_atoms + 2, event.v.abs[0].z);
				break;
			case XWII_EVENT_NUNCHUK_KEY:

				switch(event.v.key.code) {
					case XWII_KEY_C:
						mask = WIIMOTE_NUNCHUK_BTN_C;
						break;
					case XWII_KEY_Z:
						mask = WIIMOTE_NUNCHUK_BTN_Z;
						break;
				}

				buttonsBitfield = atom_getfloat(x->nc_btn_atoms);
				buttonsBitfield |= mask; // Set the bit to 1 if it isnt already
				buttonsBitfield ^= mask; // Flip the bit to 0
				buttonsBitfield |= event.v.key.state ? mask : 0; // Set the bit to 1 if required

				SETFLOAT(x->nc_btn_atoms, buttonsBitfield);
				break;
			case XWII_EVENT_NUNCHUK_MOVE:
				SETFLOAT(x->nc_stick_atoms, event.v.abs[0].x);
				SETFLOAT(x->nc_stick_atoms + 1, event.v.abs[0].y);

				SETFLOAT(x->nc_acc_atoms, event.v.abs[1].x);
				SETFLOAT(x->nc_acc_atoms + 1, event.v.abs[1].y);
				SETFLOAT(x->nc_acc_atoms + 2, event.v.abs[1].z);
				break;
			case XWII_EVENT_CLASSIC_CONTROLLER_KEY:
			case XWII_EVENT_CLASSIC_CONTROLLER_MOVE:
                post("Warning: Classic controller not implemented\n");
				break;
			case XWII_EVENT_BALANCE_BOARD:
                post("Warning: Balance board not implemented\n");
				break;
			case XWII_EVENT_PRO_CONTROLLER_KEY:
			case XWII_EVENT_PRO_CONTROLLER_MOVE:
                post("Warning: Pro controller not implemented\n");
				break;
			case XWII_EVENT_GUITAR_KEY:
			case XWII_EVENT_GUITAR_MOVE:
                post("Warning: Guitar controller not implemented\n");
				break;
			case XWII_EVENT_DRUMS_KEY:
			case XWII_EVENT_DRUMS_MOVE:
                post("Warning: Drums controller not implemented\n");
				break;
			}
		}
		if (pthread_mutex_unlock(&x->polling_mutex)) {
			post("Mutex unlock error (polling mutex) - Deadlock warning\n");
		}
    }

}

static void pd_wiimote_setRumble(t_wiimote *x, t_floatarg f) {
    if (pthread_mutex_lock(&x->polling_mutex)) {
        post("Mutex lock error (polling mutex)\n");
        return;
    }

    if(xwii_iface_rumble(x->xwii_interface, f)) {
        post("Error: Cannot toggle rumble motor\n");
    }

    if (pthread_mutex_unlock(&x->polling_mutex)) {
        post("Mutex unlock error (polling mutex) - Deadlock warning\n");
    }
}

static void pd_wiimote_setReportMode(t_wiimote *x, t_floatarg r) {
    post("setReportMode is a no-op on disis_wiimote 2.0.0+");
}

static void pd_wiimote_reportAcceleration(t_wiimote *x, t_floatarg f) {
    x->report_acceleration = f;
}

static void pd_wiimote_reportExpansion(t_wiimote *x, t_floatarg f) {
    x->report_expansion = f;
}

static void pd_wiimote_reportIR(t_wiimote *x, t_floatarg f) {
    x->report_ir = f;
}

static void pd_wiimote_togglePassthrough(t_wiimote *x, t_floatarg f) {
    post("togglePassthrough is a no-op on disis_wiimote 2.0.0+");
}

static void pd_wiimote_setLED(t_wiimote *x, t_floatarg f) {
    if (pthread_mutex_lock(&x->polling_mutex)) {
        post("Mutex lock error (polling mutex)\n");
        return;
    }

	int ret;
	for(int n = 0; n < 4; n++) {
		bool state;
		bool desired = ((int)f >> n) % 2;

		ret = xwii_iface_get_led(x->xwii_interface, XWII_LED(n+1), &state);
		if (ret) {
			post("Error: Cannot get LED %i: %d", n+1, ret);
			continue;
		}

		if(state != desired) {
			ret = xwii_iface_set_led(x->xwii_interface, XWII_LED(n+1), desired);
			if (ret) {
				post("Error: Cannot toggle LED %i: %d", n+1, ret);
			}
		}
	}

    if (pthread_mutex_unlock(&x->polling_mutex)) {
        post("Mutex unlock error (polling mutex) - Deadlock warning\n");
    }
}

static void pd_wiimote_status(t_wiimote *x) {
	 if (pthread_mutex_lock(&x->polling_mutex)) {
        post("Mutex lock error (polling mutex)\n");
        return;
    }

	if(x->is_connected) {
		uint8_t battery;
		t_atom ap[1];
		xwii_iface_get_battery(x->xwii_interface, &battery);
		SETFLOAT(ap+0, battery);
		outlet_anything(x->outlet_data, gensym("battery"), 1, ap);
	}

    outlet_float(x->outlet_status, x->is_connected);

    if (pthread_mutex_unlock(&x->polling_mutex)) {
        post("Mutex unlock error (polling mutex) - Deadlock warning\n");
    }
}

static void pd_wiimote_doBang(t_wiimote *x) {
	int i;
	t_atom ap[4];

	if(!x->is_connected)
		return;


    if (pthread_mutex_lock(&x->polling_mutex)) {
        post("Mutex lock error (polling mutex)\n");
        return;
    }

	int interfaces = xwii_iface_available(x->xwii_interface);

	if (x->report_expansion == 1) {
		if(interfaces & XWII_IFACE_NUNCHUK) {
			// nunchuk
			SETSYMBOL(ap+0, gensym("stick"));
			SETFLOAT (ap+1, atom_getfloat(x->nc_stick_atoms+0) + 128);
			SETFLOAT (ap+2, atom_getfloat(x->nc_stick_atoms+1) + 128);
			outlet_anything(x->outlet_data, gensym("nunchuk"), 3, ap);

			SETSYMBOL(ap+0, gensym("acceleration"));
			SETFLOAT (ap+1, (atom_getfloat(x->nc_acc_atoms+0) - atom_getfloat(x->nc_acc_calibration_atoms+0)) / 256.f);
			SETFLOAT (ap+2, (atom_getfloat(x->nc_acc_atoms+1) - atom_getfloat(x->nc_acc_calibration_atoms+1)) / 256.f);
			SETFLOAT (ap+3, (atom_getfloat(x->nc_acc_atoms+2) - atom_getfloat(x->nc_acc_calibration_atoms+2)) / 256.f);
			outlet_anything(x->outlet_data, gensym("nunchuk"), 4, ap);

			if (atom_getfloat(x->old_nc_btn_atoms) != atom_getfloat(x->nc_btn_atoms)) {
				SETSYMBOL(ap+0, gensym("button"));
				SETFLOAT (ap+1, atom_getfloat(x->nc_btn_atoms+0));
				outlet_anything(x->outlet_data, gensym("nunchuk"), 2, ap);
				SETFLOAT(x->old_nc_btn_atoms+0, atom_getfloat(x->nc_btn_atoms));
			}
		}
		if(interfaces & XWII_IFACE_MOTION_PLUS) {
			SETSYMBOL(ap+0, gensym("angle_rate"));
			SETFLOAT (ap+1, atom_getfloat(x->mp_ar_atoms+0) - atom_getfloat(x->mp_ar_calibration_atoms+0));
			SETFLOAT (ap+2, atom_getfloat(x->mp_ar_atoms+1) - atom_getfloat(x->mp_ar_calibration_atoms+1));
			SETFLOAT (ap+3, atom_getfloat(x->mp_ar_atoms+2) - atom_getfloat(x->mp_ar_calibration_atoms+2));
			outlet_anything(x->outlet_data, gensym("motionplus"), 4, ap);	
		}
	}
	if (x->report_ir == 1) {
		if(interfaces & XWII_IFACE_IR) {
			for (i=0; i<4; i++) {
				if (atom_getfloat(x->ir_atoms[i]+0) != -1.0) {
					SETFLOAT (ap+0, atom_getfloat(x->ir_atoms[i]+0));
					SETFLOAT (ap+1, atom_getfloat(x->ir_atoms[i]+1));
					SETFLOAT (ap+2, atom_getfloat(x->ir_atoms[i]+2));
					SETFLOAT (ap+3, atom_getfloat(x->ir_atoms[i]+3));
					outlet_anything(x->outlet_data, gensym("ir"), 4, ap);
				}
			}
		}
	}
	if (x->report_acceleration == 1) {
		if(interfaces & XWII_IFACE_ACCEL) {
			SETFLOAT (ap+0, (atom_getfloat(x->acc_atoms+0) - atom_getfloat(x->acc_calibration_atoms+0)) / 128.f);
			SETFLOAT (ap+1, (atom_getfloat(x->acc_atoms+1) - atom_getfloat(x->acc_calibration_atoms+1)) / 128.f);
			SETFLOAT (ap+2, (atom_getfloat(x->acc_atoms+2) - atom_getfloat(x->acc_calibration_atoms+2)) / 128.f);
			outlet_anything(x->outlet_data, gensym("acceleration"), 3, ap);
		}
	}
	if(interfaces & XWII_IFACE_CORE) {
		if (atom_getfloat(x->old_btn_atoms+0) != atom_getfloat(x->btn_atoms+0) ||
			atom_getfloat(x->old_btn_atoms+1) != atom_getfloat(x->btn_atoms+1)) {
			SETFLOAT (ap+0, atom_getfloat(x->btn_atoms+0));
			SETFLOAT (ap+1, atom_getfloat(x->btn_atoms+1));
			outlet_anything(x->outlet_data, gensym("button"), 2, ap);
			SETFLOAT(x->old_btn_atoms+0, atom_getfloat(x->btn_atoms+0));
			SETFLOAT(x->old_btn_atoms+1, atom_getfloat(x->btn_atoms+1));
		}
	}

    if (pthread_mutex_unlock(&x->polling_mutex)) {
        post("Mutex unlock error (polling mutex) - Deadlock warning\n");
    }
}

static void pd_wiimote_doDisconnect(t_wiimote *x) {
    if(!x->is_connected) {
        post("Not connected to a wiimote!\n");
        return;
    }

    if(pthread_cancel(x->polling_thread)) {
        post("Failed to stop polling thread!\n");
    }

    if(pthread_join(x->polling_thread, NULL)) {
        post("Failed to join polling thread!\n");
    }

	xwii_iface_unref(x->xwii_interface);

	x->is_connected = false;
	pd_wiimote_status(x);
}

static void pd_wiimote_doConnect(t_wiimote *x, t_symbol *dongaddr) {

    if(x->is_connected) {
        post("Already connected to another wiimote!\n");
        return;
    }

	struct xwii_monitor *monitor = xwii_monitor_new(false,false);
	if(!monitor) {
		post("Cannot create xwii monitor!\n");
		return;
	}

	char *device;
    
    if(dongaddr != NULL) {

		bool found = false;
		while(!found) {

			// Get the next connected wiimote
			if(!(device = xwii_monitor_poll(monitor))) {
				break;
			}

			// Create a string with the file path of the uevent file
			char* path = calloc(sizeof(char), strlen(device) + strlen("/uevent") + 1);
			strcpy(path, device);
			strcat(path, "/uevent");

			// Open the file
			FILE* file = fopen(path, "r");
			free(path);
			if (file == NULL) {
				perror("Error opening file");
				continue;
			}

			char* hid_uniq_value = NULL;
			char line[100];

			// Read the file line by line
			while (fgets(line, sizeof(line), file)) {
				// Check if the line starts with "HID_UNIQ="
				if (strncmp(line, "HID_UNIQ=", strlen("HID_UNIQ=")) == 0) {
					// Extract the value after the "=" sign
					hid_uniq_value = strdup(line + strlen("HID_UNIQ="));
					// Remove trailing newline character if present
					char* newline_ptr = strchr(hid_uniq_value, '\n');
					if (newline_ptr != NULL)
						*newline_ptr = '\0';
					break;
				}
			}

			// Linux reports the MAC address with the hex digits in lowercase, but
			// pd-l2ork uses uppercase. This converts the found MAC address to uppercase.
			for(int i=0; i<strlen(hid_uniq_value); i++)
				hid_uniq_value[i] = toupper(hid_uniq_value[i]);

			if(strcmp(hid_uniq_value, dongaddr->s_name) == 0)
				found=true;

			// Free all the memory we used
			free(hid_uniq_value);
			fclose(file);
		}
    } else
        device = xwii_monitor_poll(monitor);

	if(!device) {
		post("Cannot find wiimote!\n");
		return;
	}

	if(xwii_iface_new(&x->xwii_interface, device)) {
		free(device);
		post("Cannot create xwii interface!\n");
		return;
	}
	free(device);

	if(xwii_iface_open(x->xwii_interface, xwii_iface_available(x->xwii_interface) | XWII_IFACE_WRITABLE)) {
		post("Cannot open xwii interface!\n");
		return;
	}


	// spawn threads for actions known to cause sample drop-outs
	if(pthread_mutex_init(&x->polling_mutex, NULL)) {
		pd_wiimote_doDisconnect(x);
		post("Cannot initialize mutex!\n");
		return;
	}

	if(pthread_create(&x->polling_thread, NULL, (void*)&pd_wiimote_polling_thread, x)) {
		pd_wiimote_doDisconnect(x);
		post("Cannot create polling thread!\n");
		return;
	}

	pd_wiimote_setRumble(x, 1);
	usleep(250000);
	pd_wiimote_setRumble(x, 0);

    x->is_connected = true;

	pd_wiimote_status(x);
    post("Connected to wiimote!\n");
}

static void pd_wiimote_discover(t_wiimote *x) {
    pd_wiimote_doConnect(x, NULL);
}

static void* pd_wiimote_new(t_symbol *s, int argc, t_atom *argv) {
    int i;

    post("DISIS new implementation of wiimote object v.2.0.0");
    t_wiimote *x = (t_wiimote *)pd_new(pd_wiimote_class);

    // create outlets:
    x->outlet_data = outlet_new(&x->x_obj, &s_list);

	// status outlet:
	x->outlet_status = outlet_new(&x->x_obj, &s_float);

    x->report_acceleration = 0;
    x->report_expansion = 0;
    x->report_ir = 0;
    x->is_connected = 0;

	// initialize values:
	SETFLOAT(x->acc_atoms+0, 0);
	SETFLOAT(x->acc_atoms+1, 0);
	SETFLOAT(x->acc_atoms+2, 0);
	for (i=0; i<4; i++)
	{
		SETFLOAT(x->ir_atoms[i]+0, -1);
		SETFLOAT(x->ir_atoms[i]+1, 0);
		SETFLOAT(x->ir_atoms[i]+2, 0);
		SETFLOAT(x->ir_atoms[i]+3, 0);
	}
	SETFLOAT(x->nc_acc_atoms+0, 0);
	SETFLOAT(x->nc_acc_atoms+1, 0);
	SETFLOAT(x->nc_acc_atoms+2, 0);
	SETFLOAT(x->nc_stick_atoms+0, 0);
	SETFLOAT(x->nc_stick_atoms+1, 0);

	SETFLOAT(x->mp_ar_atoms+0, 0);
	SETFLOAT(x->mp_ar_atoms+1, 0);
	SETFLOAT(x->mp_ar_atoms+2, 0);

	SETFLOAT(x->cl_l_stick_atoms+0, 0);
	SETFLOAT(x->cl_l_stick_atoms+1, 0);

	SETFLOAT(x->cl_r_stick_atoms+0, 0);
	SETFLOAT(x->cl_r_stick_atoms+1, 0);

	SETFLOAT(x->old_cl_buttons_atoms+0, 0);
	SETFLOAT(x->old_cl_buttons_atoms+1, 0);
	SETFLOAT(x->old_cl_buttons_atoms+2, 0);
	SETFLOAT(x->old_cl_buttons_atoms+3, 0);

	SETFLOAT(x->cl_buttons_atoms+0, 0);
	SETFLOAT(x->cl_buttons_atoms+1, 0);
	SETFLOAT(x->cl_buttons_atoms+2, 0);
	SETFLOAT(x->cl_buttons_atoms+3, 0);

	SETFLOAT(x->balance_atoms+0, 0);
	SETFLOAT(x->balance_atoms+1, 0);
	SETFLOAT(x->balance_atoms+2, 0);
	SETFLOAT(x->balance_atoms+3, 0);

	// connect if user provided an address as an argument:
	if (argc > 0)
	{
		post("conecting to provided address...");
		if (argv->a_type == A_SYMBOL)
		{
			pd_wiimote_doConnect(x, argv->a_w.w_symbol);
		} else {
			error("[disis_wiimote] expects either no argument, or a bluetooth address as an argument. eg, 00:19:1D:70:CE:72");
			return NULL;
		}
	}
	return (x);
}

static void pd_wiimote_calibrate(t_wiimote *x) {
    if (pthread_mutex_lock(&x->polling_mutex)) {
        post("Mutex lock error (polling mutex)\n");
        return;
    }

	x->acc_calibration_atoms[0] = x->acc_atoms[0];
	x->acc_calibration_atoms[1] = x->acc_atoms[1];
	x->acc_calibration_atoms[2] = x->acc_atoms[2];

	x->nc_acc_calibration_atoms[0] = x->nc_acc_atoms[0];
	x->nc_acc_calibration_atoms[1] = x->nc_acc_atoms[1];
	x->nc_acc_calibration_atoms[2] = x->nc_acc_atoms[2];

	x->mp_ar_calibration_atoms[0] = x->mp_ar_atoms[0];
	x->mp_ar_calibration_atoms[1] = x->mp_ar_atoms[1];
	x->mp_ar_calibration_atoms[2] = x->mp_ar_atoms[2];

    if (pthread_mutex_unlock(&x->polling_mutex)) {
        post("Mutex unlock error (polling mutex) - Deadlock warning\n");
    }
}

void disis_wiimote_setup(void)
{
	pd_wiimote_class = class_new(gensym("disis_wiimote"), (t_newmethod)pd_wiimote_new, (t_method)pd_wiimote_free, sizeof(t_wiimote), CLASS_DEFAULT, A_GIMME, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_debug, gensym("debug"), 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_doConnect, gensym("connect"), A_SYMBOL, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_doDisconnect, gensym("disconnect"), 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_discover, gensym("discover"), 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_setReportMode, gensym("setReportMode"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_reportAcceleration, gensym("reportAcceleration"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_reportExpansion, gensym("reportExpansion"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_reportIR, gensym("reportIR"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_togglePassthrough, gensym("togglePassthrough"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_setRumble, gensym("setRumble"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_setLED, gensym("setLED"), A_DEFFLOAT, 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_status, gensym("status"), 0);
	class_addmethod(pd_wiimote_class, (t_method) pd_wiimote_calibrate, gensym("calibrate"), 0);
	class_addbang(pd_wiimote_class, pd_wiimote_doBang);
}