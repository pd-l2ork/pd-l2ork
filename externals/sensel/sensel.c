#include <stdio.h>
#include <string.h>
#include <m_pd.h>
#include "g_canvas.h"

#include <unistd.h>
#include <stdlib.h>
#include "sensel.h"
#include "sensel_device.h"

#ifdef __MINGW32__
	#include <pthread.h>
	#include <time.h>
#elif defined WIN32
	// TODO: consider including native Windows compiler
	//       ifdef for threads and adapt threads below
#else
	#include <pthread.h>
	#include <time.h>
#endif

/*
	The Sensel Morph Pd external, written by
	Rachel Hachem <rachelly@vt.edu>
	under the guidance of Ivica Ico Bukvic <ico@vt.edu>

	2020-04-13 v.0.9.0
	Initial release by Rachel Hachem <rachelly@vt.edu>
	under the guidance of Ivica Ico Bukvic <ico@vt.edu>

	2020-05-20 v.1.0.0
	Threaded implementation with additional features,
	including, disconnect, free, identify, improved
	console output, versioning, updated help file, and
	bug-fixes to support multiple sensel morphs by
	Ivica Ico Bukvic <ico@vt.edu>

	2020-05-22 v.1.1.0
	Improved integration of the contacts API to fix
	contact id and provide contact status, as well as
	address missed contact status changes, reworked
	polling logic and removed reliance on the external
 	metro, added total contact count, and setting the
	polling time by Ivica Ico Bukvic <ico@vt.edu>

	2020-05-22 v.1.2.0
	Introduced ability to change LED brightness and
	further improved the help file by Ivica Ico Bukvic
	<ico@vt.edu>
  
	Please see the supporting help file for the
	explanation of its features.
*/
static t_class *sensel_class;

/*
	Single-linked list for accumulating data output
*/
typedef struct _data
{
	t_atom args[20];
	int type;	// 0 = data (20 args)
				// 1 = number of contacts (only one arg)
	struct _data *next;
} t_data;

/*
	An array keeping track of which devices are
	already connected up to maximum allowed by
	the sensel API
*/
static t_symbol sensel_connected_devices[SENSEL_MAX_DEVICES];

/*
	Adds a device to the list of connected devices,
	returning:
		0 for success
		-1 for failure (shouldn't happen as API should throw error first)
*/
static int add_connected_to_sensel_device_list(t_symbol *s)
{
	int i = 0;

	while (i < SENSEL_MAX_DEVICES)
	{
		if (sensel_connected_devices[i].s_name == NULL)
		{
			sensel_connected_devices[i] = *s;
			return(0);
		}
		else
			i++;
	}
	return(-1);
}

/*
	Removes device from the list of connected devices,
	returning:
		0 for success
		-1 for failure (shouldn't happen as API should throw error first)
*/
static int remove_connected_to_sensel_device_list(t_symbol *s)
{
	int i = 0;

	while (i < SENSEL_MAX_DEVICES)
	{
		if (sensel_connected_devices[i].s_name != NULL)
		{
			if (!strcmp(sensel_connected_devices[i].s_name, s->s_name))
			{
				sensel_connected_devices[i].s_name = NULL;
				return(0);
			}
			i++;
		}
	}
	return(-1);
}

/*
	Checks if a device we wish to connect is already
	on the list of connected devices, returns:
		1 for yes
		0 for no
*/ 
static int check_if_already_on_sensel_device_list(t_symbol *s)
{
	int i = 0;

	while (i < SENSEL_MAX_DEVICES)
	{
		if (sensel_connected_devices[i].s_name != NULL)
		{
			if (!strcmp(sensel_connected_devices[i].s_name, s->s_name))
				return(1);
		}
		i++;
	}
	return(0);
}

