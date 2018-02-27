//modified by: Derrick Alden
//date:
//purpose:
//
//author:  Gordon Griesel
//date:    Fall 2016
//         Spring 2018
//terrain rendering with procedural terrain generation.
//
#include <fstream>
using namespace std;
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include "log.h"
#include "fonts.h"

//#define SHOW_QUAD_ONLY
#define ALLOW_EROSION
#define ALLOW_SMOOTHING
#define USE_SRTM

typedef float Flt;
typedef Flt Vec[3];
typedef Flt	Matrix[3][3];
//some defined macros
#define MakeVector(x, y, z, v) (v)[0]=(x),(v)[1]=(y),(v)[2]=(z)
#define VecNegate(a)	(a)[0]=(-(a)[0]); (a)[1]=(-(a)[1]); (a)[2]=(-(a)[2]);
#define VecDot(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VecLen(a)	((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2])
#define VecLenSq(a)	sqrtf((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2])
#define VecCopy(a,b)	(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];
#define VecAdd(a,b,c) (c)[0]=(a)[0]+(b)[0];\
                      (c)[1]=(a)[1]+(b)[1];\
                      (c)[2]=(a)[2]+(b)[2]
#define VecSub(a,b,c) (c)[0]=(a)[0]-(b)[0];\
                      (c)[1]=(a)[1]-(b)[1];\
                      (c)[2]=(a)[2]-(b)[2]
#define VecS(A,a,b) (b)[0]=(A)*(a)[0]; (b)[1]=(A)*(a)[1]; (b)[2]=(A)*(a)[2]
#define VecAddS(A,a,b,c) (c)[0]=(A)*(a)[0]+(b)[0];\
                         (c)[1]=(A)*(a)[1]+(b)[1];\
                         (c)[2]=(A)*(a)[2]+(b)[2]
#define VecCross(a,b,c) (c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1];\
                        (c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2];\
                        (c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0]
#define VecZero(v) (v)[0]=0.0;(v)[1]=0.0;v[2]=0.0
#define ABS(a) (((a)<0)?(-(a)):(a))
#define SGN(a) (((a)<0)?(-1):(1))
#define SGND(a) (((a)<0.0)?(-1.0):(1.0))
#define rnd() (float)rand() / (float)RAND_MAX
#define PI 3.14159265358979323846264338327950
#define MY_INFINITY 1000.0


void init();
void init_opengl();
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void physics();
void render();
Flt vecNormalize(Vec vec);

#ifdef USE_SRTM
const int twidth = 600;
#else //USE_SRTM
const int twidth = 124;
#endif //USE_SRTM

