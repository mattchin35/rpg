#include "rpg_legacy.h"


/*----------------------------------------------------*/
/* Python Module Implementation                       */
/*----------------------------------------------------*/

static PyObject* py_buildgrating(PyObject *self, PyObject *args) {
    char* filename;
    double duration, angle, sf, tf, contrast, percent_sigma, percent_diameter,
           percent_center_left, percent_center_top, percent_padding;
    int width, height, waveform, background, colormode;
    if (!PyArg_ParseTuple(args, "sdddddiiiidddddi", &filename, &duration, &angle,
                          &sf, &tf, &contrast, &background, &width, &height, &waveform,
                          &percent_sigma, &percent_diameter, &percent_center_left,
			  &percent_center_top, &percent_padding,&colormode)){
        return NULL;
    }
    if(build_grating(filename,duration,angle,sf,tf,contrast,background,width,height,waveform,
			percent_sigma, percent_diameter,percent_center_left,
			percent_center_top, percent_padding,colormode)){
        return NULL;
    }
    Py_RETURN_NONE; 
}



static PyObject* py_init(PyObject *self, PyObject *args) {
    int xres,yres,colormode;
    if (!PyArg_ParseTuple(args, "iii", &xres, &yres,&colormode)) {
        return NULL;
    }
    fb_config* fb0_pointer = malloc(sizeof(fb_config)); 
    if (fb0_pointer == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    *fb0_pointer = init(xres,yres,colormode);
    if(fb0_pointer->error){
        if (fb0_pointer->backend_type == RPG_BACKEND_DRM) {
            drm_close_display(fb0_pointer);
        }
        free(fb0_pointer);
        return NULL;
    }
    PyObject* fb0_capsule = PyCapsule_New(fb0_pointer, "framebuffer",NULL);
    Py_INCREF(fb0_capsule);
    return fb0_capsule;
}

static PyObject* py_displaycolor(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
    int r,g,b,colormode,blocking;
        if (!PyArg_ParseTuple(args, "Oiiiii", &fb0_capsule,&r,&g,&b,&colormode,&blocking)) {
        return NULL;
    }
    uint16_t color_16 = rgb_to_uint(r,g,b);
    uint24_t color_24 = rgb_to_uint_24bit(r,g,b);
    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule,"framebuffer");
    display_color(fb0_pointer,color_16,color_24,colormode,blocking);
    Py_RETURN_NONE;
}


static PyObject* py_loadgrating(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
    char* filename;
        if (!PyArg_ParseTuple(args, "Os", &fb0_capsule,&filename)) {
        return NULL;
    }
    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule,"framebuffer");

    //Check file height/width against framebuffer height/width

    int filedes = open(filename, O_RDWR);
    if(filedes == -1){
        perror("Failed to open file");
        return NULL;
    }

    fileheader_t* header = mmap(NULL,sizeof(fileheader_t),PROT_READ,MAP_PRIVATE,
					filedes,0);
    close(filedes);

    if(header==MAP_FAILED){
        perror("From mmap for header access");
        exit(1);
    }

    if (fb0_pointer->width != header->width || fb0_pointer->height != header->height) {
        PyErr_Format(PyExc_ValueError, "Grating cannot be displayed at current Screen solution. Grating is %d x %d px, while Screen is %d x %d px.", header->width, header->height, fb0_pointer->width, fb0_pointer->height);
        return NULL;
    }

    void* grating_data = load_grating(filename,*fb0_pointer);

    if (grating_data == NULL) {
        if (PyErr_Occurred()) {
            return NULL;
        }
        PyErr_Format(PyExc_FileNotFoundError, "You probably mistyped the file name. Parsed as %s", filename);
 	return NULL;
    }
    PyObject* grating_capsule = PyCapsule_New(grating_data, "grating_data",NULL);
    Py_INCREF(grating_capsule);
    return grating_capsule;
}

