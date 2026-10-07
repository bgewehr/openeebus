#include <gtest/gtest.h>

#include "src/spine/api/entity_local_interface.h"
#include "src/spine/api/feature_local_interface.h"
#include "src/spine/heartbeat/heartbeat_manager.h"
#include "src/spine/model/device_diagnosis_types.h"
#include "tests/src/memory_leak.inc"

static uint64_t last_counter;
static uint32_t last_timeout;
static unsigned int update_count;

static FeatureLocalInterface HeartbeatFeatureMethods() {
  FeatureLocalInterface methods{};
  methods.feature_interface.get_type
      = [](const FeatureObject*) -> FeatureTypeType { return kFeatureTypeTypeDeviceDiagnosis; };
  methods.feature_interface.get_role = [](const FeatureObject*) -> RoleType { return kRoleTypeServer; };
  methods.feature_interface.get_function_operations
      = [](const FeatureObject*, FunctionType) -> const OperationsObject* {
    static const OperationsObject operations{};
    return &operations;
  };
  methods.set_data = [](FeatureLocalObject*, FunctionType, void* data) {
    const auto* heartbeat = static_cast<const DeviceDiagnosisHeartbeatDataType*>(data);
    last_counter          = *heartbeat->heartbeat_counter;
    last_timeout          = heartbeat->heartbeat_timeout->seconds;
    ++update_count;
  };
  return methods;
}

TEST(HeartbeatManagerTest, SendsEarlyWithoutShorteningDeclaredTimeout) {
  const auto methods = HeartbeatFeatureMethods();
  FeatureLocalObject feature{&methods};
  EntityLocalObject entity{};
  update_count  = 0;
  auto* manager = HeartbeatManagerCreate(&entity, 60);
  HEARTBEAT_MANAGER_SET_LOCAL_FEATURE(manager, &entity, &feature);
  EXPECT_EQ(update_count, 1u);
  for (unsigned int tick = 0; tick < 44; ++tick) {
    HEARTBEAT_MANAGER_TICK(manager);
  }
  EXPECT_EQ(last_counter, 0u);
  HEARTBEAT_MANAGER_TICK(manager);
  EXPECT_EQ(last_counter, 1u);
  EXPECT_EQ(last_timeout, 60u);
  for (unsigned int tick = 0; tick < 45; ++tick) {
    HEARTBEAT_MANAGER_TICK(manager);
  }
  EXPECT_EQ(last_counter, 2u);
  EXPECT_EQ(last_timeout, 60u);
  HeartbeatManagerDelete(manager);
  CheckForMemoryLeaks();
}

TEST(HeartbeatManagerTest, ZeroDisablesAndOneSecondRemainsUsable) {
  const auto methods = HeartbeatFeatureMethods();
  FeatureLocalObject feature{&methods};
  EntityLocalObject entity{};
  for (uint32_t period : {0u, 1u}) {
    update_count  = 0;
    auto* manager = HeartbeatManagerCreate(&entity, period);
    HEARTBEAT_MANAGER_SET_LOCAL_FEATURE(manager, &entity, &feature);
    HEARTBEAT_MANAGER_TICK(manager);
    EXPECT_EQ(update_count, period + 1u);
    EXPECT_EQ(last_timeout, period);
    HeartbeatManagerDelete(manager);
  }
  CheckForMemoryLeaks();
}
