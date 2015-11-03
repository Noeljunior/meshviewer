/* The default window size */
#define MV_WINW         640
#define MV_WINH         480

/* Zoom factor with mouse wheel */
#define MV_ZOOMFACTOR   1.25
/* The default zoom */
#define MV_ZOOMDEFAULT  64
/* The speed of pan and zoom animations; 0 disables the animations */
#define MV_ANIMSPEED 1.0

/* Maximum objects allowed simultaneously */
#define MV_MAXOBJECTS   32

typedef enum _MVprimitive {
    /* vertices: an unidimensional float array, each two consecutive represents a (x, y) coordinate
     *           float[] vertices = { x0, y0,  x1, y1,  x2, y2, ... }
     * countv  : number of coordinates, or number of elements in vertices array divided by two
     * indices : an unidimensional unsigned int array, each three consecutive forms a triangle
     *           usigned int indices = { i0, i1, i2, ... }
     * counti  : number os elements in indices array
     * colour  : a three element float array containing rgb values between 0 and 1
     * width   : not used
     */
    MV_2D_TRIANGLES   = 1, /* Draw filled triangles */

    /* vertices: an unidimensional float array, each two consecutive represents a (x, y) coordinate
     *           float[] vertices = { x0, y0,  x1, y1,  x2, y2, ... }
     * countv  : number of coordinates, or number of elements in vertices array divided by two
     * indices : an unidimensional unsigned int array, each twoo consecutive forms a line
     *           usigned int indices = { i0, i1, i2, ... }
     * counti  : number os elements in indices array
     * colour  : a three element float array containing rgb values between 0 and 1
     * width   : float width of the line in pixels if greater than 0, otherwise is set to 1
     */
    MV_2D_LINES       = 2, /* Draw lines */

    /* vertices: an unidimensional float array, each one represents a (x, y) coordinate
     *           float[] vertices = { x0, y0,  x1, y1,  x2, y2, ... }
     * countv  : number of coordinates, or number of elements in vertices array divided by two
     * indices : an unidimensional unsigned int array, each twoo consecutive forms a line
     *           usigned int indices = { i0, i1, i2, ... }
     * counti  : number os elements in indices array
     * colour  : a three element float array containing rgb values between 0 and 1
     * width   : float width of the line in pixels if greater than 0, otherwise is set to 1
     */
    MV_2D_POINTS      = 3,

    MV_2D_TRIANGLES_AS_LINES    = 4,
    MV_2D_TRIANGLES_AS_POINTS   = 5,

    MV_USE_COLOUR_ARRAY = 1 << 5,   /* reads countv colours from colour */


} MVprimitive;

void    mv_start(int maxlayers, int verbose);
void    mv_wait();
void    mv_stop();


void    mv_show(int id);
void    mv_hide(int id);
void    mv_destroy(int id);
void    mv_setscale(int id, float scale);
void    mv_settranslate(int id, float x, float y);
void    mv_setrotate(int id, float angle);
void    mv_setlayer(int id, int layer);
void    mv_updatecolourarray(int id, float * colour, int size);
void    mv_updatevertexarray(int id, float * vertices, int size);

int     mv_add(MVprimitive primitive, float * vertices, unsigned int countv, unsigned int * indices, unsigned int counti, float * colour, float width, int layer, int * id);
int     mv_add_plot(MVprimitive primitive, double (*f)(double x), double xmin, double xmax, double step, float * colour, float width, int layer, int * id);

extern float mv_white[];
extern float mv_gray[];
extern float mv_dgray[];
extern float mv_red[];
extern float mv_green[];
extern float mv_blue[];
extern float mv_pink[];
extern float mv_yellow[];
extern float mv_cyan[];

