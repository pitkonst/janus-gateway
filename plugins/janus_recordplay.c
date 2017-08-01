/*! \file   janus_recordplay.c
 * \author Lorenzo Miniero <lorenzo@meetecho.com>
 * \copyright GNU General Public License v3
 * \brief  Janus Record&Play plugin
 * \details  This is a simple application that implements two different
 * features: it allows you to record a message you send with WebRTC in
 * the format defined in recorded.c (MJR recording) and subsequently
 * replay this recording (or other previously recorded) through WebRTC
 * as well.
 * 
 * This application aims at showing how easy recording frames sent by
 * a peer is, and how this recording can be re-used directly, without
 * necessarily involving a post-processing process (e.g., through the
 * tool we provide in janus-pp-rec.c).
 * 
 * The configuration process is quite easy: just choose where the
 * recordings should be saved. The same folder will also be used to list
 * the available recordings that can be replayed.
 * 
 * \note The application creates a special file in INI format with
 * \c .nfo extension for each recording that is saved. This is necessary
 * to map a specific audio .mjr file to a different video .mjr one, as
 * they always get saved in different files. If you want to replay
 * recordings you took in a different application (e.g., the streaming
 * or videoroom plugins) just copy the related files in the folder you
 * configured this plugin to use and create a .nfo file in the same
 * folder to create a mapping, e.g.:
 * 
 * 		[12345678]
 * 		name = My videoroom recording
 * 		date = 2014-10-14 17:11:26
 * 		audio = mcu-audio.mjr
 * 		video = mcu-video.mjr
 * 
 * \section recplayapi Record&Play API
 * 
 * The Record&Play API supports several requests, some of which are
 * synchronous and some asynchronous. There are some situations, though,
 * (invalid JSON, invalid request) which will always result in a
 * synchronous error response even for asynchronous requests. 
 * 
 * \c list and \c update are synchronous requests, which means you'll
 * get a response directly within the context of the transaction. \c list
 * lists all the available recordings, while \c update forces the plugin
 * to scan the folder of recordings again in case some were added manually
 * and not indexed in the meanwhile.
 * 
 * The \c record , \c play , \c start and \c stop requests instead are
 * all asynchronous, which means you'll get a notification about their
 * success or failure in an event. \c record asks the plugin to start
 * recording a session; \c play asks the plugin to prepare the playout
 * of one of the previously recorded sessions; \c start starts the
 * actual playout, and \c stop stops whatever the session was for, i.e.,
 * recording or replaying.
 * 
 * The \c list request has to be formatted as follows:
 *
\verbatim
{
	"request" : "list"
}
\endverbatim
 *
 * A successful request will result in an array of recordings:
 * 
\verbatim
{
	"recordplay" : "list",
	"list": [	// Array of recording objects
		{			// Recording #1
			"id": <numeric ID>,
			"name": "<Name of the recording>",
			"date": "<Date of the recording>",
			"audio": "<Audio rec file, if any; optional>",
			"video": "<Video rec file, if any; optional>"
		},
		<other recordings>
	]
}
\endverbatim
 * 
 * An error instead (and the same applies to all other requests, so this
 * won't be repeated) would provide both an error code and a more verbose
 * description of the cause of the issue:
 * 
\verbatim
{
	"recordplay" : "event",
	"error_code" : <numeric ID, check Macros below>,
	"error" : "<error description as a string>"
}
\endverbatim
 * 
 * The \c update request instead has to be formatted as follows:
 *
\verbatim
{
	"request" : "update"
}
\endverbatim
 *
 * which will always result in an immediate ack ( \c ok ):
 * 
\verbatim
{
	"recordplay" : "ok",
}
\endverbatim
 *
 * Coming to the asynchronous requests, \c record has to be attached to
 * a JSEP offer (failure to do so will result in an error) and has to be
 * formatted as follows:
 *
\verbatim
{
	"request" : "record",
	"name" : "<Pretty name for the recording>"
}
\endverbatim
 *
 * A successful management of this request will result in a \c recording
 * event which will include the unique ID of the recording and a JSEP
 * answer to complete the setup of the associated PeerConnection to record:
 * 
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "recording",
		"id" : <unique numeric ID>
	}
}
\endverbatim
 *
 * A \c stop request can interrupt the recording process and tear the
 * associated PeerConnection down:
 * 
\verbatim
{
	"request" : "stop",
}
\endverbatim
 * 
 * This will result in a \c stopped status:
 * 
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "stopped",
		"id" : <unique numeric ID of the interrupted recording>
	}
}
\endverbatim
 * 
 * For what concerns the playout, instead, the process is slightly
 * different: you first choose a recording to replay, using \c play ,
 * and then start its playout using a \c start request. Just as before,
 * a \c stop request will interrupt the playout and tear the PeerConnection
 * down. It's very important to point out that no JSEP offer must be
 * sent for replaying a recording: in this case, it will always be the
 * plugin to generate a JSON offer (in response to a \c play request),
 * which means you'll then have to provide a JSEP answer within the
 * context of the following \c start request which will close the circle.
 * 
 * A \c play request has to be formatted as follows:
 * 
\verbatim
{
	"request" : "play",
	"id" : <unique numeric ID of the recording to replay>
}
\endverbatim
 * 
 * This will result in a \c preparing status notification which will be
 * attached to the JSEP offer originated by the plugin in order to
 * match the media available in the recording:
 * 
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "preparing",
		"id" : <unique numeric ID of the recording>
	}
}
\endverbatim
 * 
 * A \c start request, which as anticipated must be attached to the JSEP
 * answer to the previous offer sent by the plugin, has to be formatted
 * as follows:
 * 
\verbatim
{
	"request" : "start",
}
\endverbatim
 * 
 * This will result in a \c playing status notification:
 * 
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "playing"
	}
}
\endverbatim
 * 
 * Just as before, a \c stop request can interrupt the playout process at
 * any time, and tear the associated PeerConnection down:
 * 
\verbatim
{
	"request" : "stop",
}
\endverbatim
 * 
 * This will result in a \c stopped status:
 * 
\verbatim
{
	"recordplay" : "event",
	"result": {
		"status" : "stopped"
	}
}
\endverbatim
 * 
 * If the plugin detects a loss of the associated PeerConnection, whether
 * as a result of a \c stop request or because the 10 seconds passed, a
 * \c done result notification is triggered to inform the application
 * the recording/playout session is over:
 * 
\verbatim
{
	"recordplay" : "event",
	"result": "done"
}
\endverbatim
 *
 * \ingroup plugins
 * \ref plugins
 */

#include "plugin.h"

#include <dirent.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <jansson.h>

#include "../debug.h"
#include "../apierror.h"
#include "../config.h"
#include "../mutex.h"
#include "../record.h"
#include "../sdp-utils.h"
#include "../rtp.h"
#include "../rtcp.h"
#include "../utils.h"


/* Plugin information */
#define JANUS_RECORDPLAY_VERSION			4
#define JANUS_RECORDPLAY_VERSION_STRING		"0.0.4"
#define JANUS_RECORDPLAY_DESCRIPTION		"This is a trivial Record&Play plugin for Janus, to record WebRTC sessions and replay them."
#define JANUS_RECORDPLAY_NAME				"JANUS Record&Play plugin"
#define JANUS_RECORDPLAY_AUTHOR				"Meetecho s.r.l."
#define JANUS_RECORDPLAY_PACKAGE			"janus.plugin.recordplay"

/* Plugin methods */
janus_plugin *create(void);
int janus_recordplay_init(janus_callbacks *callback, const char *onfig_path);
void janus_recordplay_destroy(void);
int janus_recordplay_get_api_compatibility(void);
int janus_recordplay_get_version(void);
const char *janus_recordplay_get_version_string(void);
const char *janus_recordplay_get_description(void);
const char *janus_recordplay_get_name(void);
const char *janus_recordplay_get_author(void);
const char *janus_recordplay_get_package(void);
void janus_recordplay_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_recordplay_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void janus_recordplay_setup_media(janus_plugin_session *handle);
void janus_recordplay_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_recordplay_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_recordplay_incoming_data(janus_plugin_session *handle, char *buf, int len);
void janus_recordplay_slow_link(janus_plugin_session *handle, int uplink, int video);
void janus_recordplay_hangup_media(janus_plugin_session *handle);
void janus_recordplay_destroy_session(janus_plugin_session *handle, int *error);
json_t *janus_recordplay_query_session(janus_plugin_session *handle);

/* Plugin setup */
static janus_plugin janus_recordplay_plugin =
	JANUS_PLUGIN_INIT (
		.init = janus_recordplay_init,
		.destroy = janus_recordplay_destroy,

		.get_api_compatibility = janus_recordplay_get_api_compatibility,
		.get_version = janus_recordplay_get_version,
		.get_version_string = janus_recordplay_get_version_string,
		.get_description = janus_recordplay_get_description,
		.get_name = janus_recordplay_get_name,
		.get_author = janus_recordplay_get_author,
		.get_package = janus_recordplay_get_package,
		
		.create_session = janus_recordplay_create_session,
		.handle_message = janus_recordplay_handle_message,
		.setup_media = janus_recordplay_setup_media,
		.incoming_rtp = janus_recordplay_incoming_rtp,
		.incoming_rtcp = janus_recordplay_incoming_rtcp,
		.incoming_data = janus_recordplay_incoming_data,
		.slow_link = janus_recordplay_slow_link,
		.hangup_media = janus_recordplay_hangup_media,
		.destroy_session = janus_recordplay_destroy_session,
		.query_session = janus_recordplay_query_session,
	);

/* Plugin creator */
janus_plugin *create(void) {
	JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_RECORDPLAY_NAME);
	return &janus_recordplay_plugin;
}

/* Parameter validation */
static struct janus_json_parameter request_parameters[] = {
	{"request", JSON_STRING, JANUS_JSON_PARAM_REQUIRED}
};
static struct janus_json_parameter configure_parameters[] = {
	{"video-bitrate-max", JSON_INTEGER, JANUS_JSON_PARAM_POSITIVE},
	{"video-keyframe-interval", JSON_INTEGER, JANUS_JSON_PARAM_POSITIVE}
};
static struct janus_json_parameter record_parameters[] = {
	{"name", JSON_STRING, JANUS_JSON_PARAM_REQUIRED | JANUS_JSON_PARAM_NONEMPTY},
	{"filename", JSON_STRING, 0}
};
static struct janus_json_parameter play_parameters[] = {
	{"id", JSON_INTEGER, JANUS_JSON_PARAM_REQUIRED | JANUS_JSON_PARAM_POSITIVE}
};

