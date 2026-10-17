#pragma once
// Unit handling. Every physical value is converted to a base unit before use:
//   Length = m, Speed = m/s, Mass = kg, Pressure = hPa, Temperature = C, Energy = J

#include <algorithm>
#include <cctype>
#include <string>

enum class Quantity { Length, Speed, Mass, Pressure, Temperature, Energy };

struct UnitDef {
    std::string name;     // lower-case lookup name
    Quantity quantity;
    double factor;        // base = value * factor + offset
    double offset = 0.0;
};

// Case-insensitive lookup ("inHg", "INHG", "inhg" all work). Returns nullptr if unknown.
const UnitDef* findUnit(const std::string& name);

inline double toBase(double value, const UnitDef& u) { return value * u.factor + u.offset; }
inline double fromBase(double base, const UnitDef& u) { return (base - u.offset) / u.factor; }

const char* quantityName(Quantity q);

// A number together with the unit it was entered in, e.g. {29.92, "inHg"}
struct Value {
    double v = 0.0;
    std::string unit;
};

// Converts a Value to base units; throws std::runtime_error on unknown/mismatched unit.
double toBase(const Value& x, Quantity expected, const std::string& what);

// Re-expresses a Value in another unit of the same quantity (rounded to 3 decimals).
Value convertTo(const Value& x, Quantity q, const std::string& unit);

// Parses "29.92", "29,92 inHg", "2600fps", "1:7 in" ... into number + unit text.
bool parseNumberWithUnit(const std::string& text, double& number, std::string& unit);

// Formats a double without trailing zeros (4.4704, 982.05, 100).
std::string fmt(double v);

inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
