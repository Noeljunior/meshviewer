/* STDLIBS */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

/* GLLIBS */
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "meshviewer.h"

#define WINTITLE        "Mesh Viewer"
#define ERRPRE          "MVERROR:"
#define printe(...)     fprintf(stderr, __VA_ARGS__)
#define printerr(err)   printe("%s %s\n", ERRPRE, err)
#define INFOPRE         "MVINFO:"
#define printi(...)     printf(__VA_ARGS__)
#define printinfo(info) printi("%s %s\n", INFOPRE, info)
#define KDOWN           0
#define KUP             1

/* TODO list

    * destroy a plot!
    * do not return mv_add until var n is defined
    * a flag so it can receive an colour array-per-vertex
    * update an un object: colour array
    * DO NOT FREE THE INPUT ARRAYS
    * add multiple layer support
    * set background colour
    * ZOOM/UNZOOM TO THE CURSOR
    * legend


    - keybind to std zoom
    - double click to zoom at click
    - free everything when stoping
    - make number of objects dynamic
    - zoom min and zoom max
    - pan min/max
    - write help
    
    - add posibility of passing the borders to set custom colour
    - emit render event after rendering a frame
*/

/* * * * * * * * * * TYPES * * * * * * * * * */
typedef enum _evtype {
    /* objects events */
    EV_CREATEOBJ     = 1,
    EV_SETPROPERTY   = 2,
    EV_DESTROYOBJ    = 3,

    /* render events */
    EV_RENDER         = 100,
} evtype;

typedef struct object {
    GLuint      vao,
                vbo, cbo, ibo,
                ibos;
    GLenum      mode;
    GLfloat     width;
} object;

typedef struct program {
    GLuint      shaderprogram,
                vertexshader,
                fragmentshader;
} program;

typedef struct renderobj {
    program     *prog;
    object      *obj;
    GLfloat     scale,
                translate[2],
                rotate;
    int         enable;
    int         layer;
} renderobj;
typedef enum ev_esp { EV_SP_VISIBILITY, EV_SP_SCALE, EV_SP_TRANSLATE, EV_SP_ROTATE, EV_SP_LAYER } ev_esp;


/* * * * * * * * * * GL ENV * * * * * * * * * */
SDL_Window *        window;
int                 running;
GLint               curWidth  = MV_WINW,    /* Window size */
                    curHeight = MV_WINH;
int                 fpscount  = 0;          /* FPS counter */
pthread_t           renderthread;
unsigned long long  totalpoints = 0;
int                 MAXLAYERS = 1;

/* * * * * * * * * * EVENTS * * * * * * * * * */
void (*ev_callbacks[256]) (void* data1, void* data2);

/* * * * * * * * * * RENDER * * * * * * * * * */
program *           defprog[16];
renderobj *         renderobjs[MV_MAXOBJECTS];
int                 allocatedRO = 0;
/* SPECIFIC RENDER OBJECTS */
/* the selection box */
renderobj *         ro_selbox;
int                 sb_isshow,
                    sb_ix, sb_iy,
                    sb_fx, sb_fy;
/* Keyboard and mouse */
int                 modshift, modctrl, modalt;
/* Camera position and zoom */
double              cx,  cy,
                    ctx, cty;
double              cz,  ctz;
double              az,  ap;
float               zoom_factor = MV_ZOOMFACTOR;


/* * * * * * * * * * HEADERS * * * * * * * * * */
void *          inthread        (void * none);
void            ev_emit         (int code, void* data1, void* data2);
void            ev_add          (int code, void (*func) (void*, void*));
void            ev_remove       (int code);
void            ev_keyboard     (int type, SDL_Keysym keysym);
void            ev_mouse        (SDL_Event event);
void            ev_renderloop   (void* data1, void* data2);
void            ev_createobj    (void* data1, void* data2);
void            eve_createobj(GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti, GLfloat width, int layer,  int * id);
void            ev_setproperty  (void* data1, void* data2);
void            eve_setproperty (ev_esp change, unsigned int id, char visibility, float scale, float translatex, float translatey, float rotate, int layer);
void            ev_destroyobj(void* data1, void* data2);
void            eve_destroyobj(int n);

Uint32          timer_countfps  (Uint32 interval, void* param);

