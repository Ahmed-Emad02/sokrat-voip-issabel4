#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/format.h"
#include "asterisk/framehook.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/frame.h"
#include "asterisk/utils.h"

#define CARRIER_INTERVAL_US 20000
#define CARRIER_IDLE_MS 30

static const char app_name[] = "SokratChanSpy";
static const char carrier_source[] = "SokratChanSpyCarrier";

struct media_carrier {
    struct ast_channel *chan;
    struct ast_format old_write_format;
    enum ast_format_id slin_format;
    unsigned int samples;
    struct timeval last_media_write;
    ast_mutex_t lock;
    pthread_t thread;
    int framehook_id;
    int stop;
    int thread_started;
    int framehook_detached;
};

static void stop_media_carrier(struct media_carrier *carrier);

static void build_silence_frame(struct media_carrier *carrier, struct ast_frame *frame, short *samples)
{
    memset(samples, 0, carrier->samples * sizeof(*samples));
    memset(frame, 0, sizeof(*frame));
    frame->frametype = AST_FRAME_VOICE;
    frame->data.ptr = samples;
    frame->samples = carrier->samples;
    frame->datalen = carrier->samples * sizeof(*samples);
    frame->src = carrier_source;
    ast_format_set(&frame->subclass.format, carrier->slin_format, 0);
}

static struct ast_frame *observe_media_write(
    struct ast_channel *chan,
    struct ast_frame *frame,
    enum ast_framehook_event event,
    void *opaque)
{
    struct media_carrier *carrier = opaque;

    (void) chan;
    if (event == AST_FRAMEHOOK_EVENT_WRITE && frame && frame->frametype == AST_FRAME_VOICE
        && (!frame->src || strcmp(frame->src, carrier_source))) {
        ast_mutex_lock(&carrier->lock);
        carrier->last_media_write = ast_tvnow();
        ast_mutex_unlock(&carrier->lock);
    }

    return frame;
}

static void mark_media_carrier_detached(void *opaque)
{
    struct media_carrier *carrier = opaque;

    ast_mutex_lock(&carrier->lock);
    carrier->framehook_detached = 1;
    ast_mutex_unlock(&carrier->lock);
}

static void *run_media_carrier(void *opaque)
{
    struct media_carrier *carrier = opaque;

    for (;;) {
        struct timeval last_media_write;
        struct ast_frame frame;
        short samples[carrier->samples];
        int stop;

        usleep(CARRIER_INTERVAL_US);

        ast_mutex_lock(&carrier->lock);
        stop = carrier->stop;
        last_media_write = carrier->last_media_write;
        ast_mutex_unlock(&carrier->lock);

        if (stop || ast_check_hangup(carrier->chan)) {
            break;
        }
        if (ast_tvdiff_ms(ast_tvnow(), last_media_write) < CARRIER_IDLE_MS) {
            continue;
        }

        build_silence_frame(carrier, &frame, samples);
        if (ast_write(carrier->chan, &frame)) {
            break;
        }
    }

    return NULL;
}

static int detach_media_carrier(struct media_carrier *carrier)
{
    struct ast_frame *frame = &ast_null_frame;
    int detached;

    ast_channel_lock(carrier->chan);
    if (ast_channel_framehooks(carrier->chan)
        && !ast_framehook_detach(carrier->chan, carrier->framehook_id)) {
        frame = ast_framehook_list_read_event(
            ast_channel_framehooks(carrier->chan),
            &ast_null_frame);
    }
    ast_channel_unlock(carrier->chan);
    (void) frame;

    ast_mutex_lock(&carrier->lock);
    detached = carrier->framehook_detached;
    ast_mutex_unlock(&carrier->lock);

    return detached ? 0 : -1;
}

static struct media_carrier *start_media_carrier(struct ast_channel *chan)
{
    struct ast_framehook_interface framehook = {
        .version = AST_FRAMEHOOK_INTERFACE_VERSION,
        .event_cb = observe_media_write,
        .destroy_cb = mark_media_carrier_detached,
    };
    struct media_carrier *carrier;
    int sample_rate;

    ast_channel_lock(chan);
    if (ast_channel_generatordata(chan)) {
        ast_channel_unlock(chan);
        return NULL;
    }
    ast_channel_unlock(chan);

    carrier = ast_calloc(1, sizeof(*carrier));
    if (!carrier) {
        return NULL;
    }

    carrier->chan = ast_channel_ref(chan);
    carrier->framehook_id = -1;
    carrier->last_media_write = ast_tvnow();
    ast_mutex_init(&carrier->lock);

    ast_channel_lock(chan);
    ast_format_copy(&carrier->old_write_format, ast_channel_writeformat(chan));
    ast_channel_unlock(chan);

    sample_rate = ast_format_rate(&carrier->old_write_format);
    if (sample_rate <= 0) {
        sample_rate = 8000;
    }
    carrier->slin_format = ast_format_slin_by_rate(sample_rate);
    carrier->samples = sample_rate / 50;
    if (!carrier->samples || ast_set_write_format_by_id(chan, carrier->slin_format) < 0) {
        ast_channel_unref(carrier->chan);
        ast_mutex_destroy(&carrier->lock);
        ast_free(carrier);
        return NULL;
    }

