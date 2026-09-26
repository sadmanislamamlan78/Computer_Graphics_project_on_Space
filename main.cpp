/* =========================================================================
 *  SPACE SHOOTER  +  SOLAR SYSTEM  (one project, three modes)
 *  -----------------------------------------------------------------------
 *  This file merges two previously-separate GLUT programs into one:
 *
 *    1. SPACE SHOOTER - the original 2D two-player game
 *    2. SOLAR SYSTEM  - a 3D solar system viewer (planets, moon, rings,
 *       asteroid belt, a roaming rocket, mouse-look camera).
 *    3.Rocket Simulation Game
 *
 *  You reach the Solar System from the main menu ("Solar System" button,
 *  next to "Start Game"). From inside it, ESC or Q returns to the menu
 *  instead of quitting the whole program - quitting the whole program is
 *  now only the menu's "Quit" button or closing the window.
 *
 *  -----------------------------------------------------------------------
 *  BUILDING AND RUNNING
 *  -----------------------------------------------------------------------
 *  You need OpenGL plus GLUT (freeglut is the usual modern version), and
 *  now also a depth buffer, which is why GLUT_DEPTH was added to
 *  glutInitDisplayMode() in main() (the original shooter didn't request
 *  one, since it never needed depth testing).
 *
 *    Code::Blocks : create a "GLUT project", replace the generated main file
 *                   with this one, and make sure freeglut's include folder,
 *                   lib folder and DLL are set up in the project options.
 *    g++          : g++ main.cpp -o game -lfreeglut -lopengl32 -lglu32
 *    Linux        : g++ main.cpp -o game -lglut -lGL -lGLU
 *    macOS        : g++ main.cpp -o game -framework OpenGL -framework GLUT
 *
 *  -----------------------------------------------------------------------
 *  CONTROLS
 *  -----------------------------------------------------------------------
 *  MENU: click a button. "Solar System" is new, next to "Start Game".
 *
 *  SPACE SHOOTER GAME
 *    PLAYER 1 (left, amber)   w/a/s/d to move, c to shoot
 *    PLAYER 2 (right, jade)   i/j/k/l to move, m to shoot
 *  Holding an up/down key while shooting angles the laser. A laser that
 *  touches the other saucer takes 5 points off that player's LIFE.
 *
 *  SOLAR SYSTEM
 *    Hold + drag mouse : look around
 *    w / s              : zoom in / out (field of view)
 *    a / d              : strafe camera left / right
 *    j / l              : strafe camera front / back
 *    r                  : reset camera to the free overview
 *    1-9                : lock camera onto Mercury..Pluto
 *    0                  : unlock camera
 *    t                  : toggle the rocket's self-rotation
 *    spacebar           : pause / resume all motion
 *    q / Esc            : back to the main menu
 *
 *  ROCKET SIMULATION - drive the ship yourself through a short mission tour
 *    Up / Down arrow    : pitch
 *    Left / Right arrow : yaw (turn) - also banks the ship visually
 *    w                  : thrust (throttle ramps up smoothly)
 *    s                  : brake (throttle ramps down)
 *    1 / 2 / 3          : toggle the Red / Green / Blue beacon light
 *    r                  : reset ship, missions and lights
 *    q / Esc            : back to the main menu
 *  Fly close to each named target planet, in order, to complete a
 *  mission and score points; the current mission and score are shown
 *  top-right. The three beacon lights are ordinary positional GL lights
 *  (GL_LIGHT1-3) placed 45+ units from the origin - well past Pluto's
 *  27.6-unit orbit and the roaming ship's ~34-unit loop in SOLAR mode -
 *  so they never sit inside a planet's path.
 * ========================================================================= */

#ifdef _WIN32
	#include<windows.h>
#endif
#include<stdio.h>
#include<stdlib.h>
#include<GL/glut.h>
#include<math.h>
#define GL_SILENCE_DEPRECATION

#define XMAX 1200
#define YMAX 700
// How far a ship moves each frame. The game is capped at FRAMES_PER_SECOND
// below, so this works out at roughly 180 world units per second.
#define SPACESHIP_SPEED 3
#define FRAMES_PER_SECOND 60
#define FRAME_DELAY (1000/FRAMES_PER_SECOND)   //milliseconds between frames
#define HIT_INTERVAL 12                        //frames between two laser hits

#define PI 3.14159265358979f   // used only by the Solar System half


// Filled in by reshape(): the width and height of the window in pixels.
// Needed to turn mouse pixel positions into world coordinates, AND (new)
// to compute the Solar System's perspective aspect ratio every frame.
GLint m_viewport[4];
bool mButtonPressed = false;      //is the left mouse button down right now?
float mouseX, mouseY;             //cursor position, converted to world units
enum view {INTRO, MENU, INSTRUCTIONS, GAME, GAMEOVER, SOLAR, ROCKET_SIM};

// One slot per GLUT special key (arrows). Same idea as keyStates[] above -
// polled every frame instead of reacting to single key-down events, so
// held turns feel smooth instead of stuttering on OS key-repeat.
bool specialKeyStates[256] = {false};
view viewPage = INTRO; // initial value
// One slot per character key. keyPressed() sets a slot to true and
// keyReleased() sets it back to false, so keyOperations() can ask "is w
// being held right now?". Reacting to the key-down event alone would make
// the ships stutter, because the operating system repeats held keys slowly.
bool keyStates[256] = {false};
// Which way each laser points: slot 0 means up, slot 1 means down, and
// neither set means straight across at the opponent.
bool laser1Dir[2] = {false};
bool laser2Dir[2] = {false};

unsigned long frameCount = 0;      //how many frames have been drawn so far
unsigned long lastHitFrame1 = 0;   //when each player was last hit, so that a
unsigned long lastHitFrame2 = 0;   //held-down laser cannot drain them instantly
int alienLife1 = 100;
int alienLife2 = 100;
// Ship positions. Both start at x = 500 because ship two is drawn mirrored
// (see gameScreenDisplay), which puts it on the opposite side of the screen.
float xOne = 500, yOne = 0;       //the right-hand ship  - PLAYER 2
float xTwo = 500, yTwo = 0;       //the left-hand ship   - PLAYER 1
bool laser1 = false, laser2 = false;   //is each ship firing?
GLint CI=0;                       //offset into LightColor, to tint the running lights
GLfloat LightColor[][3]={{0.95,0.80,0.40}, {0.62,0.86,0.95}, {0.90,0.55,0.45}};	//running lights

/* ------------------------------------------------------------------------
 * COLOUR PALETTE
 *
 * Every colour in the game is named here instead of being written inline, so
 * the whole look can be re-themed by editing this one block. OpenGL colours
 * are three floats - red, green and blue - each from 0.0 (none) to 1.0 (full).
 * glColor3fv() takes such an array and makes it the "current" colour: every
 * vertex drawn afterwards uses it until the next glColor call.
 *
 * Each player owns one hue: PLAYER 1 is amber, PLAYER 2 is jade. That hue is
 * reused for their alien, their hull, their laser and their LIFE counter, so
 * you can always tell at a glance which half of the screen is yours.
 * ---------------------------------------------------------------------- */

//----- screen text -----
GLfloat colTitle[3]  = {0.95, 0.80, 0.40};		//"SPACE SHOOTER" heading - warm gold
GLfloat colAccent[3] = {0.60, 0.80, 0.95};		//section headings - ice blue
GLfloat colText[3]   = {0.90, 0.92, 0.96};		//body text - soft white
GLfloat colInfo[3]   = {0.58, 0.64, 0.76};		//secondary text - muted slate

//----- the two players -----
GLfloat colTeam1[3]  = {0.93, 0.72, 0.28};		//PLAYER 1 - amber
GLfloat colTeam2[3]  = {0.35, 0.80, 0.50};		//PLAYER 2 - jade
GLfloat colHull1[3]  = {0.52, 0.32, 0.12};		//their hulls: a darker shade of
GLfloat colHull2[3]  = {0.13, 0.40, 0.30};		//the same hue

//----- shared alien and spaceship parts -----
GLfloat colFace[3]   = {0.26, 0.31, 0.52};		//slate indigo
GLfloat colBeak[3]   = {0.88, 0.84, 0.74};		//ivory
GLfloat colEye[3]    = {0.80, 0.93, 1.00};		//pale ice
GLfloat colDome[3]   = {0.62, 0.78, 0.86};		//glass canopy
GLfloat colWheel[3]  = {0.26, 0.28, 0.34};		//graphite
GLfloat colOutline[3]= {0.05, 0.06, 0.09};		//outlines, near-black

//----- menu furniture -----
GLfloat colBorder[3] = {0.35, 0.45, 0.62};		//steel blue frame
GLfloat colButton[3] = {0.80, 0.64, 0.28};		//brass
GLfloat colButtonHot[3]={0.98, 0.82, 0.45};		//brass, lit up under the cursor
GLfloat colButtonText[3]={0.08, 0.08, 0.10};
GLfloat AlienBody[][2]={{-4,9}, {-6,0}, {0,0}, {0.5,9}, {0.15,12}, {-14,18}, {-19,10}, {-20,0},{-6,0}};
GLfloat AlienCollar[][2]={{-9,10.5}, {-6,11}, {-5,12}, {6,18}, {10,20}, {13,23}, {16,30}, {19,39}, {16,38},
						  {10,37}, {-13,39}, {-18,41}, {-20,43}, {-20.5,42}, {-21,30}, {-19.5,23}, {-19,20},
						  {-14,16}, {-15,17},{-13,13},  {-9,10.5}};
GLfloat ALienFace[][2]={{-6,11}, {-4.5,18}, {0.5,20}, {0.,20.5}, {0.1,19.5}, {1.8,19}, {5,20}, {7,23}, {9,29},
						{6,29.5}, {5,28}, {7,30}, {10,38},{11,38}, {11,40}, {11.5,48}, {10,50.5},{8.5,51}, {6,52},
						{1,51}, {-3,50},{-1,51}, {-3,52}, {-5,52.5}, {-6,52}, {-9,51}, {-10.5,50}, {-12,49}, {-12.5,47},
						{-12,43}, {-13,40}, {-12,38.5}, {-13.5,33},{-15,38},{-14.5,32},  {-14,28}, {-13.5,33}, {-14,28},
						{-13.8,24}, {-13,20}, {-11,19}, {-10.5,12}, {-6,11} } ;
GLfloat ALienBeak[][2]={{-6,21.5}, {-6.5,22}, {-9,21}, {-11,20.5}, {-20,20}, {-14,23}, {-9.5,28}, {-7,27}, {-6,26.5},
						{-4.5,23}, {-4,21}, {-6,19.5}, {-8.5,19}, {-10,19.5}, {-11,20.5} };


// The font used for all the on-screen text. GLUT ships a fixed set of fonts;
// change this single line to try another one. The alternatives are
// GLUT_BITMAP_TIMES_ROMAN_24, GLUT_BITMAP_TIMES_ROMAN_10, GLUT_BITMAP_HELVETICA_10,
// GLUT_BITMAP_HELVETICA_12, GLUT_BITMAP_9_BY_15 and GLUT_BITMAP_8_BY_13.
#define UI_FONT GLUT_BITMAP_HELVETICA_18

// Draws a line of text starting at world position (x,y). "Raster" text is made
// of ready-made bitmaps, so it is always the same size on screen no matter how
// the window is scaled or resized.
void displayRasterText(float x ,float y ,float z ,const char *stringToDisplay) {
	glRasterPos3f(x, y, z);
	for(const char* c = stringToDisplay; *c != '\0'; c++){
		glutBitmapCharacter(UI_FONT , *c);
	}
}

// How wide a string will be, measured in world units.
// glutBitmapWidth() answers in screen pixels, but we position everything in the
// world coordinates set up by gluOrtho2D(), so we convert between the two. The
// world is 2400 units wide and the window is m_viewport[2] pixels wide, which
// gives us the number of world units in one pixel. Measuring the text instead
// of hard-coding offsets means the layout stays correct if you change UI_FONT
// or resize the window.
float rasterTextWidth(const char *stringToDisplay) {
	int pixels = 0;
	for(const char* c = stringToDisplay; *c != '\0'; c++)
		pixels += glutBitmapWidth(UI_FONT, *c);
	return pixels * (2400.0f / m_viewport[2]);
}

// Text centred about x.
void displayCenteredRasterText(float x ,float y ,float z ,const char *stringToDisplay) {
	displayRasterText(x - rasterTextWidth(stringToDisplay)/2, y, z, stringToDisplay);
}

// Text that ends at x, used to keep the right-hand LIFE counter off the edge.
void displayRightAlignedRasterText(float x ,float y ,float z ,const char *stringToDisplay) {
	displayRasterText(x - rasterTextWidth(stringToDisplay), y, z, stringToDisplay);
}

// Bitmap fonts come in fixed sizes only, so headings use the stroke font
// instead - it is drawn with lines and can be scaled to any size.
void displayStrokeText(float x ,float y ,float scale ,float thickness ,const char *stringToDisplay) {
	glPushMatrix();
	glTranslatef(x, y, 0);
	glScalef(scale, scale, scale);
	glLineWidth(thickness);
	for(const char* c = stringToDisplay; *c != '\0'; c++){
		glutStrokeCharacter(GLUT_STROKE_ROMAN , *c);
	}
	glPopMatrix();
	glLineWidth(1);
}

