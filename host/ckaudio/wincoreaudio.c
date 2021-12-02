#include "ckaudio/core.h"
#include "context.h"
#include "queue.h"

#define NOMIXMAX
#include <AudioClient.h>
#include <AudioPolicy.h>
#include <MMDeviceAPI.h>
#include <Mmreg.h>

#include <functiondiscoverykeys.h>
#include <initguid.h>
#include <stringapiset.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define CKAUDIO_MESSAGES_LIMIT 16
#define CKAUDIO_MESSAGE_TYPE_LIMIT 3

static CKAudioMessage s_audio_messages[CKAUDIO_MESSAGES_LIMIT];
static unsigned s_audio_message_head = 0;
static unsigned s_audio_message_tail = 0;

typedef enum {
    kCKAudioMessageTypeInfo,
    kCKAudioMessageTypeWarn,
    kCKAudioMessageTypeFail
} CKAudioMessageType;

static void *allocateLocal(void *ctx, size_t amt) { return malloc(amt); }

static void freeLocal(void *ctx, void *p) { free(p); }

CKAudioContext g_ckaudio_context;

void ckaudio_message(CKAudioMessageType type, const char *module,
                     const char *fmt, ...) {
    static const char *type_strings[CKAUDIO_MESSAGE_TYPE_LIMIT] = {
        "INFO", "WARN", "FAIL"};
    CKAudioMessage *msg;
    unsigned next_message_tail;
    size_t offset;
    va_list args;

    next_message_tail = (s_audio_message_tail + 1) % CKAUDIO_MESSAGES_LIMIT;
    if (next_message_tail == s_audio_message_head) {
        s_audio_message_head =
            (s_audio_message_head + 1) % CKAUDIO_MESSAGES_LIMIT;
    }
    msg = &s_audio_messages[s_audio_message_tail];
    s_audio_message_tail = (s_audio_message_tail + 1) % CKAUDIO_MESSAGES_LIMIT;

    va_start(args, fmt);
    offset = snprintf(
        msg->text, sizeof(msg->text),
        "%6.3lf [%s] %s: ", g_ckaudio_context.mix_timestamp_ms * 0.001,
        type_strings[(int)(type) % CKAUDIO_MESSAGE_TYPE_LIMIT], module);
    vsnprintf(msg->text + offset, sizeof(msg->text) - offset, fmt, args);
    va_end(args);
}

////////////////////////////////////////////////////////////////////////////////
#pragma comment(lib, "ksuser.lib")
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D,
            0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46,
            0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2,
            0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF,
            0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);

#define CKAUDIO_WIN32_DESIRED_LATENCY_MS 50
#define CKAUDIO_API_WORKER_EVENT_LIMIT 16

enum CKAudioWorkerStatus {
    kCKAudioWorkerStatusInactive,
    kCKAudioWorkerStatusOk,
    kCKAudioWorkerStatusInitFailed,
    kCKAudioWorkerStatusSystemFailure
};

/* The CKAudioWorker takes care of all platform specific operation on a worker
   thread or equivalent.

   Though CKAudioWorker implementation is specific to the platform, the worker
   often needs some basic information from the cross-platform layer, like
   threading objects and the mix buffer.   These items are called out below
   in a null CKAudioWorker implementation
*/
struct CKAudioWorkerQueue {
    struct CKAudioQueue container;
    HANDLE ready_event;
    CRITICAL_SECTION crit_sec;
};

bool ckaudio_worker_queue_init(struct CKAudioWorkerQueue *queue,
                               unsigned queue_limit) {
    CKAudioDataFormat queue_format;
    queue->container.buffer = ckaudio_buffer_create(
        0, ckaudio_queue_init_data_format(&queue_format, queue_limit,
                                          sizeof(struct CKAudioEvent)));
    InitializeCriticalSection(&queue->crit_sec);
    queue->ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    return true;
}

void ckaudio_worker_queue_term(struct CKAudioWorkerQueue *queue) {
    CloseHandle(queue->ready_event);
    DeleteCriticalSection(&queue->crit_sec);
    ckaudio_buffer_release(queue->container.buffer);
}