/*
	Main Sensel Morph data structure
*/
typedef struct _sensel
{
	t_object x_obj;
	t_atom x_atom;
	t_canvas *x_canvas;

	t_outlet *x_outlet_data;
	t_outlet *x_outlet_status;

	SENSEL_HANDLE x_handle;
	SenselFrameData *x_frame;
	int x_connected;
	int x_thread_connected;

	pthread_t x_unsafe_t;
	pthread_mutex_t x_unsafe_mutex;
	int x_unsafe;
	t_data *x_data;
	t_data *x_data_end;
	int x_n_contacts;
	int x_poll_wait;

	short unsigned int x_led[24];
	short unsigned int x_thread_led[24];

	t_clock *x_clock_output;
    int x_clock_set;

	t_symbol *x_serial;

} t_sensel;

/*
	Struct for passing args to the Sensel thread
*/
typedef struct _threadedFunctionParams
{
	t_sensel *s_inst;
} t_threadedFunctionParams;

/*
	Forward declarations
*/
static void sensel_poll(t_sensel *x);

/*
	Allows adjustment of the wait time
	in between poll reads from the Sensel socket
*/
static void sensel_set_poll_wait_time(t_sensel *x, t_floatarg f)
{
	if (f < 1.0 || f > 100.0)
	{
		error("sensel: poll time must be between 1 and 100ms (default 10ms).");
		return;
	}
	x->x_poll_wait = (int)(f * 1000.0);
}

/*
	Sets Sensel's LED with an ID to a desired brightness in (0-100)
*/
static void sensel_set_led(t_sensel *x, t_floatarg id, t_floatarg brightness)
{
	if (x->x_connected == 1)
	{
		if (id >= 0 && id <= 23 && brightness >= 0 && brightness <= 100)
		{
			x->x_led[(int)id] = (int)brightness;
		}
	}
}

/*
	Processes changes in LEDs via subthread call
*/
static void sensel_update_leds(t_sensel *x)
{
	for (int i = 0; i < 24; i++)
	{
		if (x->x_led[i] != x->x_thread_led[i])
		{
			x->x_thread_led[i] = x->x_led[i];
			senselSetLEDBrightness(x->x_handle, i, x->x_thread_led[i]);
		}
	}
}

/*
	Threaded function that reads from the Sensel
	without blocking the main audio thread
*/
static void *sensel_pthreadForAudioUnfriendlyOperations(void *ptr)
{
	t_threadedFunctionParams *rPars = (t_threadedFunctionParams*)ptr;
	t_sensel *x = rPars->s_inst;

	while(x->x_unsafe > -1)
	{
		pthread_mutex_lock(&x->x_unsafe_mutex);
		// inform the external when the thread is ready
		if (x->x_unsafe == 1)
			x->x_unsafe = 0;

		if (x->x_thread_connected != x->x_connected)
		{
			x->x_thread_connected = x->x_connected;
			if (x->x_thread_connected)
			{
				// Start scanning the Sensel device
				senselStartScanning(x->x_handle);
			}
			else {
				// This is where we stop scanning and disconnect
				senselStopScanning(x->x_handle);
				senselFreeFrameData(x->x_handle, x->x_frame);
				senselClose(x->x_handle);
			}
		}

		// only poll for new data if the previous sensel_output_data
		// has been processed (I suppose this could be also done
		// with a semaphore but since Windows pthreads implementation
		// via CygWin is not entirely compatible, I figured this may
		// be a "cleaner" way to do this
		if (x->x_clock_set == 0)
		{
			sensel_poll(x);
		}

		sensel_update_leds(x);

		pthread_mutex_unlock(&x->x_unsafe_mutex);

		usleep(x->x_poll_wait);
	}
	pthread_exit(0);

	return(0);
}

/*
	Outputs received data via clock delay that is triggered
	from the sub-thread
*/
static void sensel_output_data(t_sensel *x)
{
    if (x->x_clock_set == 1)
    {
		while (x->x_data != NULL)
		{
			switch (x->x_data->type)
            {
				case 0: // data
                    outlet_list(x->x_outlet_data, gensym("list"), 20, x->x_data->args);
					break;
				case 1: // number of contacts
					outlet_anything(x->x_outlet_data, gensym("contacts"), 1, &x->x_data->args[0]);
					break;
			}
			t_data *last = x->x_data;
            x->x_data = x->x_data->next;
			free(last);
		}
        x->x_clock_set = 0;
    }
}