static PyObject* py_debugdumpgrating(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
    PyObject* grating_capsule;
    char* filename;
    if (!PyArg_ParseTuple(args, "OOs", &fb0_capsule,&grating_capsule,&filename)) {
        return NULL;
    }
    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule,"framebuffer");
    void* grating_data = PyCapsule_GetPointer(grating_capsule,"grating_data");
    if(grating_data == NULL){
        return NULL;
    }
    if (debug_dump_grating(grating_data,*fb0_pointer,filename)){
	    return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* py_loadraw(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
    char* filename;
    if (!PyArg_ParseTuple(args, "Os", &fb0_capsule, &filename)) {
        return NULL;
    }

    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule,"framebuffer");

    //Check file height/width against framebuffer height/width

    int filedes = open(filename, O_RDWR);
    if(filedes == -1){
        perror("Failed to open file");
        return NULL;
    }

    fileheader_raw* header = mmap(NULL,sizeof(fileheader_raw),PROT_READ,MAP_PRIVATE,
					filedes,0);
    close(filedes);

    if(header==MAP_FAILED){
        perror("From mmap for header access");
        exit(1);
    }

    if (fb0_pointer->width != header->width || fb0_pointer->height != header->height) {
        PyErr_Format(PyExc_ValueError, "Raw cannot be displayed at current Screen solution. Raw is %d x %d px, while Screen is %d x %d px.", header->width, header->height, fb0_pointer->width, fb0_pointer->height);
        return NULL;
    }

    void* raw_data = load_raw(filename);
    if (raw_data == NULL) {
        if (PyErr_Occurred()) {
            return NULL;
        }
        PyErr_Format(PyExc_FileNotFoundError, "You probably mistyped the file name. Parsed as %s", filename);
        return NULL;
    }
    PyObject* raw_capsule = PyCapsule_New(raw_data, "raw_data", NULL);
    Py_INCREF(raw_capsule);
    return raw_capsule;
}

static PyObject* py_unloadgrating(PyObject* self, PyObject* args){
    PyObject* grating_capsule;
    void* grating_pointer;
    if (!PyArg_ParseTuple(args, "O", &grating_capsule)) {
        return NULL;
    }
    grating_pointer = PyCapsule_GetPointer(grating_capsule,"grating_data");
    unload_grating(grating_pointer);
    Py_DECREF(grating_capsule);
    Py_RETURN_NONE;
}

static PyObject* py_unloadraw(PyObject* self, PyObject* args) {
    PyObject* raw_capsule;
    void* raw_pointer;
    if (!PyArg_ParseTuple(args, "O", &raw_capsule)) {
        return NULL;
    }
    raw_pointer = PyCapsule_GetPointer(raw_capsule, "raw_data");
    unload_raw(raw_pointer);
    Py_DECREF(raw_capsule);
    Py_RETURN_NONE;
}

static PyObject* py_displaygrating(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
    PyObject* grating_capsule;
    int trig_pin;
    if (!PyArg_ParseTuple(args, "OOi", &fb0_capsule,&grating_capsule,&trig_pin)) {
        return NULL;
    }
    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule,"framebuffer");
    void* grating_data = PyCapsule_GetPointer(grating_capsule,"grating_data");
    int colormode;
    if(fb0_pointer->depth==24){
        colormode = RGB888MODE;
    }else{
        colormode = RGB565MODE;
    }
    if(grating_data == NULL){
        return NULL;
    }
    int start_time = time(NULL);
    double* grat_info = display_grating(grating_data,fb0_pointer,trig_pin,colormode);
    if (grat_info == NULL) {
        free(grat_info);
        if (!PyErr_Occurred()) {
            PyErr_Format(PyExc_KeyboardInterrupt, "Key pressed while waiting for pulse - or maybe a very weird error?");
        }
 	return NULL;
    } else {
        PyObject* return_tuple = Py_BuildValue("(ddi)",*grat_info,*(grat_info+1),start_time);
        free(grat_info);
        return return_tuple;
    }
}