void ckaudio_int_worker_queue_event(struct CKAudioWorkerQueue *queue,
                                    struct CKAudioEvent *event) {
    struct CKAudioQueue *container = &queue->container;
    struct CKAudioQueueWindow write_window;
    uint32_t frame_count;
    EnterCriticalSection(&queue->crit_sec);
    frame_count = ckaudio_queue_write_window(&write_window, &queue->container);
    if (frame_count > 0) {
        memcpy(ckaudio_buffer_get_typed_frame(
                   container->buffer, struct CKAudioEvent, write_window.start),
               event, sizeof(struct CKAudioEvent));
        ckaudio_queue_write(container, 1);
    }
    LeaveCriticalSection(&queue->crit_sec);
    SetEvent(queue->ready_event);
}

void ckaudio_worker_queue_pull_events(
    struct CKAudioWorkerQueue *queue,
    bool (*handler_callback)(struct CKAudioEvent *, void *),
    void *handler_context) {
    struct CKAudioQueueWindow read_window;
    struct CKAudioEvent *event;
    uint32_t frame_count;
    uint32_t index;

    EnterCriticalSection(&queue->crit_sec);

    frame_count = ckaudio_queue_read_window(&read_window, &queue->container);

    for (index = 0; index < frame_count; ++index) {
        event = ckaudio_buffer_get_typed_frame(
            read_window.buffer, struct CKAudioEvent, index + read_window.start);
        if (!(*handler_callback)(event, handler_context)) {
            break;
        }
    }
    ckaudio_queue_read(&queue->container, frame_count);
    ckaudio_queue_finish(&queue->container);

    LeaveCriticalSection(&queue->crit_sec);
}

struct CKAudioWorker {
    /* These are the shared attributes */
    CKAudioReadyCallback ready_callback;
    void *ready_callback_ctx_ptr;
    enum CKAudioWorkerStatus status;
    CKAudioDataFormat device_format;

    struct CKAudioWorkerQueue event_queue;
    struct CKAudioWorkerQueue notify_queue;

    /* These are the platform-specific attributes.  Perhaps a C like pattern
       of using a CKAudioWin32Worker struct with a CKAudioWorker platform
       agnostic member as the first member. */
    HANDLE thread_handle;
    CRITICAL_SECTION render_crit_sec;
};

static struct CKAudioWorker s_audio_worker;

static IMMDevice *ckaudio_find_device(IMMDeviceEnumerator *enumerator) {
    IMMDevice *device;
    IPropertyStore *properties;
    PROPVARIANT property_value;
    char name[64];

    enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole,
                                                &device);
    device->lpVtbl->OpenPropertyStore(device, STGM_READ, &properties);
    PropVariantInit(&property_value);
    properties->lpVtbl->GetValue(properties, &PKEY_Device_FriendlyName,
                                 &property_value);
    properties->lpVtbl->Release(properties);
    WideCharToMultiByte(CP_UTF8, 0, property_value.pwszVal, -1, name,
                        sizeof(name), NULL, NULL);
    ckaudio_message(kCKAudioMessageTypeInfo, "ckaudio_find_device",
                    "endpoint is %s", name);
    return device;
}

static void ckaudio_win32_log_waveformatex(WAVEFORMATEX *fmt,
                                           const char *module) {
    ckaudio_message(kCKAudioMessageTypeInfo, module, "channels:     %u",
                    fmt->nChannels);
    ckaudio_message(kCKAudioMessageTypeInfo, module, "frequency:    %u",
                    fmt->nSamplesPerSec);
    ckaudio_message(kCKAudioMessageTypeInfo, module, "bps:          %u",
                    fmt->wBitsPerSample);
    ckaudio_message(kCKAudioMessageTypeInfo, module, "block align:  %u",
                    fmt->nBlockAlign);
    ckaudio_message(kCKAudioMessageTypeInfo, module, "format tag:   %u",
                    fmt->wFormatTag);

    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *ext_fmt = (WAVEFORMATEXTENSIBLE *)fmt;
        printf("Endpoint Mix Channel Mask:  %u\n", ext_fmt->dwChannelMask);
        printf("Endpoint Mix Valid BPS:     %u\n",
               ext_fmt->Samples.wValidBitsPerSample);
        if (IsEqualGUID(&ext_fmt->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)) {
            printf("Endpoint Mix Subformat:     PCM\n");
        } else if (IsEqualGUID(&ext_fmt->SubFormat,
                               &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            printf("Endpoint Mix Subformat:     float\n");
        } else {
            printf("Endpoint Mix Subformat:     unsupported\n");
        }
    } else if (fmt->wFormatTag == WAVE_FORMAT_PCM) {
        printf("Endpoint Format:          PCM\n");
    } else if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        printf("Endpoint Format:          float\n");
    } else {
        printf("Endpoint Format:          unsupported\n");
    }
}