// Same, but horizontally centred about x.
void displayCenteredStrokeText(float x ,float y ,float scale ,float thickness ,const char *stringToDisplay) {
	float width = glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)stringToDisplay) * scale;
	displayStrokeText(x - width/2, y, scale, thickness, stringToDisplay);
}

/* =========================================================================
 * SOLAR SYSTEM  (ported in as its own self-contained block)
 * ========================================================================= */

typedef struct
{
    const char *name;

    float size;

    float semimajor;      /* orbit ellipse X extent (0 for the sun) */
    float semiminor;      /* orbit ellipse Z extent */
    float inclination;    /* orbital plane tilt, degrees (0 = flat) */

    float orbitSpeed;     /* degrees per animation tick */
    float selfSpeed;      /* self-rotation degrees per tick */

    float color[3];

    int   hasRing;
    float ringInner;
    float ringOuter;
    float ringColor[3];
    float ringTilt;

    int   isMoon;
    int   parent;         /* index into the same array */
    float moonDistance;   /* used instead of semimajor/semiminor when isMoon */

    int   drawOrbit;

    /* --- runtime state (updated every frame) --- */
    float currentAngle;
    float selfAngle;
    float worldPos[3];

} Planet;

enum
{
    P_SUN = 0,
    P_MERCURY,
    P_VENUS,
    P_EARTH,
    P_MOON,
    P_MARS,
    P_JUPITER,
    P_SATURN,
    P_URANUS,
    P_NEPTUNE,
    P_PLUTO,
    PLANET_COUNT
};