/*
	Connects the Pd patch to a specific Sensel device, using
	the serial number as an argument.
*/
static void sensel_connect(t_sensel *x, t_symbol *s)
{
	post("sensel: connecting to device with serial number %s...", s->s_name);

	if (x->x_connected == 1)
	{
		error("sensel: connect failed--device already connected.");
		return;
	}

	if (check_if_already_on_sensel_device_list(s))
	{
		error("sensel: connect failed--device is already connected to another sensel object.");
		return;
	}

	// List of all available Sensel devices
	SenselDeviceList list;

	// Get a list of available Sensel devices
	senselGetDeviceList(&list);
	if (list.num_devices == 0)
	{
		error("sensel: connect failed--no device found.");
		return;
	}

	int result = senselOpenDeviceBySerialNum(&x->x_handle, (unsigned char *)s->s_name);

	if (!result)
	{
		// Set the frame content to scan contact data
		senselSetFrameContent(x->x_handle, FRAME_CONTENT_CONTACTS_MASK);
		// Allocate a frame of data, must be done before reading frame data
		senselAllocateFrameData(x->x_handle, &x->x_frame);
		post("sensel: successfully connected to device with a serial number %s.", s->s_name);
		x->x_connected = 1;

		x->x_serial = s;
		add_connected_to_sensel_device_list(x->x_serial);

		// sensel_connected_devices[0] = *gensym("blah");
		outlet_float(x->x_outlet_status, x->x_connected);
	}

	if (x->x_connected != 1) {
		error("sensel: connect failed--device with a serial number %s not found.", s->s_name);
	}
}

/*
	Discovers and connects to the first available Sensel Morph
*/
static void sensel_discover(t_sensel *x)
{

	if (x->x_connected == 1)
	{
		error("sensel: discover failed--device already connected.");
		return;
	}

	// List of all available Sensel devices
	SenselDeviceList list;

	// Get a list of available Sensel devices
	senselGetDeviceList(&list);
	if (list.num_devices == 0)
	{
		error("sensel: discover failed--no device found.");
		return;
	}

	int i = 0;
	int found = 0;

	while(i < list.num_devices)
	{
		if (!check_if_already_on_sensel_device_list(gensym((const char *)list.devices[i].serial_num)))
		{
			found = 1;
			break;
		}
		i++;
	}
	if (!found)
	{
		error("sensel: discover failed--all discoverable devices are already connected to another sensel object.");
		return;
	}
 
	// Open a Sensel device by the id in the SenselDeviceList, handle initialized 
	senselOpenDeviceByID(&x->x_handle, list.devices[i].idx);

	// Set the frame content to scan contact data
	senselSetFrameContent(x->x_handle, FRAME_CONTENT_CONTACTS_MASK);

	// Allocate a frame of data, must be done before reading frame data
	senselAllocateFrameData(x->x_handle, &x->x_frame);

	// Post information to the Pd console
	post("sensel: successfully connected to device with a serial number %s.", list.devices[i].serial_num);

	x->x_serial = gensym((const char *)list.devices[i].serial_num);
	add_connected_to_sensel_device_list(x->x_serial);

	x->x_connected = 1;

	outlet_float(x->x_outlet_status, x->x_connected);
}

/*
	Lists serial numbers of all available Sensel Morphs
*/
static void sensel_identify(t_sensel *x)
{
	// disable polling while doing identifying
	// as this tends to misbehave on Macs when
	// it gets this request while outputting data
	pthread_mutex_lock(&x->x_unsafe_mutex);
	
	// List of all available Sensel devices
	SenselDeviceList list;

	// Get a list of available Sensel devices
	senselGetDeviceList(&list);
	if (list.num_devices == 0)
	{
		post("sensel: identify found no devices.");
		return;
	}

	post("sensel: identify found following devices:");

	int i;
	for (i = 0; i < list.num_devices; i++)
	{
		post("%d: %s", i+1, list.devices[i].serial_num);
	}
	// reenable polling since we are now done
	pthread_mutex_unlock(&x->x_unsafe_mutex);
}

