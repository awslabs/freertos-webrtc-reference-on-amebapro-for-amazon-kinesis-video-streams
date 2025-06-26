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

#include "logging.h"
#include "demo_config.h"
#include "ameba_pro2_media_port.h"
#include "platform_opts.h"
#include <math.h>

#include "mmf2_link.h"
#include "mmf2_siso.h"
#include "mmf2_miso.h"

#include "module_video.h"
#include "module_audio.h"
#include "module_g711.h"
#include "module_opusc.h"
#include "module_opusd.h"
#include "module_array.h"
#include "opus_defines.h"
#include "opus.h"

#include "avcodec.h"

#include "video_api.h"

#include "FreeRTOS.h"
#include "networking_utils.h"
#include "queue.h"
#include "semphr.h"

#if METRIC_PRINT_ENABLED
#include "metric.h"
#endif

/* Audio sending and receiving both enabled using MMF */
#define MEDIA_PORT_ENABLE_AUDIO_RECV ( 1 )

/* MMF Array Module for dynamic audio injection */
#if MEDIA_PORT_ENABLE_AUDIO_RECV

/* Audio frame buffer for dynamic injection */
static uint8_t * pAudioFrameBuffer = NULL;
static size_t audioFrameBufferSize = 0;
static SemaphoreHandle_t audioFrameMutex = NULL;