void            renderloop      ();
void            render_ro       (renderobj * ro);
int             add_ro          (renderobj * ro);
program *       new_program     (const char * vertexsource, const char * fragmentsource);
program *       new_program_default();
object *        new_buffer      (GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti);

renderobj *     new_ro_selbox   ();
void            sb_dozoom       ();

void            transform_triangles_in_lines(GLuint ** pindices, unsigned int * pcounti, int countv);
void            transform_triangles_in_points(GLuint ** pindices, unsigned int * pcounti, int countv);

/* * * * * * * * * * * * * * * * * *
 *  THE MESH VIEWER
 * * * * * * * * * * * * * * * * * */
void mv_start() {
    pthread_create(&renderthread, NULL, inthread, 0);
}

void mv_start_layers(int maxlayers) {
    MAXLAYERS = maxlayers > 1 ? maxlayers : 1;
    pthread_create(&renderthread, NULL, inthread, 0);
}

void mv_wait() {
    pthread_join(renderthread, NULL);
}

void mv_stop() {
    running = 0;
}

void * inthread(void * none) {
    printinfo("Working on separated thread");
    mv_init();
    mv_draw();
    printinfo("Leaving render thread");
    pthread_exit(NULL);
}

void mv_init() {
    /* Init the GL env */
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    /* SDL WINDOW */
    window = SDL_CreateWindow(WINTITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        curWidth, curHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_GL_CreateContext(window);

    /* * * * Init GLGlew * * * */
    GLenum result = glewInit();
    if (result != GLEW_OK) {
        printe("%s %s\n", ERRPRE, "Glew error. Exiting");
        exit(-1);
    }
    /*printi("%s %s %s\n", INFOPRE, "Using OpenGL version", glGetString(GL_VERSION));*/
    
    /* * * * Init OpenGL * * * */
    glEnable(GL_DEPTH_TEST | GL_LINE_SMOOTH | GL_POLYGON_SMOOTH | GL_ALPHA_TEST);
    glClearColor(0,0,0,1);
    
    glDepthMask(GL_FALSE);
    glEnable   (GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    /* SDL register timers */
    SDL_AddTimer(1000, timer_countfps, 0);
    
    /* SDL Register the events */
    ev_add(EV_RENDER,      ev_renderloop);
    ev_add(EV_CREATEOBJ,   ev_createobj);
    ev_add(EV_SETPROPERTY, ev_setproperty);
    ev_add(EV_DESTROYOBJ,  ev_destroyobj);
    
    
    /* Prepare the render stuff */
    ctz = cz = MV_ZOOMDEFAULT; /* TODO make the zoom somekind auto - based on mins maxs? */
    defprog[0] = new_program_default();
    ro_selbox = new_ro_selbox();
}

void mv_init_layers(int maxlayers) {
    MAXLAYERS = maxlayers > 1 ? maxlayers : 1;
    mv_init();
}

void mv_draw() {
    running = 1;
    SDL_Event event;

    ev_emit(EV_RENDER, NULL, NULL);

    while (running && SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_USEREVENT:
                if (ev_callbacks[event.user.code])        /* is it allocated? */
                    (*ev_callbacks[event.user.code])(event.user.data1, event.user.data2);
                break;


            case SDL_KEYDOWN: if (!event.key.repeat) ev_keyboard(KDOWN, event.key.keysym); break;
            case SDL_KEYUP:                          ev_keyboard(KUP,   event.key.keysym); break;
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:                     ev_mouse(event);                      break;


            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) { /* Resized */
                    curWidth  = event.window.data1,
                    curHeight = event.window.data2;
                }
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
                break;
            case SDL_QUIT:                           running = 0;                          break;
            default: break;
        }
    }
    SDL_Quit();
}



/* * * * * * * * * * * * * * * * * *
 *  THE EVENTS
 * * * * * * * * * * * * * * * * * */
void ev_add(int code, void (*func) (void*, void*)) {
    ev_callbacks[code] = func;
}
void ev_remove(int code) {
    ev_callbacks[code] = NULL;
}
void ev_emit(int code, void* data1, void* data2) {
    SDL_Event event;
    event.type       = SDL_USEREVENT;
    event.user.code  = code;
    event.user.data1 = data1;
    event.user.data2 = data2;
    SDL_PushEvent(&event);
}

