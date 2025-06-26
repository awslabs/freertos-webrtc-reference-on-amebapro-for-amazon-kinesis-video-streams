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
#include "opus_defines.h"

#include "avcodec.h"

#include "video_api.h"

#include "FreeRTOS.h"
#include "networking_utils.h"
#include "queue.h"
#include "semphr.h"

#if METRIC_PRINT_ENABLED
#include "metric.h"
#endif

/* Audio sending is always enabled, audio receiving now using direct audio API */
#define MEDIA_PORT_ENABLE_AUDIO_RECV ( 1 )

/* Audio playback using direct audio API - the correct approach for speaker output */
#if MEDIA_PORT_ENABLE_AUDIO_RECV

#ifdef AUDIO_OPUS
#include "opus.h"
static OpusDecoder *g_opus_decoder = NULL;
#endif

#include "audio_api.h"

/* Audio DMA page size - same as used in audio examples */
#define AUDIO_DMA_PAGE_SIZE  2048

/* Audio playback globals - using direct audio hardware control */
static audio_t g_playback_audio;
static xQueueHandle audio_tx_pcm_queue = NULL;
static uint32_t audio_tx_pcm_cache_len = 0;
static int16_t audio_tx_pcm_cache[AUDIO_DMA_PAGE_SIZE / 2];  // Use standard DMA page size
static uint8_t g_audio_playback_initialized = 0;

/* DMA buffers for audio playback */
#define AUDIO_DMA_PAGE_NUM   AUDIO_PNUM_4
static uint8_t dma_txdata[AUDIO_DMA_PAGE_SIZE * AUDIO_DMA_PAGE_NUM];
static uint8_t dma_rxdata[AUDIO_DMA_PAGE_SIZE * AUDIO_DMA_PAGE_NUM];

/* Audio DMA interrupt handlers for playback */
static void audio_tx_complete(uint32_t arg, uint8_t *pbuf)
{
    uint8_t *ptx_buf;
    static int dma_count = 0;
    
    ptx_buf = (uint8_t *)audio_get_tx_page_adr(&g_playback_audio);
    if (xQueueReceiveFromISR(audio_tx_pcm_queue, ptx_buf, NULL) != pdPASS) {
        memset(ptx_buf, 0, AUDIO_DMA_PAGE_SIZE);
    } else {
        dma_count++;
        if (dma_count % 100 == 0) {
            LogInfo(("Audio DMA playing data"));
        }
    }
    audio_set_tx_page(&g_playback_audio, (uint8_t *)ptx_buf);
}

static void audio_rx_complete(uint32_t arg, uint8_t *pbuf)
{
    audio_t *obj = (audio_t *)arg;
    audio_set_rx_page(obj);
}