    framehook.data = carrier;
    ast_channel_lock(chan);
    carrier->framehook_id = ast_framehook_attach(chan, &framehook);
    ast_channel_unlock(chan);
    if (carrier->framehook_id < 0) {
        ast_set_write_format(chan, &carrier->old_write_format);
        ast_channel_unref(carrier->chan);
        ast_mutex_destroy(&carrier->lock);
        ast_free(carrier);
        return NULL;
    }

    if (ast_pthread_create_background(&carrier->thread, NULL, run_media_carrier, carrier)) {
        stop_media_carrier(carrier);
        return NULL;
    }
    carrier->thread_started = 1;

    return carrier;
}

static void stop_media_carrier(struct media_carrier *carrier)
{
    struct ast_channel *chan;
    struct ast_format old_write_format;

    if (!carrier) {
        return;
    }

    ast_mutex_lock(&carrier->lock);
    carrier->stop = 1;
    ast_mutex_unlock(&carrier->lock);
    if (carrier->thread_started) {
        pthread_join(carrier->thread, NULL);
    }

    chan = carrier->chan;
    ast_format_copy(&old_write_format, &carrier->old_write_format);
    if (detach_media_carrier(carrier)) {
        ast_log(LOG_ERROR,
            "SokratChanSpy could not synchronously detach its media framehook from %s; "
            "retaining carrier state to protect the channel\n",
            ast_channel_name(chan));
        ast_set_write_format(chan, &old_write_format);
        return;
    }

    ast_set_write_format(chan, &old_write_format);
    ast_channel_unref(chan);
    ast_mutex_destroy(&carrier->lock);
    ast_free(carrier);
}

static struct ast_channel *find_target_channel(const char *target_name)
{
    struct ast_channel *target;
    char target_prefix[AST_CHANNEL_NAME];
    size_t target_length = strlen(target_name);

    target = ast_channel_get_by_name(target_name);
    if (target) {
        return target;
    }
    if (target_length + 2 > sizeof(target_prefix)) {
        return NULL;
    }

    snprintf(target_prefix, sizeof(target_prefix), "%s%s",
        target_name,
        target_name[target_length - 1] == '-' ? "" : "-");
    return ast_channel_get_by_name_prefix(target_prefix, strlen(target_prefix));
}

static int sokrat_chanspy_exec(struct ast_channel *chan, const char *data)
{
    struct ast_app *chanspy_app;
    struct ast_channel *target = NULL;
    struct ast_channel *peer = NULL;
    struct media_carrier *target_carrier = NULL;
    struct media_carrier *peer_carrier = NULL;
    struct ast_str *resolved_data = NULL;
    char target_name[AST_CHANNEL_NAME];
    char *parse;
    int result;

    AST_DECLARE_APP_ARGS(args,
        AST_APP_ARG(target);
        AST_APP_ARG(options);
    );

    if (ast_strlen_zero(data)) {
        ast_log(LOG_WARNING, "SokratChanSpy requires target[,options]\n");
        return -1;
    }

    chanspy_app = pbx_findapp("ChanSpy");
    if (!chanspy_app) {
        ast_log(LOG_ERROR, "ChanSpy application is not loaded\n");
        return -1;
    }

    parse = ast_strdupa(data);
    AST_STANDARD_APP_ARGS(args, parse);
    if (ast_strlen_zero(args.target)) {
        ast_log(LOG_WARNING, "SokratChanSpy requires a target channel prefix\n");
        return -1;
    }

    target = find_target_channel(args.target);
    if (!target || target == chan) {
        if (target) {
            ast_channel_unref(target);
        }
        return pbx_exec(chan, chanspy_app, data);
    }

    ast_channel_lock(target);
    ast_copy_string(target_name, ast_channel_name(target), sizeof(target_name));
    if (args.options && strchr(args.options, 'B')) {
        peer = ast_bridged_channel(target);
        if (peer) {
            ast_channel_ref(peer);
        }
    }
    ast_channel_unlock(target);

    target_carrier = start_media_carrier(target);
    if (peer && peer != target) {
        peer_carrier = start_media_carrier(peer);
    }

    resolved_data = ast_str_create(strlen(target_name) + (args.options ? strlen(args.options) : 0) + 2);
    if (resolved_data) {
        ast_str_set(&resolved_data, 0, "%s%s%s", target_name,
            ast_strlen_zero(args.options) ? "" : ",",
            ast_strlen_zero(args.options) ? "" : args.options);
    }

    result = pbx_exec(chan, chanspy_app, resolved_data ? ast_str_buffer(resolved_data) : data);

    ast_free(resolved_data);
    stop_media_carrier(peer_carrier);
    stop_media_carrier(target_carrier);
    if (peer) {
        ast_channel_unref(peer);
    }
    ast_channel_unref(target);

    return result;
}

static int unload_module(void)
{
    return ast_unregister_application(app_name);
}

static int load_module(void)
{
    return ast_register_application(app_name, sokrat_chanspy_exec,
        "ChanSpy with idle-media carriers for reliable whisper and barge audio",
        "SokratChanSpy(channel-prefix[,options]) runs ChanSpy while supplying silent "
        "media only when the target stream is idle, allowing Asterisk 11 audiohooks "
        "to deliver whisper and barge audio without replacing active call media.");
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY,
    "Sokrat reliable ChanSpy wrapper");
