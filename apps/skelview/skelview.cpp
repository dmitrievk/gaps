// Source file for feature visualizer



// include files

#include "R3Graphics/R3Graphics.h"
#include "RNDataStructures/RNDataStructures.h"
#include "fglut/fglut.h"
#include <vector>



// GLUT defines

#define ENTER 13
#define ESCAPE 27
#define SPACEBAR 32



// program arguments

static const char* prefix = NULL;
static int print_debug = 0;
static int print_verbose = 0;
static int threshold = 20000;
static int maximum_distance = 600;
static RNScalar resolution[3] = { 6, 6, 30 };



// feature variables

static std::vector<RNScalar> predictions;
static std::vector<RNBoolean> truths;
static std::vector<RNBoolean> isCorrect;
static R3Grid *grid = NULL;



// toggle variables

static int feature_index = 0;
static int incorrect_examples = 0;



// transformation variables

static R3Viewer* viewer = NULL;
static R3Point selected_position;
static R3Box world_box;
static R3Affine transformation;



// GLUT variables

static int GLUTwindow = 0;
static int GLUTwindow_height = 1200;
static int GLUTwindow_width = 1200;
static int GLUTmouse[2] = { 0, 0 };
static int GLUTbutton[3] = { 0, 0, 0 };
static int GLUTmodifiers = 0;



// style variables

static RNScalar background_color[3] = { 0, 0, 0 };



////////////////////////////////////////////////////////////////////////
// Input/output functions
////////////////////////////////////////////////////////////////////////

static int ReadCandidates(void)
{
    // get the ground truth file
    char candidate_filename[4096];
    sprintf(candidate_filename, "features/skeleton/%s-%d-%dnm-inference.candidates", prefix, threshold, maximum_distance);

    // open the file
    FILE *fp = fopen(candidate_filename, "rb");
    if (!fp) { fprintf(stderr, "Failed to read %s\n", candidate_filename); return 0; }

    int ncandidates;
    if (fread(&ncandidates, sizeof(int), 1, fp) != 1) return 0;

    for (int iv = 0; iv < ncandidates; ++iv) {
        unsigned long dummy[5];
        if (fread(&dummy, sizeof(unsigned long), 5, fp) != 5) return 0;
        unsigned long ground_truth;
        if (fread(&ground_truth, sizeof(unsigned long), 1, fp) != 1) return 0;
        truths.push_back(ground_truth);
    }

    // return success
    return 1;
}



static int ReadPredictions(void)
{
    // get the prediction filename
    char prediction_filename[4096];
    sprintf(prediction_filename, "results/skeleton/%s-%d-%dnm.results", prefix, threshold, maximum_distance);

    // open the file
    FILE *fp = fopen(prediction_filename, "rb");
    if (!fp) { fprintf(stderr, "Failed to read %s\n", prediction_filename); return 0; }

    int ncandidates;
    if (fread(&ncandidates, sizeof(int), 1, fp) != 1) return 0;
    rn_assertion (ncandidates == (int)truths.size())

    for (int iv = 0; iv < ncandidates; ++iv) {
        RNScalar probability;
        if (fread(&probability, sizeof(RNScalar), 1, fp) != 1) return 0;
        predictions.push_back(probability);

        if (probability > 0.5 && truths[iv]) isCorrect.push_back(TRUE);
        else if (probability < 0.5 && !truths[iv]) isCorrect.push_back(TRUE);
        else isCorrect.push_back(FALSE);
    }

    // return success
    return 1;
}




static int ReadFeature(int index)
{
    // free old grid memory
    if (grid) delete grid;

    // get the filename for this feature
    char feature_filename[4096];
    sprintf(feature_filename, "features/skeleton/%s/%d-%dnm-%05d.h5", prefix, threshold, maximum_distance, index);

    // read the feature into an R3Grid
    R3Grid **grids = RNReadH5File(feature_filename, "main");
    if (!grids) return 0;
    grid = grids[0];

    delete[] grids;

    // return success
    return 1;
}



