/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <fsmod/FileSystem.hpp>
#include <fsmod/FileSystemException.hpp>
#include <fsmod/JBODStorage.hpp>
#include <fsmod/OneDiskStorage.hpp>
#include <simgrid/s4u.hpp>

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

extern "C" void load_platform_cpp(const sg4::Engine& e);
void load_platform_cpp(const sg4::Engine& e)
{
  auto* datacenter = e.get_netzone_root()->add_netzone_full("datacenter");

  auto* pfs        = datacenter->add_netzone_empty("pfs");
  auto* pfs_server = pfs->add_host("pfs_server", "1Gf");

  auto pfs_disk = pfs_server->add_disk("pfs_disk", "180MBps", "160MBps");
  auto pfs_storage = sgfs::JBODStorage::create("pfs_storage", {pfs_disk});
  pfs->seal();

  auto* pub_cluster = datacenter->add_netzone_star("pub_cluster");
  /* create the backbone link */
  const auto* pub_backbone =pub_cluster->add_link("pub_backbone", "10Gbps")->set_latency("1ms");

  std::vector<std::shared_ptr<sgfs::OneDiskStorage>> local_nvmes;
  for (int i = 0; i < 256; i++) {
    std::string hostname = std::string("node-") + std::to_string(i) + ".pub";
    auto* host           = pub_cluster->add_host(hostname, "11Gf")->set_core_count(96);
    auto* nvme           = host->add_disk(hostname + "_nvme", "560MBps", "510MBps");
    local_nvmes.push_back(sgfs::OneDiskStorage::create(hostname + "_local_nvme", nvme));

    auto* link_up        = pub_cluster->add_link(hostname + "_LinkUP", "1Gbps")->set_latency("2ms");
    auto* link_down      = pub_cluster->add_link(hostname + "_LinkDOWN", "1Gbps")->set_latency("2ms");
    auto* loopback       = pub_cluster->add_link(hostname + "_loopback", "1Gbps")->set_latency("1.75ms")
                                      ->set_sharing_policy(sg4::Link::SharingPolicy::FATPIPE);

    pub_cluster->add_route(host, nullptr, {sg4::LinkInRoute(link_up), sg4::LinkInRoute(pub_backbone)}, false);
    pub_cluster->add_route(nullptr, host, {sg4::LinkInRoute(pub_backbone), sg4::LinkInRoute(link_down)}, false);
    pub_cluster->add_route(host, host, {loopback});
  }

  pub_cluster->set_gateway(pub_cluster->add_router("pub_router"));
  pub_cluster->seal();

  auto* sub_cluster = datacenter->add_netzone_star("sub_cluster");
  /* create the backbone link */
  const auto* sub_backbone = sub_cluster->add_link("sub_backbone", "10Gbps")->set_latency("1ms");

  for (int i = 0; i < 128; i++) {
    std::string hostname = std::string("node-") + std::to_string(i) + ".sub";
    auto* host           = sub_cluster->add_host(hostname, "6Gf")->set_core_count(48);

    auto* link_up        = sub_cluster->add_link(hostname + "_LinkUP", "1Gbps")->set_latency("2ms");
    auto* link_down      = sub_cluster->add_link(hostname + "_LinkDOWN", "1Gbps")->set_latency("2ms");
    auto* loopback       = sub_cluster->add_link(hostname + "_loopback", "1Gbps")->set_latency("1.75ms")
                                       ->set_sharing_policy(sg4::Link::SharingPolicy::FATPIPE);

    sub_cluster->add_route(host, nullptr, {sg4::LinkInRoute(link_up), sg4::LinkInRoute(sub_backbone)}, false);
    sub_cluster->add_route(nullptr, host, {sg4::LinkInRoute(sub_backbone), sg4::LinkInRoute(link_down)}, false);
    sub_cluster->add_route(host, host, {loopback});
  }
  sub_cluster->set_gateway(sub_cluster->add_router("sub_router"));
  sub_cluster->seal();

  const auto* inter_cluster_link = datacenter->add_link("inter-cluster", "20Gbps")->set_latency("1ms");
  const auto* pub_pfs_link  = datacenter->add_link("pub-pfs", "20Gbps")->set_latency("1ms");
  const auto* sub_pfs_link  = datacenter->add_link("sub-pfs", "10Gbps")->set_latency("1ms");

  datacenter->add_route(pub_cluster, sub_cluster, {sg4::LinkInRoute(inter_cluster_link)});
  datacenter->add_route(pub_cluster, pfs, {sg4::LinkInRoute(pub_pfs_link)});
  datacenter->add_route(sub_cluster, pfs, {sg4::LinkInRoute(sub_pfs_link)});

  auto remote_fs = sgfs::FileSystem::create("remote_fs", 100000000);
  remote_fs->mount_partition("/pfs/", pfs_storage, "100TB");
  sgfs::FileSystem::register_file_system(pfs, remote_fs);

  auto local_fs = sgfs::FileSystem::create("local_fs", 100000000);
  for (int i = 0; i < 256; i++) {
    std::string partition_name = std::string("/node-") + std::to_string(i) + ".pub/scratch/";
    local_fs->mount_partition(partition_name, local_nvmes.at(i), "1TB");
  }

  sgfs::FileSystem::register_file_system(pub_cluster, local_fs);

  datacenter->seal();
}