/*
 *  The Audio Worker Interface handles interation between the platform agnostic
 *  mixer and the platform specific worker.
 *  - Start
 *  - Push/Send Event
 *  - Acquire Write Window
 *  - Stop
 */

enum CKAudioWorkerWin32State {
    kCKAudioWin32WorkerFindDeviceClient,
    kCKAudioWin32WorkerInitDeviceClient,
    kCKAudioWin32WorkerStartDeviceClient,
    kCKAudioWin32WorkerDeviceClientReady,
    kCKAudioWin32WorkerDeviceClientFailed,
    kCKAudioWin32WorkerStopDeviceClient
};

static bool ckaudio_win32_worker_render(struct CKAudioWorker *worker,
                                        CKAudioDataFormat *data_format,
                                        IAudioClient *client,
                                        IAudioRenderClient *render_client,
                                        uint32_t render_buffer_size) {
    CKAudioBuffer audio_buffer;
    CKAudioTimePoint timepoint;
    uint32_t queued_frame_count;
    uint32_t avail_out_frame_count;
    BYTE *data = NULL;
    uint32_t xfer_frame_count = 0;
    uint32_t frames_output;

    HRESULT hr;
    hr = client->lpVtbl->GetCurrentPadding(client, &queued_frame_count);
    if (FAILED(hr)) {
        return false;
    }
    avail_out_frame_count = render_buffer_size - queued_frame_count;
    frames_output = 0;
    /*
        if (audioFrameCount_ == 0) {
        audioAvailCounter_ = 0;
        if (audioStarvedCounter_ == 0) {
            printf("audio: starved!\n");
        }
        ++audioStarvedCounter_;
        } else {
        audioStarvedCounter_ = 0;
        if (audioAvailCounter_ == 0) {
            printf("audio: buffered\n");
        }
        ++audioAvailCounter_;
        }
    */
    while (avail_out_frame_count > 0) {
        ckaudio_timepoint_now(&timepoint);

        hr = render_client->lpVtbl->GetBuffer(render_client,
                                              avail_out_frame_count, &data);
        if (FAILED(hr)) {
            printf("FAILED! (%08x)\n", hr);
            break;
        }

        if (worker->ready_callback) {
            audio_buffer.data = data;
            audio_buffer.frame_limit = avail_out_frame_count;
            audio_buffer.ref_count = 0;
            memcpy(&audio_buffer.data_format, data_format,
                   sizeof(CKAudioDataFormat));
            xfer_frame_count = (*worker->ready_callback)(
                &audio_buffer, &timepoint, worker->ready_callback_ctx_ptr);
        } else {
            xfer_frame_count = avail_out_frame_count;
        }

        render_client->lpVtbl->ReleaseBuffer(render_client, xfer_frame_count,
                                             0);
        client->lpVtbl->GetCurrentPadding(client, &queued_frame_count);
        avail_out_frame_count = render_buffer_size - queued_frame_count;
        frames_output += xfer_frame_count;
    } // end while
    if (avail_out_frame_count > 0) {
        hr = render_client->lpVtbl->GetBuffer(render_client,
                                              avail_out_frame_count, &data);
        if (SUCCEEDED(hr)) {
            render_client->lpVtbl->ReleaseBuffer(render_client,
                                                 avail_out_frame_count,
                                                 AUDCLNT_BUFFERFLAGS_SILENT);
        }
    }

    return true;
}

struct CKAudioWorkerState {
    enum CKAudioWorkerWin32State state;
};

static bool ckaudio_win32_worker_event_callback(struct CKAudioEvent *event,
                                                void *context) {
    struct CKAudioWorkerState *worker_state =
        (struct CKAudioWorkerState *)context;

    if (!strncmp(event->data, "end", sizeof(event->data))) {
        worker_state->state = kCKAudioWin32WorkerStopDeviceClient;
        return false;
    }

    return true;
}

void ckaudio_win32_lock_worker(struct CKAudioWorker *worker) {
    EnterCriticalSection(&worker->render_crit_sec);
}

void ckaudio_win32_unlock_worker(struct CKAudioWorker *worker) {
    LeaveCriticalSection(&worker->render_crit_sec);
}