static Planet planets[PLANET_COUNT] =
{
    /* Orbit sizes below are derived from each planet's real distance from
       the sun (semimajor a = sqrt(realAU) * 3.2 / eccentricity, semiminor
       b = a * eccentricity), so the relative proportions between planets
       are meaningfully realistic - Mercury through Mars stay close
       together, there's a real jump out to Jupiter, and the outer giants
       are spread much further apart, same as the actual solar system,
       just compressed by a square-root scale so it's still navigable
       (a literal 1:1 AU scale would put Neptune ~80x farther out than
       Mercury - unusable at this world scale).
       On top of that, every value was walked outward and pushed further
       if needed so that no two orbits (using the semiMINOR axis, since
       that's the tightest clearance between same-oriented ellipses of
       different size) ever come within 0.4 units of each other, counting
       each body's REAL visual extent - not just its own sphere size, but
       Saturn/Uranus's ring outer radius and Earth's moon distance too,
       since those reach well beyond the planet itself. Mercury's inner
       edge also clears the sun's outer glow sphere, not just the sun's
       solid size. See the "orbit sizing" note in solarSystemInit() for
       the values everything else (belt radius, roam radius, camera
       distance) was adjusted to match. */
    /* name       size   semimajor semiminor incl  orbitSpd selfSpd  color                  ring  rIn  rOut ringColor            ringTilt isMoon parent moonDist drawOrbit */
    { "Sun",      1.10f, 0.0f,     0.0f,     0.0f, 0.00000f,0.60f,  {1.00f,0.55f,0.05f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   0 },
    { "Mercury",  0.14f, 3.41f,    3.07f,    0.0f, 1.24585f,0.0102f,{0.70f,0.62f,0.55f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
    { "Venus",    0.22f, 4.26f,    3.83f,    0.0f, 0.48765f,0.0025f,{0.95f,0.80f,0.40f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
    { "Earth",    0.24f, 5.82f,    5.24f,    0.0f, 0.30000f,0.6000f,{0.15f,0.55f,0.85f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
    { "Moon",     0.07f, 0.0f,     0.0f,     0.0f, 2.20000f,0.30f,  {0.72f,0.72f,0.72f},    0,   0,0,  {0,0,0},              0.0f,   1, P_EARTH, 0.55f, 0 },
    { "Mars",     0.18f, 7.34f,    6.61f,    0.0f, 0.15950f,0.5854f,{0.85f,0.30f,0.12f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
    { "Jupiter",  0.55f, 11.04f,   9.94f,    0.0f, 0.02529f,1.4545f,{0.85f,0.55f,0.30f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
    { "Saturn",   0.46f, 15.06f,   13.55f,   0.0f, 0.01018f,1.3458f,{0.92f,0.80f,0.50f},    1,   1.35f,2.2f,{0.90f,0.78f,0.48f}, 22.0f,  0, 0,   0.0f,   1 },
    { "Uranus",   0.34f, 20.94f,   18.85f,   0.0f, 0.00357f,0.8372f,{0.40f,0.85f,0.90f},    1,   1.40f,1.9f,{0.35f,0.72f,0.80f}, 82.0f,  0, 0,   0.0f,   1 },
    { "Neptune",  0.32f, 24.23f,   21.81f,   0.0f, 0.00182f,0.8944f,{0.15f,0.25f,0.95f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
    { "Pluto",    0.10f, 27.60f,   22.63f,   17.0f,0.00121f,0.0939f,{0.85f,0.78f,0.72f},    0,   0,0,  {0,0,0},              0.0f,   0, 0,   0.0f,   1 },
};

/* Number-key (1-9) -> planet index. */
static const int keyToPlanet[9] =
{
    P_MERCURY, P_VENUS, P_EARTH, P_MARS, P_JUPITER,
    P_SATURN, P_URANUS, P_NEPTUNE, P_PLUTO
};

#define NUM_ASTEROIDS 150
static float asteroidAngleOffset[NUM_ASTEROIDS];
static float asteroidRadiusJitter[NUM_ASTEROIDS];
static float asteroidHeightJitter[NUM_ASTEROIDS];
static float asteroidSize[NUM_ASTEROIDS];
static float asteroidShade[NUM_ASTEROIDS];

#define BELT_RADIUS 8.09f   /* sits between Mars (6.61) and Jupiter (9.94), see the Part 1 sizing note above the Planet table */
#define BELT_SPREAD 0.9f

static float beltAngle = 0.0f;
static const float beltSpeed = 0.10f;

#define NUM_STARS 320
static float starPos[NUM_STARS][3];

typedef struct
{
    float x, y, z;
    float yaw, pitch;
    float fov;
    float targetFov;
    float velocityX, velocityZ;

    int locked;      /* -1 = free, else index into planets[] */

    /* mouse drag state */
    int dragging;
    int lastX, lastY;

} Camera;

static Camera cam;

static const float CAM_DEFAULT_X = 0.0f;
static const float CAM_DEFAULT_Y = 32.0f;
static const float CAM_DEFAULT_Z = 62.0f;
static const float CAM_DEFAULT_YAW   = -90.0f;
static const float CAM_DEFAULT_PITCH = -29.0f;
static const float CAM_DEFAULT_FOV   = 55.0f;

static int solarPaused = 0;
/* Procedural surface maps keep the project self-contained while giving every
   planet a distinct visual language. */
static GLuint planetTextures[PLANET_COUNT] = {0};
static GLuint earthCloudTexture = 0;
static GLUquadric *planetTextureQuadric = NULL;

/* Drives both the sun's glow alpha and GL_LIGHT0's intensity each
   frame, so the "dynamic lighting" is one coherent effect rather
   than a glow sprite layered on top of static light. */
static float sunPulse = 1.0f;

static void generateStars(void)
{
    srand(7);

    for(int i = 0; i < NUM_STARS; i++)
    {
        float theta = ((float)(rand() % 3600)) / 10.0f * (PI / 180.0f);
        float phi   = ((float)(rand() % 1800)) / 10.0f * (PI / 180.0f) - (PI / 2.0f);
        float dist  = 55.0f + (float)(rand() % 500) / 10.0f;

        starPos[i][0] = dist * cosf(phi) * cosf(theta);
        starPos[i][1] = dist * sinf(phi);
        starPos[i][2] = dist * cosf(phi) * sinf(theta);
    }
}

static void drawStars(void)
{
    glDisable(GL_LIGHTING);
    glPointSize(1.4f);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_POINTS);
    for(int i = 0; i < NUM_STARS; i++)
        glVertex3f(starPos[i][0], starPos[i][1], starPos[i][2]);
    glEnd();

    glEnable(GL_LIGHTING);
}

static void generateAsteroids(void)
{
    srand(99);

    for(int i = 0; i < NUM_ASTEROIDS; i++)
    {
        asteroidAngleOffset[i]  = (float)(rand() % 3600) / 10.0f;
        asteroidRadiusJitter[i] = -BELT_SPREAD + ((float)(rand() % 1000) / 1000.0f) * (2.0f * BELT_SPREAD);
        asteroidHeightJitter[i] = -0.25f + ((float)(rand() % 1000) / 1000.0f) * 0.5f;
        asteroidSize[i]         = 0.025f + ((float)(rand() % 1000) / 1000.0f) * 0.045f;
        asteroidShade[i]        = 0.30f + ((float)(rand() % 1000) / 1000.0f) * 0.30f;
    }
}

static void drawAsteroidBelt(void)
{
    for(int i = 0; i < NUM_ASTEROIDS; i++)
    {
        float angle = (beltAngle + asteroidAngleOffset[i]) * PI / 180.0f;
        float radius = BELT_RADIUS + asteroidRadiusJitter[i];

        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        float y = asteroidHeightJitter[i];

        GLfloat ambient[]  = { asteroidShade[i]*0.4f, asteroidShade[i]*0.4f, asteroidShade[i]*0.42f, 1.0f };
        GLfloat diffuse[]  = { asteroidShade[i], asteroidShade[i], asteroidShade[i]*1.05f, 1.0f };
        GLfloat specular[] = { 0.15f, 0.15f, 0.15f, 1.0f };

        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 8.0f);

        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef((beltAngle * 4.0f) + asteroidAngleOffset[i] * 2.0f, 1.0f, 0.6f, 0.2f);
        glutSolidSphere(asteroidSize[i], 6, 5);
        glPopMatrix();
    }
}

static void drawOrbitPath(const Planet *p, const float *centerPos)
{
    if(!p->drawOrbit)
        return;

    glDisable(GL_LIGHTING);
    glColor3f(0.22f, 0.22f, 0.28f);

    float inclRad = p->inclination * PI / 180.0f;

    glBegin(GL_LINE_LOOP);
    for(int i = 0; i < 360; i += 3)
    {
        float t = i * PI / 180.0f;
        float lx = p->semimajor * cosf(t);
        float lz0 = p->semiminor * sinf(t);

        float ly = lz0 * sinf(inclRad);
        float lz = lz0 * cosf(inclRad);

        glVertex3f(centerPos[0] + lx, centerPos[1] + ly, centerPos[2] + lz);
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

static void drawRing(float innerRadius, float outerRadius, const float *color)
{
    GLfloat ambient[]  = { color[0]*0.4f, color[1]*0.4f, color[2]*0.4f, 0.9f };
    GLfloat diffuse[]  = { color[0], color[1], color[2], 0.9f };
    GLfloat specular[] = { 0.3f, 0.3f, 0.3f, 0.9f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 20.0f);

    glBegin(GL_QUAD_STRIP);
    for(int i = 0; i <= 360; i += 4)
    {
        float t = i * PI / 180.0f;
        float cx = cosf(t);
        float cz = sinf(t);

        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(cx * innerRadius, 0.0f, cz * innerRadius);
        glVertex3f(cx * outerRadius, 0.0f, cz * outerRadius);
    }
    glEnd();
}
static float textureNoise(float longitude, float latitude, float scale)
{
    return 0.5f + 0.5f * sinf(longitude * scale + sinf(latitude * scale * 0.73f) * 1.7f);
}

static void setTexturePixel(unsigned char *pixels, int index, float red, float green, float blue)
{
    pixels[index + 0] = (unsigned char)(fmaxf(0.0f, fminf(255.0f, red * 255.0f)));
    pixels[index + 1] = (unsigned char)(fmaxf(0.0f, fminf(255.0f, green * 255.0f)));
    pixels[index + 2] = (unsigned char)(fmaxf(0.0f, fminf(255.0f, blue * 255.0f)));
}

static void createPlanetTexture(int planetIndex)
{
    const int width = 256;
    const int height = 128;
    unsigned char pixels[width * height * 3];

    for(int y = 0; y < height; y++)
    {
        float latitude = ((float)y / (float)(height - 1)) * 2.0f - 1.0f;

        for(int x = 0; x < width; x++)
        {
            float longitude = ((float)x / (float)width) * 2.0f - 1.0f;
            float red = 0.5f, green = 0.5f, blue = 0.5f;
            float noise = textureNoise(longitude, latitude, 18.0f);

            if(planetIndex == P_MERCURY)
            {
                float craterField = 0.5f + 0.5f * sinf(longitude * 48.0f + sinf(latitude * 25.0f) * 4.0f);
                float crater = craterField > 0.92f ? 0.20f : 0.0f;
                red = 0.28f + noise * 0.18f + crater;
                green = 0.27f + noise * 0.17f + crater;
                blue = 0.25f + noise * 0.15f + crater;
                if(fabsf(latitude) > 0.84f) red += 0.08f, green += 0.08f, blue += 0.08f;
            }
            else if(planetIndex == P_VENUS)
            {
                float haze = 0.5f + 0.5f * sinf(latitude * 22.0f + noise * 3.0f);
                red = 0.78f + haze * 0.17f;
                green = 0.62f + haze * 0.20f;
                blue = 0.30f + haze * 0.20f;
            }
            else if(planetIndex == P_EARTH)
            {
                float continentA = 1.0f - (fabsf(longitude + 0.48f) * 1.35f + fabsf(latitude - 0.18f) * 1.75f);
                float continentB = 1.0f - (fabsf(longitude - 0.18f) * 1.10f + fabsf(latitude + 0.05f) * 1.45f);
                float continentC = 1.0f - (fabsf(longitude - 0.62f) * 1.55f + fabsf(latitude + 0.48f) * 1.35f);
                float land = fmaxf(continentA, fmaxf(continentB, continentC));
                if(land > 0.18f)
                {
                    float vegetation = fminf(1.0f, (land - 0.18f) * 2.0f);
                    red = 0.08f + 0.18f * vegetation + noise * 0.10f;
                    green = 0.28f + 0.40f * vegetation + noise * 0.12f;
                    blue = 0.12f + 0.12f * vegetation;
                }
                else
                {
                    red = 0.02f + noise * 0.04f;
                    green = 0.18f + noise * 0.08f;
                    blue = 0.55f + noise * 0.18f;
                }
                float clouds = 0.5f + 0.5f * sinf(longitude * 15.0f + sinf(latitude * 31.0f) * 2.0f);
                if(clouds > 0.87f) red = red * 0.45f + 0.55f, green = green * 0.45f + 0.55f, blue = blue * 0.45f + 0.55f;
                if(fabsf(latitude) > 0.86f) red = 0.78f, green = 0.86f, blue = 0.92f;
            }
            else if(planetIndex == P_MARS)
            {
                float basalt = 0.5f + 0.5f * sinf(longitude * 10.0f + latitude * 19.0f);
                red = 0.55f + noise * 0.23f - basalt * 0.13f;
                green = 0.16f + noise * 0.10f - basalt * 0.04f;
                blue = 0.07f + noise * 0.05f;
                if(fabsf(latitude) > 0.84f) red = 0.88f, green = 0.86f, blue = 0.78f;
                if(fabsf(latitude) < 0.12f && fabsf(longitude) < 0.62f) red *= 0.55f, green *= 0.60f, blue *= 0.65f;
            }
            else if(planetIndex == P_JUPITER)
            {
                float bands = 0.5f + 0.5f * sinf(latitude * 42.0f + noise * 2.0f);
                red = 0.72f + bands * 0.20f;
                green = 0.48f + bands * 0.25f;
                blue = 0.28f + bands * 0.25f;
                float stormX = (longitude + 0.38f) / 0.24f;
                float stormY = (latitude + 0.25f) / 0.13f;
                if(stormX * stormX + stormY * stormY < 1.0f) red = 0.65f, green = 0.12f, blue = 0.06f;
            }
            else if(planetIndex == P_SATURN)
            {
                float bands = 0.5f + 0.5f * sinf(latitude * 25.0f + noise * 1.4f);
                red = 0.72f + bands * 0.18f;
                green = 0.60f + bands * 0.18f;
                blue = 0.36f + bands * 0.18f;
            }
            else if(planetIndex == P_URANUS)
            {
                red = 0.28f + noise * 0.035f;
                green = 0.70f + noise * 0.06f;
                blue = 0.78f + noise * 0.08f;
            }
            else if(planetIndex == P_NEPTUNE)
            {
                float streaks = 0.5f + 0.5f * sinf(longitude * 28.0f + latitude * 8.0f);
                red = 0.03f + streaks * 0.05f;
                green = 0.12f + streaks * 0.16f;
                blue = 0.52f + streaks * 0.28f;
                float spotX = (longitude - 0.30f) / 0.19f;
                float spotY = (latitude + 0.18f) / 0.12f;
                if(spotX * spotX + spotY * spotY < 1.0f) red = 0.015f, green = 0.04f, blue = 0.18f;
                if(streaks > 0.94f) red = 0.72f, green = 0.82f, blue = 0.92f;
            }
            else if(planetIndex == P_PLUTO)
            {
                float heartX = longitude * 1.65f;
                float heartY = latitude * 1.65f;
                float heart = heartX * heartX + heartY * heartY - 1.0f;
                bool inHeart = heart * heart * heart - heartX * heartX * heartY * heartY * heartY <= 0.0f && heartY > -0.55f;
                if(inHeart) red = 0.88f, green = 0.74f, blue = 0.72f;
                else if(noise > 0.56f) red = 0.22f + noise * 0.16f, green = 0.23f + noise * 0.15f, blue = 0.25f + noise * 0.14f;
                else red = 0.60f + noise * 0.20f, green = 0.57f + noise * 0.18f, blue = 0.52f + noise * 0.17f;
            }

            int index = (y * width + x) * 3;
            setTexturePixel(pixels, index, red, green, blue);
        }
    }

    glGenTextures(1, &planetTextures[planetIndex]);
    glBindTexture(GL_TEXTURE_2D, planetTextures[planetIndex]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void drawPlanetSurface(const Planet *p)
{
    if(planetTextures[p - planets] == 0 || planetTextureQuadric == NULL)
    {
        glutSolidSphere(p->size, p->isMoon ? 16 : 26, p->isMoon ? 10 : 16);
        return;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, planetTextures[p - planets]);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(1.0f, 1.0f, 1.0f);
    gluSphere(planetTextureQuadric, p->size, 36, 24);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    if(p->name[0] == 'E' && p->name[1] == 'a' && p->name[2] == 'r' && earthCloudTexture != 0)
    {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, earthCloudTexture);
        glColor4f(1.0f, 1.0f, 1.0f, 0.82f);
        gluSphere(planetTextureQuadric, p->size * 1.008f, 36, 24);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

static void createEarthCloudTexture(void)
{
    const int width = 256;
    const int height = 128;
    unsigned char pixels[width * height * 4];

    for(int y = 0; y < height; y++)
    {
        float latitude = ((float)y / (float)(height - 1)) * 2.0f - 1.0f;
        for(int x = 0; x < width; x++)
        {
            float longitude = ((float)x / (float)width) * 2.0f - 1.0f;
            float cloudField = 0.5f + 0.5f * sinf(longitude * 17.0f + sinf(latitude * 29.0f) * 2.5f);
            cloudField += 0.22f * (0.5f + 0.5f * sinf(longitude * 43.0f - latitude * 11.0f));
            int index = (y * width + x) * 4;
            pixels[index + 0] = 255;
            pixels[index + 1] = 255;
            pixels[index + 2] = 255;
            pixels[index + 3] = cloudField > 0.86f ? (unsigned char)(150.0f + cloudField * 80.0f) : 0;
        }
    }

    glGenTextures(1, &earthCloudTexture);
    glBindTexture(GL_TEXTURE_2D, earthCloudTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void updatePlanetPosition(Planet *p)
{
    float base[3] = { 0.0f, 0.0f, 0.0f };

    if(p->isMoon)
    {
        base[0] = planets[p->parent].worldPos[0];
        base[1] = planets[p->parent].worldPos[1];
        base[2] = planets[p->parent].worldPos[2];

        float t = p->currentAngle * PI / 180.0f;
        p->worldPos[0] = base[0] + p->moonDistance * cosf(t);
        p->worldPos[1] = base[1];
        p->worldPos[2] = base[2] + p->moonDistance * sinf(t);
        return;
    }

    if(p->semimajor <= 0.0f) /* the sun: stays at the origin */
    {
        p->worldPos[0] = 0.0f;
        p->worldPos[1] = 0.0f;
        p->worldPos[2] = 0.0f;
        return;
    }

    float t = p->currentAngle * PI / 180.0f;
    float lx = p->semimajor * cosf(t);
    float lz0 = p->semiminor * sinf(t);

    float inclRad = p->inclination * PI / 180.0f;
    float ly = lz0 * sinf(inclRad);
    float lz = lz0 * cosf(inclRad);

    p->worldPos[0] = lx;
    p->worldPos[1] = ly;
    p->worldPos[2] = lz;
}

static void drawPlanet(const Planet *p)
{
    float centerForOrbit[3] = { 0.0f, 0.0f, 0.0f };

    if(p->isMoon)
    {
        centerForOrbit[0] = planets[p->parent].worldPos[0];
        centerForOrbit[1] = planets[p->parent].worldPos[1];
        centerForOrbit[2] = planets[p->parent].worldPos[2];
    }

    drawOrbitPath(p, centerForOrbit);

    glPushMatrix();
    glTranslatef(p->worldPos[0], p->worldPos[1], p->worldPos[2]);
    glRotatef(p->selfAngle, 0.0f, 1.0f, 0.0f);

    if(p->name[0] == 'S' && p->name[1] == 'u' && p->name[2] == 'n')
    {
        /* the sun is emissive - drawn unlit, plain color, with a
           soft pulsing glow layered on top (see sunPulse, which
           also drives GL_LIGHT0's intensity in solarSystemFrame()
           so the glow and the actual scene lighting breathe
           together instead of the glow being a purely cosmetic
           overlay) */
        glDisable(GL_LIGHTING);
        glColor3f(p->color[0], p->color[1], p->color[2]);
        glutSolidSphere(p->size, 40, 24);

        /* Depth-test stays ON but depth WRITES are turned off for
           the glow: it still gets correctly hidden behind anything
           truly in front of it, but it won't punch a hole in the
           depth buffer that could wrongly hide Mercury or the
           rocket if they pass behind its (much larger) glow shell. */
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glColor4f(1.0f, 0.85f, 0.35f, 0.35f * sunPulse);
        glutSolidSphere(p->size * 1.6, 28, 18);

        glColor4f(1.0f, 0.65f, 0.15f, 0.18f * sunPulse);
        glutSolidSphere(p->size * 2.3, 28, 18);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        glEnable(GL_LIGHTING);
    }
    else
    {
        GLfloat ambient[]  = { p->color[0]*0.45f, p->color[1]*0.45f, p->color[2]*0.45f, 1.0f };
        GLfloat diffuse[]  = { p->color[0], p->color[1], p->color[2], 1.0f };
        GLfloat specular[] = { 0.25f, 0.25f, 0.25f, 1.0f };

        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 18.0f);

        if(p->isMoon)
            glutSolidSphere(p->size, 16, 10);
        else
            drawPlanetSurface(p);
    }

    if(p->hasRing)
    {
        glPushMatrix();
        glRotatef(p->ringTilt, 0.0f, 0.0f, 1.0f);
        drawRing(p->ringInner, p->ringOuter, p->ringColor);
        glPopMatrix();
    }

    glPopMatrix();
}

/* ---- rocket ---- */

#define ROAM_BASE_RADIUS   32.0f   /* clears Pluto's new 27.6-unit orbit with margin */
#define ROAM_RADIUS_VARY    2.0f
#define ROAM_HEIGHT_VARY    1.5f
#define ROAM_ANGLE_SPEED    0.05f  /* slow drift around the whole system */
#define ROAM_PHASE_SPEED    0.012f /* much slower than the angle speed, so
                                       the radius/height wobble drifts
                                       gently instead of looking bumpy */

#define ROCKET_SCALE        0.40f

#define NOZZLE_ATTACH_R   0.20f
#define NOZZLE_THROAT_R   0.09f
#define NOZZLE_EXIT_R     0.26f
#define NOZZLE_CONVERGE_LEN 0.16f
#define NOZZLE_BELL_LEN     0.50f

static float rocketRoamAngle = 0.0f;
static float rocketRoamPhase = 0.0f;

static int   rocketSelfRotate = 0; /* off by default; toggled with 't' */
static float rocketSpinX = 0.0f, rocketSpinY = 0.0f, rocketSpinZ = 0.0f;
static const float rocketSpinSpeedX = 0.6f;
static const float rocketSpinSpeedY = 0.9f;
static const float rocketSpinSpeedZ = 0.4f;
static float rocketWorldPos[3] = { 0.0f, 0.0f, 0.0f };
static float rocketFlameFlicker[3] = { 1.0f, 1.0f, 1.0f };

/* Multiplies every flame's brightness - 1.0 for the always-lit roaming
   ship in SOLAR mode, driven by throttle in ROCKET_SIM mode (see Part 3). */
static float rocketThrottleGlow = 1.0f;

/* 0 = the roaming ship's normal colors (SOLAR mode). Nonzero lerps every
   rocket color toward white by that fraction - used in ROCKET_SIM mode
   to make the player's ship read clearly up close instead of the same
   fairly dark hull tones that read fine from a distance. See Part 2. */
static float rocketBrightenAmount = 0.0f;

#define SIM_ROCKET_SCALE 0.55f   /* bigger than the roaming ship's 0.40f */

/* ---- player-controlled rocket (ROCKET_SIM mode) ---- */
static float simRocketX = 0.0f, simRocketY = 6.0f, simRocketZ = 40.0f;
static float simRocketYaw = 180.0f;   /* facing back toward the sun at spawn */
static float simRocketPitch = 0.0f;
static float simRocketRoll = 0.0f;
static float simThrottle = 0.0f;      /* 0..1, ramps smoothly - see Part 3 */

#define SIM_MAX_SPEED_PER_TICK 0.35f
#define SIM_THROTTLE_RAMP      0.02f
#define SIM_THROTTLE_DECAY     0.01f
#define SIM_YAW_RATE           1.1f
#define SIM_PITCH_RATE         0.9f
#define SIM_MAX_PITCH          80.0f
#define SIM_MAX_BANK           28.0f
#define SIM_BANK_EASE          0.08f  /* how quickly roll eases to its target */

/* ---- extra light sources (ROCKET_SIM mode only) ----
   Positioned well clear of every orbit and the old roaming ship's loop:
   Pluto's orbit reaches out to 27.6 units, and the roaming ship's loop
   in SOLAR mode goes out to about 34. All three of these sit past 45
   units from the origin, so they can never end up inside a planet's
   path or collide visually with anything else in the scene. */
#define NUM_EXTRA_LIGHTS 3

typedef struct
{
    const char *name;
    float pos[3];
    float color[3];
    int enabled;      /* toggled with '1'/'2'/'3' - see rocketSimHandleKey() */
} ExtraLight;

static ExtraLight extraLights[NUM_EXTRA_LIGHTS] =
{
    { "Red Beacon",   { 45.0f,  12.0f, -10.0f}, {1.00f, 0.25f, 0.20f}, 0 },
    { "Green Beacon", {-35.0f, -18.0f,  30.0f}, {0.30f, 1.00f, 0.35f}, 0 },
    { "Blue Beacon",  {  5.0f,  30.0f,  45.0f}, {0.35f, 0.55f, 1.00f}, 0 },
};

/* ---- missions (ROCKET_SIM mode only) ----
   A short tour: fly close enough to each listed planet, in order. Ordered
   roughly by distance so it plays as an easy-to-hard progression. */
#define NUM_MISSIONS 5
static const int missionTargets[NUM_MISSIONS] = { P_MERCURY, P_MARS, P_JUPITER, P_SATURN, P_NEPTUNE };
static const char *missionNames[NUM_MISSIONS] = { "Mercury", "Mars", "Jupiter", "Saturn", "Neptune" };

static int currentMission = 0;
static int missionScore = 0;
static int allMissionsComplete = 0;
static float missionMessageTimer = 0.0f; /* frames left to show "complete!" */

static float rocketClamp01(float v)
{
    if(v > 1.0f) return 1.0f;
    if(v < 0.0f) return 0.0f;
    return v;
}

// Lerps a color channel toward white by "amount" (0 = unchanged, 1 = white).
// Used instead of a flat multiplier because several rocket hull colors are
// quite dark (near-navy, near-maroon) - multiplying a dark value by, say,
// 1.5 barely changes it, but lerping toward white visibly lifts it while
// still keeping each part's distinguishing hue.
static float rocketBrighten(float v, float amount)
{
    return rocketClamp01(v + (1.0f - v) * amount);
}

static void rocketSetMaterial(float ar, float ag, float ab,
                               float dr, float dg, float db,
                               float sr, float sg, float sb,
                               float shininess)
{
    float amt = rocketBrightenAmount;

    GLfloat ambient[]  = { rocketBrighten(ar,amt), rocketBrighten(ag,amt), rocketBrighten(ab,amt), 1.0f };
    GLfloat diffuse[]  = { rocketBrighten(dr,amt), rocketBrighten(dg,amt), rocketBrighten(db,amt), 1.0f };
    GLfloat specular[] = { rocketClamp01(sr*1.1f), rocketClamp01(sg*1.1f), rocketClamp01(sb*1.1f), 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

static void rocketClosedCylinder(float radius, float height)
{
    GLUquadric *quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);

    gluCylinder(quad, radius, radius, height, 28, 12);

    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(quad, 0.0, radius, 28, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(quad, 0.0, radius, 28, 1);
    glPopMatrix();

    gluDeleteQuadric(quad);
}

static void rocketEngineNozzle(void)
{
    rocketSetMaterial(0.08f, 0.09f, 0.11f,
                       0.22f, 0.24f, 0.28f,
                       0.80f, 0.82f, 0.85f,
                       85.0f);

    GLUquadric *quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);

    gluCylinder(quad, NOZZLE_ATTACH_R, NOZZLE_THROAT_R, NOZZLE_CONVERGE_LEN, 24, 6);

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, NOZZLE_CONVERGE_LEN);
    gluCylinder(quad, NOZZLE_THROAT_R, NOZZLE_EXIT_R, NOZZLE_BELL_LEN, 24, 10);
    glPopMatrix();

    gluDeleteQuadric(quad);
}

static void rocketExhaustFlame(float flicker)
{
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);

    glColor4f(1.0f, 0.35f, 0.05f, 0.30f * flicker);
    glutSolidCone(0.30 * flicker, 1.35 * flicker, 20, 10);

    glColor4f(1.0f, 0.85f, 0.45f, 0.55f * flicker);
    glutSolidCone(0.15 * flicker, 0.80 * flicker, 20, 10);

    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void rocketFrontWindow(void)
{
    rocketSetMaterial(0.005f, 0.008f, 0.012f, 0.015f, 0.025f, 0.04f, 0.4f, 0.5f, 0.65f, 90.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.82f);
    glScalef(0.78f, 0.48f, 0.08f);
    glutSolidSphere(1.0, 32, 16);
    glPopMatrix();

    rocketSetMaterial(0.01f, 0.04f, 0.08f, 0.03f, 0.15f, 0.28f, 0.65f, 0.80f, 1.0f, 100.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.90f);
    glScalef(0.63f, 0.34f, 0.06f);
    glutSolidSphere(1.0, 32, 16);
    glPopMatrix();
}

static void rocketSideWindow(float xSign)
{
    rocketSetMaterial(0.005f, 0.015f, 0.025f, 0.02f, 0.08f, 0.15f, 0.55f, 0.75f, 0.95f, 100.0f);
    glPushMatrix();
    glTranslatef(0.73f * xSign, 0.0f, 2.05f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glScalef(0.42f, 0.28f, 0.07f);
    glutSolidSphere(1.0, 24, 12);
    glPopMatrix();
}

static void rocketLeftFin(void)
{
    rocketSetMaterial(0.12f, 0.015f, 0.01f, 0.65f, 0.025f, 0.015f, 0.75f, 0.18f, 0.12f, 60.0f);

    glBegin(GL_TRIANGLES);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-0.55f, -0.15f, 1.15f);
    glVertex3f(-1.55f, -0.15f, 0.15f);
    glVertex3f(-0.68f, -0.15f, -0.25f);
    glEnd();

    glPushMatrix();
    glTranslatef(-1.05f, -0.18f, 0.30f);
    glScalef(0.65f, 0.12f, 0.50f);
    glutSolidCube(1.0);
    glPopMatrix();
}

static void rocketRightFin(void)
{
    rocketSetMaterial(0.12f, 0.015f, 0.01f, 0.65f, 0.025f, 0.015f, 0.75f, 0.18f, 0.12f, 60.0f);

    glBegin(GL_TRIANGLES);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.55f, -0.15f, 1.15f);
    glVertex3f(1.55f, -0.15f, 0.15f);
    glVertex3f(0.68f, -0.15f, -0.25f);
    glEnd();

    glPushMatrix();
    glTranslatef(1.05f, -0.18f, 0.30f);
    glScalef(0.65f, 0.12f, 0.50f);
    glutSolidCube(1.0);
    glPopMatrix();
}

static void rocketTopFin(void)
{
    rocketSetMaterial(0.12f, 0.015f, 0.01f, 0.70f, 0.025f, 0.015f, 0.8f, 0.2f, 0.15f, 60.0f);

    glBegin(GL_TRIANGLES);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.45f, 0.95f);
    glVertex3f(0.0f, 1.30f, 0.10f);
    glVertex3f(0.0f, 0.45f, -0.35f);
    glEnd();

    glPushMatrix();
    glTranslatef(0.0f, 0.78f, 0.25f);
    glScalef(0.12f, 0.65f, 0.55f);
    glutSolidCube(1.0);
    glPopMatrix();
}

static void rocketBottomFin(void)
{
    rocketSetMaterial(0.10f, 0.01f, 0.008f, 0.55f, 0.02f, 0.01f, 0.7f, 0.15f, 0.10f, 55.0f);

    glBegin(GL_TRIANGLES);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, -0.45f, 0.95f);
    glVertex3f(0.0f, -1.10f, 0.15f);
    glVertex3f(0.0f, -0.45f, -0.30f);
    glEnd();
}

static void rocketBodyStripe(void)
{
    rocketSetMaterial(0.25f, 0.02f, 0.02f, 0.65f, 0.05f, 0.04f, 0.6f, 0.3f, 0.3f, 50.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 1.55f);
    rocketClosedCylinder(0.735f, 0.10f);
    glPopMatrix();
}

static void rocketMainBody(void)
{
    rocketSetMaterial(0.015f, 0.035f, 0.07f, 0.04f, 0.18f, 0.38f, 0.65f, 0.80f, 1.0f, 90.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.55f);
    rocketClosedCylinder(0.72f, 3.0f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.25f);
    glScalef(0.72f, 0.72f, 0.45f);
    glutSolidSphere(1.0, 32, 16);
    glPopMatrix();

    rocketBodyStripe();
}

static void rocketRearEngineSection(void)
{
    rocketSetMaterial(0.05f, 0.055f, 0.06f, 0.18f, 0.20f, 0.23f, 0.85f, 0.88f, 0.90f, 90.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.75f);
    glScalef(0.85f, 0.85f, 0.18f);
    glutSolidTorus(0.16, 0.75, 24, 32);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.90f);
    rocketClosedCylinder(0.38f, 0.35f);
    glPopMatrix();
}

static void rocketOneEngine(float xOffset, float flicker)
{
    glPushMatrix();
    glTranslatef(xOffset, 0.0f, -1.20f);

    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    rocketEngineNozzle();
    glPopMatrix();

    glTranslatef(0.0f, 0.0f, -(NOZZLE_CONVERGE_LEN + NOZZLE_BELL_LEN));
    // rocketThrottleGlow is 1.0 in SOLAR mode (roaming ship always lit) and
    // driven by simThrottle in ROCKET_SIM mode, so the flame visibly grows
    // and brightens as the player accelerates instead of staying constant.
    rocketExhaustFlame(flicker * rocketThrottleGlow);

    glPopMatrix();
}

static void rocketEngines(void)
{
    rocketOneEngine(-0.62f, rocketFlameFlicker[0]);
    rocketOneEngine( 0.00f, rocketFlameFlicker[1]);
    rocketOneEngine( 0.62f, rocketFlameFlicker[2]);
}

static void rocketNose(void)
{
    rocketSetMaterial(0.20f, 0.20f, 0.21f, 0.88f, 0.89f, 0.90f, 0.95f, 0.95f, 0.95f, 100.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.35f);
    glutSolidCone(0.70, 1.55, 40, 20);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.10f);
    glScalef(0.72f, 0.72f, 0.38f);
    glutSolidSphere(1.0, 32, 16);
    glPopMatrix();

    rocketSetMaterial(0.30f, 0.30f, 0.32f, 0.75f, 0.75f, 0.78f, 1.0f, 1.0f, 1.0f, 120.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 3.88f);
    glutSolidSphere(0.05, 12, 8);
    glPopMatrix();
}

static void rocketCockpit(void)
{
    rocketSetMaterial(0.005f, 0.008f, 0.015f, 0.015f, 0.025f, 0.04f, 0.45f, 0.55f, 0.70f, 100.0f);
    glPushMatrix();
    glTranslatef(0.0f, -0.18f, 2.55f);
    glRotatef(-10.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.52f, 0.12f, 0.50f);
    glutSolidSphere(1.0, 32, 16);
    glPopMatrix();

    rocketSetMaterial(0.005f, 0.02f, 0.04f, 0.01f, 0.07f, 0.13f, 0.55f, 0.75f, 1.0f, 120.0f);
    glPushMatrix();
    glTranslatef(0.0f, -0.28f, 2.58f);
    glRotatef(-10.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.40f, 0.06f, 0.35f);
    glutSolidSphere(1.0, 32, 16);
    glPopMatrix();
}

static void rocketAntenna(void)
{
    rocketSetMaterial(0.08f, 0.08f, 0.08f, 0.30f, 0.30f, 0.30f, 0.80f, 0.80f, 0.80f, 90.0f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 3.75f);
    rocketClosedCylinder(0.035f, 0.30f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 4.10f);
    glutSolidSphere(0.08, 16, 8);
    glPopMatrix();
}

static void rocketDrawAll(void)
{
    rocketMainBody();
    rocketNose();
    rocketCockpit();
    rocketFrontWindow();
    rocketSideWindow(-1.0f);
    rocketSideWindow(1.0f);
    rocketLeftFin();
    rocketRightFin();
    rocketTopFin();
    rocketBottomFin();
    rocketRearEngineSection();
    rocketEngines();
    rocketAntenna();
}

static void rocketUpdate(void)
{
    rocketRoamAngle += ROAM_ANGLE_SPEED;
    if(rocketRoamAngle >= 360.0f) rocketRoamAngle -= 360.0f;

    rocketRoamPhase += ROAM_PHASE_SPEED;
    if(rocketRoamPhase >= 360.0f) rocketRoamPhase -= 360.0f;

    if(rocketSelfRotate)
    {
        rocketSpinX += rocketSpinSpeedX;
        if(rocketSpinX >= 360.0f) rocketSpinX -= 360.0f;
        rocketSpinY += rocketSpinSpeedY;
        if(rocketSpinY >= 360.0f) rocketSpinY -= 360.0f;
        rocketSpinZ += rocketSpinSpeedZ;
        if(rocketSpinZ >= 360.0f) rocketSpinZ -= 360.0f;
    }

    for(int i = 0; i < 3; i++)
        rocketFlameFlicker[i] = 0.70f + 0.30f * ((float)(rand() % 1000) / 1000.0f);

    float angleRad = rocketRoamAngle * PI / 180.0f;
    float phaseRad = rocketRoamPhase * PI / 180.0f;

    float radius = ROAM_BASE_RADIUS + ROAM_RADIUS_VARY * sinf(phaseRad);
    float height = ROAM_HEIGHT_VARY * cosf(phaseRad);

    rocketWorldPos[0] = radius * cosf(angleRad);
    rocketWorldPos[1] = height;
    rocketWorldPos[2] = radius * sinf(angleRad);
}

static void rocketDraw(void)
{
    glPushMatrix();
    glTranslatef(rocketWorldPos[0], rocketWorldPos[1], rocketWorldPos[2]);
    glScalef(ROCKET_SCALE, ROCKET_SCALE, ROCKET_SCALE);
    glRotatef(rocketSpinX, 1.0f, 0.0f, 0.0f);
    glRotatef(rocketSpinY, 0.0f, 1.0f, 0.0f);
    glRotatef(rocketSpinZ, 0.0f, 0.0f, 1.0f);
    rocketDrawAll();
    glPopMatrix();
}

// Draws the same rocket model at an arbitrary position/orientation/scale -
// used by ROCKET_SIM for the player's ship, which flies free instead of
// following the roaming ship's fixed loop. Rotation order (yaw, then pitch,
// then roll in code) is applied to the model in reverse (roll, then pitch,
// then yaw) by OpenGL's matrix stacking, which is the standard aircraft
// convention and matches the forward-vector math in rocketSimFrame().
static void rocketDrawAt(float x, float y, float z,
                          float yawDeg, float pitchDeg, float rollDeg,
                          float scale)
{
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(yawDeg, 0.0f, 1.0f, 0.0f);
    glRotatef(-pitchDeg, 1.0f, 0.0f, 0.0f); /* negated: positive pitch = nose up */
    glRotatef(rollDeg, 0.0f, 0.0f, 1.0f);
    glScalef(scale, scale, scale);
    rocketDrawAll();
    glPopMatrix();
}

/* ---- one-time solar-system setup, called from the combined init() ---- */
static void solarSystemInit(void)
{
    glEnable(GL_NORMALIZE); /* the rocket is uniformly scaled by ROCKET_SCALE */
    planetTextureQuadric = gluNewQuadric();
    if(planetTextureQuadric != NULL)
    {
        gluQuadricNormals(planetTextureQuadric, GLU_SMOOTH);
        gluQuadricTexture(planetTextureQuadric, GL_TRUE);
        for(int i = P_MERCURY; i < PLANET_COUNT; i++)
            if(!planets[i].isMoon)
                createPlanetTexture(i);
        createEarthCloudTexture();
    }

    GLfloat mat_specular[]   = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light_position[] = { 0.0f, 0.0f, 0.0f, 1.0f }; /* at the sun */
    GLfloat light_ambient[]  = { 0.05f, 0.05f, 0.06f, 1.0f };
    GLfloat light_diffuse[]  = { 1.0f, 0.97f, 0.9f, 1.0f };

    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 25.0f);

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glEnable(GL_LIGHT0); /* the light SOURCE can stay enabled permanently -
                            GL_LIGHTING (the overall system) is what actually
                            gets toggled on/off per mode in display() */

    glColorMaterial(GL_FRONT, GL_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    // The three extra beacon lights (ROCKET_SIM only). Colors are fixed
    // here since they don't change; each light's POSITION still has to be
    // re-issued every frame *after* the camera's gluLookAt (see
    // applyExtraLights()) because GL transforms a positional light's
    // coordinates by whatever MODELVIEW matrix is active at the moment
    // glLightfv(..., GL_POSITION, ...) is called - setting it here, before
    // any camera exists, would place it wrong.
    for(int i = 0; i < NUM_EXTRA_LIGHTS; i++)
    {
        GLenum lightId = GL_LIGHT1 + i;
        GLfloat diffuse[]  = { extraLights[i].color[0], extraLights[i].color[1], extraLights[i].color[2], 1.0f };
        GLfloat specular[] = { extraLights[i].color[0], extraLights[i].color[1], extraLights[i].color[2], 1.0f };
        GLfloat ambient[]  = { 0.0f, 0.0f, 0.0f, 1.0f }; /* colored point lights only - no ambient tint */

        glLightfv(lightId, GL_DIFFUSE, diffuse);
        glLightfv(lightId, GL_SPECULAR, specular);
        glLightfv(lightId, GL_AMBIENT, ambient);
        glLightf(lightId, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(lightId, GL_LINEAR_ATTENUATION, 0.01f);
        glLightf(lightId, GL_QUADRATIC_ATTENUATION, 0.0005f);
        /* left disabled here - applyExtraLights() enables/disables each
           one every frame based on extraLights[i].enabled */
    }

    generateStars();
    generateAsteroids();
    rocketUpdate(); /* valid rocket position before the first frame */

    cam.x = CAM_DEFAULT_X;
    cam.y = CAM_DEFAULT_Y;
    cam.z = CAM_DEFAULT_Z;
    cam.yaw = CAM_DEFAULT_YAW;
    cam.pitch = CAM_DEFAULT_PITCH;
    cam.fov = CAM_DEFAULT_FOV;
    cam.targetFov = CAM_DEFAULT_FOV;
    cam.velocityX = 0.0f;
    cam.velocityZ = 0.0f;
    cam.locked = -1;
    cam.dragging = 0;
}

static void solarSetCamera(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if(cam.locked >= 0)
    {
        Planet *p = &planets[cam.locked];

        float camDist = p->size * 8.0f + 2.2f;

        float eyeX = p->worldPos[0];
        float eyeY = p->worldPos[1] + camDist * 0.45f;
        float eyeZ = p->worldPos[2] + camDist;

        gluLookAt(eyeX, eyeY, eyeZ,
                  p->worldPos[0], p->worldPos[1], p->worldPos[2],
                  0.0, 1.0, 0.0);
        return;
    }

    float yawRad = cam.yaw * PI / 180.0f;
    float pitchRad = cam.pitch * PI / 180.0f;

    float dirX = cosf(yawRad) * cosf(pitchRad);
    float dirY = sinf(pitchRad);
    float dirZ = sinf(yawRad) * cosf(pitchRad);

    gluLookAt(cam.x, cam.y, cam.z,
              cam.x + dirX, cam.y + dirY, cam.z + dirZ,
              0.0, 1.0, 0.0);
}

static void solarCameraOperations(void)
{
    const float fovStep = 0.75f;
    const float acceleration = 0.035f;
    const float maxSpeed = 0.42f;
    const float damping = 0.78f;
    float yawRad = cam.yaw * PI / 180.0f;
    float forwardX = cosf(yawRad);
    float forwardZ = sinf(yawRad);
    float rightX = -sinf(yawRad);
    float rightZ = cosf(yawRad);

    if(keyStates['w'] || keyStates['W']) cam.targetFov -= fovStep;
    if(keyStates['s'] || keyStates['S']) cam.targetFov += fovStep;
    if(cam.targetFov < 20.0f) cam.targetFov = 20.0f;
    if(cam.targetFov > 90.0f) cam.targetFov = 90.0f;
    cam.fov += (cam.targetFov - cam.fov) * 0.18f;

    if(cam.locked < 0)
    {
        float desiredX = 0.0f;
        float desiredZ = 0.0f;

        if(keyStates['a'] || keyStates['A']) { desiredX -= rightX; desiredZ -= rightZ; }
        if(keyStates['d'] || keyStates['D']) { desiredX += rightX; desiredZ += rightZ; }
        if(keyStates['j'] || keyStates['J']) { desiredX += forwardX; desiredZ += forwardZ; }
        if(keyStates['l'] || keyStates['L']) { desiredX -= forwardX; desiredZ -= forwardZ; }

        cam.velocityX += desiredX * acceleration;
        cam.velocityZ += desiredZ * acceleration;
        if(cam.velocityX > maxSpeed) cam.velocityX = maxSpeed;
        if(cam.velocityX < -maxSpeed) cam.velocityX = -maxSpeed;
        if(cam.velocityZ > maxSpeed) cam.velocityZ = maxSpeed;
        if(cam.velocityZ < -maxSpeed) cam.velocityZ = -maxSpeed;

        cam.x += cam.velocityX;
        cam.z += cam.velocityZ;
        cam.velocityX *= damping;
        cam.velocityZ *= damping;
    }
    else
    {
        cam.velocityX = 0.0f;
        cam.velocityZ = 0.0f;
    }
}

// Small 2D overlay reminding the player how to get back to the menu and
// what the controls are. Drawn last, on top of the 3D scene, by briefly
// switching to an orthographic projection (matching the same world used
// by every shooter screen) and restoring the 3D one afterwards.
static void solarHudOverlay(void)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(-1200, 1200, -700, 700);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glColor3fv(colText);
    displayCenteredRasterText(0, -670, 0,
        "drag: look   W/S: smooth zoom   A/D: smooth strafe   J/L: smooth front/back   1-9: lock   R: reset");

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// Advances every planet's orbit/self-rotation and the asteroid belt by one
// tick. Shared by solarSystemFrame() (gated by solarPaused) and
// rocketSimFrame() (always running - see Part 5) so the two modes don't
// duplicate this loop.
static void advancePlanetAndBeltAngles(void)
{
    for(int i = 0; i < PLANET_COUNT; i++)
    {
        planets[i].currentAngle += planets[i].orbitSpeed;
        if(planets[i].currentAngle >= 360.0f) planets[i].currentAngle -= 360.0f;

        planets[i].selfAngle += planets[i].selfSpeed;
        if(planets[i].selfAngle >= 360.0f) planets[i].selfAngle -= 360.0f;
    }

    beltAngle += beltSpeed;
    if(beltAngle >= 360.0f) beltAngle -= 360.0f;
}

// Recomputes sunPulse and applies it to GL_LIGHT0's intensity. Shared by
// both 3D modes so the sun's dynamic lighting looks the same whichever
// mode you're viewing it from.
static void updateDynamicLighting(void)
{
    float elapsedSeconds = (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    sunPulse = 0.5f + 0.4f * sinf(0.5f * PI * elapsedSeconds);

    float lightBoost = 0.85f + 0.15f * sunPulse;

    GLfloat light_position[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat light_diffuse[]  = { 1.0f * lightBoost, 0.97f * lightBoost, 0.9f * lightBoost, 1.0f };
    GLfloat light_specular[] = { 1.0f * lightBoost, 1.0f * lightBoost, 1.0f * lightBoost, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
}

// ROCKET_SIM only. Re-issues each beacon light's world position every
// frame (must happen AFTER the camera's gluLookAt - see the comment in
// solarSystemInit()) and enables/disables it per extraLights[i].enabled.
static void applyExtraLights(void)
{
    for(int i = 0; i < NUM_EXTRA_LIGHTS; i++)
    {
        GLenum lightId = GL_LIGHT1 + i;
        GLfloat pos[] = { extraLights[i].pos[0], extraLights[i].pos[1], extraLights[i].pos[2], 1.0f };
        glLightfv(lightId, GL_POSITION, pos);

        if(extraLights[i].enabled)
            glEnable(lightId);
        else
            glDisable(lightId);
    }
}

// Always turns the three beacons off - called from solarSystemFrame() so
// a light left ON during a Rocket Simulation visit can never leak into
// the Solar System viewer afterwards.
static void disableExtraLights(void)
{
    for(int i = 0; i < NUM_EXTRA_LIGHTS; i++)
        glDisable(GL_LIGHT1 + i);
}

// Small glowing marker at each ENABLED beacon's position, drawn unlit and
// additively blended (same technique as the sun's glow) so turning a
// light on is visually obvious even before its illumination reaches
// anything - otherwise an invisible light source floating in empty space
// would be hard to even notice.
static void drawExtraLightMarkers(void)
{
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for(int i = 0; i < NUM_EXTRA_LIGHTS; i++)
    {
        if(!extraLights[i].enabled)
            continue;

        glPushMatrix();
        glTranslatef(extraLights[i].pos[0], extraLights[i].pos[1], extraLights[i].pos[2]);

        glColor4f(extraLights[i].color[0], extraLights[i].color[1], extraLights[i].color[2], 0.9f);
        glutSolidSphere(0.6, 16, 12);

        glColor4f(extraLights[i].color[0], extraLights[i].color[1], extraLights[i].color[2], 0.25f);
        glutSolidSphere(1.6, 16, 12);

        glPopMatrix();
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

// Called once per frame from display() while viewPage == SOLAR. Advances
// the simulation (unless paused) and draws the whole 3D scene. Projection,
// GL_DEPTH_TEST and GL_LIGHTING are already set up by display() before this
// runs; GL_MODELVIEW is set here via solarSetCamera().
static void solarSystemFrame(void)
{
    solarSetCamera();
    updateDynamicLighting();
    disableExtraLights(); /* beacons are ROCKET_SIM-only; never leak in here */

    drawStars();

    if(!solarPaused)
    {
        advancePlanetAndBeltAngles();
        rocketUpdate();
    }

    for(int i = 0; i < PLANET_COUNT; i++)
        updatePlanetPosition(&planets[i]);

    for(int i = 0; i < PLANET_COUNT; i++)
        drawPlanet(&planets[i]);

    drawAsteroidBelt();

    rocketBrightenAmount = 0.0f; /* roaming ship: normal colors */
    rocketThrottleGlow = 1.0f;   /* roaming ship: always fully lit */
    rocketDraw();

    solarHudOverlay();
}

/* =========================================================================
 * ROCKET SIMULATION  (mode 3: drive the rocket yourself)
 * ========================================================================= */

// Puts the player's ship back at a fixed, clear spawn point facing the
// solar system - used both when the mode is first entered (from the menu)
// and on the 'r' key.
static void rocketSimReset(void)
{
    simRocketX = 0.0f;
    simRocketY = 6.0f;
    simRocketZ = 40.0f;
    simRocketYaw = 180.0f;  /* nose toward the sun at the origin */
    simRocketPitch = 0.0f;
    simRocketRoll = 0.0f;
    simThrottle = 0.0f;

    for(int i = 0; i < NUM_EXTRA_LIGHTS; i++)
        extraLights[i].enabled = 0;

    currentMission = 0;
    missionScore = 0;
    allMissionsComplete = 0;
    missionMessageTimer = 0.0f;
}

static void rocketSimHudOverlay(void)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(-1200, 1200, -700, 700);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    char throttleLine[64];
    snprintf(throttleLine, sizeof(throttleLine), "THROTTLE  %3d%%", (int)(simThrottle * 100.0f));

    glColor3fv(colTitle);
    displayRasterText(-1150, 630, 0, throttleLine);

    // A small filled bar next to the readout, so throttle is visible at a
    // glance rather than only as a number.
    float barX = -900.0f, barY = 620.0f, barW = 260.0f, barH = 22.0f;
    glColor3f(0.15f, 0.15f, 0.18f);
    glBegin(GL_POLYGON);
        glVertex2f(barX, barY);
        glVertex2f(barX + barW, barY);
        glVertex2f(barX + barW, barY + barH);
        glVertex2f(barX, barY + barH);
    glEnd();

    glColor3f(0.95f - 0.5f*simThrottle, 0.55f + 0.35f*simThrottle, 0.25f);
    glBegin(GL_POLYGON);
        glVertex2f(barX, barY);
        glVertex2f(barX + barW*simThrottle, barY);
        glVertex2f(barX + barW*simThrottle, barY + barH);
        glVertex2f(barX, barY + barH);
    glEnd();

    glColor3fv(colText);
    displayCenteredRasterText(0, -670, 0,
        "UP/DOWN: pitch   LEFT/RIGHT: yaw   w: thrust   s: brake   1/2/3: lights   r: reset   ESC/Q: menu");

    // --- mission readout: current target + score, top right ---
    char scoreLine[64];
    snprintf(scoreLine, sizeof(scoreLine), "SCORE  %d", missionScore);
    glColor3fv(colTitle);
    displayRightAlignedRasterText(1150, 630, 0, scoreLine);

    if(allMissionsComplete)
    {
        glColor3fv(colAccent);
        displayRightAlignedRasterText(1150, 590, 0, "ALL MISSIONS COMPLETE!");
    }
    else
    {
        char missionLine[64];
        snprintf(missionLine, sizeof(missionLine), "MISSION %d/%d: fly to %s",
                 currentMission + 1, NUM_MISSIONS, missionNames[currentMission]);
        glColor3fv(colAccent);
        displayRightAlignedRasterText(1150, 590, 0, missionLine);
    }

    // --- light toggle status, so it's clear which beacons are on ---
    float lightStatusY = 550.0f;
    for(int i = 0; i < NUM_EXTRA_LIGHTS; i++)
    {
        char lightLine[48];
        snprintf(lightLine, sizeof(lightLine), "%d: %s [%s]",
                 i + 1, extraLights[i].name, extraLights[i].enabled ? "ON" : "off");

        if(extraLights[i].enabled)
            glColor3fv(extraLights[i].color);
        else
            glColor3fv(colInfo);

        displayRightAlignedRasterText(1150, lightStatusY, 0, lightLine);
        lightStatusY -= 34.0f;
    }

    // --- brief "mission complete" flash, centered, when a mission is
    //     freshly finished (missionMessageTimer counts down in
    //     rocketSimFrame()) ---
    if(missionMessageTimer > 0.0f)
    {
        glColor3fv(colTitle);
        displayCenteredStrokeText(0, 250, 0.5, 4.0, "MISSION COMPLETE!");
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// Called once per frame from display() while viewPage == ROCKET_SIM.
// Polls held keys for smooth, continuous control (same reasoning as the
// shooter's own keyOperations(): reacting only to key-down events makes
// input feel stuttery, since the OS repeats held keys slowly).
static void rocketSimFrame(void)
{
    // --- throttle: ramps smoothly toward 1 while 'w' is held, toward 0
    //     while 's' is held or brakes, and gently decays with no input.
    //     This ramp (not an instant on/off) IS the "animation when the
    //     rocket moves" for acceleration - speed visibly builds up and
    //     bleeds off instead of snapping. ---
    if(keyStates['w'] || keyStates['W'])
        simThrottle += SIM_THROTTLE_RAMP;
    else if(keyStates['s'] || keyStates['S'])
        simThrottle -= SIM_THROTTLE_RAMP * 1.5f;
    else
        simThrottle -= SIM_THROTTLE_DECAY;

    if(simThrottle > 1.0f) simThrottle = 1.0f;
    if(simThrottle < 0.0f) simThrottle = 0.0f;

    // --- yaw / pitch from the arrow keys ---
    if(specialKeyStates[GLUT_KEY_LEFT])  simRocketYaw -= SIM_YAW_RATE;
    if(specialKeyStates[GLUT_KEY_RIGHT]) simRocketYaw += SIM_YAW_RATE;
    if(simRocketYaw >= 360.0f) simRocketYaw -= 360.0f;
    if(simRocketYaw < 0.0f) simRocketYaw += 360.0f;

    if(specialKeyStates[GLUT_KEY_UP])   simRocketPitch += SIM_PITCH_RATE;
    if(specialKeyStates[GLUT_KEY_DOWN]) simRocketPitch -= SIM_PITCH_RATE;
    if(simRocketPitch > SIM_MAX_PITCH) simRocketPitch = SIM_MAX_PITCH;
    if(simRocketPitch < -SIM_MAX_PITCH) simRocketPitch = -SIM_MAX_PITCH;

    // --- banking animation: rolls toward a bank angle while turning, and
    //     eases back to level otherwise. Purely cosmetic (the chase camera
    //     stays level - see below) but this is the other half of "give
    //     animation when the rocket moves". ---
    float rollTarget = 0.0f;
    if(specialKeyStates[GLUT_KEY_LEFT])  rollTarget = SIM_MAX_BANK;
    if(specialKeyStates[GLUT_KEY_RIGHT]) rollTarget = -SIM_MAX_BANK;
    simRocketRoll += (rollTarget - simRocketRoll) * SIM_BANK_EASE;

    // --- forward vector from yaw/pitch. Derived to match rocketDrawAt()'s
    //     rotation order (yaw, then -pitch, then roll applied to the
    //     model): at yaw=0,pitch=0 this is (0,0,1), matching the model's
    //     native nose direction (+Z). ---
    float yawRad = simRocketYaw * PI / 180.0f;
    float pitchRad = simRocketPitch * PI / 180.0f;

    float forwardX = cosf(pitchRad) * sinf(yawRad);
    float forwardY = sinf(pitchRad);
    float forwardZ = cosf(pitchRad) * cosf(yawRad);

    float speed = simThrottle * SIM_MAX_SPEED_PER_TICK;
    simRocketX += forwardX * speed;
    simRocketY += forwardY * speed;
    simRocketZ += forwardZ * speed;

    // --- chase camera: behind and above the ship, looking a little ahead
    //     of it. Stays world-up regardless of the ship's own roll, which
    //     is more comfortable to fly with than a camera that banks too. ---
    const float chaseDistance = 6.0f;
    const float chaseHeight = 2.2f;
    const float lookAhead = 5.0f;

    float eyeX = simRocketX - forwardX * chaseDistance;
    float eyeY = simRocketY - forwardY * chaseDistance + chaseHeight;
    float eyeZ = simRocketZ - forwardZ * chaseDistance;

    float centerX = simRocketX + forwardX * lookAhead;
    float centerY = simRocketY + forwardY * lookAhead;
    float centerZ = simRocketZ + forwardZ * lookAhead;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, 0.0, 1.0, 0.0);

    updateDynamicLighting();
    applyExtraLights();

    drawStars();

    // The backdrop solar system always keeps moving in this mode (no pause
    // control here - piloting through a frozen system would feel odd).
    advancePlanetAndBeltAngles();

    for(int i = 0; i < PLANET_COUNT; i++)
        updatePlanetPosition(&planets[i]);

    // --- mission check: has the ship reached this mission's target
    //     planet? Capture radius scales with the planet's own size so
    //     small planets (Mercury) and big ones (Jupiter) both feel fair. ---
    if(!allMissionsComplete)
    {
        Planet *target = &planets[missionTargets[currentMission]];

        float dx = simRocketX - target->worldPos[0];
        float dy = simRocketY - target->worldPos[1];
        float dz = simRocketZ - target->worldPos[2];
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        float captureRadius = target->size * 5.0f + 1.5f;

        if(dist < captureRadius)
        {
            missionScore += 100;
            missionMessageTimer = 120.0f; /* ~2 seconds at 60fps */
            currentMission++;

            if(currentMission >= NUM_MISSIONS)
                allMissionsComplete = 1;
        }
    }

    if(missionMessageTimer > 0.0f)
        missionMessageTimer -= 1.0f;

    for(int i = 0; i < PLANET_COUNT; i++)
        drawPlanet(&planets[i]);

    drawAsteroidBelt();
    drawExtraLightMarkers();

    // The player's ship: bigger and brightened for clarity (Parts 1-2),
    // flame intensity tied to throttle (Part 2), drawn at its own free
    // position/orientation rather than the roaming ship's fixed loop.
    rocketBrightenAmount = 0.35f;
    rocketThrottleGlow = 0.15f + 0.85f * simThrottle;
    rocketDrawAt(simRocketX, simRocketY, simRocketZ,
                 simRocketYaw, simRocketPitch, simRocketRoll,
                 SIM_ROCKET_SCALE);

    rocketSimHudOverlay();
}

// 'r' resets the ship; 'q'/'Q'/Esc returns to the menu. Thrust, brake, and
// turning are all polled directly out of keyStates[]/specialKeyStates[] in
// 'r' resets the ship (and missions/lights with it); 'q'/'Q'/Esc returns to
// the menu; '1'/'2'/'3' individually toggle each beacon light. Thrust,
// brake, and turning are all polled directly out of keyStates[]/
// specialKeyStates[] in rocketSimFrame() above rather than handled here,
// since they need to be smooth/continuous rather than one-shot per key press.
static void rocketSimHandleKey(unsigned char key)
{
    switch(key)
    {
        case 'r':
        case 'R':
            rocketSimReset();
            break;

        case 'q':
        case 'Q':
        case 27:
            viewPage = MENU;
            break;

        case '1':
            extraLights[0].enabled = !extraLights[0].enabled;
            break;

        case '2':
            extraLights[1].enabled = !extraLights[1].enabled;
            break;

        case '3':
            extraLights[2].enabled = !extraLights[2].enabled;
            break;

        default:
            break;
    }
}

// The old solar keyboard() switch, folded in as a helper called only while
// viewPage == SOLAR (see the combined keyPressed() near the bottom of the
// file). One behavior change: 'q'/'Q'/Esc now returns to the menu instead
// of exit(0), since this is a mode inside a bigger program now.
static void solarHandleKey(unsigned char key)
{
    switch(key)
    {
        case 'r':
        case 'R':
            cam.x = CAM_DEFAULT_X;
            cam.y = CAM_DEFAULT_Y;
            cam.z = CAM_DEFAULT_Z;
            cam.yaw = CAM_DEFAULT_YAW;
            cam.pitch = CAM_DEFAULT_PITCH;
            cam.fov = CAM_DEFAULT_FOV;
            cam.targetFov = CAM_DEFAULT_FOV;
            cam.velocityX = 0.0f;
            cam.velocityZ = 0.0f;
            cam.locked = -1;
            break;

        case '0':
            cam.locked = -1;
            break;

        case 't':
        case 'T':
            rocketSelfRotate = !rocketSelfRotate;
            break;

        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            cam.locked = keyToPlanet[key - '1'];
            break;

        case ' ':
            solarPaused = !solarPaused;
            break;

        case 'q':
        case 'Q':
        case 27:
            viewPage = MENU; /* was exit(0) in the standalone version */
            break;

        default:
            break;
    }
}

/* =======================================================================
 * SET-UP
 * ======================================================================= */
// Runs once, before the first frame. Sets the background colour, the
// (2D) world coordinate system used for every shooter screen, and the
// Solar System's one-time lighting/material/data setup. The projection
// matrix is NOT fixed here for good, though - display() recomputes it
// every frame based on the current mode, so switching between the 2D
// shooter screens and the 3D Solar System can never leave a stale
// projection matrix behind.
void init()
{
	glClearColor(0.02,0.03,0.06,0);
	glColor3f(1.0,0.0,0.0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    gluOrtho2D(-1200,1200,-700,700);
	glMatrixMode(GL_MODELVIEW);

	solarSystemInit();
}

/* =======================================================================
 * SCREEN 1 - THE TITLE PAGE
 * ======================================================================= */
// The title page. Nothing here moves; it simply waits for ENTER, which is
// picked up by keyOperations().
void introScreen()
{
	glClear(GL_COLOR_BUFFER_BIT);

		glColor3fv(colAccent);
	displayCenteredRasterText(0, 490, 0.0,"SOUTHEAST UNIVERSITY");
		glColor3fv(colText);
	displayCenteredRasterText(0, 415, 0.0,"DEPARTMENT OF COMPUTER SCIENCE AND ENGINEERING");
		glColor3fv(colInfo);
	displayCenteredRasterText(0, 320, 0.0,"A PROJECT ON");
		glColor3fv(colTitle);
	displayCenteredStrokeText(0, 170, 1.1, 5.0,"SPACE");
   		glColor3fv(colInfo);
	displayCenteredRasterText(0, 90, 0.0,"created by");
		glColor3fv(colText);
	displayCenteredRasterText(0, 20, 0.0,"CUBIC");
	 	glColor3fv(colAccent);
	displayRasterText(-800, -140, 0.0,"STUDENT NAMES");
	  	glColor3fv(colText);
	displayRasterText(-800, -230, 0.0,"Sadman Islam Amlan");
	displayRasterText(-800, -300, 0.0,"Sadia Amin Ruba");
	displayRasterText(-800, -370, 0.0,"Anika Anjum Bristy");
		glColor3fv(colAccent);
	displayRasterText(400, -140, 0.0,"Under the Guidance of");
		glColor3fv(colText);
	displayRasterText(400, -230, 0.0,"Ms. Tanjina Oriana");
		glColor3fv(colAccent);
	displayCenteredRasterText(0, -430, 0.0,"Academic Year 2026");
        glColor3fv(colText);
	displayCenteredRasterText(0, -560, 0.0,"Press ENTER to start the game");
}

/* =======================================================================
 * SCREEN 2 - THE MENU
 * ======================================================================= */
// The mouse position is stored in half the units used for drawing (see
// passiveMotionFunc), so a button drawn between y1 and y2 is hovered when the
// cursor lies between half those values.
bool isHovered(float x1 ,float y1 ,float x2 ,float y2) {
	return mouseX >= x1/2 && mouseX <= x2/2 && mouseY >= y1/2 && mouseY <= y2/2;
}

// A menu button is just a filled rectangle, drawn brighter when the cursor
// is over it so the player can see it is clickable.
void drawButton(float x1 ,float y1 ,float x2 ,float y2 ,bool hovered) {
	if(hovered)
		glColor3fv(colButtonHot);		//lit up while the cursor is over it
	else
		glColor3fv(colButton);
	glBegin(GL_POLYGON);
		glVertex2f(x1 ,y1);
		glVertex2f(x1 ,y2);
		glVertex2f(x2 ,y2);
		glVertex2f(x2 ,y1);
	glEnd();
}

// The menu. Note the order: we work out what is hovered first, then draw the
// buttons, then act on any click. Deciding before drawing is what lets a
// button change colour on the same frame the cursor arrives.
//
// Five buttons now - Rocket Simulation sits between Solar System and
// Instructions, and all five are spaced 40 units apart, evenly.
void startScreenDisplay()
{
	bool startHovered = isHovered(-200 ,330 ,200 ,430);
	bool solarHovered = isHovered(-200 ,190 ,200 ,290);
	bool rocketSimHovered = isHovered(-200 ,50 ,200 ,150);
	bool instructionsHovered = isHovered(-200 ,-90 ,200 ,10);
	bool quitHovered = isHovered(-200 ,-230 ,200 ,-130);

	glLineWidth(10);

	glColor3fv(colBorder);
	glBegin(GL_LINE_LOOP);               //Border
		glVertex2f(-750 ,-500);
		glVertex2f(-750 ,550);
		glVertex2f(750 ,550);
		glVertex2f(750 ,-500);
	glEnd();

	glLineWidth(1);

	glColor3fv(colTitle);
	displayCenteredStrokeText(0 ,480 ,0.8 ,5.0 ,"SPACE SHOOTER");

	drawButton(-200 ,330 ,200 ,430 ,startHovered);			//START GAME
	drawButton(-200 ,190 ,200 ,290 ,solarHovered);			//SOLAR SYSTEM
	drawButton(-200 ,50 ,200 ,150 ,rocketSimHovered);		//ROCKET SIMULATION
	drawButton(-200 ,-90 ,200 ,10 ,instructionsHovered);	//INSTRUCTIONS
	drawButton(-200 ,-230 ,200 ,-130 ,quitHovered);		//QUIT

	// The labels sit in the middle of each button. displayCenteredRasterText()
	// measures the text for us, so they stay centred whatever font is chosen.
	glColor3fv(colButtonText);
	if(startHovered && mButtonPressed){
		alienLife1 = alienLife2 = 100;
		viewPage = GAME;
		mButtonPressed = false;
	}
	displayCenteredRasterText(0 ,365 ,0.4 ,"Start Game");

	if(solarHovered && mButtonPressed){
		viewPage = SOLAR;
		mButtonPressed = false;
		// Reset the camera each time the viewer is entered, so leaving it
		// locked onto a planet (or dragged somewhere odd) on a previous
		// visit doesn't carry over.
		cam.x = CAM_DEFAULT_X;
		cam.y = CAM_DEFAULT_Y;
		cam.z = CAM_DEFAULT_Z;
		cam.yaw = CAM_DEFAULT_YAW;
		cam.pitch = CAM_DEFAULT_PITCH;
		cam.fov = CAM_DEFAULT_FOV;
        cam.targetFov = CAM_DEFAULT_FOV;
        cam.velocityX = 0.0f;
        cam.velocityZ = 0.0f;
		cam.locked = -1;
	}
	displayCenteredRasterText(0 ,225 ,0.4 ,"Solar System");

	if(rocketSimHovered && mButtonPressed){
		viewPage = ROCKET_SIM;
		mButtonPressed = false;
		rocketSimReset(); // fresh position/throttle each time it's entered
	}
	displayCenteredRasterText(0 ,85 ,0.4 ,"Rocket Simulation");

	if(instructionsHovered && mButtonPressed){
		viewPage = INSTRUCTIONS;
		mButtonPressed = false;
	}
	displayCenteredRasterText(0 ,-55 ,0.4 ,"Instructions");

	if(quitHovered && mButtonPressed){
		mButtonPressed = false;
		exit(0);
	}
	displayCenteredRasterText(0 ,-195 ,0.4 ,"Quit");
	glutPostRedisplay();
}

/* =======================================================================
 * SCREEN 3 - THE INSTRUCTIONS
 * ======================================================================= */
void backButton() {
	if(mouseX <= -450 && mouseX >= -500 && mouseY >= -275 && mouseY <= -250){
			glColor3fv(colAccent);
			if(mButtonPressed) {
				viewPage = MENU;
				mButtonPressed = false;
				glutPostRedisplay();
			}
	}
	else glColor3fv(colText);
	displayRasterText(-1000 ,-550 ,0, "Back");
}

void instructionsScreenDisplay()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor3fv(colAccent);
	displayRasterText(-900 ,550 ,0.4 ,"INSTRUCTIONS");
	glColor3fv(colTeam1);
	displayRasterText(-1000 ,400 ,0.4 ,"PLAYER 1");
	glColor3fv(colTeam2);
	displayRasterText(200 ,400 ,0.4 ,"PLAYER 2");
	glColor3fv(colText);
	displayRasterText(-1100 ,300 ,0.4 ,"Press 'w' to move up.");
	displayRasterText(-1100 ,200 ,0.4 ,"Press 's' to move down.");
	displayRasterText(-1100 ,100 ,0.4 ,"Press 'd' to move right.");
	displayRasterText(-1100 ,0 ,0.4 ,"Press 'a' to move left.");
	displayRasterText(100 ,300 ,0.4 ,"Press 'i' to move up.");
    displayRasterText(100 ,200 ,0.4 ,"Press 'k' to move down.");
    displayRasterText(100 ,100 ,0.4 ,"Press 'l' to move right.");
    displayRasterText(100 ,0 ,0.4 ,"Press 'j' to move left.");
	displayRasterText(-1100 ,-100 ,0.4 ,"Press 'c' to shoot. Hold 'w' or 's' while shooting to aim.");
	displayRasterText(100 ,-100 ,0.4 ,"Press 'm' to shoot. Hold 'i' or 'k' while shooting to aim.");
	displayRasterText(-1100, -300,0.4,"The objective is to kill your opponent.");
	displayRasterText(-1100 ,-370 ,0.4 ,"Each time a player gets shot, LIFE decreases by 5 points.");
	glColor3fv(colInfo);
	displayRasterText(-1100 ,-440 ,0.4 ,"There is also a Solar System viewer and a Rocket Simulation, from the main menu.");
	backButton();
}

/* =======================================================================
 * DRAWING THE ALIEN, PIECE BY PIECE
 * ======================================================================= */
// The alien is built from four flat shapes stacked on top of each other:
// body, then collar, then face, then beak, and finally the eyes. Each one is
// a list of corner points stored in the arrays near the top of the file, and
// each is drawn twice - once filled with colour, once as an outline.
void DrawAlienBody(bool isPlayerTwo)
{
	if(isPlayerTwo)
		glColor3fv(colTeam2);
	else
		glColor3fv(colTeam1);	//BODY color
	glBegin(GL_POLYGON);
	for(int i=0;i<=8;i++)
		glVertex2fv(AlienBody[i]);
	glEnd();

	glColor3fv(colOutline);		//BODY Outline
	glLineWidth(1);
	glBegin(GL_LINE_STRIP);
	for(int i=0;i<=8;i++)
		glVertex2fv(AlienBody[i]);
	glEnd();

	glBegin(GL_LINES);                //BODY effect
		glVertex2f(-13,11);
		glVertex2f(-15,9);
	glEnd();
}
void DrawAlienCollar(bool isPlayerTwo)
{
	if(isPlayerTwo)
		glColor3fv(colTeam2);
	else
		glColor3fv(colTeam1);				//COLLAR
	glBegin(GL_POLYGON);
	for(int i=0;i<=20 ;i++)
		glVertex2fv(AlienCollar[i]);
	glEnd();

	glColor3fv(colOutline);				//COLLAR outline
	glBegin(GL_LINE_STRIP);
	for(int i=0;i<=20 ;i++)
		glVertex2fv(AlienCollar[i]);
	glEnd();
}
void DrawAlienFace()
{
	glColor3fv(colFace);				//FACE

	glBegin(GL_POLYGON);
	for(int i=0;i<=42 ;i++)
		glVertex2fv(ALienFace[i]);
	glEnd();

	glColor3fv(colOutline);				//FACE outline
	glBegin(GL_LINE_STRIP);
	for(int i=0;i<=42 ;i++)
		glVertex2fv(ALienFace[i]);
	glEnd();

	glBegin(GL_LINE_STRIP);      //EAR effect
		glVertex2f(3.3,22);
		glVertex2f(4.4,23.5);
		glVertex2f(6.3,26);
	glEnd();
}
void DrawAlienBeak()
{
	glColor3fv(colBeak);				//BEAK color
	glBegin(GL_POLYGON);
	for(int i=0;i<=14 ;i++)
		glVertex2fv(ALienBeak[i]);
	glEnd();

	glColor3fv(colOutline);				//BEAK outline
	glBegin(GL_LINE_STRIP);
	for(int i=0;i<=14 ;i++)
		glVertex2fv(ALienBeak[i]);
	glEnd();
}
void DrawAlienEyes()
{
	glColor3fv(colEye);				//EYES

	glPushMatrix();
	glRotated(-10,0,0,1);
	glTranslated(-6,32.5,0);      //Left eye
	glScalef(2.5,4,0);
	glutSolidSphere(1,20,30);
	glPopMatrix();

	glPushMatrix();
	glRotated(-1,0,0,1);
	glTranslated(-8,36,0);							//Right eye
	glScalef(2.5,4,0);
	glutSolidSphere(1,20,20);
	glPopMatrix();
}
void DrawAlien(bool isPlayerTwo)
{
	DrawAlienBody(isPlayerTwo);
	DrawAlienCollar(isPlayerTwo);
	DrawAlienFace();
	DrawAlienBeak();
	DrawAlienEyes();
}
/* =======================================================================
 * DRAWING THE SPACESHIP
 * ======================================================================= */
// The saucer. glScalef() squashes a circle into the flat oval hull, which is
// far less work than listing the points of an ellipse by hand.
void DrawSpaceshipBody(bool isPlayerTwo)
{
	if(isPlayerTwo)
		glColor3fv(colHull2);		//BASE
	else
		glColor3fv(colHull1);

	glPushMatrix();
	glScalef(70,20,1);
	glutSolidSphere(1,50,50);
	glPopMatrix();

	glPushMatrix();							//LIGHTS
	glScalef(3,3,1);
	glTranslated(-20,0,0);					//move to the first light
	// Nine lights in a row. Each glTranslated() shifts the drawing position
	// along, so the next sphere lands 5 units further to the right.
	for(int i=0; i<9; i++) {
		glColor3fv(LightColor[(CI+i)%3]);
		glutSolidSphere(1,20,20);
		glTranslated(5,0,0);
	}
	glPopMatrix();
}
void DrawSteeringWheel()
{
	glPushMatrix();
	glLineWidth(3);
	glColor3fv(colWheel);
	glScalef(7,4,1);
	glTranslated(-1.9,5.5,0);
	glutWireSphere(1,8,8);
	glPopMatrix();

}
// The glass dome over the pilot. ("Doom" here means dome.)
void DrawSpaceshipDoom()
{
	glColor3fv(colDome);
	glPushMatrix();
	glTranslated(0,30,0);
	glScalef(35,50,1);
	glutSolidSphere(1,50,50);
	glPopMatrix();
}

// The laser is one thick line running from the ship to the far edge of the
// world. dir[] decides whether it goes up, down, or straight across.
void DrawLaser(int x, int y, bool dir[], GLfloat *colour) {
	int xend = -XMAX, yend = y;
	if(dir[0])
		yend = YMAX;
	else if(dir[1])
		yend = -YMAX;
	glLineWidth(5);
	glColor3fv(colour);
	glBegin(GL_LINES);
		glVertex2f(x, y);
		glVertex2f(xend, yend);
	glEnd();
}

// Draws one complete saucer with its pilot at position (x,y).
// Everything inside is drawn around the origin, and the glTranslated() moves
// the coordinate system so it lands in the right place. The push/pop pair
// makes sure the move is undone afterwards and the next saucer is unaffected.
void SpaceshipCreate(int x, int y, bool isPlayerTwo){
	glPushMatrix();
	glTranslated(x,y,0);
	DrawSpaceshipDoom();
	glPushMatrix();
	glTranslated(4,19,0);
	DrawAlien(isPlayerTwo);
	glPopMatrix();
	DrawSteeringWheel();
	DrawSpaceshipBody(isPlayerTwo);
	glEnd();
	glPopMatrix();
}

/* =======================================================================
 * SCREEN 4 - THE GAME ITSELF
 * ======================================================================= */
void DisplayHealthBar1() {
	char temp1[40];
	glColor3fv(colTeam1);
	snprintf(temp1 ,sizeof(temp1) ,"PLAYER 1   LIFE = %d",alienLife1);
	displayRasterText(-1100 ,600 ,0.4 ,temp1);
}

void DisplayHealthBar2() {
	char temp2[40];
	glColor3fv(colTeam2);
	snprintf(temp2 ,sizeof(temp2) ,"PLAYER 2   LIFE = %d",alienLife2);
	displayRightAlignedRasterText(1100 ,600 ,0.4 ,temp2);	//mirrors PLAYER 1 on the left
}

// Has a laser hit the other saucer?
//
// The laser is a straight line and the saucer is treated as a circle of
// radius r around (xp,yp). Substituting the line y = m*x + k into the circle
// equation gives a quadratic, and the discriminant b*b - 4*a*c of that
// quadratic tells us how many points the two share:
//      below zero -> no solutions  -> the laser misses
//      zero       -> one solution  -> it grazes the edge
//      above zero -> two solutions -> it passes through, so it is a hit
void checkLaserContact(int x, int y, bool dir[], int xp, int yp, bool player1) {
	int xend = -XMAX, yend = y;
	xp += 8; yp += 8; // moving circle slightly up to fix laser issue
	if(dir[0])
		yend = YMAX;
	else if(dir[1])
		yend = -YMAX;

	float m = (float)(yend - y) / (float)(xend - x);
	float k = y - m * x ;
	int r = 50; // approx radius of the spaceship

	float b = 2 * xp - 2 * m * (k - yp);
	float a = 1 + m * m;
	float c = xp * xp + (k - yp) * (k - yp) - r * r;

	float d = (b * b - 4 * a * c); // discriminant for the equation

	if(d >= 0) {
		if(player1) {
			if(frameCount - lastHitFrame1 >= HIT_INTERVAL) {
				alienLife1 -= 5;
				lastHitFrame1 = frameCount;
			}
		}
		else {
			if(frameCount - lastHitFrame2 >= HIT_INTERVAL) {
				alienLife2 -= 5;
				lastHitFrame2 = frameCount;
			}
		}
	}
}

// The game itself: two saucers, their lasers, and the two LIFE counters.
void gameScreenDisplay()
{
	DisplayHealthBar1();
	DisplayHealthBar2();
	glScalef(2, 2 ,0);

	if(alienLife1 > 0){
		SpaceshipCreate(xOne, yOne, true);
		if(laser1) {
			DrawLaser(xOne, yOne, laser1Dir, colTeam2);
			checkLaserContact(xOne, yOne, laser1Dir, -xTwo, yTwo, true);
		}
	}
	else {
		viewPage = GAMEOVER;
	}

	if(alienLife2 > 0) {
		glPushMatrix();
		glScalef(-1, 1, 1);
		SpaceshipCreate(xTwo, yTwo, false);
		if(laser2) {
			DrawLaser(xTwo, yTwo, laser2Dir, colTeam1);
			checkLaserContact(xTwo, yTwo, laser2Dir, -xOne, yOne, false);
		}
		glPopMatrix();
	}
	else {
		viewPage = GAMEOVER;
	}

	if(viewPage == GAMEOVER) {
		xOne = xTwo = 500;
		yOne = yTwo = 0;
	}
}

void displayGameOverMessage() {
	const char* message;
	if(alienLife1 > 0) {
		glColor3fv(colTeam1);
		message = "GAME OVER  -  PLAYER 1 WINS";
	}
	else {
		glColor3fv(colTeam2);
		message = "GAME OVER  -  PLAYER 2 WINS";
	}

	displayCenteredStrokeText(0 ,595 ,0.6 ,5.0 , message);
}

/* =======================================================================
 * INPUT - KEYBOARD AND MOUSE
 * ======================================================================= */
// Called once per frame, before anything is drawn. Rather than acting on key
// presses as they arrive, we look at which keys are currently held down. That
// gives smooth movement and lets both players move at the same time.
void keyOperations() {
	if(keyStates[13] == true && viewPage == INTRO) {
		viewPage = MENU;
	}
    if(viewPage == SOLAR) {
        solarCameraOperations();
    }
	if(viewPage == GAME) {
		laser1Dir[0] = laser1Dir[1] = false;
		laser2Dir[0] = laser2Dir[1] = false;
		if(keyStates['c'] == true) {
			laser2 = true;
			if(keyStates['w'] == true) 	laser2Dir[0] = true;
			if(keyStates['s'] == true) 	laser2Dir[1] = true;
		}
		else {
			laser2 = false;
			if(keyStates['d'] == true) xTwo-=SPACESHIP_SPEED;
			if(keyStates['a'] == true) xTwo+=SPACESHIP_SPEED;
			if(keyStates['w'] == true) yTwo+=SPACESHIP_SPEED;
			if(keyStates['s'] == true) yTwo-=SPACESHIP_SPEED;
		}

		if(keyStates['m'] == true) {
			laser1 = true;
			if(keyStates['i'] == true) laser1Dir[0] = true;
			if(keyStates['k'] == true) laser1Dir[1] = true;
		}
		else {
			laser1 = false;
			if(keyStates['l'] == true) xOne+=SPACESHIP_SPEED;
			if(keyStates['j'] == true) xOne-=SPACESHIP_SPEED;
			if(keyStates['i'] == true) yOne+=SPACESHIP_SPEED;
			if(keyStates['k'] == true) yOne-=SPACESHIP_SPEED;
		}
	}
}

/* =======================================================================
 * THE MAIN DRAW FUNCTION - CALLED ONCE PER FRAME
 * ======================================================================= */
// Called for every frame. Clears the window, sets up the projection and
// GL state the current mode needs, draws whichever screen is current, then
// shows the result.
void display()
{
	frameCount++;
	keyOperations();

	// The projection matrix is rebuilt every frame from the current mode,
	// rather than once at startup - this is what makes it safe to freely
	// switch between the 2D shooter screens and the 3D Solar System.
	float aspect = (float)m_viewport[2] / (float)(m_viewport[3] ? m_viewport[3] : 1);

	bool in3DMode = (viewPage == SOLAR || viewPage == ROCKET_SIM);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if(viewPage == SOLAR)
		gluPerspective(cam.fov, aspect, 0.1, 200.0);
	else if(viewPage == ROCKET_SIM)
		gluPerspective(65.0, aspect, 0.1, 200.0); // fixed FOV, independent of
		                                            // whatever cam.fov was left
		                                            // at from a SOLAR-mode visit
	else
		gluOrtho2D(-1200, 1200, -700, 700);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if(in3DMode) {
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_LIGHTING);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	switch (viewPage)
	{
		case INTRO:
			introScreen();
			break;
		case MENU:
			startScreenDisplay();
			break;
		case INSTRUCTIONS:
			instructionsScreenDisplay();
			break;
		case GAME:
			gameScreenDisplay();
			break;
		case GAMEOVER:
			displayGameOverMessage();
			startScreenDisplay();
			break;
		case SOLAR:
			solarSystemFrame();
			break;
		case ROCKET_SIM:
			rocketSimFrame();
			break;
	}

	glFlush();
	glLoadIdentity();
	glutSwapBuffers();
}

// Called by GLUT whenever the window is resized. Neither the shooter's 2D
// world nor the Solar System's aspect ratio depend on anything cached here
// beyond m_viewport itself (display() reads it fresh every frame), so this
// stays exactly as simple as the original.
void reshape(GLint w, GLint h)
{
	glViewport(0, 0, w, h);
	m_viewport[2] = w;
	m_viewport[3] = h;
	glutPostRedisplay();
}

// Called by GLUT whenever the mouse moves with NO button held. Used for
// menu-button hover in every shooter screen; harmless (just unused) while
// viewPage == SOLAR, since the camera look-drag lives in motionFunc()
// below instead (a button IS held for that).
void passiveMotionFunc(int x,int y) {
	mouseX = float(x)/(m_viewport[2]/1200.0)-600.0;
	mouseY = -(float(y)/(m_viewport[3]/700.0)-350.0);
	glutPostRedisplay();
}

// Called by GLUT whenever the mouse moves WHILE a button is held. This is
// the Solar System's look-around drag; it's a separate GLUT callback from
// passiveMotionFunc() above (GLUT allows both to be registered at once),
// so no dispatch-by-mode is needed here beyond the viewPage check itself.
void motionFunc(int x, int y) {
	if(viewPage == SOLAR && cam.dragging && cam.locked < 0) {
		int dx = x - cam.lastX;
		int dy = y - cam.lastY;

		cam.lastX = x;
		cam.lastY = y;

		float sensitivity = 0.25f;

		cam.yaw += dx * sensitivity;
		cam.pitch -= dy * sensitivity;

		if(cam.pitch > 85.0f) cam.pitch = 85.0f;
		if(cam.pitch < -85.0f) cam.pitch = -85.0f;
	}
	glutPostRedisplay();
}

// One click handler doing two unrelated jobs, guarded by mode: shooter's
// menu-button clicks, and Solar System's drag-to-look start/stop.
void mouseClick(int buttonPressed ,int state ,int x, int y) {

	if(buttonPressed == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		mButtonPressed = true;
	else
		mButtonPressed = false;

	if(viewPage == SOLAR && buttonPressed == GLUT_LEFT_BUTTON) {
		if(state == GLUT_DOWN) {
			cam.dragging = 1;
			cam.lastX = x;
			cam.lastY = y;
		}
		else {
			cam.dragging = 0;
		}
	}

	glutPostRedisplay();
}

// GLUT hands us the character of the key that changed. keyStates[] is
// always updated (that's what keyOperations() polls for shooter movement,
// and what rocketSimFrame() polls for thrust/brake); additionally, while
// viewPage == SOLAR or ROCKET_SIM, the key is also forwarded to that mode's
// own single-press handler (zoom/camera-lock for SOLAR, reset/back-to-menu
// for ROCKET_SIM).
void keyPressed(unsigned char key, int x, int y)
{
	keyStates[key] = true;
	if(viewPage == SOLAR) {
		solarHandleKey(key);
	}
	else if(viewPage == ROCKET_SIM) {
		rocketSimHandleKey(key);
	}
	glutPostRedisplay();
}

// Arrow keys (GLUT "special" keys) aren't covered by keyPressed()/
// keyReleased() above - GLUT reports them through a separate pair of
// callbacks. Tracked the same way as keyStates[]: set true/false here,
// polled every frame in rocketSimFrame() for smooth turning.
void specialKeyPressed(int key, int x, int y)
{
	if(key >= 0 && key < 256) specialKeyStates[key] = true;
	glutPostRedisplay();
}

void specialKeyReleased(int key, int x, int y)
{
	if(key >= 0 && key < 256) specialKeyStates[key] = false;
}

// Asks for a redraw, then books the next one FRAME_DELAY milliseconds later.
// This single timer now drives BOTH modes at the same fixed, machine-
// independent rate - no separate glutIdleFunc is registered at all, which
// also fixes a latent issue in the standalone Solar System file, where its
// animation ran at whatever rate an uncapped idle loop happened to hit.
void timer(int value) {
	glutPostRedisplay();
	glutTimerFunc(FRAME_DELAY, timer, 0);
}

void keyReleased(unsigned char key, int x, int y) {
	keyStates[key] = false;
}

/* =======================================================================
 * PROGRAM ENTRY POINT
 * ======================================================================= */
// Creates the window, tells GLUT which function handles which event, and then
// hands control over to GLUT for good.
int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	// GLUT_DEPTH is new here - the original shooter never requested a depth
	// buffer since it never needed one, but the Solar System's overlapping
	// 3D spheres do.
	glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(1200, 600);
    glutCreateWindow("Space Shooter + Solar System");
    init();
    glutReshapeFunc(reshape);
	glutTimerFunc(FRAME_DELAY, timer, 0);
    glutKeyboardFunc(keyPressed);
	glutKeyboardUpFunc(keyReleased);
	glutSpecialFunc(specialKeyPressed);
	glutSpecialUpFunc(specialKeyReleased);
	glutMouseFunc(mouseClick);
	glutPassiveMotionFunc(passiveMotionFunc);
	glutMotionFunc(motionFunc);
	glGetIntegerv(GL_VIEWPORT ,m_viewport);
    glutDisplayFunc(display);
    glutMainLoop();
}
