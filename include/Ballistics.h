#pragma once

#include "Atmosphere.h"
#include "DragTables.h"

#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Small 3D vector. Solver frame (right-handed):
//   x = along the line of sight (downrange)
//   y = perpendicular to line of sight, "up"
//   z = to the shooter's right
// ---------------------------------------------------------------------------
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(double s) const { return { x * s, y * s, z * s }; }
    double norm() const { return std::sqrt(x * x + y * y + z * z); }
};

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------
struct Bullet {
    double bc = 0.243;                 // ballistic coefficient (lb/in^2)
    DragModel model = DragModel::G7;
    double muzzleVelocity = 790.0;     // m/s
    double massGrains = 175.0;         // needed for energy + stability
    double diameterIn = 0.308;         // needed for stability
    double lengthIn = 1.240;           // needed for stability
};

struct Rifle {
    double twistIn = 10.0;             // inches per full turn (1:10)
    bool rightHandTwist = true;
    double sightHeightCm = 3.8;        // centre of scope above centre of bore
    double zeroRangeM = 100.0;
};

struct ShotConditions {
    double windSpeed = 0.0;            // m/s
    double windFromDeg = 0.0;          // compass direction wind blows FROM (0 = N, 90 = E)
    double shootingAzimuthDeg = 0.0;   // compass direction of the shot (0 = N, 90 = E)
    double shootingAngleDeg = 0.0;     // incline, + uphill / - downhill
    double latitudeDeg = 45.0;         // + north / - south
};

// ---------------------------------------------------------------------------
// Outputs
// ---------------------------------------------------------------------------
struct TrajectoryPoint {
    double range = 0.0;        // m (along line of sight)
    double time = 0.0;         // s
    double velocity = 0.0;     // m/s (ground-relative)
    double mach = 0.0;
    double energyJ = 0.0;
    double dropCm = 0.0;       // relative to line of sight, + above / - below
    double elevMil = 0.0;      // correction to dial, + = up
    double elevMoa = 0.0;
    double windageCm = 0.0;    // total lateral offset incl. spin drift & Coriolis, + right
    double windMil = 0.0;      // correction to dial, + = right
    double windMoa = 0.0;
    double spinDriftCm = 0.0;
    double coriolisHCm = 0.0;  // horizontal Coriolis contribution
    double coriolisVCm = 0.0;  // vertical (Eotvos) contribution
};

struct Solution {
    Atmosphere atmosphere;
    double gravity = 9.80665;
    double stabilityFactor = 0.0;   // Miller Sg
    double boreAngleRad = 0.0;      // bore angle relative to line of sight
    double headwind = 0.0;          // m/s, + = from target towards shooter
    double crosswind = 0.0;         // m/s, + = from the right
    std::vector<TrajectoryPoint> points;
};

// ---------------------------------------------------------------------------
// Point-mass solver (3-DOF) with RK4 integration
// ---------------------------------------------------------------------------
class BallisticSolver {
public:
    BallisticSolver(const Bullet& bullet, const Rifle& rifle,
        const Atmosphere& atmosphere, const ShotConditions& shot);

    // ranges: distances along the line of sight in metres, ascending
    Solution solve(const std::vector<double>& rangesM) const;

    double stabilityFactor() const;   // Miller twist rule, density/velocity corrected
    double findZeroAngle() const;     // bore angle that zeroes at rifle.zeroRangeM

private:
    struct Env {
        Vec3 gravity;
        Vec3 wind;
        Vec3 omega;   // Earth rotation vector in solver frame
    };
    struct Sample {
        Vec3 pos;
        Vec3 vel;
        double t = 0.0;
    };

    Vec3 acceleration(const Vec3& vel, const Env& env) const;
    std::vector<Sample> integrate(double boreAngle, const Env& env,
        const std::vector<double>& ranges) const;

    Bullet bullet_;
    Rifle rifle_;
    Atmosphere atmo_;
    ShotConditions shot_;
    double gravity_;
};