int eegg = 0;
void ev_keyboard(int type, SDL_Keysym keysym) {

    /*if (type == 1 && modalt) printf("Key%s: %d\n", type == KDOWN ? "down" : "up", keysym.sym);*/

    if (keysym.sym == 1073742049 || keysym.sym == 1073742053) modshift = type; /* Shift */
    if (keysym.sym == 1073742048 || keysym.sym == 1073742052) modctrl  = type; /* Control */
    if (keysym.sym == 1073742050 || keysym.sym == 1073742050) modalt   = type; /* Alt */
    
    if (keysym.sym == 27 && type == KDOWN) { /* TODO elegant way to go out, eventually */
        if (eegg < 5) eegg++;
        else          running = 0;
    }
    
    if (keysym.sym == 97 && type == KDOWN) { /* A */
    }
}

void ev_mouse(SDL_Event event) {
    SDL_MouseMotionEvent *evm = (SDL_MouseMotionEvent *) &event;
    SDL_MouseButtonEvent *evb = (SDL_MouseButtonEvent *) &event;

    switch (event.type) {

        case SDL_MOUSEBUTTONDOWN:
                if (evb->button == SDL_BUTTON_RIGHT) {
                    sb_ix = sb_fx = evb->x;
                    sb_iy = sb_fy = evb->y;
                    sb_isshow = 1;
                }
            break;
        case SDL_MOUSEBUTTONUP:
            if (evb->button == SDL_BUTTON_RIGHT) {
                sb_isshow = 0;
                sb_dozoom();
            }
            break;
        case SDL_MOUSEMOTION:
            if (evm->state & 1 << (SDL_BUTTON_LEFT - 1)) {
                ctx += evm->xrel / cz;
                cty -= evm->yrel / cz;
            }
            if (sb_isshow) {
                sb_fx = evm->x;
                sb_fy = evm->y;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (evb->x > 0) ctz *= zoom_factor;
            else            ctz /= zoom_factor;
            break;
    }
}

void ev_renderloop(void* data1, void* data2) {
    renderloop();
}

/* CREATE OBJECT EVENT  */
typedef struct ev_object {
    GLenum mode;
    GLfloat * vertices, * colours;
    GLuint * indices;
    int size, countv, countc, counti;
    GLfloat width;
    int layer;
    pthread_mutex_t *mutex;
} ev_object;

void ev_createobj(void* data1, void* data2) {
    if (allocatedRO >= MV_MAXOBJECTS) {
        if (data2 != NULL) *((int *)data2) = -1;
        printerr("Trying to add an object but the list of objects is full");
        return;
    }
    ev_object * obj = (ev_object *) data1;
    renderobj * ro = (renderobj *) malloc(sizeof(renderobj));
    int id = add_ro(ro);
    if (data2 != NULL) *((int *)data2) = id;

    ro->obj          = new_buffer(obj->mode, obj->vertices, obj->colours, obj->size, obj->countv, obj->countc, obj->indices, obj->counti);;
    ro->prog         = defprog[0];
    ro->scale        = 1.0;
    ro->translate[0] = 0.0;
    ro->translate[1] = 0.0;
    ro->rotate       = 0.0;
    ro->enable       = 1;
    ro->layer        = obj->layer;
    ro->obj->width   = obj->width;

    totalpoints += obj->counti;
    printi("%s New object added. Now drawing %lld points in %d object%s\n", INFOPRE, totalpoints, allocatedRO, allocatedRO == 1 ? "" : "s");

    if (pthread_mutex_unlock(obj->mutex) != 0)
        printerr("Error while creating a new object: couldn't release the mutex; it's time to a deadlock!");
    free(data1);
}

void eve_createobj(GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti, GLfloat width, int layer, int * id) {
    ev_object * obj = (ev_object *) malloc(sizeof(ev_object));

    pthread_mutex_t mutex;
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printerr("Error while creating a new object: couldn't create the mutex");
        return;
    }

    if (pthread_mutex_lock(&mutex) == 0) {
        obj->mode       = mode;
        obj->vertices   = vertices;
        obj->colours    = colours;
        obj->indices    = indices;
        obj->size       = size;
        obj->countv     = countv;
        obj->countc     = countc;
        obj->counti     = counti;
        obj->width      = width;
        obj->layer      = layer;
        obj->mutex      = &mutex;
        ev_emit(EV_CREATEOBJ, obj, id);

        if (pthread_mutex_lock(&mutex) != 0)
            printerr("Error while creating a new object: couldn't wait for the creation of the object");
    } else
        printerr("Error while creating a new object: couldn't lock mutex");

    pthread_mutex_destroy(&mutex);
}