static PyObject* py_displayraw(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
    PyObject* raw_capsule;
    int trig_pin;
    if (!PyArg_ParseTuple(args, "OOi", &fb0_capsule, &raw_capsule, &trig_pin)) {
        return NULL;
    }
    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule, "framebuffer");
    void* raw_data = PyCapsule_GetPointer(raw_capsule, "raw_data");
    if(raw_data == NULL){
        return NULL;
    }
    int colormode;    
    if(fb0_pointer->depth==24){
        colormode = RGB888MODE;
    }else{
        colormode = RGB565MODE;
    }
    int start_time = time(NULL);
    float* raw_info = display_raw(raw_data, fb0_pointer, trig_pin, colormode);
    if (raw_info == 0) {
        free(raw_info);
        if (PyErr_Occurred()) {
            return NULL;
        }
        Py_RETURN_NONE;
    } else {
        PyObject* return_tuple = Py_BuildValue("(ddi)", *raw_info, *(raw_info+1), start_time);
        free(raw_info);
        return return_tuple;
    }
}

static PyObject* py_closedisplay(PyObject* self, PyObject* args){
    PyObject* fb0_capsule;
        if (!PyArg_ParseTuple(args, "O", &fb0_capsule)) {
        return NULL;
    }
    fb_config* fb0_pointer = PyCapsule_GetPointer(fb0_capsule,"framebuffer");
    if(close_display(fb0_pointer)){
        return NULL;
    }
    Py_DECREF(fb0_capsule);
    Py_RETURN_NONE;
}


static PyObject* py_convertraw(PyObject* self, PyObject* args){
	char *filename, *new_filename;
	int n_frames, width, height, refresh_per_frame, colormode;
	if (!PyArg_ParseTuple(args, "ssiiiii", &filename, &new_filename,
				&n_frames, &width, &height, &refresh_per_frame,
				&colormode)) {
		return NULL;
	}
	if(convert_raw(filename, new_filename, n_frames, width, height, refresh_per_frame, colormode)) {
		return NULL;
	}
	Py_RETURN_NONE;
}


