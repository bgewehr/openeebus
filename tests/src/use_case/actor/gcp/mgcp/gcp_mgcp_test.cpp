/*
 * Copyright 2025 NIBE AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @brief Currently it is not a regular unit test but more a "sand box"
 * to feed the SPINE Device with specific datagrams and check the outgoing messages printed.
 * @note Remember to enable the message printing in PrintMessage() before getting started
 */

#include "src/use_case/actor/gcp/mgcp/gcp_mgcp.h"

#include <gtest/gtest.h>

#include <memory>

#include "mocks/common/eebus_timer/eebus_timer_mock.h"
#include "mocks/ship/ship_connection/data_writer_mock.h"
#include "src/common/array_util.h"
#include "src/common/eebus_malloc.h"
#include "src/common/eebus_timer/eebus_timer.h"
#include "src/common/message_buffer.h"
#include "src/spine/device/device_local.h"
#include "src/spine/device/device_local_internal.h"
#include "src/spine/entity/entity_local.h"
#include "tests/src/json.h"
#include "tests/src/use_case/actor/gcp/mgcp/receive/device_configuration_description_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/device_configuration_key_value_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/device_configuration_subscription_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/discovery_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/discovery_response.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/electrical_connection_parameter_description_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/electrical_connection_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/electrical_connection_subscription_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/measurement_constraints_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/measurement_description_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/measurement_subscription_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/node_management_subscription_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/result_data_msg_cnt_ref_3.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/use_case_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/receive/use_case_request.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/device_configuration_description_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/device_configuration_key_value_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/discovery_read.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/discovery_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/electrical_connection_description_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/electrical_connection_parameter_description_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/measurement_constraints_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/measurement_description_reply.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/node_management_subscription_call.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/result_data_msg_cnt_ref_11.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/result_data_msg_cnt_ref_3.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/result_data_msg_cnt_ref_5.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/result_data_msg_cnt_ref_8.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/use_case_data_read.inc"
#include "tests/src/use_case/actor/gcp/mgcp/send/use_case_data_reply.inc"
#include "tests/src/use_case/use_case_test_fixture.h"

using testing::_;
using testing::Return;

namespace gcp_mgcp_test {

class GcpMgcpTestFixture : public UseCaseTestFixture {
 public:
  GcpMgcpTestFixture() : UseCaseTestFixture("GridConnectionPoint", "GridConnectionPoint", "123456789") {};
  void SetUpUseCase() override {
    uint32_t entity_ids[1]{static_cast<uint32_t>(VectorGetSize(DEVICE_LOCAL_GET_ENTITIES(device_local_.get())))};

    EntityLocalObject* const entity = EntityLocalCreate(
        device_local_.get(),
        kEntityTypeTypeGridConnectionPointOfPremises,
        entity_ids,
        ARRAY_SIZE(entity_ids),
        kHeartbeatTimeout
    );

    static constexpr GcpMgcpMeasurementConfig measurement_default_cfg = {
        .value_source = kMeasurementValueSourceTypeMeasuredValue,
    };

    static constexpr GcpMgcpMonitorEnergyConfig energy_cfg{
        .energy_feed_in_cfg  = &measurement_default_cfg,
        .energy_consumed_cfg = &measurement_default_cfg,
    };

    static constexpr GcpMgcpMonitorCurrentConfig current_cfg = {
        .current_phase_a_cfg = &measurement_default_cfg,
    };

    static constexpr GcpMgcpMonitorFrequencyConfig frequency_cfg = {
        .frequency_cfg = measurement_default_cfg,
    };

    static constexpr GcpMgcpPvCurtailmentConfig pv_curtailment_cfg = {};

    static constexpr GcpMgcpConfig cfg{
        .pv_curtailment_cfg = &pv_curtailment_cfg,

        .power_cfg = {
            .phases          = kElectricalConnectionPhaseNameTypeAbc,
            .power_total_cfg = measurement_default_cfg,
        },

        .energy_cfg    = &energy_cfg,
        .current_cfg   = &current_cfg,
        .voltage_cfg   = NULL,
        .frequency_cfg = &frequency_cfg,
    };

    use_case_.reset(GcpMgcpUseCaseCreate(entity, 0, &cfg));

    static constexpr ScaledValue power_total = {1000, 0};
    GcpMgcpSetMeasurementDataCache(use_case_.get(), kGcpPowerTotal, &power_total, NULL, NULL);

    static constexpr ScaledValue energy_feed_in = {200, 0};
    static constexpr EebusDateTime start_time   = {
          .date = {.year = 2025, .month = 9, .day = 1},
          .time = {   .hour = 0,   .min = 0, .sec = 0}
    };

    static constexpr EebusDateTime end_time = {
        .date = {.year = 2025, .month = 10, .day = 2},
        .time = {   .hour = 0,    .min = 0, .sec = 0}
    };

    GcpMgcpSetEnergyFeedInCache(use_case_.get(), &energy_feed_in, NULL, NULL, &start_time, &end_time);

    static constexpr ScaledValue energy_consumed = {800, 0};
    GcpMgcpSetEnergyConsumedCache(use_case_.get(), &energy_consumed, NULL, NULL, &start_time, &end_time);

    static constexpr ScaledValue current_phase_a = {15, -1};
    GcpMgcpSetMeasurementDataCache(use_case_.get(), kGcpCurrentPhaseA, &current_phase_a, NULL, NULL);

    static constexpr ScaledValue frequency = {50, 0};
    GcpMgcpSetMeasurementDataCache(use_case_.get(), kGcpFrequency, &frequency, NULL, NULL);

    static constexpr ScaledValue pv_curtailment_limit_factor = {75, 0};
    GcpMgcpSetPvCurtailmentLimitFactor(use_case_.get(), &pv_curtailment_limit_factor);

    GcpMgcpUpdate(use_case_.get());

    DEVICE_LOCAL_ADD_ENTITY(device_local_.get(), entity);

    ExpectSendMessage(send::discovery_read);
  }

