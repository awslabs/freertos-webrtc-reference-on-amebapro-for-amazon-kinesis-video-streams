/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEMO_DATA_TYPES_H
#define DEMO_DATA_TYPES_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "sdp_controller.h"
#include "signaling_controller.h"
#include "peer_connection.h"
#include "app_media_source.h"

#define DEMO_SDP_BUFFER_MAX_LENGTH ( 10000 )
#define DEMO_TRANSCEIVER_MEDIA_INDEX_VIDEO ( 0 )
#define DEMO_TRANSCEIVER_MEDIA_INDEX_AUDIO ( 1 )
#define REMOTE_ID_MAX_LENGTH    ( 256 )

typedef struct DemoPeerConnectionSession
{
    /* The remote client ID, representing the remote peer, from signaling message. */
    char remoteClientId[ REMOTE_ID_MAX_LENGTH ];
    size_t remoteClientIdLength;

    /* Configuration. */
    uint8_t canTrickleIce;

    /* Peer connection session. */
    PeerConnectionSession_t peerConnectionSession;
    Transceiver_t transceivers[ PEER_CONNECTION_TRANSCEIVER_MAX_COUNT ];
} DemoPeerConnectionSession_t;

typedef struct DemoContext
{
    /* Signaling controller. */
    SignalingControllerContext_t signalingControllerContext;

    char sdpConstructedBuffer[ PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH ];
    size_t sdpConstructedBufferLength;

    char sdpBuffer[ PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH ];

    /* Peer Connection. */
    DemoPeerConnectionSession_t peerConnectionSessions[ AWS_MAX_VIEWER_NUM ];
    AppMediaSourcesContext_t appMediaSourcesContext;
} DemoContext_t;

#ifdef __cplusplus
}
#endif

#endif /* DEMO_DATA_TYPES_H */