static PyMethodDef _rpigratings_methods[] = { 
    {   
        "init", py_init, METH_VARARGS,
        "Initialise the display and return a framebuffer object.\n"
	":Param xres:  the virtual width of the display\n"
	":Param yres:  the virtual height of the display\n"
	":Param depth: the number of bits per pixel - 16 or 24\n"
	":rtype framebuffer capsule: a framebuffer object for use\n"
	"with other functions in this module.\n"
	"WARNING: only one instance of this object should\n"
	"exist at any one time. Framebuffer objects should\n"
	"be cleaned up by being passed to close_display().\n\n"
	"Since init modifies the screen settings, it is a good\n"
	"idea to try and catch all exceptions, call \n"
	"close_display, and then re-raise the exeption like this:\n\n"
        " >>> root = init(1680,1050) \n"
        " >>> for file in os.listdir():\n"
        " >>>     try:\n"
        " >>>         load_grating(root, file)\n"
        " >>>         display_grating(root, file)\n"
        " >>>         unload_grating(root, file)\n"
        " >>>     except:\n"
        " >>>         close_grating(root)\n"
        " >>>         raise\n",
        " >>> close_grating(root)\n",
    },  
    {   
        "display_color", py_displaycolor, METH_VARARGS,
        "Display a rbg color to the framebuffer.\n"
	":Param fb0: a framebuffer object returned from init()\n"
	":Param r: the red component of the color (int)\n"
	":Param b: the blue component of the color (int)\n"
	":Param g: the green component of the color (int)\n"
	":rtype None:"
    },  
{   
        "display_grating", py_displaygrating, METH_VARARGS,
        "Displays data that has been loaded into memory to the screen.\n"
	":Param fb0: a framebuffer object created from an init() call\n"
	":Param data: a raw data object created from a load_grating() call\n"
	":rtype None:"
    },
{   
        "debug_dump_grating", py_debugdumpgrating, METH_VARARGS,
        "Dumps a loaded grating in filename.\n"
	":Param fb0: a framebuffer object created from an init() call\n"
	":Param data: a raw data object created from a load_grating() call\n"
	":Param filename: file to dump the grating in.\n"
	":rtype None:"
    },
{
	"display_raw", py_displayraw, METH_VARARGS,
	":rtype None:"
},  
    {   
        "build_grating", py_buildgrating, METH_VARARGS,
        "Creates a raw animation file of a drifting grating.\n"
	":Param filename:\n"
    	":Param angle: angle of propogation of the drifting grating (in\n"
	"      degrees anticlockwise from the x-axis).\n"
    	":Param sf: Spacial frequency in cycles per degree of visual angle\n"
	":Param tf: Temporal frequency in cycles per second\n"
    	":Param width: X component of the desired resolution\n"
	":Param height: Y component of the desired resolution\n"
	":Param waveform: SINE or SQUARE\n"
	":Param percent_diameter: 0 for full screen or width of circlular mask\n"
	":rtype None:\n\n"
	"NOTE: the resolution of this file must match the resolution used\n"
	"in init() calls that are used to display this file."
    },  
    {   
        "load_grating", py_loadgrating, METH_VARARGS,
        "Loads a raw animation file into memory for use with\n"
	"The display_grating function\n"
	":Param fb0: a framebuffer object returned from init()\n"
	":Param filename: (string) the raw data file to be loaded\,\n"
	"      typically created with a draw_grating call.\n"
	":rtype grating_data capsule: The raw data object."
    },
    {
	"load_raw", py_loadraw, METH_VARARGS,
	":rtype raw_data capsule"
    },  
    {   
        "unload_grating", py_unloadgrating, METH_VARARGS,
        "Unloads raw animation data, freeing the assosiated memory\n"
	":Param data: a grating_data object returned from load_grating()\n"
	":rtype None:"
    },  
   {
	"unload_raw", py_unloadraw, METH_VARARGS,
	":rtype None:"
   }, 
   {   
        "close_display", py_closedisplay, METH_VARARGS,
        "Destroy/uninitialise a framebuffer object.\n"
	":Param fb0: an initialised framebuffer object\n"
	":rtype None:"
    },  
    {   
	"convertraw", py_convertraw, METH_VARARGS,
	"fillertext\n"
	":type None:"
    },
    {NULL, NULL, 0, NULL}
};


static struct PyModuleDef _rpigratings_definition = { 
    PyModuleDef_HEAD_INIT,
    "_rpigratings",
    "A Python module that displays drifting gratings\n"
    "on Raspberry Pis. This module should be used from\n"
    "the terminal rather than a windowing system for\n"
    "intended behaviour. The terminal can be accessed\n"
    "with Ctrl+Alt+F1 on raspbian (Ctrl+Alt+F7 will return\n"
    "to the windowing system).\n\n"
    "Typical usage:\n"
    " >>> import _rpigratings as rg\n"
    " >>> #First create a grating file...\n"
    " >>> rg.draw_grating(\"grat_file\",60,0.5,3,1680,1050)\n"
    " >>> #Then initialise the display...\n"
    " >>> root = rg.init(1680,1050)\n"
    " >>> #Then display midgrey while loading our grating file\n"
    " >>> rg.display_color(root,127,127,127)\n"
    " >>> my_grating = rg.load_grating(root,\"grat_file\")\n"
    " >>> #Now display the loaded grating...\n"
    " >>> rg.diplay_grating(root, my_grating)\n"
    " >>> #Free the memory assosiated with the grating...\n"
    " >>> rg.unload_grating(my_grating)\n"
    " >>> #Remember: uninitialise module to restore screen settings\n"
    " >>> rg.close_display(root)\n",
    -1, 
    _rpigratings_methods
};


PyMODINIT_FUNC PyInit__rpigratings(void) {
    return PyModule_Create(&_rpigratings_definition);
}
