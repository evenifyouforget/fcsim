extern "C" {

#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <pthread.h>
#include "poocs.h"
#include "unused.h"

void key_down(int key);
void key_up(int key);
void move(int x, int y);
void button_down(int button);
void button_up(int button);
void scroll(int delta);
void resize(int w, int h);
void init(char *xml, int len);
void draw(void);

void process_events(Display *dpy)
{
	XEvent xev;

	while (XPending(dpy) > 0) {
		XNextEvent(dpy, &xev);

		switch (xev.type) {
		case Expose:
			resize(xev.xexpose.width, xev.xexpose.height);
			break;
		case KeyPress:
			key_down(xev.xkey.keycode);
			break;
		case KeyRelease:
			key_up(xev.xkey.keycode);
			break;
		case ButtonPress:
			if (xev.xbutton.button == 4)
				scroll(+1);
			else if (xev.xbutton.button == 5)
				scroll(-1);
			else
				button_down(xev.xbutton.button);
			break;
		case ButtonRelease:
			if (xev.xbutton.button != 4 && xev.xbutton.button != 5)
				button_up(xev.xbutton.button);
			break;
		case MotionNotify:
			move(xev.xmotion.x, xev.xmotion.y);
			break;
		}
	}
}

struct slot {
	void (*func)(void *arg);
	void *arg;
	int delay;
	uint64_t next_ts;
};

struct slot slots[4];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void ts_to_time(uint64_t ts, struct timespec *spec)
{
	spec->tv_sec = ts / 1000;
	spec->tv_nsec = (ts % 1000) * 1000000;
}

void time_to_ts(struct timespec *spec, uint64_t *ts)
{
	*ts = (uint64_t)spec->tv_sec * 1000 + spec->tv_nsec / 1000000;
}

// we need to pass this function pointer to something,
// which expects a specific signature.
// there is no error.
void *func(UNUSED void *arg)
{
	struct timespec this_time;
	struct timespec next_time;
	uint64_t this_ts;
	uint64_t next_ts = (uint64_t)-1;
	int i;

	pthread_mutex_lock(&mutex);
	while (1) {
		if (next_ts == (uint64_t)-1) {
			pthread_cond_wait(&cond, &mutex);
		} else {
			ts_to_time(next_ts, &next_time);
			pthread_cond_timedwait(&cond, &mutex, &next_time);
		}
		clock_gettime(CLOCK_REALTIME, &this_time);
		time_to_ts(&this_time, &this_ts);
		for (i = 0; i < 4; i++) {
			if (slots[i].func && slots[i].next_ts <= this_ts) {
				slots[i].func(slots[i].arg);
				slots[i].next_ts += slots[i].delay;
			}
		}
		next_ts = (uint64_t)-1;
		for (i = 0; i < 4; i++) {
			if (slots[i].func && slots[i].next_ts < next_ts)
				next_ts = slots[i].next_ts;
		}
	}
}

int set_interval(void (*func)(void *arg), int delay, void *arg)
{
	struct timespec this_time;
	uint64_t this_ts;
	int i;

	for (i = 0; i < 4; i++) {
		if (!slots[i].func)
			break;
	}
	if (i == 4)
		return -1;

	clock_gettime(CLOCK_REALTIME, &this_time);
	time_to_ts(&this_time, &this_ts);

	slots[i].func = func;
	slots[i].arg = arg;
	slots[i].delay = delay;
	slots[i].next_ts = this_ts + delay;

	pthread_cond_signal(&cond);

	return i;
}

void clear_interval(int i)
{
	slots[i].func = NULL;
}

} // extern "C"

int main(void)
{
	Display *dpy;
	Window root;
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	XVisualInfo *vi;
	Colormap cmap;
	XSetWindowAttributes swa;
	Window win;
	GLXContext glc;
	pthread_t thread;

	pthread_create(&thread, NULL, func, NULL);
	pthread_detach(thread);

	dpy = XOpenDisplay(NULL);
	if (!dpy)
		return 1;

	root = DefaultRootWindow(dpy);

	vi = glXChooseVisual(dpy, 0, att);
	if (!vi)
		return 1;

	cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

	swa.colormap = cmap;
	swa.event_mask = ExposureMask |
			 KeyPressMask |
			 KeyReleaseMask |
			 ButtonPressMask |
			 ButtonReleaseMask |
			 PointerMotionMask;

	win = XCreateWindow(dpy, root,
			    0, 0, 600, 600,
			    0,
			    vi->depth,
			    InputOutput,
			    vi->visual,
			    CWColormap | CWEventMask,
			    &swa);

	XMapWindow(dpy, win);

	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);

	init(poocs_xml, sizeof(poocs_xml));

	while (1) {
		pthread_mutex_lock(&mutex);
		draw();
		pthread_mutex_unlock(&mutex);

		glFinish();
		glXSwapBuffers(dpy, win);

		pthread_mutex_lock(&mutex);
		process_events(dpy);
		pthread_mutex_unlock(&mutex);
	}
}