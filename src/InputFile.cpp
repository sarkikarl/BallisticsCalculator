#include "InputFile.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace {

constexpr double GRAIN_KG = 6.479891e-5;
constexpr double INCH_M = 0.0254;

Value parseValue(const std::string& text, Quantity q, const std::string& defaultUnit) {
    double v = 0.0;
    std::string u;
    if (!parseNumberWithUnit(text, v, u)) throw std::runtime_error("'" + text + "' is not a number");
    if (u.empty()) u = defaultUnit;
    const UnitDef* def = findUnit(u);
    if (!def || def->quantity != q)
        throw std::runtime_error("'" + u + "' is not a valid " + quantityName(q) + " unit");
    return {v, u};
}

// Plain number; tolerates a trailing "%" or "deg"
double parsePlain(const std::string& text) {
    double v = 0.0;
    std::string u;
    if (!parseNumberWithUnit(text, v, u)) throw std::runtime_error("'" + text + "' is not a number");
    u = toLower(u);
    if (!u.empty() && u != "%" && u != "deg" && u != "degrees" && u != "\xC2\xB0")
        throw std::runtime_error("unexpected unit '" + u + "'");
    return v;
}

void applyKey(InputSet& in, const std::string& key, const std::string& val) {
    std::string lv = toLower(val);
    using Q = Quantity;

    if (key == "drag_model") {
        if (lv == "g1" || lv == "1") in.model = DragModel::G1;
        else if (lv == "g7" || lv == "7") in.model = DragModel::G7;
        else throw std::runtime_error("drag_model must be G1 or G7");
    }
    else if (key == "bc") in.bc = parsePlain(val);
    else if (key == "muzzle_velocity") in.muzzleVelocity = parseValue(val, Q::Speed, "m/s");
    else if (key == "bullet_weight") in.bulletWeight = parseValue(val, Q::Mass, "gr");
    else if (key == "bullet_diameter") in.bulletDiameter = parseValue(val, Q::Length, "in");
    else if (key == "bullet_length") in.bulletLength = parseValue(val, Q::Length, "in");
    else if (key == "twist") in.twist = parseValue(val, Q::Length, "in");
    else if (key == "twist_direction") {
        if (lv == "r" || lv == "right") in.rightHandTwist = true;
        else if (lv == "l" || lv == "left") in.rightHandTwist = false;
        else throw std::runtime_error("twist_direction must be R or L");
    }
    else if (key == "sight_height") in.sightHeight = parseValue(val, Q::Length, "cm");
    else if (key == "zero_range") in.zeroRange = parseValue(val, Q::Length, "m");
    else if (key == "temperature") in.temperature = parseValue(val, Q::Temperature, "C");
    else if (key == "humidity") in.humidityPct = parsePlain(val);
    else if (key == "altitude") in.altitude = parseValue(val, Q::Length, "m");
    else if (key == "pressure_type") {
        if (lv == "station" || lv == "absolute" || lv == "s") in.pressureIsStation = true;
        else if (lv == "barometric" || lv == "sea-level" || lv == "sealevel" || lv == "qnh" || lv == "b")
            in.pressureIsStation = false;
        else throw std::runtime_error("pressure_type must be station or barometric");
    }
    else if (key == "pressure") in.pressure = parseValue(val, Q::Pressure, "hPa");
    else if (key == "wind_speed") in.windSpeed = parseValue(val, Q::Speed, "m/s");
    else if (key == "wind_direction") in.windFromDeg = parsePlain(val);
    else if (key == "shooting_direction") in.shootingAzimuthDeg = parsePlain(val);
    else if (key == "shooting_angle") in.shootingAngleDeg = parsePlain(val);
    else if (key == "latitude") in.latitudeDeg = parsePlain(val);
    else if (key == "max_range") in.maxRange = parseValue(val, Q::Length, "m");
    else if (key == "range_step") in.rangeStep = parseValue(val, Q::Length, "m");
    else if (key == "range_unit") {
        if (lv == "m" || lv == "meters" || lv == "metres") in.output.rangeUnit = "m";
        else if (lv == "yd" || lv == "yards" || lv == "yds") in.output.rangeUnit = "yd";
        else throw std::runtime_error("range_unit must be m or yd");
    }
    else if (key == "output_units") {
        if (lv == "metric") in.output.usUnits = false;
        else if (lv == "us" || lv == "imperial") in.output.usUnits = true;
        else throw std::runtime_error("output_units must be metric or us");
    }
    else if (key == "corrections") {
        if (lv == "mil" || lv == "mrad") in.output.useMil = true;
        else if (lv == "moa") in.output.useMil = false;
        else throw std::runtime_error("corrections must be mil or moa");
    }
    else throw std::runtime_error("unknown key '" + key + "'");
}