static DWORD __stdcall ckaudio_win32_worker(LPVOID context_opaque) {
    struct CKAudioWorker *worker = (struct CKAudioWorker *)context_opaque;
    IMMDeviceEnumerator *device_enumerator = NULL;
    IMMDevice *device = NULL;
    IAudioClient *client = NULL;
    IAudioRenderClient *render_client = NULL;
    WAVEFORMATEX *endpoint_format = NULL;
    HANDLE device_ready_event = NULL;
    HANDLE wait_event_handles[2];
    HRESULT hr;

    uint32_t render_buffer_size;
    struct CKAudioWorkerState worker_state = {
        .state = kCKAudioWin32WorkerFindDeviceClient};
    struct CKAudioEvent audio_event;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMMDeviceEnumerator, &device_enumerator);
    if (SUCCEEDED(hr)) {
        device = ckaudio_find_device(device_enumerator);
        if (device) {
            hr = device->lpVtbl->Activate(device, &IID_IAudioClient,
                                          CLSCTX_INPROC_SERVER, NULL, &client);
            if (SUCCEEDED(hr)) {
                worker_state.state = kCKAudioWin32WorkerInitDeviceClient;
            } else {
                // ckaudio_message(kCKAudioMessageTypeFail, "ckaudio_start",
                //                "Activate() failed %08x", hr);
                printf("IAudioClient failed\n");
            }
        } else {
            printf("IMMDevice failure\n");
        }
    } else {
        printf("IMMDeviceEnumerator failure\n");
        // ckaudio_message(kCKAudioMessageTypeFail, "ckaudio_init",
        //                "CoCreateInstance failed %08x", hr);
    }
    if (worker_state.state == kCKAudioWin32WorkerInitDeviceClient) {
        hr = client->lpVtbl->GetMixFormat(client, &endpoint_format);
        if (SUCCEEDED(hr)) {
            WAVEFORMATEXTENSIBLE *ext_fmt;
            ckaudio_win32_log_waveformatex(endpoint_format, "ckaudio_worker");
            worker->device_format.buffer_format = kCKAudioBufferFormatUnknown;
            if (endpoint_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                ext_fmt = (WAVEFORMATEXTENSIBLE *)endpoint_format;
                if (IsEqualGUID(&ext_fmt->SubFormat,
                                &KSDATAFORMAT_SUBTYPE_PCM)) {
                    worker->device_format.buffer_format =
                        kCKAudioBufferFormatPCM;
                } else if (IsEqualGUID(&ext_fmt->SubFormat,
                                       &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                    worker->device_format.buffer_format =
                        kCKAudioBufferFormatFloat;
                }
            } else if (endpoint_format->wFormatTag == WAVE_FORMAT_PCM) {
                worker->device_format.buffer_format = kCKAudioBufferFormatPCM;
            } else if (endpoint_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                worker->device_format.buffer_format = kCKAudioBufferFormatFloat;
            }
            hr = client->lpVtbl->Initialize(client, AUDCLNT_SHAREMODE_SHARED,
                                            /*AUDCLNT_STREAMFLAGS_EVENTCALLBACK | */
                                                AUDCLNT_STREAMFLAGS_NOPERSIST,
                                            CKAUDIO_WIN32_DESIRED_LATENCY_MS *
                                                10000,  /* 100 ns units */
                                            0, endpoint_format, NULL);
            if (SUCCEEDED(hr)) {
                worker->device_format.frame_size = endpoint_format->nBlockAlign;
                worker->device_format.num_channels = endpoint_format->nChannels;
                worker->device_format.frequency =
                    endpoint_format->nSamplesPerSec;
                client->lpVtbl->GetBufferSize(client, &render_buffer_size);
                worker_state.state = kCKAudioWin32WorkerStartDeviceClient;
            }
        }
    }
    if (worker_state.state == kCKAudioWin32WorkerStartDeviceClient) {
        hr = client->lpVtbl->GetService(client, &IID_IAudioRenderClient,
                                        &render_client);
        if (SUCCEEDED(hr)) {
            // preroll silence
            BYTE *data;
            hr = render_client->lpVtbl->GetBuffer(render_client,
                                                  render_buffer_size, &data);
            if (SUCCEEDED(hr)) {
                hr = render_client->lpVtbl->ReleaseBuffer(
                    render_client, render_buffer_size,
                    AUDCLNT_BUFFERFLAGS_SILENT);
            }
            if (SUCCEEDED(hr)) {
                /*
                device_ready_event = CreateEvent(NULL, FALSE, FALSE, NULL);
                if (device_ready_event != NULL) {
                    client->lpVtbl->SetEventHandle(client, device_ready_event);
                    hr = client->lpVtbl->Start(client);
                    if (SUCCEEDED(hr)) {
                        worker_state.state =
                            kCKAudioWin32WorkerDeviceClientReady;
                    }
                }
                */
                hr = client->lpVtbl->Start(client);
                if (SUCCEEDED(hr)) {
                    worker_state.state =
                            kCKAudioWin32WorkerDeviceClientReady;
                }
            }
        }
    }
    wait_event_handles[0] = worker->event_queue.ready_event;
    wait_event_handles[1] = device_ready_event;

    worker->status =
        (worker_state.state == kCKAudioWin32WorkerDeviceClientReady)
            ? kCKAudioWorkerStatusOk
            : kCKAudioWorkerStatusInitFailed;

    if (worker->status == kCKAudioWorkerStatusOk) {
        printf("win32: coreaudio mixer started\n");
        strncpy(audio_event.data, "ready", sizeof(audio_event.data));
        ckaudio_int_worker_queue_event(&worker->notify_queue, &audio_event);
    }
    /* This simple loop performs most of the work on this worker */
    while (worker_state.state == kCKAudioWin32WorkerDeviceClientReady) {
        //hr = WaitForMultipleObjects(2, wait_event_handles, FALSE, 1000);
        hr = WaitForSingleObject(wait_event_handles[0], CKAUDIO_WIN32_DESIRED_LATENCY_MS / 2);
        switch (hr) {
        case WAIT_TIMEOUT:
            ckaudio_win32_lock_worker(worker);
            /* core audio mixer waiting is available for writing */
            if (!ckaudio_win32_worker_render(worker, &worker->device_format,
                                             client, render_client,
                                             render_buffer_size)) {
                worker_state.state = kCKAudioWin32WorkerDeviceClientFailed;
            }
            ckaudio_win32_unlock_worker(worker);
            /* audio is not being processed through the output mixer? */
            break;
        case WAIT_OBJECT_0:
            /* worker event came in */
            ckaudio_worker_queue_pull_events(
                &worker->event_queue, &ckaudio_win32_worker_event_callback,
                &worker_state);
            break;
        case WAIT_OBJECT_0 + 1:
            ckaudio_win32_lock_worker(worker);
            /* core audio mixer waiting is available for writing */
            if (!ckaudio_win32_worker_render(worker, &worker->device_format,
                                             client, render_client,
                                             render_buffer_size)) {
                worker_state.state = kCKAudioWin32WorkerDeviceClientFailed;
            }
            ckaudio_win32_unlock_worker(worker);
            break;
        case WAIT_FAILED:
            worker_state.state = kCKAudioWin32WorkerDeviceClientFailed;
            break;
        }
    }
    if (worker_state.state == kCKAudioWin32WorkerDeviceClientFailed) {
        worker->status = kCKAudioWorkerStatusSystemFailure;
        worker_state.state = kCKAudioWin32WorkerStopDeviceClient;
    }
    if (worker_state.state == kCKAudioWin32WorkerStopDeviceClient) {
        if (render_client) {
            render_client->lpVtbl->Release(render_client);
            render_client = NULL;
        }
        if (client) {
            client->lpVtbl->Stop(client);
            client->lpVtbl->SetEventHandle(client, NULL);
        }
        if (device_ready_event) {
            CloseHandle(device_ready_event);
            device_ready_event = NULL;
        }
    }
    if (client) {
        client->lpVtbl->Release(client);
        client = NULL;
    }
    if (device) {
        device->lpVtbl->Release(device);
        device = NULL;
    }
    if (device_enumerator) {
        device_enumerator->lpVtbl->Release(device_enumerator);
        device_enumerator = NULL;
    }

    CoUninitialize();

    return 0;
}