static int32_t initialize_audio_hardware(uint32_t sample_rate)
{
    uint8_t smpl_rate_idx;
    
    /* Convert sample rate to audio API format */
    switch (sample_rate) {
    case 8000:
        smpl_rate_idx = ASR_8KHZ;
        break;
    case 16000:
        smpl_rate_idx = ASR_16KHZ;
        break;
    case 32000:
        smpl_rate_idx = ASR_32KHZ;
        break;
    case 44100:
        smpl_rate_idx = ASR_44p1KHZ;
        break;
    case 48000:
        smpl_rate_idx = ASR_48KHZ;
        break;
    default:
        LogWarn(("Unsupported sample rate, using 8kHz"));
        smpl_rate_idx = ASR_8KHZ;
        break;
    }
    
    /* Initialize audio hardware for playback - use capless mode to avoid conflict with microphone */
    audio_init(&g_playback_audio, OUTPUT_CAPLESS, INPUT_DISABLE, AUDIO_CODEC_2p8V);
    audio_dac_digital_vol(&g_playback_audio, 0xEF);  // High volume but not maximum to avoid conflicts
    audio_hpo_amplifier(&g_playback_audio, 1);  // Enable headphone amplifier for better output
    
    /* Set up DMA buffer */
    audio_set_dma_buffer(&g_playback_audio, dma_txdata, dma_rxdata, AUDIO_DMA_PAGE_SIZE, AUDIO_DMA_PAGE_NUM);
    
    /* Set up interrupt handlers */
    audio_tx_irq_handler(&g_playback_audio, (audio_irq_handler)audio_tx_complete, (uint32_t *)&g_playback_audio);
    audio_rx_irq_handler(&g_playback_audio, (audio_irq_handler)audio_rx_complete, (uint32_t *)&g_playback_audio);
    
    /* Set audio parameters */
    audio_set_param(&g_playback_audio, smpl_rate_idx, WL_16BIT);
    
    /* Create PCM queue for buffering */
    audio_tx_pcm_queue = xQueueCreate(10, AUDIO_DMA_PAGE_SIZE);
    if (!audio_tx_pcm_queue) {
        LogError(("Failed to create audio playback queue"));
        return -1;
    }
    
    /* Initialize DMA pages */
    for (int i = 0; i < (AUDIO_DMA_PAGE_NUM - 1); i++) {
        uint8_t *ptx_buf = audio_get_tx_page_adr(&g_playback_audio);
        if (ptx_buf) {
            memset(ptx_buf, 0x0, AUDIO_DMA_PAGE_SIZE);
            audio_set_tx_page(&g_playback_audio, ptx_buf);
        }
        audio_set_rx_page(&g_playback_audio);
    }
    
    /* Start audio playback */
    audio_trx_start(&g_playback_audio);
    
    g_audio_playback_initialized = 1;
    LogInfo(("Audio hardware initialized for playback alongside microphone"));
    
    /* Generate a test tone to verify audio output is working */
    int16_t test_tone[AUDIO_DMA_PAGE_SIZE / 2];
    for (int i = 0; i < AUDIO_DMA_PAGE_SIZE / 2; i++) {
        test_tone[i] = (int16_t)(sin(2.0 * 3.14159 * 440.0 * i / 8000.0) * 8000); // 440Hz tone
    }
    
    /* Queue a few test tone buffers */
    for (int i = 0; i < 3; i++) {
        xQueueSend(audio_tx_pcm_queue, test_tone, 0);
    }
    LogInfo(("Test tone queued for audio verification"));
    
    return 0;
}

static int32_t initialize_audio_playback(void)
{
#ifdef AUDIO_OPUS
    int error;
    g_opus_decoder = opus_decoder_create(8000, 1, &error);
    if (error != OPUS_OK) {
        LogError(("Failed to create Opus decoder"));
        g_opus_decoder = NULL;
        return -1;
    }
    LogInfo(("Opus decoder initialized for audio playback"));
#endif
    
    /* Initialize audio hardware for 8kHz playback */
    return initialize_audio_hardware(8000);
}

/* Function to queue PCM data for playback */
static void queue_pcm_for_playback(int16_t *pcm_data, size_t samples)
{
    static int queue_count = 0;
    
    if (!g_audio_playback_initialized || !audio_tx_pcm_queue) {
        LogWarn(("Audio not initialized or queue missing"));
        return;
    }
    
    /* Copy PCM data to cache buffer */
    size_t max_samples = sizeof(audio_tx_pcm_cache) / sizeof(int16_t);
    size_t samples_to_copy = (samples > max_samples) ? max_samples : samples;
    
    for (size_t i = 0; i < samples_to_copy; i++) {
        /* Amplify quiet audio signals by 8x to make them more audible */
        int32_t amplified = (int32_t)pcm_data[i] * 8;
        
        /* Add some noise gate to reduce background noise */
        if (amplified > -100 && amplified < 100) {
            amplified = 0; /* Suppress very quiet noise */
        }
        
        /* Clamp to prevent overflow */
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        
        audio_tx_pcm_cache[audio_tx_pcm_cache_len++] = (int16_t)amplified;
        if (audio_tx_pcm_cache_len == AUDIO_DMA_PAGE_SIZE / 2) {
            if (xQueueSend(audio_tx_pcm_queue, audio_tx_pcm_cache, 0) == pdPASS) {
                queue_count++;
                if (queue_count % 50 == 0) {
                    LogInfo(("Queued %d audio buffers for playback", queue_count));
                }
            } else {
                LogWarn(("Failed to queue audio buffer - queue full"));
            }
            audio_tx_pcm_cache_len = 0;
        }
    }
}