/* Useful stuff */
static volatile gint initialized = 0, stopping = 0;
static gboolean notify_events = TRUE;
static janus_callbacks *gateway = NULL;
static GThread *handler_thread;
static GThread *watchdog;
static void *janus_recordplay_handler(void *data);

typedef struct janus_recordplay_message {
	janus_plugin_session *handle;
	char *transaction;
	json_t *message;
	json_t *jsep;
} janus_recordplay_message;
static GAsyncQueue *messages = NULL;
static janus_recordplay_message exit_message;

typedef struct janus_recordplay_rtp_header_extension {
	uint16_t type;
	uint16_t length;
} janus_recordplay_rtp_header_extension;

typedef struct janus_recordplay_frame_packet {
	uint16_t seq;	/* RTP Sequence number */
	uint64_t ts;	/* RTP Timestamp */
	int len;		/* Length of the data */
	long offset;	/* Offset of the data in the file */
	struct janus_recordplay_frame_packet *next;
	struct janus_recordplay_frame_packet *prev;
} janus_recordplay_frame_packet;
janus_recordplay_frame_packet *janus_recordplay_get_frames(const char *dir, const char *filename);

typedef struct janus_recordplay_recording {
	guint64 id;			/* Recording unique ID */
	char *name;			/* Name of the recording */
	char *date;			/* Time of the recording */
	char *arc_file;		/* Audio file name */
	char *vrc_file;		/* Video file name */
	gboolean completed;	/* Whether this recording was completed or still going on */
	char *offer;		/* The SDP offer that will be sent to watchers */
	GList *viewers;		/* List of users watching this recording */
	gint64 destroyed;	/* Lazy timestamp to mark recordings as destroyed */
	janus_mutex mutex;	/* Mutex for this recording */
} janus_recordplay_recording;
static GHashTable *recordings = NULL;
static janus_mutex recordings_mutex;

typedef struct janus_recordplay_session {
	janus_plugin_session *handle;
	gboolean active;
	gboolean recorder;		/* Whether this session is used to record or to replay a WebRTC session */
	gboolean firefox;	/* We send Firefox users a different kind of FIR */
	janus_recordplay_recording *recording;
	janus_recorder *arc;	/* Audio recorder */
	janus_recorder *vrc;	/* Video recorder */
	janus_mutex rec_mutex;	/* Mutex to protect the recorders from race conditions */
	janus_recordplay_frame_packet *aframes;	/* Audio frames (for playout) */
	janus_recordplay_frame_packet *vframes;	/* Video frames (for playout) */
	guint video_remb_startup;
	gint64 video_remb_last;
	guint32 video_bitrate;
	guint video_keyframe_interval; /* keyframe request interval (ms) */
	guint64 video_keyframe_request_last; /* timestamp of last keyframe request sent */
	gint video_fir_seq;
	guint32 simulcast_ssrc;	/* We don't support Simulcast in this plugin, so we'll stick to the base substream */
	volatile gint hangingup;
	gint64 destroyed;	/* Time at which this session was marked as destroyed */
} janus_recordplay_session;
static GHashTable *sessions;
static GList *old_sessions;
static janus_mutex sessions_mutex;


static char *recordings_path = NULL;
void janus_recordplay_update_recordings_list(void);
static void *janus_recordplay_playout_thread(void *data);

/* Helper to send RTCP feedback back to recorders, if needed */
void janus_recordplay_send_rtcp_feedback(janus_plugin_session *handle, int video, char *buf, int len);

/* To make things easier, we use static payload types for viewers */
#define OPUS_PT		111
#define VP8_PT		100

/* Helper method to prepare an SDP offer when a recording is available */
static int janus_recordplay_generate_offer(janus_recordplay_recording *rec) {
	if(rec == NULL)
		return -1;
	/* Prepare an SDP offer we'll send to playout viewers */
	gboolean offer_audio = (rec->arc_file != NULL),
		offer_video = (rec->vrc_file != NULL);
	char s_name[100];
	g_snprintf(s_name, sizeof(s_name), "Recording %"SCNu64, rec->id);
	janus_sdp *offer = janus_sdp_generate_offer(
		s_name, "1.1.1.1",
		JANUS_SDP_OA_AUDIO, offer_audio,
		JANUS_SDP_OA_AUDIO_CODEC, "opus",
		JANUS_SDP_OA_AUDIO_PT, OPUS_PT,
		JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_SENDONLY,
		JANUS_SDP_OA_VIDEO, offer_video,
		JANUS_SDP_OA_VIDEO_CODEC, "vp8",
		JANUS_SDP_OA_VIDEO_PT, VP8_PT,
		JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_SENDONLY,
		JANUS_SDP_OA_DATA, FALSE,
		JANUS_SDP_OA_DONE);
	g_free(rec->offer);
	rec->offer = janus_sdp_write(offer);
	janus_sdp_free(offer);
	return 0;
}

static void janus_recordplay_message_free(janus_recordplay_message *msg) {
	if(!msg || msg == &exit_message)
		return;

	msg->handle = NULL;

	g_free(msg->transaction);
	msg->transaction = NULL;
	if(msg->message)
		json_decref(msg->message);
	msg->message = NULL;
	if(msg->jsep)
		json_decref(msg->jsep);
	msg->jsep = NULL;

	g_free(msg);
}


/* Error codes */
#define JANUS_RECORDPLAY_ERROR_NO_MESSAGE			411
#define JANUS_RECORDPLAY_ERROR_INVALID_JSON			412
#define JANUS_RECORDPLAY_ERROR_INVALID_REQUEST		413
#define JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT		414
#define JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT		415
#define JANUS_RECORDPLAY_ERROR_NOT_FOUND			416
#define JANUS_RECORDPLAY_ERROR_INVALID_RECORDING	417
#define JANUS_RECORDPLAY_ERROR_INVALID_STATE		418
#define JANUS_RECORDPLAY_ERROR_INVALID_SDP			419
#define JANUS_RECORDPLAY_ERROR_UNKNOWN_ERROR		499


/* Record&Play watchdog/garbage collector (sort of) */
static void *janus_recordplay_watchdog(void *data) {
	JANUS_LOG(LOG_INFO, "Record&Play watchdog started\n");
	gint64 now = 0;
	while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
		janus_mutex_lock(&sessions_mutex);
		/* Iterate on all the sessions */
		now = janus_get_monotonic_time();
		if(old_sessions != NULL) {
			GList *sl = old_sessions;
			JANUS_LOG(LOG_HUGE, "Checking %d old Record&Play sessions...\n", g_list_length(old_sessions));
			while(sl) {
				janus_recordplay_session *session = (janus_recordplay_session *)sl->data;
				if(!session) {
					sl = sl->next;
					continue;
				}
				if(now-session->destroyed >= 5*G_USEC_PER_SEC) {
					/* We're lazy and actually get rid of the stuff only after a few seconds */
					JANUS_LOG(LOG_VERB, "Freeing old Record&Play session\n");
					GList *rm = sl->next;
					old_sessions = g_list_delete_link(old_sessions, sl);
					sl = rm;
					session->handle = NULL;
					g_free(session);
					session = NULL;
					continue;
				}
				sl = sl->next;
			}
		}
		janus_mutex_unlock(&sessions_mutex);
		g_usleep(500000);
	}
	JANUS_LOG(LOG_INFO, "Record&Play watchdog stopped\n");
	return NULL;
}


/* Plugin implementation */
int janus_recordplay_init(janus_callbacks *callback, const char *config_path) {
	if(g_atomic_int_get(&stopping)) {
		/* Still stopping from before */
		return -1;
	}
	if(callback == NULL || config_path == NULL) {
		/* Invalid arguments */
		return -1;
	}

	/* Read configuration */
	char filename[255];
	g_snprintf(filename, 255, "%s/%s.cfg", config_path, JANUS_RECORDPLAY_PACKAGE);
	JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
	janus_config *config = janus_config_parse(filename);
	if(config != NULL)
		janus_config_print(config);
	/* Parse configuration */
	if(config != NULL) {
		janus_config_item *path = janus_config_get_item_drilldown(config, "general", "path");
		if(path && path->value)
			recordings_path = g_strdup(path->value);
		janus_config_item *events = janus_config_get_item_drilldown(config, "general", "events");
		if(events != NULL && events->value != NULL)
			notify_events = janus_is_true(events->value);
		if(!notify_events && callback->events_is_enabled()) {
			JANUS_LOG(LOG_WARN, "Notification of events to handlers disabled for %s\n", JANUS_RECORDPLAY_NAME);
		}
		/* Done */
		janus_config_destroy(config);
		config = NULL;
	}
	if(recordings_path == NULL) {
		JANUS_LOG(LOG_FATAL, "No recordings path specified, giving up...\n");
		return -1;
	}
	/* Create the folder, if needed */
	struct stat st = {0};
	if(stat(recordings_path, &st) == -1) {
		int res = janus_mkdir(recordings_path, 0755);
		JANUS_LOG(LOG_VERB, "Creating folder: %d\n", res);
		if(res != 0) {
			JANUS_LOG(LOG_ERR, "%s", strerror(errno));
			return -1;	/* No point going on... */
		}
	}
	recordings = g_hash_table_new_full(g_int64_hash, g_int64_equal, (GDestroyNotify)g_free, NULL);
	janus_mutex_init(&recordings_mutex);
	janus_recordplay_update_recordings_list();
	
	sessions = g_hash_table_new(NULL, NULL);
	janus_mutex_init(&sessions_mutex);
	messages = g_async_queue_new_full((GDestroyNotify) janus_recordplay_message_free);
	/* This is the callback we'll need to invoke to contact the gateway */
	gateway = callback;

	g_atomic_int_set(&initialized, 1);

	GError *error = NULL;
	/* Start the sessions watchdog */
	watchdog = g_thread_try_new("recordplay watchdog", &janus_recordplay_watchdog, NULL, &error);
	if(error != NULL) {
		g_atomic_int_set(&initialized, 0);
		JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Record&Play watchdog thread...\n", error->code, error->message ? error->message : "??");
		return -1;
	}
	/* Launch the thread that will handle incoming messages */
	handler_thread = g_thread_try_new("recordplay handler", janus_recordplay_handler, NULL, &error);
	if(error != NULL) {
		g_atomic_int_set(&initialized, 0);
		JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Record&Play handler thread...\n", error->code, error->message ? error->message : "??");
		return -1;
	}
	JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_RECORDPLAY_NAME);
	return 0;
}