class Global {
public:
	int xres, yres;
	int erode;
	int smooth;
	Flt aspectRatio;
	Vec cameraPosition;
	int cameraDirectionFlag;
	Vec cameraDirection;
	GLfloat lightPosition[4];
	Flt vert[twidth+1][twidth+1];
	Flt color[twidth][twidth][3][2]; //4D array!
	Flt altScale;
	Flt increment;
	Global() {
		//constructor
		xres = 640;
		yres = 480;
		erode = 0;
		smooth = 0;
		cameraDirectionFlag = 0;
		MakeVector(0.0, -0.5, -1.0, cameraDirection);
		vecNormalize(cameraDirection);
		aspectRatio = (GLfloat)xres / (GLfloat)yres;
		MakeVector(0.0, 6.0, 9.0, cameraPosition);
		//light is up high, right a little, toward a little
		MakeVector(100.0f, 100.0f, 10.0f, lightPosition);
		lightPosition[3] = 1.0f;
		#ifndef SHOW_QUAD_ONLY
		for (int i=0; i<twidth+1; i++) {
			for (int j=0; j<twidth+1; j++) {
				vert[j][i] = 0.0;
			}
		}
		//define colors for each triangle.
		for (int i=0; i<twidth; i++) {
			for (int j=0; j<twidth; j++) {
				color[j][i][0][0] = rnd();
				color[j][i][0][1] = rnd();
				color[j][i][0][2] = rnd();
				color[j][i][1][0] = rnd();
				color[j][i][1][1] = rnd();
				color[j][i][1][2] = rnd();
			}
		}
		#endif //SHOW_QUAD_ONLY
		altScale = 1.0 / 256.0;
		increment = 0.05;
	}
} g;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	X11_wrapper() {
		//Look here for information on XVisualInfo parameters.
		//http://www.talisman.org/opengl-1.1/Reference/glXChooseVisual.html
		//
		GLint att[] = { GLX_RGBA,
						GLX_STENCIL_SIZE, 2,
						GLX_DEPTH_SIZE, 24,
						GLX_DOUBLEBUFFER, None };
		//GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
		//GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, None };
		//XVisualInfo *vi;
		XSetWindowAttributes swa;
		setup_screen_res(640, 480);
		dpy = XOpenDisplay(NULL);
		if (dpy == NULL) {
			printf("\n\tcannot connect to X server\n\n");
			exit(EXIT_FAILURE);
		}
		Window root = DefaultRootWindow(dpy);
		XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
		if (vi == NULL) {
			printf("\n\tno appropriate visual found\n\n");
			exit(EXIT_FAILURE);
		} 
		Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		swa.colormap = cmap;
		swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
			StructureNotifyMask | SubstructureNotifyMask;
		win = XCreateWindow(dpy, root, 0, 0, g.xres, g.yres, 0,
			vi->depth, InputOutput, vi->visual,
			CWColormap | CWEventMask, &swa);
		set_title();
		glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
		glXMakeCurrent(dpy, win, glc);
	}
	~X11_wrapper() {
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
	}
	void reshape_window(int width, int height) {
		//window has been resized.
		setup_screen_res(width, height);
		//
		glViewport(0, 0, (GLint)width, (GLint)height);
		glMatrixMode(GL_PROJECTION); glLoadIdentity();
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
		glOrtho(0, g.xres, 0, g.yres, -1, 1);
		set_title();
	}
	void set_title() {
		//Set the window title bar.
		XMapWindow(dpy, win);
		XStoreName(dpy, win, "3480 - terrain demo");
	}
	void setup_screen_res(const int w, const int h) {
		g.xres = w;
		g.yres = h;
		g.aspectRatio = (GLfloat)g.xres / (GLfloat)g.yres;
	}
	void check_resize(XEvent *e) {
		//The ConfigureNotify is sent by the
		//server if the window is resized.
		if (e->type != ConfigureNotify)
			return;
		XConfigureEvent xce = e->xconfigure;
		if (xce.width != g.xres || xce.height != g.yres) {
			//Window size did change.
			reshape_window(xce.width, xce.height);
		}
	}
	bool getXPending() {
		return XPending(dpy);
	}
	XEvent getXNextEvent() {
		XEvent e;
		XNextEvent(dpy, &e);
		return e;
	}
	void swapBuffers() {
		glXSwapBuffers(dpy, win);
	}
} x11;


int main()
{
	init_opengl();
	#ifdef USE_SRTM
	void init_srtm();
	init_srtm();
	#endif //USE_SRTM
	int done = 0;
	while (!done) {
		while (x11.getXPending()) {
			XEvent e = x11.getXNextEvent();
			x11.check_resize(&e);
			check_mouse(&e);
			done = check_keys(&e);
		}
		physics();
		render();
		x11.swapBuffers();
		usleep(2000);
	}
	cleanup_fonts();
	return 0;
}

