#include "Ballistics.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    constexpr double PI = 3.14159265358979323846;
    constexpr double EARTH_OMEGA = 7.2921159e-5;   // rad/s
    constexpr double LBIN2_TO_KGM2 = 703.0695796;  // 1 lb/in^2 in kg/m^2
    constexpr double GRAIN_TO_KG = 6.479891e-5;
    constexpr double STD_DENSITY = 1.2250;         // ICAO sea-level density, kg/m^3
    constexpr double DT = 0.0005;                  // integration time step, s
    constexpr double MAX_TIME = 30.0;              // s
    constexpr double MIN_VELOCITY = 60.0;          // m/s

    double deg2rad(double d) { return d * PI / 180.0; }
}  // namespace

BallisticSolver::BallisticSolver(const Bullet& bullet, const Rifle& rifle,
    const Atmosphere& atmosphere, const ShotConditions& shot)
    : bullet_(bullet), rifle_(rifle), atmo_(atmosphere), shot_(shot),
    gravity_(localGravity(shot.latitudeDeg, atmosphere.altitudeM)) {
}

// Drag + gravity + Coriolis.
//   drag decel = rho * v^2 * pi * Cd_ref(M) / (8 * BC)   with BC in kg/m^2
Vec3 BallisticSolver::acceleration(const Vec3& vel, const Env& env) const {
    Vec3 vAir = vel - env.wind;              // velocity relative to the air mass
    double speed = vAir.norm();
    double mach = speed / atmo_.speedOfSound;
    double cd = dragCoefficient(bullet_.model, mach);
    double k = atmo_.density * PI * cd / (8.0 * bullet_.bc * LBIN2_TO_KGM2);

    Vec3 drag = vAir * (-k * speed);
    Vec3 coriolis = cross(env.omega, vel) * -2.0;
    return drag + env.gravity + coriolis;
}

std::vector<BallisticSolver::Sample> BallisticSolver::integrate(
    double boreAngle, const Env& env, const std::vector<double>& ranges) const {
    std::vector<Sample> out;
    if (ranges.empty()) return out;

    // Bullet starts on the bore line, which is sightHeight below the line of sight.
    Vec3 pos{ 0.0, -rifle_.sightHeightCm / 100.0, 0.0 };
    Vec3 vel{ bullet_.muzzleVelocity * std::cos(boreAngle),
             bullet_.muzzleVelocity * std::sin(boreAngle), 0.0 };
    double t = 0.0;
    size_t idx = 0;

    while (idx < ranges.size() && ranges[idx] <= 0.0) {
        out.push_back({ pos, vel, t });
        ++idx;
    }

    while (idx < ranges.size()) {
        // Classic RK4 on (position, velocity)
        const double h = DT;
        Vec3 k1v = acceleration(vel, env);
        Vec3 k1p = vel;
        Vec3 k2v = acceleration(vel + k1v * (h / 2), env);
        Vec3 k2p = vel + k1v * (h / 2);
        Vec3 k3v = acceleration(vel + k2v * (h / 2), env);
        Vec3 k3p = vel + k2v * (h / 2);
        Vec3 k4v = acceleration(vel + k3v * h, env);
        Vec3 k4p = vel + k3v * h;

        Vec3 newVel = vel + (k1v + k2v * 2.0 + k3v * 2.0 + k4v) * (h / 6.0);
        Vec3 newPos = pos + (k1p + k2p * 2.0 + k3p * 2.0 + k4p) * (h / 6.0);
        double newT = t + h;

        // Record every requested range crossed during this step (linear interpolation)
        while (idx < ranges.size() && newPos.x >= ranges[idx]) {
            double f = (ranges[idx] - pos.x) / (newPos.x - pos.x);
            out.push_back({ pos + (newPos - pos) * f, vel + (newVel - vel) * f, t + h * f });
            ++idx;
        }

        pos = newPos;
        vel = newVel;
        t = newT;

        if (t > MAX_TIME || vel.norm() < MIN_VELOCITY || vel.x <= 0.0) break;
    }
    return out;
}

double BallisticSolver::findZeroAngle() const {
    // Zero is assumed to have been set on flat ground, no wind, in today's air.
    Env flat;
    flat.gravity = { 0.0, -gravity_, 0.0 };

    const double R = rifle_.zeroRangeM;
    double angle = 0.0;
    for (int i = 0; i < 50; ++i) {
        auto s = integrate(angle, flat, { R });
        if (s.empty()) throw std::runtime_error("Zero range cannot be reached with these inputs.");
        double err = s[0].pos.y;            // metres above/below line of sight at zero range
        if (std::fabs(err) < 1e-6) break;
        angle -= err / R;                   // Newton step (dy/dangle ~= R)
    }
    return angle;
}