void janus_recordplay_destroy(void) {
	if(!g_atomic_int_get(&initialized))
		return;
	g_atomic_int_set(&stopping, 1);

	g_async_queue_push(messages, &exit_message);
	if(handler_thread != NULL) {
		g_thread_join(handler_thread);
		handler_thread = NULL;
	}
	if(watchdog != NULL) {
		g_thread_join(watchdog);
		watchdog = NULL;
	}
	/* FIXME We should destroy the sessions cleanly */
	janus_mutex_lock(&sessions_mutex);
	g_hash_table_destroy(sessions);
	janus_mutex_unlock(&sessions_mutex);
	g_async_queue_unref(messages);
	messages = NULL;
	sessions = NULL;
	g_atomic_int_set(&initialized, 0);
	g_atomic_int_set(&stopping, 0);
	JANUS_LOG(LOG_INFO, "%s destroyed!\n", JANUS_RECORDPLAY_NAME);
}

int janus_recordplay_get_api_compatibility(void) {
	/* Important! This is what your plugin MUST always return: don't lie here or bad things will happen */
	return JANUS_PLUGIN_API_VERSION;
}

int janus_recordplay_get_version(void) {
	return JANUS_RECORDPLAY_VERSION;
}

const char *janus_recordplay_get_version_string(void) {
	return JANUS_RECORDPLAY_VERSION_STRING;
}

const char *janus_recordplay_get_description(void) {
	return JANUS_RECORDPLAY_DESCRIPTION;
}

const char *janus_recordplay_get_name(void) {
	return JANUS_RECORDPLAY_NAME;
}

const char *janus_recordplay_get_author(void) {
	return JANUS_RECORDPLAY_AUTHOR;
}

const char *janus_recordplay_get_package(void) {
	return JANUS_RECORDPLAY_PACKAGE;
}

void janus_recordplay_create_session(janus_plugin_session *handle, int *error) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		*error = -1;
		return;
	}	
	janus_recordplay_session *session = (janus_recordplay_session *)g_malloc0(sizeof(janus_recordplay_session));
	session->handle = handle;
	session->active = FALSE;
	session->recorder = FALSE;
	session->firefox = FALSE;
	session->arc = NULL;
	session->vrc = NULL;
	janus_mutex_init(&session->rec_mutex);
	session->destroyed = 0;
	g_atomic_int_set(&session->hangingup, 0);
	session->video_remb_startup = 4;
	session->video_remb_last = janus_get_monotonic_time();
	session->video_bitrate = 1024 * 1024; 		/* This is 1mbps by default */
	session->video_keyframe_request_last = 0;
	session->video_keyframe_interval = 15000; 	/* 15 seconds by default */
	session->video_fir_seq = 0;
	handle->plugin_handle = session;
	janus_mutex_lock(&sessions_mutex);
	g_hash_table_insert(sessions, handle, session);
	janus_mutex_unlock(&sessions_mutex);

	return;
}

void janus_recordplay_destroy_session(janus_plugin_session *handle, int *error) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		*error = -1;
		return;
	}	
	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;
	if(!session) {
		JANUS_LOG(LOG_ERR, "No Record&Play session associated with this handle...\n");
		*error = -2;
		return;
	}
	janus_mutex_lock(&sessions_mutex);
	if(!session->destroyed) {
		JANUS_LOG(LOG_VERB, "Removing Record&Play session...\n");
		janus_recordplay_hangup_media(handle);
		session->destroyed = janus_get_monotonic_time();
		g_hash_table_remove(sessions, handle);
		/* Cleaning up and removing the session is done in a lazy way */
		old_sessions = g_list_append(old_sessions, session);
	}
	janus_mutex_unlock(&sessions_mutex);
	return;
}

json_t *janus_recordplay_query_session(janus_plugin_session *handle) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
		return NULL;
	}	
	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return NULL;
	}
	/* In the echo test, every session is the same: we just provide some configure info */
	json_t *info = json_object();
	json_object_set_new(info, "type", json_string(session->recorder ? "recorder" : (session->recording ? "player" : "none")));
	if(session->recording) {
		json_object_set_new(info, "recording_id", json_integer(session->recording->id));
		json_object_set_new(info, "recording_name", json_string(session->recording->name));
	}
	json_object_set_new(info, "destroyed", json_integer(session->destroyed));
	return info;
}

struct janus_plugin_result *janus_recordplay_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) {
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized", NULL);

	/* Pre-parse the message */
	int error_code = 0;
	char error_cause[512];
	json_t *root = message;
	json_t *response = NULL;
	
	if(message == NULL) {
		JANUS_LOG(LOG_ERR, "No message??\n");
		error_code = JANUS_RECORDPLAY_ERROR_NO_MESSAGE;
		g_snprintf(error_cause, 512, "%s", "No message??");
		goto plugin_response;
	}

	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;	
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		error_code = JANUS_RECORDPLAY_ERROR_UNKNOWN_ERROR;
		g_snprintf(error_cause, 512, "%s", "session associated with this handle...");
		goto plugin_response;
	}
	if(session->destroyed) {
		JANUS_LOG(LOG_ERR, "Session has already been destroyed...\n");
		error_code = JANUS_RECORDPLAY_ERROR_UNKNOWN_ERROR;
		g_snprintf(error_cause, 512, "%s", "Session has already been destroyed...");
		goto plugin_response;
	}
	if(!json_is_object(root)) {
		JANUS_LOG(LOG_ERR, "JSON error: not an object\n");
		error_code = JANUS_RECORDPLAY_ERROR_INVALID_JSON;
		g_snprintf(error_cause, 512, "JSON error: not an object");
		goto plugin_response;
	}
	/* Get the request first */
	JANUS_VALIDATE_JSON_OBJECT(root, request_parameters,
		error_code, error_cause, TRUE,
		JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT);
	if(error_code != 0)
		goto plugin_response;
	json_t *request = json_object_get(root, "request");
	/* Some requests ('create' and 'destroy') can be handled synchronously */
	const char *request_text = json_string_value(request);
	if(!strcasecmp(request_text, "update")) {
		/* Update list of available recordings, scanning the folder again */
		janus_recordplay_update_recordings_list();
		/* Send info back */
		response = json_object();
		json_object_set_new(response, "recordplay", json_string("ok"));
		goto plugin_response;
	} else if(!strcasecmp(request_text, "list")) {
		json_t *list = json_array();
		JANUS_LOG(LOG_VERB, "Request for the list of recordings\n");
		/* Return a list of all available recordings */
		janus_mutex_lock(&recordings_mutex);
		GHashTableIter iter;
		gpointer value;
		g_hash_table_iter_init(&iter, recordings);
		while (g_hash_table_iter_next(&iter, NULL, &value)) {
			janus_recordplay_recording *rec = value;
			if(!rec->completed)	/* Ongoing recording, skip */
				continue;
			json_t *ml = json_object();
			json_object_set_new(ml, "id", json_integer(rec->id));
			json_object_set_new(ml, "name", json_string(rec->name));
			json_object_set_new(ml, "date", json_string(rec->date));
			json_object_set_new(ml, "audio", rec->arc_file ? json_true() : json_false());
			json_object_set_new(ml, "video", rec->vrc_file ? json_true() : json_false());
			json_array_append_new(list, ml);
		}
		janus_mutex_unlock(&recordings_mutex);
		/* Send info back */
		response = json_object();
		json_object_set_new(response, "recordplay", json_string("list"));
		json_object_set_new(response, "list", list);
		goto plugin_response;
	} else if(!strcasecmp(request_text, "configure")) {
		JANUS_VALIDATE_JSON_OBJECT(root, configure_parameters,
			error_code, error_cause, TRUE,
			JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT);
		if(error_code != 0)
			goto plugin_response;
		json_t *video_bitrate_max = json_object_get(root, "video-bitrate-max");
		if(video_bitrate_max) {
			session->video_bitrate = json_integer_value(video_bitrate_max);
			JANUS_LOG(LOG_VERB, "Video bitrate has been set to %"SCNu32"\n", session->video_bitrate);
		}
		json_t *video_keyframe_interval= json_object_get(root, "video-keyframe-interval");
		if(video_keyframe_interval) {
			session->video_keyframe_interval = json_integer_value(video_keyframe_interval);
			JANUS_LOG(LOG_VERB, "Video keyframe interval has been set to %u\n", session->video_keyframe_interval);
		}
		response = json_object();
		json_object_set_new(response, "recordplay", json_string("configure"));
		json_object_set_new(response, "status", json_string("ok"));
		/* Return a success, and also let the client be aware of what changed, to allow crosschecks */
		json_t *settings = json_object();
		json_object_set_new(settings, "video-keyframe-interval", json_integer(session->video_keyframe_interval)); 
		json_object_set_new(settings, "video-bitrate-max", json_integer(session->video_bitrate)); 
		json_object_set_new(response, "settings", settings); 
		goto plugin_response;
	} else if(!strcasecmp(request_text, "record") || !strcasecmp(request_text, "play")
			|| !strcasecmp(request_text, "start") || !strcasecmp(request_text, "stop")) {
		/* These messages are handled asynchronously */
		janus_recordplay_message *msg = g_malloc0(sizeof(janus_recordplay_message));
		msg->handle = handle;
		msg->transaction = transaction;
		msg->message = root;
		msg->jsep = jsep;

		g_async_queue_push(messages, msg);

		return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
	} else {
		JANUS_LOG(LOG_VERB, "Unknown request '%s'\n", request_text);
		error_code = JANUS_RECORDPLAY_ERROR_INVALID_REQUEST;
		g_snprintf(error_cause, 512, "Unknown request '%s'", request_text);
	}

