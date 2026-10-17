#include "Atmosphere.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double R_DRY = 287.058;    // J/(kg*K) specific gas constant, dry air
constexpr double R_VAPOR = 461.495;  // J/(kg*K) specific gas constant, water vapour
constexpr double GAMMA = 1.4;        // ratio of specific heats for air
}  // namespace

double stationPressureFromSeaLevel(double seaLevelHpa, double altitudeM) {
    // International barometric formula (troposphere)
    return seaLevelHpa * std::pow(1.0 - 2.25577e-5 * altitudeM, 5.25588);
}

double saturationVaporPressureHpa(double temperatureC) {
    return 6.1078 * std::pow(10.0, 7.5 * temperatureC / (temperatureC + 237.3));
}

double localGravity(double latitudeDeg, double altitudeM) {
    double lat = latitudeDeg * PI / 180.0;
    double s = std::sin(lat);
    double s2 = std::sin(2.0 * lat);
    double g0 = 9.780327 * (1.0 + 0.0053024 * s * s - 0.0000058 * s2 * s2);
    return g0 - 3.086e-6 * altitudeM;
}

Atmosphere Atmosphere::fromWeather(const Weather& w) {
    Atmosphere a;
    a.temperatureC = w.temperatureC;
    a.humidityPct = std::clamp(w.humidityPct, 0.0, 100.0);
    a.altitudeM = w.altitudeM;

    double p = w.pressureIsStation ? w.pressureHpa
                                   : stationPressureFromSeaLevel(w.pressureHpa, w.altitudeM);
    a.stationPressureHpa = p;

    double tK = w.temperatureC + 273.15;
    double pv = (a.humidityPct / 100.0) * saturationVaporPressureHpa(w.temperatureC);  // hPa
    double pd = p - pv;                                                                  // hPa

    // Humid air density: sum of dry-air and water-vapour partial densities (Pa -> *100)
    a.density = (pd * 100.0) / (R_DRY * tK) + (pv * 100.0) / (R_VAPOR * tK);

    // Speed of sound using virtual temperature (humid air is slightly "lighter")
    double tVirtual = tK / (1.0 - (pv / p) * (1.0 - R_DRY / R_VAPOR));
    a.speedOfSound = std::sqrt(GAMMA * R_DRY * tVirtual);

    return a;
}
