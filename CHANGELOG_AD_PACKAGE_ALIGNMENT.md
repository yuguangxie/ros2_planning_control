# Changelog — AD Package v1.1 Alignment

This update modifies the Codex generation pack so it matches the current roadnet editor ZIP protocol.

## Changed

- Replaced older `manifest.json` assumption with `project_manifest.json`.
- Replaced older `trajectory/waypoints.json` assumption with `trajectory/waypoints.yaml`.
- Replaced root-level `validation_report.json` assumption with `validation/validation_report.json`.
- Updated all prompts to require `schema == low_speed_roadnet_ad_package` and `schema_version == 1.1.0` / `1.1.x` support.
- Added explicit waypoint field mapping: `kappa -> kappa_1pm`, `v_mps -> target_speed_mps`.
- Added requirement to support legacy inclusive `end_index` and preferred `end_index_exclusive`.
- Added configurable localization topic default `/localization/pose` in all docs/configs.
- Added front and dual Ackermann vehicle model requirements.
- Added no-ROS2 Codex validation workflow and phase reports.
- Updated sample AD Package under `templates/sample_ad_package` to include full v1.1 file structure.

## New Files

- `docs/01_ad_package_contract.md`
- `docs/06_ackermann_vehicle_model.md`
- `docs/08_testing_without_ros2.md`
- `docs/11_ad_package_to_algorithm_usage.md`
- `docs/12_implementation_checklists.md`
- `skills/roadnet_ad_package_contract/SKILL.md`
- `skills/testing_without_ros2/SKILL.md`
- `templates/offline_validation/*.py`

## Removed from Canonical Contract

These are no longer canonical:

```text
manifest.json
trajectory/waypoints.json
validation_report.json
```

They may be supported as optional compatibility fallback only if needed.