plugin_response:
		{
			if(error_code == 0 && !response) {
				error_code = JANUS_RECORDPLAY_ERROR_UNKNOWN_ERROR;
				g_snprintf(error_cause, 512, "Invalid response");
			}
			if(error_code != 0) {
				/* Prepare JSON error event */
				json_t *event = json_object();
				json_object_set_new(event, "recordplay", json_string("event"));
				json_object_set_new(event, "error_code", json_integer(error_code));
				json_object_set_new(event, "error", json_string(error_cause));
				response = event;
			}
			if(root != NULL)
				json_decref(root);
			if(jsep != NULL)
				json_decref(jsep);
			g_free(transaction);

			return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
		}

}

void janus_recordplay_setup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "WebRTC media is now available\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;	
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	if(session->destroyed)
		return;
	g_atomic_int_set(&session->hangingup, 0);
	/* Take note of the fact that the session is now active */
	session->active = TRUE;
	if(!session->recorder) {
		GError *error = NULL;
		g_thread_try_new("recordplay playout thread", &janus_recordplay_playout_thread, session, &error);
		if(error != NULL) {
			/* FIXME Should we notify this back to the user somehow? */
			JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Record&Play playout thread...\n", error->code, error->message ? error->message : "??");
		}
	}
}

void janus_recordplay_send_rtcp_feedback(janus_plugin_session *handle, int video, char *buf, int len) {
	if(video != 1)
		return;	/* We just do this for video, for now */

	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;
	char rtcpbuf[24];

	/* Send a RR+SDES+REMB every five seconds, or ASAP while we are still
	 * ramping up (first 4 RTP packets) */
	gint64 now = janus_get_monotonic_time();
	gint64 elapsed = now - session->video_remb_last;
	gboolean remb_rampup = session->video_remb_startup > 0;

	if(remb_rampup || (elapsed >= 5*G_USEC_PER_SEC)) {
		guint32 bitrate = session->video_bitrate;

		if(remb_rampup) {
			bitrate = bitrate / session->video_remb_startup;
			session->video_remb_startup--;
		}

		/* Send a new REMB back */
		char rtcpbuf[24];
		janus_rtcp_remb((char *)(&rtcpbuf), 24, bitrate);
		gateway->relay_rtcp(handle, video, rtcpbuf, 24);

		session->video_remb_last = now;
	}

	/* Request a keyframe on a regular basis (every session->video_keyframe_interval ms) */
	elapsed = now - session->video_keyframe_request_last;
	gint64 interval = (gint64)(session->video_keyframe_interval / 1000) * G_USEC_PER_SEC;

	if(elapsed >= interval) {
		/* Send both a FIR and a PLI, just to be sure */
		janus_rtcp_fir((char *)&rtcpbuf, 20, &session->video_fir_seq);
		gateway->relay_rtcp(handle, video, rtcpbuf, 20);
		janus_rtcp_pli((char *)&rtcpbuf, 12);
		gateway->relay_rtcp(handle, video, rtcpbuf, 12);
		session->video_keyframe_request_last = now;
	}
}

void janus_recordplay_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	if(gateway) {
		janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;	
		if(!session) {
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			return;
		}
		if(session->destroyed)
			return;
		if(video && session->simulcast_ssrc) {
			/* The user is simulcasting: drop everything except the base layer */
			rtp_header *header = (rtp_header *)buf;
			uint32_t ssrc = ntohl(header->ssrc);
			if(ssrc != session->simulcast_ssrc) {
				JANUS_LOG(LOG_DBG, "Dropping packet (not base simulcast substream)\n");
				return;
			}
		}
		/* Are we recording? */
		if(session->recorder) {
			janus_recorder_save_frame(video ? session->vrc : session->arc, buf, len);
		}

		janus_recordplay_send_rtcp_feedback(handle, video, buf, len);
	}
}

void janus_recordplay_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
}

void janus_recordplay_incoming_data(janus_plugin_session *handle, char *buf, int len) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	/* FIXME We don't care */
}

void janus_recordplay_slow_link(janus_plugin_session *handle, int uplink, int video) {
	if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized) || !gateway)
		return;

	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;
	if(!session || session->destroyed)
		return;

	json_t *event = json_object();
	json_object_set_new(event, "recordplay", json_string("event"));
	json_t *result = json_object();
	json_object_set_new(result, "status", json_string("slow_link"));
	/* What is uplink for the server is downlink for the client, so turn the tables */
	json_object_set_new(result, "current-bitrate", json_integer(session->video_bitrate));
	json_object_set_new(result, "uplink", json_integer(uplink ? 0 : 1));
	json_object_set_new(event, "result", result);
	gateway->push_event(session->handle, &janus_recordplay_plugin, NULL, event, NULL);
	json_decref(event);
}

void janus_recordplay_hangup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "No WebRTC media anymore\n");
	if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
		return;
	janus_recordplay_session *session = (janus_recordplay_session *)handle->plugin_handle;
	if(!session) {
		JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
		return;
	}
	session->active = FALSE;
	if(session->destroyed || !session->recorder)
		return;
	if(g_atomic_int_add(&session->hangingup, 1))
		return;
	session->simulcast_ssrc = 0;

	/* Send an event to the browser and tell it's over */
	json_t *event = json_object();
	json_object_set_new(event, "recordplay", json_string("event"));
	json_object_set_new(event, "result", json_string("done"));
	int ret = gateway->push_event(handle, &janus_recordplay_plugin, NULL, event, NULL);
	JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
	json_decref(event);

	/* FIXME Simulate a "stop" coming from the browser */
	janus_recordplay_message *msg = g_malloc0(sizeof(janus_recordplay_message));
	msg->handle = handle;
	msg->message = json_pack("{ss}", "request", "stop");
	msg->transaction = NULL;
	msg->jsep = NULL;
	g_async_queue_push(messages, msg);
}

