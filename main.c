/* STDLIBS */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* GLLIBS */
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include "meshviewer.h"


void read_mesh(char * path);
void read_portugal(char * path);

int main (int argc, char ** argv) {
    int a;



    mv_start();

/*    read_mesh(argv[1]);
    read_mesh(argv[1]);/**/
    read_portugal(argv[1]);/**/


    /*scanf("%d", &a);
    mv_hide(2);
    mv_hide(5);*/


    scanf("%d", &a);
    mv_stop();
    scanf("%d", &a);
    return 0;
}


void read_mesh(char * path) {
    FILE * f = fopen(path, "r");
    
    GLfloat * vertices;
    GLuint * indices;
    unsigned int countv, counti;
    
    fscanf(f, "%d", &countv);
    
    vertices = (GLfloat *) malloc(sizeof(GLfloat) * countv * 2);
    
    int i, p = 0, b;
    for (i = 0; i < countv; i++) {
        fscanf(f, "%f %f %d", vertices + p++, vertices + p++, &b);
    }
    
    
    fscanf(f, "%d", &counti);
    counti *= 3;
    indices = (GLuint *) malloc(sizeof(GLuint) * counti);
    
    for (i = 0, p = 0; i < counti; i++) {
        fscanf(f, "%d %d %d", indices + p++, indices + p++, indices + p++);
    }
    fclose(f);

    int ids[3];

    GLfloat * cvertices = (GLfloat *) malloc(sizeof(GLfloat) * countv * 2);
    memcpy(cvertices, vertices, sizeof(GLfloat) * countv * 2);
    GLuint  * cindices = (GLuint *) malloc(sizeof(GLuint) * counti);
    memcpy(cindices, indices, sizeof(GLuint) * counti);



    /* * * * * MV_2D_TRIANGLES * * * * */
    mv_add(MV_2D_TRIANGLES, vertices, countv, indices, counti, mv_gray, 0, ids);



    vertices = (GLfloat *) malloc(sizeof(GLfloat) * countv * 2);
    memcpy(vertices, cvertices, sizeof(GLfloat) * countv * 2);
    indices = (GLuint *) malloc(sizeof(GLuint) * counti);
    memcpy(indices, cindices, sizeof(GLuint) * counti);



    /* * * * * MV_2D_TRIANGLES_AS_LINES * * * * */
    mv_add(MV_2D_TRIANGLES_AS_LINES, cvertices, countv, cindices, counti, mv_white, 2, ids + 1);

    /* * * * * MV_2D_TRIANGLES_AS_POINTS * * * * */
    mv_add(MV_2D_TRIANGLES_AS_POINTS, vertices, countv, indices, counti, mv_red, 5, ids + 2);

}

void read_portugal(char * path) {
    FILE * f = fopen(path, "r");
    
    GLfloat * vertices;
    GLuint * indices;
    unsigned int countv, counti;
    
    fscanf(f, "%d", &countv);
    
    vertices = (GLfloat *) malloc(sizeof(GLfloat) * countv * 2);
    
    int i, p = 0, b;
    double x, y;
    for (i = 0; i < countv; i++) {
        fscanf(f, "%d %lf %lf", &b, &x, &y);
        double diff = (6671231 - 335642);
        
        vertices[p++] = (float) ((x + 2367875) / diff);
        vertices[p++] = (float) ((y - 335642) / diff);
        /*if (i<2) {
            printf("%lf: %lf %lf; %f %f\n", diff, x, y, vertices[p-2], vertices[p-1]);
        }*/
    }
    printf("read1\n");
    
    fscanf(f, "%d", &counti);
    counti *= 2;
    indices = (GLuint *) malloc(sizeof(GLuint) * counti);
    
    for (i = 0, p = 0; i < counti; i++) {
        fscanf(f, "%d %d", indices + p++, indices + p++);
    }
    fclose(f);
    printf("read2\n");
    mv_add(MV_2D_LINES, vertices, countv, indices, counti, mv_pink, 2.5, 0);
}
