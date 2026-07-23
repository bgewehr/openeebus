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

#ifndef SRC_EEBUS_SRC_SPINE_EVENTS_EVENTS_H_
#define SRC_EEBUS_SRC_SPINE_EVENTS_EVENTS_H_

#include "src/common/eebus_errors.h"
#include "src/spine/api/events.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Legacy API: subscribe handler for all events (core-level only, device_owner = NULL)
EebusError EventSubscribe(EventHandlerLevel level, EventHandler handler, void* ctx);
EebusError EventUnsubscribe(EventHandlerLevel level, EventHandler handler, void* ctx);

// Instance-scoped API: subscribe handler for specific device instance
// device_owner must be non-NULL for application-level handlers.
// Core-level handlers (device_owner = NULL) always receive all events.
EebusError EventSubscribeWithDeviceOwner(
    EventHandlerLevel level, EventHandler handler, void* ctx, DeviceLocalObject* device_owner);
EebusError EventUnsubscribeWithDeviceOwner(
    EventHandlerLevel level, EventHandler handler, void* ctx, DeviceLocalObject* device_owner);

// Publish event to handlers registered with the owning device instance.
// Core-level handlers receive all events. Application-level handlers only
// receive events for their registered device.
void EventPublish(const EventPayload* payload);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // SRC_EEBUS_SRC_SPINE_EVENTS_EVENTS_H_