/*
	Polls for the Sensel contact data. Outputs a list for every
	current contact, each comprised of 19 data points, listed
	clearly in the code.
*/
static void sensel_poll(t_sensel *x)
{

	if (x->x_connected == 1)
	{

		unsigned int num_frames = 0;

		t_data *temp;

		// Read all available data from the Sensel device
		senselReadSensor(x->x_handle);

		senselGetNumAvailableFrames(x->x_handle, &num_frames);

		for (unsigned int f = 0; f < num_frames; f++)
		{

			// Read one frame of data
			senselGetFrame(x->x_handle, x->x_frame);

			for (int c = 0; c < x->x_frame->n_contacts; c++)
			{

				if (x->x_data == NULL) // this is the first time around
				{
					x->x_data = (t_data *)getbytes(sizeof(t_data));
					x->x_data->next = NULL;
					x->x_data->type = 0;

					x->x_data_end = x->x_data;
				}
				else
				{
					temp = (t_data *)getbytes(sizeof(t_data));
					temp->next = NULL;
					temp->type = 0;

					x->x_data_end->next = temp;
					x->x_data_end = temp;
				}

				SETFLOAT(&(x->x_data_end->args[0]), x->x_frame->contacts[c].id);
				SETFLOAT(&(x->x_data_end->args[1]), x->x_frame->contacts[c].state);
				SETFLOAT(&(x->x_data_end->args[2]), x->x_frame->contacts[c].orientation);
				SETFLOAT(&(x->x_data_end->args[3]), x->x_frame->contacts[c].major_axis);
				SETFLOAT(&(x->x_data_end->args[4]), x->x_frame->contacts[c].minor_axis);
				SETFLOAT(&(x->x_data_end->args[5]), x->x_frame->contacts[c].delta_x);
				SETFLOAT(&(x->x_data_end->args[6]), x->x_frame->contacts[c].delta_y);
				SETFLOAT(&(x->x_data_end->args[7]), x->x_frame->contacts[c].delta_force);
				SETFLOAT(&(x->x_data_end->args[8]), x->x_frame->contacts[c].delta_area);
				SETFLOAT(&(x->x_data_end->args[9]), x->x_frame->contacts[c].min_x);
				SETFLOAT(&(x->x_data_end->args[10]), x->x_frame->contacts[c].min_y);
				SETFLOAT(&(x->x_data_end->args[11]), x->x_frame->contacts[c].max_x);
				SETFLOAT(&(x->x_data_end->args[12]), x->x_frame->contacts[c].max_y);
				SETFLOAT(&(x->x_data_end->args[13]), x->x_frame->contacts[c].peak_x);
				SETFLOAT(&(x->x_data_end->args[14]), x->x_frame->contacts[c].peak_y);
				SETFLOAT(&(x->x_data_end->args[16]), x->x_frame->contacts[c].x_pos);
				SETFLOAT(&(x->x_data_end->args[17]), x->x_frame->contacts[c].y_pos);
				SETFLOAT(&(x->x_data_end->args[19]), x->x_frame->contacts[c].area);

				if (x->x_frame->contacts[c].state != 3)
				{
					SETFLOAT(&(x->x_data_end->args[15]), x->x_frame->contacts[c].peak_force);
					SETFLOAT(&(x->x_data_end->args[18]), x->x_frame->contacts[c].total_force);
				}
				else
				{
					SETFLOAT(&(x->x_data_end->args[15]), 0);
					SETFLOAT(&(x->x_data_end->args[18]), 0);
				}

				x->x_data_end->type = 0;
			}
			// output a total number of contacts
			if (x->x_frame->n_contacts != x->x_n_contacts)
			{
				if (x->x_data == NULL) // this is the first time around
				{
					x->x_data = (t_data *)getbytes(sizeof(t_data));
					x->x_data->next = NULL;
					x->x_data->type = 1;

					x->x_data_end = x->x_data;
				}
				else
				{
					temp = (t_data *)getbytes(sizeof(t_data));
					temp->next = NULL;
					temp->type = 1;

					x->x_data_end->next = temp;
					x->x_data_end = temp;
				}
				SETFLOAT(&(x->x_data_end->args[0]), x->x_frame->n_contacts);
				x->x_n_contacts = x->x_frame->n_contacts;
			}
		}

		if (x->x_data != NULL)
        {
			clock_delay(x->x_clock_output, 0);
            x->x_clock_set = 1;
        }
	}
}