/* Thread to handle incoming messages */
static void *janus_recordplay_handler(void *data) {
	JANUS_LOG(LOG_VERB, "Joining Record&Play handler thread\n");
	janus_recordplay_message *msg = NULL;
	int error_code = 0;
	char error_cause[512];
	json_t *root = NULL;
	while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
		msg = g_async_queue_pop(messages);
		if(msg == NULL)
			continue;
		if(msg == &exit_message)
			break;
		if(msg->handle == NULL) {
			janus_recordplay_message_free(msg);
			continue;
		}
		janus_recordplay_session *session = NULL;
		janus_mutex_lock(&sessions_mutex);
		if(g_hash_table_lookup(sessions, msg->handle) != NULL ) {
			session = (janus_recordplay_session *)msg->handle->plugin_handle;
		}
		if(!session) {
			janus_mutex_unlock(&sessions_mutex);
			JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
			janus_recordplay_message_free(msg);
			continue;
		}
		if(session->destroyed) {
			janus_mutex_unlock(&sessions_mutex);
			janus_recordplay_message_free(msg);
			continue;
		}
		janus_mutex_unlock(&sessions_mutex);
		/* Handle request */
		error_code = 0;
		root = NULL;
		if(msg->message == NULL) {
			JANUS_LOG(LOG_ERR, "No message??\n");
			error_code = JANUS_RECORDPLAY_ERROR_NO_MESSAGE;
			g_snprintf(error_cause, 512, "%s", "No message??");
			goto error;
		}
		root = msg->message;
		/* Get the request first */
		JANUS_VALIDATE_JSON_OBJECT(root, request_parameters,
			error_code, error_cause, TRUE,
			JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT);
		if(error_code != 0)
			goto error;
		const char *msg_sdp_type = json_string_value(json_object_get(msg->jsep, "type"));
		const char *msg_sdp = json_string_value(json_object_get(msg->jsep, "sdp"));
		gboolean perc = json_is_true(json_object_get(msg->jsep, "perc"));
		json_t *request = json_object_get(root, "request");
		const char *request_text = json_string_value(request);
		json_t *event = NULL;
		json_t *result = NULL;
		char *sdp = NULL;
		const char *filename_text = NULL;
		if(!strcasecmp(request_text, "record")) {
			if(!msg_sdp) {
				JANUS_LOG(LOG_ERR, "Missing SDP offer\n");
				error_code = JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT;
				g_snprintf(error_cause, 512, "Missing SDP offer");
				goto error;
			}
			JANUS_VALIDATE_JSON_OBJECT(root, record_parameters,
				error_code, error_cause, TRUE,
				JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			char error_str[512];
			janus_sdp *offer = janus_sdp_parse(msg_sdp, error_str, sizeof(error_str));
			if(offer == NULL) {
				json_decref(event);
				JANUS_LOG(LOG_ERR, "Error parsing offer: %s\n", error_str);
				error_code = JANUS_RECORDPLAY_ERROR_INVALID_SDP;
				g_snprintf(error_cause, 512, "Error parsing offer: %s", error_str);
				goto error;
			}
			json_t *name = json_object_get(root, "name");
			const char *name_text = json_string_value(name);
			json_t *filename = json_object_get(root, "filename");
			if(filename) {
				filename_text = json_string_value(filename);
			}
			guint64 id = 0;
			while(id == 0) {
				id = janus_random_uint64();
				if(g_hash_table_lookup(recordings, &id) != NULL) {
					/* Room ID already taken, try another one */
					id = 0;
				}
			}
			JANUS_LOG(LOG_VERB, "Starting new recording with ID %"SCNu64"\n", id);
			if(perc) {
				JANUS_LOG(LOG_WARN, "Notice that this is a PERC session: the recording payload will be encrypted, and you'll need the key to replay it\n");
			}
			janus_recordplay_recording *rec = (janus_recordplay_recording *)g_malloc0(sizeof(janus_recordplay_recording));
			rec->id = id;
			rec->name = g_strdup(name_text);
			rec->viewers = NULL;
			rec->destroyed = 0;
			rec->completed = FALSE;
			rec->offer = NULL;
			janus_mutex_init(&rec->mutex);
			/* Create a date string */
			time_t t = time(NULL);
			struct tm *tmv = localtime(&t);
			char outstr[200];
			strftime(outstr, sizeof(outstr), "%Y-%m-%d %H:%M:%S", tmv);
			rec->date = g_strdup(outstr);
			if(strstr(msg_sdp, "m=audio")) {
				char filename[256];
				if(filename_text != NULL) {
					g_snprintf(filename, 256, "%s-audio", filename_text);
				} else {
					g_snprintf(filename, 256, "rec-%"SCNu64"-audio", id);
				}
				rec->arc_file = g_strdup(filename);
				/* FIXME Assuming Opus */
				session->arc = janus_recorder_create(recordings_path, "opus", rec->arc_file, perc);
			}
			if(strstr(msg_sdp, "m=video")) {
				char filename[256];
				if(filename_text != NULL) {
					g_snprintf(filename, 256, "%s-video", filename_text);
				} else {
					g_snprintf(filename, 256, "rec-%"SCNu64"-video", id);
				}
				rec->vrc_file = g_strdup(filename);
				/* FIXME Assuming VP8 */
				session->vrc = janus_recorder_create(recordings_path, "vp8", rec->vrc_file, perc);
			}
			session->recorder = TRUE;
			session->recording = rec;
			janus_mutex_lock(&recordings_mutex);
			g_hash_table_insert(recordings, janus_uint64_dup(rec->id), rec);
			janus_mutex_unlock(&recordings_mutex);
			/* We need to prepare an answer */
			janus_sdp *answer = janus_sdp_generate_answer(offer,
				JANUS_SDP_OA_AUDIO_CODEC, "opus",
				JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_RECVONLY,
				JANUS_SDP_OA_VIDEO_CODEC, "vp8",
				JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_RECVONLY,
				JANUS_SDP_OA_DATA, FALSE,
				JANUS_SDP_OA_DONE);
			g_free(answer->s_name);
			char s_name[100];
			g_snprintf(s_name, sizeof(s_name), "Recording %"SCNu64, session->recording->id);
			answer->s_name = g_strdup(s_name);
			sdp = janus_sdp_write(answer);
			janus_sdp_free(offer);
			janus_sdp_free(answer);
			JANUS_LOG(LOG_VERB, "Going to answer this SDP:\n%s\n", sdp);
			/* If the user negotiated simulcasting, just stick with the base substream */
			json_t *msg_simulcast = json_object_get(msg->jsep, "simulcast");
			if(msg_simulcast) {
				JANUS_LOG(LOG_WARN, "Recording client negotiated simulcasting which we don't do here, falling back to base substream...\n");
				session->simulcast_ssrc = json_integer_value(json_object_get(msg_simulcast, "ssrc-0"));
			}
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("recording"));
			json_object_set_new(result, "id", json_integer(id));
			/* Also notify event handlers */
			if(notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("recording"));
				json_object_set_new(info, "id", json_integer(id));
				json_object_set_new(info, "audio", session->arc ? json_true() : json_false());
				json_object_set_new(info, "video", session->vrc ? json_true() : json_false());
				gateway->notify_event(&janus_recordplay_plugin, session->handle, info);
			}
		} else if(!strcasecmp(request_text, "play")) {
			if(msg_sdp) {
				JANUS_LOG(LOG_ERR, "A play request can't contain an SDP\n");
				error_code = JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT;
				g_snprintf(error_cause, 512, "A play request can't contain an SDP");
				goto error;
			}
			JANUS_LOG(LOG_VERB, "Replaying a recording\n");
			JANUS_VALIDATE_JSON_OBJECT(root, play_parameters,
				error_code, error_cause, TRUE,
				JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT, JANUS_RECORDPLAY_ERROR_INVALID_ELEMENT);
			if(error_code != 0)
				goto error;
			json_t *id = json_object_get(root, "id");
			guint64 id_value = json_integer_value(id);
			/* Look for this recording */
			janus_mutex_lock(&recordings_mutex);
			janus_recordplay_recording *rec = g_hash_table_lookup(recordings, &id_value);
			janus_mutex_unlock(&recordings_mutex);
			if(rec == NULL || rec->destroyed || rec->offer == NULL) {
				JANUS_LOG(LOG_ERR, "No such recording\n");
				error_code = JANUS_RECORDPLAY_ERROR_NOT_FOUND;
				g_snprintf(error_cause, 512, "No such recording");
				goto error;
			}
			/* Access the frames */
			const char *warning = NULL;
			if(rec->arc_file) {
				session->aframes = janus_recordplay_get_frames(recordings_path, rec->arc_file);
				if(session->aframes == NULL) {
					JANUS_LOG(LOG_WARN, "Error opening audio recording, trying to go on anyway\n");
					warning = "Broken audio file, playing video only";
				}
			}
			if(rec->vrc_file) {
				session->vframes = janus_recordplay_get_frames(recordings_path, rec->vrc_file);
				if(session->vframes == NULL) {
					JANUS_LOG(LOG_WARN, "Error opening video recording, trying to go on anyway\n");
					warning = "Broken video file, playing audio only";
				}
			}
			if(session->aframes == NULL && session->vframes == NULL) {
				error_code = JANUS_RECORDPLAY_ERROR_INVALID_RECORDING;
				g_snprintf(error_cause, 512, "Error opening recording files");
				goto error;
			}
			session->recording = rec;
			session->recorder = FALSE;
			rec->viewers = g_list_append(rec->viewers, session);
			/* Send this viewer the prepared offer  */
			sdp = g_strdup(rec->offer);
			JANUS_LOG(LOG_VERB, "Going to offer this SDP:\n%s\n", sdp);
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("preparing"));
			json_object_set_new(result, "id", json_integer(id_value));
			if(warning)
				json_object_set_new(result, "warning", json_string(warning));
			/* Also notify event handlers */
			if(notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("playout"));
				json_object_set_new(info, "id", json_integer(id_value));
				json_object_set_new(info, "audio", session->aframes ? json_true() : json_false());
				json_object_set_new(info, "video", session->vframes ? json_true() : json_false());
				gateway->notify_event(&janus_recordplay_plugin, session->handle, info);
			}
		} else if(!strcasecmp(request_text, "start")) {
			if(!session->aframes && !session->vframes) {
				JANUS_LOG(LOG_ERR, "Not a playout session, can't start\n");
				error_code = JANUS_RECORDPLAY_ERROR_INVALID_STATE;
				g_snprintf(error_cause, 512, "Not a playout session, can't start");
				goto error;
			}
			/* Just a final message we make use of, e.g., to receive an ANSWER to our OFFER for a playout */
			if(!msg_sdp) {
				JANUS_LOG(LOG_ERR, "Missing SDP answer\n");
				error_code = JANUS_RECORDPLAY_ERROR_MISSING_ELEMENT;
				g_snprintf(error_cause, 512, "Missing SDP answer");
				goto error;
			}
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("playing"));
			/* Also notify event handlers */
			if(notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("playing"));
				json_object_set_new(info, "id", json_integer(session->recording->id));
				gateway->notify_event(&janus_recordplay_plugin, session->handle, info);
			}
		} else if(!strcasecmp(request_text, "stop")) {
			/* Stop the recording/playout */
			session->active = FALSE;
			janus_mutex_lock(&session->rec_mutex);
			if(session->arc) {
				janus_recorder_close(session->arc);
				JANUS_LOG(LOG_INFO, "Closed audio recording %s\n", session->arc->filename ? session->arc->filename : "??");
				janus_recorder_free(session->arc);
			}
			session->arc = NULL;
			if(session->vrc) {
				janus_recorder_close(session->vrc);
				JANUS_LOG(LOG_INFO, "Closed video recording %s\n", session->vrc->filename ? session->vrc->filename : "??");
				janus_recorder_free(session->vrc);
			}
			session->vrc = NULL;
			janus_mutex_unlock(&session->rec_mutex);
			if(session->recorder) {
				/* Create a .nfo file for this recording */
				char nfofile[1024], nfo[1024];
				g_snprintf(nfofile, 1024, "%s/%"SCNu64".nfo", recordings_path, session->recording->id);
				FILE *file = fopen(nfofile, "wt");
				if(file == NULL) {
					JANUS_LOG(LOG_ERR, "Error creating file %s...\n", nfofile);
				} else {
					if(session->recording->arc_file && session->recording->vrc_file) {
						g_snprintf(nfo, 1024,
							"[%"SCNu64"]\r\n"
							"name = %s\r\n"
							"date = %s\r\n"
							"audio = %s.mjr\r\n"
							"video = %s.mjr\r\n",
								session->recording->id, session->recording->name, session->recording->date,
								session->recording->arc_file, session->recording->vrc_file);
					} else if(session->recording->arc_file) {
						g_snprintf(nfo, 1024,
							"[%"SCNu64"]\r\n"
							"name = %s\r\n"
							"date = %s\r\n"
							"audio = %s.mjr\r\n",
								session->recording->id, session->recording->name, session->recording->date,
								session->recording->arc_file);
					} else if(session->recording->vrc_file) {
						g_snprintf(nfo, 1024,
							"[%"SCNu64"]\r\n"
							"name = %s\r\n"
							"date = %s\r\n"
							"video = %s.mjr\r\n",
								session->recording->id, session->recording->name, session->recording->date,
								session->recording->vrc_file);
					}
					/* Write to the file now */
					fwrite(nfo, strlen(nfo), sizeof(char), file);
					fclose(file);
					/* Generate the offer */
					if(janus_recordplay_generate_offer(session->recording) < 0) {
						JANUS_LOG(LOG_WARN, "Could not generate offer for recording %"SCNu64"...\n", session->recording->id);
					}
					session->recording->completed = TRUE;
				}
			}
			/* Done! */
			result = json_object();
			json_object_set_new(result, "status", json_string("stopped"));
			if(session->recording)
				json_object_set_new(result, "id", json_integer(session->recording->id));
			/* Also notify event handlers */
			if(notify_events && gateway->events_is_enabled()) {
				json_t *info = json_object();
				json_object_set_new(info, "event", json_string("stopped"));
				if(session->recording)
					json_object_set_new(info, "id", json_integer(session->recording->id));
				gateway->notify_event(&janus_recordplay_plugin, session->handle, info);
			}
		} else {
			JANUS_LOG(LOG_ERR, "Unknown request '%s'\n", request_text);
			error_code = JANUS_RECORDPLAY_ERROR_INVALID_REQUEST;
			g_snprintf(error_cause, 512, "Unknown request '%s'", request_text);
			goto error;
		}

		/* Any SDP to handle? */
		if(msg_sdp) {
			session->firefox = strstr(msg_sdp, "Mozilla") ? TRUE : FALSE;
			JANUS_LOG(LOG_VERB, "This is involving a negotiation (%s) as well:\n%s\n", msg_sdp_type, msg_sdp);
		}

		/* Prepare JSON event */
		event = json_object();
		json_object_set_new(event, "recordplay", json_string("event"));
		if(result != NULL)
			json_object_set_new(event, "result", result);
		if(!sdp) {
			int ret = gateway->push_event(msg->handle, &janus_recordplay_plugin, msg->transaction, event, NULL);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
			json_decref(event);
		} else {
			const char *type = session->recorder ? "answer" : "offer";
			json_t *jsep = json_pack("{ssss}", "type", type, "sdp", sdp);
			/* How long will the gateway take to push the event? */
			g_atomic_int_set(&session->hangingup, 0);
			gint64 start = janus_get_monotonic_time();
			int res = gateway->push_event(msg->handle, &janus_recordplay_plugin, msg->transaction, event, jsep);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (took %"SCNu64" us)\n",
				res, janus_get_monotonic_time()-start);
			g_free(sdp);
			json_decref(event);
			json_decref(jsep);
		}
		janus_recordplay_message_free(msg);
		continue;
		
