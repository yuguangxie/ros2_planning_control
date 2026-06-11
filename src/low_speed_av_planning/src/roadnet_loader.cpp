#include "low_speed_av_planning/roadnet_loader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

namespace low_speed_av_planning {
namespace {

constexpr std::array<uint32_t, 64> kSha256K{
  0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
  0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
  0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
  0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
  0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
  0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
  0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
  0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

uint32_t rotr(uint32_t value, uint32_t shift)
{
  return (value >> shift) | (value << (32U - shift));
}

std::string sha256_hex(const std::string & input)
{
  std::vector<uint8_t> data(input.begin(), input.end());
  const uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8ULL;
  data.push_back(0x80U);
  while ((data.size() % 64U) != 56U) {
    data.push_back(0U);
  }
  for (int i = 7; i >= 0; --i) {
    data.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xffU));
  }

  std::array<uint32_t, 8> h{
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  for (std::size_t chunk = 0; chunk < data.size(); chunk += 64U) {
    std::array<uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16U; ++i) {
      const std::size_t j = chunk + i * 4U;
      w[i] = (static_cast<uint32_t>(data[j]) << 24U) |
        (static_cast<uint32_t>(data[j + 1U]) << 16U) |
        (static_cast<uint32_t>(data[j + 2U]) << 8U) |
        static_cast<uint32_t>(data[j + 3U]);
    }
    for (std::size_t i = 16U; i < 64U; ++i) {
      const uint32_t s0 = rotr(w[i - 15U], 7U) ^ rotr(w[i - 15U], 18U) ^ (w[i - 15U] >> 3U);
      const uint32_t s1 = rotr(w[i - 2U], 17U) ^ rotr(w[i - 2U], 19U) ^ (w[i - 2U] >> 10U);
      w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
    }
    auto [a, b, c, d, e, f, g, hh] = h;
    for (std::size_t i = 0; i < 64U; ++i) {
      const uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
      const uint32_t ch = (e & f) ^ ((~e) & g);
      const uint32_t temp1 = hh + s1 + ch + kSha256K[i] + w[i];
      const uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
      const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto value : h) {
    out << std::setw(8) << value;
  }
  return out.str();
}

std::string read_file(const fs::path & path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot read " + path.string());
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

double yaml_double(const YAML::Node & n, const char * key, double fallback = 0.0)
{
  return n[key] ? n[key].as<double>() : fallback;
}

std::string yaml_string(const YAML::Node & n, const char * key, const std::string & fallback = "")
{
  const auto value = n[key];
  if (!value || value.IsNull()) {
    return fallback;
  }
  auto text = value.as<std::string>();
  const auto not_space = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
  text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
  auto lower = text;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (text.empty() || lower == "null" || lower == "none") {
    return fallback;
  }
  return text;
}

int yaml_int(const YAML::Node & n, const char * key, int fallback = 0)
{
  return n[key] ? n[key].as<int>() : fallback;
}

bool yaml_bool(const YAML::Node & n, const char * key, bool fallback = false)
{
  return n[key] ? n[key].as<bool>() : fallback;
}

void require_node(const YAML::Node & n, const std::string & label)
{
  if (!n) {
    throw std::runtime_error("missing required field: " + label);
  }
}

void require_finite(double value, const std::string & label)
{
  if (!std::isfinite(value)) {
    throw std::runtime_error("non-finite numeric field: " + label);
  }
}

bool point_in_polygon(const Pose2d & point, const std::vector<Pose2d> & polygon)
{
  if (polygon.size() < 3U) {
    return false;
  }
  bool inside = false;
  for (std::size_t i = 0U, j = polygon.size() - 1U; i < polygon.size(); j = i++) {
    const auto & a = polygon[i];
    const auto & b = polygon[j];
    const bool crosses = ((a.y_m > point.y_m) != (b.y_m > point.y_m)) &&
      (point.x_m < (b.x_m - a.x_m) * (point.y_m - a.y_m) /
      ((b.y_m - a.y_m) == 0.0 ? 1e-12 : (b.y_m - a.y_m)) + a.x_m);
    inside = inside != crosses;
  }
  return inside;
}

bool is_no_go_area(const SemanticArea & area)
{
  return area.type == "no_go_area" || area.type == "keepout" || !area.allow_planning_through;
}

int gear_from_direction(const std::string & direction)
{
  if (direction == "reverse") {
    return 2;
  }
  if (direction == "park") {
    return 3;
  }
  return 1;
}

SemanticPoint parse_semantic_point(const YAML::Node & src, const std::string & fallback_type)
{
  SemanticPoint point;
  point.id = yaml_string(src, "id");
  point.type = yaml_string(src, "type", fallback_type);
  if (src["pose"]) {
    point.pose.x_m = yaml_double(src["pose"], "x");
    point.pose.y_m = yaml_double(src["pose"], "y");
    point.pose.yaw_rad = yaml_double(src["pose"], "yaw");
  }
  point.linked_node_id = yaml_string(src, "linked_node_id");
  point.linked_edge_id = yaml_string(src, "linked_edge_id");
  if (point.linked_edge_id.empty()) {
    point.linked_edge_id = yaml_string(src, "entry_edge_id");
  }
  if (point.linked_edge_id.empty() && src["approach"]) {
    point.linked_edge_id = yaml_string(src["approach"], "edge_id");
  }
  point.linked_path_id = yaml_string(src, "linked_path_id");
  if (point.linked_path_id.empty() && src["properties"]) {
    point.linked_path_id = yaml_string(src["properties"], "linked_path_id");
  }
  if (point.linked_path_id.empty() && src["properties"]) {
    point.linked_path_id = yaml_string(src["properties"], "path_id");
  }
  point.linked_s_m = yaml_double(src, "linked_s_m");
  if (point.linked_s_m == 0.0 && src["properties"]) {
    point.linked_s_m = yaml_double(src["properties"], "s_on_path");
  }
  return point;
}

void load_semantic_points(
  const fs::path & path,
  const std::string & array_key,
  const std::string & fallback_type,
  std::map<std::string, SemanticPoint> & out)
{
  if (!fs::exists(path)) {
    return;
  }
  const YAML::Node doc = YAML::LoadFile(path.string());
  for (const auto & item : doc[array_key]) {
    auto point = parse_semantic_point(item, fallback_type);
    if (!point.id.empty()) {
      out[point.id] = point;
    }
  }
}

}  // namespace

RoadnetPackage RoadnetLoader::load(const fs::path & package_root, const Options & options) const
{
  RoadnetPackage package;
  package.root_path = package_root.string();

  const auto manifest_path = package_root / "project_manifest.json";
  if (!fs::exists(manifest_path)) {
    throw std::runtime_error("missing canonical project_manifest.json");
  }
  const YAML::Node manifest = YAML::LoadFile(manifest_path.string());

  if (yaml_string(manifest, "schema") != "low_speed_roadnet_ad_package") {
    throw std::runtime_error("unsupported AD package schema");
  }
  package.schema_version = yaml_string(manifest, "schema_version");
  if (package.schema_version.rfind("1.1.", 0) != 0 && package.schema_version != "1.1.0") {
    throw std::runtime_error("unsupported schema_version " + package.schema_version);
  }
  package.package_id = yaml_string(manifest, "package_id");
  if (package.package_id.empty()) {
    throw std::runtime_error("manifest package_id is required");
  }
  if (manifest["coordinate_system"]) {
    package.global_frame = yaml_string(manifest["coordinate_system"], "global_frame", "map");
    package.control_reference_frame = yaml_string(
      manifest["coordinate_system"], "control_reference_frame", "rear_axle");
  }
  if (manifest["files"]) {
    for (const auto & item : manifest["files"]) {
      package.files[item.first.as<std::string>()] = item.second.as<std::string>();
    }
  }
  if (manifest["hashes"]) {
    for (const auto & item : manifest["hashes"]) {
      package.manifest_hashes[item.first.as<std::string>()] = item.second.as<std::string>();
    }
  }
  if (manifest["units"]) {
    for (const auto & item : manifest["units"]) {
      package.units[item.first.as<std::string>()] = item.second.as<std::string>();
    }
  }

  if (manifest["validation"]) {
    const auto status = yaml_string(manifest["validation"], "status", "unknown");
    const auto blocking_errors = yaml_int(manifest["validation"], "blocking_errors", 0);
    if (options.reject_failed_validation && (status == "failed" || blocking_errors > 0)) {
      throw std::runtime_error("manifest validation failed or has blocking_errors");
    }
  }

  const auto validation_path = resolve_file(package, "validation_report", "validation/validation_report.json");
  const YAML::Node validation = YAML::LoadFile(validation_path.string());
  const auto report_status = yaml_string(validation, "status", "unknown");
  const auto report_blocking_errors = validation["summary"] ?
    yaml_int(validation["summary"], "blocking_errors", 0) : yaml_int(validation, "blocking_errors", 0);
  if (options.reject_failed_validation && (report_status == "failed" || report_blocking_errors > 0)) {
    throw std::runtime_error("AD package validation report failed or has blocking_errors");
  }

  const auto topology_path = resolve_file(package, "topology", "roadnet/topology.json");
  const auto waypoints_path = resolve_file(package, "waypoints_yaml", "trajectory/waypoints.yaml");
  const auto index_path = resolve_file(package, "waypoint_index", "trajectory/waypoint_index.json");
  for (const auto & required : {topology_path, waypoints_path, index_path}) {
    if (!fs::exists(required)) {
      throw std::runtime_error("missing required package file: " + required.string());
    }
  }

  const YAML::Node topology = YAML::LoadFile(topology_path.string());
  require_node(topology["nodes"], "topology.nodes");
  require_node(topology["edges"], "topology.edges");
  std::set<std::string> node_ids;
  for (const auto & n : topology["nodes"]) {
    TopologyNode node;
    node.id = yaml_string(n, "id");
    if (node.id.empty()) {
      throw std::runtime_error("topology node id is required");
    }
    node.type = yaml_string(n, "type", "normal");
    if (n["pose"]) {
      node.pose.x_m = yaml_double(n["pose"], "x");
      node.pose.y_m = yaml_double(n["pose"], "y");
      node.pose.yaw_rad = yaml_double(n["pose"], "yaw");
    }
    require_finite(node.pose.x_m, "node.pose.x");
    require_finite(node.pose.y_m, "node.pose.y");
    require_finite(node.pose.yaw_rad, "node.pose.yaw");
    node_ids.insert(node.id);
    package.nodes.push_back(node);
  }
  std::set<std::string> edge_ids;
  for (const auto & e : topology["edges"]) {
    TopologyEdge edge;
    edge.id = yaml_string(e, "id");
    edge.from_node_id = yaml_string(e, "from");
    edge.to_node_id = yaml_string(e, "to");
    if (edge.id.empty() || edge.from_node_id.empty() || edge.to_node_id.empty()) {
      throw std::runtime_error("topology edge id/from/to are required");
    }
    if (node_ids.count(edge.from_node_id) == 0 || node_ids.count(edge.to_node_id) == 0) {
      throw std::runtime_error("topology edge references unknown node: " + edge.id);
    }
    edge.direction = yaml_string(e, "direction", "forward");
    edge.length_m = yaml_double(e, "length_m");
    edge.cost = yaml_double(e, "cost", edge.length_m);
    edge.speed_limit_mps = yaml_double(e, "speed_limit_mps", 0.5);
    require_finite(edge.length_m, "edge.length_m");
    require_finite(edge.cost, "edge.cost");
    require_finite(edge.speed_limit_mps, "edge.speed_limit_mps");
    edge.enabled = !(e["availability"] && e["availability"]["enabled"] && !e["availability"]["enabled"].as<bool>());
    edge.blocked_by_default = e["availability"] &&
      yaml_bool(e["availability"], "blocked_by_default", false);
    edge.allow_reverse = e["constraints"] &&
      yaml_bool(e["constraints"], "allow_reverse", false);
    edge_ids.insert(edge.id);
    package.edges.push_back(edge);
  }

  const YAML::Node wp_doc = YAML::LoadFile(waypoints_path.string());
  require_node(wp_doc["waypoints"], "waypoints.waypoints");
  for (const auto & src : wp_doc["waypoints"]) {
    Waypoint wp;
    wp.index = src["global_index"] ? src["global_index"].as<std::size_t>() : package.waypoints.size();
    wp.waypoint_id = yaml_string(src, "waypoint_id");
    wp.edge_id = yaml_string(src, "edge_id");
    wp.path_id = yaml_string(src, "path_id");
    if (wp.edge_id.empty() || edge_ids.count(wp.edge_id) == 0) {
      throw std::runtime_error("waypoint references unknown edge: " + wp.waypoint_id);
    }
    wp.edge_s_m = yaml_double(src, "s_m");
    wp.x_m = yaml_double(src, "x");
    wp.y_m = yaml_double(src, "y");
    wp.yaw_rad = yaml_double(src, "yaw");
    wp.kappa_1pm = yaml_double(src, "kappa");
    wp.target_speed_mps = yaml_double(src, "v_mps", 0.5);
    require_finite(wp.edge_s_m, "waypoint.s_m");
    require_finite(wp.x_m, "waypoint.x");
    require_finite(wp.y_m, "waypoint.y");
    require_finite(wp.yaw_rad, "waypoint.yaw");
    require_finite(wp.kappa_1pm, "waypoint.kappa");
    require_finite(wp.target_speed_mps, "waypoint.v_mps");
    const auto direction = yaml_string(src, "direction", "forward");
    wp.gear = gear_from_direction(direction);
    wp.behavior = yaml_string(src, "behavior", "follow");
    if (src["flags"]) {
      for (const auto & f : src["flags"]) {
        const auto flag = f.as<std::string>();
        wp.edge_start = wp.edge_start || flag == "edge_start";
        wp.edge_end = wp.edge_end || flag == "edge_end";
      }
    }
    package.waypoints.push_back(wp);
  }

  const YAML::Node index = YAML::LoadFile(index_path.string());
  require_node(index["edges"], "waypoint_index.edges");
  for (const auto & item : index["edges"]) {
    const auto edge_id = item.first.as<std::string>();
    if (edge_ids.count(edge_id) == 0) {
      throw std::runtime_error("waypoint_index references unknown edge: " + edge_id);
    }
    WaypointRange range;
    range.start_index = item.second["start_index"].as<std::size_t>();
    if (item.second["end_index_exclusive"]) {
      range.end_index_exclusive = item.second["end_index_exclusive"].as<std::size_t>();
    } else {
      range.end_index_exclusive = item.second["end_index"].as<std::size_t>() + 1;
      range.used_legacy_inclusive_end = true;
      package.warnings.push_back("legacy inclusive end_index used for " + edge_id);
    }
    range.count = item.second["count"] ? item.second["count"].as<std::size_t>() :
      range.end_index_exclusive - range.start_index;
    if (range.start_index >= package.waypoints.size() ||
      range.end_index_exclusive > package.waypoints.size() ||
      range.end_index_exclusive < range.start_index)
    {
      throw std::runtime_error("invalid waypoint range for edge: " + edge_id);
    }
    package.waypoint_index_by_edge[edge_id] = range;
  }

  const auto areas_path = resolve_file(package, "areas", "semantics/areas.json");
  if (fs::exists(areas_path)) {
    const YAML::Node areas_doc = YAML::LoadFile(areas_path.string());
    for (const auto & item : areas_doc["areas"]) {
      SemanticArea area;
      area.id = yaml_string(item, "id");
      area.type = yaml_string(item, "type");
      area.speed_limit_mps = yaml_double(item, "speed_limit_mps", 0.0);
      area.allow_planning_through = yaml_bool(item, "allow_planning_through", true);
      area.priority = yaml_int(item, "priority", 0);
      if (item["polygon"]) {
        for (const auto & vertex : item["polygon"]) {
          Pose2d p;
          p.x_m = yaml_double(vertex, "x");
          p.y_m = yaml_double(vertex, "y");
          require_finite(p.x_m, "semantic_area.polygon.x");
          require_finite(p.y_m, "semantic_area.polygon.y");
          area.polygon.push_back(p);
        }
      }
      if (!area.id.empty()) {
        package.areas.push_back(area);
      }
    }
  }
  load_semantic_points(
    resolve_file(package, "route_points", "semantics/route_points.json"),
    "route_points", "route", package.route_points);
  load_semantic_points(
    resolve_file(package, "task_points", "semantics/task_points.json"),
    "task_points", "task", package.task_points);
  load_semantic_points(
    resolve_file(package, "parking_points", "semantics/parking_points.json"),
    "parking_points", "parking", package.parking_points);
  load_semantic_points(
    resolve_file(package, "charging_points", "semantics/charging_points.json"),
    "charging_points", "charging", package.charging_points);

  for (const auto & area : package.areas) {
    if (!is_no_go_area(area)) {
      continue;
    }
    for (const auto & wp : package.waypoints) {
      if (point_in_polygon({wp.x_m, wp.y_m, wp.yaw_rad}, area.polygon)) {
        package.blocked_edges.insert(wp.edge_id);
      }
    }
  }

  if (options.verify_checksums) {
    verify_checksums(package_root, package);
  }
  return package;
}

fs::path RoadnetLoader::resolve_file(
  const RoadnetPackage & package, const std::string & key, const std::string & canonical_fallback) const
{
  const auto it = package.files.find(key);
  return fs::path(package.root_path) / (it == package.files.end() ? canonical_fallback : it->second);
}

void RoadnetLoader::verify_checksums(const fs::path & root, RoadnetPackage & package) const
{
  std::map<std::string, std::string> expected = package.manifest_hashes;
  const auto checksums_path = root / "checksums.sha256";
  if (fs::exists(checksums_path)) {
    std::istringstream lines(read_file(checksums_path));
    std::string line;
    while (std::getline(lines, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::istringstream parts(line);
      std::string hash;
      std::string rel;
      parts >> hash >> rel;
      if (!hash.empty() && !rel.empty()) {
        const auto existing = expected.find(rel);
        if (existing != expected.end() && existing->second != hash) {
          throw std::runtime_error("checksum source mismatch for " + rel);
        }
        expected[rel] = hash;
      }
    }
  } else if (expected.empty()) {
    package.warnings.push_back("checksums.sha256 missing and manifest.hashes is empty");
    return;
  } else {
    package.warnings.push_back("checksums.sha256 missing; using manifest.hashes");
  }

  for (const auto & item : expected) {
    const auto rel = fs::path(item.first).lexically_normal();
    if (rel.empty() || rel.is_absolute() || item.first.find("..") != std::string::npos) {
      throw std::runtime_error("unsafe checksum path: " + item.first);
    }
    const auto file_path = root / rel;
    if (!fs::exists(file_path)) {
      throw std::runtime_error("checksum references missing file: " + file_path.string());
    }
    const auto actual = sha256_hex(read_file(file_path));
    if (actual != item.second) {
      throw std::runtime_error(
        "checksum mismatch for " + file_path.string() + ": expected " + item.second + " actual " + actual);
    }
  }
}

}  // namespace low_speed_av_planning
