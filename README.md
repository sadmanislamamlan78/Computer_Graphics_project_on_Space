# Computer Graphics Project: Space Shooter

## Space Shooter + Solar System

An OpenGL and GLUT computer graphics project that combines a two-player 2D space shooter, an interactive 3D solar-system viewer, and a player-controlled rocket mission. The complete application is implemented in a single C++ source file, `main.cpp`.

## Features

- Intro screen and mouse-driven main menu.
- Two-player space shooter with moving saucers, lasers, aiming, life points, and a game-over screen.
- 3D solar-system viewer with:
  - Sun and planets
  - Earth's Moon
  - Planetary rings
  - Asteroid belt
  - Star field
  - Roaming rocket
- Mouse-look camera with zoom, strafing, planet lock-on, animation pause, and reset.
- Rocket simulation with pitch, yaw, thrust, braking, visual banking, engine glow, and beacon lights.
- Five ordered rocket missions: Mercury, Mars, Jupiter, Saturn, and Neptune.
- Mission score of 100 points for each target planet reached.
- Fixed 60 FPS timer for consistent animation and movement.
- Procedurally generated planet surfaces and Earth cloud texture.
- No external textures, models, sounds, or data files are required.

## Requirements

- A C++ compiler
- OpenGL
- GLUT or freeglut
- OpenGL Utility Library (GLU)
- A windowing environment capable of creating an OpenGL context

Freeglut is recommended on Windows.

## Building

### Windows with MinGW or MSYS2

Install freeglut and make sure its headers, library files, and runtime DLL are available to the compiler and executable.

From the project directory, run:

```bash
g++ main.cpp -o SpaceShooter.exe -lfreeglut -lopengl32 -lglu32
```

Run the program with:

```bash
./SpaceShooter.exe
```

If the linker cannot find freeglut, add its `include` and `lib` directories to the compiler search paths. Place the matching freeglut DLL beside the executable or in a directory listed in `PATH`.

### Linux

On Debian or Ubuntu-based systems, install the required development packages:

```bash
sudo apt install build-essential freeglut3-dev
```

Compile and run:

```bash
g++ main.cpp -o SpaceShooter -lglut -lGL -lGLU
./SpaceShooter
```

### macOS

With GLUT and OpenGL available, compile and run:

```bash
g++ main.cpp -o SpaceShooter -framework OpenGL -framework GLUT
./SpaceShooter
```

### Code::Blocks

1. Create a GLUT or freeglut project.
2. Replace the generated source file with `main.cpp`.
3. Configure the compiler include and library directories for freeglut.
4. Link against freeglut, OpenGL, and GLU.
5. Build and run the project.

## How to Play

### Starting the Application

The program opens on the title screen. Press **Enter** to open the main menu. Use the mouse to choose an option:

- **Start Game**: Opens the two-player shooter.
- **Solar System**: Opens the interactive 3D viewer.
- **Rocket Simulation**: Opens the guided flight mode.
- **Instructions**: Displays the shooter controls.
- **Quit**: Closes the application.

### Space Shooter

Both players can move and shoot at the same time. Each player starts with 100 LIFE points. A successful laser hit removes 5 LIFE points. The player whose opponent reaches zero LIFE wins.

| Player | Move Up | Move Down | Move Left | Move Right | Shoot |
| --- | --- | --- | --- | --- | --- |
| Player 1 | `W` | `S` | `A` | `D` | `C` |
| Player 2 | `I` | `K` | `J` | `L` | `M` |

Hold the shoot key together with the player's up or down key to angle the laser.

### Solar-System Viewer

| Input | Action |
| --- | --- |
| Hold left mouse button and drag | Look around |
| `W` / `S` | Zoom in / out by changing the field of view |
| `A` / `D` | Strafe left / right |
| `J` / `L` | Move forward / backward |
| `1` to `9` | Lock the camera to Mercury through Pluto |
| `0` | Unlock the camera |
| `R` | Reset the camera |
| `T` | Toggle the roaming rocket's self-rotation |
| Spacebar | Pause or resume planetary and asteroid motion |
| `Q` or `Esc` | Return to the main menu |

### Rocket Simulation

Fly the rocket close to each target planet in the order shown by the mission display. The simulation awards 100 points for every completed mission.

| Input | Action |
| --- | --- |
| Up / Down arrow | Pitch the rocket |
| Left / Right arrow | Yaw and visually bank the rocket |
| `W` | Apply thrust |
| `S` | Brake |
| `1` | Toggle the red beacon |
| `2` | Toggle the green beacon |
| `3` | Toggle the blue beacon |
| `R` | Reset the rocket, missions, and beacon lights |
| `Q` or `Esc` | Return to the main menu |

## Project Structure

```text
Space/
├── main.cpp    # Complete application source
└── README.md   # Project documentation
```

## Technical Notes

- The intro, menu, instructions, and shooter screens use orthographic projection.
- The Solar System and Rocket Simulation use perspective projection.
- Depth testing is enabled for the 3D modes.
- OpenGL lighting is used for planets, asteroids, rockets, and beacon lights.
- GLU camera helpers are used to create the 3D camera views.
- The application requests a double-buffered RGB window with a depth buffer.
- Keyboard states are polled every frame so movement, thrust, braking, and turning remain smooth while keys are held.
- A fixed timer runs at 60 frames per second.
- The initial window size is 1200 x 600 pixels and the viewport adapts when the window is resized.
- Planet textures and the Earth's cloud layer are generated procedurally at startup.
- The Solar System contains animated planetary orbits, self-rotation, an asteroid belt, stars, and a roaming rocket.

## Troubleshooting

### Header not found

Install the GLUT/freeglut development files and add their `include` directory to the compiler configuration.

### Linker errors for OpenGL or GLUT

Check that the platform-specific libraries are linked and that the 32-bit or 64-bit library versions match the compiler.

### Missing DLL on Windows

Copy the matching freeglut DLL next to the executable or add its directory to `PATH`.

### Blank window or incorrect 3D view

Use a graphics driver with OpenGL support and confirm that the program was linked with a depth-buffer display mode.

## Academic Context

This project was created for the Computer Science and Engineering computer graphics course at Southeast University during Academic Year 2026.

It demonstrates:

- 2D geometric drawing
- 3D transformations
- Materials and lighting
- Animation
- Camera control
- Keyboard and mouse input
- Collision-style laser hit detection
- Texture generation
- Interactive scene and mode management
