BallisticsCalculator
====================

A command-line external ballistics calculator written in modern C++17.

It computes projectile trajectories using a 3-degree-of-freedom (point-mass)
model with RK4 integration. The solver accounts for:

  - G1 and G7 drag models
  - Atmosphere (temperature, pressure, humidity, altitude)
  - Wind (headwind / crosswind components)
  - Spin drift (Bryan Litz empirical formula)
  - Coriolis / Eötvös effects
  - Inclined shots (uphill / downhill)
  - Local gravity variation with latitude and altitude
  - Miller stability factor (Sg)

The program is designed to be reused as a library or run as a standalone
tool. Input can be given interactively or via a simple text file.

License
-------
MIT License – see LICENSE.txt


Building
--------
Requirements:
  - CMake 3.14 or newer
  - A C++17 compiler (GCC, Clang, or MSVC)

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

This produces the executable `ballistics` (or `ballistics.exe` on Windows).


Running
-------

### Interactive mode
```bash
./ballistics
```
The program will prompt for units, bullet data, rifle data, weather, and
shot conditions. Defaults are provided so you can just press Enter for
common values.

### File mode
```bash
./ballistics path/to/input.txt
```
You will still be asked for output options (range unit, metric/US, MIL/MOA).

### Batch mode (no prompts)
```bash
./ballistics path/to/input.txt --batch
```
All settings (including output options) are taken from the input file.


Input file format
-----------------
Lines starting with `#` are comments.  
Each setting is written as:

```
key = value [unit]
```

Units may be mixed freely (e.g. `2600 fps`, `100 yd`, `29.92 inHg`, `15 C`).

Example (see also `test.txt` in the repository):

```
# --- Bullet ---
drag_model          = G7
bc                  = 0.274
muzzle_velocity     = 2600 fps
bullet_weight       = 88 gr
bullet_diameter     = 0.224 in
bullet_length       = 1.245 in

# --- Rifle ---
twist               = 7 in           # 1:7
twist_direction     = R
sight_height        = 2.5 in
zero_range          = 100 yd

# --- Weather ---
temperature         = 59 F
humidity            = 50 %
altitude            = 1000 ft
pressure_type       = barometric     # or "station"
pressure            = 29.92 inHg
wind_speed          = 10 mph
wind_direction      = 90             # degrees, wind FROM (0 = North)

# --- Shot ---
shooting_direction  = 0              # degrees, 0 = North
shooting_angle      = 0              # + uphill / - downhill
latitude            = 45             # + North / - South

# --- Output ---
max_range           = 1000 yd
range_step          = 100 yd
range_unit          = yd             # m | yd
output_units        = us             # metric | us
corrections         = moa            # mil | moa
```

Supported keys
--------------
Bullet:
  drag_model, bc, muzzle_velocity, bullet_weight,
  bullet_diameter, bullet_length

Rifle:
  twist, twist_direction (R/L), sight_height, zero_range

Weather:
  temperature, humidity, altitude,
  pressure_type (station | barometric), pressure,
  wind_speed, wind_direction

Shot:
  shooting_direction, shooting_angle, latitude

Output:
  max_range, range_step, range_unit, output_units, corrections


Output
------
The program prints:

  - Environment summary (station pressure, density, speed of sound, gravity,
    headwind / crosswind components)
  - Rifle / bullet summary (Miller stability factor, bore angle for zero)
  - Trajectory table with columns for:
      Range, Drop, Elevation correction, Windage, Wind correction,
      Spin drift, Coriolis H/V, Velocity, Mach, Energy, Time of flight


Project structure
-----------------
```
BallisticsCalculator/
├── include/
│   ├── Atmosphere.h      # Weather → air density / speed of sound
│   ├── Ballistics.h      # Solver interface and data structures
│   ├── DragTables.h      # G1 / G7 Cd vs Mach tables
│   ├── InputFile.h       # Text-file parser
│   └── Units.h           # Flexible unit conversion
├── src/
│   ├── Atmosphere.cpp
│   ├── Ballistics.cpp    # RK4 integrator + physics
│   ├── InputFile.cpp
│   ├── Units.cpp
│   └── main.cpp          # CLI front-end
├── CMakeLists.txt
├── LICENSE.txt
└── test.txt              # Example input file
```


Notes & disclaimer
------------------
This is a mathematical model. Real-world results depend on many factors
(ammunition lot, barrel, exact atmosphere, etc.). Always confirm zero and
trajectory with live fire under your actual conditions.

The author intends to reuse this calculator in other projects. Feedback and
bug reports are welcome.

Repository: https://github.com/sarkikarl/BallisticsCalculator
