#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "low_speed_av_planning/roadnet_loader.hpp"

namespace fs = std::filesystem;
namespace low_speed_av_planning {
namespace {

class TemporaryPackage {
public:
  TemporaryPackage() {
    static unsigned counter = 0U;
    root_ = fs::temp_directory_path() /
            ("low_speed_av_loader_test_" + std::to_string(++counter));
    fs::remove_all(root_);
    fs::copy(fs::path(LOW_SPEED_AV_TEST_REPO_ROOT) /
                 "src/low_speed_av_bringup/sample_ad_package",
             root_, fs::copy_options::recursive);
  }

  ~TemporaryPackage() { fs::remove_all(root_); }
  const fs::path &root() const { return root_; }

  void replace(const fs::path &relative, const std::string &from,
               const std::string &to) {
    const auto path = root_ / relative;
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    auto text = buffer.str();
    const auto position = text.find(from);
    if (position == std::string::npos) {
      throw std::runtime_error("fixture token not found: " + from);
    }
    text.replace(position, from.size(), to);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
  }

  void append(const fs::path &relative, const std::string &text) {
    std::ofstream output(root_ / relative, std::ios::binary | std::ios::app);
    output << text;
  }

private:
  fs::path root_;
};

RoadnetLoader::Options without_hashes() {
  RoadnetLoader::Options options;
  options.verify_checksums = false;
  return options;
}

TEST(RoadnetLoaderProduction, LoadsCanonicalSampleAndExclusiveIndex) {
  const auto root = fs::path(LOW_SPEED_AV_TEST_REPO_ROOT) /
                    "src/low_speed_av_bringup/sample_ad_package";
  const auto package = RoadnetLoader().load(root, RoadnetLoader::Options{});
  EXPECT_EQ(package.schema_version, "1.1.0");
  EXPECT_EQ(package.nodes.size(), 3U);
  EXPECT_EQ(package.edges.size(), 2U);
  EXPECT_EQ(package.waypoints.size(), 6U);
  ASSERT_TRUE(package.waypoint_index_by_edge.count("E_L001_F"));
  EXPECT_FALSE(
      package.waypoint_index_by_edge.at("E_L001_F").used_legacy_inclusive_end);
}

TEST(RoadnetLoaderProduction, AcceptsCompatiblePatchVersion) {
  TemporaryPackage fixture;
  fixture.replace("project_manifest.json", "\"schema_version\": \"1.1.0\"",
                  "\"schema_version\": \"1.1.9\"");
  EXPECT_NO_THROW(RoadnetLoader().load(fixture.root(), without_hashes()));
}

TEST(RoadnetLoaderProduction, RejectsWrongSchema) {
  TemporaryPackage fixture;
  fixture.replace("project_manifest.json", "low_speed_roadnet_ad_package",
                  "unsupported_package");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), without_hashes()),
               std::runtime_error);
}

TEST(RoadnetLoaderProduction, RejectsManifestValidationFailure) {
  TemporaryPackage fixture;
  fixture.replace("project_manifest.json", "\"status\": \"passed\"",
                  "\"status\": \"failed\"");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), without_hashes()),
               std::runtime_error);
}

TEST(RoadnetLoaderProduction, RejectsManifestBlockingErrors) {
  TemporaryPackage fixture;
  fixture.replace("project_manifest.json", "\"blocking_errors\": 0",
                  "\"blocking_errors\": 1");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), without_hashes()),
               std::runtime_error);
}

TEST(RoadnetLoaderProduction, RejectsValidationReportFailure) {
  TemporaryPackage fixture;
  fixture.replace("validation/validation_report.json", "\"status\": \"passed\"",
                  "\"status\": \"failed\"");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), without_hashes()),
               std::runtime_error);
}

TEST(RoadnetLoaderProduction, RejectsChecksumMismatch) {
  TemporaryPackage fixture;
  fixture.append("trajectory/waypoints.yaml", "\n# deterministic tamper\n");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), RoadnetLoader::Options{}),
               std::runtime_error);
}

TEST(RoadnetLoaderProduction, RejectsConflictingChecksumSources) {
  TemporaryPackage fixture;
  fixture.replace(
      "checksums.sha256",
      "7fa86b97e3557035e1b825a43c6b25cadac9264284f0eb5fe3732c6a28606732",
      "0000000000000000000000000000000000000000000000000000000000000000");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), RoadnetLoader::Options{}),
               std::runtime_error);
}

TEST(RoadnetLoaderProduction, SupportsLegacyInclusiveEndIndex) {
  TemporaryPackage fixture;
  fixture.replace("trajectory/waypoint_index.json",
                  "\"end_index_exclusive\": 3", "\"end_index\": 2");
  fixture.replace("trajectory/waypoint_index.json",
                  "\"end_index_exclusive\": 6", "\"end_index\": 5");
  const auto package = RoadnetLoader().load(fixture.root(), without_hashes());
  EXPECT_TRUE(
      package.waypoint_index_by_edge.at("E_L001_F").used_legacy_inclusive_end);
  EXPECT_EQ(package.waypoint_index_by_edge.at("E_L001_F").end_index_exclusive,
            3U);
}

TEST(RoadnetLoaderProduction, RejectsTamperedWaypointIndexRange) {
  TemporaryPackage fixture;
  fixture.replace("trajectory/waypoint_index.json",
                  "\"end_index_exclusive\": 3", "\"end_index_exclusive\": 999");
  EXPECT_THROW(RoadnetLoader().load(fixture.root(), without_hashes()),
               std::runtime_error);
}

} // namespace
} // namespace low_speed_av_planning
