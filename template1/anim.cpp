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
ShapeData pyrData;

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
/*
 * This class represents all drawable objects
 */
class drawableObject {
        private:
                friend class SceneObject;
        protected:
                double myTime;
                bool isAnimated;

                //ever animation has its own internal time and a boolean whether its currently running
                map<int, bool> animIndexMap;
                map<int, double> animTimeMap;

                double x, y, z;
                double Rx, Ry, Rz;

                //index assigned from a SceneManager
                int index;

                //texture lists (should be filled with texture stuff before calling drawableObject::genTextures())
                vector<GLuint*> texture_stuff;
                vector<TgaImage*> images;
                /* 
                 * this function is meant to be called by derivatives of this class
                 * assumes that the texture_stuff is already filled by some initial program
                 */
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
                /*
                 * meant to be called by derivatives of the class
                 * draws the specific object ignoring the translations and other stuff
                 * should never be called by another class
                 */
                virtual void draw() = 0;
        public:
                double getX() {return x;}
                double getY() {return y;}
                double getZ() {return z;}
                double getXAngle() {return Rx;}
                double getYAngle() {return Ry;}
                double getZAngle() {return Rz;}
                void setX(double x) { this->x = x; }
                void setY(double y) { this->y = y; }
                void setZ(double z) { this->z = z; }
                void setXAngle(double x) { this->Rx = x; }
                void setYAngle(double y) { this->Ry = y; }
                void setZAngle(double z) { this->Rz = z; }
                int getIndex() {return index;}
                virtual void updateScene() {
                                for (map<int, double>::iterator iter = animTimeMap.begin(); iter != animTimeMap.end(); iter++) {
                                        if (isCurrAnimation(iter->first))
                                                iter->second += .02;
                                }
                }

