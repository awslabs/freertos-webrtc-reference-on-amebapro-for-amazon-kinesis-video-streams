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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "sys_api.h"      /* sys_backtrace_enable() */
#include "sntp/sntp.h"    /* SNTP series APIs */
#include "wifi_conf.h"    /* WiFi series APIs */
#include "lwip_netconf.h" /* LwIP_GetIP() */
#include "srtp.h"

#include "demo_config.h"
#include "app_common.h"
#include "app_media_source.h"
#include "logging.h"

#define DEMO_MASTER_CLIENT_ID "ProduceMaster"
#define DEMO_MASTER_CLIENT_ID_LENGTH ( 13 )

AppContext_t appContext;
AppMediaSourcesContext_t appMediaSourceContext;

static void Viewer_Task( void * pParameter );

static int32_t InitTransceiver( void * pMediaCtx,
                                TransceiverTrackKind_t trackKind,
                                Transceiver_t * pTranceiver );
static int32_t OnMediaSinkHook( void * pCustom,
                                MediaFrame_t * pFrame );
static int32_t InitializeAppMediaSource( AppContext_t * pAppContext,
                                         AppMediaSourcesContext_t * pAppMediaSourceContext );

static int32_t InitTransceiver( void * pMediaCtx,
                                TransceiverTrackKind_t trackKind,
                                Transceiver_t * pTranceiver )
{
    int32_t ret = 0;
    AppMediaSourcesContext_t * pMediaSourceContext = ( AppMediaSourcesContext_t * )pMediaCtx;

    if( ( pMediaCtx == NULL ) || ( pTranceiver == NULL ) )
    {
        LogError( ( "Invalid input, pMediaCtx: %p, pTranceiver: %p", pMediaCtx, pTranceiver ) );
        ret = -1;
    }
    else if( ( trackKind != TRANSCEIVER_TRACK_KIND_VIDEO ) &&
             ( trackKind != TRANSCEIVER_TRACK_KIND_AUDIO ) )
    {
        LogError( ( "Invalid track kind: %d", trackKind ) );
        ret = -2;
    }
    else
    {
        /* Empty else marker. */
    }

    if( ret == 0 )
    {
        switch( trackKind )
        {
            case TRANSCEIVER_TRACK_KIND_VIDEO:
                ret = AppMediaSource_InitVideoTransceiver( pMediaSourceContext,
                                                           pTranceiver );
                break;
            case TRANSCEIVER_TRACK_KIND_AUDIO:
                ret = AppMediaSource_InitAudioTransceiver( pMediaSourceContext,
                                                           pTranceiver );
                break;
            default:
                LogError( ( "Invalid track kind: %d", trackKind ) );
                ret = -3;
                break;
        }
    }

    return ret;
}

static int32_t OnMediaSinkHook( void * pCustom,
                                MediaFrame_t * pFrame )
{
    int32_t ret = 0;
    AppContext_t * pAppContext = ( AppContext_t * ) pCustom;
    PeerConnectionResult_t peerConnectionResult;
    Transceiver_t * pTransceiver = NULL;
    PeerConnectionFrame_t peerConnectionFrame;
    int i;

    if( ( pAppContext == NULL ) || ( pFrame == NULL ) )
    {
        LogError( ( "Invalid input, pCustom: %p, pFrame: %p", pCustom, pFrame ) );
        ret = -1;
    }

    if( ret == 0 )
    {
        peerConnectionFrame.version = PEER_CONNECTION_FRAME_CURRENT_VERSION;
        peerConnectionFrame.presentationUs = pFrame->timestampUs;
        peerConnectionFrame.pData = pFrame->pData;
        peerConnectionFrame.dataLength = pFrame->size;

        for( i = 0; i < AWS_MAX_VIEWER_NUM; i++ )
        {
            if( pFrame->trackKind == TRANSCEIVER_TRACK_KIND_VIDEO )
            {
                pTransceiver = &pAppContext->appSessions[ i ].transceivers[ DEMO_TRANSCEIVER_MEDIA_INDEX_VIDEO ];
            }
            else if( pFrame->trackKind == TRANSCEIVER_TRACK_KIND_AUDIO )
            {
                pTransceiver = &pAppContext->appSessions[ i ].transceivers[ DEMO_TRANSCEIVER_MEDIA_INDEX_AUDIO ];
            }
            else
            {
                /* Unknown kind, skip that. */
                LogWarn( ( "Unknown track kind: %d", pFrame->trackKind ) );
                break;
            }

            if( pAppContext->appSessions[ i ].peerConnectionSession.state == PEER_CONNECTION_SESSION_STATE_CONNECTION_READY )
            {
                peerConnectionResult = PeerConnection_WriteFrame( &pAppContext->appSessions[ i ].peerConnectionSession,
                                                                  pTransceiver,
                                                                  &peerConnectionFrame );

                if( peerConnectionResult != PEER_CONNECTION_RESULT_OK )
                {
                    LogError( ( "Fail to write %s frame, result: %d", ( pFrame->trackKind == TRANSCEIVER_TRACK_KIND_VIDEO ) ? "video" : "audio",
                                peerConnectionResult ) );
                    ret = -3;
                }
            }
        }
    }

    return ret;
}