/* Function to handle received audio frames through MMF Array Module */
int32_t AppMediaSourcePort_PlayAudioFrame( uint8_t *pData, size_t dataLen )
{
    static int frame_count = 0;
    frame_count++;
    
    if (frame_count % 50 == 0) { // Log every 50th frame to avoid spam
        LogInfo(("MMF: Received audio frame %d, size: %d bytes", frame_count, dataLen));
    }
    
    /* Take mutex to protect buffer access */
    if (audioFrameMutex != NULL && xSemaphoreTake(audioFrameMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        /* Reallocate buffer if needed */
        if (audioFrameBufferSize < dataLen) {
            if (pAudioFrameBuffer != NULL) {
                vPortFree(pAudioFrameBuffer);
            }
            pAudioFrameBuffer = (uint8_t*)pvPortMalloc(dataLen);
            if (pAudioFrameBuffer != NULL) {
                audioFrameBufferSize = dataLen;
            } else {
                audioFrameBufferSize = 0;
                LogError(("Failed to allocate audio frame buffer"));
                xSemaphoreGive(audioFrameMutex);
                return -1;
            }
        }
        
        /* Copy received frame to buffer */
        if (pAudioFrameBuffer != NULL) {
            memcpy(pAudioFrameBuffer, pData, dataLen);
            
            /* Update array module with new audio data */
            extern mm_context_t * pArrayContext;
            if (pArrayContext != NULL) {
                array_t audioArray;
                audioArray.data_addr = (uint32_t)pAudioFrameBuffer;
                audioArray.data_len = dataLen;
                
                /* Stop streaming temporarily */
                mm_module_ctrl(pArrayContext, CMD_ARRAY_STREAMING, 0);
                
                /* Small delay to help AEC synchronization */
                vTaskDelay(pdMS_TO_TICKS(1));
                
                /* Update array data */
                mm_module_ctrl(pArrayContext, CMD_ARRAY_SET_ARRAY, (int)&audioArray);
                
                /* Restart streaming */
                mm_module_ctrl(pArrayContext, CMD_ARRAY_STREAMING, 1);
                
                LogDebug(("Audio frame injected into MMF pipeline: %d bytes", dataLen));
            }
        }
        
        xSemaphoreGive(audioFrameMutex);
    } else {
        LogWarn(("Could not acquire audio frame mutex"));
        return -1;
    }
    
    return 0;
}

#else /* MEDIA_PORT_ENABLE_AUDIO_RECV */

/* Stub implementation when audio receive is disabled */
int32_t AppMediaSourcePort_PlayAudioFrame( uint8_t *pData, size_t dataLen )
{
    (void)pData;
    (void)dataLen;
    LogDebug(("Audio playback disabled - received frame ignored"));
    return 0;
}

#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

/* used to monitor skb resource */
extern int skbbuf_used_num;
extern int skbdata_used_num;
extern int max_local_skb_num;
extern int max_skb_buf_num;

#define MEDIA_PORT_SKB_BUFFER_THRESHOLD ( 64 )

#define VIDEO_QCIF  0
#define VIDEO_CIF   1
#define VIDEO_WVGA  2
#define VIDEO_VGA   3
#define VIDEO_D1    4
#define VIDEO_HD    5
#define VIDEO_FHD   6
#define VIDEO_3M    7
#define VIDEO_5M    8
#define VIDEO_2K    9

/*****************************************************************************
* ISP channel : 0
* Video type  : H264/HEVC
*****************************************************************************/
#define MEDIA_PORT_V1_CHANNEL 0
#define MEDIA_PORT_V1_RESOLUTION VIDEO_HD
#define MEDIA_PORT_V1_FPS 30
#define MEDIA_PORT_V1_GOP 30
#define MEDIA_PORT_V1_BPS 512 * 1024
#define MEDIA_PORT_V1_RCMODE 2 // 1: CBR, 2: VBR

#if USE_VIDEO_CODEC_H265
#define MEDIA_PORT_VIDEO_TYPE VIDEO_HEVC
#define MEDIA_PORT_VIDEO_CODEC AV_CODEC_ID_H265
#else
#define MEDIA_PORT_VIDEO_TYPE VIDEO_H264
#define MEDIA_PORT_VIDEO_CODEC AV_CODEC_ID_H264
#endif

#if MEDIA_PORT_V1_RESOLUTION == VIDEO_VGA
#define MEDIA_PORT_V1_WIDTH 640
#define MEDIA_PORT_V1_HEIGHT 480
#elif MEDIA_PORT_V1_RESOLUTION == VIDEO_HD
#define MEDIA_PORT_V1_WIDTH 1280
#define MEDIA_PORT_V1_HEIGHT 720
#elif MEDIA_PORT_V1_RESOLUTION == VIDEO_FHD
#define MEDIA_PORT_V1_WIDTH 1920
#define MEDIA_PORT_V1_HEIGHT 1080
#endif

static mm_context_t * pVideoContext = NULL;
static mm_context_t * pAudioContext = NULL;
#if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
#if MEDIA_PORT_ENABLE_AUDIO_RECV
static mm_context_t * pG711dContext = NULL;
#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
static mm_context_t * pG711eContext = NULL;
#endif /* ( AUDIO_G711_MULAW || AUDIO_G711_ALAW ) */
#if ( AUDIO_OPUS )
static mm_context_t * pOpuscContext = NULL;
#if MEDIA_PORT_ENABLE_AUDIO_RECV
static mm_context_t * pOpusdContext = NULL;
#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
#endif /* AUDIO_OPUS */
#if MEDIA_PORT_ENABLE_AUDIO_RECV
mm_context_t * pArrayContext = NULL;  /* Array module for audio injection */
#endif
static mm_context_t * pWebrtcMmContext = NULL;

static mm_siso_t * pSisoAudioA1 = NULL;
#if MEDIA_PORT_ENABLE_AUDIO_RECV
static mm_siso_t * pSisoArrayDecoder = NULL;  /* Array -> Decoder pipeline */
static mm_siso_t * pSisoDecoderAudio = NULL;  /* Decoder -> Audio pipeline */
#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
static mm_miso_t * pMisoWebrtc = NULL;

static video_params_t videoParams = {
    .stream_id = MEDIA_PORT_V1_CHANNEL,
    .type = MEDIA_PORT_VIDEO_TYPE,
    .resolution = MEDIA_PORT_V1_RESOLUTION,
    .width = MEDIA_PORT_V1_WIDTH,
    .height = MEDIA_PORT_V1_HEIGHT,
    .bps = MEDIA_PORT_V1_BPS,
    .fps = MEDIA_PORT_V1_FPS,
    .gop = MEDIA_PORT_V1_GOP,
    .rc_mode = MEDIA_PORT_V1_RCMODE,
    .use_static_addr = 1
};

#if !USE_DEFAULT_AUDIO_SET
static audio_params_t audioParams = {
    .sample_rate = ASR_8KHZ,
    .word_length = WL_16BIT,
    .mic_gain = MIC_30DB,  // Reduce mic gain to minimize echo pickup
    .dmic_l_gain = DMIC_BOOST_24DB,
    .dmic_r_gain = DMIC_BOOST_24DB,
    .use_mic_type = USE_AUDIO_AMIC,
    .channel = 1,
    .mix_mode = 1,     // Enable mix mode for bidirectional audio
    .enable_aec = 1,   // Enable AEC for proper bidirectional audio operation
    .ADC_gain = 0x55,  // Reduce ADC gain for less sensitive microphone
    .DAC_gain = 0x8F,  // Moderate DAC gain for clear but not overpowering speaker output
    .hpf_set = 1       // Enable high-pass filter to remove low-frequency noise
};
#endif

#if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
static g711_params_t g711eParams = {
    .codec_id = AV_CODEC_ID_PCMU,
    .buf_len = 2048,
    .mode = G711_ENCODE
};

#if MEDIA_PORT_ENABLE_AUDIO_RECV
static g711_params_t g711dParams = {
    .codec_id = AV_CODEC_ID_PCMU,
    .buf_len = 2048,
    .mode = G711_DECODE
};
#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
#endif /* ( AUDIO_G711_MULAW || AUDIO_G711_ALAW ) */

#if ( AUDIO_OPUS )
static opusc_params_t opuscParams = {
    .sample_rate = 8000,
    .channel = 1,
    .bit_length = 16,
    .complexity = 5,     // Restore original complexity for better quality
    .bitrate = 25000,
    .use_framesize = 40, // Use 40ms frame size to prevent buffer overflow
    .enable_vbr = 1,
    .vbr_constraint = 0,
    .packetLossPercentage = 0,
    .opus_application = OPUS_APPLICATION_VOIP  // Use VOIP mode for better real-time performance
};

#if MEDIA_PORT_ENABLE_AUDIO_RECV
static opusd_params_t opusdParams = {
    .sample_rate = 8000,
    .channel = 1,
    .bit_length = 16,
    .opus_application = OPUS_APPLICATION_AUDIO
};

/* Array module parameters for Opus audio injection */
static array_params_t opusArrayParams = {
    .type = AVMEDIA_TYPE_AUDIO,
    .codec_id = AV_CODEC_ID_OPUS,
    .mode = ARRAY_MODE_ONCE,  /* Play once per injection, not loop */
    .u = {
        .a = {
            .channel = 1,
            .samplerate = 8000,
            .frame_size = 320,  /* 40ms @ 8kHz = 320 samples */
        }
    }
};
#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

#endif /* AUDIO_OPUS */

static int HandleModuleFrameHook( void * p,
                                  void * input,
                                  void * output );
static int ControlModuleHook( void * p,
                              int cmd,
                              int arg );
static void * DestroyModuleHook( void * p );
static void * CreateModuleHook( void * parent );
static void * NewModuleItemHook( void * p );
static void * DeleteModuleItemHook( void * p,
                                    void * d );

mm_module_t webrtcMmModule = {
    .create = CreateModuleHook,
    .destroy = DestroyModuleHook,
    .control = ControlModuleHook,
    .handle = HandleModuleFrameHook,

    .new_item = NewModuleItemHook,
    .del_item = DeleteModuleItemHook,

    .output_type = MM_TYPE_ASINK, // output for audio sink
    .module_type = MM_TYPE_AVSINK, // module type is video algorithm
    .name = "KVS_WebRTC"
};

static int HandleModuleFrameHook( void * p,
                                  void * input,
                                  void * output )
{
    int ret = 0;
    MediaModuleContext_t * pCtx = ( MediaModuleContext_t * )p;
    MediaFrame_t frame;
    mm_queue_item_t * pInputItem = ( mm_queue_item_t * )input;
    mm_queue_item_t * pOutputItem = ( mm_queue_item_t * )output;

    ( void ) pOutputItem;

    if( pCtx->mediaStart != 0 )
    {
        /* Debug: Log all frames coming to WebRTC module */
        static int total_frame_count = 0;
        total_frame_count++;
        if (total_frame_count % 100 == 0) {
            LogInfo(("WebRTC module received %d total frames", total_frame_count));
        }
        
        do
        {
            /* Set SKB buffer threshold to manage memory allocation. Reference:
             * https://github.com/Freertos-kvs-LTS/freertos-kvs-LTS/blob/bd0702130e0b8dfa386e011644ce1bc7e0d7fd09/component/example/kvs_webrtc_mmf/webrtc_app_src/AppMediaSrc_AmebaPro2.c#L86-L88 */
            if( ( skbdata_used_num > ( max_skb_buf_num - MEDIA_PORT_SKB_BUFFER_THRESHOLD ) ) ||
                ( skbbuf_used_num > ( max_local_skb_num - MEDIA_PORT_SKB_BUFFER_THRESHOLD ) ) )
            {
                ret = -1;
                break; //skip this frame and wait for skb resource release.
            }

            frame.size = pInputItem->size;
            frame.pData = ( uint8_t * ) pvPortMalloc( frame.size );
            if( !frame.pData )
            {
                LogWarn( ( "Fail to allocate memory for webrtc media frame" ) );
                ret = -1;
                break;
            }

            memcpy( frame.pData,
                    ( uint8_t * )pInputItem->data_addr,
                    frame.size );
            frame.freeData = 1;
            frame.timestampUs = NetworkingUtils_GetCurrentTimeUs( &pInputItem->timestamp );

            if( ( pInputItem->type == AV_CODEC_ID_H264 ) || ( pInputItem->type == AV_CODEC_ID_H265 ) )
            {
                if( pCtx->onVideoFrameReadyToSendFunc )
                {
                    frame.trackKind = TRANSCEIVER_TRACK_KIND_VIDEO;
                    ( void ) pCtx->onVideoFrameReadyToSendFunc( pCtx->pOnVideoFrameReadyToSendCustomContext,
                                                                &frame );
                }
                else
                {
                    LogError( ( "No available ready to send callback function pointer for video." ) );
                    vPortFree( frame.pData );
                    ret = -1;
                }
            }
            else if( ( pInputItem->type == AV_CODEC_ID_OPUS ) ||
                     ( pInputItem->type == AV_CODEC_ID_PCMU ) )
            {
                /* Debug: Log audio frame details */
                static int audio_frame_count = 0;
                audio_frame_count++;
                if (audio_frame_count % 10 == 0) {
                    LogInfo(("Audio frame #%d: type=0x%lx, size=%lu", audio_frame_count, (unsigned long)pInputItem->type, (unsigned long)frame.size));
                }
                
                if( pCtx->onAudioFrameReadyToSendFunc )
                {
                    frame.trackKind = TRANSCEIVER_TRACK_KIND_AUDIO;
                    ( void ) pCtx->onAudioFrameReadyToSendFunc( pCtx->pOnAudioFrameReadyToSendCustomContext,
                                                                &frame );
                    /* Add periodic logging to verify audio is being sent */
                    static int audio_send_count = 0;
                    audio_send_count++;
                    if (audio_send_count % 10 == 0) {
                        LogInfo(("Audio frame sent to viewer - frame size: %lu bytes", (unsigned long)frame.size));
                    }
                }
                else
                {
                    LogError( ( "No available ready to send callback function pointer for audio." ) );
                    vPortFree( frame.pData );
                    ret = -1;
                }
            }
            else
            {
                static int unknown_frame_count = 0;
                unknown_frame_count++;
                if (unknown_frame_count % 10 == 0) {
                    LogWarn( ( "Input type cannot be handled: type=0x%lx, size=%lu (count=%d)", (unsigned long)pInputItem->type, (unsigned long)frame.size, unknown_frame_count ) );
                }
                vPortFree( frame.pData );
                ret = -1;
            }
        } while( pdFALSE );
    }

    return ret;
}

static int ControlModuleHook( void * p,
                              int cmd,
                              int arg )
{
    MediaModuleContext_t * pCtx = ( MediaModuleContext_t * )p;

    switch( cmd )
    {
        case CMD_KVS_WEBRTC_START:
            /* If loopback is enabled, we don't need the camera to provide frames.
             * Instead, we loopback the received frames. */
            #ifdef ENABLE_STREAMING_LOOPBACK
            pCtx->mediaStart = 0;
            #else
            pCtx->mediaStart = 1;
            #endif
            break;
        case CMD_KVS_WEBRTC_STOP:
            pCtx->mediaStart = 0;
            break;
        case CMD_KVS_WEBRTC_REG_VIDEO_SEND_CALLBACK:
            pCtx->onVideoFrameReadyToSendFunc = ( OnFrameReadyToSend_t ) arg;
            break;
        case CMD_KVS_WEBRTC_REG_VIDEO_SEND_CALLBACK_CUSTOM_CONTEXT:
            pCtx->pOnVideoFrameReadyToSendCustomContext = ( void * ) arg;
            break;
        case CMD_KVS_WEBRTC_REG_AUDIO_SEND_CALLBACK:
            pCtx->onAudioFrameReadyToSendFunc = ( OnFrameReadyToSend_t ) arg;
            break;
        case CMD_KVS_WEBRTC_REG_AUDIO_SEND_CALLBACK_CUSTOM_CONTEXT:
            pCtx->pOnAudioFrameReadyToSendCustomContext = ( void * ) arg;
            break;

        default:
            LogWarn( ( "Unknown module command" ) );
            break;
    }
    return 0;
}

static void * DestroyModuleHook( void * p )
{
    MediaModuleContext_t * ctx = ( MediaModuleContext_t * )p;
    if( ctx )
    {

        vPortFree( ctx );
    }
    return NULL;
}

static void * CreateModuleHook( void * parent )
{
    MediaModuleContext_t * ctx = pvPortMalloc( sizeof( MediaModuleContext_t ) );

    if( ctx )
    {
        memset( ctx,
                0,
                sizeof( MediaModuleContext_t ) );
        ctx->pParent = parent;
    }

    return ctx;
}

static void * NewModuleItemHook( void * p )
{
    ( void ) p;
    return NULL;
}

static void * DeleteModuleItemHook( void * p,
                                    void * d )
{
    ( void ) p;
    ( void ) d;
    return NULL;
}

void AppMediaSourcePort_Destroy( void )
{
    // Pause Linkers
    siso_pause( pSisoAudioA1 );
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( pSisoArrayDecoder )
    {
        siso_pause( pSisoArrayDecoder );
    }
    if( pSisoDecoderAudio )
    {
        siso_pause( pSisoDecoderAudio );
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
    miso_pause( pMisoWebrtc,
                MM_OUTPUT );

    // Stop modules
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_STOP,
                    0 );
    mm_module_ctrl( pVideoContext,
                    CMD_VIDEO_STREAM_STOP,
                    MEDIA_PORT_V1_CHANNEL );
    mm_module_ctrl( pAudioContext,
                    CMD_AUDIO_SET_TRX,
                    0 );
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( pArrayContext )
    {
        mm_module_ctrl( pArrayContext,
                        CMD_ARRAY_STREAMING,
                        0 );
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

    // Delete linkers
    pSisoAudioA1 = siso_delete( pSisoAudioA1 );
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( pSisoArrayDecoder )
    {
        pSisoArrayDecoder = siso_delete( pSisoArrayDecoder );
    }
    if( pSisoDecoderAudio )
    {
        pSisoDecoderAudio = siso_delete( pSisoDecoderAudio );
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
    pMisoWebrtc = miso_delete( pMisoWebrtc );

    // Close modules
    pWebrtcMmContext = mm_module_close( pWebrtcMmContext );
    pVideoContext = mm_module_close( pVideoContext );
    pAudioContext = mm_module_close( pAudioContext );
    #if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
    pG711eContext = mm_module_close( pG711eContext );
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    pG711dContext = mm_module_close( pG711dContext );
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
    #elif AUDIO_OPUS
    pOpuscContext = mm_module_close( pOpuscContext );
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    pOpusdContext = mm_module_close( pOpusdContext );
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
    #endif
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( pArrayContext )
    {
        pArrayContext = mm_module_close( pArrayContext );
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

    // Clean up audio frame buffer and mutex
    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( audioFrameMutex != NULL )
    {
        if( xSemaphoreTake( audioFrameMutex, pdMS_TO_TICKS(100) ) == pdTRUE )
        {
            if( pAudioFrameBuffer != NULL )
            {
                vPortFree( pAudioFrameBuffer );
                pAudioFrameBuffer = NULL;
            }
            audioFrameBufferSize = 0;
            xSemaphoreGive( audioFrameMutex );
        }
        vSemaphoreDelete( audioFrameMutex );
        audioFrameMutex = NULL;
        LogInfo( ( "Audio frame buffer and mutex cleaned up" ) );
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

    // Video Deinit
    video_deinit();
}

int32_t AppMediaSourcePort_Init( void )
{
    int32_t ret = 0;
    int voe_heap_size;

    pWebrtcMmContext = mm_module_open( &webrtcMmModule );
    if( pWebrtcMmContext )
    {
        mm_module_ctrl( pWebrtcMmContext,
                        MM_CMD_SET_QUEUE_LEN,
                        3 );
        mm_module_ctrl( pWebrtcMmContext,
                        MM_CMD_INIT_QUEUE_ITEMS,
                        MMQI_FLAG_STATIC );
        mm_module_ctrl( pWebrtcMmContext, CMD_KVS_WEBRTC_SET_APPLY, 0 );
        

    }
    else
    {
        LogError( ( "KVS open fail" ) );
        ret = -1;
    }

    if( ret == 0 )
    {
        voe_heap_size = video_voe_presetting( 1, MEDIA_PORT_V1_WIDTH, MEDIA_PORT_V1_HEIGHT, MEDIA_PORT_V1_BPS, 0,
                                              0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0,
                                              0, 0, 0 );
        ( void ) voe_heap_size;
        LogInfo( ( "voe heap size initialized" ) );
    }

    if( ret == 0 )
    {
        pVideoContext = mm_module_open( &video_module );
        if( pVideoContext )
        {
            mm_module_ctrl( pVideoContext,
                            CMD_VIDEO_SET_PARAMS,
                            ( int )&videoParams );
            mm_module_ctrl( pVideoContext,
                            MM_CMD_SET_QUEUE_LEN,
                            MEDIA_PORT_V1_FPS * 3 );
            mm_module_ctrl( pVideoContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_DYNAMIC );
            mm_module_ctrl( pVideoContext,
                            CMD_VIDEO_APPLY,
                            MEDIA_PORT_V1_CHANNEL ); // start channel 0
        }
        else
        {
            LogError( ( "video open fail" ) );
            ret = -1;
        }
    }

    if( ret == 0 )
    {
        pAudioContext = mm_module_open( &audio_module );
        if( pAudioContext )
        {
            #if !USE_DEFAULT_AUDIO_SET
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_PARAMS,
                            ( int )&audioParams );
            #endif
            mm_module_ctrl( pAudioContext,
                            MM_CMD_SET_QUEUE_LEN,
                            3 );  // Restore standard queue length
            mm_module_ctrl( pAudioContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_APPLY,
                            0 );
            /* Enable both transmit (capture) and receive (playback) */
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_TRX,
                            1 );  // Enable both TX and RX for bidirectional audio
            
            /* Enhanced AEC configuration for better echo cancellation */
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_AEC_ENABLE,
                            1 );  // Ensure AEC is enabled
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_AEC_LEVEL,
                            3 );  // Set AEC to aggressive level (0-3, 3 is most aggressive)
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_NS_ENABLE,
                            2 );  // Enable noise suppression (moderate level)
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_AGC_ENABLE,
                            1 );  // Enable automatic gain control
            
            LogInfo(("Audio bidirectional system started with enhanced AEC (capture + playback)"));
        }
        else
        {
            LogError( ( "Audio open fail" ) );
            ret = -1;
        }
    }

    if( ret == 0 )
    {
        #if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
        pG711eContext = mm_module_open( &g711_module );
        if( pG711eContext )
        {
            mm_module_ctrl( pG711eContext,
                            CMD_G711_SET_PARAMS,
                            ( int )&g711eParams );
            mm_module_ctrl( pG711eContext,
                            MM_CMD_SET_QUEUE_LEN,
                            6 );
            mm_module_ctrl( pG711eContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pG711eContext,
                            CMD_G711_APPLY,
                            0 );
        }
        else
        {
            LogError( ( "G711 encoder open fail" ) );
            ret = -1;
        }
        
        #if MEDIA_PORT_ENABLE_AUDIO_RECV
        pG711dContext = mm_module_open( &g711_module );
        if( pG711dContext )
        {
            mm_module_ctrl( pG711dContext,
                            CMD_G711_SET_PARAMS,
                            ( int )&g711dParams );
            mm_module_ctrl( pG711dContext,
                            MM_CMD_SET_QUEUE_LEN,
                            6 );
            mm_module_ctrl( pG711dContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pG711dContext,
                            CMD_G711_APPLY,
                            0 );
        }
        else
        {
            LogError( ( "G711 decoder open fail" ) );
            ret = -1;
        }
        #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
        
        #elif AUDIO_OPUS
        pOpuscContext = mm_module_open( &opusc_module );
        if( pOpuscContext )
        {
            mm_module_ctrl( pOpuscContext,
                            CMD_OPUSC_SET_PARAMS,
                            ( int )&opuscParams );
            mm_module_ctrl( pOpuscContext,
                            MM_CMD_SET_QUEUE_LEN,
                            6 );  // Restore standard queue length
            mm_module_ctrl( pOpuscContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pOpuscContext,
                            CMD_OPUSC_APPLY,
                            0 );
        }
        else
        {
            LogError( ( "OPUSC encoder open fail" ) );
            ret = -1;
        }
        
        #if MEDIA_PORT_ENABLE_AUDIO_RECV
        pOpusdContext = mm_module_open( &opusd_module );
        if( pOpusdContext )
        {
            mm_module_ctrl( pOpusdContext,
                            CMD_OPUSD_SET_PARAMS,
                            ( int )&opusdParams );
            mm_module_ctrl( pOpusdContext,
                            MM_CMD_SET_QUEUE_LEN,
                            6 );
            mm_module_ctrl( pOpusdContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pOpusdContext,
                            CMD_OPUSD_APPLY,
                            0 );
        }
        else
        {
            LogError( ( "OPUSD decoder open fail" ) );
            ret = -1;
        }
        #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
        
        #endif
    }

    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( ret == 0 )
    {
        /* Initialize audio frame buffer mutex */
        audioFrameMutex = xSemaphoreCreateMutex();
        if( audioFrameMutex == NULL )
        {
            LogError( ( "Failed to create audio frame mutex" ) );
            ret = -1;
        }
        else
        {
            LogInfo( ( "Audio frame mutex created successfully" ) );
        }
    }

    if( ret == 0 )
    {
        /* Initialize Array Module for dynamic audio injection */
        pArrayContext = mm_module_open( &array_module );
        if( pArrayContext )
        {
            #if AUDIO_OPUS
            mm_module_ctrl( pArrayContext,
                            CMD_ARRAY_SET_PARAMS,
                            ( int )&opusArrayParams );
            #elif ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
            /* Configure array for G.711 if using G.711 codec */
            array_params_t g711ArrayParams = {
                .type = AVMEDIA_TYPE_AUDIO,
                .codec_id = AV_CODEC_ID_PCMU,
                .mode = ARRAY_MODE_ONCE,
                .u = {
                    .a = {
                        .channel = 1,
                        .samplerate = 8000,
                        .frame_size = 160,  /* 20ms @ 8kHz = 160 samples */
                    }
                }
            };
            mm_module_ctrl( pArrayContext,
                            CMD_ARRAY_SET_PARAMS,
                            ( int )&g711ArrayParams );
            #endif
            mm_module_ctrl( pArrayContext,
                            MM_CMD_SET_QUEUE_LEN,
                            6 );
            mm_module_ctrl( pArrayContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_DYNAMIC );
            mm_module_ctrl( pArrayContext,
                            CMD_ARRAY_APPLY,
                            0 );
            /* Don't start streaming yet - will be controlled by frame injection */
            LogInfo( ( "Array module for audio injection initialized" ) );
        }
        else
        {
            LogError( ( "Array module open fail" ) );
            ret = -1;
        }
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

    if( ret == 0 )
    {
        /* Audio capture pipeline: Audio -> Encoder -> WebRTC */
        pSisoAudioA1 = siso_create();
        if( pSisoAudioA1 )
        {
            siso_ctrl( pSisoAudioA1,
                       MMIC_CMD_ADD_INPUT,
                       ( uint32_t )pAudioContext,
                       0 );
            #if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
            siso_ctrl( pSisoAudioA1,
                       MMIC_CMD_ADD_OUTPUT,
                       ( uint32_t )pG711eContext,
                       0 );
            #elif AUDIO_OPUS
            siso_ctrl( pSisoAudioA1,
                       MMIC_CMD_ADD_OUTPUT,
                       ( uint32_t )pOpuscContext,
                       0 );
            siso_ctrl( pSisoAudioA1,
                       MMIC_CMD_SET_STACKSIZE,
                       32 * 1024,
                       0 );
            #endif
            siso_start( pSisoAudioA1 );
            LogInfo(("Audio capture pipeline started"));
        }
        else
        {
            LogError( ( "Audio capture pipeline creation fail" ) );
            ret = -1;
        }
    }

    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( ret == 0 )
    {
        /* Audio playback pipeline: Array -> Decoder */
        pSisoArrayDecoder = siso_create();
        if( pSisoArrayDecoder )
        {
            siso_ctrl( pSisoArrayDecoder,
                       MMIC_CMD_ADD_INPUT,
                       ( uint32_t )pArrayContext,
                       0 );
            #if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
            siso_ctrl( pSisoArrayDecoder,
                       MMIC_CMD_ADD_OUTPUT,
                       ( uint32_t )pG711dContext,
                       0 );
            #elif AUDIO_OPUS
            siso_ctrl( pSisoArrayDecoder,
                       MMIC_CMD_ADD_OUTPUT,
                       ( uint32_t )pOpusdContext,
                       0 );
            siso_ctrl( pSisoArrayDecoder,
                       MMIC_CMD_SET_STACKSIZE,
                       24 * 1024,
                       0 );
            #endif
            siso_start( pSisoArrayDecoder );
            LogInfo(("Audio playback pipeline: Array -> Decoder"));
        }
        else
        {
            LogError( ( "Audio playback Array->Decoder pipeline creation fail" ) );
            ret = -1;
        }
    }

    if( ret == 0 )
    {
        /* Audio playback pipeline: Decoder -> Audio Module */
        pSisoDecoderAudio = siso_create();
        if( pSisoDecoderAudio )
        {
            #if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
            siso_ctrl( pSisoDecoderAudio,
                       MMIC_CMD_ADD_INPUT,
                       ( uint32_t )pG711dContext,
                       0 );
            #elif AUDIO_OPUS
            siso_ctrl( pSisoDecoderAudio,
                       MMIC_CMD_ADD_INPUT,
                       ( uint32_t )pOpusdContext,
                       0 );
            #endif
            siso_ctrl( pSisoDecoderAudio,
                       MMIC_CMD_ADD_OUTPUT,
                       ( uint32_t )pAudioContext,
                       0 );
            siso_start( pSisoDecoderAudio );
            LogInfo(("Audio playback pipeline: Decoder -> Audio Module"));
        }
        else
        {
            LogError( ( "Audio playback Decoder->Audio pipeline creation fail" ) );
            ret = -1;
        }
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

    if( ret == 0 )
    {
        pMisoWebrtc = miso_create();
        if( pMisoWebrtc )
        {
            #if defined( configENABLE_TRUSTZONE ) && ( configENABLE_TRUSTZONE == 1 )
            miso_ctrl( pMisoWebrtc,
                       MMIC_CMD_SET_SECURE_CONTEXT,
                       1,
                       0 );
            #endif
            miso_ctrl( pMisoWebrtc,
                       MMIC_CMD_ADD_INPUT0,
                       ( uint32_t )pVideoContext,
                       0 );
            #if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
            miso_ctrl( pMisoWebrtc,
                       MMIC_CMD_ADD_INPUT1,
                       ( uint32_t )pG711eContext,
                       0 );
            #elif AUDIO_OPUS
            miso_ctrl( pMisoWebrtc,
                       MMIC_CMD_ADD_INPUT1,
                       ( uint32_t )pOpuscContext,
                       0 );
            #endif
            miso_ctrl( pMisoWebrtc,
                       MMIC_CMD_ADD_OUTPUT,
                       ( uint32_t )pWebrtcMmContext,
                       0 );
            miso_start( pMisoWebrtc );
        }
        else
        {
            LogError( ( "pMisoWebrtc open fail" ) );
            ret = -1;
        }
    }

    #if MEDIA_PORT_ENABLE_AUDIO_RECV
    if( ret == 0 )
    {
        LogInfo(("MMF bidirectional audio system initialized successfully"));
        LogInfo(("Capture: Audio -> Encoder -> WebRTC"));
        LogInfo(("Playback: Array -> Decoder -> Audio"));
        LogInfo(("Ready for dynamic audio frame injection"));
    }
    #endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */

    return ret;
}

int32_t AppMediaSourcePort_Start( OnFrameReadyToSend_t onVideoFrameReadyToSendFunc,
                                  void * pOnVideoFrameReadyToSendCustomContext,
                                  OnFrameReadyToSend_t onAudioFrameReadyToSendFunc,
                                  void * pOnAudioFrameReadyToSendCustomContext )
{
    int32_t ret = 0;

    #if METRIC_PRINT_ENABLED
    Metric_StartEvent( METRIC_EVENT_MEDIA_PORT_START );
    #endif
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_REG_VIDEO_SEND_CALLBACK,
                    ( int ) onVideoFrameReadyToSendFunc );
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_REG_VIDEO_SEND_CALLBACK_CUSTOM_CONTEXT,
                    ( int ) pOnVideoFrameReadyToSendCustomContext );
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_REG_AUDIO_SEND_CALLBACK,
                    ( int ) onAudioFrameReadyToSendFunc );
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_REG_AUDIO_SEND_CALLBACK_CUSTOM_CONTEXT,
                    ( int ) pOnAudioFrameReadyToSendCustomContext );
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_START,
                    0 );
    #if METRIC_PRINT_ENABLED
    Metric_EndEvent( METRIC_EVENT_MEDIA_PORT_START );
    #endif

    return ret;
}

void AppMediaSourcePort_Stop( void )
{
    #if METRIC_PRINT_ENABLED
    Metric_StartEvent( METRIC_EVENT_MEDIA_PORT_STOP );
    #endif
    mm_module_ctrl( pWebrtcMmContext,
                    CMD_KVS_WEBRTC_STOP,
                    0 );
    #if METRIC_PRINT_ENABLED
    Metric_EndEvent( METRIC_EVENT_MEDIA_PORT_STOP );
    #endif
}


