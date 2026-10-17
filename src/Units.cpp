#include "Units.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
const std::vector<UnitDef>& unitTable() {
    using Q = Quantity;
    static const std::vector<UnitDef> table = {
        // Length (base m)
        {"m", Q::Length, 1.0}, {"meter", Q::Length, 1.0}, {"meters", Q::Length, 1.0},
        {"metre", Q::Length, 1.0}, {"metres", Q::Length, 1.0},
        {"cm", Q::Length, 0.01}, {"mm", Q::Length, 0.001}, {"km", Q::Length, 1000.0},
        {"yd", Q::Length, 0.9144}, {"yds", Q::Length, 0.9144}, {"yard", Q::Length, 0.9144},
        {"yards", Q::Length, 0.9144},
        {"ft", Q::Length, 0.3048}, {"foot", Q::Length, 0.3048}, {"feet", Q::Length, 0.3048},
        {"in", Q::Length, 0.0254}, {"inch", Q::Length, 0.0254}, {"inches", Q::Length, 0.0254},
        // Speed (base m/s)
        {"m/s", Q::Speed, 1.0}, {"mps", Q::Speed, 1.0},
        {"fps", Q::Speed, 0.3048}, {"ft/s", Q::Speed, 0.3048},
        {"mph", Q::Speed, 0.44704},
        {"km/h", Q::Speed, 1.0 / 3.6}, {"kph", Q::Speed, 1.0 / 3.6}, {"kmh", Q::Speed, 1.0 / 3.6},
        {"kn", Q::Speed, 0.514444}, {"kt", Q::Speed, 0.514444}, {"knots", Q::Speed, 0.514444},
        // Mass (base kg)
        {"kg", Q::Mass, 1.0}, {"g", Q::Mass, 0.001}, {"gram", Q::Mass, 0.001}, {"grams", Q::Mass, 0.001},
        {"gr", Q::Mass, 6.479891e-5}, {"grain", Q::Mass, 6.479891e-5}, {"grains", Q::Mass, 6.479891e-5},
        {"lb", Q::Mass, 0.45359237},
        // Pressure (base hPa)
        {"hpa", Q::Pressure, 1.0}, {"mbar", Q::Pressure, 1.0}, {"mb", Q::Pressure, 1.0},
        {"kpa", Q::Pressure, 10.0}, {"pa", Q::Pressure, 0.01},
        {"inhg", Q::Pressure, 33.8638866667}, {"mmhg", Q::Pressure, 1.33322387415},
        {"psi", Q::Pressure, 68.9475729318},
        // Temperature (base C)
        {"c", Q::Temperature, 1.0, 0.0}, {"degc", Q::Temperature, 1.0, 0.0},
        {"f", Q::Temperature, 5.0 / 9.0, -160.0 / 9.0}, {"degf", Q::Temperature, 5.0 / 9.0, -160.0 / 9.0},
        {"k", Q::Temperature, 1.0, -273.15},
        // Energy (base J)
        {"j", Q::Energy, 1.0}, {"ft-lb", Q::Energy, 1.3558179483}, {"ftlb", Q::Energy, 1.3558179483},
        {"ft-lbf", Q::Energy, 1.3558179483}, {"ftlbf", Q::Energy, 1.3558179483},
    };
    return table;
}
}  // namespace

const UnitDef* findUnit(const std::string& name) {
    std::string key = toLower(trim(name));
    // Allow "°C" / "°F" (UTF-8 degree sign)
    const std::string deg = "\xC2\xB0";
    size_t p = key.find(deg);
    if (p != std::string::npos) key.erase(p, deg.size());
    for (const auto& u : unitTable())
        if (u.name == key) return &u;
    return nullptr;
}

const char* quantityName(Quantity q) {
    switch (q) {
        case Quantity::Length: return "length";
        case Quantity::Speed: return "speed";
        case Quantity::Mass: return "mass";
        case Quantity::Pressure: return "pressure";
        case Quantity::Temperature: return "temperature";
        case Quantity::Energy: return "energy";
    }
    return "?";
}

double toBase(const Value& x, Quantity expected, const std::string& what) {
    const UnitDef* u = findUnit(x.unit);
    if (!u) throw std::runtime_error(what + ": unknown unit '" + x.unit + "'");
    if (u->quantity != expected)
        throw std::runtime_error(what + ": '" + x.unit + "' is not a " + quantityName(expected) + " unit");
    return toBase(x.v, *u);
}

Value convertTo(const Value& x, Quantity q, const std::string& unit) {
    if (toLower(x.unit) == toLower(unit)) return x;
    const UnitDef* target = findUnit(unit);
    if (!target) throw std::runtime_error("unknown unit '" + unit + "'");
    double v = fromBase(toBase(x, q, "conversion"), *target);
    return {std::round(v * 1000.0) / 1000.0, unit};
}

bool parseNumberWithUnit(const std::string& textIn, double& number, std::string& unit) {
    std::string text = trim(textIn);
    if (text.empty()) return false;
    // Accept twist written as "1:7"
    if (text.size() > 2 && text[0] == '1' && text[1] == ':') text = trim(text.substr(2));
    // Accept decimal comma (European keyboards): 29,92 -> 29.92
    std::replace(text.begin(), text.end(), ',', '.');
    try {
        size_t used = 0;
        number = std::stod(text, &used);
        if (!std::isfinite(number)) return false;
        unit = trim(text.substr(used));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string fmt(double v) {
    std::ostringstream os;
    os << std::setprecision(10) << v;
    return os.str();
}
