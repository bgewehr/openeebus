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

#include "src/spine/events/events.h"

#include "src/common/eebus_malloc.h"
#include "src/common/vector.h"

typedef struct EventHandlerInfo EventHandlerInfo;

struct EventHandlerInfo {
  EventHandlerLevel level;
  EventHandler handler;
  void* ctx;
  // The device instance this handler is registered with. NULL means
  // handler is registered for all events (core-level handlers only).
  DeviceLocalObject* device_owner;
};

static Vector handlers = {0};

EventHandlerInfo* EventHandlerInfoCreate(
    EventHandlerLevel level, EventHandler handler, void* ctx, DeviceLocalObject* device_owner) {
  EventHandlerInfo* info = EEBUS_MALLOC(sizeof(*info));
  if (info == NULL) {
    return NULL;
  }

  info->level         = level;
  info->handler       = handler;
  info->ctx           = ctx;
  info->device_owner  = device_owner;
  return info;
}

void EventHandlerInfoDelete(EventHandlerInfo* info) { EEBUS_FREE(info); }

const EventHandlerInfo* EventHandlerFind(
    EventHandlerLevel level, EventHandler handler, void* ctx, DeviceLocalObject* device_owner) {
  for (size_t i = 0; i < VectorGetSize(&handlers); ++i) {
    EventHandlerInfo* info = VectorGetElement(&handlers, i);
    if ((info->level == level) && (info->handler == handler) && (info->ctx == ctx)
        && (info->device_owner == device_owner)) {
      return info;
    }
  }

  return NULL;
}

EebusError EventSubscribe(EventHandlerLevel level, EventHandler handler, void* ctx) {
  return EventSubscribeWithDeviceOwner(level, handler, ctx, NULL);
}

EebusError EventSubscribeWithDeviceOwner(
    EventHandlerLevel level, EventHandler handler, void* ctx, DeviceLocalObject* device_owner) {
  if (EventHandlerFind(level, handler, ctx, device_owner) != NULL) {
    return kEebusErrorOk;
  }

  EventHandlerInfo* const new_handler_info
      = EventHandlerInfoCreate(level, handler, ctx, device_owner);
  if (new_handler_info == NULL) {
    return kEebusErrorMemoryAllocate;
  }

  VectorPushBack(&handlers, new_handler_info);
  return kEebusErrorOk;
}

EebusError EventUnsubscribe(EventHandlerLevel level, EventHandler handler, void* ctx) {
  return EventUnsubscribeWithDeviceOwner(level, handler, ctx, NULL);
}

EebusError EventUnsubscribeWithDeviceOwner(
    EventHandlerLevel level, EventHandler handler, void* ctx, DeviceLocalObject* device_owner) {
  const EventHandlerInfo* const info = EventHandlerFind(level, handler, ctx, device_owner);
  if (info == NULL) {
    return kEebusErrorNoChange;
  }

  VectorRemove(&handlers, (void*)info);
  EventHandlerInfoDelete((void*)info);
  if (VectorGetSize(&handlers) == 0) {
    // Required to pass the memory check test as vector can keep the allocated buffer
    VectorClear(&handlers);
  }

  return kEebusErrorOk;
}

void EventPublish(const EventPayload* payload) {
  if (payload == NULL) {
    return;
  }

  // Route events to handlers registered with the owning device instance.
  // Core-level handlers (device_owner == NULL) receive all events regardless
  // of device ownership, for bootstrapping and system-wide logic.
  // Application-level handlers (device_owner != NULL) only receive events
  // from their registered device, implementing instance isolation.
  for (size_t i = 0; i < VectorGetSize(&handlers); ++i) {
    EventHandlerInfo* info = VectorGetElement(&handlers, i);

    // Core-level handlers always receive events
    if (info->level == kEventHandlerLevelCore) {
      info->handler(payload, info->ctx);
      continue;
    }

    // Application-level handlers only receive events for their device
    if (info->device_owner == payload->device_owner) {
      info->handler(payload, info->ctx);
    }
  }
}