int32_t AppMediaSourcePort_PlayAudioFrame( uint8_t *pData, size_t dataLen )
{
    static int frame_count = 0;
    static int16_t decode_buffer[2048]; // Static buffer for decoding
    int samples_decoded = 0;
    
    frame_count++;
    
    /* Initialize audio playback on first frame */
    if (!g_audio_playback_initialized) {
        if (initialize_audio_playback() != 0) {
            LogError(("Failed to initialize audio playback"));
            return -1;
        }
    }
    
#if AUDIO_OPUS
    if (g_opus_decoder == NULL) {
        LogError(("Opus decoder not initialized"));
        return -1;
    }
    
    samples_decoded = opus_decode(g_opus_decoder, pData, dataLen, decode_buffer, 2048, 0);
    
    if (samples_decoded > 0) {
        /* Queue PCM data for playback */
        queue_pcm_for_playback(decode_buffer, samples_decoded);
        
        if (frame_count % 50 == 0) { // Log every 50th frame to avoid spam
            LogInfo(("Opus: decoded %d samples from %d bytes", samples_decoded, dataLen));
            /* Log a few sample values to verify data */
            LogInfo(("Sample values: %d, %d, %d, %d", 
                    decode_buffer[0], decode_buffer[1], 
                    decode_buffer[samples_decoded/2], decode_buffer[samples_decoded-1]));
            /* Show amplified values too */
            LogInfo(("Amplified values: %d, %d, %d, %d", 
                    (int)(decode_buffer[0] * 8), (int)(decode_buffer[1] * 8),
                    (int)(decode_buffer[samples_decoded/2] * 8), (int)(decode_buffer[samples_decoded-1] * 8)));
        }
    } else {
        LogWarn(("Opus decode failed with error: %d", samples_decoded));
        return -1;
    }
    
#elif AUDIO_G711_MULAW || AUDIO_G711_ALAW
    /* G.711 decoding - data is already in the right format, just need to convert from 8-bit to 16-bit */
    if (dataLen > 2048) {
        LogWarn(("G.711 frame too large"));
        dataLen = 2048;
    }
    
    /* Convert G.711 8-bit samples to 16-bit PCM */
    for (size_t i = 0; i < dataLen; i++) {
#if AUDIO_G711_MULAW
        /* μ-law to linear PCM conversion */
        uint8_t ulaw = pData[i];
        int16_t linear;
        
        /* Simple μ-law to linear conversion */
        ulaw = ~ulaw;
        int sign = (ulaw & 0x80) ? -1 : 1;
        int exponent = (ulaw >> 4) & 0x07;
        int mantissa = ulaw & 0x0F;
        
        if (exponent == 0) {
            linear = (mantissa << 2) + 0x84;
        } else {
            linear = ((mantissa << 1) + 0x21) << (exponent + 2);
        }
        
        decode_buffer[i] = sign * linear;
        
#elif AUDIO_G711_ALAW
        /* A-law to linear PCM conversion */
        uint8_t alaw = pData[i];
        int16_t linear;
        
        /* Simple A-law to linear conversion */
        alaw ^= 0x55;
        int sign = (alaw & 0x80) ? -1 : 1;
        int exponent = (alaw >> 4) & 0x07;
        int mantissa = alaw & 0x0F;
        
        if (exponent == 0) {
            linear = (mantissa << 1) + 1;
        } else {
            linear = ((mantissa << 1) + 0x21) << (exponent - 1);
        }
        
        decode_buffer[i] = sign * linear;
#endif
    }
    
    samples_decoded = dataLen;
    
    /* Queue PCM data for playback */
    queue_pcm_for_playback(decode_buffer, samples_decoded);
    
    if (frame_count % 50 == 0) { // Log every 50th frame to avoid spam
        LogInfo(("G.711: Successfully played audio samples"));
    }
    
#else
    LogWarn(("No audio decoder available - check demo_config.h audio format settings"));
    return -1;
#endif
    
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
static mm_context_t * pWebrtcMmContext = NULL;

static mm_siso_t * pSisoAudioA1 = NULL;
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
    .mic_gain = MIC_40DB,  // Maximum mic gain for better pickup
    .dmic_l_gain = DMIC_BOOST_24DB,
    .dmic_r_gain = DMIC_BOOST_24DB,
    .use_mic_type = USE_AUDIO_AMIC,
    .channel = 1,
    .mix_mode = 0,
    .enable_aec = 0  // Keep AEC disabled to avoid conflicts with separate playback audio
};
#endif

