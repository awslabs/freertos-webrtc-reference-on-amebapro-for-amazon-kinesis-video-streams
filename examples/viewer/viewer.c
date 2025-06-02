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

/* This file is not compiled now. The function here is prepared for future use as viewer side. */
#define DEMO_MASTER_CLIENT_ID "ProduceMaster"
#define DEMO_MASTER_CLIENT_ID_LENGTH ( 13 )

static int32_t CreateSdpOffer( AppContext_t * pAppContext )
{
    int32_t ret = 0;
    uint8_t skipProcess = 0;
    SignalingControllerResult_t signalingControllerReturn;
    PeerConnectionResult_t peerConnectionResult;
    PeerConnectionBufferSessionDescription_t bufferSessionDescription;
    size_t sdpOfferMessageLength = 0;
    AppSession_t * pAppSession = NULL;
    SignalingControllerEventMessage_t eventMessage = {
        .event = SIGNALING_CONTROLLER_EVENT_SEND_WSS_MESSAGE,
        .onCompleteCallback = NULL,
        .pOnCompleteCallbackContext = NULL,
    };

    if( skipProcess == 0 )
    {
        pAppSession = GetCreatePeerConnectionSession( pAppContext, DEMO_MASTER_CLIENT_ID, DEMO_MASTER_CLIENT_ID_LENGTH, 1U );
        if( pAppSession == NULL )
        {
            LogWarn( ( "No available peer connection session for remote client ID(%u): %.*s",
                       DEMO_MASTER_CLIENT_ID_LENGTH,
                       ( int ) DEMO_MASTER_CLIENT_ID_LENGTH,
                       DEMO_MASTER_CLIENT_ID ) );
            skipProcess = 1;
        }
    }

    if( skipProcess == 0 )
    {
        memset( &bufferSessionDescription, 0, sizeof( PeerConnectionBufferSessionDescription_t ) );
        bufferSessionDescription.pSdpBuffer = pAppContext->sdpBuffer;
        bufferSessionDescription.sdpBufferLength = PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH;
        peerConnectionResult = PeerConnection_SetLocalDescription( &pAppSession->peerConnectionSession,
                                                                   &bufferSessionDescription );
        if( peerConnectionResult != PEER_CONNECTION_RESULT_OK )
        {
            LogWarn( ( "PeerConnection_SetLocalDescription fail, result: %d.", peerConnectionResult ) );
        }
    }

    if( skipProcess == 0 )
    {
        pAppContext->sdpConstructedBufferLength = PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH;
        peerConnectionResult = PeerConnection_CreateOffer( &pAppSession->peerConnectionSession,
                                                           &bufferSessionDescription,
                                                           pAppContext->sdpConstructedBuffer,
                                                           &pAppContext->sdpConstructedBufferLength );
        if( peerConnectionResult != PEER_CONNECTION_RESULT_OK )
        {
            LogWarn( ( "PeerConnection_CreateOffer fail, result: %d.", peerConnectionResult ) );
            skipProcess = 1;
        }
    }

    if( skipProcess == 0 )
    {
        /* Translate from SDP formal format into signaling event message by replacing newline with "\\n" or "\\r\\n". */
        sdpOfferMessageLength = PEER_CONNECTION_SDP_DESCRIPTION_BUFFER_MAX_LENGTH;
        signalingControllerReturn = SignalingController_SerializeSdpContentNewline( pAppContext->sdpConstructedBuffer,
                                                                                    pAppContext->sdpConstructedBufferLength,
                                                                                    pAppContext->sdpBuffer,
                                                                                    &sdpOfferMessageLength );
        if( signalingControllerReturn != SIGNALING_CONTROLLER_RESULT_OK )
        {
            LogError( ( "Fail to deserialize SDP offer newline, result: %d, constructed buffer(%u): %.*s",
                        signalingControllerReturn,
                        pAppContext->sdpConstructedBufferLength,
                        ( int ) pAppContext->sdpConstructedBufferLength,
                        pAppContext->sdpConstructedBuffer ) );
            skipProcess = 1;
        }
    }

    if( skipProcess == 0 )
    {
        eventMessage.eventContent.correlationIdLength = 0U;
        memset( eventMessage.eventContent.correlationId, 0, SECRET_ACCESS_KEY_MAX_LEN );
        eventMessage.eventContent.messageType = SIGNALING_TYPE_MESSAGE_SDP_OFFER;
        eventMessage.eventContent.pDecodeMessage = pAppContext->sdpBuffer;
        eventMessage.eventContent.decodeMessageLength = sdpOfferMessageLength;
        memcpy( eventMessage.eventContent.remoteClientId, pEvent->pRemoteClientId, pEvent->remoteClientIdLength );
        eventMessage.eventContent.remoteClientIdLength = pEvent->remoteClientIdLength;

        signalingControllerReturn = SignalingController_SendMessage( &pAppContext->ignalingControllerContext, &eventMessage );
        if( signalingControllerReturn != SIGNALING_CONTROLLER_RESULT_OK )
        {
            skipProcess = 1;
            LogError( ( "Send signaling message fail, result: %d", signalingControllerReturn ) );
        }
    }

    if( skipProcess == 0 )
    {
        LogInfo( ( "Created SDP offer(%u): %.*s",
                   sdpOfferMessageLength,
                   ( int ) sdpOfferMessageLength,
                   pAppContext->sdpBuffer ) );
    }

    return ret;
}