double BallisticSolver::stabilityFactor() const {
    // Don Miller's twist rule
    double d = bullet_.diameterIn;
    double t = rifle_.twistIn / d;          // twist in calibers
    double l = bullet_.lengthIn / d;        // length in calibers
    double sg = 30.0 * bullet_.massGrains / (t * t * d * d * d * l * (1.0 + l * l));

    double vFps = bullet_.muzzleVelocity * 3.28084;
    sg *= std::cbrt(vFps / 2800.0);         // velocity correction
    sg *= STD_DENSITY / atmo_.density;      // air density correction
    return sg;
}

Solution BallisticSolver::solve(const std::vector<double>& ranges) const {
    Solution sol;
    sol.atmosphere = atmo_;
    sol.gravity = gravity_;
    sol.stabilityFactor = stabilityFactor();
    sol.boreAngleRad = findZeroAngle();

    const double th = deg2rad(shot_.shootingAngleDeg);
    Env env;

    // Gravity expressed in the inclined line-of-sight frame
    env.gravity = { -gravity_ * std::sin(th), -gravity_ * std::cos(th), 0.0 };

    // Wind: relative angle 0 = blowing from the target (headwind), 90 = from the right
    double phi = deg2rad(shot_.windFromDeg - shot_.shootingAzimuthDeg);
    sol.headwind = shot_.windSpeed * std::cos(phi);
    sol.crosswind = shot_.windSpeed * std::sin(phi);
    double wDown = -sol.headwind;            // air moving downrange (horizontal)
    double wRight = -sol.crosswind;          // air moving to the right
    env.wind = { wDown * std::cos(th), -wDown * std::sin(th), wRight };

    // Earth rotation vector: local (downrange, up, right) then tilted by incline
    double lat = deg2rad(shot_.latitudeDeg);
    double az = deg2rad(shot_.shootingAzimuthDeg);
    double oDown = EARTH_OMEGA * std::cos(lat) * std::cos(az);
    double oUp = EARTH_OMEGA * std::sin(lat);
    double oRight = -EARTH_OMEGA * std::cos(lat) * std::sin(az);
    env.omega = { oDown * std::cos(th) + oUp * std::sin(th),
                 -oDown * std::sin(th) + oUp * std::cos(th), oRight };

    Env noCoriolis = env;
    noCoriolis.omega = {};

    auto full = integrate(sol.boreAngleRad, env, ranges);
    auto base = integrate(sol.boreAngleRad, noCoriolis, ranges);

    const double massKg = bullet_.massGrains * GRAIN_TO_KG;
    const double sgForDrift = std::max(sol.stabilityFactor, 0.0);
    const double twistSign = rifle_.rightHandTwist ? 1.0 : -1.0;

    for (size_t i = 0; i < full.size(); ++i) {
        const Sample& s = full[i];
        TrajectoryPoint p;
        p.range = ranges[i];
        p.time = s.t;
        p.velocity = s.vel.norm();
        p.mach = (s.vel - env.wind).norm() / atmo_.speedOfSound;
        p.energyJ = 0.5 * massKg * p.velocity * p.velocity;

        // Bryan Litz empirical spin-drift formula (inches)
        double spinM = 1.25 * (sgForDrift + 1.2) * std::pow(s.t, 1.83) * 0.0254 * twistSign;

        double y = s.pos.y;
        double z = s.pos.z + spinM;

        p.dropCm = y * 100.0;
        p.windageCm = z * 100.0;
        p.spinDriftCm = spinM * 100.0;
        if (i < base.size()) {
            p.coriolisHCm = (s.pos.z - base[i].pos.z) * 100.0;
            p.coriolisVCm = (s.pos.y - base[i].pos.y) * 100.0;
        }

        if (p.range > 0.0) {
            double elev = std::atan2(-y, p.range);
            double wind = std::atan2(-z, p.range);
            p.elevMil = elev * 1000.0;
            p.elevMoa = elev * 180.0 / PI * 60.0;
            p.windMil = wind * 1000.0;
            p.windMoa = wind * 180.0 / PI * 60.0;
        }
        sol.points.push_back(p);
    }
    return sol;
}