# JSON Platform Loader for SimGrid

[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL_v2.1-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)
[![Build-Linux](https://github.com/frs69wq/NRTWsim-platform-generator/actions/workflows/build.yml/badge.svg)](https://github.com/frs69wq/NRTWsim-platform-generator/actions/workflows/build.yml)

A shared library that loads SimGrid platforms from JSON configuration files, enabling declarative platform definitions without recompilation.

## Overview

This project provides a `libplatform.so` shared library that implements the SimGrid `load_platform()` function. Instead of hard-coding platform topology in C++, you define your platform in a JSON configuration file. The library reads this configuration at runtime and constructs the corresponding SimGrid platform with zones, hosts, links, storage systems, and filesystems.

## Features

- **Declarative Configuration**: Define platforms in JSON without writing C++ code
- **Runtime Flexibility**: Change platform topology without recompilation
- **Full SimGrid Support**: Zones, hosts, links, routes, disks, and filesystems
- **FSMod Integration**: Built-in support for JBOD and OneDisk storage systems

## Building

### Prerequisites

- CMake 3.16+
- C++17 compiler
- SimGrid 4.x
- FSMod (SimGrid filesystem module)
- nlohmann/json

### Build Commands

```bash
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
ctest
```

## Usage

### With a SimGrid Simulator

To use the JSON platform loader with your SimGrid simulator:

```bash
# Set the configuration file path
export PLATFORM_CONFIG=/path/to/your/platform_config.json

# Run your simulator with the platform library
./your_simulator --cfg=platf:/path/to/libplatform.so
```

Or use the platform library directly in code:

```cpp
#include <simgrid/s4u.hpp>

int main(int argc, char** argv) {
    simgrid::s4u::Engine e(&argc, argv);

    // Load platform from the shared library
    e.load_platform("/path/to/libplatform.so");

    // Your simulation code here...

    e.run();
    return 0;
}
```

### Configuration File Location

The library searches for the configuration file in this order:

1. `PLATFORM_CONFIG` environment variable
2. `platform_config.json` in the same directory as `libplatform.so`
3. `platform_config.json` in the current working directory

### Platform Summary Utility

A helper utility is provided to display a summary of any SimGrid platform:

```bash
./platform_summary <platform_file>
```

Supported formats:

- `.xml` : SimGrid XML platform file
- `.so` : Shared library with `load_platform()` function

Example:

```bash
./platform_summary libplatform.so
```

## JSON Configuration Format

### Top-Level Structure

```json
{
  "facilities": [...],
  "filesystems": [...]
}
```

### Facilities

A facility represents a top-level network zone (e.g., a datacenter). Facilities always use Full routing.

```json
{
  "name": "datacenter",
  "storage_systems": [...],
  "clusters": [...],
  "links": [...],
  "routes": [...]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Unique identifier for the facility |
| `storage_systems` | array | Storage system definitions |
| `clusters` | array | Compute cluster definitions |
| `links` | array | Inter-zone link definitions |
| `routes` | array | Route definitions between zones |

### Storage Systems

Storage systems define shared storage resources (e.g., parallel filesystems):

```json
{
  "name": "pfs",
  "server_speed": "1Gf",
  "type": "JBOD",
  "disk_count": 1,
  "read_bandwidth": "180MBps",
  "write_bandwidth": "160MBps"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Storage system name (generates `{name}_server`, `{name}_storage`, `{name}_disk`) |
| `server_speed` | string | Compute speed of the storage server |
| `type` | string | Storage type: `"JBOD"` or `"OneDisk"` |
| `disk_count` | integer | Number of disks in the storage system |
| `read_bandwidth` | string | Disk read bandwidth |
| `write_bandwidth` | string | Disk write bandwidth |

### Clusters

Clusters define groups of compute nodes with star topology:

```json
{
  "name": "compute_cluster",
  "prefix": "node-",
  "suffix": ".cluster",
  "count": 256,
  "node": {
    "speed": "11Gf",
    "cores": 96,
    "private_link": {
      "bandwidth": "1Gbps",
      "latency": "2ms",
      "sharing_policy": "SPLITDUPLEX"
    },
    "loopback": {
      "bandwidth": "1Gbps",
      "latency": "1.75ms"
    },
    "storage": {
      "name": "local_nvme",
      "read_bandwidth": "560MBps",
      "write_bandwidth": "510MBps"
    }
  },
  "backbone": {
    "bandwidth": "10Gbps",
    "latency": "1ms"
  }
}
```

Node-local storage is always of type `OneDisk`.

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Cluster zone name |
| `prefix` | string | Hostname prefix |
| `suffix` | string | Hostname suffix |
| `count` | integer | Number of nodes |
| `node` | object | Node configuration (see below) |
| `backbone.bandwidth` | string | Backbone link bandwidth |
| `backbone.latency` | string | Backbone link latency (optional, defaults to "0s") |

**Node Configuration:**

| Field | Type | Description |
|-------|------|-------------|
| `speed` | string | Compute speed per node |
| `cores` | integer | Number of cores per node |
| `private_link.bandwidth` | string | Node-to-backbone link bandwidth |
| `private_link.latency` | string | Node-to-backbone link latency (optional, defaults to "0s") |
| `private_link.sharing_policy` | string | Sharing policy (optional) |
| `loopback.bandwidth` | string | Loopback link bandwidth |
| `loopback.latency` | string | Loopback link latency (optional, defaults to "0s") |
| `storage` | object | Optional local storage per node |

Host names are generated as: `{prefix}{index}{suffix}` (e.g., `node-0.cluster`, `node-1.cluster`, ...)

### Links

Inter-zone links connect different zones within a facility:

```json
{
  "name": "inter-cluster",
  "bandwidth": "20Gbps",
  "latency": "1ms"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Link name (used in route definitions) |
| `bandwidth` | string | Link bandwidth |
| `latency` | string | Link latency (optional, defaults to "0s") |

### Routes

Routes define the path between zones using links:

```json
{
  "src": "compute_cluster",
  "dst": "pfs",
  "links": ["cluster-to-pfs"]
}
```

### Filesystems

Filesystems define mount points backed by storage systems or cluster-local storage:

**Remote Filesystem (on a storage system):**

```json
{
  "name": "remote_fs",
  "storage_system": "pfs",
  "mount_point": "/pfs/",
  "size": "100TB"
}
```

**Local Filesystem (per-node in a cluster):**

```json
{
  "name": "local_fs",
  "cluster": "compute_cluster",
  "mount_point": "/{hostname}/scratch/",
  "size": "1TB"
}
```

The `{hostname}` placeholder is replaced with each node's hostname, creating per-node partitions.

## Complete Example

See [platform_config.json](platform_config.json) for a complete example configuration.

```json
{
  "facilities": [
    {
      "name": "datacenter",
      "storage_systems": [
        {
          "name": "pfs",
          "server_speed": "1Gf",
          "type": "JBOD",
          "disk_count": 1,
          "read_bandwidth": "180MBps",
          "write_bandwidth": "160MBps"
        }
      ],
      "clusters": [
        {
          "name": "pub_cluster",
          "prefix": "node-",
          "suffix": ".pub",
          "count": 256,
          "node": {
            "speed": "11Gf",
            "cores": 96,
            "private_link": {
              "bandwidth": "1Gbps",
              "latency": "2ms",
              "sharing_policy": "SPLITDUPLEX"
            },
            "loopback": {
              "bandwidth": "1Gbps",
              "latency": "1.75ms"
            },
            "storage": {
              "name": "local_nvme",
              "read_bandwidth": "560MBps",
              "write_bandwidth": "510MBps"
            }
          },
          "backbone": {
            "bandwidth": "10Gbps",
            "latency": "1ms"
          }
        }
      ],
      "links": [
        {
          "name": "pub-pfs",
          "bandwidth": "20Gbps",
          "latency": "1ms"
        }
      ],
      "routes": [
        {
          "src": "pub_cluster",
          "dst": "pfs",
          "links": ["pub-pfs"]
        }
      ]
    }
  ],
  "filesystems": [
    {
      "name": "remote_fs",
      "storage_system": "pfs",
      "mount_point": "/pfs/",
      "size": "100TB"
    },
    {
      "name": "local_fs",
      "cluster": "pub_cluster",
      "mount_point": "/{hostname}/scratch/",
      "size": "1TB"
    }
  ]
}
```

## Project Structure

```
.
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── LICENSE                  # GNU LGPL v2.1
├── .clang-format            # Code formatting rules
├── .gitignore               # Git ignore patterns
├── json_platform_loader.cpp # Main library source
├── platform_config.json     # Default configuration file
├── platform_summary.cpp     # Platform display utility
├── cmake/                   # CMake find modules
│   ├── FindSimGrid.cmake
│   └── FindFSMod.cmake
├── tests/
│   ├── compare_cluster25.cpp   # Comparison test
│   ├── platform_cluster25.cpp  # Reference C++ platform
│   └── platform_cluster25.json # Matching JSON config
└── .github/
    └── workflows/
        └── build.yml        # CI workflow
```

## License

This project is licensed under the **GNU Lesser General Public License v2.1** (LGPL-2.1).

You are free to use, modify, and distribute this software under the terms of the LGPL-2.1 license. See the [LICENSE](LICENSE) file for details.

Copyright (c) 2026. The SWAT Team. All rights reserved.
