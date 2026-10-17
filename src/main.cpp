#include "Atmosphere.h"
#include "Ballistics.h"
#include "InputFile.h"
#include "Units.h"

#include <cctype>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    constexpr double PI = 3.14159265358979323846;

    // ---------------------------------------------------------------------------
    // Prompt helpers. Every numeric prompt accepts decimals (91.44, 29,92) and an
    // optional unit after the number which overrides the shown unit (e.g. "29.92 inHg").
    // ---------------------------------------------------------------------------
    bool readLine(std::string& line) {
        if (!std::getline(std::cin, line)) return false;
        line = trim(line);
        return true;
    }

    double askNumber(const std::string& prompt, double def, double minV, double maxV) {
        while (true) {
            std::cout << "  " << prompt << " [" << fmt(def) << "]: ";
            std::string line;
            if (!readLine(line) || line.empty()) return def;
            double v = 0.0;
            std::string unit;
            if (!parseNumberWithUnit(line, v, unit)) {
                std::cout << "    Not a valid number, try again.\n";
                continue;
            }
            if (v < minV || v > maxV) {
                std::cout << "    Please enter a value between " << fmt(minV) << " and " << fmt(maxV) << ".\n";
                continue;
            }
            return v;
        }
    }

    Value askValue(const std::string& prompt, const Value& def, Quantity q, double minBase, double maxBase) {
        const UnitDef* shown = findUnit(def.unit);
        while (true) {
            std::cout << "  " << prompt << " (" << def.unit << ") [" << fmt(def.v) << "]: ";
            std::string line;
            if (!readLine(line) || line.empty()) return def;

            double v = 0.0;
            std::string unit;
            if (!parseNumberWithUnit(line, v, unit)) {
                std::cout << "    Not a valid number, try again.\n";
                continue;
            }
            if (unit.empty()) unit = def.unit;
            const UnitDef* u = findUnit(unit);
            if (!u || u->quantity != q) {
                std::cout << "    '" << unit << "' is not a " << quantityName(q) << " unit.\n";
                continue;
            }
            double b = toBase(v, *u);
            if (b < minBase || b > maxBase) {
                std::cout << "    Please enter a value between " << fmt(std::round(fromBase(minBase, *shown) * 100) / 100)
                    << " and " << fmt(std::round(fromBase(maxBase, *shown) * 100) / 100) << " " << def.unit
                    << ".\n    Tip: you can type a unit after the number, e.g. '29.92 inHg' or '2600 fps'.\n";
                continue;
            }
            return { v, unit };
        }
    }

    char askChoice(const std::string& prompt, const std::string& options, char def) {
        while (true) {
            std::cout << "  " << prompt << " [" << def << "]: ";
            std::string line;
            if (!readLine(line) || line.empty()) return def;
            char c = static_cast<char>(std::toupper(static_cast<unsigned char>(line[0])));
            if (options.find(c) != std::string::npos) return c;
            std::cout << "    Please enter one of: " << options << "\n";
        }
    }

    // ---------------------------------------------------------------------------
    // Interactive input
    // ---------------------------------------------------------------------------
    InputSet askInputs() {
        using Q = Quantity;
        InputSet in;

        std::cout << "UNITS\n";
        bool us = askChoice("Input units (M = metric: m, cm, m/s, C, hPa | U = US: yd, in, fps, F, inHg, mph)",
            "MU", 'M') == 'U';
        bool grains = askChoice("Bullet/twist units (G = grains & inches | M = grams & mm)", "GM", 'G') == 'G';

        if (us) {   // friendlier US defaults
            in.muzzleVelocity = { 2600.0, "fps" };
            in.sightHeight = { 1.5, "in" };
            in.zeroRange = { 100.0, "yd" };
            in.temperature = { 59.0, "F" };
            in.altitude = { 0.0, "ft" };
            in.pressure = { 29.92, "inHg" };
            in.windSpeed = { 10.0, "mph" };
            in.maxRange = { 1000.0, "yd" };
            in.rangeStep = { 100.0, "yd" };
            in.output.rangeUnit = "yd";
            in.output.usUnits = true;
            in.output.useMil = false;
        }
        const std::string uMass = grains ? "gr" : "g";
        const std::string uSize = grains ? "in" : "mm";

        std::cout << "\nBULLET\n";
        in.model = askChoice("Drag model (1 = G1, 7 = G7)", "17", '7') == '1' ? DragModel::G1 : DragModel::G7;
        in.bc = askNumber("Ballistic coefficient (lb/in^2)", in.bc, 0.01, 2.0);
        in.muzzleVelocity = askValue("Muzzle velocity", in.muzzleVelocity, Q::Speed, 50.0, 2000.0);
        in.bulletWeight = askValue("Bullet weight", convertTo(in.bulletWeight, Q::Mass, uMass), Q::Mass,
            1.0 * 6.479891e-5, 3000.0 * 6.479891e-5);
        in.bulletDiameter = askValue("Bullet diameter", convertTo(in.bulletDiameter, Q::Length, uSize), Q::Length,
            0.1 * 0.0254, 1.0 * 0.0254);
        in.bulletLength = askValue("Bullet length", convertTo(in.bulletLength, Q::Length, uSize), Q::Length,
            0.2 * 0.0254, 4.0 * 0.0254);

        std::cout << "\nRIFLE\n";
        in.twist = askValue("Barrel twist, length per turn (1:7 -> 7)", convertTo(in.twist, Q::Length, uSize),
            Q::Length, 2.0 * 0.0254, 60.0 * 0.0254);
        in.rightHandTwist = askChoice("Twist direction (R/L)", "RL", 'R') == 'R';
        in.sightHeight = askValue("Sight height above bore", in.sightHeight, Q::Length, 0.0, 0.30);
        in.zeroRange = askValue("Zero range", in.zeroRange, Q::Length, 5.0, 2000.0);

        std::cout << "\nWEATHER\n";
        in.temperature = askValue("Temperature", in.temperature, Q::Temperature, -60.0, 60.0);
        in.humidityPct = askNumber("Relative humidity (%)", in.humidityPct, 0.0, 100.0);
        in.altitude = askValue("Altitude above sea level", in.altitude, Q::Length, -500.0, 9000.0);
        in.pressureIsStation =
            askChoice("Pressure type (S = station/absolute | B = barometric, sea-level corrected)", "SB", 'S') == 'S';
        in.pressure = askValue("Air pressure", in.pressure, Q::Pressure, 300.0, 1100.0);
        in.windSpeed = askValue("Wind speed", in.windSpeed, Q::Speed, 0.0, 60.0);
        in.windFromDeg = askNumber("Wind direction, blowing FROM (0-360, 0 = North)", in.windFromDeg, 0.0, 360.0);

        std::cout << "\nSHOT\n";
        in.shootingAzimuthDeg = askNumber("Shooting direction (0-360, 0 = North)", in.shootingAzimuthDeg, 0.0, 360.0);
        in.shootingAngleDeg = askNumber("Shooting angle (deg, + uphill / - downhill)", in.shootingAngleDeg, -89.0, 89.0);
        in.latitudeDeg = askNumber("Latitude (deg, + North / - South)", in.latitudeDeg, -90.0, 90.0);
        return in;
    }

    void askOutputOptions(InputSet& in) {
        using Q = Quantity;
        std::cout << "\nOUTPUT\n";
        char r = askChoice("Range unit (M = meters | Y = yards)", "MY", in.output.rangeUnit == "yd" ? 'Y' : 'M');
        in.output.rangeUnit = (r == 'Y') ? "yd" : "m";
        in.output.usUnits = askChoice("Drop/velocity/energy (M = metric: cm, m/s, J | U = US: in, fps, ft-lb)", "MU",
            in.output.usUnits ? 'U' : 'M') == 'U';
        in.output.useMil = askChoice("Scope corrections (M = MIL | O = MOA)", "MO", in.output.useMil ? 'M' : 'O') == 'M';
        // Keep round numbers when the range unit changes: 100 m step -> 100 yd step
        Value maxDef{ in.maxRange.v, in.output.rangeUnit };
        Value stepDef{ in.rangeStep.v, in.output.rangeUnit };
        if (toBase(maxDef, Q::Length, "max_range") > 5000.0) maxDef = convertTo(in.maxRange, Q::Length, in.output.rangeUnit);
        in.maxRange = askValue("Maximum range", maxDef, Q::Length, 10.0, 5000.0);
        in.rangeStep = askValue("Range step", stepDef, Q::Length, 0.5, 1000.0);
    }

    // ---------------------------------------------------------------------------
    // Output
    // ---------------------------------------------------------------------------
    std::string correction(double v, char posChar, char negChar, int prec) {
        double scale = std::pow(10.0, prec);
        double r = std::round(v * scale) / scale;
        std::ostringstream os;
        os << std::fixed << std::setprecision(prec);
        if (r == 0.0) os << "  " << 0.0;
        else os << (r > 0 ? posChar : negChar) << ' ' << std::fabs(r);
        return os.str();
    }

    void printResults(const Solution& sol, const SolverInputs& s, const OutputOptions& out) {
        const bool us = out.usUnits;
        const Atmosphere& atmo = sol.atmosphere;

        // display conversion factors (from internal metric)
        const double cmTo = us ? 1.0 / 2.54 : 1.0;                 // cm -> cm | in
        const double msTo = us ? 1.0 / 0.3048 : 1.0;               // m/s -> m/s | fps
        const double jTo = us ? 1.0 / 1.3558179483 : 1.0;          // J -> J | ft-lb
        const double windTo = us ? 1.0 / 0.44704 : 1.0;            // m/s -> m/s | mph
        const char* uSmall = us ? "(in)" : "(cm)";
        const char* uVel = us ? "(fps)" : "(m/s)";
        const char* uEnergy = us ? "(ft-lb)" : "(J)";
        const std::string uWind = us ? "mph" : "m/s";
        const std::string uRange = "(" + out.rangeUnit + ")";

        double pressure = us ? atmo.stationPressureHpa / 33.8638866667 : atmo.stationPressureHpa;
        std::cout << std::fixed << "\n=== ENVIRONMENT ===\n"
            << "  Station pressure : " << std::setprecision(us ? 2 : 1) << pressure << (us ? " inHg\n" : " hPa\n")
            << "  Air density      : " << std::setprecision(4) << atmo.density << " kg/m^3 ("
            << std::setprecision(1) << atmo.density / 1.225 * 100.0 << "% of ICAO standard)\n"
            << "  Speed of sound   : " << std::setprecision(1) << atmo.speedOfSound * msTo
            << (us ? " fps\n" : " m/s\n")
            << "  Local gravity    : " << std::setprecision(4) << sol.gravity * msTo
            << (us ? " ft/s^2\n" : " m/s^2\n")
            << "  Headwind comp.   : " << std::setprecision(2)
            << (std::fabs(sol.headwind) < 0.005 ? 0.0 : sol.headwind * windTo) << " " << uWind
            << " (+ = from target)\n"
            << "  Crosswind comp.  : " << std::fabs(sol.crosswind * windTo) << " " << uWind << " from the "
            << (sol.crosswind >= 0 ? "right" : "left") << "\n";

        std::string stability = sol.stabilityFactor < 1.0 ? "UNSTABLE - bullet will likely tumble"
            : sol.stabilityFactor < 1.4 ? "marginal - BC may be reduced"
            : "stable";
        double boreMoa = sol.boreAngleRad * 180.0 / PI * 60.0;
        std::cout << "\n=== RIFLE / BULLET ===\n"
            << "  Stability (Sg)   : " << std::setprecision(2) << sol.stabilityFactor << "  -> " << stability << "\n"
            << "  Bore angle (zero): " << std::setprecision(2) << boreMoa << " MOA / "
            << sol.boreAngleRad * 1000.0 << " MIL\n";

        const char* corrUnit = out.useMil ? "MIL" : "MOA";
        std::cout << "\n=== TRAJECTORY (corrections to dial: U/D elevation, L/R windage) ===\n"
            << std::right << std::setw(7) << "Range" << std::setw(9) << "Drop" << std::setw(9) << "Elev"
            << std::setw(9) << "Wind" << std::setw(9) << "Wind" << std::setw(8) << "Spin"
            << std::setw(8) << "CorH" << std::setw(8) << "CorV" << std::setw(8) << "Vel"
            << std::setw(7) << "Mach" << std::setw(9) << "Energy" << std::setw(8) << "Time" << "\n"
            << std::setw(7) << uRange << std::setw(9) << uSmall << std::setw(9) << corrUnit
            << std::setw(9) << uSmall << std::setw(9) << corrUnit << std::setw(8) << uSmall
            << std::setw(8) << uSmall << std::setw(8) << uSmall << std::setw(8) << uVel
            << std::setw(7) << "" << std::setw(9) << uEnergy << std::setw(8) << "(s)" << "\n"
            << std::string(99, '-') << "\n";

        double subsonicAt = -1.0;
        for (size_t i = 0; i < sol.points.size(); ++i) {
            const auto& p = sol.points[i];
            double range = s.rangesDisplay[i];
            double elev = out.useMil ? p.elevMil : p.elevMoa;
            double wind = out.useMil ? p.windMil : p.windMoa;
            if (subsonicAt < 0 && p.mach < 1.0) subsonicAt = range;

            std::cout << std::setw(7) << std::setprecision(range == std::floor(range) ? 0 : 1) << range
                << std::setw(9) << std::setprecision(1) << p.dropCm * cmTo
                << std::setw(9) << correction(elev, 'U', 'D', 2)
                << std::setw(9) << std::setprecision(1) << p.windageCm * cmTo
                << std::setw(9) << correction(wind, 'R', 'L', 2)
                << std::setw(8) << std::setprecision(1) << p.spinDriftCm * cmTo
                << std::setw(8) << std::setprecision(1) << p.coriolisHCm * cmTo
                << std::setw(8) << std::setprecision(1) << p.coriolisVCm * cmTo
                << std::setw(8) << std::setprecision(0) << p.velocity * msTo
                << std::setw(7) << std::setprecision(2) << p.mach
                << std::setw(9) << std::setprecision(0) << p.energyJ * jTo
                << std::setw(8) << std::setprecision(3) << p.time << "\n";
        }

        if (sol.points.size() < s.rangesM.size())
            std::cout << "\nNote: trajectory ended before maximum range (velocity/time limit reached).\n";
        if (subsonicAt >= 0)
            std::cout << "\nNote: bullet goes subsonic by ~" << std::setprecision(0) << subsonicAt << " "
            << out.rangeUnit << "; predictions past the transonic zone are less reliable.\n";
        std::cout << "\nWindage column already includes spin drift and horizontal Coriolis;\n"
            "drop column already includes vertical Coriolis (Eotvos effect).\n";
    }

    std::string stripQuotes(std::string s) {
        s = trim(s);
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
        return s;
    }

}  // namespace

