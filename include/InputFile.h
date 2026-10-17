#pragma once
// All user inputs, each stored with the unit it was entered in.
// Can be filled interactively, loaded from a text file, or saved to one.

#include "Ballistics.h"
#include "Units.h"

#include <iosfwd>
#include <string>
#include <vector>

struct OutputOptions {
    std::string rangeUnit = "m";   // "m" or "yd"
    bool usUnits = false;          // false: cm, m/s, J   true: in, fps, ft-lb
    bool useMil = true;            // false: MOA
};

struct InputSet {
    // Bullet
    DragModel model = DragModel::G7;
    double bc = 0.243;                          // lb/in^2
    Value muzzleVelocity{790.0, "m/s"};
    Value bulletWeight{175.0, "gr"};
    Value bulletDiameter{0.308, "in"};
    Value bulletLength{1.24, "in"};
    // Rifle
    Value twist{10.0, "in"};                    // length per turn
    bool rightHandTwist = true;
    Value sightHeight{3.8, "cm"};
    Value zeroRange{100.0, "m"};
    // Weather
    Value temperature{15.0, "C"};
    double humidityPct = 50.0;
    Value altitude{0.0, "m"};
    bool pressureIsStation = true;
    Value pressure{1013.25, "hPa"};
    Value windSpeed{4.0, "m/s"};
    double windFromDeg = 270.0;
    // Shot
    double shootingAzimuthDeg = 0.0;
    double shootingAngleDeg = 0.0;
    double latitudeDeg = 45.0;
    // Table
    Value maxRange{1000.0, "m"};
    Value rangeStep{100.0, "m"};
    OutputOptions output;
};

// Solver-ready data (base/internal units) plus the table rows.
struct SolverInputs {
    Bullet bullet;
    Rifle rifle;
    Weather weather;
    ShotConditions shot;
    std::vector<double> rangesM;        // rows in metres (for the solver)
    std::vector<double> rangesDisplay;  // same rows in the chosen output range unit
};

// Throws std::runtime_error if the file cannot be opened. Bad lines are reported
// in `warnings` and skipped (their defaults are kept).
InputSet loadInputFile(const std::string& path, std::vector<std::string>& warnings);

void writeInputs(std::ostream& os, const InputSet& in);
bool saveInputFile(const std::string& path, const InputSet& in);

// Converts units and validates ranges. Throws std::runtime_error with a clear message.
SolverInputs buildSolverInputs(const InputSet& in);
