#ifndef _DRIVERS_THERMAL_PLATFORM_H
#define _DRIVERS_THERMAL_PLATFORM_H

struct coefficients {
	int frequency;
	int power;
};

#define CPUFREQ_LEVEL_INDEX_END		L24

struct coefficients little_cpu_coeffs[CPUFREQ_LEVEL_INDEX_END];
struct coefficients big_cpu_coeffs[CPUFREQ_LEVEL_INDEX_END];

#endif /* _DRIVERS_THERMAL_PLATFORM_H */