////////////////////////////////////////////////////////////////////////
// GLUT interface functions
////////////////////////////////////////////////////////////////////////

void GLUTStop(void)
{
    // destroy window
    glutDestroyWindow(GLUTwindow);

    // delete the neuron data
    RNTime start_time;
    start_time.Read();

    if (grid) delete grid;

    // print statistics
    if(print_verbose) {
        printf("Deleted data...\n");
        printf("  Time = %.2f seconds\n", start_time.Elapsed());
        fflush(stdout);
    }

    // exit
    exit(0);
}



void GLUTRedraw(void)
{
    // clear window
    glClearColor(background_color[0], background_color[1], background_color[2], 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // set viewing transformation
    viewer->Camera().Load();

    // set lights
    static GLfloat light0_position[] = { 3.0, 4.0, 5.0, 0.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    static GLfloat light1_position[] = { -3.0, -2.0, -3.0, 0.0 };
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);

    // prologue
    glDisable(GL_LIGHTING);

    // draw feature bounding box
    if (isCorrect[feature_index]) RNLoadRgb(RNwhite_rgb);
    else RNLoadRgb(RNred_rgb);
    world_box.Outline();

    transformation.Push();

    // draw the actual points in 3D
    glBegin(GL_POINTS);
    for (int iz = 0; iz < grid->ZResolution(); ++iz) {
        for (int iy = 0; iy < grid->YResolution(); ++iy) {
            for (int ix = 0; ix < grid->XResolution(); ++ix) {
                int grid_value = (int)(grid->GridValue(ix, iy, iz) + 0.5);
                if (grid_value == 1) {
                    if (truths[feature_index]) RNLoadRgb(RNgreen_rgb);
                    else RNLoadRgb(RNred_rgb);
                    glVertex3f(ix, iy, iz);
                }
                else if (grid_value == 2) {
                    if (truths[feature_index]) RNLoadRgb(RNblue_rgb);
                    else RNLoadRgb(RNyellow_rgb);
                    glVertex3f(ix, iy, iz);
                }
            }
        }
    }
    glEnd();

    transformation.Pop();

    // get the title
    char feature_label[4096];
    if (predictions.size()) {
        const char *ground_truth = NULL;
        if (truths[feature_index]) ground_truth = "YES";
        else ground_truth = "NO";

        const char *prediction = NULL;
        if (predictions[feature_index] > 0.5) prediction = "YES";
        else prediction = "NO";

        sprintf(feature_label, "Feature Visualizer - %d - Ground Truth: %s - Predicted: %s - Probability: %lf\n", feature_index, ground_truth, prediction, predictions[feature_index]);
    }
    else {
        sprintf(feature_label, "Feature Visualizer - %d\n", feature_index);
    }

    // write the feature
    glutSetWindowTitle(feature_label);

    // epilogue
    glEnable(GL_LIGHTING);

    // swap buffers
    glutSwapBuffers();
}



void GLUTResize(int w, int h)
{
    // resize window
    glViewport(0, 0, w, h);

    // resize viewer viewport
    viewer->ResizeViewport(0, 0, w, h);

    // remember window size
    GLUTwindow_width = w;
    GLUTwindow_height = h;

    // redraw
    glutPostRedisplay();
}



void GLUTMotion(int x, int y)
{
    // invert y coordinate
    y = GLUTwindow_height - y;

    // remember mouse position
    int dx = x - GLUTmouse[0];
    int dy = y - GLUTmouse[1];

    // world in hand navigation
    R3Point origin = world_box.Centroid();
    if(GLUTbutton[0])
        viewer->RotateWorld(1.0, origin, x, y, dx, dy);
    else if(GLUTbutton[1])
        viewer->ScaleWorld(1.0, origin, x, y, dx, dy);
    else if(GLUTbutton[2]) {
        viewer->TranslateWorld(1.0, origin, x, y, dx, dy);
    }

    // redisplay if a mouse was down
    if(GLUTbutton[0] || GLUTbutton[1] || GLUTbutton[2]) glutPostRedisplay();

    // remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;
}



void GLUTMouse(int button, int state, int x, int y)
{
    // invert y coordinate
    y = GLUTwindow_height - y;

    // remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;

    // remember button state
    int b = (button == GLUT_LEFT_BUTTON) ? 0 : ((button == GLUT_MIDDLE_BUTTON) ? 1 : 2);
    GLUTbutton[b] = (state == GLUT_DOWN) ? 1 : 0;

    // remember modifiers
    GLUTmodifiers = glutGetModifiers();

    // remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;

    // redraw
    glutPostRedisplay();
}


static void DecrementIndex(void)
{
    if (feature_index) --feature_index;
}



static void IncrementIndex(void)
{
    if (feature_index < (int)truths.size() - 1) ++feature_index;
}


void GLUTSpecial(int key, int x, int y)
{
    // invert y coordinate
    y = GLUTwindow_height - y;

    // remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;

    // remember modifiers
    GLUTmodifiers = glutGetModifiers();

    switch(key) {
        case GLUT_KEY_LEFT: {
            DecrementIndex();
            while (incorrect_examples && feature_index && isCorrect[feature_index])
                DecrementIndex();
            break;
        }

        case GLUT_KEY_RIGHT: {
            IncrementIndex();
            while (incorrect_examples && feature_index < (int)truths.size() - 1 && isCorrect[feature_index])
                IncrementIndex();
        
            break;
        }
    }

    ReadFeature(feature_index);    

    // redraw
    glutPostRedisplay();
}



void GLUTKeyboard(unsigned char key, int x, int y)
{
    // invert y coordinate
    y = GLUTwindow_height - y;

    // remember mouse position
    GLUTmouse[0] = x;
    GLUTmouse[1] = y;

    // remember modifiers
    GLUTmodifiers = glutGetModifiers();

    // keys regardless of projection status
    switch(key) {
        case SPACEBAR: {
            incorrect_examples = 1 - incorrect_examples;
            break;
        }

        case ESCAPE: {
            GLUTStop();
            break;
        }
    }

    // redraw
    glutPostRedisplay();
}



void GLUTInit(int* argc, char** argv)
{
    // open window
    glutInit(argc, argv);
    glutInitWindowPosition(10, 10);
    glutInitWindowSize(GLUTwindow_width, GLUTwindow_height);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    GLUTwindow = glutCreateWindow("Feature Visualizer");

    // initialize background color
    glClearColor(background_color[0], background_color[1], background_color[2], 1.0);

    // initialize lights
    static GLfloat lmodel_ambient[] = { 0.2, 0.2, 0.2, 1.0 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    static GLfloat light0_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glEnable(GL_LIGHT0);
    static GLfloat light1_diffuse[] = { 0.5, 0.5, 0.5, 1.0 };
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);

    // initialize graphics mode
    glEnable(GL_DEPTH_TEST);

    // initialize GLUT callback functions
    glutDisplayFunc(GLUTRedraw);
    glutReshapeFunc(GLUTResize);
    glutKeyboardFunc(GLUTKeyboard);
    glutSpecialFunc(GLUTSpecial);
    glutMouseFunc(GLUTMouse);
    glutMotionFunc(GLUTMotion);
}



void GLUTMainLoop(void)
{
    // run main loop -- never returns
    glutMainLoop();
}



////////////////////////////////////////////////////////////////////////
// Viewer functions
////////////////////////////////////////////////////////////////////////

static R3Viewer* CreateViewer(void)
{
    // start statistics
    RNTime start_time;
    start_time.Read();

    if(world_box.IsEmpty()) RNAbort("Error in CreateViewer - box is empty");
    RNLength r = world_box.DiagonalRadius();
    if(r < 0 || RNIsInfinite(r)) RNAbort("Error in CreateViewer - r must be positive finite");

    // set up camera view looking down the z axis
    static R3Vector initial_camera_towards = R3Vector(0.0, 0.0, -1.5);
    static R3Vector initial_camera_up = R3Vector(0.0, 1.0, 0.0);
    R3Point initial_camera_origin = world_box.Centroid() - initial_camera_towards * 2.5 * r;
    R3Camera camera(initial_camera_origin, initial_camera_towards, initial_camera_up, 0.4, 0.4, 0.1 * r, 1000.0 * r);
    R2Viewport viewport(0, 0, GLUTwindow_width, GLUTwindow_height);
    R3Viewer* viewer = new R3Viewer(camera, viewport);

    // print statistics
    if(print_verbose) {
        printf("Created viewer ...\n");
        printf("  Time = %.2f seconds\n", start_time.Elapsed());
        printf("  Origin = %g %g %g\n", camera.Origin().X(), camera.Origin().Y(), camera.Origin().Z());
        printf("  Towards = %g %g %g\n", camera.Towards().X(), camera.Towards().Y(), camera.Towards().Z());
        printf("  Up = %g %g %g\n", camera.Up().X(), camera.Up().Y(), camera.Up().Z());
        printf("  Fov = %g %g\n", camera.XFOV(), camera.YFOV());
        printf("  Near = %g\n", camera.Near());
        printf("  Far = %g\n", camera.Far());
        fflush(stdout);
    }

    // return viewer
    return viewer;
}



////////////////////////////////////////////////////////////////////////
// Program argument parsing
////////////////////////////////////////////////////////////////////////

static int ParseArgs(int argc, char** argv)
{
    // parse arguments
    argc--; argv++;
    while (argc > 0) {
        if ((*argv)[0] == '-') {
            if (!strcmp(*argv, "-v")) print_verbose = 1;
            else if (!strcmp(*argv, "-debug")) print_debug = 1;
            else if (!strcmp(*argv, "-threshold")) { argv++; argc--; threshold = atoi(*argv); }
            else if (!strcmp(*argv, "-maximum_distance")) { argv++; argc--; maximum_distance = atoi(*argv); }
            else { fprintf(stderr, "Invalid program argument: %s\n", *argv); return 0; }
        } else {
            if (!prefix) { prefix = *argv; } 
            else { fprintf(stderr, "Invalid program argument: %s\n", *argv); return 0; }
        }
        argv++; argc--;
    }

    // error if there is no input name
    if(!prefix) { fprintf(stderr, "Need to supply a prefix file\n"); return 0; }

    // return success
    return 1;
}



////////////////////////////////////////////////////////////////////////
// Main program
////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    // parse program arguments
    if (!ParseArgs(argc, argv)) exit(-1);

    //////////////////////////////////////////////////
    //// Read in the voxel and ground truth files ////
    //////////////////////////////////////////////////

    // read in the ground truth file
    if (!ReadCandidates()) exit(-1);

    if (!ReadPredictions()) exit(-1);

    // read the first feature
    if (!ReadFeature(feature_index)) exit(-1);

    if (print_verbose) {
        printf("Resolution = (%d %d %d)\n", grid->XResolution(), grid->YResolution(), grid->ZResolution());
    }

    // set world box
    world_box = R3Box(0, 0, 0, grid->XResolution() * resolution[RN_X], grid->YResolution() * resolution[RN_Y], grid->ZResolution() * resolution[RN_Z]);
        
    // get the transformation
    transformation = R3Affine(R4Matrix(resolution[RN_X], 0, 0, 0, 0, resolution[RN_Y], 0, 0, 0, 0, resolution[RN_Z], 0, 0, 0, 0, 1));

    // create viewer
    viewer = CreateViewer();
    if (!viewer) exit(-1);

    // initialize GLUT
    GLUTInit(&argc, argv);

    // run GLUT interface
    GLUTMainLoop();

    // return success
    return 1;
}