struct CKAudioNotifyHandler {
    char type[16];
    bool is_ok;
};

static bool _ckaudio_handle_worker_notify(struct CKAudioEvent *event,
                                          void *context) {
    struct CKAudioNotifyHandler *handler =
        (struct CKAudioNotifyHandler *)context;
    if (!strncmp(handler->type, event->data, sizeof(handler->type))) {
        handler->is_ok = true;
    }
    return true;
}

static void ckaudio_int_term_worker(struct CKAudioWorker *worker) {
    CloseHandle(worker->thread_handle);

    ckaudio_worker_queue_term(&worker->notify_queue);
    ckaudio_worker_queue_term(&worker->event_queue);

    DeleteCriticalSection(&worker->render_crit_sec);

    memset(worker, 0, sizeof(*worker));
}

struct CKAudioWorker *
ckaudio_int_start_worker(struct CKAudioWorker *worker,
                         CKAudioReadyCallback ready_callback, void *user_ptr) {
    struct CKAudioNotifyHandler notify_handler;
    HANDLE wait_handles[2];
    DWORD wait_result;

    memset(worker, 0, sizeof(*worker));
    worker->status = kCKAudioWorkerStatusInactive;
    worker->ready_callback = ready_callback;
    worker->ready_callback_ctx_ptr = user_ptr;

    ckaudio_worker_queue_init(&worker->event_queue,
                              CKAUDIO_API_WORKER_EVENT_LIMIT);
    ckaudio_worker_queue_init(&worker->notify_queue,
                              CKAUDIO_API_WORKER_EVENT_LIMIT);
    InitializeCriticalSection(&worker->render_crit_sec);
    worker->thread_handle =
        CreateThread(NULL, 0, ckaudio_win32_worker, worker, 0, NULL);

    SetThreadPriority(worker->thread_handle, THREAD_PRIORITY_HIGHEST);

    strncpy(notify_handler.type, "ready", sizeof(notify_handler.type));

    wait_handles[0] = worker->notify_queue.ready_event;
    wait_handles[1] = worker->thread_handle;
    wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, 10000);
    switch (wait_result) {
    case WAIT_TIMEOUT:
        /* TODO: investigate if this happens */
        TerminateThread(worker->thread_handle, 1);
        ckaudio_int_term_worker(worker);
        break;
    case WAIT_OBJECT_0:
        ckaudio_worker_queue_pull_events(&worker->notify_queue,
                                         &_ckaudio_handle_worker_notify,
                                         &notify_handler);
        break;
    case WAIT_OBJECT_0 + 1:
        ckaudio_int_term_worker(worker);
        break;
    }

    return worker;
}