#ifdef USE_SRTM
void init_srtm()
{
	printf("init_srtm()...\n");
	//Convert png to ppm.
	//Convert to P3 (ascii)
	//Resize to width of 600 while maintaining aspect ratio
	system("convert N35W119.png -compress none -resize 600 temp.ppm");
	//Open the ppm file.
	ifstream fin("temp.ppm");
	//read past header...
	char ts[32];
	int val;
	fin >> ts;
	printf("%s\n", ts);
	int w, h, m;
	fin >> w >> h >> m;
	printf("%i %i %i\n", w, h, m);
	//read height data
	for (int i=0; i<h; i++) {
		for (int j=0; j<w; j++) {
			fin >> val >> val >> val;
			g.vert[i][j] = (Flt)val;
			g.vert[i][j] *= g.altScale;
		}
	}
	fin.close();
	unlink("temp.ppm");
	//srtmNorm = new Vec [srtmWidth*srtmWidth];
}
#endif //USE_SRTM

void init(void)
{
	g.aspectRatio = (GLfloat)g.xres / (GLfloat)g.yres;
	//light is up high, right, and toward us
	MakeVector(100.0f, 240.0f, 40.0f, g.lightPosition);
	g.lightPosition[3] = 1.0f;
}

void init_opengl(void)
{
	//OpenGL initialization
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, g.aspectRatio, 0.1f, 100.0f);
	glMatrixMode(GL_MODELVIEW);
	//Enable this so material colors are the same as vert colors.
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	//Turn on a light
	glLightfv(GL_LIGHT0, GL_POSITION, g.lightPosition);
	glEnable(GL_LIGHT0);
	//Do this to allow fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();
	//init_textures();
}

Flt vecNormalize(Vec vec) {
	Flt len = vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2];
	if (len == 0.0) {
		//return a vector of length 1.
		MakeVector(0.0,0.0,1.0,vec);
		return 1.0;
	}
	len = sqrt(len);
	Flt tlen = 1.0 / len;
	vec[0] *= tlen;
	vec[1] *= tlen;
	vec[2] *= tlen;
	return len;
}

void identity(Matrix m) {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			m[i][j] = 0.0;
			if (i ==j)
				m[i][j] = 1.0;
		}
	}
}

void trans_mat(Flt angle, Matrix m) {
	Flt s = sin(angle);
	Flt c = cos(angle);
	m[0][0] = c;
	m[0][2] = -s;
	m[2][0] = s;
	m[2][2] = c;
}

void transform_vector(Matrix m, Vec in, Vec out) {
	Flt f0 = m[0][0] * in[0] + m[1][0] * in[1] + m[2][0] * in[2];
	Flt f1 = m[0][1] * in[0] + m[1][1] * in[1] + m[2][1] * in[2];
	Flt f2 = m[0][2] * in[0] + m[1][2] * in[1] + m[2][2] * in[2];
	out[0] = f0;
	out[1] = f1;
	out[2] = f2;
}

void check_mouse(XEvent *e)
{
	//Did the mouse move?
	//Was a mouse button clicked?
	static int savex = 0;
	static int savey = 0;
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button is down
		}
		if (e->xbutton.button==3) {
			//Right button is down
		}
	}
	if (savex != e->xbutton.x || savey != e->xbutton.y) {
		//Mouse moved
		savex = e->xbutton.x;
		savey = e->xbutton.y;
	}
}

