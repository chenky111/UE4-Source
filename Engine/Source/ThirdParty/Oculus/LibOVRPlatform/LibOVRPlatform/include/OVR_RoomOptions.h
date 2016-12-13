// This file was @generated with LibOVRPlatform/codegen/main. Do not modify it!

#ifndef OVR_ROOM_OPTIONS_H
#define OVR_ROOM_OPTIONS_H

#include "OVR_Platform_Defs.h"
#include <stddef.h>
#include <stdbool.h>
#include <OVR_Types.h>

#include "OVR_UserOrdering.h"

struct ovrRoomOptions;
typedef struct ovrRoomOptions* ovrRoomOptionsHandle;

OVRP_PUBLIC_FUNCTION(ovrRoomOptionsHandle) ovr_RoomOptions_Create();
OVRP_PUBLIC_FUNCTION(void) ovr_RoomOptions_Destroy(ovrRoomOptionsHandle handle);
OVRP_PUBLIC_FUNCTION(void) ovr_RoomOptions_SetOrdering(ovrRoomOptionsHandle handle, ovrUserOrdering value);
OVRP_PUBLIC_FUNCTION(void) ovr_RoomOptions_SetRoomId(ovrRoomOptionsHandle handle, ovrID value);

#endif