error:
		{
			/* Prepare JSON error event */
			json_t *event = json_object();
			json_object_set_new(event, "recordplay", json_string("event"));
			json_object_set_new(event, "error_code", json_integer(error_code));
			json_object_set_new(event, "error", json_string(error_cause));
			int ret = gateway->push_event(msg->handle, &janus_recordplay_plugin, msg->transaction, event, NULL);
			JANUS_LOG(LOG_VERB, "  >> Pushing event: %d (%s)\n", ret, janus_get_api_error(ret));
			json_decref(event);
			janus_recordplay_message_free(msg);
		}
	}
	JANUS_LOG(LOG_VERB, "LeavingRecord&Play handler thread\n");
	return NULL;
}

void janus_recordplay_update_recordings_list(void) {
	if(recordings_path == NULL)
		return;
	JANUS_LOG(LOG_VERB, "Updating recordings list in %s\n", recordings_path);
	janus_mutex_lock(&recordings_mutex);
	/* First of all, let's keep track of which recordings are currently available */
	GList *old_recordings = NULL;
	if(recordings != NULL && g_hash_table_size(recordings) > 0) {
		GHashTableIter iter;
		gpointer value;
		g_hash_table_iter_init(&iter, recordings);
		while(g_hash_table_iter_next(&iter, NULL, &value)) {
			janus_recordplay_recording *rec = value;
			if(rec) {
				old_recordings = g_list_append(old_recordings, &rec->id);
			}
		}
	}
	/* Open dir */
	DIR *dir = opendir(recordings_path);
	if(!dir) {
		JANUS_LOG(LOG_ERR, "Couldn't open folder...\n");
		g_list_free(old_recordings);
		janus_mutex_unlock(&recordings_mutex);
		return;
	}
	struct dirent *recent = NULL;
	char recpath[1024];
	while((recent = readdir(dir))) {
		int len = strlen(recent->d_name);
		if(len < 4)
			continue;
		if(strcasecmp(recent->d_name+len-4, ".nfo"))
			continue;
		JANUS_LOG(LOG_VERB, "Importing recording '%s'...\n", recent->d_name);
		memset(recpath, 0, 1024);
		g_snprintf(recpath, 1024, "%s/%s", recordings_path, recent->d_name);
		janus_config *nfo = janus_config_parse(recpath);
		if(nfo == NULL) { 
			JANUS_LOG(LOG_ERR, "Invalid recording '%s'...\n", recent->d_name);
			continue;
		}
		GList *cl = janus_config_get_categories(nfo);
		if(cl == NULL || cl->data == NULL) {
			JANUS_LOG(LOG_WARN, "No recording info in '%s', skipping...\n", recent->d_name);
			janus_config_destroy(nfo);
			continue;
		}
		janus_config_category *cat = (janus_config_category *)cl->data;
		guint64 id = g_ascii_strtoull(cat->name, NULL, 0);
		if(id == 0) {
			JANUS_LOG(LOG_WARN, "Invalid ID, skipping...\n");
			janus_config_destroy(nfo);
			continue;
		}
		janus_recordplay_recording *rec = g_hash_table_lookup(recordings, &id);
		if(rec != NULL) {
			JANUS_LOG(LOG_VERB, "Skipping recording with ID %"SCNu64", it's already in the list...\n", id);
			janus_config_destroy(nfo);
			/* Mark that we updated this recording */
			old_recordings = g_list_remove(old_recordings, &rec->id);
			continue;
		}
		janus_config_item *name = janus_config_get_item(cat, "name");
		janus_config_item *date = janus_config_get_item(cat, "date");
		janus_config_item *audio = janus_config_get_item(cat, "audio");
		janus_config_item *video = janus_config_get_item(cat, "video");
		if(!name || !name->value || strlen(name->value) == 0 || !date || !date->value || strlen(date->value) == 0) {
			JANUS_LOG(LOG_WARN, "Invalid info for recording %"SCNu64", skipping...\n", id);
			janus_config_destroy(nfo);
			continue;
		}
		if((!audio || !audio->value) && (!video || !video->value)) {
			JANUS_LOG(LOG_WARN, "No audio and no video in recording %"SCNu64", skipping...\n", id);
			janus_config_destroy(nfo);
			continue;
		}
		rec = (janus_recordplay_recording *)g_malloc0(sizeof(janus_recordplay_recording));
		rec->id = id;
		rec->name = g_strdup(name->value);
		rec->date = g_strdup(date->value);
		if(audio && audio->value) {
			rec->arc_file = g_strdup(audio->value);
			char *ext = strstr(rec->arc_file, ".mjr");
			if(ext != NULL)
				*ext = '\0';
		}
		if(video && video->value) {
			rec->vrc_file = g_strdup(video->value);
			char *ext = strstr(rec->vrc_file, ".mjr");
			if(ext != NULL)
				*ext = '\0';
		}
		rec->viewers = NULL;
		rec->destroyed = 0;
		rec->completed = TRUE;
		if(janus_recordplay_generate_offer(rec) < 0) {
			JANUS_LOG(LOG_WARN, "Could not generate offer for recording %"SCNu64"...\n", rec->id);
		}
		janus_mutex_init(&rec->mutex);
		
		janus_config_destroy(nfo);

		/* Add to the list of recordings */
		g_hash_table_insert(recordings, janus_uint64_dup(rec->id), rec);
	}
	closedir(dir);
	/* Now let's check if any of the previously existing recordings was removed */
	if(old_recordings != NULL) {
		while(old_recordings != NULL) {
			guint64 id = *((guint64 *)old_recordings->data);
			JANUS_LOG(LOG_VERB, "Recording %"SCNu64" is not available anymore, removing...\n", id);
			janus_recordplay_recording *old_rec = g_hash_table_lookup(recordings, &id);
			if(old_rec != NULL) {
				/* Remove it */
				g_hash_table_remove(recordings, &id);
				/* Only destroy the object if no one's watching, though */
				janus_mutex_lock(&old_rec->mutex);
				old_rec->destroyed = janus_get_monotonic_time();
				if(old_rec->viewers == NULL) {
					JANUS_LOG(LOG_VERB, "Recording %"SCNu64" has no viewers, destroying it now\n", id);
					janus_mutex_unlock(&old_rec->mutex);
					g_free(old_rec->name);
					g_free(old_rec->date);
					g_free(old_rec->arc_file);
					g_free(old_rec->vrc_file);
					g_free(old_rec->offer);
					g_free(old_rec);
				} else {
					JANUS_LOG(LOG_VERB, "Recording %"SCNu64" still has viewers, delaying its destruction until later\n", id);
					janus_mutex_unlock(&old_rec->mutex);
				}
			}
			old_recordings = old_recordings->next;
		}
		g_list_free(old_recordings);
	}
	janus_mutex_unlock(&recordings_mutex);
}

