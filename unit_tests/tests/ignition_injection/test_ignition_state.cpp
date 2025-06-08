//
// Created by FDSoftware on 10/04/25.
//

#include "pch.h"
#include "ignition_state.h"

using ::testing::_;

TEST(ignition_state, getRunningAdvanceSimple) {
  EngineTestHelper eth(engine_type_e::TEST_ENGINE);
  const float rpm = 4500;
  const float load = 50;

  setWholeTimingTable(10);
  initIgnitionAdvanceControl();

  // invalid load, should return NAN
  auto correction = getRunningAdvance(rpm, NAN);
  EXPECT_TRUE(std::isnan(correction));

  // valid load, all secondary conditions off == 10°
  Sensor::setMockValue(SensorType::Clt, 35);
  Sensor::setMockValue(SensorType::VehicleSpeed, 50);
  Sensor::setMockValue(SensorType::WheelSlipRatio, 0);

  // coverage on: test_ignition_angle_advance.cpp
  engineConfiguration->launchControlEnabled = false;
  // to cover: (on test_antilag.cpp)
  engineConfiguration->antiLagEnabled = false;
  // coverage on: test_shift_torque_reduction_spark_skip_ratio.cpp
  engineConfiguration->torqueReductionEnabled = false;
  // coverage on: test_nitrous_ignition_retard.cpp
  engineConfiguration->nitrousControlEnabled = false;
  // to cover (this same file)
  engineConfiguration->useSeparateAdvanceForIdle = false;

  correction = getRunningAdvance(rpm, load);
  EXPECT_NEAR(10, getRunningAdvance(rpm, load), EPS2D);
}

struct MockIdle : public MockIdleController {
  bool isIdling = true;

  bool isIdlingOrTaper() const override {
    return isIdling;
  }
};

TEST(ignition_state, getRunningAdvanceIdleTable) {
  EngineTestHelper eth(engine_type_e::TEST_ENGINE);
  const float rpm = 900;
  const float load = 30;
  setWholeTimingTable(10);
  setArrayValues(config->idleAdvance, 5);

  MockIdle idler;
  engine->engineModules.get<IdleController>().set(&idler);

  Sensor::setMockValue(SensorType::DriverThrottleIntent, 0);

  engineConfiguration->useSeparateAdvanceForIdle = true;
  engineConfiguration->idlePidDeactivationTpsThreshold = 2;

  // no TPS === running advance
  EXPECT_NEAR(10, getRunningAdvance(rpm, load), EPS2D);

  idler.isIdling = true;
  EXPECT_TRUE(engine->module<IdleController>()->isIdlingOrTaper());

  // 0% of idle threshold -> idle table
  Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.001);
  EXPECT_NEAR(5, getRunningAdvance(rpm, load), EPS2D);

  // 50% of idle threshold -> idle table
  Sensor::setMockValue(SensorType::DriverThrottleIntent, 1);
  EXPECT_NEAR(5, getRunningAdvance(rpm, load), EPS2D);

  // 75% of idle threshold -> idle table
  Sensor::setMockValue(SensorType::DriverThrottleIntent, 1.5);
  EXPECT_NEAR(7.5, getRunningAdvance(rpm, load), EPS2D);

  // idle threshold -> normal table
  Sensor::setMockValue(SensorType::DriverThrottleIntent, 2);
  EXPECT_NEAR(10, getRunningAdvance(rpm, load), EPS2D);
}

TEST(ignition_state, getRunningAdvanceTractionDrop) {
  EngineTestHelper eth(engine_type_e::TEST_ENGINE);
  engineConfiguration->torqueReductionEnabled = true;
  const float rpm = 4500;
  const float load = 50;

  setWholeTimingTable(10);
  initIgnitionAdvanceControl();

  Sensor::setMockValue(SensorType::Clt, 35);
  setTable(engineConfiguration->tractionControlTimingDrop, 0);
  engineConfiguration->tractionControlTimingDrop[0][0] = -15;
  engineConfiguration->tractionControlTimingDrop[0][1] = -15;

  size_t lastYIndex = TRACTION_CONTROL_ETB_DROP_SLIP_SIZE - 1;
  size_t lastXIndex = TRACTION_CONTROL_ETB_DROP_SPEED_SIZE - 1;

  engineConfiguration->tractionControlTimingDrop[lastYIndex - 1][lastXIndex - 1] = 15;
  engineConfiguration->tractionControlTimingDrop[lastYIndex][lastXIndex] = 15;

  // test correct X/Y on table
  // we expect here that the first values are -5 (10° base - 15° from traction table),
  // and the last on the rigth side of the table are 25 (10° base + 15° from traction table)

  Sensor::setMockValue(SensorType::VehicleSpeed, 10.0);
  Sensor::setMockValue(SensorType::WheelSlipRatio, 0);
  EXPECT_NEAR(-5, getRunningAdvance(rpm, load), EPS2D);

  Sensor::setMockValue(SensorType::VehicleSpeed, 120.0);
  Sensor::setMockValue(SensorType::WheelSlipRatio, 1.2);
  EXPECT_NEAR(25, getRunningAdvance(rpm, load), EPS2D);
}

TEST(ignition_state, getRunningAdvanceTractionSparkSkip) {
  EngineTestHelper eth(engine_type_e::TEST_ENGINE);
  engineConfiguration->torqueReductionEnabled = true;
  const float rpm = 4500;
  const float load = 50;

  setWholeTimingTable(10);
  initIgnitionAdvanceControl();

  // invalid load, should return NAN
  auto correction = getRunningAdvance(rpm, NAN);
  EXPECT_TRUE(std::isnan(correction));

  // valid load, all secondary conditions off == 10°
  Sensor::setMockValue(SensorType::Clt, 35);
  setTable(engineConfiguration->tractionControlIgnitionSkip, 5);
  engineConfiguration->tractionControlIgnitionSkip[0][0] = 0;
  engineConfiguration->tractionControlIgnitionSkip[0][1] = 0;

  size_t lastYIndex = TRACTION_CONTROL_ETB_DROP_SLIP_SIZE - 1;
  size_t lastXIndex = TRACTION_CONTROL_ETB_DROP_SPEED_SIZE - 1;

  engineConfiguration->tractionControlIgnitionSkip[lastYIndex - 1][lastXIndex - 1] = 50;
  engineConfiguration->tractionControlIgnitionSkip[lastYIndex][lastXIndex] = 50;

  // test correct X/Y on table
  // we expect here that the first values are 0, and the last on the rigth side of the table are 50
  Sensor::setMockValue(SensorType::VehicleSpeed, 10.0);
  Sensor::setMockValue(SensorType::WheelSlipRatio, 0);
  getRunningAdvance(rpm, load);
  EXPECT_NEAR(0, engine->engineState.tractionControlSparkSkip, EPS2D);

  Sensor::setMockValue(SensorType::VehicleSpeed, 120.0);
  Sensor::setMockValue(SensorType::WheelSlipRatio, 1.2);
  getRunningAdvance(rpm, load);
  EXPECT_NEAR(50, engine->engineState.tractionControlSparkSkip, EPS2D);
}