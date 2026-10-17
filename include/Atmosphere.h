#pragma once

// Raw weather input as the shooter would read it from a weather meter / app.
struct Weather {
    double temperatureC = 15.0;
    double pressureHpa = 1013.25;
    bool pressureIsStation = true;   // true  = absolute (station) pressure at the firing point
                                     // false = barometric pressure corrected to sea level (QNH)
    double humidityPct = 50.0;       // relative humidity 0..100 %
    double altitudeM = 0.0;          // altitude of firing point above sea level
};

// Derived air properties used by the solver.
struct Atmosphere {
    double temperatureC = 15.0;
    double stationPressureHpa = 1013.25;
    double humidityPct = 50.0;
    double altitudeM = 0.0;
    double density = 1.225;          // kg/m^3 (humid air)
    double speedOfSound = 340.3;     // m/s

    static Atmosphere fromWeather(const Weather& w);
};

// Converts a sea-level-corrected reading to absolute pressure at the given altitude.
double stationPressureFromSeaLevel(double seaLevelHpa, double altitudeM);

// Saturation vapour pressure of water (hPa), Tetens equation.
double saturationVaporPressureHpa(double temperatureC);

// Local gravitational acceleration (m/s^2) from latitude (WGS-84 / Somigliana)
// with free-air correction for altitude.
double localGravity(double latitudeDeg, double altitudeM);