janus_recordplay_frame_packet *janus_recordplay_get_frames(const char *dir, const char *filename) {
	if(!dir || !filename)
		return NULL;
	/* Open the file */
	char source[1024];
	if(strstr(filename, ".mjr"))
		g_snprintf(source, 1024, "%s/%s", dir, filename);
	else
		g_snprintf(source, 1024, "%s/%s.mjr", dir, filename);
	FILE *file = fopen(source, "rb");
	if(file == NULL) {
		JANUS_LOG(LOG_ERR, "Could not open file %s\n", source);
		return NULL;
	}
	fseek(file, 0L, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0L, SEEK_SET);
	JANUS_LOG(LOG_VERB, "File is %zu bytes\n", fsize);

	/* Pre-parse */
	JANUS_LOG(LOG_VERB, "Pre-parsing file %s to generate ordered index...\n", source);
	gboolean parsed_header = FALSE;
	int bytes = 0;
	long offset = 0;
	uint16_t len = 0, count = 0;
	uint32_t first_ts = 0, last_ts = 0, reset = 0;	/* To handle whether there's a timestamp reset in the recording */
	char prebuffer[1500];
	memset(prebuffer, 0, 1500);
	/* Let's look for timestamp resets first */
	while(offset < fsize) {
		/* Read frame header */
		fseek(file, offset, SEEK_SET);
		bytes = fread(prebuffer, sizeof(char), 8, file);
		if(bytes != 8 || prebuffer[0] != 'M') {
			JANUS_LOG(LOG_ERR, "Invalid header...\n");
			fclose(file);
			return NULL;
		}
		if(prebuffer[1] == 'E') {
			/* Either the old .mjr format header ('MEETECHO' header followed by 'audio' or 'video'), or a frame */
			offset += 8;
			bytes = fread(&len, sizeof(uint16_t), 1, file);
			len = ntohs(len);
			offset += 2;
			if(len == 5 && !parsed_header) {
				/* This is the main header */
				parsed_header = TRUE;
				JANUS_LOG(LOG_VERB, "Old .mjr header format\n");
				bytes = fread(prebuffer, sizeof(char), 5, file);
				if(prebuffer[0] == 'v') {
					JANUS_LOG(LOG_INFO, "This is a video recording, assuming VP8\n");
				} else if(prebuffer[0] == 'a') {
					JANUS_LOG(LOG_INFO, "This is an audio recording, assuming Opus\n");
				} else {
					JANUS_LOG(LOG_WARN, "Unsupported recording media type...\n");
					fclose(file);
					return NULL;
				}
				offset += len;
				continue;
			} else if(len < 12) {
				/* Not RTP, skip */
				JANUS_LOG(LOG_VERB, "Skipping packet (not RTP?)\n");
				offset += len;
				continue;
			}
		} else if(prebuffer[1] == 'J') {
			/* New .mjr format, the header may contain useful info */
			offset += 8;
			bytes = fread(&len, sizeof(uint16_t), 1, file);
			len = ntohs(len);
			offset += 2;
			if(len > 0 && !parsed_header) {
				/* This is the info header */
				JANUS_LOG(LOG_VERB, "New .mjr header format\n");
				bytes = fread(prebuffer, sizeof(char), len, file);
				if(bytes < 0) {
					JANUS_LOG(LOG_ERR, "Error reading from file... %s\n", strerror(errno));
					fclose(file);
					return NULL;
				}
				parsed_header = TRUE;
				prebuffer[len] = '\0';
				json_error_t error;
				json_t *info = json_loads(prebuffer, 0, &error);
				if(!info) {
					JANUS_LOG(LOG_ERR, "JSON error: on line %d: %s\n", error.line, error.text);
					JANUS_LOG(LOG_WARN, "Error parsing info header...\n");
					fclose(file);
					return NULL;
				}
				/* First of all let's check if this is a PERC recording */
				json_t *perc = json_object_get(info, "p");
				if(perc && json_is_true(perc)) {
					/* It is, clarify that the encryption key will be needed to replay this... */
					JANUS_LOG(LOG_WARN, "This is a PERC recording, so the viewer will need the right key to decrypt the payload...\n");
				}
				/* Is it audio or video? */
				json_t *type = json_object_get(info, "t");
				if(!type || !json_is_string(type)) {
					JANUS_LOG(LOG_WARN, "Missing/invalid recording type in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				const char *t = json_string_value(type);
				int video = 0;
				gint64 c_time = 0, w_time = 0;
				if(!strcasecmp(t, "v")) {
					video = 1;
				} else if(!strcasecmp(t, "a")) {
					video = 0;
				} else {
					JANUS_LOG(LOG_WARN, "Unsupported recording type '%s' in info header...\n", t);
					json_decref(info);
					fclose(file);
					return NULL;
				}
				/* What codec was used? */
				json_t *codec = json_object_get(info, "c");
				if(!codec || !json_is_string(codec)) {
					JANUS_LOG(LOG_WARN, "Missing recording codec in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				const char *c = json_string_value(codec);
				if(video && strcasecmp(c, "vp8")) {
					JANUS_LOG(LOG_WARN, "The Record&Play plugin only supports VP8 video for now (was '%s')...\n", c);
					json_decref(info);
					fclose(file);
					return NULL;
				} else if(!video && strcasecmp(c, "opus")) {
					JANUS_LOG(LOG_WARN, "The Record&Play plugin only supports Opus audio for now (was '%s')...\n", c);
					json_decref(info);
					fclose(file);
					return NULL;
				}
				/* When was the file created? */
				json_t *created = json_object_get(info, "s");
				if(!created || !json_is_integer(created)) {
					JANUS_LOG(LOG_WARN, "Missing recording created time in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				c_time = json_integer_value(created);
				/* When was the first frame written? */
				json_t *written = json_object_get(info, "u");
				if(!written || !json_is_integer(written)) {
					JANUS_LOG(LOG_WARN, "Missing recording written time in info header...\n");
					json_decref(info);
					fclose(file);
					return NULL;
				}
				w_time = json_integer_value(created);
				/* Summary */
				JANUS_LOG(LOG_VERB, "This is %s recording:\n", video ? "a video" : "an audio");
				JANUS_LOG(LOG_VERB, "  -- Codec:   %s\n", c);
				JANUS_LOG(LOG_VERB, "  -- Created: %"SCNi64"\n", c_time);
				JANUS_LOG(LOG_VERB, "  -- Written: %"SCNi64"\n", w_time);
				json_decref(info);
			}
		} else {
			JANUS_LOG(LOG_ERR, "Invalid header...\n");
			fclose(file);
			return NULL;
		}
		/* Only read RTP header */
		bytes = fread(prebuffer, sizeof(char), 16, file);
		rtp_header *rtp = (rtp_header *)prebuffer;
		if(last_ts == 0) {
			first_ts = ntohl(rtp->timestamp);
			if(first_ts > 1000*1000)	/* Just used to check whether a packet is pre- or post-reset */
				first_ts -= 1000*1000;
		} else {
			if(ntohl(rtp->timestamp) < last_ts) {
				/* The new timestamp is smaller than the next one, is it a timestamp reset or simply out of order? */
				if(last_ts-ntohl(rtp->timestamp) > 2*1000*1000*1000) {
					reset = ntohl(rtp->timestamp);
					JANUS_LOG(LOG_VERB, "Timestamp reset: %"SCNu32"\n", reset);
				}
			} else if(ntohl(rtp->timestamp) < reset) {
				JANUS_LOG(LOG_VERB, "Updating timestamp reset: %"SCNu32" (was %"SCNu32")\n", ntohl(rtp->timestamp), reset);
				reset = ntohl(rtp->timestamp);
			}
		}
		last_ts = ntohl(rtp->timestamp);
		/* Skip data for now */
		offset += len;
	}
	/* Now let's parse the frames and order them */
	offset = 0;
	janus_recordplay_frame_packet *list = NULL, *last = NULL;
	while(offset < fsize) {
		/* Read frame header */
		fseek(file, offset, SEEK_SET);
		bytes = fread(prebuffer, sizeof(char), 8, file);
		prebuffer[8] = '\0';
		JANUS_LOG(LOG_HUGE, "Header: %s\n", prebuffer);
		offset += 8;
		bytes = fread(&len, sizeof(uint16_t), 1, file);
		len = ntohs(len);
		JANUS_LOG(LOG_HUGE, "  -- Length: %"SCNu16"\n", len);
		offset += 2;
		if(prebuffer[1] == 'J' || len < 12) {
			/* Not RTP, skip */
			JANUS_LOG(LOG_HUGE, "  -- Not RTP, skipping\n");
			offset += len;
			continue;
		}
		/* Only read RTP header */
		bytes = fread(prebuffer, sizeof(char), 16, file);
		if(bytes < 0) {
			JANUS_LOG(LOG_WARN, "Error reading RTP header, stopping here...\n");
			break;
		}
		rtp_header *rtp = (rtp_header *)prebuffer;
		JANUS_LOG(LOG_HUGE, "  -- RTP packet (ssrc=%"SCNu32", pt=%"SCNu16", ext=%"SCNu16", seq=%"SCNu16", ts=%"SCNu32")\n",
				ntohl(rtp->ssrc), rtp->type, rtp->extension, ntohs(rtp->seq_number), ntohl(rtp->timestamp));
		/* Generate frame packet and insert in the ordered list */
		janus_recordplay_frame_packet *p = g_malloc0(sizeof(janus_recordplay_frame_packet));
		p->seq = ntohs(rtp->seq_number);
		if(reset == 0) {
			/* Simple enough... */
			p->ts = ntohl(rtp->timestamp);
		} else {
			/* Is this packet pre- or post-reset? */
			if(ntohl(rtp->timestamp) > first_ts) {
				/* Pre-reset... */
				p->ts = ntohl(rtp->timestamp);
			} else {
				/* Post-reset... */
				uint64_t max32 = UINT32_MAX;
				max32++;
				p->ts = max32+ntohl(rtp->timestamp);
			}
		}
		p->len = len;
		p->offset = offset;
		p->next = NULL;
		p->prev = NULL;
		if(list == NULL) {
			/* First element becomes the list itself (and the last item), at least for now */
			list = p;
			last = p;
		} else {
			/* Check where we should insert this, starting from the end */
			int added = 0;
			janus_recordplay_frame_packet *tmp = last;
			while(tmp) {
				if(tmp->ts < p->ts) {
					/* The new timestamp is greater than the last one we have, append */
					added = 1;
					if(tmp->next != NULL) {
						/* We're inserting */
						tmp->next->prev = p;
						p->next = tmp->next;
					} else {
						/* Update the last packet */
						last = p;
					}
					tmp->next = p;
					p->prev = tmp;
					break;
				} else if(tmp->ts == p->ts) {
					/* Same timestamp, check the sequence number */
					if(tmp->seq < p->seq && (abs(tmp->seq - p->seq) < 10000)) {
						/* The new sequence number is greater than the last one we have, append */
						added = 1;
						if(tmp->next != NULL) {
							/* We're inserting */
							tmp->next->prev = p;
							p->next = tmp->next;
						} else {
							/* Update the last packet */
							last = p;
						}
						tmp->next = p;
						p->prev = tmp;
						break;
					} else if(tmp->seq > p->seq && (abs(tmp->seq - p->seq) > 10000)) {
						/* The new sequence number (resetted) is greater than the last one we have, append */
						added = 1;
						if(tmp->next != NULL) {
							/* We're inserting */
							tmp->next->prev = p;
							p->next = tmp->next;
						} else {
							/* Update the last packet */
							last = p;
						}
						tmp->next = p;
						p->prev = tmp;
						break;
					}
				}
				/* If either the timestamp ot the sequence number we just got is smaller, keep going back */
				tmp = tmp->prev;
			}
			if(!added) {
				/* We reached the start */
				p->next = list;
				list->prev = p;
				list = p;
			}
		}
		/* Skip data for now */
		offset += len;
		count++;
	}
	
	JANUS_LOG(LOG_VERB, "Counted %"SCNu16" RTP packets\n", count);
	janus_recordplay_frame_packet *tmp = list;
	count = 0;
	while(tmp) {
		count++;
		JANUS_LOG(LOG_HUGE, "[%10lu][%4d] seq=%"SCNu16", ts=%"SCNu64"\n", tmp->offset, tmp->len, tmp->seq, tmp->ts);
		tmp = tmp->next;
	}
	JANUS_LOG(LOG_VERB, "Counted %"SCNu16" frame packets\n", count);
	
	/* Done! */
	fclose(file);
	return list;
}

static void *janus_recordplay_playout_thread(void *data) {
	janus_recordplay_session *session = (janus_recordplay_session *)data;
	if(!session) {
		JANUS_LOG(LOG_ERR, "Invalid session, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	if(session->recorder) {
		JANUS_LOG(LOG_ERR, "This is a recorder, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	if(!session->aframes && !session->vframes) {
		JANUS_LOG(LOG_ERR, "No audio and no video frames, can't start playout thread...\n");
		g_thread_unref(g_thread_self());
		return NULL;
	}
	JANUS_LOG(LOG_INFO, "Joining playout thread\n");
	/* Open the files */
	FILE *afile = NULL, *vfile = NULL;
	if(session->aframes) {
		char source[1024];
		if(strstr(session->recording->arc_file, ".mjr"))
			g_snprintf(source, 1024, "%s/%s", recordings_path, session->recording->arc_file);
		else
			g_snprintf(source, 1024, "%s/%s.mjr", recordings_path, session->recording->arc_file);
		afile = fopen(source, "rb");
		if(afile == NULL) {
			JANUS_LOG(LOG_ERR, "Could not open audio file %s, can't start playout thread...\n", source);
			g_thread_unref(g_thread_self());
			return NULL;
		}
	}
	if(session->vframes) {
		char source[1024];
		if(strstr(session->recording->vrc_file, ".mjr"))
			g_snprintf(source, 1024, "%s/%s", recordings_path, session->recording->vrc_file);
		else
			g_snprintf(source, 1024, "%s/%s.mjr", recordings_path, session->recording->vrc_file);
		vfile = fopen(source, "rb");
		if(vfile == NULL) {
			JANUS_LOG(LOG_ERR, "Could not open video file %s, can't start playout thread...\n", source);
			if(afile)
				fclose(afile);
			afile = NULL;
			g_thread_unref(g_thread_self());
			return NULL;
		}
	}
	
	/* Timer */
	gboolean asent = FALSE, vsent = FALSE;
	struct timeval now, abefore, vbefore;
	time_t d_s, d_us;
	gettimeofday(&now, NULL);
	gettimeofday(&abefore, NULL);
	gettimeofday(&vbefore, NULL);

	janus_recordplay_frame_packet *audio = session->aframes, *video = session->vframes;
	char *buffer = (char *)g_malloc0(1500);
	memset(buffer, 0, 1500);
	int bytes = 0;
	int64_t ts_diff = 0, passed = 0;
	
	while(!session->destroyed && session->active && !session->recording->destroyed && (audio || video)) {
		if(!asent && !vsent) {
			/* We skipped the last round, so sleep a bit (5ms) */
			usleep(5000);
		}
		asent = FALSE;
		vsent = FALSE;
		if(audio) {
			if(audio == session->aframes) {
				/* First packet, send now */
				fseek(afile, audio->offset, SEEK_SET);
				bytes = fread(buffer, sizeof(char), audio->len, afile);
				if(bytes != audio->len)
					JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, audio->len);
				/* Update payload type */
				rtp_header *rtp = (rtp_header *)buffer;
				rtp->type = OPUS_PT;	/* FIXME We assume it's Opus */
				if(gateway != NULL)
					gateway->relay_rtp(session->handle, 0, (char *)buffer, bytes);
				gettimeofday(&now, NULL);
				abefore.tv_sec = now.tv_sec;
				abefore.tv_usec = now.tv_usec;
				asent = TRUE;
				audio = audio->next;
			} else {
				/* What's the timestamp skip from the previous packet? */
				ts_diff = audio->ts - audio->prev->ts;
				ts_diff = (ts_diff*1000)/48;	/* FIXME Again, we're assuming Opus and it's 48khz */
				/* Check if it's time to send */
				gettimeofday(&now, NULL);
				d_s = now.tv_sec - abefore.tv_sec;
				d_us = now.tv_usec - abefore.tv_usec;
				if(d_us < 0) {
					d_us += 1000000;
					--d_s;
				}
				passed = d_s*1000000 + d_us;
				if(passed < (ts_diff-5000)) {
					asent = FALSE;
				} else {
					/* Update the reference time */
					abefore.tv_usec += ts_diff%1000000;
					if(abefore.tv_usec > 1000000) {
						abefore.tv_sec++;
						abefore.tv_usec -= 1000000;
					}
					if(ts_diff/1000000 > 0) {
						abefore.tv_sec += ts_diff/1000000;
						abefore.tv_usec -= ts_diff/1000000;
					}
					/* Send now */
					fseek(afile, audio->offset, SEEK_SET);
					bytes = fread(buffer, sizeof(char), audio->len, afile);
					if(bytes != audio->len)
						JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, audio->len);
					/* Update payload type */
					rtp_header *rtp = (rtp_header *)buffer;
					rtp->type = OPUS_PT;	/* FIXME We assume it's Opus */
					if(gateway != NULL)
						gateway->relay_rtp(session->handle, 0, (char *)buffer, bytes);
					asent = TRUE;
					audio = audio->next;
				}
			}
		}
		if(video) {
			if(video == session->vframes) {
				/* First packets: there may be many of them with the same timestamp, send them all */
				uint64_t ts = video->ts;
				while(video && video->ts == ts) {
					fseek(vfile, video->offset, SEEK_SET);
					bytes = fread(buffer, sizeof(char), video->len, vfile);
					if(bytes != video->len)
						JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, video->len);
					/* Update payload type */
					rtp_header *rtp = (rtp_header *)buffer;
					rtp->type = VP8_PT;	/* FIXME We assume it's VP8 */
					if(gateway != NULL)
						gateway->relay_rtp(session->handle, 1, (char *)buffer, bytes);
					video = video->next;
				}
				vsent = TRUE;
				gettimeofday(&now, NULL);
				vbefore.tv_sec = now.tv_sec;
				vbefore.tv_usec = now.tv_usec;
			} else {
				/* What's the timestamp skip from the previous packet? */
				ts_diff = video->ts - video->prev->ts;
				ts_diff = (ts_diff*1000)/90;
				/* Check if it's time to send */
				gettimeofday(&now, NULL);
				d_s = now.tv_sec - vbefore.tv_sec;
				d_us = now.tv_usec - vbefore.tv_usec;
				if(d_us < 0) {
					d_us += 1000000;
					--d_s;
				}
				passed = d_s*1000000 + d_us;
				if(passed < (ts_diff-5000)) {
					vsent = FALSE;
				} else {
					/* Update the reference time */
					vbefore.tv_usec += ts_diff%1000000;
					if(vbefore.tv_usec > 1000000) {
						vbefore.tv_sec++;
						vbefore.tv_usec -= 1000000;
					}
					if(ts_diff/1000000 > 0) {
						vbefore.tv_sec += ts_diff/1000000;
						vbefore.tv_usec -= ts_diff/1000000;
					}
					/* There may be multiple packets with the same timestamp, send them all */
					uint64_t ts = video->ts;
					while(video && video->ts == ts) {
						/* Send now */
						fseek(vfile, video->offset, SEEK_SET);
						bytes = fread(buffer, sizeof(char), video->len, vfile);
						if(bytes != video->len)
							JANUS_LOG(LOG_WARN, "Didn't manage to read all the bytes we needed (%d < %d)...\n", bytes, video->len);
						/* Update payload type */
						rtp_header *rtp = (rtp_header *)buffer;
						rtp->type = VP8_PT;	/* FIXME We assume it's VP8 */
						if(gateway != NULL)
							gateway->relay_rtp(session->handle, 1, (char *)buffer, bytes);
						video = video->next;
					}
					vsent = TRUE;
				}
			}
		}
	}
	
	g_free(buffer);

	/* Get rid of the indexes */
	janus_recordplay_frame_packet *tmp = NULL;
	audio = session->aframes;
	while(audio) {
		tmp = audio->next;
		g_free(audio);
		audio = tmp;
	}
	session->aframes = NULL;
	video = session->vframes;
	while(video) {
		tmp = video->next;
		g_free(video);
		video = tmp;
	}
	session->vframes = NULL;

	if(afile)
		fclose(afile);
	afile = NULL;
	if(vfile)
		fclose(vfile);
	vfile = NULL;

	if(session->recording->destroyed) {
		/* Remove from the list of viewers */
		janus_mutex_lock(&session->recording->mutex);
		session->recording->viewers = g_list_remove(session->recording->viewers, session);
		if(session->recording->viewers == NULL) {
			/* This was the last viewer, destroying the recording */
			JANUS_LOG(LOG_VERB, "Last viewer stopped playout of recording %"SCNu64", destroying it now\n", session->recording->id);
			janus_mutex_unlock(&session->recording->mutex);
			g_free(session->recording->name);
			g_free(session->recording->date);
			g_free(session->recording->arc_file);
			g_free(session->recording->vrc_file);
			g_free(session->recording->offer);
			g_free(session->recording);
			session->recording = NULL;
		} else {
			/* Other viewers still on, don't do anything */
			JANUS_LOG(LOG_VERB, "Recording %"SCNu64" still has viewers, delaying its destruction until later\n", session->recording->id);
			janus_mutex_unlock(&session->recording->mutex);
		}
	}

	/* Tell the core to tear down the PeerConnection, hangup_media will do the rest */
	gateway->close_pc(session->handle);
	
	JANUS_LOG(LOG_INFO, "Leaving playout thread\n");
	g_thread_unref(g_thread_self());
	return NULL;
}