/* SET PROPERTIES EVENT */
typedef struct ev_property {
    unsigned int    id;
    ev_esp          change;
    char            visibility;
    float           scale,
                    translate[2],
                    rotate;
    int             layer;
} ev_property;

void ev_setproperty(void* data1, void* data2) {
    ev_property * sp = (ev_property*) data1;
    if (sp->id >= allocatedRO || renderobjs[sp->id] == NULL)
        return;

    if (sp->change & EV_SP_VISIBILITY) {
        renderobjs[sp->id]->enable = sp->visibility;
    }
    if (sp->change & EV_SP_SCALE) {
        renderobjs[sp->id]->scale = sp->scale;
    }
    if (sp->change & EV_SP_TRANSLATE) {
        renderobjs[sp->id]->translate[0] = sp->translate[0];
        renderobjs[sp->id]->translate[1] = sp->translate[1];
    }
    if (sp->change & EV_SP_ROTATE) {
        renderobjs[sp->id]->rotate = sp->rotate;
    }
    if (sp->change & EV_SP_LAYER) {
        renderobjs[sp->id]->layer = sp->layer;
    }
    free(data1);
}
void eve_setproperty(ev_esp change, unsigned int id, char visibility, float scale, float translatex, float translatey, float rotate, int layer) {
    ev_property * obj = (ev_property *) malloc(sizeof(ev_property));
    obj->id           = id;
    obj->change       = change;
    obj->visibility   = visibility;
    obj->scale        = scale;
    obj->translate[0] = translatex;
    obj->translate[1] = translatey;
    obj->rotate       = rotate;
    obj->layer        = layer;
    ev_emit(EV_SETPROPERTY, obj, NULL);
}

/* SET PROPERTIES EVENT */
void ev_destroyobj(void* data1, void* data2) {
    /* destroy the object n */
    int n = *((int *) data1);
    free(data1);

    if (n >= allocatedRO || renderobjs[n] == NULL)
        return;

    allocatedRO--;

    totalpoints -= renderobjs[n]->obj->ibos;

    /* free the buffers */
    glDeleteBuffers(1, &(renderobjs[n]->obj->vbo));
    glDeleteBuffers(1, &(renderobjs[n]->obj->cbo));
    glDeleteBuffers(1, &(renderobjs[n]->obj->ibo));
    glDeleteVertexArrays(1, &(renderobjs[n]->obj->vao));

    free(renderobjs[n]->obj);
    free(renderobjs[n]);
    renderobjs[n] = NULL;
    //printf("Object %d was destroyed! %d total used\n", n, allocatedRO);
}

void eve_destroyobj(int n) {
    int * pn = (int *) malloc(sizeof(int));
    *pn = n;
    ev_emit(EV_DESTROYOBJ, pn, NULL);
}

/* * * * * * * * * * * * * * * * * *
 *  THE TIMERS
 * * * * * * * * * * * * * * * * * */
Uint32 timer_countfps(Uint32 interval, void* param) {
    char title[50];
    sprintf(title, "%s (%d fps)", WINTITLE, fpscount);
    fpscount = 0;
    SDL_SetWindowTitle(window, title);
    return interval;
}



/* * * * * * * * * * * * * * * * * *
 *  THE RENDER
 * * * * * * * * * * * * * * * * * */
float   a     = 0;
long    last  = 0;
float   delta = 0.0;

