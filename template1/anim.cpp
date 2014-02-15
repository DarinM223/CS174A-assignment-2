////////////////////////////////////////////////////
// anim.cpp version 4.1
// Template code for drawing an articulated figure.
// CS 174A 
////////////////////////////////////////////////////

#include <GL/glew.h>
#ifdef WIN32
#include <windows.h>
#include "GL/glew.h"
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#ifdef WIN32
#include "GL/freeglut.h"
#else
#include <GL/glut.h>
#endif

#include "Ball.h"
#include "FrameSaver.h"
#include "Timer.h"
#include "Shapes.h"
#include "tga.h"

#include "Angel/Angel.h"

#include <queue>
#include <vector>
#include <map>
using std::vector;
using std::queue;
using std::map;
#ifdef __APPLE__
#define glutInitContextVersion(a,b)
#define glutInitContextProfile(a)
#define glewExperimental int glewExperimentalAPPLE
#define glewInit()
#endif

FrameSaver FrSaver ;
Timer TM ;

BallData *Arcball = NULL ;
int Width = 800;
int Height = 800 ;
int Button = -1 ;
float Zoom = 1 ;
int PrevY = 0 ;

int Animate = 0 ;
int Recording = 0 ;

void resetArcball() ;
void save_image();
void instructions();
void set_colour(float r, float g, float b) ;

const int STRLEN = 100;
typedef char STR[STRLEN];

#define PI 3.1415926535897
#define X 0
#define Y 1
#define Z 2

//texture

GLuint texture_cube;
GLuint texture_earth;

// Structs that hold the Vertex Array Object index and number of vertices of each shape.
ShapeData cubeData;
ShapeData sphereData;
ShapeData coneData;
ShapeData cylData;

TgaImage coolImage, earthImage;


// Matrix stack that can be used to push and pop the modelview matrix.
class MatrixStack {
    int    _index;
    int    _size;
    mat4*  _matrices;

   public:
    MatrixStack( int numMatrices = 32 ):_index(0), _size(numMatrices)
        { _matrices = new mat4[numMatrices]; }

    ~MatrixStack()
	{ delete[]_matrices; }

    void push( const mat4& m ) {
        assert( _index + 1 < _size );
        _matrices[_index++] = m;
    }

    mat4& pop( void ) {
        assert( _index - 1 >= 0 );
        _index--;
        return _matrices[_index];
    }
};




MatrixStack  mvstack;
mat4         model_view;
GLint        uModelView, uProjection, uView;
GLint        uAmbient, uDiffuse, uSpecular, uLightPos, uShininess;
GLint        uTex, uEnableTex;

class drawableObject;
typedef void (*sceneCall)(drawableObject* obj);
typedef void (*cameraCall)(void);


// The eye point and look-at point.
// Currently unused. Use to control a camera with LookAt().
Angel::vec4 eye{0, 0.0, 50.0,1.0};
Angel::vec4 ref{0.0, 0.0, 0.0,1.0};
Angel::vec4 up{0.0,1.0,0.0,0.0};

double TIME = 0.0 ;

class SceneManager;
class drawableObject {
        private:
                friend class SceneObject;
        protected:
                mat4 myMV;
                double myTime;
                bool isAnimated;
                map<int, bool> animIndexMap;
                map<int, double> animTimeMap;
                double x, y, z;
                int index;
                vector<GLuint*> texture_stuff;
                vector<TgaImage*> images;
                virtual void genTextures() {
                        for (size_t i = 0; i < texture_stuff.size(); i++) {
                                glGenTextures(1, texture_stuff[i]);
                                glBindTexture(GL_TEXTURE_2D, *texture_stuff[i]);
                                glTexImage2D(GL_TEXTURE_2D, 0, 4, images[i]->width, images[i]->height, 0, 
                                                (images[i]->byteCount == 3) ? GL_BGR : GL_BGRA,
                                                GL_UNSIGNED_BYTE, images[i]->data);
                                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        }
                }
        public:
                double getX() {return x;}
                double getY() {return y;}
                double getZ() {return z;}
                int getIndex() {return index;}
                virtual void draw() { }
                void drawObject() {
                                //myTime += .02;
                                for (map<int, double>::iterator iter = animTimeMap.begin(); iter != animTimeMap.end(); iter++) {
                                        if (isCurrAnimation(iter->first))
                                                iter->second += .02;
                                }
                                mvstack.push(model_view);
                                model_view *= myMV;
                                /*
                                 *  [0] [4] [8] [12]
                                 *  [1] [5] [9] [13]
                                 *  [2] [6] [10][14]
                                 *  [3] [7] [11][15]
                                 */
                                x = myMV[0][3];
                                y = myMV[1][3];
                                z = myMV[2][3];
                                draw();
                                model_view = mvstack.pop();
                }
                virtual void anim(int animIndex) { 
                        if (!isCurrAnimation(animIndex)) {
                                isAnimated = true;
                                animIndexMap[animIndex] = true;
                                animTimeMap[animIndex] = 0;
                        }
                }
                virtual bool isCurrAnimation(int animIndex) {
                        return (animIndexMap.find(animIndex) != animIndexMap.end() && animIndexMap[animIndex]);
                }
                virtual void stopAnim(int animIndex) {
                        if (isCurrAnimation(animIndex)) {
                                animIndexMap[animIndex] = false;
                        }
                }
                virtual double getTime(int animIndex) { 
                        if (animTimeMap.find(animIndex) == animTimeMap.end()) return 0;
                        return animTimeMap[animIndex]; 
                }
                virtual void stopAllAnim() {
                        isAnimated = false;
                        for (map<int,bool>::iterator iter = animIndexMap.begin(); iter != animIndexMap.end(); iter++) {
                                iter->second = false; //set all values to false
                        }
                }
                virtual void resetAllAnim() { 
                        for (map<int,double>::iterator iter = animTimeMap.begin(); iter != animTimeMap.end(); iter++) {
                                iter->second = 0;
                        }
                }
                virtual void resetAnim(int animIndex) {
                        if (animTimeMap.find(animIndex) != animTimeMap.end()) {
                                animTimeMap[animIndex] = 0;
                        }
                } 
                void move(double x, double y, double z) {
                        myMV *= Translate(x, y, z);
                }
                void rotate(int angleX, int angleY, int angleZ) {
                        myMV *= RotateX(angleX);
                        myMV *= RotateY(angleY);
                        myMV *= RotateZ(angleZ);
                }
                drawableObject() : x(0), y(0), z(0) {
                        myMV = mat4(1.0f);
                        myTime = 0;
                        isAnimated = false;
                }
                virtual ~drawableObject() {
                }
};