/*
	Constructor for the sensel object
*/
static void *sensel_new()
{
	post("L2Ork Sensel Morph v.1.2.0\nIf this object fails to create,"
		" you have probably forgotten to install the SenselLib available"
		" at https://github.com/sensel/sensel-api/tree/master/sensel-install");

	t_sensel *x = (t_sensel *)pd_new(sensel_class);

	x->x_outlet_data = outlet_new(&x->x_obj, &s_list);
	x->x_outlet_status = outlet_new(&x->x_obj, &s_float);

	x->x_connected = 0;
	x->x_thread_connected = 0;
	x->x_n_contacts = 0;
	x->x_frame = NULL;
	x->x_data =  NULL;
	x->x_data_end = NULL;

	for (int i = 0; i < 24; i++)
	{
		x->x_led[i] = 0;
		x->x_thread_led[i] = 0;
	}

	x->x_clock_output = clock_new(x, (t_method)sensel_output_data);
    x->x_clock_set = 0;

	// prep the secondary thread init variable
	x->x_unsafe = 1;
	
	// initialize 10ms polling time expressed in useconds
	x->x_poll_wait = 10000;

	t_threadedFunctionParams rPars;
	rPars.s_inst = x;
	pthread_mutex_init(&x->x_unsafe_mutex, NULL);
	//pthread_cond_init(&x->x_unsafe_cond, NULL);
	pthread_create( &x->x_unsafe_t, NULL, (void *) &sensel_pthreadForAudioUnfriendlyOperations, (void *) &rPars);

	// wait until other thread has properly intialized so that
	// rPars do not get destroyed before the thread has gotten its
	// pointer information
	while(x->x_unsafe == 1)
	{
		sched_yield();
	}

	return(x);
}

/*
	Disconnects the Sensel Morph
*/
static void sensel_disconnect(t_sensel *x)
{
	if (x->x_connected)
	{
		x->x_connected = 0;
		while(x->x_thread_connected != x->x_connected)
			sched_yield();

		remove_connected_to_sensel_device_list(x->x_serial);
		x->x_serial = NULL;

		outlet_float(x->x_outlet_status, x->x_connected);
	}
	else
	{
		error("sensel: disconnect failed--no device connected.");
	}
}


/*
	Frees the object (when it is deleted)
*/
static void sensel_free(t_sensel * x)
{
	if (x->x_connected) {
		sensel_disconnect(x);
	}

	x->x_unsafe = -1;

	pthread_join(x->x_unsafe_t, NULL);
	pthread_mutex_destroy(&x->x_unsafe_mutex);

	clock_free(x->x_clock_output);

	// delete any leftover single-linked output data list
	t_data *last = NULL;
	while (x->x_data != NULL)
	{
		outlet_list(x->x_outlet_data, gensym("list"), 19, x->x_data->args);
		last = x->x_data;
		x->x_data = x->x_data->next;
		free(last);
	}
}

/*
	Init sensel object
*/
void sensel_setup(void)
{
	sensel_class = class_new(gensym("sensel"), 
		(t_newmethod)sensel_new, (t_method)sensel_free, 
		sizeof(t_sensel), CLASS_DEFAULT, 0);

	//class_addbang(sensel_class, (t_method)sensel_bang);
	class_addmethod(sensel_class, (t_method)sensel_connect,
		gensym("connect"), A_SYMBOL, 0);
	class_addmethod(sensel_class, (t_method)sensel_discover,
		gensym("discover"), 0);
	class_addmethod(sensel_class, (t_method)sensel_disconnect,
		gensym("disconnect"), 0);
	class_addmethod(sensel_class, (t_method)sensel_identify,
		gensym("identify"), 0);
	class_addmethod(sensel_class, (t_method)sensel_set_poll_wait_time,
		gensym("poll"), A_FLOAT);
	class_addmethod(sensel_class, (t_method)sensel_set_led,
		gensym("led"), A_FLOAT, A_FLOAT, 0);
}