int check_keys(XEvent *e)
{
	//Was there input from the keyboard?
	if (e->type == KeyPress) {
		int key = XLookupKeysym(&e->xkey, 0);
		switch(key) {
			case XK_e:
				g.erode ^= 1;
				break;
			case XK_s:
				g.smooth = 1;
				break;
			case XK_d:
				g.cameraDirectionFlag ^= 1;
				break;
			case XK_f: {
				//camera forward
				Flt val = g.increment * 10.0;
				Vec v;
				VecCopy(g.cameraDirection, v);
				VecS(val, v, v);
				VecAdd(g.cameraPosition, v, g.cameraPosition);
				break;
			}
			case XK_b: {
				//camera backward
				Flt val = g.increment * 10.0;
				Vec v;
				VecCopy(g.cameraDirection, v);
				VecS(val, v, v);
				VecSub(g.cameraPosition, v, g.cameraPosition);
				break;
			}
			case XK_Right:
				if (g.cameraDirectionFlag) {
					//rotate camera direction to the right.
					//rotate on the Y axis.
					Matrix m;
					identity(m);
					Flt val = -g.increment;
					trans_mat(val, m);
					transform_vector(m, g.cameraDirection, g.cameraDirection);
				} else {
					//move right in relation to the camera direction
					//get a right vector...
					Vec up = {0.0, 1.0, 0.0};
					Vec right;
					VecCross(g.cameraDirection, up, right);
					Flt val = g.increment;
					VecS(val, right, right);
					VecAdd(g.cameraPosition, right, g.cameraPosition);
				}
				break;
			case XK_Left:
				if (g.cameraDirectionFlag) {
					//rotate camera direction to the left.
					//rotate on the Y axis.
					Matrix m;
					identity(m);
					Flt val = g.increment;
					trans_mat(val, m);
					transform_vector(m, g.cameraDirection, g.cameraDirection);
				} else {
					//move left in relation to the camera direction
					//get a left vector...
					Vec up = {0.0, 1.0, 0.0};
					Vec left;
					VecCross(up, g.cameraDirection, left);
					Flt val = g.increment;
					VecS(val, left, left);
					VecAdd(g.cameraPosition, left, g.cameraPosition);
				}
				break;
			case XK_Up:
				if (g.cameraDirectionFlag) {
					//rotate camera direction upward.
					//simplify by lifting the y vector component upward.
					g.cameraDirection[1] -= g.increment;
					vecNormalize(g.cameraDirection);
				} else {
					g.cameraPosition[1] += g.increment;
				}
				break;
			case XK_Down:
				if (g.cameraDirectionFlag) {
					//rotate camera direction upward.
					//simplify by lowering the y vector component downward.
					g.cameraDirection[1] += g.increment;
					vecNormalize(g.cameraDirection);
				} else {
					g.cameraPosition[1] -= g.increment;
				}
				break;
			case XK_equal:
				g.increment *= 1.1;
				break;
			case XK_minus:
				g.increment *= (1.0 / 1.1);
				break;
			case XK_Escape:
				return 1;
		}
	}
	return 0;
}

Flt triangleArea(Flt tri[3][2])
{
	//http://www.mathopenref.com/coordtrianglearea.html
	//Find the area of a triangle given 3 integer vertices.
	//note: area might not be an integer.
	//area = abs(ax*(by-cy) + bx*(cy-ay) + cx*(ay-by)) / 2)
	Flt area =
		tri[0][0] * (tri[1][1]-tri[2][1]) +
		tri[1][0] * (tri[2][1]-tri[0][1]) +
		tri[2][0] * (tri[0][1]-tri[1][1]);
	//area = abs(area);
	return area * 0.5;
}

int getBarycentric(Vec v0, Vec v1, Vec v2, Flt x, Flt y, Flt *u, Flt *v)
{
	//Define triangle and sub-triangles
	Flt t0[3][2]={{v0[0],v0[1]}, {v1[0],v1[1]}, {v2[0],v2[1]}};
	Flt t1[3][2]={{x,    y},     {v1[0],v1[1]}, {v2[0],v2[1]}};
	Flt t2[3][2]={{x,    y},     {v2[0],v2[1]}, {v0[0],v0[1]}};
	//Find areas
	Flt areaABC = triangleArea(t0);
	Flt areaPBC = triangleArea(t1);
	Flt areaPCA = triangleArea(t2);
	//Log("areaABC: %lf %lf %lf\n",areaABC,areaPBC,areaPCA);
	if (areaABC == 0.0) {
		*u = *v = 0.0;
		return 0;
	}
	//Calculate alpha, beta, gamma
	*u = areaPBC / areaABC;
	*v = areaPCA / areaABC;
	//
	//Return an integer indicating in or out...
	return (*u >= 0.0 && *v >= 0.0 && *u+*v <= 1.0);
}