  void TearDownUseCase() override {
    use_case_.reset();
  }

 protected:
  std::unique_ptr<GcpMgcpUseCaseObject, decltype(&GcpMgcpUseCaseDelete)> use_case_{nullptr, GcpMgcpUseCaseDelete};
};

TEST_F(GcpMgcpTestFixture, GcpMgcpTest) {
  // 1. Check that values assigned during SetUpUseCase are correctly stored
  ScaledValue value{0};
  EXPECT_EQ(GcpMgcpGetMeasurementData(use_case_.get(), kGcpPowerTotal, &value), kEebusErrorOk);
  EXPECT_EQ(value.value, 1000);
  EXPECT_EQ(value.scale, 0);

  EXPECT_EQ(GcpMgcpGetMeasurementData(use_case_.get(), kGcpCurrentPhaseA, &value), kEebusErrorOk);
  EXPECT_EQ(value.value, 15);
  EXPECT_EQ(value.scale, -1);

  EXPECT_EQ(GcpMgcpGetMeasurementData(use_case_.get(), kGcpFrequency, &value), kEebusErrorOk);
  EXPECT_EQ(value.value, 50);
  EXPECT_EQ(value.scale, 0);

  // 2. Receive the detailed discovery request and send the response
  ExpectSendMessage(send::discovery_reply);
  HandleMessage(receive::discovery_request);

  // 3. Receive the detailed discovery response and send subscriptions + use case read
  ExpectSendMessage(send::node_management_subscription_call);
  ExpectSendMessage(send::use_case_data_read);
  HandleMessage(receive::discovery_response);

  // 4. Receive the Node Management subscription request and send result
  ExpectSendMessage(send::result_data_msg_cnt_ref_3);
  HandleMessage(receive::node_management_subscription_request);

  // 5. Receive the use case discovery request and send the reply
  ExpectSendMessage(send::use_case_data_reply);
  HandleMessage(receive::use_case_request);

  // 6. Receive the electrical connection subscription request and send result
  ExpectSendMessage(send::result_data_msg_cnt_ref_5);
  HandleMessage(receive::electrical_connection_subscription_request);

  // 7. Receive the electrical connection read request and send the response
  ExpectSendMessage(send::electrical_connection_description_reply);
  HandleMessage(receive::electrical_connection_request);

  // 8. Receive the electrical connection parameter description request and send the response
  ExpectSendMessage(send::electrical_connection_parameter_description_reply);
  HandleMessage(receive::electrical_connection_parameter_description_request);

  // 9. Receive the measurement subscription request and send result
  ExpectSendMessage(send::result_data_msg_cnt_ref_8);
  HandleMessage(receive::measurement_subscription_request);

  // 10. Receive the measurement description request and send the response
  ExpectSendMessage(send::measurement_description_reply);
  HandleMessage(receive::measurement_description_request);

  // 11. Receive the measurement constraints request and send the response
  ExpectSendMessage(send::measurement_constraints_reply);
  HandleMessage(receive::measurement_constraints_request);

  // 12. Receive the device configuration subscription request and send result
  ExpectSendMessage(send::result_data_msg_cnt_ref_11);
  HandleMessage(receive::device_configuration_subscription_request);

  // 13. Receive the device configuration description request and send the response
  ExpectSendMessage(send::device_configuration_description_reply);
  HandleMessage(receive::device_configuration_description_request);

  // 14. Receive the device configuration key value request and send the response
  ExpectSendMessage(send::device_configuration_key_value_reply);
  HandleMessage(receive::device_configuration_key_value_request);

  // 15. Receive the result with message counter reference 3
  HandleMessage(receive::result_data_msg_cnt_ref_3);

  // 16. Receive the Use Case reply
  HandleMessage(receive::use_case_reply);
}

}  // namespace gcp_mgcp_test