                /*
                 * What is called to draw the object
                 */
                virtual void drawObject() {
                                mvstack.push(model_view);

                                //do translations
                                model_view *= Translate(this->x, this->y, this->z);

                                //then do rotations around its own origin
                                model_view *= RotateX(this->Rx);
                                model_view *= RotateY(this->Ry);
                                model_view *= RotateZ(this->Rz);
                               
                                draw();
                                model_view = mvstack.pop();
                }
                //set the animation of a specific animation index
                virtual void anim(int animIndex) { 
                        if (!isCurrAnimation(animIndex)) {
                                isAnimated = true;
                                animIndexMap[animIndex] = true; 
                                animTimeMap[animIndex] = 0;
                        }
                }
                virtual bool isCurrAnimation(int animIndex) {
                        //checks if either the index cannot be found or the index is mapped to false
                        return (animIndexMap.find(animIndex) != animIndexMap.end() && animIndexMap[animIndex]);
                }
                virtual void stopAnim(int animIndex) {
                        if (isCurrAnimation(animIndex)) { //if the animation is currently animating
                                animIndexMap[animIndex] = false; //set the animation to false
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
                //sets rotation back to where it was before any rotations
                virtual void resetRotation() {
                        rotate(-Rx, -Ry, -Rz); //rotate the complete opposite of your total rotation
                }
                //resets all animations in the object
                virtual void resetAllAnim() { 
                        //set internal time for every animation to 0
                        for (map<int,double>::iterator iter = animTimeMap.begin(); iter != animTimeMap.end(); iter++) {
                                iter->second = 0;
                        }
                }
                virtual void resetAnim(int animIndex) {
                        //set internal time for specific animation to 0
                        if (animTimeMap.find(animIndex) != animTimeMap.end()) {
                                animTimeMap[animIndex] = 0;
                        }
                } 
                void move(double x, double y, double z) {
                        this->x += x;
                        this->y += y;
                        this->z += z;
                }
                void rotate(int angleX, int angleY, int angleZ) {
                        Rx += angleX;
                        Ry += angleY;
                        Rz += angleZ;
                }
                drawableObject() : x(0), y(0), z(0) {
                        myTime = 0;
                        isAnimated = false;
                        Rx = 0;
                        Ry = 0;
                        Rz = 0;
                }
                virtual ~drawableObject() {
                }
};

void drawSphere();
void drawPyramid();
void drawCube();
void drawCone();

class Ground : public drawableObject {
        private:
                void drawGround() {
                        mvstack.push(model_view);
                        set_colour(6, 6, 6);
                        //model_view *= Translate(0,0,0);
                        model_view *= Translate(0, 0, -300);
                        model_view *= Scale(2000, 1, 2000);
                        drawCube();
                        model_view = mvstack.pop();
                        mvstack.push(model_view);
                                set_colour(0, 0, 1);
                                model_view *= Scale(2000, 0, 2000);
                                drawCube();
                        model_view = mvstack.pop();
                }
        public:
                virtual void draw() {
                        drawGround();
                }
                Ground() : drawableObject() {
                }
};
void drawWithTexture(void (*f)(void), GLuint tex);
class Fish : public drawableObject {
	private:
        void drawFish() {
                //draw body
                mvstack.push(model_view);
                       model_view *= Scale(.25, .5, 1);
                       drawSphere(); 
                model_view = mvstack.pop();
        
                //draw tail
                mvstack.push(model_view);
                        model_view *= Translate(0, 0, -1);
                        model_view *= RotateX(180);
                        model_view *= Scale(.1, .5, .1);
                        drawCone();
                model_view = mvstack.pop();
        
                //draw eyes
                mvstack.push(model_view);
                       model_view *= Translate(.25, 0, .5);
                       model_view *= Scale(.1, .1, .1);
                       drawSphere(); 
                model_view = mvstack.pop();
        
                mvstack.push(model_view);
                       model_view *= Translate(-.25, 0, .5);
                       model_view *= Scale(.1, .1, .1);
                       drawSphere(); 
                model_view = mvstack.pop();
        }
        public:
                virtual void draw() {
                        drawFish();
                }
                Fish() : drawableObject() { }
};
class Heart : public drawableObject {
	private:
                void drawHeart() {
                        set_colour(1, 0, 0);
                        mvstack.push(model_view);
                                model_view *= Translate(-.2, 0, 0);
                                model_view *= RotateZ(30);
                                model_view *= Scale(.5, .8, .5);
                                drawSphere();
                        model_view = mvstack.pop();
                        mvstack.push(model_view);
                                model_view *= Translate(.2, 0, 0);
                                model_view *= RotateZ(-30);
                                model_view *= Scale(.5, .8, .5);
                                drawSphere();
                        model_view = mvstack.pop();
                }
	public:
		virtual void draw() {
			drawHeart();
		}
		Heart() : drawableObject() { }
};
class Bird : public drawableObject {
        private:
                void drawBird() {
                        set_colour(1,1,1);
                        //draw body
                        mvstack.push(model_view);
                                model_view *= Scale(.5, .5, 1);
                                drawSphere();
                        model_view = mvstack.pop();
                        //draw beak and eyes
                        mvstack.push(model_view);
                                model_view *= Translate(0, 0, 1);
                                set_colour(1, .5, 0);
                                mvstack.push(model_view);
                                        model_view *= Scale(.3, .3, .5);
                                        model_view *= RotateX(180);
                                        drawCone();
                                model_view = mvstack.pop();
                                set_colour(0, 0, 0);
                                mvstack.push(model_view);
                                        model_view *= Translate(.35, .1, -.3);
                                        model_view *= Scale(.05, .05, .05);
                                        drawSphere();
                                model_view = mvstack.pop();
                                mvstack.push(model_view);
                                        model_view *= Translate(-.35, .1, -.3);
                                        model_view *= Scale(.05, .05, .05);
                                        drawSphere();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                        //draw wings
                        set_colour(.9, .9, .9);
                        mvstack.push(model_view);
                                mvstack.push(model_view);
                                       model_view *= Translate(.5, .1, .1); 
                                       model_view *= RotateZ(10*sin(TIME));
                                       model_view *= Translate(.5, 0, 0);
                                       model_view *= Scale(1, .1, .5);
                                       drawCube();
                                model_view = mvstack.pop();
                                mvstack.push(model_view);
                                       model_view *= Translate(-.5, .1, .1); 
                                       model_view *= RotateZ(-10*sin(TIME));
                                       model_view *= Translate(-.5, 0, 0);
                                       model_view *= Scale(1, .1, .5);
                                       drawCube();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                        //draw tail
                        mvstack.push(model_view);
                                model_view *= Translate(0, 0, -1);
                                model_view *= RotateX(180);
                                model_view *= Scale(.5, .5, .5);
                                drawCone();
                        model_view = mvstack.pop();
                
                        //draw legs
                        set_colour(1, .5, 0);
                        mvstack.push(model_view);
                                mvstack.push(model_view);
                                        model_view *= Translate(.2, -.45, 0);
                                        model_view *= RotateX(-30);
                                        model_view *= RotateX(-abs(10 * sin(TIME)));
                                        model_view *= Translate(0, -0.25, 0);
                                        model_view *= Scale(.1, .5, .1);
                                        drawCube();
                                model_view = mvstack.pop();
                                mvstack.push(model_view);
                                        model_view *= Translate(-.2, -.45, 0);
                                        model_view *= RotateX(-30);
                                        model_view *= RotateX(-abs(10 * sin(TIME)));
                                        model_view *= Translate(0, -0.25, 0);
                                        model_view *= Scale(.1, .5, .1);
                                        drawCube();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
        public:
                virtual void draw() {
                        drawBird();
                }
                Bird() : drawableObject() { }
};
class Whale : public drawableObject {
	private:
                TgaImage *skinImage;
                GLuint *skinTex;
                
		void drawWhale() {
                        mvstack.push(model_view);
                                model_view *= Scale(2, 2, 2);
                                //draws body of whale
                                mvstack.push(model_view);
                                        set_colour(1, 1, 1);
                                        model_view *= Scale(4, 4, 8);
                                        //drawSphere();
                                        drawWithTexture(drawSphere, *texture_stuff[WHALE_SKIN]);
                                model_view = mvstack.pop();
                                //draws first part of tail of whale
                                set_colour(0.3, 0.3, 0.3);
                                mvstack.push(model_view);
                                        model_view *= Translate(0, 0, -7);
                                        model_view *= Scale(3.2, 3.2, 4);
                                        drawCone();
                                model_view = mvstack.pop();
                                //draws second part of tail of whale
                                mvstack.push(model_view);
                                        model_view *= Translate(0, 0, -10);
                                        model_view *= RotateX(180);
                                        model_view *= Scale(4, 1, 2);
                                        drawCone();
                                model_view = mvstack.pop();

                                //draws top fin of whale
                                set_colour(0.3, 0.3, 0.3);
                                mvstack.push(model_view);
                                        model_view *= Translate(0, 4.5, 0);
                                        model_view *= RotateX(90);
                                        model_view *= RotateZ(45);
                                        model_view *= Scale(2, 2, 2);
                                        drawPyramid();
                                model_view = mvstack.pop();

                                set_colour(0.3, 0.3, 0.3);
                                mvstack.push(model_view);
                                        //draws side fins of whale
                                        mvstack.push(model_view);
                                                model_view *= Translate(4, 0, 0);
                                                model_view *= RotateZ(-30);
                                                model_view *= RotateY(-5);
                                                model_view *= RotateZ(10 * sin(TIME));
                                                model_view *= Translate(2, 0, 0);
                                                model_view *= Scale(4, .5, 4);
                                                drawCube();
                                        model_view = mvstack.pop();

                                        mvstack.push(model_view);
                                                model_view *= Translate(-4, 0, 0);
                                                model_view *= RotateZ(30);
                                                model_view *= RotateY(5);
                                                model_view *= RotateZ(-10 * sin(TIME));
                                                model_view *= Translate(-2, 0, 0);
                                                model_view *= Scale(4, .5, 4);
                                                drawCube();
                                        model_view = mvstack.pop();
                                model_view = mvstack.pop();
                                
                                //the eyes of the whale
                                mvstack.push(model_view);
                                        mvstack.push(model_view);
                                                model_view *= Translate(3, -.4, 5);
                                                model_view *= Scale(.2, .2, .2);
                                                drawSphere();
                                        model_view = mvstack.pop();

                                        mvstack.push(model_view);
                                                model_view *= Translate(-3, -.4, 5);
                                                model_view *= Scale(.2, .2, .2);
                                                drawSphere();
                                        model_view = mvstack.pop();
                                model_view = mvstack.pop();
                        model_view = mvstack.pop();
                }
                virtual void genTextures() {
                        skinImage = new TgaImage();
                        skinTex = new GLuint;

                        if (!skinImage->loadTGA("whaleskin.tga")) {
                                printf("Error loading image file\n");
                                exit(1);
                        }

                        images.push_back(skinImage);
                        texture_stuff.push_back(skinTex);
                        
                        drawableObject::genTextures();
                }
                enum WhaleTextures {
                        WHALE_SKIN
                };
        public:

                virtual void draw() {
                        drawWhale();
                }
                Whale() : drawableObject() { genTextures(); }
};
class Penguin : public drawableObject {
	private:
                //specific animation variables
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
                drawableObject *fish;

                //texture variables
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
                        if (isCurrAnimation(this->PENGUIN_WITHFISH)) {
                                fish = new Fish();
                                fish->move(this->getX(), this->getY(), this->getZ() + .2);
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
                                //if penguin is sliding rotate penguin's head by 90 degrees
                                if (isCurrAnimation(this->PENGUIN_SLIDING)) {
                                       model_view *= RotateX(-90);
                                } else {
                                
                                }
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
                void drawPenguin() {
                        mvstack.push(model_view); 
                                //if the penguin is sliding rotate whole penguin 90 degrees 
                                if (isCurrAnimation(this->PENGUIN_SLIDING)) {
                                        model_view *= RotateX(90);
                                }
                                drawPenguinBody();
                                drawPenguinHead();
                                drawPenguinArms();
                                drawPenguinLegs();
                        model_view = mvstack.pop();
                }
                void genTextures() {
                        furOne = new TgaImage();
                        furTwo = new TgaImage();
                        furTexOne = new GLuint;
                        furTexTwo = new GLuint;
                        //load images from filesystem
                        if (!furOne->loadTGA("fur.tga")) {
                                printf("Error loading image file\n");
                                exit(1);
                        }
                        if (!furTwo->loadTGA("fur2.tga")) {
                                printf("Error loading image file\n");
                                exit(1);
                        }
                        //push the texture stuff to the corresponding vectors
                        images.push_back(furOne);
                        images.push_back(furTwo);
                        texture_stuff.push_back(furTexOne);
                        texture_stuff.push_back(furTexTwo);
                        //call drawableObject::genTextures() (actually generates the textures)
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
                        PENGUIN_SLIDING,
                        PENGUIN_WITHFISH
                };
                void walk() {
                        //if the penguin is in the middle of walking
                        if (getTime(Penguin::PENGUIN_LEFTLEG) < walkTime && getTime(Penguin::PENGUIN_RIGHTLEG) < walkTime) {
                                //animate the current leg
                                if (rightLeg) {
                                        anim(Penguin::PENGUIN_RIGHTLEG);
                                } else {
                                        anim(Penguin::PENGUIN_LEFTLEG);
                                }
                        //if the penguin has just finished walking
                        } else if (getTime(Penguin::PENGUIN_LEFTLEG) >= walkTime || getTime(Penguin::PENGUIN_RIGHTLEG) >= walkTime) {
                                //switch legs
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
                        drawPenguin();
                }
                virtual void anim(int animIndex) {
                        if (animIndex == this->PENGUIN_ROUNDEYES || animIndex == this->PENGUIN_LAZYEYES) {
                                typeOfFace = animIndex;
                        } else {
                                drawableObject::anim(animIndex);
                        }
                }
                Penguin() : walkTime(1) {
                        beakRot = 10;
                        beakVel = 2;
                        flapAngle = 0; 
                        typeOfFace = PENGUIN_LAZYEYES;
                        flapVel = 0;    
                        legLeftRot = 0;     
                        legLeftVel = 3;     
                        legRightRot = 0;
                        legRightVel = 3;
                        rightLeg = false;
                        fish = NULL;
                        genTextures();
                }
                ~Penguin() {
                        if (fish != NULL) {
                                delete fish;
                        }
                }
};

/*
 * Represents a camera object which holds multiple camera movements. 
 * This allows multiple camera movements to be used at the same time.
 */
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
/*
 * Manages CameraObjects and how long each CameraObject takes to play
 */ 
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
                void updateCurrCamera() {
                        //if there isn't a current camera movement playing
                        if (currCamera == NULL) {
                                //if there is still movement scenes left
                                if (cameraScenes.size() != 0) {
                                    //load a new movement scene
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
                        } else if (TIME > startTime && TIME < endTime) { // a current movement scene is currently playing
                                currCamera->play();
                        } else if (TIME >= endTime) { //if a current movement scene is done
                                //clean up the current camera
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

/*
 * Represents a drawableObject and all of the animations and movements for that drawableObject
 */
class SceneObject {
        private:
                queue<sceneCall> scenes;
                queue<double> sceneTimes;
                drawableObject* object;
                sceneCall currScene;
                sceneCall gravFunc;
                double startTime, endTime;
        public:
                drawableObject* getObject() {return object;}
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
                        gravFunc = NULL;
                }
                SceneObject(drawableObject *obj, sceneCall gravFunc) {
                        this->object = obj;
                        startTime = 0;
                        endTime = 0;
                        currScene = NULL;
                        this->gravFunc = gravFunc;
                }
                ~SceneObject() {
                        delete object;
                }
                void setGravity(sceneCall gravFunc) {
                        this->gravFunc = gravFunc;
                } 
                void updateCurrScene() {
                        //do gravity function
                        if (gravFunc != NULL) gravFunc(object);
                        object->updateScene(); 
                        //if there isn't a current scene playing
                        if (currScene == NULL) {
                                if (scenes.size() != 0) { //if there is stuff left in the scene queue
                                    //load the new scene
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
                        //object->drawObject();
                } 
};
/*
 * Manages all of the scenes for every object and every camera movement
 */
class SceneManager {
        private:
                vector<SceneObject*> sceneObjs;
                CameraManager camManager; 
        public:
                void drawObjects() {
                        for (size_t i = 0; i < sceneObjs.size(); i++) {
                                sceneObjs[i]->getObject()->drawObject();
                        }
                }
                void updateScene() {
                        //play current scene for every object
                        for (size_t i = 0; i < sceneObjs.size(); i++) {
                                sceneObjs[i]->updateCurrScene();
                        }
                        //play current camera movement
                        camManager.updateCurrCamera();
                }                
                int addObject(drawableObject* obj) {
                        SceneObject *newObj = new SceneObject(obj);
                        sceneObjs.push_back(newObj);
                        int index = sceneObjs.size() - 1;                        
                        sceneObjs[index]->setIndex(index);
                        return index;
                }
                drawableObject* getObject(int index) { return sceneObjs[index]->getObject(); }
                int addObject(drawableObject *obj, sceneCall gravFunc) {
                        int index = addObject(obj);
                        sceneObjs[index]->setGravity(gravFunc);
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

//draws the primitive with the certain texture
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
void lookFromSidePenguin(drawableObject *peng);
void lookFromSideWhale();
void lookFromFrontPenguin();
void lookFromFrontWhale();
void lookFromBackPenguin();
void zoomIn();
void zoomOut();
void doAnimate(drawableObject *obj);
void moveForward(drawableObject *obj);
void doPenguin(drawableObject *obj);
void doPenguinBackwards(drawableObject *obj);
void stopObject(drawableObject *obj);
void doGravity(drawableObject *obj);
void doWhale(drawableObject *obj);

Penguin *myPeng;
Whale *myWhale;
SceneManager manager;
int firstPenguinIndex;
vector<vec3*> penguinPositions;
vector<vec3*> fishPositions;
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
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( cubeData.vao );
    glDrawArrays( GL_TRIANGLES, 0, cubeData.numVertices );
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
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( sphereData.vao );
    glDrawArrays( GL_TRIANGLES, 0, sphereData.numVertices );
}
void drawPyramid(void)
{
    glUniformMatrix4fv( uModelView, 1, GL_TRUE, model_view );
    glBindVertexArray( pyrData.vao );
    glDrawArrays( GL_TRIANGLES, 0, pyrData.numVertices );
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
map<int, double> objVelocMap;
map<int, bool> objWaterMap;
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

void doNothing() {
}
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
    generatePyramid(program, &pyrData);
    

    uModelView  = glGetUniformLocation( program, "ModelView"  );
    uProjection = glGetUniformLocation( program, "Projection" );
    uView       = glGetUniformLocation( program, "View"       );

    glClearColor( 0.5, 0.5, 1.0, 1.0 ); // dark blue background

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

    glUniform1i( uTex, 0);
    
    Arcball = new BallData;
    Ball_Init(Arcball);
    Ball_Place(Arcball,qOne,0.75);


    manager.addCameraMovement(new CameraObject(do360), 7);

    drawableObject *ground = new Ground();
    int groundIndex = manager.addObject(ground);

    drawableObject *whale = new Whale();
    whale->rotate(0, 180, 0);
    whale->move(0, -10, 750);
    int whaleIndex = manager.addObject(whale,doGravity);
    manager.addSceneToObject(whaleIndex, stopObject, 32);
    manager.addSceneToObject(whaleIndex, doWhale, 7);
    myWhale = dynamic_cast<Whale*>(manager.getObject(whaleIndex));

    double origin = 400;
    drawableObject *mainPeng = new Penguin();
    mainPeng->move(0, 3, origin);
    int mainPengIndex = manager.addObject(mainPeng, doGravity);

    //add main penguin
    eye.x = mainPeng->getX();
    eye.y = mainPeng->getY()+3;
    eye.z = mainPeng->getZ()+40;
    firstPenguinIndex = mainPengIndex;
    myPeng = dynamic_cast<Penguin*>(manager.getObject(firstPenguinIndex));
    manager.addSceneToObject(mainPengIndex, stopObject, 7);
    manager.addSceneToObject(mainPengIndex, doPenguin, 20);
    manager.addSceneToObject(mainPengIndex, doPenguinBackwards, 20);
    manager.addSceneToObject(mainPengIndex, stopObject, 2);

    //add spectating penguins
    penguinPositions.push_back(new vec3(10, 3, origin+8));
    penguinPositions.push_back(new vec3(15, 3, origin-5));
    penguinPositions.push_back(new vec3(30, 3, origin));
    penguinPositions.push_back(new vec3(-20, 3, origin-10));
    penguinPositions.push_back(new vec3(-30, 3, origin));
    penguinPositions.push_back(new vec3(-20, 3, origin-15));
    penguinPositions.push_back(new vec3(-35, 3, origin-20));
    penguinPositions.push_back(new vec3(-40, 3, origin-15));
    penguinPositions.push_back(new vec3(40, 3, origin-15));

    penguinPositions.push_back(new vec3(-100, 3, origin+150));
    penguinPositions.push_back(new vec3(-80, 3, origin+140));
    penguinPositions.push_back(new vec3(-90, 3, origin+153));
    penguinPositions.push_back(new vec3(-100, 3, origin+120));
    penguinPositions.push_back(new vec3(-80, 3, origin+110));
    penguinPositions.push_back(new vec3(-90, 3, origin+123));

    penguinPositions.push_back(new vec3(30, 3, origin+130));
    penguinPositions.push_back(new vec3(25, 3, origin+135));
    penguinPositions.push_back(new vec3(35, 3, origin+140));
    
    //penguinPositions.push_back(new vec3(-50, 5, -50));
    //penguinPositions.push_back(new vec3(-70, 5, -50));
    //penguinPositions.push_back(new vec3(-50, 5, -70));
    //penguinPositions.push_back(new vec3(50, 5, -50));
    //penguinPositions.push_back(new vec3(100, 5, -100));
    //penguinPositions.push_back(new vec3(80, 5, -120));
    //penguinPositions.push_back(new vec3(-50, 5, -120));
    //
    //penguinPositions.push_back(new vec3(100, 5, 100));
    //penguinPositions.push_back(new vec3(80, 5, 80));
    //penguinPositions.push_back(new vec3(80, 5, 120));

    //penguinPositions.push_back(new vec3(-20, 5, 150));
    //penguinPositions.push_back(new vec3(-120, 5, 120));
    //penguinPositions.push_back(new vec3(-150, 5, 200));
    //penguinPositions.push_back(new vec3(120, 5, 180));
    for (size_t i = 0; i < penguinPositions.size(); i++) {
            drawableObject *newPeng = new Penguin();
            newPeng->move(penguinPositions[i]->x, penguinPositions[i]->y, penguinPositions[i]->z);
            int newPengIndex = manager.addObject(newPeng, doGravity);
            manager.addSceneToObject(newPengIndex, stopObject, 15);
    }
    //remove spectating penguin positions
    for (size_t i = 0; i < penguinPositions.size(); i++) {
            delete penguinPositions[i];
    }

    //add spectating fish
    for (size_t i = 0; i < fishPositions.size(); i++) {
            drawableObject *newFish = new Fish();
            newFish->move(fishPositions[i]->x, fishPositions[i]->y, fishPositions[i]->z);
            int newFishIndex = manager.addObject(newFish);
            manager.addSceneToObject(newFishIndex, stopObject, 12);
    }
    //remove spectating fish positions
    for (size_t i = 0; i < fishPositions.size(); i++) {
            delete fishPositions[i];
    }
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
double lastTime = 0;
int frames = 0;
int FPS = 0;
double LASTTIME = 0;
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
    frames++; 

    LASTTIME = TIME;
    //manager.drawScene();
    manager.drawObjects();
    if ((TIME - lastTime) >= 1) {
            FPS = frames;
            frames = 0;
            lastTime = TIME;
    }



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

    mat4 projection = Perspective(50.0f, (float)w/(float)h, .5f, 3000.0f);
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

        //manager.updateScene();        
        manager.updateScene();
        printf("FPS: %d\n", FPS);
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
        eye.x = (40)*sin(TIME);
        eye.x += TIME;
        eye.z = (40)*cos(TIME)+myPeng->getZ();
        ref.x = myPeng->getX();
        ref.y = myPeng->getY();
        ref.z = myPeng->getZ();
        if (TIME < 1.67)
                eye.z -= TIME/2;
        else {
                eye.z += TIME/2;
        }
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
const double pengVelocity = 20;
void doPenguin(drawableObject *obj) {
        Penguin *peng = dynamic_cast<Penguin*>(obj);
        peng->anim(Penguin::PENGUIN_TALKING);
        peng->anim(Penguin::PENGUIN_FLAPPING);
        peng->anim(Penguin::PENGUIN_LAZYEYES); 
        //peng->move(0, 0, TIME);
        double dt = TIME - LASTTIME;
        peng->move(0, 0, pengVelocity * dt);
        
        //if (peng->getTime(Penguin::PENGUIN_TALKING) > (double)4) {
        //        peng->stopAnim(Penguin::PENGUIN_TALKING);
        //}        
        if (peng->getZ() >= 900) {
                peng->anim(Penguin::PENGUIN_SLIDING);
        }
        peng->walk();
        if (peng->getIndex() == firstPenguinIndex) {
                //if (peng->getZ() < 700)
                        lookFromSidePenguin(peng);
        }
}
void doPenguinBackwards(drawableObject *obj) {
        Penguin *peng = dynamic_cast<Penguin*>(obj);
        peng->anim(Penguin::PENGUIN_TALKING);
        peng->anim(Penguin::PENGUIN_FLAPPING);
        peng->anim(Penguin::PENGUIN_LAZYEYES); 
        peng->setYAngle(180);
        double dt = TIME - LASTTIME;
        peng->move(0, 0, -pengVelocity * dt);
        if (peng->getZ() < 700) {
                peng->stopAnim(Penguin::PENGUIN_SLIDING);
        }
        peng->walk();
        if (peng->getIndex() == firstPenguinIndex) {
                //if (peng->getZ() < 700) 
                        lookFromSidePenguin(peng);
        }
}
void doWhale(drawableObject *obj) {
        Whale *whale = dynamic_cast<Whale*>(obj);
        whale->move(0, 0, -1);
        //lookFromSideWhale();
}
void doGravity(drawableObject *obj) {
        if (objVelocMap.find(obj->getIndex()) == objVelocMap.end()) 
                objVelocMap[obj->getIndex()] = 0.0;
        if (objWaterMap.find(obj->getIndex()) == objWaterMap.end())
                objWaterMap[obj->getIndex()] = false;
        double acceleration = 9.8;
        if (obj->getZ() < 700) { //if the object is going into land 
              if (objWaterMap[obj->getIndex()] == true) { //if the object was in water before
                      objVelocMap[obj->getIndex()] = -20; //do a jump (only once)
                      objWaterMap[obj->getIndex()] = false;
              }
        } else { //if the object is in water
              if (objWaterMap[obj->getIndex()] == false) { //if the object was on land before
                      objVelocMap[obj->getIndex()] = -20; //do a jump (only once)
                      objWaterMap[obj->getIndex()] = true;
              }
        }
        double yValue = (obj->getY() - ((objVelocMap[obj->getIndex()]*.2) + (.5*acceleration*.2*.2)));
        if (obj->getZ() < 700) {
              if (yValue < 3) yValue = 3;
        } else {
              if (yValue < -5) yValue = -5;
        }

        obj->setY(yValue);
        objVelocMap[obj->getIndex()] += acceleration*.2;
}
void stopObject(drawableObject *obj) {
        obj->stopAllAnim();
        obj->resetAllAnim();
}
void lookFromFrontPenguin() {
        eye.x = myPeng->getX();
        eye.y = myPeng->getY()+2;
        eye.z = myPeng->getZ() + 40;
        ref.x = myPeng->getX();
        ref.y = myPeng->getY();
        ref.z = myPeng->getZ();
}
void lookFromFrontWhale() {
        eye.x = myWhale->getX();
        eye.y = myWhale->getY()+2;
        eye.z = myWhale->getZ() + 40;
        ref.x = myWhale->getX();
        ref.y = myWhale->getY();
        ref.z = myWhale->getZ();
}
void lookFromBackPenguin() {
        eye.x = myPeng->getX();
        eye.y = myPeng->getY();
        eye.z = myPeng->getZ()-40;
        ref.x = myPeng->getX();
        ref.y = myPeng->getY();
        ref.z = myPeng->getZ();
}

void lookFromSideWhale() {
        eye.x = myWhale->getX() + 80;
        if (myWhale->getZ() >= 700) {
                eye.y = myWhale->getY() + 2;
        } else {
                eye.y = myWhale->getY();
        }
        eye.z = myWhale->getZ();
        ref.x = myWhale->getX();
        ref.y = myWhale->getY();
        ref.z = myWhale->getZ();
}
void lookFromSidePenguin(drawableObject *peng) {
        eye.x = peng->getX() + 40;
        if (peng->getZ() >= 700) {
                eye.y = peng->getY() + 2;
        } else {
                eye.y = peng->getY();
        }
        eye.z = peng->getZ();
        ref.x = peng->getX();
        ref.y = peng->getY();
        ref.z = peng->getZ();
}

