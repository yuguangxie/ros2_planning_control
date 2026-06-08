#pragma once

#include <filesystem>
#include <string>

#include "low_speed_av_planning/roadnet_types.hpp"

namespace low_speed_av_planning {

class RoadnetLoader {
public:
  struct Options {
    // Reject packages whose manifest/report validation is failed or blocking.
    bool reject_failed_validation{true};
    // Verify checksums when the package provides hashes/checksums.
    bool verify_checksums{true};
  };

  // Load a Low Speed Roadnet AD Package v1.1 directory from disk.
  // This method intentionally uses project_manifest.json as the entry point.
  RoadnetPackage load(const std::filesystem::path & package_root, const Options & options) const;

private:
  std::filesystem::path resolve_file(
    const RoadnetPackage & package,
    const std::string & key,
    const std::string & canonical_fallback) const;
  void verify_checksums(const std::filesystem::path & root, RoadnetPackage & package) const;
};

}  // namespace low_speed_av_planning
