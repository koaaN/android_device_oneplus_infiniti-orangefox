/*
 * Copyright (C) 2026 The OrangeFox Recovery Project
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <android-base/properties.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <glob.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr auto kPollInterval = std::chrono::seconds(3);
constexpr int kWarmSamples = 2;
constexpr int kReleaseSamples = 10;  // 30 seconds at the normal poll interval.
constexpr int kReapplySamples = 10;

constexpr int64_t kWarmEnter = 78000;
constexpr int64_t kWarmRelease = 72000;
constexpr int64_t kElevatedEnter = 83000;
constexpr int64_t kElevatedRelease = 77000;
constexpr int64_t kHotEnter = 90000;
constexpr int64_t kHotRelease = 84000;
constexpr int64_t kCriticalEnter = 97000;
constexpr int64_t kCriticalRelease = 90000;

enum class State { kNormal, kWarm, kElevated, kHot, kCritical };

struct CpuPolicy {
    std::string path;
    int64_t hardware_max = 0;
    int64_t original_max = 0;
    int64_t last_written = 0;
    std::vector<int64_t> available_frequencies;
};

struct ThermalSensor {
    std::string path;
    std::string name;
};

struct ThermalReading {
    int64_t temperature = 0;
    std::string sensor;
};

std::atomic_bool g_running{true};

void HandleSignal(int) {
    g_running = false;
}

void Log(const std::string& message) {
    std::ofstream kmsg("/dev/kmsg");
    if (kmsg)
        kmsg << "[fox-thermal-guard] " << message << '\n';
}

bool ReadInt(const std::string& path, int64_t* value) {
    std::ifstream input(path);
    return input && (input >> *value);
}

std::string ReadString(const std::string& path) {
    std::ifstream input(path);
    std::string value;
    input >> value;
    return value;
}

bool WriteInt(const std::string& path, int64_t value) {
    std::ofstream output(path);
    if (!output)
        return false;
    output << value;
    return output.good();
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.compare(0, prefix.size(), prefix) == 0;
}

std::vector<int64_t> ReadFrequencies(const std::string& path) {
    std::vector<int64_t> frequencies;
    std::ifstream input(path);
    int64_t frequency;
    while (input >> frequency) {
        if (frequency > 0)
            frequencies.push_back(frequency);
    }
    std::sort(frequencies.begin(), frequencies.end());
    frequencies.erase(std::unique(frequencies.begin(), frequencies.end()), frequencies.end());
    return frequencies;
}

std::vector<CpuPolicy> DiscoverPolicies() {
    std::vector<CpuPolicy> policies;
    glob_t matches{};
    if (glob("/sys/devices/system/cpu/cpufreq/policy*", 0, nullptr, &matches) != 0)
        return policies;

    for (size_t i = 0; i < matches.gl_pathc; ++i) {
        CpuPolicy policy;
        policy.path = matches.gl_pathv[i];
        if (!ReadInt(policy.path + "/cpuinfo_max_freq", &policy.hardware_max) ||
            !ReadInt(policy.path + "/scaling_max_freq", &policy.original_max) ||
            policy.hardware_max <= 0 || policy.original_max <= 0) {
            continue;
        }
        policy.available_frequencies =
            ReadFrequencies(policy.path + "/scaling_available_frequencies");
        policies.push_back(std::move(policy));
    }
    globfree(&matches);
    return policies;
}

std::vector<ThermalSensor> DiscoverSensors() {
    std::vector<ThermalSensor> sensors;
    glob_t matches{};
    if (glob("/sys/class/thermal/thermal_zone*", 0, nullptr, &matches) != 0)
        return sensors;

    for (size_t i = 0; i < matches.gl_pathc; ++i) {
        const std::string zone = matches.gl_pathv[i];
        const std::string name = ReadString(zone + "/type");
        if (name.empty() || StartsWith(name, "cpu-hw-trip-"))
            continue;
        if (StartsWith(name, "cpu-") || StartsWith(name, "cpullc-") ||
            StartsWith(name, "qmx-")) {
            sensors.push_back({zone + "/temp", name});
        }
    }
    globfree(&matches);
    return sensors;
}

ThermalReading ReadHottest(const std::vector<ThermalSensor>& sensors) {
    ThermalReading hottest;
    for (const auto& sensor : sensors) {
        int64_t temperature;
        if (!ReadInt(sensor.path, &temperature))
            continue;
        if (temperature > 0 && temperature < 1000)
            temperature *= 1000;
        if (temperature <= 0 || temperature >= 200000)
            continue;
        if (temperature > hottest.temperature)
            hottest = {temperature, sensor.name};
    }
    return hottest;
}

int CapPercent(State state) {
    switch (state) {
        case State::kWarm: return 80;
        case State::kElevated: return 70;
        case State::kHot: return 55;
        case State::kCritical: return 35;
        case State::kNormal: return 100;
    }
}

const char* StateName(State state) {
    switch (state) {
        case State::kWarm: return "warm";
        case State::kElevated: return "elevated";
        case State::kHot: return "hot";
        case State::kCritical: return "critical";
        case State::kNormal: return "normal";
    }
}

int64_t ChooseCap(const CpuPolicy& policy, int percent) {
    if (percent == 100)
        return policy.original_max;

    const int64_t target = policy.hardware_max * percent / 100;
    int64_t selected = 0;
    for (int64_t frequency : policy.available_frequencies) {
        if (frequency <= target)
            selected = frequency;
        else
            break;
    }
    return selected > 0 ? selected : target;
}

void ApplyState(std::vector<CpuPolicy>* policies, State state,
                const ThermalReading& reading) {
    const int percent = CapPercent(state);
    for (auto& policy : *policies) {
        int64_t current;
        if (!ReadInt(policy.path + "/scaling_max_freq", &current))
            continue;

        const int64_t target = ChooseCap(policy, percent);
        bool should_write = false;
        if (state == State::kNormal) {
            // Raise a limit only when it still equals the value this daemon
            // wrote. A different lower value belongs to another controller.
            should_write = policy.last_written > 0 && current == policy.last_written;
        } else if (current > target) {
            should_write = true;
        } else if (policy.last_written > 0 && current == policy.last_written &&
                   target > current) {
            // Controlled step-down from a stronger guard state.
            should_write = true;
        }

        if (should_write) {
            if (WriteInt(policy.path + "/scaling_max_freq", target))
                policy.last_written = target;
            else
                Log("failed to write " + policy.path + ": " + std::to_string(errno));
        } else if (current != policy.last_written) {
            policy.last_written = 0;
        }

        if (state == State::kNormal)
            policy.last_written = 0;
    }

    android::base::SetProperty("fox.thermal.status", StateName(state));
    android::base::SetProperty("fox.thermal.temp", std::to_string(reading.temperature));
    android::base::SetProperty("fox.thermal.sensor", reading.sensor);
    android::base::SetProperty("fox.thermal.cap_pct", std::to_string(percent));
    Log(std::string("state=") + StateName(state) + " sensor=" + reading.sensor +
        " temp=" + std::to_string(reading.temperature / 1000) + "C cpu_cap=" +
        std::to_string(percent) + "%");
}

void Restore(std::vector<CpuPolicy>* policies) {
    for (auto& policy : *policies) {
        int64_t current;
        if (policy.last_written > 0 &&
            ReadInt(policy.path + "/scaling_max_freq", &current) &&
            current == policy.last_written) {
            WriteInt(policy.path + "/scaling_max_freq", policy.original_max);
        }
        policy.last_written = 0;
    }
    android::base::SetProperty("fox.thermal.status", "stopped");
    android::base::SetProperty("fox.thermal.cap_pct", "100");
    Log("restored daemon-owned CPU limits");
}

State UpdateState(State state, int64_t temperature, int* warm_samples,
                  int* release_samples) {
    if (temperature >= kCriticalEnter) {
        *warm_samples = 0;
        *release_samples = 0;
        return State::kCritical;
    }
    if (state != State::kCritical && temperature >= kHotEnter) {
        *warm_samples = 0;
        *release_samples = 0;
        return State::kHot;
    }
    if ((state == State::kNormal || state == State::kWarm) &&
        temperature >= kElevatedEnter) {
        *warm_samples = 0;
        *release_samples = 0;
        return State::kElevated;
    }

    int64_t release_threshold = 0;
    State released_state = state;
    switch (state) {
        case State::kCritical:
            release_threshold = kCriticalRelease;
            released_state = State::kHot;
            break;
        case State::kHot:
            release_threshold = kHotRelease;
            released_state = State::kElevated;
            break;
        case State::kElevated:
            release_threshold = kElevatedRelease;
            released_state = State::kWarm;
            break;
        case State::kWarm:
            release_threshold = kWarmRelease;
            released_state = State::kNormal;
            break;
        case State::kNormal:
            if (temperature >= kWarmEnter) {
                ++*warm_samples;
                if (*warm_samples >= kWarmSamples) {
                    *warm_samples = 0;
                    return State::kWarm;
                }
            } else {
                *warm_samples = 0;
            }
            return State::kNormal;
    }

    if (temperature < release_threshold) {
        ++*release_samples;
        if (*release_samples >= kReleaseSamples) {
            *release_samples = 0;
            return released_state;
        }
    } else {
        *release_samples = 0;
    }
    return state;
}

}  // namespace

int main() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    auto policies = DiscoverPolicies();
    const auto sensors = DiscoverSensors();
    if (policies.empty() || sensors.empty()) {
        Log("missing usable CPU policies or thermal sensors");
        return 1;
    }

    State state = State::kNormal;
    int warm_samples = 0;
    int release_samples = 0;
    int reapply_samples = 0;
    int64_t peak_temperature = 0;
    std::string peak_sensor;

    android::base::SetProperty("fox.thermal.status", "normal");
    android::base::SetProperty("fox.thermal.cap_pct", "100");
    Log("started with " + std::to_string(policies.size()) + " CPU policies and " +
        std::to_string(sensors.size()) + " thermal sensors");

    while (g_running) {
        const ThermalReading reading = ReadHottest(sensors);
        if (reading.temperature <= 0) {
            Log("all monitored sensors unavailable; stopping fail-open");
            break;
        }

        android::base::SetProperty("fox.thermal.temp", std::to_string(reading.temperature));
        android::base::SetProperty("fox.thermal.sensor", reading.sensor);
        if (reading.temperature > peak_temperature) {
            peak_temperature = reading.temperature;
            peak_sensor = reading.sensor;
            android::base::SetProperty("fox.thermal.peak_temp",
                                       std::to_string(peak_temperature));
            android::base::SetProperty("fox.thermal.peak_sensor", peak_sensor);
        }

        const State next = UpdateState(state, reading.temperature, &warm_samples,
                                       &release_samples);
        ++reapply_samples;
        if (next != state || (state != State::kNormal &&
                              reapply_samples >= kReapplySamples)) {
            state = next;
            ApplyState(&policies, state, reading);
            reapply_samples = 0;
        }

        for (int i = 0; i < kPollInterval.count() && g_running; ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Restore(&policies);
    return 0;
}
