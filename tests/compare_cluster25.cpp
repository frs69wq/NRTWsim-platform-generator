/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <fsmod/FileSystem.hpp>
#include <simgrid/s4u.hpp>

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

// Canonical representation of a platform for comparison
struct PlatformFingerprint {
  std::map<std::string, int> zone_host_counts;
  std::map<std::tuple<std::string, double, int, size_t>, int> host_groups;
  std::map<std::tuple<double, double>, int> disk_groups;

  void collect(const sg4::Engine& e) {
    collect_zone(e.get_netzone_root());
  }

  void collect_zone(sg4::NetZone* zone) {
    int host_count = 0;
    for (auto* host : zone->get_all_hosts()) {
      if (host->get_englobing_zone() == zone) {
        host_count++;
        host_groups[{zone->get_name(), host->get_speed(), host->get_core_count(), host->get_disks().size()}]++;
        for (auto* disk : host->get_disks()) {
          disk_groups[{disk->get_read_bandwidth(), disk->get_write_bandwidth()}]++;
        }
      }
    }
    zone_host_counts[zone->get_name()] = host_count;

    for (auto* child : zone->get_children()) {
      collect_zone(child);
    }
  }

  std::string serialize() const {
    std::ostringstream oss;
    for (const auto& [name, count] : zone_host_counts) {
      oss << "Z:" << name << ":" << count << "\n";
    }
    for (const auto& [key, count] : host_groups) {
      auto [zone, speed, cores, disks] = key;
      oss << "H:" << zone << ":" << speed << ":" << cores << ":" << disks << ":" << count << "\n";
    }
    for (const auto& [key, count] : disk_groups) {
      auto [rbw, wbw] = key;
      oss << "D:" << rbw << ":" << wbw << ":" << count << "\n";
    }
    return oss.str();
  }
};

// Platform loaders
extern "C" void load_platform(const sg4::Engine& e);       // JSON-based loader
extern "C" void load_platform_cpp(const sg4::Engine& e);   // Original C++ loader

void run_json_test(int argc, char** argv) {
  sg4::Engine e(&argc, argv);
  load_platform(e);
  PlatformFingerprint fp;
  fp.collect(e);
  std::cout << fp.serialize();
}

void run_cpp_test(int argc, char** argv) {
  sg4::Engine e(&argc, argv);
  load_platform_cpp(e);
  PlatformFingerprint fp;
  fp.collect(e);
  std::cout << fp.serialize();
}

int main(int argc, char** argv)
{
  // Check for subcommand mode
  if (argc >= 2) {
    if (strcmp(argv[1], "--json") == 0) {
      run_json_test(argc - 1, argv + 1);
      return 0;
    }
    if (strcmp(argv[1], "--cpp") == 0) {
      run_cpp_test(argc - 1, argv + 1);
      return 0;
    }
  }

  // Main comparison mode: run both as subprocesses
  std::cout << "=== Platform Comparison Test: cluster25 ===\n\n";

  // Get the path to this executable
  std::string exe_path = argv[0];

  // Run JSON version
  std::cout << "Loading JSON-generated platform...\n";
  FILE* json_pipe = popen((exe_path + " --json 2>/dev/null").c_str(), "r");
  if (!json_pipe) {
    std::cerr << "Failed to run JSON platform test\n";
    return 1;
  }
  std::string json_output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), json_pipe)) {
    json_output += buffer;
  }
  int json_status = pclose(json_pipe);
  if (json_status != 0) {
    std::cerr << "JSON platform test failed with status " << json_status << "\n";
    return 1;
  }

  // Run C++ version
  std::cout << "Loading original C++ platform...\n";
  FILE* cpp_pipe = popen((exe_path + " --cpp 2>/dev/null").c_str(), "r");
  if (!cpp_pipe) {
    std::cerr << "Failed to run C++ platform test\n";
    return 1;
  }
  std::string cpp_output;
  while (fgets(buffer, sizeof(buffer), cpp_pipe)) {
    cpp_output += buffer;
  }
  int cpp_status = pclose(cpp_pipe);
  if (cpp_status != 0) {
    std::cerr << "C++ platform test failed with status " << cpp_status << "\n";
    return 1;
  }

  std::cout << "\n";

  // Compare
  if (json_output == cpp_output) {
    std::cout << "Result: PASS - Platforms are equivalent\n\n";

    // Parse and display summary
    int zones = 0, hosts = 0, disks = 0;
    std::istringstream iss(json_output);
    std::string line;
    while (std::getline(iss, line)) {
      if (line[0] == 'Z') zones++;
      else if (line[0] == 'H') {
        // Extract count from end
        auto pos = line.rfind(':');
        hosts += std::stoi(line.substr(pos + 1));
      }
      else if (line[0] == 'D') {
        auto pos = line.rfind(':');
        disks += std::stoi(line.substr(pos + 1));
      }
    }
    std::cout << "  Zones: " << zones << "\n";
    std::cout << "  Hosts: " << hosts << "\n";
    std::cout << "  Disks: " << disks << "\n";
    return 0;
  } else {
    std::cout << "Result: FAIL - Platforms differ\n\n";
    std::cout << "JSON output:\n" << json_output << "\n";
    std::cout << "C++ output:\n" << cpp_output << "\n";
    return 1;
  }
}