struct CKAudioWorker *ckaudio_int_stop_worker(struct CKAudioWorker *worker) {
    struct CKAudioEvent event;
    if (!worker) {
        return NULL;
    }

    strncpy(event.data, "end", sizeof(event.data));
    ckaudio_int_worker_queue_event(&worker->event_queue, &event);
    WaitForSingleObject(worker->thread_handle, INFINITE);

    ckaudio_int_term_worker(worker);
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#define CKAUDIO_API_MIX_SAMPLES_RATE 48000
#define CKAUDIO_API_MIX_BUFFER_DURATION_MS 1000
#define CKAUDIO_API_MIX_CHANNELS 2

void ckaudio_init(CKAudioAllocator *allocator) {
    if (allocator != NULL) {
        memcpy(&g_ckaudio_context.allocator, allocator,
               sizeof(g_ckaudio_context.allocator));
    } else {
        g_ckaudio_context.allocator.allocate = &allocateLocal;
        g_ckaudio_context.allocator.free = &freeLocal;
        g_ckaudio_context.allocator.user_ctx = NULL;
    }
}

void ckaudio_term() {
    while (g_ckaudio_context.worker != NULL) {
        g_ckaudio_context.worker =
            ckaudio_int_stop_worker(g_ckaudio_context.worker);
    }
    memset(&g_ckaudio_context, 0, sizeof(g_ckaudio_context));
}

const char *ckaudio_get_message() {
    const CKAudioMessage *message;
    if (s_audio_message_head == s_audio_message_tail) {
        return NULL;
    }
    message = &s_audio_messages[s_audio_message_head];
    s_audio_message_head = (s_audio_message_head + 1) % CKAUDIO_MESSAGES_LIMIT;
    return message->text;
}

void ckaudio_start(CKAudioReadyCallback audio_ready_cb, void *user_ctx) {
    //  TODO: create worker objects
    g_ckaudio_context.worker =
        ckaudio_int_start_worker(&s_audio_worker, audio_ready_cb, user_ctx);
}

void ckaudio_stop() {
    g_ckaudio_context.worker = ckaudio_int_stop_worker(&s_audio_worker);
}

void ckaudio_lock() {
    ckaudio_win32_lock_worker(g_ckaudio_context.worker);
}

void ckaudio_unlock() {
    ckaudio_win32_unlock_worker(g_ckaudio_context.worker);
}

void ckaudio_get_data_format(CKAudioDataFormat *out) {
    memcpy(out, &g_ckaudio_context.worker->device_format, sizeof(*out));
}