#if ( AUDIO_G711_MULAW || AUDIO_G711_ALAW )
static g711_params_t g711eParams = {
    .codec_id = AV_CODEC_ID_PCMU,
    .buf_len = 2048,
    .mode = G711_ENCODE
};

#if MEDIA_PORT_ENABLE_AUDIO_RECV
static g711_params_t g711dParams __attribute__((unused)) = {
    .codec_id = AV_CODEC_ID_PCMU,
    .buf_len = 2048,
    .mode = G711_DECODE
};
#endif /* MEDIA_PORT_ENABLE_AUDIO_RECV */
#endif /* ( AUDIO_G711_MULAW || AUDIO_G711_ALAW ) */

#if ( AUDIO_OPUS )
static opusc_params_t opuscParams = {
    .sample_rate = 8000, // 16000
    .channel = 1,
    .bit_length = 16,    // 16 recommand
    .complexity = 3,     // Reduce complexity to speed up processing
    .bitrate = 25000,    // default 25000
    .use_framesize = 40, // Back to 40ms to prevent buffer overflow
    .enable_vbr = 1,
    .vbr_constraint = 0,
    .packetLossPercentage = 0,
    .opus_application = OPUS_APPLICATION_VOIP  // Use VOIP mode for better real-time performance
};


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

    ( void ) output;

    if( pCtx->mediaStart != 0 )
    {
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
                if( pCtx->onAudioFrameReadyToSendFunc )
                {
                    frame.trackKind = TRANSCEIVER_TRACK_KIND_AUDIO;
                    ( void ) pCtx->onAudioFrameReadyToSendFunc( pCtx->pOnAudioFrameReadyToSendCustomContext,
                                                                &frame );
                    /* Add periodic logging to verify audio is being sent */
                    static int audio_send_count = 0;
                    audio_send_count++;
                    if (audio_send_count % 50 == 0) {
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
                LogWarn( ( "Input type cannot be handled" ) );
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

    // Delete linkers
    pSisoAudioA1 = siso_delete( pSisoAudioA1 );
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
                            3 );
            mm_module_ctrl( pAudioContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_APPLY,
                            0 );
            /* Ensure audio system is started for microphone capture */
            mm_module_ctrl( pAudioContext,
                            CMD_AUDIO_SET_TRX,
                            1 );
            LogInfo(("Audio microphone system started"));
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
            LogError( ( "G711 open fail" ) );
            ret = -1;
        }
        #elif AUDIO_OPUS
        pOpuscContext = mm_module_open( &opusc_module );
        if( pOpuscContext )
        {
            mm_module_ctrl( pOpuscContext,
                            CMD_OPUSC_SET_PARAMS,
                            ( int )&opuscParams );
            mm_module_ctrl( pOpuscContext,
                            MM_CMD_SET_QUEUE_LEN,
                            2 );
            mm_module_ctrl( pOpuscContext,
                            MM_CMD_INIT_QUEUE_ITEMS,
                            MMQI_FLAG_STATIC );
            mm_module_ctrl( pOpuscContext,
                            CMD_OPUSC_APPLY,
                            0 );
        }
        else
        {
            LogError( ( "OPUSC open fail" ) );
            ret = -1;
        }
        #endif
    }

    if( ret == 0 )
    {
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
        }
        else
        {
            LogError( ( "pSisoAudioA1 open fail" ) );
            ret = -1;
        }
    }

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
        /* Don't initialize audio playback here - delay until first frame to avoid conflicts */
        LogInfo(("Audio playback will be initialized on first received frame"));
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