void drawSphere();
void drawCube();
void drawCone();

class Flower : public drawableObject {
        private:
                void drawFlowerHead() {
                        mvstack.push(model_view);
                        set_colour(1.0, 0.0, 0.0);
                        model_view *= RotateZ(12*sin(myTime));
                        model_view *= Translate(0, 9.9, 0);
                        model_view *= Scale(1.5, 1.5, 1.5);
                        drawSphere();
                        model_view = mvstack.pop();
                }
                void drawFlowerStem() {
                        mvstack.push(model_view);
                                set_colour(0.5, 0.35, 0.05);
                                //draw pieces of the stem
                                //#1
                                mvstack.push(model_view);
                                        model_view *= RotateZ(1.25*sin(myTime));
                                        model_view *= Translate(0, 1.0, 0);
                                        model_view *= RotateZ(1.25*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#2
                                mvstack.push(model_view);
                                        model_view *= RotateZ(2.5*sin(myTime));
                                        model_view *= Translate(0, 2, 0);
                                        model_view *= RotateZ(2.5*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#3
                                mvstack.push(model_view);
                                        model_view *= RotateZ(3.75*sin(myTime));
                                        model_view *= Translate(0, 3, 0);
                                        model_view *= RotateZ(3.75*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#4
                                mvstack.push(model_view);
                                        model_view *= RotateZ(5*sin(myTime));
                                        model_view *= Translate(0, 4, 0);
                                        model_view *= RotateZ(5*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#5
                                mvstack.push(model_view);
                                        model_view *= RotateZ(6.25*sin(myTime));
                                        model_view *= Translate(0, 5, 0);
                                        model_view *= RotateZ(6.25*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#6
                                mvstack.push(model_view);
                                        model_view *= RotateZ(7.5*sin(myTime));
                                        model_view *= Translate(0, 6, 0);
                                        model_view *= RotateZ(7.5*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#7
                                mvstack.push(model_view);
                                        model_view *= RotateZ(8.75*sin(myTime));
                                        model_view *= Translate(0, 7, 0);
                                        model_view *= RotateZ(8.75*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                                //#8
                                mvstack.push(model_view);
                                        model_view *= RotateZ(10*sin(myTime));
                                        model_view *= Translate(0, 8, 0);
                                        model_view *= RotateZ(10*sin(myTime));
                                        model_view *= Scale(.15, 1, .15);
                                        drawCube();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
                
                void drawFlower() {
                        //draw the flower parts
                        mvstack.push(model_view);
                        drawFlowerHead();
                        drawFlowerStem();
                        model_view = mvstack.pop();
                }
        public:
                virtual void draw() {
                        //drawableObject::draw();
                        mvstack.push(model_view);
                        model_view *= myMV;
                        drawFlower();
                        model_view = mvstack.pop();
                }
                Flower() : drawableObject() {
                }
};
class Ground : public drawableObject {
        private:
                void drawGround() {
                        mvstack.push(model_view);
                        //set_colour(0.0, 0.5, 0.0);
                        set_colour(1, 1, 1);
                        //model_view *= Translate(0,0,0);
                        model_view *= Scale(1000, 1, 1000);
                        drawCube();
                        model_view = mvstack.pop();
                }
        public:
                virtual void draw() {
                        //drawableObject::draw();
                        mvstack.push(model_view);
                        model_view *= myMV;
                        drawGround();
                        model_view = mvstack.pop();
                }
                Ground() : drawableObject() {
                }
};
void drawWithTexture(void (*f)(void), GLuint tex);
class Penguin : public drawableObject {
	private:
                int beakRot ;
                int beakVel ;
                int typeOfFace ;
                int flapAngle ; 
                int flapVel ;     
                int legRightRot ;      
                int legRightVel ;        
                int legLeftRot;
                int legLeftVel;
                bool rightLeg;
                const double walkTime ;

                TgaImage *furOne, *furTwo;
                GLuint *furTexOne, *furTexTwo;
                void drawPenguinBody() {
                        mvstack.push(model_view);
                                set_colour(1, 1, 1);
                                model_view *= Scale(2, 2.5, 2);
                                drawWithTexture(drawSphere, *texture_stuff[BODY_FUR]);
                        model_view = mvstack.pop();
                }
                void drawPenguinEyes() {
                        //0 is normal face, 1 is tired face
                        mvstack.push(model_view);
                                set_colour(1, 1, 1);
                                model_view *= Translate(0, .5, .75);
                                model_view *= Translate(-.5, 0, 0);
                                mvstack.push(model_view);
                                        if (typeOfFace == this->PENGUIN_ROUNDEYES) {
                                                model_view *= Scale(.2, .2, .2);
                                                drawSphere();
                                        } else if (typeOfFace == this->PENGUIN_LAZYEYES) {
                                                model_view *= Scale(.3, .05, .1);
                                                drawCube();
                                        }
                                model_view = mvstack.pop();
                                model_view *= Translate(1, 0, 0);
                                mvstack.push(model_view);
                                        if (typeOfFace == this->PENGUIN_ROUNDEYES) {
                                                model_view *= Scale(.2, .2, .2);
                                                drawSphere();
                                        } else if (typeOfFace == this->PENGUIN_LAZYEYES) {
                                                model_view *= Scale(.3, .05, .1);
                                                drawCube();
                                        }
                                model_view = mvstack.pop();
                         model_view = mvstack.pop();
                }
                void drawPenguinBeak() {
                        if (isCurrAnimation(this->PENGUIN_TALKING)) {        
                                beakRot = 10;
                                beakVel = 2;
                        } else {
                                beakRot = 0;
                                beakVel = 0;
                        }
                        mvstack.push(model_view);
                                set_colour(1, .5, 0);
                                model_view *= RotateX(180);
                                model_view *= Translate(0, 0, -1);
                                mvstack.push(model_view);
                                        model_view *= RotateX(-beakRot* sin(beakVel*getTime(this->PENGUIN_TALKING)));
                                        model_view *= Translate(0, 0, -.25);
                                        model_view *= Scale(.3, .1, .5);
                                        drawCone();
                                model_view = mvstack.pop();
                                mvstack.push(model_view);
                                        model_view *= RotateX(beakRot * sin(beakVel*getTime(this->PENGUIN_TALKING)));
                                        model_view *= Translate(0, 0, -.25);
                                        model_view *= Scale(.3, .1, .5);
                                        drawCone();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
                void drawPenguinHead() {
                        //draw head
                        mvstack.push(model_view);
                                set_colour(1, 1, 1);
                                model_view *= Translate(0, 3.25, 0);
                                if (isCurrAnimation(this->PENGUIN_SLIDING)) {
                                       model_view *= RotateX(-90);
                                } else {
                                
                                }
                                //drawSphere();
                                drawWithTexture(drawSphere, *texture_stuff[WING_FUR]);
                                //draw eyes
                                drawPenguinEyes();
                                drawPenguinBeak();
                        model_view = mvstack.pop(); 
                }
                void drawPenguinArms() {
                        //0 is no rotation, 1 is flap
                        if (isCurrAnimation(this->PENGUIN_FLAPPING)) {
                                flapAngle = 20;
                                flapVel = 3;
                        } else {
                                flapAngle = 0;
                                flapVel = 0;
                        }
                        mvstack.push(model_view);
                                set_colour(1,1,1);
                                model_view *= Translate(1, 2, 0);
                                model_view *= RotateZ(-10);
                                model_view *= RotateZ(abs(flapAngle * sin(flapVel*getTime(this->PENGUIN_FLAPPING))));
                                mvstack.push(model_view);
                                        model_view *= RotateZ(80);
                                        model_view *= Translate(0, -.5, 0);
                                        model_view *= Scale(.1, 1, 1.5);
                                        drawWithTexture(drawCube, *texture_stuff[WING_FUR]);
                                        //drawCube();
                                model_view = mvstack.pop();
                                mvstack.push(model_view);
                                        model_view *= Translate(1.5, -1.6, 0);
                                        model_view *= RotateZ(20);
                                        model_view *= Scale(.1, 3, 1.5);
                                        drawWithTexture(drawCube, *texture_stuff[WING_FUR]);
                                        //drawCube();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                        mvstack.push(model_view);
                                model_view *= Translate(-1, 2, 0);
                                model_view *= RotateZ(10);
                                model_view *= RotateZ(-abs(flapAngle * sin(flapVel*getTime(this->PENGUIN_FLAPPING))));
                                mvstack.push(model_view);
                                        model_view *= RotateZ(-80);
                                        model_view *= Translate(0, -.5, 0);
                                        model_view *= Scale(.1, 1, 1.5);
                                        drawWithTexture(drawCube, *texture_stuff[WING_FUR]);
                                        //drawCube();
                                model_view = mvstack.pop();
                                mvstack.push(model_view);
                                        model_view *= Translate(-1.5, -1.6, 0);
                                        model_view *= RotateZ(-20);
                                        model_view *= Scale(.1, 3, 1.5);
                                        drawWithTexture(drawCube, *texture_stuff[WING_FUR]);
                                        //drawCube();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
                void drawPenguinLegs() {
                        if (isCurrAnimation(this->PENGUIN_RIGHTLEG)) {
                                legRightRot = 20;
                        } else legRightRot = 0;
                        if (isCurrAnimation(this->PENGUIN_LEFTLEG)) {
                                legLeftRot = 20;
                        } else legLeftRot = 0;
                        mvstack.push(model_view);
                                set_colour(1, .5, 0);
                                model_view *= Translate(0, -2.4, .2);
                                model_view *= Translate(-.7, 0, 0);
                                //right leg
                                mvstack.push(model_view);
                                                model_view *= RotateX(-abs(legRightRot * sin(legRightVel * getTime(this->PENGUIN_RIGHTLEG))));
                                        model_view *= Translate(0, 0, 1);
                                        model_view *= Scale(.5, .1, 1);
                                        drawSphere();
                                model_view = mvstack.pop();
                                model_view *= Translate(1.4, 0, 0);
                                //left leg
                                mvstack.push(model_view);
                                                model_view *= RotateX(-abs(legLeftRot * sin(legLeftVel * getTime(this->PENGUIN_LEFTLEG))));
                                        model_view *= Translate(0, 0, 1);
                                        model_view *= Scale(.5, .1, 1);
                                        drawSphere();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
                //void drawPenguinTail() {
                //        mvstack.push(model_view);
                //                set_colour(0,0,0);
                //                model_view *= Translate(0, -2.4, -2);
                //                model_view *= Scale(.25, .5, 1);
                //                drawCone();
                //        model_view = mvstack.pop();
                //}
                void drawPenguin() {
                        mvstack.push(model_view); 
                                if (isCurrAnimation(this->PENGUIN_SLIDING)) {
                                        model_view *= RotateX(90);
                                }
                                drawPenguinBody();
                                drawPenguinHead();
                                drawPenguinArms();
                                drawPenguinLegs();
                                //drawPenguinTail();
                        model_view = mvstack.pop();
                }
                void genTextures() {
                        furOne = new TgaImage();
                        furTwo = new TgaImage();
                        furTexOne = new GLuint;
                        furTexTwo = new GLuint;
                        if (!furOne->loadTGA("fur.tga")) {
                                printf("Error loading image file\n");
                                exit(1);
                        }
                        if (!furTwo->loadTGA("fur2.tga")) {
                                printf("Error loading image file\n");
                                exit(1);
                        }
                        images.push_back(furOne);
                        images.push_back(furTwo);
                        texture_stuff.push_back(furTexOne);
                        texture_stuff.push_back(furTexTwo);
                        drawableObject::genTextures();
                }
                enum PenguinTextures {
                        BODY_FUR,
                        WING_FUR
                };
        public:
                enum PenguinActions {
                        PENGUIN_TALKING,
                        PENGUIN_FLAPPING,
                        PENGUIN_ROUNDEYES,
                        PENGUIN_LAZYEYES,
                        PENGUIN_RIGHTLEG,
                        PENGUIN_LEFTLEG,
                        PENGUIN_SLIDING
                };
                void walk() {
                        if (getTime(Penguin::PENGUIN_LEFTLEG) < walkTime && getTime(Penguin::PENGUIN_RIGHTLEG) < walkTime) {
                                if (rightLeg) {
                                        anim(Penguin::PENGUIN_RIGHTLEG);
                                } else {
                                        anim(Penguin::PENGUIN_LEFTLEG);
                                }
                        } else if (getTime(Penguin::PENGUIN_LEFTLEG) >= walkTime || getTime(Penguin::PENGUIN_RIGHTLEG) >= walkTime) {
                                if (rightLeg == false) {
                                        stopAnim(Penguin::PENGUIN_LEFTLEG);
                                        resetAnim(Penguin::PENGUIN_LEFTLEG);
                                        rightLeg = true;
                                }
                                else { 
                                        stopAnim(Penguin::PENGUIN_RIGHTLEG);
                                        resetAnim(Penguin::PENGUIN_RIGHTLEG);
                                        rightLeg = false;
                                }
                        }
                }
                virtual void draw() {
                        //drawableObject::draw();
                        mvstack.push(model_view);
                        model_view *= myMV;
                        drawPenguin();
                        model_view = mvstack.pop();
                }
                virtual void anim(int animIndex) {
                        if (animIndex == this->PENGUIN_ROUNDEYES || animIndex == this->PENGUIN_LAZYEYES) {
                                typeOfFace = animIndex;
                        } else {
                                drawableObject::anim(animIndex);
                        }
                }
                Penguin() : walkTime(3) {
                        beakRot = 10;
                        beakVel = 2;
                        flapAngle = 0; 
                        typeOfFace = 0;
                        flapVel = 0;    
                        legLeftRot = 0;     
                        legLeftVel = 1;     
                        legRightRot = 0;
                        legRightVel = 1;
                        rightLeg = false;
                        genTextures();
                }
};

class Bee : public drawableObject {
        private:
                void drawBody() {
                    mvstack.push(model_view);
                    //draw body
                    model_view *= Scale(2, 1, 1);
                    //set color to grey
                    set_colour(.658, .658, .658);
                    drawCube();
                    model_view = mvstack.pop();
                }
                void drawHead() {
                    mvstack.push(model_view);
                    //draw head
                    model_view *= Translate(-1.5, 0, 0);
                    model_view *= Scale(.5, .5, .5);
                    //set color to blue
                    set_colour(0.0, 0.0, 1.0);
                    drawSphere();
                    model_view = mvstack.pop();
                }
                void drawTail() {
                    mvstack.push(model_view);
                    //draw tail
                    model_view *= Translate(2.5, 0, 0);
                    model_view *= Scale(1.5, .75, .75);
                    //set color to yellow
                    set_colour(1.0, 1.0, 0.0);
                    drawSphere();
                    model_view = mvstack.pop();
                }
                void drawBee() {
                    mvstack.push(model_view);
                    //rotate bee
                    //model_view *= RotateY(-10*TIME);
                    //model_view *= Translate(0, 5, 0);
                    //model_view *= Translate(5, .5*sin(100+TIME), 0);
                    //model_view *= RotateY(90);
                
                    //draw bee parts
                    drawBody();    
                    drawHead();
                    drawTail();
                    drawWings();
                    drawLegz();
                
                    model_view = mvstack.pop();
                }
                void drawWings() {
                        mvstack.push(model_view);
                        set_colour(.658, .658, .658);
                                mvstack.push(model_view); //draw first wing
                                        model_view *= Translate(0, .5, -.5);
                                        model_view *= RotateX(50*sin(myTime));
                                        model_view *= Translate(0, 0.025, -1);
                                        model_view *= Scale(1.0, 0.05, 2);
                                        drawCube();
                                model_view = mvstack.pop();
                
                                mvstack.push(model_view); //draw second wing
                                        model_view *= Translate(0, .5, .5);
                                        model_view *= RotateX(-50*sin(myTime));
                                        model_view *= Translate(0, 0.025, 1);
                                        model_view *= Scale(1.0, 0.05, 2);
                                        drawCube();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
                void drawLeg(int side) {
                        if (side == 0) { //if left leg
                                mvstack.push(model_view); //draw thigh
                                        model_view *= Translate(0, -.5, -.5);
                                        model_view *= RotateX(-abs(20 * sin(.5 * myTime)));
                                        mvstack.push(model_view);
                                                model_view *= Translate(0, -.3, -.075);
                                                model_view *= Scale(.15, .6, .15);
                                                drawCube();
                                        model_view = mvstack.pop();
                
                                        model_view *= Translate(0, -.6, 0); //move down to draw foot
                                        mvstack.push(model_view); //draw foot
                                                model_view *= RotateX(-abs(20 * sin(.5 * myTime)));
                                                model_view *= Translate(0, -.3, -.075);
                                                model_view *= Scale(.15, .6, .15);
                                                drawCube();
                                        model_view = mvstack.pop();
                                model_view = mvstack.pop();
                        } else if (side == 1) { //if right leg
                                mvstack.push(model_view); //draw thigh
                                        model_view *= Translate(0, -.5, .5);
                                        model_view *= RotateX(abs(20 * sin(.5 * myTime)));
                                        mvstack.push(model_view);
                                                model_view *= Translate(0, -.3, .075);
                                                model_view *= Scale(.15, .6, .15);
                                                drawCube();
                                        model_view = mvstack.pop();
                
                                        model_view *= Translate(0, -.6, 0); //move down to draw foot
                                        mvstack.push(model_view); //draw foot
                                                model_view *= RotateX(abs(20 * sin(.5 * myTime)));
                                                model_view *= Translate(0, -.3, .075);
                                                model_view *= Scale(.15, .6, .15);
                                                drawCube();
                                        model_view = mvstack.pop();
                                model_view = mvstack.pop();
                        }
                }
                void drawLegz() {
                    //draw left legs
                    mvstack.push(model_view);
                        drawLeg(0);
                        model_view *= Translate(-.5, 0, 0);
                        drawLeg(0);
                        model_view *= Translate(1, 0, 0);
                        drawLeg(0);
                    model_view = mvstack.pop();
                
                    //draw right legs
                    mvstack.push(model_view);
                        drawLeg(1);
                        model_view *= Translate(-.5, 0, 0);
                        drawLeg(1);
                        model_view *= Translate(1, 0, 0);
                        drawLeg(1);
                    model_view = mvstack.pop();
                }
        public:
                virtual void draw() {
                        //drawableObject::draw();
                        mvstack.push(model_view);
                        model_view *= myMV;
                        drawBee();
                        model_view = mvstack.pop();
                }
                Bee() : drawableObject() {
                }
};
class CameraObject {
        private:
                vector<cameraCall> cameras;
        public:
                void addCamera(cameraCall scene) {
                        cameras.push_back(scene);
                }
                void play() {
                        for (size_t i = 0; i < cameras.size(); i++) {
                                cameras[i]();
                        }
                }
                CameraObject(cameraCall _camera) {
                        addCamera(_camera);
                }
                CameraObject() {
                }
};
class CameraManager {
        private:
                queue<CameraObject*> cameraScenes;
                queue<double> cameraTimes;
                CameraObject *currCamera;
                double startTime, endTime;
        public:
                void addCameraScene(CameraObject *camScene, double time) {
                        cameraScenes.push(camScene);
                        cameraTimes.push(time);
                }
                void playCurrCamera() {
                        if (currCamera == NULL) {
                                if (cameraScenes.size() != 0) {
                                    currCamera = cameraScenes.front();
                                    endTime = startTime+cameraTimes.front();
                                    cameraScenes.pop();
                                    cameraTimes.pop();
                                    if (endTime != startTime) {
                                            currCamera->play();
                                    }
                                } else {
                                        //do nothing
                                }
                        } else if (TIME > startTime && TIME < endTime) {
                                currCamera->play();
                        } else if (TIME >= endTime) {
                                startTime = endTime;
                                delete currCamera;
                                currCamera = NULL;
                        } 
                }
                ~CameraManager() {
                        while (!cameraScenes.empty()) {
                                CameraObject* cam = cameraScenes.front();
                                cameraScenes.pop();
                                delete cam;
                        }
                }
};

class SceneObject {
        private:
                queue<sceneCall> scenes;
                queue<double> sceneTimes;
                drawableObject* object;
                sceneCall currScene;
                double startTime, endTime;
        public:
                void setIndex(int index) {object->index = index;}
                void addScene(sceneCall scene, double sceneTime) {
                        this->scenes.push(scene);
                        this->sceneTimes.push(sceneTime);
                }
                SceneObject(drawableObject* obj) {
                        this->object = obj;
                        startTime = 0;
                        endTime = 0;
                        currScene = NULL;
                }
                ~SceneObject() {
                        delete object;
                }
                void playCurrScene() {
                        if (currScene == NULL) {
                                if (scenes.size() != 0) {
                                    currScene = scenes.front();
                                    endTime = startTime+sceneTimes.front();
                                    scenes.pop();
                                    sceneTimes.pop();
                                    if (endTime != startTime) {
                                            currScene(object);
                                    }
                                } else {
                                        //do nothing
                                }
                        } else if (TIME > startTime && TIME < endTime) {
                                currScene(object);
                        } else if (TIME >= endTime) {
                                startTime = endTime;
                                currScene = NULL;
                        } 
                        object->drawObject();
                } 
};
class SceneManager {
        private:
                vector<SceneObject*> sceneObjs;
                CameraManager camManager; 
        public:
                void drawScene() {
                        for (size_t i = 0; i < sceneObjs.size(); i++) {
                                sceneObjs[i]->playCurrScene();
                        }
                        camManager.playCurrCamera();
                }                
                int addObject(drawableObject* obj) {
                        SceneObject *newObj = new SceneObject(obj);
                        sceneObjs.push_back(newObj);
                        int index = sceneObjs.size() - 1;                        
                        sceneObjs[index]->setIndex(index);
                        return index;
                }
                void addSceneToObject(int objIndex, sceneCall func, double sceneTimeLength) {
                       sceneObjs[objIndex]->addScene(func, sceneTimeLength); 
                }
                void addCameraMovement( CameraObject *camScene, double cameraMoveLength) {
                        camManager.addCameraScene(camScene, cameraMoveLength);
                }
                SceneManager() {
                }
                ~SceneManager() {
                        for (size_t i = 0; i < sceneObjs.size(); i++) {
                                delete sceneObjs[i];
                        }
                }
};

void drawWithTexture(void (*f)(void), GLuint tex) {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glUniform1i(uEnableTex, 1);
        f();
        glUniform1i(uEnableTex, 0);
}

void do360();
void panLeft();
void panRight();
void zoomIn();
void zoomOut();
void doAnimate(drawableObject *obj);
void moveForward(drawableObject *obj);
void doPenguin(drawableObject *obj);
void stopObject(drawableObject *obj);

/////////////////////////////////////////////////////
//    PROC: drawCylinder()
//    DOES: this function 
//          render a solid cylinder  oriented along the Z axis. Both bases are of radius 1. 
//          The bases of the cylinder are placed at Z = 0, and at Z = 1.
//
//          
// Don't change.
//////////////////////////////////////////////////////
void drawCylinder(void)
{
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( cylData.vao );
    glDrawArrays( GL_TRIANGLES, 0, cylData.numVertices );
}

//////////////////////////////////////////////////////
//    PROC: drawCone()
//    DOES: this function 
//          render a solid cone oriented along the Z axis with base radius 1. 
//          The base of the cone is placed at Z = 0, and the top at Z = 1. 
//         
// Don't change.
//////////////////////////////////////////////////////
void drawCone(void)
{
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( coneData.vao );
    glDrawArrays( GL_TRIANGLES, 0, coneData.numVertices );
}


//////////////////////////////////////////////////////
//    PROC: drawCube()
//    DOES: this function draws a cube with dimensions 1,1,1
//          centered around the origin.
// 
// Don't change.
//////////////////////////////////////////////////////

void drawCube(void)
{
    //glBindTexture( GL_TEXTURE_2D, texture_cube );
    //glUniform1i( uEnableTex, 1 );
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( cubeData.vao );
    glDrawArrays( GL_TRIANGLES, 0, cubeData.numVertices );
    //glUniform1i( uEnableTex, 0 );
}


//////////////////////////////////////////////////////
//    PROC: drawSphere()
//    DOES: this function draws a sphere with radius 1
//          centered around the origin.
// 
// Don't change.
//////////////////////////////////////////////////////

void drawSphere(void)
{
    //glBindTexture( GL_TEXTURE_2D, texture_earth);
    //glUniform1i( uEnableTex, 1);
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( sphereData.vao );
    glDrawArrays( GL_TRIANGLES, 0, sphereData.numVertices );
    //glUniform1i( uEnableTex, 0 );
}


void resetArcball()
{
    Ball_Init(Arcball);
    Ball_Place(Arcball,qOne,0.75);
}


//////////////////////////////////////////////////////
//    PROC: myKey()
//    DOES: this function gets caled for any keypresses
// 
//////////////////////////////////////////////////////

void myKey(unsigned char key, int x, int y)
{
    float time ;
    switch (key) {
        case 'q':
        case 27:
            exit(0); 
        case 's':
            FrSaver.DumpPPM(Width,Height) ;
            break;
        case 'r':
            resetArcball() ;
            glutPostRedisplay() ;
            break ;
        case 'a': // togle animation
            Animate = 1 - Animate ;
            // reset the timer to point to the current time		
            time = TM.GetElapsedTime() ;
            TM.Reset() ;
            // printf("Elapsed time %f\n", time) ;
            break ;
        case '0':
            //reset your object
            break ;
        case 'm':
            if( Recording == 1 )
            {
                printf("Frame recording disabled.\n") ;
                Recording = 0 ;
            }
            else
            {
                printf("Frame recording enabled.\n") ;
                Recording = 1  ;
            }
            FrSaver.Toggle(Width);
            break ;
        case 'h':
        case '?':
            instructions();
            break;
    }
    glutPostRedisplay() ;

}

SceneManager manager;
int firstPenguinIndex;
double penguinX, penguinY, penguinZ;
/*********************************************************
    PROC: myinit()
    DOES: performs most of the OpenGL intialization
     -- change these with care, if you must.

**********************************************************/

void myinit(void)
{
    // Load shaders and use the resulting shader program
    GLuint program = InitShader( "vshader.glsl", "fshader.glsl" );
    glUseProgram(program);

    // Generate vertex arrays for geometric shapes
    generateCube(program, &cubeData);
    generateSphere(program, &sphereData);
    generateCone(program, &coneData);
    generateCylinder(program, &cylData);

    uModelView  = glGetUniformLocation( program, "ModelView"  );
    uProjection = glGetUniformLocation( program, "Projection" );
    uView       = glGetUniformLocation( program, "View"       );

    glClearColor( 0.1, 0.1, 0.2, 1.0 ); // dark blue background

    uAmbient   = glGetUniformLocation( program, "AmbientProduct"  );
    uDiffuse   = glGetUniformLocation( program, "DiffuseProduct"  );
    uSpecular  = glGetUniformLocation( program, "SpecularProduct" );
    uLightPos  = glGetUniformLocation( program, "LightPosition"   );
    uShininess = glGetUniformLocation( program, "Shininess"       );
    uTex       = glGetUniformLocation( program, "Tex"             );
    uEnableTex = glGetUniformLocation( program, "EnableTex"       );

    glUniform4f(uAmbient,    0.2f,  0.2f,  0.2f, 1.0f);
    glUniform4f(uDiffuse,    0.6f,  0.6f,  0.6f, 1.0f);
    glUniform4f(uSpecular,   0.2f,  0.2f,  0.2f, 1.0f);
    glUniform4f(uLightPos,  15.0f, 15.0f, 30.0f, 0.0f);
    glUniform1f(uShininess, 100.0f);

    glEnable(GL_DEPTH_TEST);

    //TgaImage coolImage;
    //if (!coolImage.loadTGA("fur2.tga"))
    //{
    //    printf("Error loading image file\n");
    //    exit(1);
    //}
    //
    //TgaImage earthImage;
    ////if (!earthImage.loadTGA("earth.tga"))
    //if (!earthImage.loadTGA("fur.tga"))
    //{
    //    printf("Error loading image file\n");
    //    exit(1);
    //}

    //
    //glGenTextures( 1, &texture_cube );
    //glBindTexture( GL_TEXTURE_2D, texture_cube );
    //
    //glTexImage2D(GL_TEXTURE_2D, 0, 4, coolImage.width, coolImage.height, 0,
    //             (coolImage.byteCount == 3) ? GL_BGR : GL_BGRA,
    //             GL_UNSIGNED_BYTE, coolImage.data );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    //
    //
    //glGenTextures( 1, &texture_earth );
    //glBindTexture( GL_TEXTURE_2D, texture_earth );
    //
    //glTexImage2D(GL_TEXTURE_2D, 0, 4, earthImage.width, earthImage.height, 0,
    //             (earthImage.byteCount == 3) ? GL_BGR : GL_BGRA,
    //             GL_UNSIGNED_BYTE, earthImage.data );
    //
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    //glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
    // Set texture sampler variable to texture unit 0
    // (set in glActiveTexture(GL_TEXTURE0))
    
    glUniform1i( uTex, 0);
    
    Arcball = new BallData;
    Ball_Init(Arcball);
    Ball_Place(Arcball,qOne,0.75);

    drawableObject *ground = new Ground();
    int groundIndex = manager.addObject(ground);
    
    for (int i = 0; i < 5; i++) {
        drawableObject *penguin = new Penguin();
        penguin->move(i*20, 3, 0);
        int penguinIndex = manager.addObject(penguin);
        if (i == 0) {
                firstPenguinIndex = penguinIndex;
        }
        manager.addSceneToObject(penguinIndex, doPenguin, 5);
        manager.addSceneToObject(penguinIndex, stopObject, 2);
    }
    for (int i = 1; i > -5; i--) {
            drawableObject *penguin = new Penguin();
            penguin->move(i*20, 3, 0);
            int penguinIndex = manager.addObject(penguin);
            manager.addSceneToObject(penguinIndex, doPenguin, 5);
            manager.addSceneToObject(penguinIndex, stopObject, 2);
    }
    eye.y = 20;
    CameraObject *cam1 = new CameraObject(panRight);
    cam1->addCamera(zoomOut);
    manager.addCameraMovement(cam1, 3);

    //manager.addCameraMovement(panRight, 1);
    //manager.addCameraMovement(zoomOut, 3);
    //manager.addCameraMovement(panLeft, 2);
    {
        manager.addCameraMovement(new CameraObject(zoomIn), .2);
        manager.addCameraMovement(new CameraObject(zoomOut), 1);
    }
    {
        manager.addCameraMovement(new CameraObject(zoomIn), .2);
        manager.addCameraMovement(new CameraObject(zoomOut), 1);
    }
    manager.addCameraMovement(new CameraObject(do360), 7.5);
}

/*********************************************************
    PROC: set_colour();
    DOES: sets all material properties to the given colour
    -- don't change
**********************************************************/

void set_colour(float r, float g, float b)
{
    float ambient  = 0.2f;
    float diffuse  = 0.6f;
    float specular = 0.2f;
    glUniform4f(uAmbient,  ambient*r,  ambient*g,  ambient*b,  1.0f);
    glUniform4f(uDiffuse,  diffuse*r,  diffuse*g,  diffuse*b,  1.0f);
    glUniform4f(uSpecular, specular*r, specular*g, specular*b, 1.0f);
}


/*********************************************************
**********************************************************
**********************************************************

    PROC: display()
    DOES: this gets called by the event handler to draw
          the scene, so this is where you need to build
          your ROBOT --  
      
        MAKE YOUR CHANGES AND ADDITIONS HERE

    Add other procedures if you like.

**********************************************************
**********************************************************
**********************************************************/
void display(void)
{
    // Clear the screen with the background colour (set in myinit)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    model_view = mat4(1.0f);
    
    
    model_view *= Translate(0.0f, 0.0f, -15.0f);
    HMatrix r;
    Ball_Value(Arcball,r);

    mat4 mat_arcball_rot(
        r[0][0], r[0][1], r[0][2], r[0][3],
        r[1][0], r[1][1], r[1][2], r[1][3],
        r[2][0], r[2][1], r[2][2], r[2][3],
        r[3][0], r[3][1], r[3][2], r[3][3]);
    model_view *= mat_arcball_rot;
    
    mat4 view = model_view;
    
    
    model_view = Angel::LookAt(eye, ref, up);//just the view matrix;

    glUniformMatrix4fv( uView, 1, GL_TRUE, model_view );

    // Previously glScalef(Zoom, Zoom, Zoom);
    model_view *= Scale(Zoom);

    // Draw Something
    
    manager.drawScene();

    glutSwapBuffers();
    if(Recording == 1)
        FrSaver.DumpPPM(Width, Height) ;
}

/**********************************************
    PROC: myReshape()
    DOES: handles the window being resized 
    
      -- don't change
**********************************************************/

void myReshape(int w, int h)
{
    Width = w;
    Height = h;

    glViewport(0, 0, w, h);

    mat4 projection = Perspective(50.0f, (float)w/(float)h, .5f, 10000.0f);
    glUniformMatrix4fv( uProjection, 1, GL_TRUE, projection );
}

void instructions() 
{
    printf("Press:\n");
    printf("  s to save the image\n");
    printf("  r to restore the original view.\n") ;
    printf("  0 to set it to the zero state.\n") ;
    printf("  a to toggle the animation.\n") ;
    printf("  m to toggle frame dumping.\n") ;
    printf("  q to quit.\n");
}

// start or end interaction
void myMouseCB(int button, int state, int x, int y)
{
    Button = button ;
    if( Button == GLUT_LEFT_BUTTON && state == GLUT_DOWN )
    {
        HVect arcball_coords;
        arcball_coords.x = 2.0*(float)x/(float)Width-1.0;
        arcball_coords.y = -2.0*(float)y/(float)Height+1.0;
        Ball_Mouse(Arcball, arcball_coords) ;
        Ball_Update(Arcball);
        Ball_BeginDrag(Arcball);

    }
    if( Button == GLUT_LEFT_BUTTON && state == GLUT_UP )
    {
        Ball_EndDrag(Arcball);
        Button = -1 ;
    }
    if( Button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN )
    {
        PrevY = y ;
    }


    // Tell the system to redraw the window
    glutPostRedisplay() ;
}

// interaction (mouse motion)
void myMotionCB(int x, int y)
{
    if( Button == GLUT_LEFT_BUTTON )
    {
        HVect arcball_coords;
        arcball_coords.x = 2.0*(float)x/(float)Width - 1.0 ;
        arcball_coords.y = -2.0*(float)y/(float)Height + 1.0 ;
        Ball_Mouse(Arcball,arcball_coords);
        Ball_Update(Arcball);
        glutPostRedisplay() ;
    }
    else if( Button == GLUT_RIGHT_BUTTON )
    {
        if( y - PrevY > 0 )
            Zoom  = Zoom * 1.03 ;
        else 
            Zoom  = Zoom * 0.97 ;
        PrevY = y ;
        glutPostRedisplay() ;
    }
}


void idleCB(void)
{
    if( Animate == 1 )
    {
        // TM.Reset() ; // commenting out this will make the time run from 0
        // leaving 'Time' counts the time interval between successive calls to idleCB
        if( Recording == 0 )
            TIME = TM.GetElapsedTime() ;
        else
            TIME += 0.033 ; // save at 30 frames per second.
        
        
        printf("TIME %f\n", TIME) ;
        glutPostRedisplay() ; 
    }
}
/*********************************************************
     PROC: main()
     DOES: calls initialization, then hands over control
           to the event handler, which calls 
           display() whenever the screen needs to be redrawn
**********************************************************/

int main(int argc, char** argv) 
{
    glutInit(&argc, argv);
    // If your code fails to run, uncommenting these lines may help.
    //glutInitContextVersion(3, 2);
    //glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowPosition (0, 0);
    glutInitWindowSize(Width,Height);
    glutCreateWindow(argv[0]);
    printf("GL version %s\n", glGetString(GL_VERSION));
    glewExperimental = GL_TRUE;
    glewInit();
    
    myinit();

    glutIdleFunc(idleCB) ;
    glutReshapeFunc (myReshape);
    glutKeyboardFunc( myKey );
    glutMouseFunc(myMouseCB) ;
    glutMotionFunc(myMotionCB) ;
    instructions();

    glutDisplayFunc(display);
    glutMainLoop();

    TM.Reset() ;
    return 0;         // never reached
}



void do360() {
        ref.x = penguinX;
        ref.y = penguinY;
        ref.z = penguinZ;
        eye.x = penguinX;
        eye.y = penguinY+3;
        eye.z = 40+penguinZ;
        //if (penguinX < 0) {
        //        eye.x = (-40+penguinX) * sin(TIME);
        //        eye.z = (-40+penguinZ) * cos(TIME);
        //} else {
                //eye.x = (40)*sin(TIME);
                //eye.z = (40)*cos(TIME);
        //}
        //eye.y += .01;
}
void panLeft() {
        eye.x += 1;
        ref.x += 1;
}
void panRight() {
        eye.x -= 1;
        ref.x -= 1;
}
void zoomIn() {
        eye.z -= .1;
}
void zoomOut() {
        eye.z += 1;
}
void moveForward(drawableObject *obj) {
        obj->move(0, 0, .1);
}
void doAnimate(drawableObject *obj) {
        obj->anim(1);
}
void doPenguin(drawableObject *obj) {
        Penguin *peng = dynamic_cast<Penguin*>(obj);
        peng->anim(Penguin::PENGUIN_TALKING);
        peng->anim(Penguin::PENGUIN_FLAPPING);
        peng->anim(Penguin::PENGUIN_LAZYEYES); 
        peng->move(0, 0, .1);
        peng->anim(Penguin::PENGUIN_SLIDING);
        if (peng->getTime(Penguin::PENGUIN_TALKING) > (double)4) {
                peng->stopAnim(Penguin::PENGUIN_TALKING);
        }        
        peng->walk();
        //moveForward(peng);
        
        if (peng->getIndex() == firstPenguinIndex) {
                penguinX = peng->getX();
                penguinY = peng->getY();
                penguinZ = peng->getZ();
        }
}
void stopObject(drawableObject *obj) {
        obj->stopAllAnim();
}