void physics(void)
{
	#ifdef SHOW_QUAD_ONLY
	return;
	#endif //SHOW_QUAD_ONLY
	//
	#ifdef ALLOW_EROSION
	//Erode terrain
	//choose a point and normal vector
	if (g.erode) {
		int pt[3] = { rand() % (twidth+1), 0, rand() % (twidth+1) };
		Vec norm = { rnd()-0.5f, 0.0, rnd()-0.5f };
		//vecNormalize(norm);
		Vec v;
		for (int i=0; i<=twidth; i++) {
			for (int j=0; j<=twidth; j++) {
				v[0] = pt[0] - i;
				v[1] = 0;
				v[2] = pt[2] - j;
				Flt dot = VecDot(v, norm);
				//printf("dot: %f\n",dot);
				if (dot > 0.0)
					g.vert[i][j] += 0.015;
				else
					g.vert[i][j] -= 0.015;
			}
		}
	}
	#endif //ALLOW_EROSION
	#ifdef ALLOW_SMOOTHING
	if (g.smooth) {
		Flt t[twidth+1][twidth+1];
		for (int i=0; i<=twidth; i++)
			for (int j=0; j<=twidth; j++)
				t[i][j] = g.vert[i][j];
		for (int i=0; i<=twidth; i++) {
			for (int j=0; j<=twidth; j++) {
				//sum 8 surrounding points
				Flt sum=0.0;
				Flt div=0.0;
				if (i>0 && j>0) {
					sum += g.vert[i-1][j-1]*0.5;
					div += 0.5;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				if (i>0) {
					sum += g.vert[i-1][j];
					div += 1.0;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				if (i>0 && j<twidth) {
					sum += g.vert[i-1][j+1]*0.5;
					div += 0.5;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				if (j>0) {
					sum += g.vert[i][j-1];
					div += 1.0;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				sum += g.vert[i][j];
				div += 1.0;
				if (j<twidth) {
					sum += g.vert[i][j+1];
					div += 1.0;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				if (i<twidth && j>0) {
					sum += g.vert[i+1][j-1]*0.5;
					div += 0.5;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				if (i<twidth) {
					sum += g.vert[i+1][j];
					div += 1.0;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				if (i<twidth && j<twidth) {
					sum += g.vert[i+1][j+1]*0.5;
					div += 0.5;
				} else {
					sum += g.vert[i][j];
					div += 1.0;
				}
				t[i][j] = sum/div;
			}
		}
		for (int i=0; i<=twidth; i++)
			for (int j=0; j<=twidth; j++)
				g.vert[i][j] = t[i][j];
		g.smooth=0;
	}
	#endif //ALLOW_SMOOTHING
}

void drawFloor()
{
	#ifdef SHOW_QUAD_ONLY
	glPushMatrix();
	//brown beach colors
	glColor3f(1.0f, 1.0f, 0.0f);
	float h = 0.0;
	float w = 5.0;
	float d = 5.0;
	glBegin(GL_QUADS);
		//flat surface
		glNormal3f( 0.0f, 1.0f, 0.0f);
		glVertex3f( w, h, -d);
		glVertex3f(-w, h, -d);
		glVertex3f(-w, h,  d);
		glVertex3f( w, h,  d);
	glEnd();
	glPopMatrix();
	//
	#else //SHOW_QUAD_ONLY
	//
	glPushMatrix();
	glTranslatef(-5, 0.0f, -5);
	Flt step = 10.0 / twidth;
	for (int i=0; i<twidth; i++) {
		for (int j=0; j<twidth; j++) {
			glBegin(GL_TRIANGLES);
				glColor3f(1.0, 0.8, 0.5);
				//new scope
				{
					Vec v0 = { j*step, g.vert[i][j], i*step };
					Vec v1 = { (j+1)*step, g.vert[i][j+1], i*step };
					Vec v2 = { j*step,     g.vert[i+1][j], (i+1)*step };
					Vec v3,v4;
					VecSub(v0, v1, v3);
					VecSub(v2, v1, v4);
					Vec v5;
					VecCross(v3,v4,v5);
					vecNormalize(v5);
					glNormal3fv(v5);
				}
				//change color here
				//printf("g.vert[i][j] = %i\n" g.vert[i][j]);
				//printf("%f", g.vert[i][j]);
				if(g.vert[i][j] > .5) {
			
					//g.vert[i][j] *= g.altScale;
					glColor3f(1.0, 0.0, 0.0);
				}
				//low elevation is blue water
				//in values between .4 and .5 are brown beach colors
				if(g.vert[i][j] <= .4) {
					//g.vert[i][j] *= g.altScale;
					glColor3f(0.0, 0.0, 1.0);
				}
				glVertex3f(j*step,     g.vert[i][j], i*step);
				glVertex3f((j+1)*step, g.vert[i][j+1], i*step);
				glVertex3f(j*step,     g.vert[i+1][j], (i+1)*step);
				//new scope
				{
					Vec v0 = { (j+1)*step, g.vert[i][j+1], i*step };
					Vec v1 = { (j+1)*step, g.vert[i+1][j+1], (i+1)*step };
					Vec v2 = { j*step,     g.vert[i+1][j], (i+1)*step };
					Vec v3,v4;
					VecSub(v0, v1, v3);
					VecSub(v2, v1, v4);
					Vec v5;
					VecCross(v3,v4,v5);
					vecNormalize(v5);
					glNormal3fv(v5);
				}
				glVertex3f((j+1)*step, g.vert[i][j+1], i*step);
				glVertex3f((j+1)*step, g.vert[i+1][j+1], (i+1)*step);
				glVertex3f(j*step,     g.vert[i+1][j], (i+1)*step);
			glEnd();
		}
	}
	glPopMatrix();
	#endif //SHOW_QUAD_ONLY
}

void render(void)
{
	Rect r;
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	//
	//3D mode
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, g.aspectRatio, 0.1f, 100.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	//
	//place the camera
	//
	//set the camera look-at spot by using the camera direction.
	Vec spot;
	VecAdd(g.cameraPosition, g.cameraDirection, spot);
	//
	//gluLookAt(camera position, look at, up vector)
	gluLookAt(g.cameraPosition[0],g.cameraPosition[1],g.cameraPosition[2],
				spot[0], spot[1], spot[2],
				0,1,0);
	glLightfv(GL_LIGHT0, GL_POSITION, g.lightPosition);
	//
	//do in class:
	//rotate the terrain quad
	//
	drawFloor();
	//
	//switch to 2D mode
	glViewport(0, 0, g.xres, g.yres);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, g.xres, 0, g.yres);
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_LIGHTING);
	r.bot = g.yres - 20;
	r.left = 10;
	r.center = 0;
	ggprint8b(&r, 16, 0x0088ff88, "<Esc> Quit");
	char ts[100];
	sprintf(ts, "D - Change camera %s", (g.cameraDirectionFlag)?"(ACTIVE)":"");
	ggprint8b(&r, 20, 0x0088ff88, ts);
	//
	//barycentric coordinates to find lat/lon.
	Flt lat = g.cameraPosition[2];
	Flt lon = g.cameraPosition[0];
	Flt u,v,w;
	Vec v0 = {-5.0, -5.0, 0.0};
	Vec v1 = { 5.0, -5.0, 0.0};
	Vec v2 = { 5.0,  5.0, 0.0};
	if (getBarycentric(v0, v1, v2, lon, lat, &u, &v)) {
		w = 1.0 - u - v;
		lon = (-119.0*u) + (-118.0*v) + (-118.0*w);
		lat = (36.0*u) + (36.0*v) + (35.0*w);
  	}
	sprintf(ts, "camera lat: %3.6f  lon: %3.6f", lat, lon);
	ggprint8b(&r, 16, 0x00ffff88, ts);
	//
	sprintf(ts, "increment: %2.6f", g.increment);
	ggprint8b(&r, 16, 0x00ffff88, ts);
	//
	glPopAttrib();
}



