/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

/**
 * @file platform_summary.cpp
 * @brief Utility to display a human-readable summary of a SimGrid platform.
 *
 * This tool loads a platform from an XML file, shared library (.so), or
 * C++ platform description and displays a comprehensive summary of zones,
 * hosts, and disks.
 *
 * Usage: platform_summary <platform_file> [simgrid-options]
 *
 * Supported formats:
 *   - .xml  : SimGrid XML platform file
 *   - .so   : Shared library with load_platform() function
 *   - .cpp  : (requires compilation) C++ platform description
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <fsmod/FileSystem.hpp>
#include <simgrid/s4u.hpp>

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

void print_zone_tree(sg4::NetZone* zone, const std::string& indent = "") {
  int host_count = 0;
  for (auto* host : zone->get_all_hosts()) {
    if (host->get_englobing_zone() == zone) {
      host_count++;
    }
  }

  std::cout << indent << zone->get_name();
  if (host_count > 0) {
    std::cout << " (" << host_count << " hosts)";
  }
  std::cout << "\n";

  for (auto* child : zone->get_children()) {
    print_zone_tree(child, indent + "  ");
  }
}

void print_host_summary(sg4::NetZone* zone) {
  std::map<std::string, std::vector<sg4::Host*>> hosts_by_zone;

  std::function<void(sg4::NetZone*)> collect = [&](sg4::NetZone* z) {
    for (auto* host : z->get_all_hosts()) {
      if (host->get_englobing_zone() == z) {
        hosts_by_zone[z->get_name()].push_back(host);
      }
    }
    for (auto* child : z->get_children()) {
      collect(child);
    }
  };
  collect(zone);

  for (const auto& [zone_name, zone_hosts] : hosts_by_zone) {
    if (zone_hosts.empty()) continue;

    if (zone_hosts.size() <= 3) {
      for (const auto* h : zone_hosts) {
        std::cout << "  " << h->get_name() << " [" << zone_name << "] "
                  << h->get_speed() / 1e9 << " Gf, " << h->get_core_count() << " cores";
        auto disks = h->get_disks();
        if (!disks.empty()) {
          std::cout << ", " << disks.size() << " disk(s)";
        }
        std::cout << "\n";
      }
    } else {
      // Aggregate similar hosts
      std::map<std::tuple<double, int, size_t>, int> host_types;
      for (const auto* h : zone_hosts) {
        host_types[{h->get_speed(), h->get_core_count(), h->get_disks().size()}]++;
      }
      std::cout << "  [" << zone_name << "] " << zone_hosts.size() << " hosts:\n";
      for (const auto& [key, count] : host_types) {
        auto [speed, cores, disk_count] = key;
        std::cout << "    " << count << "x: " << speed / 1e9 << " Gf, "
                  << cores << " cores, " << disk_count << " disk(s)\n";
      }
    }
  }
}

void print_disk_summary(sg4::NetZone* zone) {
  std::map<std::tuple<double, double>, int> disk_types;

  std::function<void(sg4::NetZone*)> collect = [&](sg4::NetZone* z) {
    for (auto* host : z->get_all_hosts()) {
      if (host->get_englobing_zone() == z) {
        for (auto* disk : host->get_disks()) {
          disk_types[{disk->get_read_bandwidth(), disk->get_write_bandwidth()}]++;
        }
      }
    }
    for (auto* child : z->get_children()) {
      collect(child);
    }
  };
  collect(zone);

  for (const auto& [key, count] : disk_types) {
    auto [rbw, wbw] = key;
    std::cout << "  " << count << "x: read=" << rbw / 1e6 << " MBps, write=" << wbw / 1e6 << " MBps\n";
  }
}

void print_usage(const char* prog_name) {
  std::cerr << "Usage: " << prog_name << " <platform_file> [simgrid-options]\n\n"
            << "Display a human-readable summary of a SimGrid platform.\n\n"
            << "Supported formats:\n"
            << "  .xml  : SimGrid XML platform file\n"
            << "  .so   : Shared library with load_platform() function\n\n"
            << "Examples:\n"
            << "  " << prog_name << " platform.xml\n"
            << "  " << prog_name << " libplatform.so\n";
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  std::string platform_file = argv[1];

  // Check if help is requested
  if (platform_file == "-h" || platform_file == "--help") {
    print_usage(argv[0]);
    return 0;
  }

  sg4::Engine e(&argc, argv);
  e.load_platform(platform_file);

  auto* root = e.get_netzone_root();

  // Count totals
  int total_hosts = 0;
  int total_disks = 0;
  std::function<void(sg4::NetZone*)> count = [&](sg4::NetZone* z) {
    for (auto* host : z->get_all_hosts()) {
      if (host->get_englobing_zone() == z) {
        total_hosts++;
        total_disks += host->get_disks().size();
      }
    }
    for (auto* child : z->get_children()) {
      count(child);
    }
  };
  count(root);

  std::cout << "\n=== PLATFORM SUMMARY ===\n\n";

  std::cout << "ZONE HIERARCHY:\n";
  print_zone_tree(root);

  std::cout << "\nHOSTS (" << total_hosts << "):\n";
  print_host_summary(root);

  std::cout << "\nDISKS (" << total_disks << "):\n";
  print_disk_summary(root);

  std::cout << "\n";

  return 0;
}
