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

#ifndef APP_MEDIA_SOURCE_PORT_H
#define APP_MEDIA_SOURCE_PORT_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "transceiver_data_types.h"

typedef struct MediaFrame {
    uint8_t * pData;
    uint32_t size;
    uint64_t timestampUs;
    TransceiverTrackKind_t trackKind;
    uint8_t freeData;  /* indicate user need to free pData after using it */
} MediaFrame_t;

typedef int32_t (* OnFrameReadyToSend_t)( void * pCtx,
                                          MediaFrame_t * pFrame );

int32_t AppMediaSourcePort_Init( void );
int32_t AppMediaSourcePort_Start( OnFrameReadyToSend_t onVideoFrameReadyToSendFunc,
                                  void * pOnVideoFrameReadyToSendCustomContext,
                                  OnFrameReadyToSend_t onAudioFrameReadyToSendFunc,
                                  void * pOnAudioFrameReadyToSendCustomContext );
void AppMediaSourcePort_Stop( void );
void AppMediaSourcePort_Destroy( void );
void AppMediaSourcePort_PlayAudioFrame( MediaFrame_t * pFrame );

/* Return code shared by the ISP helpers below, meaning the video pipeline is
 * not up yet. Callers should retry rather than treat it as a failure: the ISP
 * is unavailable for a few seconds after boot. */
#define APP_MEDIA_SOURCE_PORT_NOT_READY  ( -2 )

/**
 * @brief Reports whether echo cancellation is actually running, and how well.
 *
 * The engine initialises lazily on the first audio frame and its init path
 * reports no status, so "configured" and "running" are not the same thing --
 * a state buffer that failed to allocate leaves AEC silently dead. This is the
 * only way to tell the two apart.
 *
 * @param[out] pRunning 1 if the canceller is active. May be NULL.
 * @param[out] pErleDb  Echo return loss enhancement in dB: how much echo is
 *                      being removed. Higher is better; near zero while
 *                      running means it is not converging. Meaningful only
 *                      during far-end speech. May be NULL.
 *
 * @return 0 on success, APP_MEDIA_SOURCE_PORT_NOT_READY before audio is up,
 *         -1 if AEC is not compiled in.
 */
int32_t AppMediaSourcePort_GetAecStatus( uint8_t * pRunning,
                                         int16_t * pErleDb );

#ifdef __cplusplus
}
#endif

#endif /* APP_MEDIA_SOURCE_PORT_H */