// Usage:
//   ballistics                      -> asks for a file, or Enter to type values
//   ballistics inputs.txt           -> loads file, lets you change output options
//   ballistics inputs.txt --batch   -> loads file and runs without any prompts
int main(int argc, char** argv) {
    std::cout << "=============================================\n"
        << "  Point-Mass Ballistic Trajectory Calculator\n"
        << "  (press Enter to accept the [default])\n"
        << "=============================================\n\n";

    std::string path;
    bool batch = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--batch") batch = true;
        else path = a;
    }
    if (path.empty() && !batch) {
        std::cout << "Input file to load (Enter = type values manually): ";
        std::string line;
        if (readLine(line)) path = stripQuotes(line);
        std::cout << "\n";
    }

    InputSet in;
    const bool fromFile = !path.empty();
    if (fromFile) {
        std::vector<std::string> warnings;
        try {
            in = loadInputFile(path, warnings);
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        for (const auto& w : warnings) std::cout << "Warning (" << path << ") " << w << "\n";
        std::cout << "Loaded inputs from " << path << ":\n\n";
        writeInputs(std::cout, in);
    }
    else {
        in = askInputs();
    }

    if (!batch) askOutputOptions(in);

    if (!fromFile) {
        std::cout << "\n  Save these inputs to a file? (file name, Enter = skip): ";
        std::string line;
        if (readLine(line) && !line.empty()) {
            std::string file = stripQuotes(line);
            if (saveInputFile(file, in)) std::cout << "    Saved to " << file << "\n";
            else std::cout << "    Could not write " << file << "\n";
        }
    }

    try {
        SolverInputs s = buildSolverInputs(in);
        Atmosphere atmo = Atmosphere::fromWeather(s.weather);
        BallisticSolver solver(s.bullet, s.rifle, atmo, s.shot);
        Solution sol = solver.solve(s.rangesM);
        printResults(sol, s, in.output);
    }
    catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
    return 0;
}