void renderloop() {
    long now = SDL_GetTicks();
    if (now > last) {
	    delta = ((float)(now - last)) / 1000;
	    last = now;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport (0, 0, curWidth, curHeight);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    /* zoom and translate smoothner */
    double animv    = delta * 20.0 * MV_ANIMSPEED;
    float  stopanim = 0.1;
    double dx = (ctx - cx),
           dy = (cty - cy),
           dz = (ctz - cz);

    if (fabs(dz * animv) > fabs(dz) || fabs(dz * animv) < stopanim) cz = ctz;
    else                                                            cz += dz * animv;


    if (fabs(dx * animv) > fabs(dx) || fabs(dx * animv * cz) < stopanim) cx = ctx;
    else                                                                 cx += dx * animv;
    if (fabs(dy * animv) > fabs(dy) || fabs(dy * animv * cz) < stopanim) cy = cty;
    else                                                                 cy += dy * animv;

     glOrtho((-curWidth/2.0)   / cz - cx,
             (curWidth/2.0)    / cz - cx,
             (-curHeight/2.0)  / cz - cy,
             (curHeight/2.0)   / cz - cy,
             -10.0, 10.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glRotatef(a, 0, 0, 1);/**/
    a += eegg;
    
    int i, j;
    for (j = 0; j < MAXLAYERS; j++) {
        for (i = 0; i < MV_MAXOBJECTS; i++) {
            if (!renderobjs[i])
                continue;
            if (MAXLAYERS > 1 && renderobjs[i]->layer != j)
                continue;
            if (!renderobjs[i]->enable)
                continue;
            render_ro(renderobjs[i]);
        }
    }
    
    /* SELECTION BOX */
    if (sb_isshow) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-curWidth/2.0, curWidth/2.0, -curHeight/2.0, curHeight/2.0, -10.0, 10.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    
        glTranslatef(-curWidth/2.0 + sb_ix, curHeight/2.0 - sb_iy, 0.0);
        glScalef((sb_fx - sb_ix), (sb_iy - sb_fy) , 1.0);
        
        render_ro(ro_selbox);
    }
    
    SDL_GL_SwapWindow(window);
    ev_emit(EV_RENDER, NULL, NULL);
    fpscount++;
}

void render_ro(renderobj * ro) {
    glUseProgram(ro->prog->shaderprogram);
    glBindVertexArray(ro->obj->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ro->obj->ibo);

    if (ro->obj->width > 0) {
        glPointSize(ro->obj->width);
        glLineWidth(ro->obj->width);
    }
    else {
        glPointSize(1.0);
        glLineWidth(1.0);
    }

    glPushMatrix();
        glScalef(ro->scale, ro->scale, 1.0);
        glRotatef(ro->rotate, 0, 0, 1);
        glTranslatef(ro->translate[0], ro->translate[1], 0.0);
        glDrawElements(ro->obj->mode, ro->obj->ibos, GL_UNSIGNED_INT, NULL);
    glPopMatrix();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

int add_ro(renderobj * ro) {
    int i;
    for (i = 0; i < MV_MAXOBJECTS; i++) {
        if (renderobjs[i] == NULL) {
            renderobjs[i] = ro;
            allocatedRO++;
            return  i;
        }
    }
    return -1;
}

program *  new_program(const char * vertexsource, const char * fragmentsource) {
    GLuint vertexshader = glCreateShader(GL_VERTEX_SHADER);

    /* VERTEX SHADER */
    glShaderSource(vertexshader, 1, (const GLchar**)&vertexsource, 0);
    glCompileShader(vertexshader);


    int IsCompiled_VS, maxLength;
    glGetShaderiv(vertexshader, GL_COMPILE_STATUS, &IsCompiled_VS);
    if(!IsCompiled_VS) {
       glGetShaderiv(vertexshader, GL_INFO_LOG_LENGTH, &maxLength);
       char * vertexInfoLog = (char *)malloc(maxLength);
       glGetShaderInfoLog(vertexshader, maxLength, &maxLength, vertexInfoLog);
       printe("%s Shader/Vertex: %s\n", ERRPRE, vertexInfoLog);
       free(vertexInfoLog);
       return NULL;
    }

    /* FRAGMENT SHADER */
    GLuint fragmentshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentshader, 1, (const GLchar**)&fragmentsource, 0);
    glCompileShader(fragmentshader);

    int IsCompiled_FS;
    glGetShaderiv(fragmentshader, GL_COMPILE_STATUS, &IsCompiled_FS);
    if(!IsCompiled_FS) {
       glGetShaderiv(fragmentshader, GL_INFO_LOG_LENGTH, &maxLength);
       char * fragmentInfoLog = (char *)malloc(maxLength);
       glGetShaderInfoLog(fragmentshader, maxLength, &maxLength, fragmentInfoLog);
       printe("%s Shader/Fragment: %s\n", ERRPRE, fragmentInfoLog);
       free(fragmentInfoLog);
       return NULL;
    }

    /* PROGRAM */
    GLuint shaderprogram = glCreateProgram();

    glAttachShader(shaderprogram, vertexshader);
    glAttachShader(shaderprogram, fragmentshader);

    glBindAttribLocation(shaderprogram, 0, "in_Position");
    glBindAttribLocation(shaderprogram, 1, "in_Color");

    glLinkProgram(shaderprogram);

    int IsLinked;
    glGetProgramiv(shaderprogram, GL_LINK_STATUS, (int *)&IsLinked);
    if(!IsLinked) {
        glGetProgramiv(shaderprogram, GL_INFO_LOG_LENGTH, &maxLength);
        char * shaderProgramInfoLog = (char *)malloc(maxLength);
        glGetProgramInfoLog(shaderprogram, maxLength, &maxLength, shaderProgramInfoLog);
        printe("%s Shader/Program: %s\n", ERRPRE, shaderProgramInfoLog);
        free(shaderProgramInfoLog);
        return NULL;
    }

    program * prog       = (program *) malloc(sizeof(program));
    prog->shaderprogram  = shaderprogram;
    prog->vertexshader   = vertexshader;
    prog->fragmentshader = fragmentshader;

    return prog;
}

program *  new_program_default() {
    const char * vs = "\
        #version 120\n\
        attribute vec4 in_Color;\n\
        void main() {\n\
	        gl_FrontColor = in_Color;\n\
	        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n\
        }\n\
        ";
    const char * fs = "\
        #version 120\n\
        void main() {\n\
	        gl_FragColor = gl_Color;\n\
        }\n\
        ";
    return new_program(vs, fs);
}

object * new_buffer(GLenum mode, GLfloat * vertices, GLfloat * colours, int size, int countv, int countc, GLuint * indices, int counti) {
    GLuint vao, vbo[3];

    glGenVertexArrays(1, &vao);
    glGenBuffers(3, vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, size * countv * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, countv, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, size * countc * sizeof(GLfloat), colours, GL_STATIC_DRAW);
    glVertexAttribPointer(1, countc, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, counti * sizeof(GLuint), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    object * obj = (object*) malloc(sizeof(object));

    obj->mode = mode;
    obj->vao = vao;
    obj->vbo = vbo[0];
    obj->cbo = vbo[1];
    obj->ibo = vbo[2];
    obj->ibos = counti;
    obj->width = -1.0;
    
    return obj;
}

/* SELECTION BOX */
renderobj * new_ro_selbox() {
    renderobj * ro = (renderobj *) malloc(sizeof(renderobj));
    
    GLfloat vertices[] = { 0.0, 0.0, 1.0,   1.0, 0.0, 1.0,   1.0, 1.0, 1.0,   0.0, 1.0, 1.0 };
    GLuint  indices[] = { 0, 1, 2, 3 };
    
    GLfloat * colours = (GLfloat *) malloc(sizeof(GLfloat) * 4 * 4);
    int i;
    for (i = 0; i < 4 * 4; i += 4) {
        colours[i]   = 0.0;
        colours[i+1] = 0.71;
        colours[i+2] = 0.82;
        colours[i+3] = 0.6;
    }
    
    object * tobj = new_buffer(GL_QUADS, vertices, colours, 4, 3, 4, indices, 4);
    ro->obj = tobj;
    ro->prog = defprog[0];
    
    ro->scale        = 1.0;
    ro->translate[0] = 0.0;
    ro->translate[1] = 0.0;
    ro->rotate       = 0.0;
    free(colours);
    ro->enable = 1;

    return ro;
}

void sb_dozoom() {
    int w = (sb_fx - sb_ix),
        h = (sb_fy - sb_iy);
    if (w == 0 || h == 00) return;

    if (curWidth / (float)abs(w) < curHeight / (float)abs(h))
        ctz *= curWidth / (float)abs(w);
    else
        ctz *= curHeight / (float)abs(h);

    w = curWidth/2 - (sb_ix + w / 2);
    h = curHeight/2 -(sb_iy + h / 2);

    ctx += w / cz;
    cty -= h / cz;
}


/* * * * * * * * * * * * * * * * * *
 *  THE ADDNERS AND STD COLOURS, PUBLIC API
 * * * * * * * * * * * * * * * * * */
float mv_white[]    = { 1.0, 1.0, 1.0 };
float mv_gray[]     = { 0.5, 0.5, 0.5 };
float mv_dgray[]    = { 0.2, 0.2, 0.2 };
float mv_red[]      = { 1.0, 0.0, 0.0 };
float mv_green[]    = { 0.0, 1.0, 0.0 };
float mv_blue[]     = { 0.0, 0.0, 1.0 };
float mv_pink[]     = { 1.0, 0.0, 1.0 };
float mv_yellow[]   = { 1.0, 1.0, 0.0 };
float mv_cyan[]     = { 0.0, 1.0, 1.0 };

void mv_show(int id) {
    eve_setproperty(EV_SP_VISIBILITY, id, 1, 0, 0, 0, 0, 0);
}
void mv_hide(int id) {
    eve_setproperty(EV_SP_VISIBILITY, id, 0, 0, 0, 0, 0, 0);
}
void mv_destroy(int id) {
    eve_destroyobj(id);
}
void mv_setscale(int id, float scale) {
    eve_setproperty(EV_SP_SCALE, id, 0, scale, 0, 0, 0, 0);
}
void mv_settranslate(int id, float x, float y) {
    eve_setproperty(EV_SP_TRANSLATE, id, 0, 0, x, y, 0, 0);
}
void mv_setrotate(int id, float angle) {
    eve_setproperty(EV_SP_ROTATE, id, 0, 0, 0, 0, angle, 0);
}
void mv_setlayer(int id, int layer) {
    eve_setproperty(EV_SP_LAYER, id, 0, 0, 0, 0, 0, layer);
}

int mv_add(MVprimitive primitive, float * vertices, unsigned int countv, unsigned int * indices, unsigned int counti, float * colour, float width, int layer, int * id) {
    GLenum mode;

    MVprimitive p = primitive & ((1 << 5) -1);

    /* Set the GL primitive */
    switch (p) {
        case MV_2D_TRIANGLES:           mode = GL_TRIANGLES; break;
        case MV_2D_LINES:               mode = GL_LINES;     break;
        case MV_2D_POINTS:              mode = GL_POINTS;    break;

        case MV_2D_TRIANGLES_AS_LINES:  mode = GL_LINES;     break;
        case MV_2D_TRIANGLES_AS_POINTS: mode = GL_POINTS;    break;
        default:
            printerr("Error while creating a new object: bad primitive type");
            return -1;/* NOT IMPLEMENT / BAD ARGUMENT */
    }

    /* Transform the indices for minimize the number of drawed elements */
    if (p == MV_2D_TRIANGLES_AS_LINES)
        transform_triangles_in_lines(&indices, &counti, countv);
    if (p == MV_2D_TRIANGLES_AS_POINTS)
        transform_triangles_in_points(&indices, &counti, countv);


    GLfloat * colours = (GLfloat *) malloc(sizeof(GLfloat) * countv * 3);
    if (primitive & MV_USE_COLOUR_ARRAY) {
        int i;
        for (i = 0; i < countv * 3; i += 3) {
            colours[i]   = colour[i];
            colours[i+1] = colour[i+1];
            colours[i+2] = colour[i+2];
        }
    } else { /* Build a colour array for each vertice */
        int i;
        for (i = 0; i < countv * 3; i += 3) {
            colours[i]   = colour[0];
            colours[i+1] = colour[1];
            colours[i+2] = colour[2];
        }
    }

    eve_createobj(mode, vertices, colours, countv, 2, 3, indices, counti, width, layer, id);
    if (primitive & MV_USE_COLOUR_ARRAY)
        free(colours);
    return 0;
}

int mv_add_plot(MVprimitive primitive, double (*f)(double x), double xmin, double xmax, double step, float * colour, float width, int layer, int * id) {
    switch (primitive) {
        case MV_2D_LINES:   break;
        case MV_2D_POINTS:  break;
        default:
            printi("%s %s\n", INFOPRE, "WARNING: invalid primitive, assuming MV_2D_LINES");
            primitive = MV_2D_LINES;
    }
    
    int steps = (int) ((xmax - xmin) / step);
    
    if (primitive == MV_2D_POINTS && steps < 1) {
        printi("%s Trying to plot %d points. Won't work\n", INFOPRE, steps);
        return -1;
    } else
    if (primitive == MV_2D_LINES && steps < 2) {
        printi("%s Trying to plot %d points as lines. Won't work\n", INFOPRE, steps);
        return -1;
    }


    float *vertices = (float *) malloc(sizeof(float) * steps*2);
    unsigned int *indices = NULL;
    int i, counti = 0;
    for (i = 0; i < steps; i++) {
        vertices[i*2]   = xmin + i * step;
        vertices[i*2+1] = (float) f(xmin + i * step);
    }

    if (primitive == MV_2D_POINTS) {
        indices = (unsigned int *) malloc(sizeof(unsigned int) * steps);
        for (i = 0; i < steps; i++)
            indices[i] = i;
        counti = steps;
    }
    else if (primitive == MV_2D_LINES) {
        counti = (steps-1)*2;
        indices = (unsigned int *) malloc(sizeof(unsigned int) * counti);
        for (i = 0; i < steps - 1; i++) {
            indices[i*2]   = i;
            indices[i*2+1] = i+1;
        }
    }

    return mv_add(primitive, vertices, steps, indices, counti, colour, width, layer, id);
}

void transform_triangles_in_lines(GLuint ** pindices, unsigned int * pcounti, int countv) {
    /* transform triangles in edges */
    GLuint * indices = *pindices;
    int      counti  = *pcounti;

    typedef struct item {
        unsigned int id;
        struct item * n;
    } item;
    item ** items = (item **) calloc(countv, sizeof(item *));
    int ttitems = 0, i;
    for (i = 0; i < counti; i+=3) {

        int a,b;
        item ** t;

        a = indices[i]; b = indices[i+1];
        if (a > b) { int c = a; a = b; b = c; }
        for (t = &items[a];  ; t = &(*t)->n) {
            if (*t == NULL) {
                *t = (item *) malloc(sizeof(item));
                (*t)->id = b;
                (*t)->n = NULL;
                ttitems++; break;
            } else if ((*t)->id == b) break;
        }

        a = indices[i+1]; b = indices[i+2];
        if (a > b) { int c = a; a = b; b = c; }
        for (t = &items[a];  ; t = &(*t)->n) {
            if (*t == NULL) {
                *t = (item *) malloc(sizeof(item));
                (*t)->id = b;
                (*t)->n = NULL;
                ttitems++; break;
            } else if ((*t)->id == b) break;
        }

        a = indices[i+2]; b = indices[i];
        if (a > b) { int c = a; a = b; b = c; }
        for (t = &items[a];  ; t = &(*t)->n) {
            if (*t == NULL) {
                *t = (item *) malloc(sizeof(item));
                (*t)->id = b;
                (*t)->n = NULL;
                ttitems++; break;
            } else if ((*t)->id == b) break;
        }
    }

    GLuint * tindices = (GLuint *) malloc(sizeof(GLuint) * ttitems * 2);
    int p = 0;
    for (i = 0; i < countv; i++) {
        item * t;
        for (t = items[i]; t != NULL ;) {
            tindices[p++] = i;
            tindices[p++] = t->id;

            item * tt = t;
            t = t->n;
            free (tt);
        }
    }

    free(items);
    free(indices);

    *pindices = tindices;
    *pcounti  = ttitems * 2;
}

void transform_triangles_in_points(GLuint ** pindices, unsigned int * pcounti, int countv) {
    GLuint * indices = *pindices;
    int      counti  = *pcounti;

    char * vmask = (char *) calloc(countv, sizeof(char));
    int i;

    for (i = 0; i < counti; i++)
        vmask[indices[i]] = 1;

    int countp = 0;
    for (i = 0; i < countv; i++)
        if (vmask[i]) countp++;

    GLuint * nindices = (GLuint *) malloc(sizeof(GLuint) * countp);

    for (i = 0; i < countp; i++)
        nindices[i] = i;

    free(vmask);
    free(indices);

    *pindices = nindices;
    *pcounti  = countp;
}


