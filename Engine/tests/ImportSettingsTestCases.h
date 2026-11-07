#pragma once
#include <string_view>

namespace ImportSettingsTestCases
{

// 1. Valid modern v1 schema with all fields
inline constexpr std::string_view kValidV1 =
R"({
  "version": 1,
  "kind": "model",
  "model": {
    "scale": 0.05,
    "useFileUnitScale": true,
    "normals": "smooth",
    "smoothingAngle": 45.0,
    "importMaterials": false,
    "importTextures": false,
    "flipUv": true,
    "mergeMeshes": true
  }
})";

// 2. Legacy S16 format {"scale": 0.01}
inline constexpr std::string_view kLegacyScale = R"({"scale": 0.01})";

// 3. Legacy S16 format with extra unknown fields {"scale": 0.01, "foo": 1}
inline constexpr std::string_view kLegacyWithUnknown = R"({"scale": 0.01, "foo": 1})";

// 4. Modern v1 with unknown fields in root and in model
inline constexpr std::string_view kValidWithUnknownFields =
R"({
  "version": 1,
  "kind": "model",
  "custom_root": 12345,
  "model": {
    "scale": 2.0,
    "custom_model_meta": "keep_me"
  }
})";

// 5. Broken JSON syntax
inline constexpr std::string_view kBrokenJson = "not a valid json { broken";

// 6. Missing required 'kind' field in v1
inline constexpr std::string_view kMissingKind = R"({"version": 1, "model": {"scale": 1.0}})";

// 7. Missing required 'model' object in v1
inline constexpr std::string_view kMissingModel = R"({"version": 1, "kind": "model"})";

// 8. Missing required 'scale' in model
inline constexpr std::string_view kMissingScale = R"({"version": 1, "kind": "model", "model": {}})";

// 9. Unsupported kind
inline constexpr std::string_view kUnsupportedKind = R"({"version": 1, "kind": "audio", "model": {"scale": 1.0}})";

// 10. Bad types
inline constexpr std::string_view kBadTypeScale = R"({"version": 1, "kind": "model", "model": {"scale": "big"}})";
inline constexpr std::string_view kBadTypeNormals = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "normals": 123}})";
inline constexpr std::string_view kBadTypeSmoothingAngle = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "smoothingAngle": "sharp"}})";
inline constexpr std::string_view kBadTypeImportMaterials = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "importMaterials": "yes"}})";
inline constexpr std::string_view kBadTypeUseFileUnitScale = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "useFileUnitScale": 42}})";
inline constexpr std::string_view kBadTypeFlipUv = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "flipUv": "false"}})";
inline constexpr std::string_view kBadTypeMergeMeshes = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "mergeMeshes": []}})";

// 11. Out of range values
inline constexpr std::string_view kOutOfRangeScaleZero = R"({"version": 1, "kind": "model", "model": {"scale": 0.0}})";
inline constexpr std::string_view kOutOfRangeScaleNegative = R"({"version": 1, "kind": "model", "model": {"scale": -5.0}})";
inline constexpr std::string_view kOutOfRangeScaleHuge = R"({"version": 1, "kind": "model", "model": {"scale": 100000.0}})";
inline constexpr std::string_view kOutOfRangeAngleNegative = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "smoothingAngle": -10.0}})";
inline constexpr std::string_view kOutOfRangeAngleHuge = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "smoothingAngle": 180.0}})";
inline constexpr std::string_view kInvalidNormalsEnum = R"({"version": 1, "kind": "model", "model": {"scale": 1.0, "normals": "invalid_enum"}})";

// 12. Legacy scale out of range / bad type
inline constexpr std::string_view kLegacyBadScale = R"({"scale": "bad"})";
inline constexpr std::string_view kLegacyOutOfRangeScale = R"({"scale": -1.0})";

} // namespace ImportSettingsTestCases

