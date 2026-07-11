#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "chassis_driver/can_ethernet_codec.hpp"
#include "chassis_driver/dbc_protocol.hpp"
#include "chassis_driver/scu_control_frame_builder.hpp"

namespace chassis_driver {
namespace {

TEST(ChassisCodecProduction, StandardFrameRoundTripUsesThirteenByteRecord) {
  CanFrame input;
  input.can_id = 0x121U;
  input.dlc = 8U;
  input.data = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  const auto encoded = CanEthernetCodec::encodeFrame(input);
  EXPECT_EQ(encoded.size(), 13U);
  bool trailing = false;
  std::size_t trailing_count = 0U;
  const std::vector<uint8_t> payload(encoded.begin(), encoded.end());
  const auto decoded =
      CanEthernetCodec::decodePayload(payload, 2U, trailing, trailing_count);
  ASSERT_EQ(decoded.size(), 1U);
  EXPECT_FALSE(trailing);
  EXPECT_EQ(trailing_count, 0U);
  EXPECT_EQ(decoded.front().can_id, input.can_id);
  EXPECT_EQ(decoded.front().data, input.data);
  EXPECT_EQ(decoded.front().channel, 2U);
}

TEST(ChassisCodecProduction,
     ExtendedIdDlcClampAndTrailingBytesAreDeterministic) {
  CanFrame input;
  input.can_id = 0x1ABCDE0U;
  input.is_extended = true;
  input.dlc = 15U;
  const auto encoded = CanEthernetCodec::encodeFrame(input);
  std::vector<uint8_t> payload(encoded.begin(), encoded.end());
  payload.push_back(0xAAU);
  payload.push_back(0xBBU);
  bool trailing = false;
  std::size_t trailing_count = 0U;
  const auto decoded =
      CanEthernetCodec::decodePayload(payload, 1U, trailing, trailing_count);
  ASSERT_EQ(decoded.size(), 1U);
  EXPECT_TRUE(decoded.front().is_extended);
  EXPECT_EQ(decoded.front().can_id, input.can_id);
  EXPECT_EQ(decoded.front().dlc, 8U);
  EXPECT_TRUE(trailing);
  EXPECT_EQ(trailing_count, 2U);
}

TEST(ChassisDbcProduction, IntelSignedAndUnsignedSignalsRoundTripAndClamp) {
  CanFrame bms;
  bms.can_id = 256U;
  bms.dlc = 8U;
  ASSERT_TRUE(DbcProtocol::encodeSignal(bms, "BMS_Voltage", 55.5));
  ASSERT_TRUE(DbcProtocol::encodeSignal(bms, "BMS_Current", -12.3));
  EXPECT_NEAR(*DbcProtocol::decodeSignal(bms, "BMS_Voltage"), 55.5, 0.05);
  EXPECT_NEAR(*DbcProtocol::decodeSignal(bms, "BMS_Current"), -12.3, 0.05);
  ASSERT_TRUE(DbcProtocol::encodeSignal(bms, "BMS_SOC", 999.0));
  EXPECT_DOUBLE_EQ(*DbcProtocol::decodeSignal(bms, "BMS_SOC"), 100.0);
  EXPECT_FALSE(DbcProtocol::encodeSignal(bms, "missing_signal", 1.0));
}

TEST(ChassisDbcProduction, RawIntelAndMotorolaBitHelpersRoundTrip) {
  std::array<uint8_t, 8> intel{};
  DbcProtocol::insertIntel(intel, 0U, 8U, 0xA5U);
  EXPECT_EQ(DbcProtocol::extractIntel(intel, 0U, 8U), 0xA5U);
  std::array<uint8_t, 8> motorola{};
  DbcProtocol::insertMotorola(motorola, 7U, 8U, 0xA5U);
  EXPECT_EQ(DbcProtocol::extractMotorola(motorola, 7U, 8U), 0xA5U);
  EXPECT_EQ(DbcProtocol::signExtend(0xFFU, 8U), -1);
}

class FakeFrameSink {
public:
  void send(const CanFrame &frame) { frames.push_back(frame); }
  std::vector<CanFrame> frames;
};

TEST(ChassisScuFrameProduction,
     LegalCommandBuildsExact121SignalsAndFakeSinkReceivesFrame) {
  ScuControlFrameInput input;
  input.shift_level = 1U;
  input.front_steering_angle_deg = 13.5;
  input.rear_steering_angle_deg = -13.5;
  input.target_speed_kmh = 4.2;
  input.brake_enable = true;
  input.left_turn_light_request = 2U;
  input.torque_or_speed_mode = 1U;
  input.steering_angle_speed_valid = true;
  input.brake_force_command_valid = true;
  const auto result = build_scu_control_frame(input, {27.0, 15.0});
  ASSERT_TRUE(result.valid);
  EXPECT_EQ(result.frame.can_id, 0x121U);
  EXPECT_EQ(result.frame.dlc, 8U);
  EXPECT_DOUBLE_EQ(
      *DbcProtocol::decodeSignal(result.frame, "SCU_Shift_Level_Request"), 1.0);
  EXPECT_DOUBLE_EQ(
      *DbcProtocol::decodeSignal(result.frame, "SCU_Steering_Angle_Front"),
      60.0);
  EXPECT_DOUBLE_EQ(
      *DbcProtocol::decodeSignal(result.frame, "SCU_Steering_Angle_Rear"),
      196.0);
  EXPECT_NEAR(*DbcProtocol::decodeSignal(result.frame, "SCU_Target_Speed"), 4.2,
              0.05);
  EXPECT_DOUBLE_EQ(*DbcProtocol::decodeSignal(result.frame, "SCU_Brake_Enable"),
                   1.0);
  EXPECT_DOUBLE_EQ(
      *DbcProtocol::decodeSignal(result.frame, "Steering_Angle_Speed_Valid"),
      1.0);
  FakeFrameSink sink;
  sink.send(result.frame);
  ASSERT_EQ(sink.frames.size(), 1U);
  EXPECT_EQ(sink.frames.front().data, result.frame.data);
}

TEST(ChassisScuFrameProduction, InvalidShiftIsRejectedWithoutFrame) {
  ScuControlFrameInput input;
  input.shift_level = 0U;
  const auto result = build_scu_control_frame(input, ScuControlFrameLimits{});
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.rejection_reason, "invalid_shift");
}

TEST(ChassisScuFrameProduction,
     NonFiniteAndOutOfRangeValuesMapToZeroAsCurrentContract) {
  ScuControlFrameInput input;
  input.shift_level = 1U;
  input.front_steering_angle_deg = std::numeric_limits<double>::quiet_NaN();
  input.rear_steering_angle_deg = 100.0;
  input.target_speed_kmh = std::numeric_limits<double>::infinity();
  const auto result = build_scu_control_frame(input, {27.0, 15.0});
  ASSERT_TRUE(result.valid);
  EXPECT_EQ(result.warnings.size(), 3U);
  EXPECT_DOUBLE_EQ(
      *DbcProtocol::decodeSignal(result.frame, "SCU_Steering_Angle_Front"),
      0.0);
  EXPECT_DOUBLE_EQ(
      *DbcProtocol::decodeSignal(result.frame, "SCU_Steering_Angle_Rear"), 0.0);
  EXPECT_DOUBLE_EQ(*DbcProtocol::decodeSignal(result.frame, "SCU_Target_Speed"),
                   0.0);
}

TEST(ChassisWatchdogKnownGap, StartupStop) {
  GTEST_SKIP() << "SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002 startup stop is "
                  "not implemented";
}

TEST(ChassisWatchdogKnownGap, TimeoutStop) {
  GTEST_SKIP() << "SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002 timeout stop is "
                  "not implemented";
}

TEST(ChassisWatchdogKnownGap, InvalidCommandDoesNotReplayOldMotion) {
  GTEST_SKIP() << "SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002 command cache is "
                  "not implemented";
}

TEST(ChassisWatchdogKnownGap, ShutdownStopAndDiagnostics) {
  GTEST_SKIP() << "SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002 shutdown stop and "
                  "diagnostics are not implemented";
}

} // namespace
} // namespace chassis_driver