void check(double v, double lo, double hi, const std::string& what, const std::string& unit) {
    if (!(v >= lo && v <= hi))
        throw std::runtime_error(what + " must be between " + fmt(lo) + " and " + fmt(hi) + " " + unit +
                                 " (got " + fmt(v) + " " + unit + ")");
}

std::string val(const Value& x) { return fmt(x.v) + " " + x.unit; }

}  // namespace

InputSet loadInputFile(const std::string& path, std::vector<std::string>& warnings) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open input file: " + path);

    InputSet in;
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            warnings.push_back("line " + std::to_string(lineNo) + ": missing '='");
            continue;
        }
        std::string key = toLower(trim(line.substr(0, eq)));
        std::string value = trim(line.substr(eq + 1));
        try {
            applyKey(in, key, value);
        } catch (const std::exception& e) {
            warnings.push_back("line " + std::to_string(lineNo) + ": " + e.what());
        }
    }
    return in;
}

void writeInputs(std::ostream& os, const InputSet& in) {
    auto line = [&os](const char* key, const std::string& value) {
        os << std::left << std::setw(20) << key << "= " << value << "\n";
    };
    os << "# Ballistic calculator input file\n"
       << "# Format: key = value [unit]. Units may be mixed (m, yd, ft, in, cm, mm, m/s, fps, mph,\n"
       << "# km/h, gr, g, C, F, hPa, inHg, mmHg ...). Lines starting with # are comments.\n\n"
       << "# --- Bullet ---\n";
    line("drag_model", in.model == DragModel::G1 ? "G1" : "G7");
    line("bc", fmt(in.bc));
    line("muzzle_velocity", val(in.muzzleVelocity));
    line("bullet_weight", val(in.bulletWeight));
    line("bullet_diameter", val(in.bulletDiameter));
    line("bullet_length", val(in.bulletLength));
    os << "\n# --- Rifle ---\n";
    line("twist", val(in.twist) + "        # length per turn, 1:7 = 7 in");
    line("twist_direction", in.rightHandTwist ? "R" : "L");
    line("sight_height", val(in.sightHeight));
    line("zero_range", val(in.zeroRange));
    os << "\n# --- Weather ---\n";
    line("temperature", val(in.temperature));
    line("humidity", fmt(in.humidityPct) + " %");
    line("altitude", val(in.altitude));
    line("pressure_type", in.pressureIsStation ? "station" : "barometric");
    line("pressure", val(in.pressure));
    line("wind_speed", val(in.windSpeed));
    line("wind_direction", fmt(in.windFromDeg) + "        # compass direction wind blows FROM, 0 = N");
    os << "\n# --- Shot ---\n";
    line("shooting_direction", fmt(in.shootingAzimuthDeg) + "        # compass, 0 = N");
    line("shooting_angle", fmt(in.shootingAngleDeg) + "        # + uphill / - downhill");
    line("latitude", fmt(in.latitudeDeg) + "        # + N / - S");
    os << "\n# --- Output ---\n";
    line("max_range", val(in.maxRange));
    line("range_step", val(in.rangeStep));
    line("range_unit", in.output.rangeUnit + "        # m | yd");
    line("output_units", std::string(in.output.usUnits ? "us" : "metric") + "        # metric | us");
    line("corrections", std::string(in.output.useMil ? "mil" : "moa") + "        # mil | moa");
}

bool saveInputFile(const std::string& path, const InputSet& in) {
    std::ofstream f(path);
    if (!f) return false;
    writeInputs(f, in);
    return static_cast<bool>(f);
}