static int32_t InitializeAppMediaSource( AppContext_t * pAppContext,
                                         AppMediaSourcesContext_t * pAppMediaSourceContext )
{
    int32_t ret = 0;

    if( ( pAppContext == NULL ) ||
        ( pAppMediaSourceContext == NULL ) )
    {
        LogError( ( "Invalid input, pAppContext: %p, pAppMediaSourceContext: %p", pAppContext, pAppMediaSourceContext ) );
        ret = -1;
    }

    if( ret == 0 )
    {
        ret = AppMediaSource_Init( pAppMediaSourceContext,
                                   OnMediaSinkHook,
                                   pAppContext );
    }

    return ret;
}

static void Viewer_Task( void * pParameter )
{
    int32_t ret = 0;
    int clientIdLength = 0;
    uint32_t randomClientIdPostfix = 0;

    ( void ) pParameter;

    LogInfo( ( "Start Viewer_Task." ) );

    ret = AppCommon_Init( &appContext, InitTransceiver, &appMediaSourceContext );

    if( ret == 0 )
    {
        ret = InitializeAppMediaSource( &appContext, &appMediaSourceContext );
    }

    if( ret == 0 )
    {
        /* Configure signaling controller with client ID and role type. */
        randomClientIdPostfix = rand() & 0xFFFFFFFFU;
        clientIdLength = snprintf( &( appContext.signalingControllerClientId[ 0 ] ),
                                   sizeof( appContext.signalingControllerClientId ),
                                   "%s%lu",
                                   SIGNALING_CONTROLLER_VIEWER_CLIENT_ID_PREFIX,
                                   randomClientIdPostfix );
        appContext.signalingControllerClientIdLength = clientIdLength;
        appContext.signalingControllerRole = SIGNALING_ROLE_VIEWER;

        if( clientIdLength < 0 )
        {
            LogError( ( "snprintf return failure, errno: %d", errno ) );
            ret = -1;
        }
    }

    if( ret == 0 )
    {
        /* Launch application with current thread serving as Signaling Controller. */
        ret = AppCommon_StartSignalingController( &appContext );
    }

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

void app_example( void )
{
    int ret = 0;

    if( ret == 0 )
    {
        #ifdef BUILD_INFO
        LogInfo( ( "\r\nBuild Info: %s\r\n", BUILD_INFO ) );
        #endif
    }

    if( ret == 0 )
    {
        if( xTaskCreate( Viewer_Task,
                         ( ( const char * ) "ViewerTask" ),
                         4096,
                         NULL,
                         tskIDLE_PRIORITY + 4,
                         NULL ) != pdPASS )
        {
            LogError( ( "xTaskCreate(Viewer_Task) failed" ) );
            ret = -1;
        }
    }
}

// static int32_t CreateSdpOffer( AppContext_t * pAppContext )
// {
//     int32_t ret = 0;
//     uint8_t skipProcess = 0;
//     SignalingControllerResult_t signalingControllerReturn;
//     PeerConnectionResult_t peerConnectionResult;
//     PeerConnectionBufferSessionDescription_t bufferSessionDescription;
//     size_t sdpOfferMessageLength = 0;
//     AppSession_t * pAppSession = NULL;
//     SignalingControllerEventMessage_t eventMessage = {
//         .event = SIGNALING_CONTROLLER_EVENT_SEND_WSS_MESSAGE,
//         .onCompleteCallback = NULL,
//         .pOnCompleteCallbackContext = NULL,
//     };

//     if( skipProcess == 0 )
//     {
//         pAppSession = GetCreatePeerConnectionSession( pAppContext, DEMO_MASTER_CLIENT_ID, DEMO_MASTER_CLIENT_ID_LENGTH, 1U );
//         if( pAppSession == NULL )
//         {
//             LogWarn( ( "No available peer connection session for remote client ID(%u): %.*s",
//                        DEMO_MASTER_CLIENT_ID_LENGTH,
//                        ( int ) DEMO_MASTER_CLIENT_ID_LENGTH,
//                        DEMO_MASTER_CLIENT_ID ) );
//             skipProcess = 1;
//         }
//     }

//     if( skipProcess == 0 )
//     {
//         memset( &bufferSessionDescription, 0, sizeof( PeerConnectionBufferSessionDescription_t ) );
//         bufferSessionDescription.pSdpBuffer = pAppContext->sdpBuffer;
//         bufferSessionDescription.sdpBufferLength = PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH;
//         peerConnectionResult = PeerConnection_SetLocalDescription( &pAppSession->peerConnectionSession,
//                                                                    &bufferSessionDescription );
//         if( peerConnectionResult != PEER_CONNECTION_RESULT_OK )
//         {
//             LogWarn( ( "PeerConnection_SetLocalDescription fail, result: %d.", peerConnectionResult ) );
//         }
//     }

//     if( skipProcess == 0 )
//     {
//         pAppContext->sdpConstructedBufferLength = PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH;
//         peerConnectionResult = PeerConnection_CreateOffer( &pAppSession->peerConnectionSession,
//                                                            &bufferSessionDescription,
//                                                            pAppContext->sdpConstructedBuffer,
//                                                            &pAppContext->sdpConstructedBufferLength );
//         if( peerConnectionResult != PEER_CONNECTION_RESULT_OK )
//         {
//             LogWarn( ( "PeerConnection_CreateOffer fail, result: %d.", peerConnectionResult ) );
//             skipProcess = 1;
//         }
//     }

//     if( skipProcess == 0 )
//     {
//         /* Translate from SDP formal format into signaling event message by replacing newline with "\\n" or "\\r\\n". */
//         sdpOfferMessageLength = PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH;
//         signalingControllerReturn = SignalingController_SerializeSdpContentNewline( pAppContext->sdpConstructedBuffer,
//                                                                                     pAppContext->sdpConstructedBufferLength,
//                                                                                     pAppContext->sdpBuffer,
//                                                                                     &sdpOfferMessageLength );
//         if( signalingControllerReturn != SIGNALING_CONTROLLER_RESULT_OK )
//         {
//             LogError( ( "Fail to deserialize SDP offer newline, result: %d, constructed buffer(%u): %.*s",
//                         signalingControllerReturn,
//                         pAppContext->sdpConstructedBufferLength,
//                         ( int ) pAppContext->sdpConstructedBufferLength,
//                         pAppContext->sdpConstructedBuffer ) );
//             skipProcess = 1;
//         }
//     }

//     if( skipProcess == 0 )
//     {
//         eventMessage.eventContent.correlationIdLength = 0U;
//         memset( eventMessage.eventContent.correlationId, 0, SECRET_ACCESS_KEY_MAX_LEN );
//         eventMessage.eventContent.messageType = SIGNALING_TYPE_MESSAGE_SDP_OFFER;
//         eventMessage.eventContent.pDecodeMessage = pAppContext->sdpBuffer;
//         eventMessage.eventContent.decodeMessageLength = sdpOfferMessageLength;
//         memcpy( eventMessage.eventContent.remoteClientId, pEvent->pRemoteClientId, pEvent->remoteClientIdLength );
//         eventMessage.eventContent.remoteClientIdLength = pEvent->remoteClientIdLength;

//         signalingControllerReturn = SignalingController_SendMessage( &pAppContext->ignalingControllerContext, &eventMessage );
//         if( signalingControllerReturn != SIGNALING_CONTROLLER_RESULT_OK )
//         {
//             skipProcess = 1;
//             LogError( ( "Send signaling message fail, result: %d", signalingControllerReturn ) );
//         }
//     }

//     if( skipProcess == 0 )
//     {
//         LogInfo( ( "Created SDP offer(%u): %.*s",
//                    sdpOfferMessageLength,
//                    ( int ) sdpOfferMessageLength,
//                    pAppContext->sdpBuffer ) );
//     }

//     return ret;
// }