SolverInputs buildSolverInputs(const InputSet& in) {
    using Q = Quantity;
    SolverInputs s;

    s.bullet.model = in.model;
    s.bullet.bc = in.bc;
    s.bullet.muzzleVelocity = toBase(in.muzzleVelocity, Q::Speed, "muzzle_velocity");
    s.bullet.massGrains = toBase(in.bulletWeight, Q::Mass, "bullet_weight") / GRAIN_KG;
    s.bullet.diameterIn = toBase(in.bulletDiameter, Q::Length, "bullet_diameter") / INCH_M;
    s.bullet.lengthIn = toBase(in.bulletLength, Q::Length, "bullet_length") / INCH_M;

    s.rifle.twistIn = toBase(in.twist, Q::Length, "twist") / INCH_M;
    s.rifle.rightHandTwist = in.rightHandTwist;
    s.rifle.sightHeightCm = toBase(in.sightHeight, Q::Length, "sight_height") * 100.0;
    s.rifle.zeroRangeM = toBase(in.zeroRange, Q::Length, "zero_range");

    s.weather.temperatureC = toBase(in.temperature, Q::Temperature, "temperature");
    s.weather.humidityPct = in.humidityPct;
    s.weather.altitudeM = toBase(in.altitude, Q::Length, "altitude");
    s.weather.pressureIsStation = in.pressureIsStation;
    s.weather.pressureHpa = toBase(in.pressure, Q::Pressure, "pressure");

    s.shot.windSpeed = toBase(in.windSpeed, Q::Speed, "wind_speed");
    s.shot.windFromDeg = in.windFromDeg;
    s.shot.shootingAzimuthDeg = in.shootingAzimuthDeg;
    s.shot.shootingAngleDeg = in.shootingAngleDeg;
    s.shot.latitudeDeg = in.latitudeDeg;

    // Validation (in internal units)
    check(s.bullet.bc, 0.01, 2.0, "bc", "lb/in^2");
    check(s.bullet.muzzleVelocity, 50.0, 2000.0, "muzzle_velocity", "m/s");
    check(s.bullet.massGrains, 1.0, 3000.0, "bullet_weight", "gr");
    check(s.bullet.diameterIn, 0.1, 1.0, "bullet_diameter", "in");
    check(s.bullet.lengthIn, 0.2, 4.0, "bullet_length", "in");
    check(s.rifle.twistIn, 2.0, 60.0, "twist", "in");
    check(s.rifle.sightHeightCm, -30.0, 30.0, "sight_height", "cm");
    check(s.rifle.zeroRangeM, 5.0, 2000.0, "zero_range", "m");
    check(s.weather.temperatureC, -60.0, 60.0, "temperature", "C");
    check(s.weather.humidityPct, 0.0, 100.0, "humidity", "%");
    check(s.weather.altitudeM, -500.0, 9000.0, "altitude", "m");
    check(s.weather.pressureHpa, 300.0, 1100.0, "pressure", "hPa");
    check(s.shot.windSpeed, 0.0, 60.0, "wind_speed", "m/s");
    check(s.shot.windFromDeg, 0.0, 360.0, "wind_direction", "deg");
    check(s.shot.shootingAzimuthDeg, 0.0, 360.0, "shooting_direction", "deg");
    check(s.shot.shootingAngleDeg, -89.0, 89.0, "shooting_angle", "deg");
    check(s.shot.latitudeDeg, -90.0, 90.0, "latitude", "deg");

    // Table rows in the chosen output range unit
    double unitM = (in.output.rangeUnit == "yd") ? 0.9144 : 1.0;
    double maxM = toBase(in.maxRange, Q::Length, "max_range");
    double stepM = toBase(in.rangeStep, Q::Length, "range_step");
    check(maxM, 10.0, 5000.0, "max_range", "m");
    check(stepM, 0.5, 1000.0, "range_step", "m");

    double maxD = maxM / unitM;
    double stepD = stepM / unitM;
    int rows = static_cast<int>(std::floor(maxD / stepD + 1e-9));
    if (rows > 2000) throw std::runtime_error("too many table rows; increase range_step");
    for (int i = 0; i <= rows; ++i) {
        double r = i * stepD;
        s.rangesDisplay.push_back(r);
        s.rangesM.push_back(r * unitM);
    }
    return s;
}
