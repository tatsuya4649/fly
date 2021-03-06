#include "v2.h"
#include "request.h"
#include "response.h"
#include "connect.h"
#include <openssl/err.h>
#include "mime.h"

int fly_hv2_request_event_handler(fly_event_t *e);
int fly_hv2_response_event(fly_event_t *e);
int fly_hv2_response_event_handler(fly_event_t *e, fly_hv2_stream_t *stream);
fly_hv2_stream_t *fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client);
fly_hv2_stream_t *fly_hv2_stream_search_from_sid(fly_hv2_state_t *state, fly_sid_t sid); void fly_send_settings_frame(fly_hv2_stream_t *stream, uint16_t *id, uint32_t *value, size_t count, bool ack);

#define FLY_HV2_SEND_FRAME_SUCCESS			(1)
#define FLY_HV2_SEND_FRAME_BLOCKING			(0)
#define FLY_HV2_SEND_FRAME_ERROR			(-1)
int fly_send_settings_frame_of_server(fly_hv2_stream_t *stream);
#define __FLY_SEND_FRAME_READING_BLOCKING			(2)
#define __FLY_SEND_FRAME_WRITING_BLOCKING			(3)
#define __FLY_SEND_FRAME_DISCONNECT					(4)
#define __FLY_SEND_FRAME_ERROR						(-1)
#define __FLY_SEND_FRAME_SUCCESS					(1)
__fly_static int __fly_send_frame(struct fly_hv2_send_frame *frame);
#define FLY_SEND_FRAME_SUCCESS			(1)
#define FLY_SEND_FRAME_BLOCKING			(0)
#define FLY_SEND_FRAME_ERROR			(-1)
void fly_settings_frame_ack(fly_hv2_stream_t *stream);
void __fly_hv2_add_yet_ack_frame(struct fly_hv2_send_frame *frame);
void fly_received_settings_frame_ack(fly_hv2_stream_t *stream);
void fly_hv2_dynamic_table_init(struct fly_hv2_state *state);
int fly_hv2_parse_headers(fly_hv2_stream_t *stream __fly_unused, uint32_t length __fly_unused, uint8_t *payload, fly_buffer_c *__c __fly_unused);
#define fly_hv2_send_no_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_NO_ERROR, type)
#define fly_hv2_send_protocol_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_PROTOCOL_ERROR, type)
#define fly_hv2_send_internal_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_INTERNAL_ERROR, type)
#define fly_hv2_send_flow_control_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_FLOW_CONTROL_ERROR, type)
#define fly_hv2_send_settings_timeout(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_SETTINGS_TIMEOUT, type)
#define fly_hv2_send_stream_closed(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_STREAM_CLOSED, type)
#define fly_hv2_send_frame_size_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_FRAME_SIZE_ERROR, type)
#define fly_hv2_send_refused_stream(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_REFUSED_STREAM, type)
#define fly_hv2_send_cancel(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_CANCEL, type)
#define fly_hv2_send_compression_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_COMPRESSION_ERROR, type)
#define fly_hv2_send_connect_error(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_CONNECT_ERROR, type)
#define fly_hv2_send_enhance_your_calm(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_ENHANCE_YOUR_CALM, type)
#define fly_hv2_send_inadequate_security(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_INADEQUATE_SECURITY, type)
#define fly_hv2_send_http_1_1_required(__s, type)	\
		__fly_hv2_send_error((__s), FLY_HV2_HTTP_1_1_REQUIRED, type)

void __fly_hv2_send_error(fly_hv2_stream_t *stream, uint32_t code, enum fly_hv2_error_type etype);
void fly_hv2_emergency(fly_event_t *event, fly_hv2_state_t *state);
void fly_hv2_dynamic_table_release(struct fly_hv2_state *state);
static inline void fly_header_hv2_state(fly_hdr_ci *__ci, struct fly_hv2_state *state);

int __fly_hv2_blocking_event(fly_event_t *e, fly_hv2_stream_t *stream);
int fly_hv2_settings_blocking_event(fly_event_t *e, fly_hv2_stream_t *stream){
	return __fly_hv2_blocking_event(e, stream);
}

int fly_hv2_response_blocking_event(fly_event_t *e, fly_hv2_stream_t *stream){
	return __fly_hv2_blocking_event(e, stream);
}

#define FLY_HV2_PARSE_DATA_SUCCESS		0
#define FLY_HV2_PARSE_DATA_ERROR		-1
int fly_hv2_parse_data(fly_event_t *event, fly_hv2_stream_t *stream, uint32_t length, uint8_t *payload, fly_buffer_c *__c);

static inline uint32_t fly_hv2_length_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	uint32_t length=0;
	if (((fly_buf_p) __fh) + (int) (FLY_HV2_FRAME_HEADER_LENGTH_LEN/FLY_HV2_OCTET_LEN) > __c->lptr){
		uint8_t *__l=(uint8_t *) &length;
		uint8_t *__ptr=(uint8_t *) *__fh;
#ifdef FLY_BIG_ENDIAN
		*(__l+0) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__l+1) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__l+2) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
#else
		*(__l+2) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__l+1) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__l+0) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
#endif
	}else{
		uint8_t *__l=(uint8_t *) &length;
#ifdef FLY_BIG_ENDIAN
		*(__l+0) = (uint8_t) (*__fh)[0];
		*(__l+1) = (uint8_t) (*__fh)[1];
		*(__l+2) = (uint8_t) (*__fh)[2];
#else
		*(__l+0) = (uint8_t) (*__fh)[2];
		*(__l+1) = (uint8_t) (*__fh)[1];
		*(__l+2) = (uint8_t) (*__fh)[0];
#endif
	}
	return length;
}

static inline uint8_t fly_hv2_type_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	uint8_t type;

	if ((fly_buf_p) __fh + 3 > __c->lptr){
		__fh = fly_update_chain(&__c, __fh, 3);
		type = (uint8_t) (*__fh)[0];
	}else
		type = (uint8_t) (*__fh)[3];
	return type;
}

static inline uint8_t fly_hv2_flags_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	uint8_t flags;

	if ((fly_buf_p) __fh + 4 > __c->lptr){
		__fh = fly_update_chain(&__c, __fh, 4);
		flags = (uint8_t) (*__fh)[0];
	}else
		flags = (uint8_t) (*__fh)[4];

	return flags;
}

static inline bool fly_hv2_r_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	bool __r;

	if ((fly_buf_p) __fh + 5 > __c->lptr){
		__fh = fly_update_chain(&__c, __fh, 5);
		__r = (*__fh)[0] & 0x1;
	}else
		__r = (*__fh)[5] & 0x1;
	return __r;
}

static inline uint32_t fly_hv2_sid_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	uint32_t __sid=0;
	if ((fly_buf_p) __fh + 5 + (int) ((FLY_HV2_FRAME_HEADER_SID_LEN+FLY_HV2_FRAME_HEADER_R_LEN)/FLY_HV2_OCTET_LEN) > __c->lptr){

		uint8_t *__s = (uint8_t *) &__sid;
		uint8_t *__ptr = (uint8_t *) *__fh;
		__ptr = fly_update_chain(&__c, __ptr, 5);
#ifdef FLY_BIG_ENDIAN
		*(__s+0) = *__ptr & ((1<<7) - 1);
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__s+1) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__s+2) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__s+3) = *__ptr;
#else
		*(__s+3) = *__ptr & ((1<<7) - 1);
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__s+2) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__s+1) = *__ptr;
		__ptr = fly_update_chain(&__c, __ptr, 1);
		*(__s+0) = *__ptr;
#endif
	}else{
		uint8_t *__s = (uint8_t *) &__sid;
#ifdef FLY_BIG_ENDIAN
		/* clear R bit */
		*(__s+0) = (uint8_t) (*__fh)[5] & ((1<<7)-1);
		*(__s+1) = (uint8_t) (*__fh)[6];
		*(__s+2) = (uint8_t) (*__fh)[7];
		*(__s+3) = (uint8_t) (*__fh)[8];
#else
		*(__s+0) = (uint8_t) (*__fh)[8];
		*(__s+1) = (uint8_t) (*__fh)[7];
		*(__s+2) = (uint8_t) (*__fh)[6];
		/* clear R bit */
		*(__s+3) = (uint8_t) (*__fh)[5] & ((1<<7)-1);
#endif
	}
	return __sid;
}

static inline uint8_t *fly_hv2_frame_payload_from_frame_header(fly_hv2_frame_header_t *fh, fly_buffer_c **__c)
{
	if ((fly_buf_p) (fh+1) > (*__c)->lptr){
		fh = fly_update_chain(__c, fh, sizeof(fly_hv2_frame_header_t));
		return (uint8_t *) *fh;
	}else
		return (uint8_t *) (*(fh+1));
}

void fly_hv2_default_settings(fly_hv2_state_t *state)
{
	state->p_header_table_size = FLY_HV2_HEADER_TABLE_SIZE_DEFAULT;
	state->p_max_concurrent_streams = FLY_HV2_MAX_CONCURRENT_STREAMS_DEFAULT;
	state->p_initial_window_size = FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT;
	state->initial_window_size = FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT;
	state->p_max_frame_size = FLY_HV2_MAX_FRAME_SIZE_DEFAULT;
	state->p_max_header_list_size = FLY_HV2_MAX_HEADER_LIST_SIZE_DEFAULT;
	state->p_enable_push = FLY_HV2_ENABLE_PUSH_DEFAULT;
	state->header_table_size = FLY_HV2_HEADER_TABLE_SIZE_DEFAULT;
	state->max_concurrent_streams = FLY_HV2_MAX_CONCURRENT_STREAMS_DEFAULT;
	state->max_frame_size = FLY_HV2_MAX_FRAME_SIZE_DEFAULT;
	state->max_header_list_size = FLY_HV2_MAX_HEADER_LIST_SIZE_DEFAULT;
	state->enable_push = FLY_HV2_ENABLE_PUSH_DEFAULT;
	state->p_window_size = FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT;
	state->window_size = FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT;
}

void __fly_hv2_add_stream(fly_hv2_state_t *state, fly_hv2_stream_t *__s);

void __fly_hv2_add_reserved(fly_hv2_state_t *state, fly_hv2_stream_t *__s)
{
	fly_queue_push(&state->reserved, &__s->rqelem);
	state->reserved_count++;
}

int fly_hv2_stream_create_reserved(fly_hv2_state_t *state, fly_sid_t id, bool from_client)
{
	fly_hv2_stream_t *__s;

	__s = fly_hv2_create_stream(state, id, from_client);
	if (fly_unlikely_null(__s))
		return -1;
	__s->reserved = true;

	__fly_hv2_add_reserved(state, __s);
	return 0;
}

void fly_hv2_create_frame(fly_hv2_stream_t *stream, uint8_t type, uint32_t length, uint8_t flags)
{
	fly_hv2_frame_t *frame;

	frame = fly_pballoc(stream->state->pool, sizeof(struct fly_hv2_frame));
	frame->stream = stream;
	frame->type = type;
	frame->length = length;
	frame->flags = flags;

	fly_queue_init(&frame->felem);
	stream->frame_count++;
	fly_queue_push(&stream->frames, &frame->felem);
}

void fly_hv2_pop_frame(struct fly_hv2_stream *stream)
{
	struct fly_hv2_frame *frame;
	struct fly_queue	 *last;

	last = fly_queue_last(&stream->frames);
	frame = (struct fly_hv2_frame *) fly_queue_data(last, struct fly_hv2_frame, felem);
	fly_queue_pop(&stream->frames);
	stream->frame_count--;
	fly_pbfree(stream->state->pool, frame);
}

void fly_hv2_release_frame(struct fly_hv2_frame *frame)
{
	fly_queue_remove(&frame->felem);
	frame->stream->frame_count--;
	fly_pbfree(frame->stream->state->pool, frame);
}

static fly_hv2_stream_t *__fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client)
{
	fly_hv2_stream_t *__s;
	__s = fly_pballoc(state->pool, sizeof(fly_hv2_stream_t));
	__s->id = id;
	__s->dependency_id = FLY_HV2_STREAM_ROOT_ID;
	fly_bllist_init(&__s->blelem);
	__s->from_client = from_client ? 1 : 0;
	__s->dep_count = 0;
	__s->dnext = __s;
	__s->dprev = __s;
	__s->deps = __s;
	__s->stream_state = FLY_HV2_STREAM_STATE_IDLE;
	__s->reserved = false;
	__s->state = state;
	fly_queue_init(&__s->frames);
	fly_queue_init(&__s->yetsend);
	fly_bllist_init(&__s->yetack);
	__s->weight = FLY_HV2_STREAM_DEFAULT_WEIGHT;
	__s->frame_count = 0;
	__s->yetsend_count = 0;
	__s->window_size = state->initial_window_size;
	__s->p_window_size = state->p_initial_window_size;
	__s->yetack_count = 0;
	__s->end_send_headers = false;
	__s->end_send_data = false;
	__s->can_response = false;
	__s->end_request_response = false;
	__s->exclusive = false;
	__s->peer_end_headers = false;

	__s->request = fly_request_init(state->connect);
	if (fly_unlikely_null(__s->request))
		return NULL;
	__s->request->stream = __s;
	fly_queue_init(&__s->rqelem);

	return __s;
}

void __fly_hv2_add_stream(fly_hv2_state_t *state, fly_hv2_stream_t *__s)
{
	fly_bllist_add_tail(&state->streams, &__s->blelem);
	state->stream_count++;
}

void __fly_hv2_remove_stream(fly_hv2_state_t *state, fly_hv2_stream_t *__s)
{
#ifdef DEBUG
	assert(state->stream_count > 0);
#endif

	fly_bllist_remove(&__s->blelem);
	state->stream_count--;
	return;
}

fly_hv2_stream_t *fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client)
{
	fly_hv2_stream_t *ns;
	if (state->max_sid >= id){
		fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
		return NULL;
	}
	if (state->max_concurrent_streams != (fly_settings_t) FLY_HV2_MAX_CONCURRENT_STREAMS_INFINITY && \
			(fly_settings_t) state->stream_count >= state->max_concurrent_streams){
		fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
		return NULL;
	}

	/*
	 *	stream id:
	 *	from client -> must be odd.
	 *	from server -> must be even.
	 */
	if (id && from_client && id%2 == 0){
		fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
		return NULL;
	}else if (id && !from_client && id%2){
		fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
		return NULL;
	}

	ns = __fly_hv2_create_stream(state, id, from_client);
	if (fly_unlikely_null(ns))
		return NULL;
	ns->request->header = fly_header_init(ns->request->ctx);
	if (fly_unlikely_null(ns->request->header))
		return NULL;
	__fly_hv2_add_stream(state, ns);
	state->max_sid = id;

	return ns;
}

void fly_hv2_send_frame_release(struct fly_hv2_send_frame *__f);
void fly_hv2_send_frame_release_noqueue_remove(struct fly_hv2_send_frame *__f);;

int fly_hv2_close_stream(fly_hv2_stream_t *stream)
{
	fly_hv2_state_t *state;

	state = stream->state;

	__fly_hv2_remove_stream(state, stream);
	/* All frames remove */
	if (stream->frame_count > 0){
		struct fly_queue *__n;
		struct fly_hv2_frame *__f;

		for (struct fly_queue *__q=stream->frames.next; __q!=&stream->frames; __q=__n){
			__n = __q->next;
			__f = fly_queue_data(__q, struct fly_hv2_frame, felem);
			fly_hv2_release_frame(__f);
		}
	}
	/* All yet send frames remove */
	if (stream->yetsend_count > 0){
		struct fly_hv2_send_frame *__f;
		struct fly_queue *__n;

		for (struct fly_queue *__q=stream->yetsend.next; __q!=&stream->yetsend; __q=__n){
			__n = __q->next;

			__f = fly_queue_data(__q, struct fly_hv2_send_frame, qelem);
			fly_queue_remove(&__f->sqelem);
			fly_hv2_send_frame_release_noqueue_remove(__f);
		}
	}
	/* All yet ack frames remove */
	if (stream->yetack_count > 0){
		struct fly_bllist *__n;
		struct fly_hv2_send_frame *__f;
		for (struct fly_bllist *__b=stream->yetack.next; __b!=&stream->yetack; __b=__n){
			__n = __b->next;

			__f = fly_bllist_data(__b, struct fly_hv2_send_frame, aqelem);
			fly_hv2_send_frame_release_noqueue_remove(__f);
		}
	}
	/*
	 * Request release. The only place to release request resource in HTTP2.
	 */
	if (stream->request)
		fly_request_release(stream->request);

	fly_pbfree(state->pool, stream);
	return 0;
}

#define FLY_HV2_INIT_CONNECTION_PREFACE_CONTINUATION	(0)
#define FLY_HV2_INIT_CONNECTION_PREFACE_ERROR			(-1)
#define FLY_HV2_INIT_CONNECTION_PREFACE_SUCCESS			(1)
int fly_hv2_init_connection_preface(fly_connect_t *conn)
{
#ifdef DEBUG
	printf("WORKER: check HTTP2 connection preface\n");
#endif
	fly_buffer_t *buf = conn->buffer;

	switch(fly_buffer_memcmp(FLY_CONNECTION_PREFACE, fly_buffer_first_useptr(buf), fly_buffer_first_chain(buf), strlen(FLY_CONNECTION_PREFACE))){
	case FLY_BUFFER_MEMCMP_OVERFLOW:
		return FLY_HV2_INIT_CONNECTION_PREFACE_CONTINUATION;
	case FLY_BUFFER_MEMCMP_EQUAL:
		return FLY_HV2_INIT_CONNECTION_PREFACE_SUCCESS;
	default:
		return FLY_HV2_INIT_CONNECTION_PREFACE_ERROR;
	}
	FLY_NOT_COME_HERE
}

/*
 *	the first event after connected.
 */

/*
 *	Init handler of HTTP2 connection state.
 *	alloc resources:
 *	@ state:				struct fly_hv2_state
 *	@ state->streams:		struct fly_hv2_stream
 *	@ state->responses:		struct fly_hv2_response
 *	@ state->dtable:		struct fly_hv2_dynamic_table
 *	@ state->emergency_ptr:	struct fly_hv2_send_frame_
 *	pool:
 *	@connect->pool
 */
#define FLY_HV2_STATE_EMERGENCY_MEMORY_SIZE			100
#define FLY_HV2_STATE_EMEPTR_MIN					\
			((size_t) FLY_HV2_STATE_EMERGENCY_MEMORY_SIZE > sizeof(struct fly_hv2_send_frame) ? FLY_HV2_STATE_EMERGENCY_MEMORY_SIZE : sizeof(struct fly_hv2_send_frame))
fly_hv2_state_t *fly_hv2_state_init(fly_connect_t *conn)
{
	fly_hv2_state_t *state;
	fly_hv2_stream_t *roots;

	state = fly_pballoc(conn->pool, sizeof(fly_hv2_state_t));
	state->pool = conn->pool;
	fly_hv2_default_settings(state);

	state->connect = conn;
	/* create root stream(0x0) */
	roots = __fly_hv2_create_stream(state, FLY_HV2_STREAM_ROOT_ID, true);
	if (fly_unlikely_null(roots))
		goto streams_error;

	fly_bllist_init(&state->streams);
	fly_bllist_add_head(&state->streams, &roots->blelem);
	state->stream_count = 1;
	fly_queue_init(&state->reserved);
	fly_queue_init(&state->responses);

	fly_queue_init(&state->send);
	state->send_count = 0;

	fly_queue_init(&state->responses);
	state->reserved_count = 0;
	state->connection_state = FLY_HV2_CONNECTION_STATE_INIT;
	state->max_sid = FLY_HV2_STREAM_ROOT_ID;
	state->max_handled_sid = 0;
	state->goaway = false;
	state->goaway_lsid = 0;
	state->response_count = 0;
	fly_hv2_dynamic_table_init(state);
	state->emergency_ptr = fly_pballoc(state->pool, FLY_HV2_STATE_EMEPTR_MIN);
	state->first_send_settings = false;

	conn->v2_state = state;
	return state;

streams_error:
	fly_pbfree(conn->pool, state);
	return NULL;
}

/*
 *	release handler of HTTP2 connection state.
 */
void fly_hv2_dynamic_table_release(struct fly_hv2_state *state);
void fly_hv2_remove_all_response(fly_hv2_state_t *state, struct fly_event *e);
void fly_hv2_state_release(fly_hv2_state_t *state, struct fly_event *e)
{
	fly_hv2_remove_all_response(state, e);
	fly_hv2_dynamic_table_release(state);
	fly_pbfree(state->pool, state->emergency_ptr);
	fly_pbfree(state->pool, state);
}

int fly_receive_v2(fly_sock_t fd, fly_connect_t *connect)
{
#ifdef DEBUG
	assert(connect != NULL);
	assert(connect->buffer != NULL);
#endif

	fly_buffer_t *__buf;

	__buf = connect->buffer;
	if (fly_unlikely(__buf->chain_count == 0)){
		struct fly_err *__err;
		__err = fly_err_init(
			connect->pool, 0, FLY_ERR_ERR,
			"http2 request receive no buffer chain error in receiving request . (%s: %d)",
			__FILE__, __LINE__
		);
		fly_error_error(__err);
	}

#ifdef DEBUG
	printf("Now can receive %d\n", fly_can_recv(fd));
#endif
	int recvlen=0, total=0;
	if (FLY_CONNECT_ON_SSL(connect)){
		SSL *ssl = connect->ssl;

#ifdef DEBUG
		assert(ssl != NULL);
		assert(fly_buffer_lunuse_ptr(__buf) != NULL);
		assert(fly_buffer_lunuse_len(__buf) > 0);
#endif
		recvlen = SSL_read(ssl, fly_buffer_lunuse_ptr(__buf),  fly_buffer_lunuse_len(__buf));
		switch(SSL_get_error(ssl, recvlen)){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_ZERO_RETURN:
			goto end_of_connection;
		case SSL_ERROR_WANT_READ:
			goto read_blocking;
		case SSL_ERROR_WANT_WRITE:
			goto write_blocking;
		case SSL_ERROR_SYSCALL:
			if (errno == EPIPE || errno == 0)
				goto end_of_connection;
			goto error;
		case SSL_ERROR_SSL:
			goto error;
		default:
			/* unknown error */
			goto error;
		}
	}else{
retry:
		recvlen = recv(fd, fly_buffer_lunuse_ptr(__buf), fly_buffer_lunuse_len(__buf), MSG_DONTWAIT);
		switch(recvlen){
		case 0:
			goto end_of_connection;
		case -1:
			if (errno == EINTR)
				goto retry;
			else if FLY_BLOCKING(recvlen)
				goto read_blocking;
			else if (errno == ECONNREFUSED)
				goto end_of_connection;
			else{
				struct fly_err *__err;
				__err = fly_err_init(
					connect->pool, errno, FLY_ERR_ERR,
					"recv error in receiving request (HTTP2)."
				);
				fly_error_error(__err);

				FLY_NOT_COME_HERE
			}
		default:
			break;
		}
	}
	total += recvlen;
	switch(fly_update_buffer(__buf, recvlen)){
	case FLY_BUF_ADD_CHAIN_SUCCESS:
		break;
	case FLY_BUF_ADD_CHAIN_LIMIT:
		goto overflow;
	case FLY_BUF_ADD_CHAIN_ERROR:
		goto buffer_error;
	default:
		FLY_NOT_COME_HERE
	}

	goto continuation;
end_of_connection:
	connect->peer_closed = true;
	return FLY_REQUEST_RECEIVE_END;
continuation:
	return FLY_REQUEST_RECEIVE_SUCCESS;
error:
	return FLY_REQUEST_RECEIVE_ERROR;
read_blocking:
	if (total > 0)
		goto continuation;
	return FLY_REQUEST_RECEIVE_READ_BLOCKING;
write_blocking:
	return FLY_REQUEST_RECEIVE_WRITE_BLOCKING;
buffer_error:
	return FLY_REQUEST_RECEIVE_ERROR;
overflow:
	return FLY_REQUEST_RECEIVE_OVERFLOW;
}

int fly_hv2_init_handler(fly_event_t *e)
{
#if !defined FLY_BIG_ENDIAN && !defined FLY_LITTLE_ENDIAN
	/* can't recognize endian */
	assert(0);
#endif
#ifdef DEBUG
	printf("WORKER: HTTP2 init handler\n");
#endif
	fly_connect_t *conn;
	fly_buffer_t *buf;

	//conn = (fly_connect_t *) e->event_data;
	conn = (fly_connect_t *) fly_event_data_get(e, __p);
	buf = conn->buffer;

	if ((size_t) (buf->per_len*buf->chain_max)<FLY_HV2_MAX_FRAME_SIZE_DEFAULT){
		buf->chain_max = ((FLY_HV2_MAX_FRAME_SIZE_DEFAULT/buf->per_len) + 1);
#ifdef DEBUG
		assert((buf->chain_max*buf->per_len) >= FLY_HV2_MAX_FRAME_SIZE_DEFAULT);
#endif
	}

	if (!conn->v2_state){
		if (fly_unlikely_null(fly_hv2_state_init(conn))){
			struct fly_err *__err;
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"state init error."
			);
			fly_event_error_add(e, __err);
			goto error;
		}
	}

	switch(fly_receive_v2(conn->c_sockfd, conn)){
	case FLY_REQUEST_RECEIVE_ERROR:
		goto error;
	case FLY_REQUEST_RECEIVE_SUCCESS:
		break;
	case FLY_REQUEST_RECEIVE_END:
		goto disconnect;
	case FLY_REQUEST_RECEIVE_READ_BLOCKING:
		goto read_continuation;
	case FLY_REQUEST_RECEIVE_WRITE_BLOCKING:
		goto write_continuation;
	case FLY_REQUEST_RECEIVE_OVERFLOW:
		goto overflow;
	default:
		FLY_NOT_COME_HERE
	}

connection_preface:
	/* invalid connection preface */
	switch (fly_hv2_init_connection_preface(conn)){
	case FLY_HV2_INIT_CONNECTION_PREFACE_CONTINUATION:
		goto read_continuation;
	case FLY_HV2_INIT_CONNECTION_PREFACE_ERROR:
		/* state->goaway is on */
		fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(conn->v2_state), FLY_HV2_CONNECTION_ERROR);
		break;
	case FLY_HV2_INIT_CONNECTION_PREFACE_SUCCESS:
		break;
	default:
		FLY_NOT_COME_HERE
	}

	/* release connection preface from buffer */
	fly_buffer_chain_release_from_length(fly_buffer_first_chain(buf), strlen(FLY_CONNECTION_PREFACE));

	conn->v2_state->connection_state = FLY_HV2_CONNECTION_STATE_CONNECTION_PREFACE;
	return fly_hv2_request_event_handler(e);

write_continuation:
	e->read_or_write |= FLY_WRITE;
	goto continuation;
read_continuation:
	e->read_or_write |= FLY_READ;
	goto continuation;
continuation:
	if (conn->buffer->use_len > strlen(FLY_CONNECTION_PREFACE))
		goto connection_preface;
	//e->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	fly_event_state_set(e, __e, EFLY_REQUEST_STATE_CONT);
	e->flag = FLY_MODIFY;
	FLY_EVENT_HANDLER(e, fly_hv2_init_handler);
	e->tflag = FLY_INHERIT;
	e->available = false;
	fly_event_socket(e);
	return fly_event_register(e);

error:
overflow:
disconnect:
	fly_hv2_state_release(conn->v2_state, e);
	fly_connect_release(conn);

	e->tflag = 0;
	e->flag = FLY_CLOSE_EV;
	return 0;
}

struct fly_hv2_send_frame *fly_hv2_send_frame_init(fly_hv2_stream_t *stream)
{
	struct fly_hv2_send_frame *frame;

	frame = fly_pballoc(stream->state->pool, sizeof(struct fly_hv2_send_frame));
	frame->stream = stream;
	frame->pool = stream->state->pool;
	frame->sid = stream->id;
	frame->payload = NULL;
	frame->payload_len = 0;
	frame->type = 0;
	frame->send_len = 0;
	frame->can_send_len = 0;
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->need_ack = false;
	fly_queue_init(&frame->qelem);
	fly_queue_init(&frame->sqelem);
	return frame;
}

void fly_hv2_send_frame_release_only_resource(struct fly_hv2_send_frame *__f)
{
	if (__f->payload)
		fly_pbfree(__f->pool, __f->payload);
	fly_pbfree(__f->pool, __f);
}

void fly_hv2_send_frame_release_noqueue_remove(struct fly_hv2_send_frame *__f)
{
	if (__f->need_ack)
		__f->stream->yetack_count--;
	__f->stream->yetsend_count--;
	__f->stream->state->send_count--;
	fly_hv2_send_frame_release_only_resource(__f);
}

void fly_hv2_send_frame_release(struct fly_hv2_send_frame *__f)
{
	fly_queue_remove(&__f->qelem);
	fly_queue_remove(&__f->sqelem);
	if (__f->need_ack)
		fly_bllist_remove(&__f->aqelem);

	fly_hv2_send_frame_release_noqueue_remove(__f);
}

static inline uint8_t fly_hv2_pad_length(uint8_t **pl, fly_buffer_c **__c)
{
	uint8_t pad_length=0;
	if ((fly_buf_p) (*pl+1) > (*__c)->lptr){
		pad_length |= (uint8_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		pad_length |= (uint8_t) (*pl)[0];

		*pl += (int) FLY_HV2_FRAME_TYPE_HEADERS_PAD_LENGTH_LEN/FLY_HV2_OCTET_LEN;
	}

	return pad_length;
}

static inline bool fly_hv2_flag(uint8_t **pl)
{
	return (**pl & (1<<7)) ? true : false;
}

static inline uint64_t fly_opaque_data(uint8_t **pl, fly_buffer_c **__c)
{
	uint64_t opaque_data=0;

	if ((fly_buf_p) (*pl+8) > (*__c)->lptr){
		uint8_t *__o = (uint8_t *) &opaque_data;
#ifdef FLY_BIG_ENDIAN
		*(__o + 0) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 1) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 2) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 3) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 4) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 5) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 6) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 7) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
#else
		*(__o + 7) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 6) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 5) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 4) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 3) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 2) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 1) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__o + 0) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
#endif
	}else{
		uint8_t *__o = (uint8_t *) &opaque_data;
#ifdef FLY_BIG_ENDIAN
		*(__o + 0) = (uint8_t) (*pl)[0];
		*(__o + 1) = (uint8_t) (*pl)[1];
		*(__o + 2) = (uint8_t) (*pl)[2];
		*(__o + 3) = (uint8_t) (*pl)[3];
		*(__o + 4) = (uint8_t) (*pl)[4];
		*(__o + 5) = (uint8_t) (*pl)[5];
		*(__o + 6) = (uint8_t) (*pl)[6];
		*(__o + 7) = (uint8_t) (*pl)[7];
#else
		*(__o + 0) = (uint8_t) (*pl)[7];
		*(__o + 1) = (uint8_t) (*pl)[6];
		*(__o + 2) = (uint8_t) (*pl)[5];
		*(__o + 3) = (uint8_t) (*pl)[4];
		*(__o + 4) = (uint8_t) (*pl)[3];
		*(__o + 5) = (uint8_t) (*pl)[2];
		*(__o + 6) = (uint8_t) (*pl)[1];
		*(__o + 7) = (uint8_t) (*pl)[0];
#endif

		*pl += (int) (FLY_HV2_FRAME_TYPE_PING_OPAQUE_DATA_LEN)/FLY_HV2_OCTET_LEN;
	}
	return opaque_data;
}

static inline uint32_t __fly_hv2_32bit(uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t uint32 = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		uint8_t *__u = (uint8_t *) &uint32;
#ifdef FLY_BIG_ENDIAN
		*(__u+0) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+1) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+2) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+3) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
#else
		*(__u+3) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+2) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+1) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+0) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
#endif
	}else{
		uint8_t *__u = (uint8_t *) &uint32;
#ifdef FLY_BIG_ENDIAN
		*(__u+0) = (uint8_t) (*pl)[0];
		*(__u+1) = (uint8_t) (*pl)[1];
		*(__u+2) = (uint8_t) (*pl)[2];
		*(__u+3) = (uint8_t) (*pl)[3];
#else
		*(__u+0) = (uint8_t) (*pl)[3];
		*(__u+1) = (uint8_t) (*pl)[2];
		*(__u+2) = (uint8_t) (*pl)[1];
		*(__u+3) = (uint8_t) (*pl)[0];
#endif

		*pl += (int) (FLY_HV2_FRAME_TYPE_PRIORITY_E_LEN+FLY_HV2_FRAME_TYPE_PRIORITY_STREAM_DEPENDENCY_LEN)/FLY_HV2_OCTET_LEN;
	}
	return uint32;
}

static inline uint32_t fly_hv2_31bit_nb(uint8_t *pl)
{
	uint32_t uint31 = 0;
	uint8_t *__u = (uint8_t *) &uint31;
#ifdef FLY_BIG_ENDIAN
	*(__u+0) = (uint8_t) pl[0] & ((1<<7)-1);
	*(__u+1) = (uint8_t) pl[1];
	*(__u+2) = (uint8_t) pl[2];
	*(__u+3) = (uint8_t) pl[3];
#else
	*(__u+0) = (uint8_t) pl[3];
	*(__u+1) = (uint8_t) pl[2];
	*(__u+2) = (uint8_t) pl[1];
	*(__u+3) = (uint8_t) pl[0] & ((1<<7)-1);
#endif
	return uint31;
}

static inline uint32_t __fly_hv2_31bit(uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t uint31 = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		uint8_t *__u = (uint8_t *) &uint31;
#ifdef FLY_BIG_ENDIAN
		*(__u+0) = **pl & ((1<<7)-1);
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+1) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+2) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+3) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
#else
		*(__u+3) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+2) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+1) = **pl;
		*pl = fly_update_chain(__c, *pl, 1);
		*(__u+0) = **pl & ((1<<7)-1);
		*pl = fly_update_chain(__c, *pl, 1);
#endif
	}else{
		uint8_t *__u = (uint8_t *) &uint31;
#ifdef FLY_BIG_ENDIAN
		*(__u+0) = (uint8_t) (*pl)[0] & ((1<<7)-1);
		*(__u+1) = (uint8_t) (*pl)[1];
		*(__u+2) = (uint8_t) (*pl)[2];
		*(__u+3) = (uint8_t) (*pl)[3];
#else
		*(__u+0) = (uint8_t) (*pl)[3];
		*(__u+1) = (uint8_t) (*pl)[2];
		*(__u+2) = (uint8_t) (*pl)[1];
		*(__u+3) = (uint8_t) (*pl)[0] & ((1<<7)-1);
#endif

		*pl += (int) (FLY_HV2_FRAME_TYPE_PRIORITY_E_LEN+FLY_HV2_FRAME_TYPE_PRIORITY_STREAM_DEPENDENCY_LEN)/FLY_HV2_OCTET_LEN;
	}
	return uint31;
}

static inline uint32_t fly_hv2_error_code(uint8_t **pl, fly_buffer_c **__c)
{
	return __fly_hv2_32bit(pl, __c);
}

static inline uint32_t fly_hv2_last_sid(uint8_t **pl, fly_buffer_c **__c)
{
	return __fly_hv2_31bit(pl, __c);
}

static inline uint32_t fly_hv2_window_size_increment(uint8_t **pl, fly_buffer_c **__c)
{
	return __fly_hv2_31bit(pl, __c);
}

static inline uint32_t fly_hv2_stream_dependency(uint8_t **pl, fly_buffer_c **__c)
{
	return __fly_hv2_31bit(pl, __c);
}

static inline uint16_t fly_hv2_weight(uint8_t **pl, fly_buffer_c **__c)
{
	uint8_t weight = 0;

	if ((fly_buf_p) (*pl+1) > (*__c)->lptr){
		weight |= (uint8_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		weight |= (uint8_t) (*pl)[0];

		*pl += (int) FLY_HV2_FRAME_TYPE_PRIORITY_WEIGHT_LEN/FLY_HV2_OCTET_LEN;
	}
	return (uint16_t) weight+1;
}


static void __fly_hv2_priority_deps_add(fly_hv2_stream_t *dist, fly_hv2_stream_t *src)
{
	fly_hv2_stream_t *n;
	for (n=dist->deps; n->dnext!=dist->deps; n=n->dnext){
		if (n->weight < src->weight){
			n->dnext->dprev = src;
			src->dnext =  n->dnext;
			src->dprev = n;
			if (n == dist->deps)
				dist->deps = src;
			else
				n->dnext = src;
			src->dep_count++;
			return;
		}
	}

	n->dnext = src;
	src->dprev = n;
	src->dnext = dist->deps;
	src->dep_count++;
	return;
}

void fly_hv2_priority_rebalance(fly_hv2_stream_t *__s)
{
	fly_sid_t dsid;
	fly_hv2_stream_t *__p;

	dsid = __s->dependency_id;
	__p = (dsid == FLY_HV2_STREAM_ROOT_ID) ? FLY_HV2_ROOT_STREAM(__s->state) : fly_hv2_stream_search_from_sid(__s->state, dsid);

	if (__s->exclusive){
		if (__p->dep_count)
			__fly_hv2_priority_deps_add(__p, __s);
		__p->deps = __s;
		__p->dep_count = 1;
	}else{
		if (__p->dep_count == 0){
			__p->deps = __s;
			__s->dnext = __p->deps;
			__s->dprev = __p->deps;
			__p->dep_count = 1;
		}else
			__fly_hv2_priority_deps_add(__p, __s);
	}
}

static inline uint16_t fly_hv2_settings_id(uint8_t **pl, fly_buffer_c **__c)
{
	uint16_t id = 0;
	if ((fly_buf_p) (*pl+2) > (*__c)->lptr){
		uint8_t *__i=(uint8_t *) &id;
#ifdef FLY_BIG_ENDIAN
		*(__i+0) = (uint8_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__i+1) = (uint8_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
#else
		*(__i+1) = (uint8_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__i+0) = (uint8_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
#endif
	}else{
		uint8_t *__i=(uint8_t *) &id;
#ifdef FLY_BIG_ENDIAN
		*(__i+0) = (uint8_t) (*pl)[0];
		*(__i+1) = (uint8_t) (*pl)[1];
#else
		*(__i+0) = (uint8_t) (*pl)[1];
		*(__i+1) = (uint8_t) (*pl)[0];
#endif
		*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_ID_LEN/FLY_HV2_OCTET_LEN);
	}
	return id;
}

static inline uint16_t fly_hv2_settings_id_nb(uint8_t **pl)
{
	uint16_t id = 0;
	uint8_t *__i=(uint8_t *) &id;
#ifdef FLY_BIG_ENDIAN
	*(__i+0) = (*pl)[0];
	*(__i+1) = (*pl)[1];
#else
	*(__i+0) = (*pl)[1];
	*(__i+1) = (*pl)[0];
#endif
	*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_ID_LEN/FLY_HV2_OCTET_LEN);
	return id;
}

static inline uint32_t fly_hv2_settings_value(__fly_unused uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t value = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		uint8_t *__v = (uint8_t *) &value;
#ifdef FLY_BIG_ENDIAN
		*(__v+0) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__v+1) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__v+2) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__v+3) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
#else
		*(__v+3) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__v+2) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__v+1) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
		*(__v+0) = (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
#endif
	}else{
		uint8_t *__v = (uint8_t *) &value;
#ifdef FLY_BIG_ENDIAN
		*(__v + 0) = (*pl)[0];
		*(__v + 1) = (*pl)[1];
		*(__v + 2) = (*pl)[2];
		*(__v + 3) = (*pl)[3];
#else
		*(__v + 0) = (*pl)[3];
		*(__v + 1) = (*pl)[2];
		*(__v + 2) = (*pl)[1];
		*(__v + 3) = (*pl)[0];
#endif
		*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_VALUE_LEN/FLY_HV2_OCTET_LEN);
	}
	return value;
}

static inline uint16_t fly_hv2_settings_value_nb(uint8_t **pl)
{
	uint32_t value = 0;
	uint8_t *__v = (uint8_t *) &value;
#ifdef FLY_BIG_ENDIAN
	*(__v + 0) = (*pl)[0];
	*(__v + 1) = (*pl)[1];
	*(__v + 2) = (*pl)[2];
	*(__v + 3) = (*pl)[3];
#else
	*(__v + 0) = (*pl)[3];
	*(__v + 1) = (*pl)[2];
	*(__v + 2) = (*pl)[1];
	*(__v + 3) = (*pl)[0];
#endif
	*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_VALUE_LEN/FLY_HV2_OCTET_LEN);
	return value;
}

#define FLY_HV2_SETTINGS_SUCCESS				(0)
int fly_hv2_peer_settings(fly_hv2_state_t *state, __fly_unused fly_sid_t sid, uint8_t *payload, uint32_t len, fly_buffer_c *__c)
{
	uint32_t total=0;

	while(total<len){
		uint16_t id = fly_hv2_settings_id(&payload, &__c);
		uint32_t value = fly_hv2_settings_value(&payload, &__c);

		switch(id){
		case FLY_HV2_SETTINGS_FRAME_SETTINGS_HEADER_TABLE_SIZE:
			state->p_header_table_size = value;
			break;
		case FLY_HV2_SETTINGS_FRAME_SETTINGS_ENABLE_PUSH:
			if (!(value == 0 || value ==1))
				return FLY_HV2_PROTOCOL_ERROR;
			state->p_enable_push = value;
			break;
		case FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_CONCURRENT_STREAMS:
			state->p_max_concurrent_streams = value;
			break;
		case FLY_HV2_SETTINGS_FRAME_SETTINGS_INITIAL_WINDOW_SIZE:
			if (value > FLY_HV2_WINDOW_SIZE_MAX)
				return FLY_HV2_FLOW_CONTROL_ERROR;

			state->p_initial_window_size = value - (state->p_initial_window_size - state->window_size);
			break;
		case FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_FRAME_SIZE:
			if (value > FLY_HV2_MAX_FRAME_SIZE_MAX)
				state->p_max_frame_size = FLY_HV2_MAX_FRAME_SIZE_MAX;
			else
				state->p_max_frame_size = value;
			break;
		case FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_HEADER_LIST_SIZE:
			if (!(value >= FLY_HV2_MAX_FRAME_SIZE_DEFAULT && value <= FLY_HV2_MAX_FRAME_SIZE_MAX))
				return FLY_HV2_PROTOCOL_ERROR;
			state->p_max_header_list_size = value;
			break;
		default:
			/* unknown setting */
			break;
		}

		total += (sizeof(id) + sizeof(value));
	}
	return FLY_HV2_SETTINGS_SUCCESS;
}

void fly_fh_setting(fly_hv2_frame_header_t *__fh, uint32_t length, uint8_t type, uint8_t flags, bool r, uint32_t sid);
void __fly_hv2_add_yet_send_frame(struct fly_hv2_send_frame *frame);

int fly_send_ping_frame(fly_hv2_state_t *state, uint64_t opaque_data)
{
	struct fly_hv2_send_frame *frame;

	frame = fly_hv2_send_frame_init(FLY_HV2_ROOT_STREAM(state));
	if (fly_unlikely_null(frame))
		return -1;

	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_GOAWAY;
	frame->payload_len = \
		(int) (FLY_HV2_FRAME_TYPE_PING_OPAQUE_DATA_LEN)/FLY_HV2_OCTET_LEN;
	frame->payload = fly_pballoc(frame->pool, frame->payload_len);
	if (fly_unlikely_null(frame->payload))
		return -1;
	memset(frame->payload, '\0', frame->payload_len);

	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, 0, false, frame->sid);

	/* opeque data setting */
	uint8_t *ptr = frame->payload;
#ifdef FLY_BIG_ENDIAN
	*ptr++ = *(((uint8_t *) &opaque_data)+0);
	*ptr++ = *(((uint8_t *) &opaque_data)+1);
	*ptr++ = *(((uint8_t *) &opaque_data)+2);
	*ptr++ = *(((uint8_t *) &opaque_data)+3);
	*ptr++ = *(((uint8_t *) &opaque_data)+4);
	*ptr++ = *(((uint8_t *) &opaque_data)+5);
	*ptr++ = *(((uint8_t *) &opaque_data)+6);
	*ptr   = *(((uint8_t *) &opaque_data)+7);
#else
	*ptr++ = *(((uint8_t *) &opaque_data)+7);
	*ptr++ = *(((uint8_t *) &opaque_data)+6);
	*ptr++ = *(((uint8_t *) &opaque_data)+5);
	*ptr++ = *(((uint8_t *) &opaque_data)+4);
	*ptr++ = *(((uint8_t *) &opaque_data)+3);
	*ptr++ = *(((uint8_t *) &opaque_data)+2);
	*ptr++ = *(((uint8_t *) &opaque_data)+1);
	*ptr   = *(((uint8_t *) &opaque_data)+0);
#endif
	__fly_hv2_add_yet_send_frame(frame);
	return 0;
}

void __fly_goaway_payload(struct fly_hv2_send_frame *frame, fly_hv2_state_t *state, bool r, uint32_t error_code)
{
	/* copy error_code/last-stream-id/r to payload */
	uint8_t *ptr = frame->payload;
	uint32_t *lsid = &state->max_handled_sid;
	uint32_t *ecode = &error_code;

	*ptr = r ? (1<<7) : 0;
#ifdef FLY_BIG_ENDIAN
	*ptr++ |= *(((uint8_t *) lsid)+0);
	*ptr++  = *(((uint8_t *) lsid)+1);
	*ptr++  = *(((uint8_t *) lsid)+2);
	*ptr++  = *(((uint8_t *) lsid)+3);
	*ptr++  = *(((uint8_t *) ecode)+0);
	*ptr++  = *(((uint8_t *) ecode)+1);
	*ptr++  = *(((uint8_t *) ecode)+2);
	*ptr    = *(((uint8_t *) ecode)+3);
#else
	*ptr++ |= *(((uint8_t *) lsid)+3);
	*ptr++  = *(((uint8_t *) lsid)+2);
	*ptr++  = *(((uint8_t *) lsid)+1);
	*ptr++  = *(((uint8_t *) lsid)+0);
	*ptr++  = *(((uint8_t *) ecode)+3);
	*ptr++  = *(((uint8_t *) ecode)+2);
	*ptr++  = *(((uint8_t *) ecode)+1);
	*ptr    = *(((uint8_t *) ecode)+0);
#endif
	state->goaway = true;
}

int fly_send_goaway_frame(fly_hv2_state_t *state, bool r, uint32_t error_code)
{
	struct fly_hv2_send_frame *frame;

	/* root stream(0x0) */
	frame = fly_hv2_send_frame_init(FLY_HV2_ROOT_STREAM(state));
	if (fly_unlikely_null(frame))
		return -1;

	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_GOAWAY;
	frame->payload_len = \
		(int) (FLY_HV2_FRAME_TYPE_GOAWAY_R_LEN+FLY_HV2_FRAME_TYPE_GOAWAY_LSID_LEN+FLY_HV2_FRAME_TYPE_GOAWAY_ERROR_CODE_LEN)/FLY_HV2_OCTET_LEN;
	frame->payload = fly_pballoc(frame->pool, frame->payload_len);
	if (fly_unlikely_null(frame->payload))
		return -1;

	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, 0, false, frame->sid);
	__fly_goaway_payload(frame, state, r, error_code);
	__fly_hv2_add_yet_send_frame(frame);

	return 0;
}

int fly_send_window_update_frame(fly_hv2_stream_t *stream, uint32_t update_size, bool r)
{
	struct fly_hv2_send_frame *frame;

#ifdef DEBUG

	assert(update_size >= 1 && update_size <= FLY_HV2_WINDOW_SIZE_MAX);
#endif
	frame = fly_hv2_send_frame_init(stream);
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_WINDOW_UPDATE;
	frame->payload_len = \
		(int) (FLY_HV2_FRAME_TYPE_WINDOW_UPDATE_LENGTH);
	frame->payload = fly_pballoc(frame->pool, frame->payload_len);

	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, 0, false, frame->sid);

	/* Search window update frame, and combine */
retry:
	;
	struct fly_queue *__q;
	struct fly_hv2_send_frame *__s;
	fly_for_each_queue(__q, &stream->yetsend){
		__s = fly_queue_data(__q, struct fly_hv2_send_frame, qelem);
		if (__s->sid == frame->sid && \
				__s->type == FLY_HV2_FRAME_TYPE_WINDOW_UPDATE){
			uint32_t __usize = fly_hv2_31bit_nb(__s->payload);
#ifdef DEBUG
			printf("Found WINDOW UPDATE Frame ! size: %d : now: %d: %s\n", __usize, update_size, (update_size + __usize <= FLY_HV2_WINDOW_SIZE_MAX) ? "combine" : "new window frame");
#endif
			if (update_size + __usize <= FLY_HV2_WINDOW_SIZE_MAX){
				update_size += __usize;
				fly_hv2_send_frame_release(__s);
				goto retry;
			}
		}
	}
	/* copy error_code/last-stream-id/r to payload */
	uint8_t *ptr = frame->payload;
	uint32_t *uptr = &update_size;

	*ptr = r ? (1<<7) : 0;
#ifdef FLY_BIG_ENDIAN
	*ptr++ |= *(((uint8_t *) uptr)+0);
	*ptr++ = *(((uint8_t *) uptr)+1);
	*ptr++ = *(((uint8_t *) uptr)+2);
	*ptr++ = *(((uint8_t *) uptr)+3);
#else
	*ptr++ |= *(((uint8_t *) uptr)+3);
	*ptr++ = *(((uint8_t *) uptr)+2);
	*ptr++ = *(((uint8_t *) uptr)+1);
	*ptr++ = *(((uint8_t *) uptr)+0);
#endif

	__fly_hv2_add_yet_send_frame(frame);
	return 0;
}

int fly_send_rst_stream_frame(fly_hv2_stream_t *stream, uint32_t error_code)
{
	struct fly_hv2_send_frame *frame;

	frame = fly_hv2_send_frame_init(stream);
	if (fly_unlikely_null(frame))
		return -1;

	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_RST_STREAM;
	frame->payload_len = \
		(int) (FLY_HV2_FRAME_TYPE_GOAWAY_ERROR_CODE_LEN)/FLY_HV2_OCTET_LEN;
	frame->payload = fly_pballoc(frame->pool, frame->payload_len);
	if (fly_unlikely_null(frame->payload))
		return -1;

	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, 0, false, frame->sid);
	/* copy error_code/last-stream-id/r to payload */
	uint8_t *ptr = frame->payload;
	uint32_t *ecode = &error_code;

#ifdef FLY_BIG_ENDIAN
	*ptr++ = *(((uint8_t *) ecode)+0);
	*ptr++ = *(((uint8_t *) ecode)+1);
	*ptr++ = *(((uint8_t *) ecode)+2);
	*ptr++ = *(((uint8_t *) ecode)+3);
#else
	*ptr++ = *(((uint8_t *) ecode)+3);
	*ptr++ = *(((uint8_t *) ecode)+2);
	*ptr++ = *(((uint8_t *) ecode)+1);
	*ptr++ = *(((uint8_t *) ecode)+0);
#endif
	__fly_hv2_add_yet_send_frame(frame);

	stream->stream_state = FLY_HV2_STREAM_STATE_CLOSED;

	return 0;
}

int fly_send_goaway_frame(fly_hv2_state_t *state, bool r, uint32_t error_code);

/*
 * if expired or end of process, this function is called
 */
int fly_hv2_close_handle(fly_event_t *e, fly_hv2_state_t *state);
int fly_hv2_end_handle(fly_event_t *e)
{
	fly_connect_t *conn;

	//conn = (fly_connect_t *) e->expired_event_data;
	conn = (fly_connect_t *) fly_expired_event_data_get(e, __p);
	return fly_hv2_close_handle(e, conn->v2_state);
}

int fly_hv2_timeout_handle(fly_event_t *e)
{
#ifdef DEBUG
	printf("!!! HTTP2 TIMEOUT HANDLE: %s: %d !!!\n", __FILE__, __LINE__);
#endif
	fly_connect_t *conn;

	//conn = (fly_connect_t *) e->expired_event_data;
	conn = (fly_connect_t *) fly_expired_event_data_get(e, __p);
	return fly_hv2_close_handle(e, conn->v2_state);
}

int fly_hv2_close_handle(fly_event_t *e, fly_hv2_state_t *state)
{
	/* all frame/stream/request release */
	fly_connect_t *conn;

	if (state->stream_count > 0){
		struct fly_bllist *__n;
		fly_hv2_stream_t *__s;
		for (struct fly_bllist *__b=state->streams.next; __b!=&state->streams; __b=__n){
			__s = fly_bllist_data(__b, fly_hv2_stream_t, blelem);
			__n = __b->next;
			if (fly_hv2_close_stream(__s) == -1)
				return -1;
		}
	}

	conn = state->connect;
	/* state release */
	fly_hv2_state_release(state, e);
	/* connect release */
	if (fly_connect_release(conn) == -1)
		return -1;

	e->tflag = 0;
	e->flag = FLY_CLOSE_EV;
	return 0;
}

int fly_send_frame(fly_event_t *e, fly_hv2_stream_t *stream);
int fly_hv2_goaway_handle(fly_event_t *e, fly_hv2_state_t *state)
{
	if (state->goaway_lsid<state->max_handled_sid || state->stream_count == 0)
		goto close_connection;

	fly_hv2_stream_t *__s;
	struct fly_bllist *__b;

retry:
	if (state->stream_count == 0)
		goto close_connection;
	fly_for_each_bllist(__b, &state->streams){
		__s = fly_bllist_data(__b, fly_hv2_stream_t, blelem);
		if (!__s->peer_end_headers || (!__s->can_response && !__s->yetsend_count)){
			if (fly_hv2_close_stream(__s) == -1)
				return -1;
			goto retry;
		}


		if (__s->yetsend_count)
			return fly_send_frame(e, __s);

		/* can response */
		return fly_hv2_response_event_handler(e, __s);
	}
close_connection:
	/* close connection */
	if (fly_hv2_close_handle(e, state) == -1)
		return -1;
	e->flag = FLY_CLOSE_EV;
	return 0;
}

int fly_hv2_goaway(fly_hv2_state_t *state, bool r, uint32_t last_stream_id, uint32_t error_code)
{
	__fly_unused uint32_t max_handled_sid;
	max_handled_sid = state->max_handled_sid;

	state->goaway = true;
	state->goaway_lsid = last_stream_id;

	if (state->max_handled_sid >= last_stream_id){
		/* connection close now */
		if (fly_send_goaway_frame(state, r, error_code) == -1)
			return -1;
	}

	return 0;
}

fly_hv2_stream_t *fly_hv2_stream_search_from_sid(fly_hv2_state_t *state, fly_sid_t sid)
{
	fly_hv2_stream_t *__s;

	if (sid == FLY_HV2_STREAM_ROOT_ID)
		return FLY_HV2_ROOT_STREAM(state);

	for (struct fly_bllist *__b=state->streams.next; __b!=&state->streams; __b=__b->next){
		__s = fly_bllist_data(__b, fly_hv2_stream_t, blelem);
		if (__s->id == sid)
			return __s;
	}
	/* not found */
	return NULL;
}

int fly_hv2_request_event_blocking_handler(fly_event_t *e)
{
	fly_connect_t *conn;

	conn = (fly_connect_t *) fly_event_data_get(e, __p);
	/* receive from peer */
	switch(fly_receive_v2(conn->c_sockfd, conn)){
	case FLY_REQUEST_RECEIVE_ERROR:
		return -1;
	case FLY_REQUEST_RECEIVE_SUCCESS:
		break;
	case FLY_REQUEST_RECEIVE_END:
		goto disconnect;
	case FLY_REQUEST_RECEIVE_READ_BLOCKING:
		goto read_continuation;
	case FLY_REQUEST_RECEIVE_WRITE_BLOCKING:
		goto write_continuation;
	case FLY_REQUEST_RECEIVE_OVERFLOW:
		goto overflow;
	default:
		FLY_NOT_COME_HERE
	}

	return fly_hv2_request_event_handler(e);

write_continuation:
#ifdef DEBUG
	printf("HTTP2 WRITE BLOCKING\n");
#endif
	e->read_or_write |= FLY_WRITE;
	goto continuation;
read_continuation:
#ifdef DEBUG
	printf("HTTP2 READ BLOCKING\n");
#endif
	e->read_or_write |= FLY_READ;
	goto continuation;
continuation:
	//e->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	fly_event_state_set(e, __e, EFLY_REQUEST_STATE_CONT);
	e->flag = FLY_MODIFY;
	//e->event_data = conn;
	fly_event_data_set(e, __p, conn);
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->tflag = FLY_INHERIT;
	e->available = false;
	fly_event_socket(e);
	return fly_event_register(e);

disconnect:
#ifdef DEBUG
	printf("HTTP2 DISCONNECTION\n");
#endif
	return fly_hv2_request_event_handler(e);
overflow:
#ifdef DEBUG
	printf("HTTP2 OVERFLOW\n");
#endif
	return -1;
}

int fly_hv2_request_event_blocking(fly_event_t *e, fly_connect_t *conn)
{
	fly_event_data_set(e, __p, conn);

	if (FLY_CONNECT_ON_SSL(conn)){
		SSL *ssl = conn->ssl;

		if (SSL_has_pending(ssl)){
#ifdef DEBUG
			printf("Have SSL buffered data %dbytes\n", SSL_pending(ssl));
#endif
			return fly_hv2_request_event_blocking_handler(e);
		}
	}
	e->read_or_write |= FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_blocking_handler);
	return fly_event_register(e);
}

static inline bool fly_hv2_settings_frame_ack_from_flags(uint8_t flags)
{
	return flags&FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK ? true : false;
}

int fly_send_frame(fly_event_t *e, fly_hv2_stream_t *stream);
int fly_hv2_responses(fly_event_t *e, fly_hv2_state_t *state __fly_unused);
int fly_state_send_frame(fly_event_t *e, fly_hv2_state_t *state);

int fly_hv2_request_event_handler(fly_event_t *event)
{
	do{
		fly_connect_t *conn;
		fly_buf_p *bufp;
		fly_buffer_c *bufc, *plbufc;
		fly_hv2_state_t *state;

		conn = (fly_connect_t *) fly_event_data_get(event, __p);
		state = conn->v2_state;
		if (state->goaway)
			goto goaway;
		if (conn->peer_closed)
			goto closed;

		if ((event->available_row & FLY_READ) && FLY_HV2_FRAME_HEADER_LENGTH<=conn->buffer->use_len){
			goto frame_header_parse;
		}else if (event->available_row & FLY_READ){
			goto blocking;
		}else if ((event->available_row & FLY_WRITE) && \
				state->response_count > 0){
			if (state->send_count>0)
				return fly_state_send_frame(event, state);
			return fly_hv2_responses(event, state);
		}else if (event->available_row & FLY_WRITE){
			if (state->send_count>0)
				return fly_state_send_frame(event, state);
		}else
			goto blocking;

	frame_header_parse:
		bufp = fly_buffer_first_useptr(conn->buffer);
		bufc = fly_buffer_first_chain(conn->buffer);
#ifdef DEBUG
		size_t __buflen;

		__buflen = conn->buffer->use_len;
#endif

		/* if frrme length more than limits, must send FRAME_SIZE_ERROR */
		fly_hv2_stream_t *__stream;
		fly_hv2_frame_header_t *__fh = (fly_hv2_frame_header_t *) bufp;
		uint32_t length = fly_hv2_length_from_frame_header(__fh, bufc);
		uint32_t orig_length = length;
		uint8_t type = fly_hv2_type_from_frame_header(__fh, bufc);
		uint8_t flags = fly_hv2_flags_from_frame_header(__fh, bufc);
		__fly_unused bool r = fly_hv2_r_from_frame_header(__fh, bufc);
		uint32_t sid = fly_hv2_sid_from_frame_header(__fh, bufc);

#ifdef DEBUG
		switch(type){
		case FLY_HV2_FRAME_TYPE_DATA:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_DATA\n");
			if (flags & FLY_HV2_FRAME_TYPE_DATA_END_STREAM)
				printf("\t\tflag: FLY_HV2_DATA_END_STREAM\n");
			if (flags & FLY_HV2_FRAME_TYPE_DATA_PADDED)
				printf("\t\tflag: FLY_HV2_DATA_PADDED\n");
			break;
		case FLY_HV2_FRAME_TYPE_HEADERS:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_HEADERS\n");
			if (flags & FLY_HV2_FRAME_TYPE_HEADERS_END_STREAM)
				printf("\t\tflag: FLY_HV2_HEADERS_END_STREAM\n");
			if (flags & FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS)
				printf("\t\tflag: FLY_HV2_HEADERS_END_HEADERS\n");
			if (flags & FLY_HV2_FRAME_TYPE_HEADERS_PADDED)
				printf("\t\tflag: FLY_HV2_HEADERS_PADDED\n");
			if (flags & FLY_HV2_FRAME_TYPE_HEADERS_PRIORITY)
				printf("\t\tflag: FLY_HV2_HEADERS_PRIORITY\n");
			break;
		case FLY_HV2_FRAME_TYPE_PRIORITY:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_PRIORITY\n");
			break;
		case FLY_HV2_FRAME_TYPE_RST_STREAM:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_RST_STREAM\n");
			break;
		case FLY_HV2_FRAME_TYPE_SETTINGS:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_SETTINGS\n");
			if (flags & FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK)
				printf("\t\tflag: FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK\n");
			break;
		case FLY_HV2_FRAME_TYPE_PUSH_PROMISE:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_PUSH_PROMISE\n");
			if (flags & FLY_HV2_FRAME_TYPE_PUSH_PROMISE_END_HEADERS)
				printf("\t\tflag: FLY_HV2_FRAME_TYPE_PUSH_PROMISE_END_HEADERS\n");
			if (flags & FLY_HV2_FRAME_TYPE_PUSH_PROMISE_PADDED)
				printf("\t\tflag: FLY_HV2_FRAME_TYPE_PUSH_PROMISE_PADDED\n");
			break;
		case FLY_HV2_FRAME_TYPE_PING:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_PING\n");
			if (flags & FLY_HV2_FRAME_TYPE_PING_ACK)
				printf("\t\tflag: FLY_HV2_FRAME_TYPE_PING_ACK\n");
			break;
		case FLY_HV2_FRAME_TYPE_GOAWAY:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_GOAWAY\n");
			break;
		case FLY_HV2_FRAME_TYPE_WINDOW_UPDATE:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_WINDOW_UPDATE\n");
			break;
		case FLY_HV2_FRAME_TYPE_CONTINUATION:
			printf("\tRECV FRAME: FLY_HV2_FRAME_TYPE_CONTINUATION\n");
			if (flags & FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS)
				printf("\t\tflag: FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS\n");
			break;
		default:
			FLY_NOT_COME_HERE
		}
		printf("\t\tsid: %d\n", sid);
		printf("\t\tpayload length: %d\n", length);
#endif

		plbufc = bufc;
		uint8_t *pl = fly_hv2_frame_payload_from_frame_header(__fh, &plbufc);
		if (length > state->max_frame_size){
			fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
		} else if ((FLY_HV2_FRAME_HEADER_LENGTH+length) > conn->buffer->use_len){
#ifdef DEBUG
			printf("\t\tread blocking...%ld/%d\n", conn->buffer->use_len, length);
#endif
			goto blocking;
		}

		__stream = fly_hv2_stream_search_from_sid(state, sid);
		/* new stream */
		if (!__stream){
			__stream = fly_hv2_create_stream(state, sid, true);
			if (!__stream)
				continue;
		}

		if (sid!=FLY_HV2_STREAM_ROOT_ID)
			fly_hv2_create_frame(__stream, type, length, flags);

		switch(state->connection_state){
		case FLY_HV2_CONNECTION_STATE_INIT:
		case FLY_HV2_CONNECTION_STATE_END:
			/* error */
#ifdef DEBUG
			assert(0);
#endif
			return -1;
		case FLY_HV2_CONNECTION_STATE_CONNECTION_PREFACE:
			/* only SETTINGS Frame */
			if (type != FLY_HV2_FRAME_TYPE_SETTINGS){
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
				continue;
			}
			break;
		case FLY_HV2_CONNECTION_STATE_COMMUNICATION:
			break;
		default:
			FLY_NOT_COME_HERE
		}

		/* receive message from peer. extension */
		if (fly_timeout_restart(event) == -1){
			struct fly_err *__err;
			__err = fly_event_err_init(
				event, errno, FLY_ERR_CRIT,
				"time handler is broken. (%s: %d)",
				__FILE__, __LINE__
			);
			fly_event_error_add(event, __err);
			return -1;
		}

		switch(type){
		case FLY_HV2_FRAME_TYPE_DATA:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_LOCAL)){
				fly_hv2_send_stream_closed(__stream, FLY_HV2_STREAM_ERROR);
			}

			{
				__fly_unused uint8_t pad_length;
				int res;

				if (flags & FLY_HV2_FRAME_TYPE_DATA_PADDED){
					pad_length = fly_hv2_pad_length(&pl, &plbufc);
					length -= (int) (FLY_HV2_FRAME_TYPE_DATA_PAD_LENGTH_LEN/FLY_HV2_OCTET_LEN);
				}

				res = fly_hv2_parse_data(event, __stream, length, pl, plbufc);
				switch(res){
				case FLY_HV2_PARSE_DATA_SUCCESS:
					break;
				case FLY_HV2_PARSE_DATA_ERROR:
					goto emergency;
				default:
					FLY_NOT_COME_HERE
				}

				if (flags & FLY_HV2_FRAME_TYPE_DATA_END_STREAM)
					__stream->stream_state = FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE;

				/* end of post data */
				if ((__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE) && (__stream->peer_end_headers || __stream->can_response)){
					__stream->can_response = true;
#ifdef DEBUG_RESPONSE_ERROR
					goto emergency;
#endif
					if (fly_hv2_response_event_handler(event, __stream) == -1)
						goto emergency;
				}

			}
			break;
		case FLY_HV2_FRAME_TYPE_HEADERS:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_IDLE || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_RESERVED_LOCAL || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE)){
				fly_hv2_send_stream_closed(__stream, FLY_HV2_STREAM_ERROR);
			}
			switch(__stream->stream_state){
			case FLY_HV2_STREAM_STATE_IDLE:
				__stream->stream_state = FLY_HV2_STREAM_STATE_OPEN;
				break;
			case FLY_HV2_STREAM_STATE_RESERVED_REMOTE:
				__stream->stream_state = FLY_HV2_STREAM_STATE_HALF_CLOSED_LOCAL;
				break;
			default:
				break;
			}

			{
				__fly_unused uint32_t hlen=length;
				__fly_unused uint8_t pad_length;
				__fly_unused bool e;
				__fly_unused uint32_t stream_dependency;
				__fly_unused uint8_t weight;

				if (flags & FLY_HV2_FRAME_TYPE_HEADERS_PADDED){
					pad_length = fly_hv2_pad_length(&pl, &plbufc);
					hlen -= (int) (FLY_HV2_FRAME_TYPE_HEADERS_PAD_LENGTH_LEN/FLY_HV2_OCTET_LEN);
				}
				if (flags & FLY_HV2_FRAME_TYPE_HEADERS_PRIORITY){
					e = fly_hv2_flag(&pl);
					stream_dependency = fly_hv2_stream_dependency(&pl, &plbufc);
					weight = fly_hv2_weight(&pl, &plbufc);

					hlen -= (int) ((FLY_HV2_FRAME_TYPE_HEADERS_E_LEN+FLY_HV2_FRAME_TYPE_HEADERS_SID_LEN+FLY_HV2_FRAME_TYPE_HEADERS_WEIGHT_LEN)/(FLY_HV2_OCTET_LEN));
				}

				if (fly_hv2_parse_headers(__stream, hlen, pl, plbufc) == -1){
					/* RFC7540: 4.3
					 *	COMPRESSION_ERROR
					 */
					fly_hv2_send_compression_error(__stream, FLY_HV2_CONNECTION_ERROR);
				}

				if (flags & FLY_HV2_FRAME_TYPE_HEADERS_END_STREAM)
					__stream->stream_state = FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE;
				/* end of request. go request handle. */
				if (flags & FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS)
					__stream->peer_end_headers = true;
				/*
				 *	request handle.
				 *	condition:
				 *	after sending all headers from peer.
				 */
				if ((__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE) && (__stream->peer_end_headers || __stream->can_response)){
					__stream->can_response = true;
					if (fly_hv2_response_event_handler(event, __stream) == -1)
						goto emergency;
				}

			}

			break;
		case FLY_HV2_FRAME_TYPE_PRIORITY:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			/* can receive at any stream state */
			if (length != FLY_HV2_FRAME_TYPE_PRIORITY_LENGTH)
				fly_hv2_send_frame_size_error(__stream, FLY_HV2_STREAM_ERROR);

			{
				bool exclusive;
				uint32_t stream_dependency;
				uint8_t weight;

				exclusive = fly_hv2_flag(&pl);
				stream_dependency = fly_hv2_stream_dependency(&pl, &plbufc);
				weight = fly_hv2_weight(&pl, &plbufc);
				/* Change Stream priority parameters */
				__stream->weight = weight;
				__stream->dependency_id = stream_dependency;
				__stream->exclusive = exclusive;

				fly_hv2_priority_rebalance(__stream);
			}
			break;
		case FLY_HV2_FRAME_TYPE_RST_STREAM:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			if (__stream->stream_state == FLY_HV2_STREAM_STATE_IDLE)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			if (length != FLY_HV2_FRAME_TYPE_RST_STREAM_LENGTH)
				fly_hv2_send_frame_size_error(__stream, FLY_HV2_CONNECTION_ERROR);

			__stream->stream_state = FLY_HV2_STREAM_STATE_CLOSED;
			break;
		case FLY_HV2_FRAME_TYPE_SETTINGS:
			{
				bool ack;
				if (sid!=FLY_HV2_STREAM_ROOT_ID)
					fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
				if (length % FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH)
					fly_hv2_send_frame_size_error(__stream, FLY_HV2_CONNECTION_ERROR);

				ack = fly_hv2_settings_frame_ack_from_flags(flags);
				if (ack){
					fly_received_settings_frame_ack(__stream);
					break;
				}else{
					/* send setting frame of server */
					if (!state->first_send_settings)
						fly_send_settings_frame_of_server(__stream);
					/* can receive at any stream state */
					switch(fly_hv2_peer_settings(state, sid, pl, length, plbufc)){
					case FLY_HV2_FLOW_CONTROL_ERROR:
						fly_hv2_send_flow_control_error(__stream, FLY_HV2_CONNECTION_ERROR);
						continue;
					case FLY_HV2_PROTOCOL_ERROR:
						fly_hv2_send_protocol_error(__stream, FLY_HV2_CONNECTION_ERROR);
						continue;
					case FLY_HV2_SETTINGS_SUCCESS:
						break;
					default:
						FLY_NOT_COME_HERE
					}
					fly_settings_frame_ack(__stream);
				}


				/* connection state => FLY_HV2_CONNECTION_STATE_COMMUNICATION */
				state->connection_state = FLY_HV2_CONNECTION_STATE_COMMUNICATION;
			}


			break;
		case FLY_HV2_FRAME_TYPE_PUSH_PROMISE:
			if (sid==FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE)){
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			}
			if (!state->enable_push)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			/* stream id must be new one */
			if (fly_hv2_stream_search_from_sid(state, sid))
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			/* create reserved stream */
			if (fly_hv2_stream_create_reserved(state, sid, true) == -1)
				goto emergency;

			break;
		case FLY_HV2_FRAME_TYPE_PING:
			if (sid!=FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			if (length != FLY_HV2_FRAME_TYPE_PING_LENGTH)
				fly_hv2_send_frame_size_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			{
				uint64_t opaque_data;
				opaque_data = fly_opaque_data(&pl, &plbufc);

				if (fly_send_ping_frame(state, opaque_data) == -1)
					goto emergency;
			}
			break;
		case FLY_HV2_FRAME_TYPE_GOAWAY:
			if (sid!=FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			{
				__fly_unused bool r;
				__fly_unused uint32_t last_stream_id;
				__fly_unused uint32_t error_code;

				r = fly_hv2_flag(&pl);
				last_stream_id = fly_hv2_last_sid(&pl, &plbufc);
				error_code = fly_hv2_error_code(&pl, &plbufc);

				/* send GOAWAY frame to receiver and connection close */
				if (fly_hv2_goaway(state, r, last_stream_id, error_code) == -1)
					goto emergency;
			}

			break;
		case FLY_HV2_FRAME_TYPE_WINDOW_UPDATE:
			if (length != FLY_HV2_FRAME_TYPE_WINDOW_UPDATE_LENGTH)
				fly_hv2_send_frame_size_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			{
				__fly_unused bool r;
				uint32_t p_window_size;

				r = fly_hv2_flag(&pl);
				p_window_size = fly_hv2_window_size_increment(&pl, &plbufc);
				if (p_window_size == 0){
					if (sid == 0)
						fly_hv2_send_protocol_error(__stream, FLY_HV2_STREAM_ERROR);
					else
						fly_hv2_send_protocol_error(__stream, FLY_HV2_CONNECTION_ERROR);
				}

				if (sid != 0)
					__stream->p_window_size += (ssize_t) p_window_size;
				else
					state->p_window_size += (ssize_t) p_window_size;
			}
			break;
		case FLY_HV2_FRAME_TYPE_CONTINUATION:
			if (sid==FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			{
				struct fly_hv2_frame *__f;
				struct fly_queue *__lq;

				__lq = fly_queue_last(&__stream->frames);
				if (__lq == &__stream->frames)
					fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

				__f = fly_queue_data(__lq, struct fly_hv2_frame, felem);
				if (!((__f->type == FLY_HV2_FRAME_TYPE_HEADERS && !(__f->flags&FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS)) || (__f->type == FLY_HV2_FRAME_TYPE_PUSH_PROMISE && !(__f->flags&FLY_HV2_FRAME_TYPE_PUSH_PROMISE_END_HEADERS)) || (__f->type == FLY_HV2_FRAME_TYPE_CONTINUATION && !(__f->flags&FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS)))){
					fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
				}

				{
					fly_hv2_parse_headers(__stream, length, pl, plbufc);
					if (flags & FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS)
						__stream->peer_end_headers = true;

				}
			}

			break;
		default:
			/* unknown type. must ignore */
			break;
		}

		/*
		 * release SETTINGS Frame resource.
		 * release start point:		__fh.
		 * release length:			Frame Header(9octet) + Length of PayLoad.
		 */
		fly_buffer_t *__buf=bufc->buffer;
		fly_buffer_chain_release_from_length(bufc, FLY_HV2_FRAME_HEADER_LENGTH+orig_length);
		bufc = fly_buffer_first_chain(__buf);
		if (bufc->buffer->use_len == 0)
			fly_buffer_chain_refresh(bufc);

#ifdef DEBUG
		/*
		 * buffer lenngth must be \
		 * previous buffer length-FRAME_SIZE+FRAME_HEADER_SIZE
		 *
		 */
		assert(conn->buffer->use_len == __buflen-(FLY_HV2_FRAME_HEADER_LENGTH+orig_length));
		printf("HTTP2 BUFFER LENGTH: %ld\n", conn->buffer->use_len);
#endif
		/* Is there next frame header in buffer? */
		if (conn->buffer->use_len >= FLY_HV2_FRAME_HEADER_LENGTH)
			continue;

		/* blocking event */
		goto blocking;
blocking:
		return fly_hv2_request_event_blocking(event, conn);
goaway:
		return fly_hv2_goaway_handle(event, state);
closed:
		if (fly_hv2_close_handle(event, state) == -1)
			return -1;

		event->flag = FLY_CLOSE_EV;
		return 0;
emergency:
		;
		fly_hv2_emergency(event, state);

		struct fly_err *__err;
		__err = fly_event_err_init(
			event,
			errno,
			FLY_ERR_ERR,
			"HTTP2 received emergency error. (%s: %d)",
			__FILE__, __LINE__
		);
		fly_event_error_add(event, __err);
		return -1;

	} while (true);

	FLY_NOT_COME_HERE
}

void fly_fh_setting(fly_hv2_frame_header_t *__fh, uint32_t length, uint8_t type, uint8_t flags, bool r, uint32_t sid)
{
#ifdef FLY_BIG_ENDIAN
	*(((uint8_t *) (__fh))+0) = (uint8_t) (((uint8_t *) &length)[0]);
	*(((uint8_t *) (__fh))+1) = (uint8_t) (((uint8_t *) &length)[1]);
	*(((uint8_t *) (__fh))+2) = (uint8_t) (((uint8_t *) &length)[2]);
#else
	*(((uint8_t *) (__fh))+0) = (uint8_t) (((uint8_t *) &length)[2]);
	*(((uint8_t *) (__fh))+1) = (uint8_t) (((uint8_t *) &length)[1]);
	*(((uint8_t *) (__fh))+2) = (uint8_t) (((uint8_t *) &length)[0]);
#endif
	*(((uint8_t *) (__fh))+3) = type;
	*(((uint8_t *) (__fh))+4) = flags;
	if (r)
		*(((uint8_t *) (__fh))+5) = (1<<7);
	else
		*(((uint8_t *) (__fh))+5) = 0x0;
#ifdef FLY_BIG_ENDIAN
	*(((uint8_t *) (__fh))+5) |= (uint8_t) (((uint8_t *) &sid)[0]);
	*(((uint8_t *) (__fh))+6)  = (uint8_t) (((uint8_t *) &sid)[1]);
	*(((uint8_t *) (__fh))+7)  = (uint8_t) (((uint8_t *) &sid)[2]);
	*(((uint8_t *) (__fh))+8)  = (uint8_t) (((uint8_t *) &sid)[3]);
#else
	*(((uint8_t *) (__fh))+5) |= (uint8_t) (((uint8_t *) &sid)[3]);
	*(((uint8_t *) (__fh))+6)  = (uint8_t) (((uint8_t *) &sid)[2]);
	*(((uint8_t *) (__fh))+7)  = (uint8_t) (((uint8_t *) &sid)[1]);
	*(((uint8_t *) (__fh))+8)  = (uint8_t) (((uint8_t *) &sid)[0]);
#endif
}

void fly_settings_frame_payload_set(uint8_t *pl, uint16_t *ids, uint32_t *values, size_t count)
{
	size_t __n=0;

	while(__n<count){
		uint16_t id = ids[__n];
		uint32_t value = values[__n];

		/* identifier */
#ifdef FLY_BIG_ENDIAN
		*(((uint8_t *) (pl))+0)	= ((uint8_t *) &id)[0];
		*(((uint8_t *) (pl))+1)	= ((uint8_t *) &id)[1];
#else
		*(((uint8_t *) (pl))+0)	= ((uint8_t *) &id)[1];
		*(((uint8_t *) (pl))+1)	= ((uint8_t *) &id)[0];
#endif
		/* value */
#ifdef FLY_BIG_ENDIAN
		*(((uint8_t *) (pl))+2)	= ((uint8_t *) &value)[0];
		*(((uint8_t *) (pl))+3)	= ((uint8_t *) &value)[1];
		*(((uint8_t *) (pl))+4)	= ((uint8_t *) &value)[2];
		*(((uint8_t *) (pl))+5)	= ((uint8_t *) &value)[3];
#else
		*(((uint8_t *) (pl))+2)	= ((uint8_t *) &value)[3];
		*(((uint8_t *) (pl))+3)	= ((uint8_t *) &value)[2];
		*(((uint8_t *) (pl))+4)	= ((uint8_t *) &value)[1];
		*(((uint8_t *) (pl))+5)	= ((uint8_t *) &value)[0];
#endif

		__n++;
	}
}

void fly_settings_frame_ack(fly_hv2_stream_t *stream)
{
	fly_send_settings_frame(stream, NULL, NULL, 0, true);
}

void fly_hv2_set_index_bit(enum fly_hv2_index_type iu, uint8_t *pl, size_t *bit_pos)
{
	switch(iu){
	case INDEX_UPDATE:
		/*
		 *	  0   1   2 3 4 5 6 7
		 *	+-------------------+
		 *	| 0 | 1 |	index	|
		 */
		*pl &= ((1 << (8-1)) - 1);
		*pl |= (1 << (7-1));
		if (bit_pos)
			*bit_pos = 3;
		break;
	case INDEX_NOUPDATE:
		/*
		 *	  0   1   2	  3   4  5  6  7
		 *	+---------------------------+
		 *	| 0 | 0 | 0 | 0 | 	index	|
		 */
		*pl &= ((1 << 4) - 1);
		if (bit_pos)
			*bit_pos = 5;
		break;
	case NOINDEX:
		/*
		 *	  0   1   2	  3   4  5  6  7
		 *	+---------------------------+
		 *	| 0 | 0 | 0 | 1 | 	index	|
		 */
		*pl &= ((1 << 4) - 1);
		*pl |= (1 << 4);
		if (bit_pos)
			*bit_pos = 5;
		break;
	default:
		FLY_NOT_COME_HERE
	}

	return;
}

void fly_hv2_set_integer(uint32_t integer, uint8_t **pl, fly_buffer_c **__c, __fly_unused uint32_t *update, uint8_t prefix_bit);

void fly_hv2_set_index(uint32_t index, enum fly_hv2_index_type iu, uint8_t **pl, fly_buffer_c **__c, uint32_t *update)
{
	fly_hv2_set_index_bit(iu, *pl, NULL);
	switch(iu){
	case INDEX_UPDATE:
		fly_hv2_set_integer(index, pl, __c, update, FLY_HV2_LITERAL_UPDATE_PREFIX_BIT);
		break;
	case INDEX_NOUPDATE:
		fly_hv2_set_integer(index, pl, __c, update, FLY_HV2_LITERAL_NOUPDATE_PREFIX_BIT);
		break;
	case NOINDEX:
		fly_hv2_set_integer(index, pl, __c, update, FLY_HV2_LITERAL_NOINDEX_PREFIX_BIT);
		break;
	default:
		FLY_NOT_COME_HERE
	}
}


static inline void __fly_hv2_set_index_bit(uint8_t *pl)
{
	*pl |= (1<<7);
}
static inline void __fly_hv2_set_huffman_bit(uint8_t *pl, bool huffman)
{
	if (huffman)
		*pl |= (1<<7);
	else
		*pl &= ((1<<7)-1);
}

/*
 *	return update bytes.
 */
uint32_t __fly_payload_from_headers(fly_buffer_t *buf, fly_hdr_c *c)
{
#define FLY_HEADERS_INDEX(p)		p+1
	fly_buffer_c *chain = fly_buffer_last_chain(buf);
	uint8_t *ptr = fly_buffer_lunuse_ptr(buf);
	uint32_t total = 0;
#ifdef DEBUG
	printf("%s: %s",
			c->name,
			c->value);
	if (!c->name_index){
		printf("(%s)\n", "name in static/dynamic table");
	}else
		printf("no table\n");
#endif
	/* static index  */
	if (c->static_table || c->dynamic_table){
		__fly_hv2_set_index_bit(ptr);
		fly_hv2_set_integer(FLY_HEADERS_INDEX(c->index), &ptr, &chain, &total, FLY_HV2_INDEX_PREFIX_BIT);
	/* dynamic index */
	/* static/dynamic index name, no index value  */
	}else if (c->name_index){
#ifdef DEBUG
		printf("\tname_index");
#endif
		size_t value_len;
		__fly_unused char *value;
#ifdef DEBUG
		printf("(%d)\n", FLY_HEADERS_INDEX(c->index));
#endif
		/* set name index */
		fly_hv2_set_index(FLY_HEADERS_INDEX(c->index), c->index_update, &ptr, &chain, &total);
		/* set value length */
		if (c->huffman_value){
#ifdef DEBUG
		printf("\thuffman_value\n");
#endif
			__fly_hv2_set_huffman_bit(ptr, true);
			fly_hv2_set_integer(c->hvalue_len, &ptr, &chain, &total, FLY_HV2_VALUE_PREFIX_BIT);
			value_len = c->hvalue_len;
			value = c->hen_value;
		}else{
			__fly_hv2_set_huffman_bit(ptr, false);
			fly_hv2_set_integer(c->value_len, &ptr, &chain, &total, FLY_HV2_VALUE_PREFIX_BIT);
			value_len = c->value_len;
			value = c->value;
		}

		while(value_len--){
			*(char *) ptr = *value++;
			fly_update_buffer(buf, 1);
			ptr = (uint8_t *) fly_buffer_lunuse_ptr(buf);
			total++;
		}
	/* no index name and value  */
	}else{
#ifdef DEBUG
		printf("\tno index\n");
#endif
		size_t value_len, name_len;
		char *value, *name;
		/* set no index */
		fly_hv2_set_index(0, c->index_update, &ptr, &chain, &total);
		/* set name length */
		if (c->huffman_name){
#ifdef DEBUG
		printf("\thuffman_name\n");
#endif
			__fly_hv2_set_huffman_bit(fly_buffer_lunuse_ptr(buf), true);
			fly_hv2_set_integer(c->hname_len, &ptr, &chain, &total, FLY_HV2_NAME_PREFIX_BIT);
			name_len = c->hname_len;
			name = c->hen_name;
		}else{
			__fly_hv2_set_huffman_bit(fly_buffer_lunuse_ptr(buf), false);
			fly_hv2_set_integer(c->name_len, &ptr, &chain, &total, FLY_HV2_NAME_PREFIX_BIT);
			name_len = c->name_len;
			name = c->name;
		}
		/* set name */
		while(name_len--){
			*(char *) ptr = *name++;
			fly_update_buffer(buf, 1);
			ptr = (uint8_t *) fly_buffer_lunuse_ptr(buf);
			total++;
		}
		/* set value length */
		if (c->huffman_value){
#ifdef DEBUG
		printf("\thuffman_value\n");
#endif
			__fly_hv2_set_huffman_bit(fly_buffer_lunuse_ptr(buf), true);
			fly_hv2_set_integer(c->hvalue_len, &ptr, &chain, &total, FLY_HV2_VALUE_PREFIX_BIT);
			value_len = c->hvalue_len;
			value = c->hen_value;
		}else{
			__fly_hv2_set_huffman_bit(fly_buffer_lunuse_ptr(buf), false);
			fly_hv2_set_integer(c->value_len, &ptr, &chain, &total, FLY_HV2_VALUE_PREFIX_BIT);
			value_len = c->value_len;
			value = c->value;
		}
		/* set value */
		while(value_len--){
			*(char *) ptr = *value++;
			fly_update_buffer(buf, 1);
			ptr = (uint8_t *) fly_buffer_lunuse_ptr(buf);
			total++;
		}
	}
	return total;
}

struct __fly_frame_header_data{
	fly_buffer_t *buf;
	size_t total;
	fly_hdr_c *header;
	fly_pool_t *pool;
};

int __fly_send_frame_h(fly_event_t *e, fly_response_t *res);
void __fly_hv2_remove_yet_send_frame(struct fly_hv2_send_frame *frame);

int fly_send_data_frame(fly_event_t *e, fly_response_t *res);
#define FLY_SEND_DATA_FH_SUCCESS			(1)
#define FLY_SEND_DATA_FH_BLOCKING			(0)
#define FLY_SEND_DATA_FH_ERROR				(-1)

int fly_send_data_frame_handler(fly_event_t *e);
int __fly_send_data_fh(fly_event_t *e, fly_response_t *res, size_t data_len, uint32_t sid, int flag);
int __fly_send_data_fh_event_handler(fly_event_t *e)
{
	fly_response_t *res;
	int flag;

	res = (fly_response_t *) fly_event_data_get(e, __p);
	flag = res->datai;
	/*
	 *	unuse data_len, sid parameter.
	 */
	return __fly_send_data_fh(e, res, 0, 0, flag);
}

int __fly_send_data_fh(fly_event_t *e, fly_response_t *res, size_t data_len, uint32_t sid, int flag)
{
	fly_hv2_frame_header_t *fh;
	ssize_t numsend, total=0;

	if (res->blocking){
		total = res->byte_from_start;
		fh = (fly_hv2_frame_header_t *) res->send_ptr;
		goto send;
	}else{
		res->byte_from_start = 0;
		res->fase = FLY_RESPONSE_FRAME_HEADER;
		fh = fly_pballoc(res->pool, sizeof(fly_hv2_frame_header_t));
		if (fly_unlikely_null(fh))
			return FLY_SEND_DATA_FH_ERROR;
		fly_fh_setting(fh, data_len, FLY_HV2_FRAME_TYPE_DATA, flag, false, sid);
		res->send_ptr = fh;
	}

send:
	/* send only frame header */
	while(total< FLY_HV2_FRAME_HEADER_LENGTH){
		if (FLY_CONNECT_ON_SSL(res->request->connect)){
			SSL *ssl = res->request->connect->ssl;
			ERR_clear_error();
			numsend = SSL_write(ssl, ((uint8_t *) fh)+total, FLY_HV2_FRAME_HEADER_LENGTH-total);

			switch(SSL_get_error(ssl, numsend)){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				return FLY_SEND_DATA_FH_ERROR;
			case SSL_ERROR_WANT_READ:
				goto read_blocking;
			case SSL_ERROR_WANT_WRITE:
				goto write_blocking;
			case SSL_ERROR_SYSCALL:
				if (FLY_SEND_DISCONNECTION(-1))
					goto disconnect;
				return FLY_SEND_DATA_FH_ERROR;
			case SSL_ERROR_SSL:
				return FLY_SEND_DATA_FH_ERROR;
			default:
				/* unknown error */
				return FLY_SEND_DATA_FH_ERROR;
			}
		}else{
			int c_sockfd;
			c_sockfd = res->request->connect->c_sockfd;
			numsend = send(c_sockfd, fh, FLY_HV2_FRAME_HEADER_LENGTH, FLY_MSG_NOSIGNAL);
			if (FLY_BLOCKING(numsend))
				goto write_blocking;
			else if (FLY_SEND_DISCONNECTION(numsend))
				goto disconnect;
			else if (numsend == -1)
				return FLY_SEND_DATA_FH_ERROR;
		}
		res->byte_from_start += numsend;
		total += numsend;
	}
	goto success;

success:
	res->blocking = false;
	res->byte_from_start = 0;
	res->send_ptr = NULL;
	fly_pbfree(res->pool, fh);
	res->fase = FLY_RESPONSE_DATA_FRAME;

	fly_event_data_set(e, __p, res->request->connect);
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	return fly_event_register(e);
read_blocking:
	e->read_or_write |= FLY_READ;
	goto blocking;
write_blocking:
	e->read_or_write |= FLY_WRITE;
	goto blocking;
blocking:
	res->datai = flag;
	fly_event_state_set(e, __e, EFLY_REQUEST_STATE_RESPONSE);
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	fly_event_data_set(e, __p, res->request->connect);
	fly_event_socket(e);
	return fly_event_register(e);
disconnect:
	res->blocking = false;
	fly_disconnect_from_response(res);
	fly_pbfree(res->pool, fh);
	res->fase = FLY_RESPONSE_DATA_FRAME;
	fly_event_data_set(e, __p, res->request->connect);
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	return fly_event_register(e);
}

int fly_hv2_response_event(fly_event_t *e);
int fly_send_data_frame_handler(fly_event_t *e)
{
	fly_response_t *res;

	//res = (fly_response_t *) e->event_data;
	res = (fly_response_t *) fly_event_data_get(e, __p);

	return fly_send_data_frame(e, res);
}

static inline void fly_hv2_peer_window_size_update(fly_hv2_stream_t *stream, ssize_t send_size)
{
	stream->p_window_size -= send_size;
	stream->state->p_window_size -= send_size;
}

__fly_unused static inline void fly_hv2_window_size_update(fly_hv2_stream_t *stream, ssize_t send_size)
{
	stream->window_size -= send_size;
	stream->state->window_size -= send_size;
}

int fly_hv2_response_blocking_event(fly_event_t *e, fly_hv2_stream_t *stream);
int fly_send_data_frame(fly_event_t *e, fly_response_t *res)
{
	fly_hv2_stream_t *stream;
	fly_hv2_state_t *state;
	size_t max_can_send;
	size_t send_len;
	int flag=0;
	size_t total=0;

	stream = res->request->stream;
	state = stream->state;
	stream->end_send_data = false;

	switch (res->fase){
	case FLY_RESPONSE_DATA_FRAME:
		goto send;
	default:
		break;
	}

	/* frame size over */
	if (stream->p_window_size <= 0 || state->p_window_size <= 0)
		goto cant_send;

	if (res->response_len == 0)
		goto success;

	max_can_send = stream->p_window_size > state->p_window_size ? state->p_window_size : stream->p_window_size;
	send_len = res->response_len;
	if ((size_t) send_len < max_can_send)
		flag |= FLY_HV2_FRAME_TYPE_DATA_END_STREAM;

	res->send_len = send_len;
	return __fly_send_data_fh(e, res, send_len, stream->id, flag);

send:
	;
	total = res->byte_from_start ? res->byte_from_start : 0;
	ssize_t numsend;
	send_len = res->send_len;
	fly_buffer_c *chain;
	fly_buf_p *send_ptr;

	switch(res->type){
	case FLY_RESPONSE_TYPE_ENCODED:
		{
			fly_de_t *de;
			de = res->de;

			chain = fly_buffer_first_chain(de->encbuf);
			send_ptr = fly_buffer_first_ptr(de->encbuf);
			numsend = total;
			while(true){
				send_ptr = fly_update_chain(&chain, send_ptr, numsend);
				if (FLY_CONNECT_ON_SSL(res->request->connect)){
					ERR_clear_error();
					SSL *ssl = res->request->connect->ssl;
					numsend = SSL_write(ssl, send_ptr, FLY_LEN_UNTIL_CHAIN_LPTR(chain, send_ptr));
					switch(SSL_get_error(ssl, numsend)){
					case SSL_ERROR_NONE:
						break;
					case SSL_ERROR_ZERO_RETURN:
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_WANT_READ:
						goto read_blocking;
					case SSL_ERROR_WANT_WRITE:
						goto write_blocking;
					case SSL_ERROR_SYSCALL:
						if (FLY_SEND_DISCONNECTION(-1))
							goto disconnect;
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
				}else{
					int c_sockfd;
					c_sockfd = res->request->connect->c_sockfd;
					numsend = send(c_sockfd, send_ptr, FLY_LEN_UNTIL_CHAIN_LPTR(chain, send_ptr), FLY_MSG_NOSIGNAL);
					if (FLY_BLOCKING(numsend))
						goto write_blocking;
					else if (FLY_SEND_DISCONNECTION(numsend))
						goto disconnect;
					else if (FLY_INTR(numsend))
						continue;
					else if (numsend == -1){
						struct fly_err *__err;

						__err = fly_err_init(
							state->pool,
							errno,
							FLY_ERR_ERR,
							"send frame error. (%s: %d)",
							__FILE__, __LINE__
						);
						fly_error_error(__err);
					}
				}
				res->byte_from_start += numsend;
				total += numsend;
				fly_hv2_peer_window_size_update(stream, numsend);

				/* send all content */
				if (total >= de->contlen)
					break;
			}
		}
		break;
	case FLY_RESPONSE_TYPE_BODY:
		{
			fly_body_t *body = res->body;
			while(total < (size_t) body->body_len){
				if (FLY_CONNECT_ON_SSL(res->request->connect)){
					SSL *ssl=res->request->connect->ssl;
					ERR_clear_error();
					numsend = SSL_write(ssl, body->body+total, body->body_len-total);
					switch(SSL_get_error(ssl, numsend)){
					case SSL_ERROR_NONE:
						break;
					case SSL_ERROR_ZERO_RETURN:
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_WANT_READ:
						goto read_blocking;
					case SSL_ERROR_WANT_WRITE:
						goto write_blocking;
					case SSL_ERROR_SYSCALL:
						if (FLY_SEND_DISCONNECTION(-1))
							goto disconnect;
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
				}else{
					numsend = send(e->fd, body->body+total, body->body_len-total, FLY_MSG_NOSIGNAL);
					if (FLY_BLOCKING(numsend))
						goto write_blocking;
					else if (FLY_SEND_DISCONNECTION(numsend))
						goto disconnect;
					else if (FLY_INTR(numsend))
						continue;
					else if (numsend == -1){
						struct fly_err *__err;

						__err = fly_err_init(
							state->pool,
							errno,
							FLY_ERR_ERR,
							"send frame error. (%s: %d)",
							__FILE__, __LINE__
						);
						fly_error_error(__err);
					}
				}
				total += numsend;
				res->byte_from_start += numsend;

				fly_hv2_peer_window_size_update(stream, numsend);
			}
		}
		break;
	case FLY_RESPONSE_TYPE_PATH_FILE:
		{
			off_t *offset = &res->offset;
			struct fly_mount_parts_file *pf = res->pf;
			int c_sockfd;

			c_sockfd = res->request->connect->c_sockfd;
			while(total < res->count){
				if (FLY_CONNECT_ON_SSL(res->request->connect)){
					SSL *ssl=res->request->connect->ssl;
					ERR_clear_error();
					char send_buf[FLY_SEND_BUF_LENGTH];
					ssize_t numread;

					if (lseek(pf->fd, *offset+(off_t) total, SEEK_SET) == -1)
						return FLY_SEND_DATA_FH_ERROR;
					numread = read(pf->fd, send_buf, res->count-total<FLY_SEND_BUF_LENGTH ? FLY_SEND_BUF_LENGTH : res->count-total);
					if (numread == -1)
						return FLY_RESPONSE_ERROR;
					numsend = SSL_write(ssl, send_buf, numread);
					switch(SSL_get_error(ssl, numsend)){
					case SSL_ERROR_NONE:
						break;
					case SSL_ERROR_ZERO_RETURN:
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_WANT_READ:
						goto read_blocking;
					case SSL_ERROR_WANT_WRITE:
						goto write_blocking;
					case SSL_ERROR_SYSCALL:
						if (FLY_SEND_DISCONNECTION(-1))
							goto disconnect;
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
				}else{
					numsend = fly_sendfile(c_sockfd, pf->fd, offset, res->count-total);
					if (FLY_BLOCKING(numsend)){
						goto write_blocking;
					}else if (numsend == -1){
						struct fly_err *__err;

						__err = fly_err_init(
							state->pool,
							errno,
							FLY_ERR_ERR,
							"send frame error. (%s: %d)",
							__FILE__, __LINE__
						);
						fly_error_error(__err);
					}
				}

				total += numsend;
				res->byte_from_start += numsend;

				fly_hv2_peer_window_size_update(stream, numsend);
			}
		}
		break;
	case FLY_RESPONSE_TYPE_DEFAULT:
		{
			fly_rcbs_t *__r=res->rcbs;
			off_t *offset = &res->offset;

			if (res->response_len == 0)
				goto success;

			while(total < res->response_len){
				if (FLY_CONNECT_ON_SSL(res->request->connect)){
					SSL *ssl=res->request->connect->ssl;
					ERR_clear_error();
					char send_buf[FLY_SEND_BUF_LENGTH];
					ssize_t numread;

					if (lseek(__r->fd, *offset+(off_t) total, SEEK_SET) == -1)
						return FLY_RESPONSE_ERROR;
					numread = read(__r->fd, send_buf, res->count-total<FLY_SEND_BUF_LENGTH ? FLY_SEND_BUF_LENGTH : res->count-total);
					if (numread == -1)
						return FLY_RESPONSE_ERROR;
					numsend = SSL_write(ssl, send_buf, numread);
					switch(SSL_get_error(ssl, numsend)){
					case SSL_ERROR_NONE:
						break;
					case SSL_ERROR_ZERO_RETURN:
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_WANT_READ:
						goto read_blocking;
					case SSL_ERROR_WANT_WRITE:
						goto write_blocking;
					case SSL_ERROR_SYSCALL:
						if (FLY_SEND_DISCONNECTION(-1))
							goto disconnect;
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
					*offset += numsend;
				}else{
					numsend = fly_sendfile(e->fd, __r->fd, offset, res->count-total);
					if (FLY_BLOCKING(numsend)){
						goto write_blocking;
					}else if (numsend == -1){
						struct fly_err *__err;

						__err = fly_err_init(
							state->pool,
							errno,
							FLY_ERR_ERR,
							"send frame error. (%s: %d)",
							__FILE__, __LINE__
						);
						fly_error_error(__err);
					}
				}

				fly_hv2_peer_window_size_update(stream, numsend);
				total += numsend;
			}
		}
		break;
	default:
		FLY_NOT_COME_HERE
	}
	goto success;

success:
	/* if now half closed(remote) state,
	 * close stream.
	 */
	if (stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE \
			&& total >= res->response_len){
		stream->stream_state = FLY_HV2_STREAM_STATE_CLOSED;
		stream->end_send_data = true;
		res->fase = FLY_RESPONSE_LOG;
		res->end_response = true;
		return fly_hv2_response_blocking_event(e, stream);
	}
	res->fase = FLY_RESPONSE_DATA_FRAME;
	stream->end_send_headers = true;

	return fly_hv2_response_blocking_event(e, stream);

cant_send:
	return fly_hv2_response_blocking_event(e, stream);
read_blocking:
	return fly_hv2_response_blocking_event(e, stream);
write_blocking:
	return fly_hv2_response_blocking_event(e, stream);
disconnect:
	return fly_hv2_close_handle(e, stream->state);
}

int fly_state_send_frame(fly_event_t *e, fly_hv2_state_t *state)
{
	struct fly_hv2_send_frame *__s;
retry:
	;
	struct fly_queue *__q;
	fly_for_each_queue(__q, &state->send){
		__s = fly_queue_data(__q, struct fly_hv2_send_frame, sqelem);
#ifdef DEBUG
		switch(__s->type){
		case FLY_HV2_FRAME_TYPE_DATA:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_DATA\n");
			break;
		case FLY_HV2_FRAME_TYPE_HEADERS:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_HEADERS\n");
			break;
		case FLY_HV2_FRAME_TYPE_PRIORITY:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_PRIORITY\n");
			break;
		case FLY_HV2_FRAME_TYPE_RST_STREAM:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_RST_STREAM\n");
			break;
		case FLY_HV2_FRAME_TYPE_SETTINGS:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_SETTINGS\n");
			break;
		case FLY_HV2_FRAME_TYPE_PUSH_PROMISE:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_PUSH_PROMISE\n");
			break;
		case FLY_HV2_FRAME_TYPE_PING:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_PING\n");
			break;
		case FLY_HV2_FRAME_TYPE_GOAWAY:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_GOAWAY\n");
			break;
		case FLY_HV2_FRAME_TYPE_WINDOW_UPDATE:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_WINDOW_UPDATE\n");
			break;
		case FLY_HV2_FRAME_TYPE_CONTINUATION:
			printf("\tSEND FRAME: FLY_HV2_FRAME_TYPE_CONTINUATION\n");
			break;
		default:
			FLY_NOT_COME_HERE
		}
		printf("\t\tsid: %d\n", __s->sid);
		printf("\t\tpayload length: %ld\n", __s->payload_len);
		printf("\t\t%s\n", __s->need_ack ? "need ack": "no need ack");
#endif
		switch(__fly_send_frame(__s)){
		case __FLY_SEND_FRAME_READING_BLOCKING:
			goto read_blocking;
		case __FLY_SEND_FRAME_WRITING_BLOCKING:
			goto write_blocking;
		case __FLY_SEND_FRAME_DISCONNECT:
			goto disconnect;
		case __FLY_SEND_FRAME_ERROR:
			return FLY_SEND_FRAME_ERROR;
		case __FLY_SEND_FRAME_SUCCESS:
			break;
		default:
			FLY_NOT_COME_HERE
		}

		if (__s->type == FLY_HV2_FRAME_TYPE_HEADERS && \
				(__s->frame_header[4]&FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS))
			__s->stream->end_send_headers = true;
		/* end of sending */
		__fly_hv2_remove_yet_send_frame(__s);
		/* release resources */
		if (!__s->need_ack){
			fly_pbfree(__s->pool, __s->payload);
			fly_pbfree(__s->pool, __s);
		}
		if (state->send_count == 0)
			break;
		else
			goto retry;
	}
	goto success;

success:
	if (state->response_count == 0)
		e->read_or_write &= ~FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	fly_event_data_set(e, __p, state->connect);
	return fly_event_register(e);

read_blocking:
	e->read_or_write |= FLY_READ;
	goto blocking;
write_blocking:
	e->read_or_write |= FLY_WRITE;
	goto blocking;
blocking:
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	fly_event_data_set(e, __p, state->connect);
	return fly_event_register(e);
disconnect:
	return fly_hv2_close_handle(e, state);
}

int fly_send_frame(fly_event_t *e, fly_hv2_stream_t *stream)
{
	struct fly_hv2_send_frame *__s;
	for (struct fly_queue *__q=stream->yetsend.next; __q!=&stream->yetsend; __q=__q->next){

		__s = fly_queue_data(__q, struct fly_hv2_send_frame, qelem);
		switch(__fly_send_frame(__s)){
		case __FLY_SEND_FRAME_READING_BLOCKING:
			goto read_blocking;
		case __FLY_SEND_FRAME_WRITING_BLOCKING:
			goto write_blocking;
		case __FLY_SEND_FRAME_ERROR:
			return FLY_SEND_FRAME_ERROR;
		case __FLY_SEND_FRAME_SUCCESS:
			break;
		case __FLY_SEND_FRAME_DISCONNECT:
			goto disconnect;
		default:
			return FLY_SEND_FRAME_ERROR;
		}
		/* end of sending */
		__fly_hv2_remove_yet_send_frame(__s);
		/* release resources */
		fly_pbfree(__s->pool, __s->payload);
		fly_pbfree(__s->pool, __s);
	}

	goto success;

success:
	e->read_or_write = FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	//e->event_data = (void *) stream->state->connect;
	fly_event_data_set(e, __p, stream->state->connect);
	fly_event_socket(e);
	return fly_event_register(e);

read_blocking:
	e->read_or_write |= FLY_READ;
	goto blocking;
write_blocking:
	e->read_or_write |= FLY_WRITE;
	goto blocking;
blocking:
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	//e->event_data = (void *) stream->state->connect;
	fly_event_data_set(e, __p, stream->state->connect);
	fly_event_socket(e);
	return fly_event_register(e);
disconnect:
	return fly_hv2_close_handle(e, stream->state);
}

int __fly_send_settings_frame(fly_event_t *e, fly_hv2_state_t *state)
{
	fly_hv2_stream_t *stream;
	struct fly_hv2_send_frame *__s;

	stream = FLY_HV2_ROOT_STREAM(state);
	if (stream->yetsend_count == 0)
		goto success;

	for (struct fly_queue *__q=stream->yetsend.next; __q!=&stream->yetsend; __q=__q->next){
		__s = fly_queue_data(__q, struct fly_hv2_send_frame, qelem);
		if (__s->type != FLY_HV2_FRAME_TYPE_SETTINGS)
			continue;

		switch(__fly_send_frame(__s)){
		case __FLY_SEND_FRAME_READING_BLOCKING:
			goto read_blocking;
		case __FLY_SEND_FRAME_WRITING_BLOCKING:
			goto write_blocking;
		case __FLY_SEND_FRAME_ERROR:
			return FLY_SEND_FRAME_ERROR;
		case __FLY_SEND_FRAME_SUCCESS:
			break;
		case __FLY_SEND_FRAME_DISCONNECT:
			goto disconnect;
		default:
			return FLY_SEND_FRAME_ERROR;
		}
		/* end of sending */
		__fly_hv2_remove_yet_send_frame(__s);
		/* release resources */
		fly_pbfree(__s->pool, __s->payload);
		fly_pbfree(__s->pool, __s);
	}

success:
	state->first_send_settings = true;
	return fly_hv2_settings_blocking_event(e, stream);
read_blocking:
	return fly_hv2_settings_blocking_event(e, stream);
write_blocking:
	return fly_hv2_settings_blocking_event(e, stream);
disconnect:
	return fly_hv2_close_handle(e, stream->state);

}

int __fly_send_frame_h(fly_event_t *e, fly_response_t *res)
{
	fly_hv2_stream_t *stream;
	struct fly_hv2_send_frame *__s;

	stream = res->request->stream;
	if (stream->yetsend_count == 0)
		goto success;

	stream->end_send_headers = false;
	for (struct fly_queue *__q=stream->yetsend.next; __q!=&stream->yetsend; __q=__q->next){
		__s = fly_queue_data(__q, struct fly_hv2_send_frame, qelem);
		if (__s->type != FLY_HV2_FRAME_TYPE_HEADERS)
			continue;

		switch(__fly_send_frame(__s)){
		case __FLY_SEND_FRAME_READING_BLOCKING:
			goto read_blocking;
		case __FLY_SEND_FRAME_WRITING_BLOCKING:
			goto write_blocking;
		case __FLY_SEND_FRAME_ERROR:
			return FLY_SEND_FRAME_ERROR;
		case __FLY_SEND_FRAME_SUCCESS:
			break;
		default:
			FLY_NOT_COME_HERE
		}
		/* end of sending */
		/* release resources */
		fly_pbfree(__s->pool, __s->payload);
		fly_pbfree(__s->pool, __s);
		__fly_hv2_remove_yet_send_frame(__s);
	}

success:
	stream->end_send_headers = true;
	return fly_hv2_response_blocking_event(e, stream);

read_blocking:
	return fly_hv2_response_blocking_event(e, stream);

write_blocking:
	return fly_hv2_response_blocking_event(e, stream);
}

struct fly_hv2_send_frame *__fly_send_headers_frame(fly_hv2_stream_t *stream, fly_pool_t *pool, fly_buffer_t *buf, size_t total, bool over, int flag)
{
	struct fly_hv2_send_frame *frame;

	frame = fly_hv2_send_frame_init(stream);
	if (fly_unlikely_null(frame))
		return NULL;

	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	if (over){
		frame->type = FLY_HV2_FRAME_TYPE_CONTINUATION;
	}else{
		frame->type = FLY_HV2_FRAME_TYPE_HEADERS;
	}
	frame->payload_len = total;
	frame->payload = fly_pballoc(pool, sizeof(uint8_t)*frame->payload_len);
	fly_buffer_memcpy((char *) frame->payload, fly_buffer_first_useptr(buf), fly_buffer_first_chain(buf), total);
	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, flag, false, stream->id);

	return frame;
}

void __fly_hv2_remove_yet_send_frame(struct fly_hv2_send_frame *frame)
{
	fly_hv2_stream_t *stream;
	fly_hv2_state_t *state;

	/* for stream */
	stream = frame->stream;
	fly_queue_remove(&frame->qelem);
	stream->yetsend_count--;

	/* for state */
	state = stream->state;
	fly_queue_remove(&frame->sqelem);
	state->send_count--;
}

void __fly_hv2_add_yet_send_frame(struct fly_hv2_send_frame *frame)
{
	fly_hv2_stream_t *stream;
	fly_hv2_state_t *state;

	stream = frame->stream;
	state = stream->state;

	/* for stream */
	fly_queue_push(&stream->yetsend, &frame->qelem);
	stream->yetsend_count++;

	/* for state */
	fly_queue_push(&state->send, &frame->sqelem);
	state->send_count++;
}

void fly_send_headers_frame(fly_hv2_stream_t *stream, fly_response_t *res)
{
#define FLY_SEND_HEADERS_FRAME_BUFFER_INIT_LEN		1
#define FLY_SEND_HEADERS_FRAME_BUFFER_CHAIN_MAX		100
#define FLY_SEND_HEADERS_FRAME_BUFFER_PER_LEN		10
	struct fly_hv2_send_frame *__f;
	__fly_unused size_t max_payload;
	size_t total;
	bool over=false;
	int flag = 0;
	fly_buffer_t *buf;
	fly_hdr_c *__h;

	max_payload = stream->state->max_frame_size;
	buf = fly_buffer_init(res->pool, FLY_SEND_HEADERS_FRAME_BUFFER_INIT_LEN, FLY_SEND_HEADERS_FRAME_BUFFER_CHAIN_MAX, FLY_SEND_HEADERS_FRAME_BUFFER_PER_LEN);

	total = 0;
	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &res->header->chain){
		__h = fly_bllist_data(__b, fly_hdr_c, blelem);
		size_t len;
		len = __fly_payload_from_headers(buf, __h);
		/* over limit of payload length */
		if (total+len >= max_payload){
			__f = __fly_send_headers_frame(stream, res->pool, buf, total, over, flag);
			__fly_hv2_add_yet_send_frame(__f);
			fly_buffer_chain_release_from_length(fly_buffer_first_chain(buf), total);
			over = true;
			total = 0;
		}

		total += len;
	}

	flag = FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS;
	if (res->type == FLY_RESPONSE_TYPE_NOCONTENT)
		flag |= FLY_HV2_FRAME_TYPE_HEADERS_END_STREAM;

	res->fase = FLY_RESPONSE_HEADER;
	__f = __fly_send_headers_frame(stream, res->pool, buf, total, over, flag);
	__fly_hv2_add_yet_send_frame(__f);
	fly_buffer_release(buf);
}

void fly_send_settings_frame(fly_hv2_stream_t *stream, uint16_t *id, uint32_t *value, size_t count, bool ack)
{
	struct fly_hv2_send_frame *frame;
	uint8_t flag=0;

#ifdef DEBUG
	/*
	 * RFC7540 :6.5
	 * StreamID of Setting frame must be 0x0
	 */
	assert(stream == FLY_HV2_ROOT_STREAM(stream->state));
#endif
	frame = fly_hv2_send_frame_init(stream);

	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->payload_len = !ack ? count*FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH : 0;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_SETTINGS;
	fly_queue_init(&frame->qelem);
	frame->payload = (!ack && count) ? fly_pballoc(stream->request->pool, frame->payload_len) : NULL;
	if (ack)
		frame->need_ack = false;
	else
		frame->need_ack = true;
	if ((!ack && count) && fly_unlikely_null(frame->payload))
		return;

	if (ack)
		flag |= FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK;

	fly_fh_setting(&frame->frame_header, frame->payload_len, FLY_HV2_FRAME_TYPE_SETTINGS, flag, false, FLY_HV2_STREAM_ROOT_ID);

	/* SETTING FRAME payload setting */
	if (!ack && count)
		fly_settings_frame_payload_set(frame->payload, id, value, count);

	if (!ack && frame->need_ack)
		__fly_hv2_add_yet_ack_frame(frame);
	__fly_hv2_add_yet_send_frame(frame);
}

__fly_static int __fly_send_frame(struct fly_hv2_send_frame *frame)
{
	size_t total = 0;
	ssize_t numsend;
	int c_sockfd;

	while(!(frame->send_fase == FLY_HV2_SEND_FRAME_FASE_PAYLOAD && total>=frame->payload_len)){
		if (FLY_CONNECT_ON_SSL(frame->stream->request->connect)){
			SSL *ssl = frame->stream->request->connect->ssl;
			ERR_clear_error();
			if (frame->send_fase == FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER)
				numsend = SSL_write(ssl, ((uint8_t *) &frame->frame_header)+total, FLY_HV2_FRAME_HEADER_LENGTH-total);
			else
				numsend = SSL_write(ssl, frame->payload+total, frame->payload_len-total);

			switch(SSL_get_error(ssl, numsend)){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				return __FLY_SEND_FRAME_ERROR;
			case SSL_ERROR_WANT_READ:
				goto read_blocking;
			case SSL_ERROR_WANT_WRITE:
				goto write_blocking;
			case SSL_ERROR_SYSCALL:
				if (FLY_SEND_DISCONNECTION(-1))
					goto disconnect;
				return __FLY_SEND_FRAME_ERROR;
			case SSL_ERROR_SSL:
				return __FLY_SEND_FRAME_ERROR;
			default:
				/* unknown error */
				return __FLY_SEND_FRAME_ERROR;
			}
		}else{
			c_sockfd = frame->stream->request->connect->c_sockfd;
			if (frame->send_fase == FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER)
				numsend = send(c_sockfd, ((uint8_t *) &frame->frame_header)+total, FLY_HV2_FRAME_HEADER_LENGTH-total, 0);
			else
				numsend = send(c_sockfd, frame->payload+total, frame->payload_len-total, FLY_MSG_NOSIGNAL);
			if (FLY_BLOCKING(numsend))
				goto write_blocking;
			else if (FLY_SEND_DISCONNECTION(numsend))
				goto disconnect;
			else if (FLY_INTR(numsend))
				continue;
			else if (numsend == -1){
				struct fly_err *__err;

				__err = fly_err_init(
					frame->pool,
					errno,
					FLY_ERR_ERR,
					"send frame error. (%s: %d)",
					__FILE__, __LINE__
				);
				fly_error_error(__err);
			}
		}

		if (frame->send_fase == FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER){
			total += numsend;
			frame->send_len = 0;
			if (total >= FLY_HV2_FRAME_HEADER_LENGTH){
				total = 0;
				frame->send_fase = FLY_HV2_SEND_FRAME_FASE_PAYLOAD;
			}
		}else{
			total += numsend;
			frame->send_len = 0;
			if (total >= frame->payload_len)
				break;
		}
	}

	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_END;
	return __FLY_SEND_FRAME_SUCCESS;
read_blocking:
	return __FLY_SEND_FRAME_READING_BLOCKING;
write_blocking:
	return __FLY_SEND_FRAME_WRITING_BLOCKING;
disconnect:
	return __FLY_SEND_FRAME_DISCONNECT;
}

#define FLY_HV2_SF_SETTINGS_HEADER_TABLE_SIZE_ENV		"FLY_SETTINGS_FRAME_HEADER_TABLE_SIZE"
#define FLY_HV2_SF_SETTINGS_ENABLE_PUSH_ENV				"FLY_SETTINGS_FRAME_ENABLE_PUSH"
#define FLY_HV2_SF_SETTINGS_MAX_CONCURRENT_STREAMS_ENV	"FLY_SETTINGS_MAX_CONCURRENT_STREAMS"
#define FLY_HV2_SF_SETTINGS_INITIAL_WINDOW_SIZE_ENV		"FLY_SETTINGS_INITIAL_WINDOW_SIZE"
#define FLY_HV2_SF_SETTINGS_MAX_FRAME_SIZE_ENV			"FLY_SETTINGS_MAX_FRAME_SIZE"
#define FLY_HV2_SF_SETTINGS_MAX_HEADER_LIST_SIZE_ENV	"FLY_SETTINGS_MAX_HEADER_LIST_SIZE"
int fly_send_settings_frame_of_server(fly_hv2_stream_t *stream)
{
	struct env{
		const char *env;
		uint16_t id;
	};
#define __FLY_HV2_SETTINGS_EID(x)		\
	{ FLY_HV2_SF_ ## x ## _ENV, FLY_HV2_SETTINGS_FRAME_ ## x }
	struct env	envs[] = {
		__FLY_HV2_SETTINGS_EID(SETTINGS_HEADER_TABLE_SIZE),
		__FLY_HV2_SETTINGS_EID(SETTINGS_ENABLE_PUSH),
		__FLY_HV2_SETTINGS_EID(SETTINGS_MAX_CONCURRENT_STREAMS),
		__FLY_HV2_SETTINGS_EID(SETTINGS_INITIAL_WINDOW_SIZE),
		__FLY_HV2_SETTINGS_EID(SETTINGS_MAX_FRAME_SIZE),
		__FLY_HV2_SETTINGS_EID(SETTINGS_MAX_HEADER_LIST_SIZE),
		{ NULL, 0x0 }
	};
	struct env *__ev = envs;
	char *env_value;
	int count=0;
	uint32_t value;
	uint16_t ids[sizeof(envs)/sizeof(envs[0])];
	uint32_t values[sizeof(envs)/sizeof(envs[0])];

	while(__ev->env){
		env_value = getenv((const char *) __ev->env);
		if (env_value){
			value = (uint32_t) atol(env_value);
			ids[count] = __ev->id;
			values[count] = value;
			count++;
		}

		__ev++;
	}
	fly_send_settings_frame(stream, ids, values, count, false);
	return 0;
#undef __FLY_HV2_SETTINGS_EID
}

void fly_hv2_release_yet_ack_frame(struct fly_hv2_send_frame *frame)
{
	fly_bllist_remove(&frame->aqelem);
	frame->stream->yetack_count--;

	if (frame->payload_len > 0)
		fly_pbfree(frame->pool, frame->payload);
	fly_pbfree(frame->pool, frame);
}

void __fly_hv2_add_yet_ack_frame(struct fly_hv2_send_frame *frame)
{
	fly_hv2_stream_t *stream;

	stream = frame->stream;

	fly_bllist_add_tail(&stream->yetack, &frame->aqelem);
	stream->yetack_count++;
}

void fly_received_settings_frame_ack(fly_hv2_stream_t *stream)
{

	if (stream->yetack_count == 0)
		return;
retry:
	;
	struct fly_bllist *__b;

	fly_for_each_bllist(__b, &stream->yetack){
		struct fly_hv2_send_frame *__yack;
		__yack = fly_bllist_data(__b, struct fly_hv2_send_frame, aqelem);
		if (__yack->type == FLY_HV2_FRAME_TYPE_SETTINGS){
			size_t len=0;

			while(len<__yack->payload_len){
				uint16_t id = fly_hv2_settings_id_nb(&__yack->payload);
				uint32_t value = fly_hv2_settings_value_nb(&__yack->payload);
				len+=FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH;
				switch(id){
				case FLY_HV2_SETTINGS_FRAME_SETTINGS_HEADER_TABLE_SIZE:
					stream->state->p_header_table_size = value;
					break;
				case FLY_HV2_SETTINGS_FRAME_SETTINGS_ENABLE_PUSH:
					stream->state->p_enable_push = value;
					break;
				case FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_CONCURRENT_STREAMS:
					stream->state->p_max_concurrent_streams = value;
					break;
				case FLY_HV2_SETTINGS_FRAME_SETTINGS_INITIAL_WINDOW_SIZE:
					stream->state->p_initial_window_size = value;
					break;
				case FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_FRAME_SIZE:
					if (value > FLY_HV2_MAX_FRAME_SIZE_MAX)
						stream->state->p_max_frame_size = FLY_HV2_MAX_FRAME_SIZE_MAX;
					else
						stream->state->p_max_frame_size = value;
					break;
				case FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_HEADER_LIST_SIZE:
					stream->state->p_max_header_list_size = value;
					break;
				default:
					/* unknown setting */
					break;
				}
			}

			fly_hv2_release_yet_ack_frame(__yack);
			goto retry;
		}
	}
	return;
}

/*
 *	Send Error:
 *	@NO_ERROR
 *	@PROTOCOL_ERROR
 *	@INTERNAL_ERROR
 *	@FLOW_CONTROL_ERROR
 *	@SETTINGS_TIMEOUT
 *	@STREAM_CLOSED
 *	@FRAME_SIZE_ERROR
 *	@REFUSED_STREAM
 *	@CANCEL
 *	@COMPRESSION_ERROR
 *	@CONNECT_ERROR
 *	@ENHANCE_YOUR_CALM
 *	@INADEQUATE_SECURITY
 *	@HTTP_1_1_REQUIRED
 *
 *	Params:
 *	code => error code(32bit)
 *	etype => FLY_HV2_CONNECTION_ERROR or FLY_HV2_STREAM_ERROR
 *
 */
void __fly_hv2_send_error(fly_hv2_stream_t *stream, uint32_t code, enum fly_hv2_error_type etype)
{
	switch(etype){
	case FLY_HV2_CONNECTION_ERROR:
		fly_send_goaway_frame(stream->state, false, code);
		break;
	case FLY_HV2_STREAM_ERROR:
		fly_send_rst_stream_frame(stream, code);
		break;
	default:
		FLY_NOT_COME_HERE
	}
	return;
}

/*
 * when an emergency occurs, send goaway frame.
 */
void fly_hv2_emergency(fly_event_t *event, fly_hv2_state_t *state)
{
	struct fly_hv2_send_frame *frame;

	frame = state->emergency_ptr;
	frame->stream = FLY_HV2_ROOT_STREAM(state);
	frame->pool = state->pool;
	frame->sid = FLY_HV2_STREAM_ROOT_ID;
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_GOAWAY;
	frame->payload_len = \
		(int) (FLY_HV2_FRAME_TYPE_GOAWAY_R_LEN+FLY_HV2_FRAME_TYPE_GOAWAY_LSID_LEN+FLY_HV2_FRAME_TYPE_GOAWAY_ERROR_CODE_LEN)/FLY_HV2_OCTET_LEN;
	frame->payload = fly_pballoc(frame->pool, frame->payload_len);

	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, 0, false, frame->sid);
	__fly_goaway_payload(frame, state, false, FLY_HV2_INTERNAL_ERROR);
	__fly_send_frame(frame);

	state->goaway = true;
	fly_hv2_close_handle(event, state);
}

int fly_hv2_add_header_by_index(struct fly_hv2_stream *stream, uint32_t index);
int fly_hv2_add_header_by_indexname(struct fly_hv2_stream *stream, __fly_unused uint32_t index, __fly_unused uint8_t *value, __fly_unused uint32_t value_len, __fly_unused bool huffman_value, fly_buffer_c *__c, enum fly_hv2_index_type index_type);
int fly_hv2_add_header_by_name(struct fly_hv2_stream *stream, uint8_t *name, uint32_t name_len, bool huffman_name, uint8_t *value, uint32_t value_len, bool huffman_value, fly_buffer_c *__nc, fly_buffer_c *__vc, enum fly_hv2_index_type index_type);

struct fly_hv2_static_table static_table[] = {
	{1,  ":authority"						, NULL},
	{2,  ":method"							, "GET"},
	{3,  ":method"							, "POST"},
	{4,  ":path"							, "/"},
	{5,  ":path"							, "/index.html"},
	{6,  ":scheme"							, "http"},
	{7,  ":scheme"							, "https"},
	{8,  ":status"							, "200"},
	{9,  ":status"							, "204"},
	{10, ":status"							, "206"},
	{11, ":status"							, "304"},
	{12, ":status"							, "400"},
	{13, ":status"							, "404"},
	{14, ":status"							, "500"},
	{15, "accept-charset"					, NULL},
	{16, "accept-encoding"					, NULL},
	{17, "accept-language"					, "gzip, deflate"},
	{18, "accept-ranges"					, NULL},
	{19, "accept"							, NULL},
	{20, "access-control-allow-origin"		, NULL},
	{21, "age"								, NULL},
	{22, "allow"							, NULL},
	{23, "authorization"					, NULL},
	{24, "cache-control"					, NULL},
	{25, "content-disposition"				, NULL},
	{26, "content-encoding"					, NULL},
	{27, "content-language"					, NULL},
	{28, "content-length"					, NULL},
	{29, "content-location"					, NULL},
	{30, "content-range"					, NULL},
	{31, "content-type"						, NULL},
	{32, "cookie"							, NULL},
	{33, "date"								, NULL},
	{34, "etag"								, NULL},
	{35, "expect"							, NULL},
	{36, "expires"							, NULL},
	{37, "from"								, NULL},
	{38, "host"								, NULL},
	{39, "if-match"							, NULL},
	{40, "if-modified-since"				, NULL},
	{41, "if-none-matcho"					, NULL},
	{42, "if-range"							, NULL},
	{43, "if-unmodified-since"				, NULL},
	{44, "last-modified"					, NULL},
	{45, "link"								, NULL},
	{46, "location"							, NULL},
	{47, "max-forwards"						, NULL},
	{48, "proxy-authenticate"				, NULL},
	{49, "proxy-authorization"				, NULL},
	{50, "range"							, NULL},
	{51, "referer"							, NULL},
	{52, "refresh"							, NULL},
	{53, "retry-after"						, NULL},
	{54, "server"							, NULL},
	{55, "set-cookie"						, NULL},
	{56, "strict-transport-security"		, NULL},
	{57, "transfer-encoding"				, NULL},
	{58, "user-agent"						, NULL},
	{59, "vary"								, NULL},
	{60, "via"								, NULL},
	{61, "www-authenticate"					, NULL},
};

void fly_hv2_dynamic_table_init(struct fly_hv2_state *state)
{
	struct fly_hv2_dynamic_table *dt;

	dt = fly_pballoc(state->pool, sizeof(struct fly_hv2_dynamic_table));
	dt->entry_size = 0;
	dt->prev = dt->prev;
	dt->next = dt->next;
	state->dtable = dt;
	state->dtable_entry_count = 0;
	state->dtable_size = 0;
	state->dtable_max_index = FLY_HV2_STATIC_TABLE_LENGTH;
}

void fly_hv2_dynamic_table_remove_entry(struct fly_hv2_state *state);
static void __fly_hv2_dynamic_table_add_entry(struct fly_hv2_state *state, struct fly_hv2_dynamic_table *dt, size_t nlen, size_t vlen)
{
	dt->hname_len = nlen;
	dt->hvalue_len = vlen;
	if (state->dtable_entry_count == 0){
		dt->prev = state->dtable;
		dt->next = state->dtable;
		state->ldtable = dt;
	}else{
		state->dtable->next->prev = dt;
		dt->next = state->dtable->next;
		dt->prev = state->dtable;
	}

	state->dtable->next = dt;
	state->dtable_entry_count++;
	state->dtable_max_index = state->dtable_entry_count+FLY_HV2_STATIC_TABLE_LENGTH;
#define FLY_HV2_DTABLE_OVERHEAD				(32)
	state->dtable_size += (nlen+vlen+FLY_HV2_DTABLE_OVERHEAD);
	/* over limit of header table size */
	while(state->dtable_size>state->header_table_size)
		/* TODO: remove dynamic table entry in order from the back */
		fly_hv2_dynamic_table_remove_entry(state);
	return;
}

int fly_hv2_dynamic_table_add_entry(struct fly_hv2_state *state, void *nptr, size_t nlen, void *vptr, size_t vlen)
{
	struct fly_hv2_dynamic_table *dt;

	dt = fly_pballoc(state->pool, sizeof(struct fly_hv2_dynamic_table));
	if (fly_unlikely_null(dt))
		return -1;
	dt->entry_size = (nlen+vlen+FLY_HV2_DTABLE_OVERHEAD);
	dt->next = state->dtable->next;
	if (nlen){
		dt->hname = fly_pballoc(state->pool, sizeof(char)*nlen);
		memcpy(dt->hname, nptr, nlen);
	}else
		dt->hname = NULL;

	if (vlen){
		dt->hvalue = fly_pballoc(state->pool, sizeof(char)*vlen);
		memcpy(dt->hvalue, vptr, vlen);
	}else
		dt->hvalue = NULL;

	__fly_hv2_dynamic_table_add_entry(state, dt, nlen, vlen);
	return 0;
}

int fly_hv2_dynamic_table_add_entry_bv(struct fly_hv2_state *state, void *nptr, size_t nlen, fly_buffer_c *vc, void *vptr, size_t vlen)
{
	struct fly_hv2_dynamic_table *dt;

	dt = fly_pballoc(state->pool, sizeof(struct fly_hv2_dynamic_table));
	if (fly_unlikely_null(dt))
		return -1;
	dt->entry_size = (nlen+vlen+FLY_HV2_DTABLE_OVERHEAD);
	dt->next = state->dtable->next;
	if (nlen){
		dt->hname = fly_pballoc(state->pool, sizeof(char)*nlen);
		memcpy(dt->hname, nptr, nlen);
		dt->hname_len = nlen;
	}else{
		dt->hname = NULL;
		dt->hname_len = 0;
	}

	if (vlen){
		dt->hvalue = fly_pballoc(state->pool, sizeof(char)*vlen);
		fly_buffer_memcpy(dt->hvalue, vptr, vc, vlen);
		dt->hvalue_len = vlen;
	}else{
		dt->hvalue = NULL;
		dt->hvalue_len = 0;
	}

	__fly_hv2_dynamic_table_add_entry(state, dt, nlen, vlen);
	return 0;
}

int fly_hv2_dynamic_table_add_entry_bvn(struct fly_hv2_state *state, fly_buffer_c *nc, void *nptr, size_t nlen, fly_buffer_c *vc, void *vptr, size_t vlen)
{
	struct fly_hv2_dynamic_table *dt;
#define FLY_HV2_DTABLE_OVERHEAD				(32)

	dt = fly_pballoc(state->pool, sizeof(struct fly_hv2_dynamic_table));
	if (fly_unlikely_null(dt))
		return -1;
	dt->entry_size = (nlen+vlen+FLY_HV2_DTABLE_OVERHEAD);
	dt->next = state->dtable->next;
	if (nlen){
		dt->hname = fly_pballoc(state->pool, sizeof(char)*nlen);
		fly_buffer_memcpy(dt->hname, nptr, nc, nlen);
	}else
		dt->hname = NULL;

	if (vlen){
		dt->hvalue = fly_pballoc(state->pool, sizeof(char)*vlen);
		fly_buffer_memcpy(dt->hvalue, vptr, vc, vlen);
	}else
		dt->hvalue = NULL;

	__fly_hv2_dynamic_table_add_entry(state, dt, nlen, vlen);
	return 0;
}

void fly_hv2_dynamic_table_remove_entry(struct fly_hv2_state *state)
{
	if (state->dtable_entry_count == 0)
		return;

	/* remove last entry */
	struct fly_hv2_dynamic_table *dt = state->ldtable;

	dt->prev->next = state->dtable;
	state->ldtable = dt->prev;
	state->dtable_size -= dt->entry_size;
	if (dt->hname)
		fly_pbfree(state->pool, dt->hname);
	if (dt->hvalue)
		fly_pbfree(state->pool, dt->hvalue);
	state->dtable_entry_count--;
	state->dtable_max_index--;
}

void fly_hv2_dynamic_table_release(struct fly_hv2_state *state)
{
	if (state->dtable_entry_count == 0)
		return;

	while(state->dtable_entry_count)
		fly_hv2_dynamic_table_remove_entry(state);
	fly_pbfree(state->pool, state->dtable);
}

#ifdef DEBUG
bool fly_hv2_is_index_header_field(uint8_t *pl)
#else
static inline bool fly_hv2_is_index_header_field(uint8_t *pl)
#endif
{
	/*	RFC7541: 6.1
	 *	| 0 |  1 2 3 4 5 6 7  |
	 *	|---------------------|
	 *	| 1 |   index (7+)    |
	 *	-----------------------
	 */
	return (*pl & (1<<7)) ? true : false;
}

#ifdef DEBUG
bool fly_hv2_is_index_header_update(uint8_t *pl)
#else
static inline bool fly_hv2_is_index_header_update(uint8_t *pl)
#endif
{
	/*	RFC7541: 6.2.1
	 *	| 0 | 1 | 2 3 4 5 6 7  |
	 *	|----------------------|
	 *	| 0 | 1 |  index (6+)  |
	 *	------------------------
	 */
	return ((*pl & (1<<7)) == 0) && ((*pl & (1<<6)) != 0) ? true : false;
}

#ifdef DEBUG
bool fly_hv2_is_index_header_noupdate(uint8_t *pl)
#else
static inline bool fly_hv2_is_index_header_noupdate(uint8_t *pl)
#endif
{
	/*	RFC7541: 6.2.2
	 *	| 0 | 1 | 2 | 3 |   4 5 6 7    |
	 *	|------------------------------|
	 *	| 0 | 0 | 0 | 0 |  index (4+)  |
	 *	--------------------------------
	 */

	/*
	 *  ex. *pl = 01101010
	 *  step1. *pl >> 4			   ---> 00000110
	 *  step2. step1 & ((1<<4)-1)  ---> 00000110
	 *				   ~~~~~~~~~~
	 *				    00001111
	 *  step3. step2 == ((1<<4)-1) ---> 0
	 */
	return ((*pl>>4) & ((1<<4)-1)) == 0 ? true : false;
}

#ifdef DEBUG
bool fly_hv2_is_index_header_noindex(uint8_t *pl)
#else
static inline bool fly_hv2_is_index_header_noindex(uint8_t *pl)
#endif
{
	/*	RFC7541: 6.2.2
	 *	| 0 | 1 | 2 | 3 |   4 5 6 7    |
	 *	|------------------------------|
	 *	| 0 | 0 | 0 | 1 |  index (4+)  |
	 *	--------------------------------
	 */

	/*
	 *  ex. *pl = 01101010
	 *  step1. *pl >> 4			   ---> 00000110
	 *  step2. step1 & ((1<<4)-1)  ---> 00000110
	 *				   ~~~~~~~~~~
	 *				    00001111
	 *  step3. step2 == 00000001 ---> 0
	 */
	return (((*pl>>4) & ((1<<4)-1)) == 0x1) ? true : false;
}

#define FLY_HV2_INT_CONTFLAG(p)			(1<<(7))
#define FLY_HV2_INT_BIT_PREFIX(p)		((1<<(p)) - 1)
#define FLY_HV2_INT_BIT_VALUE(p)			((1<<(7)) - 1)
void fly_hv2_set_integer(uint32_t integer, uint8_t **pl, fly_buffer_c **__c, __fly_unused uint32_t *update, uint8_t prefix_bit)
{
	fly_buffer_t *__b;
	**pl &= (~FLY_HV2_INT_BIT_PREFIX(prefix_bit));
	if (integer < (uint32_t) FLY_HV2_INT_BIT_PREFIX(prefix_bit)){
		if (update)
			(*update)++;
		**pl |=  (uint8_t) integer;
		fly_update_buffer((*__c)->buffer, 1);
		__b = (*__c)->buffer;
		*pl = (uint8_t *) fly_buffer_lunuse_ptr(__b);
		*__c = fly_buffer_last_chain((*__c)->buffer);
	}else{
		int now = integer - FLY_HV2_INT_BIT_PREFIX(prefix_bit);
		**pl |=  (uint8_t) integer;
		if (update)
			(*update)++;
		fly_update_buffer((*__c)->buffer, 1);
		__b = (*__c)->buffer;
		*pl = (uint8_t *) fly_buffer_lunuse_ptr(__b);
		*__c = fly_buffer_last_chain((*__c)->buffer);

		while(now>=FLY_HV2_INT_CONTFLAG(prefix_bit)){
			**pl = (now%FLY_HV2_INT_CONTFLAG(prefix_bit))+FLY_HV2_INT_CONTFLAG(prefix_bit);
			if (update)
				(*update)++;
			**pl |= FLY_HV2_INT_CONTFLAG(prefix_bit);
			fly_update_buffer((*__c)->buffer, 1);
			*pl = (uint8_t *) fly_buffer_lunuse_ptr(__b);
			__b = (*__c)->buffer;
			*__c = fly_buffer_last_chain((*__c)->buffer);
			now /= FLY_HV2_INT_CONTFLAG(prefix_bit);
		}

		**pl = now;
		**pl &= FLY_HV2_INT_BIT_VALUE(prefix_bit);
		fly_update_buffer((*__c)->buffer, 1);
		*pl = (uint8_t *) fly_buffer_lunuse_ptr(__b);
		*__c = fly_buffer_last_chain((*__c)->buffer);
		if (update)
			(*update)++;
	}
	return;
}

/*
 *	RFC 7541 "HPACK: Header Compression for HTTP/2"
 *
 *	5.1 Integer Repression
 *
 */
uint32_t fly_hv2_integer(uint8_t **pl, fly_buffer_c **__c, __fly_unused uint32_t *update, uint8_t prefix_bit)
{
	/*
	 *	Check whether integer value is less than 2^N-1(N=prefix_bit)
	 */
	if (((*pl)[0]&FLY_HV2_INT_BIT_PREFIX(prefix_bit)) == FLY_HV2_INT_BIT_PREFIX(prefix_bit)){
		bool cont = true;
		uint32_t number=(uint32_t) FLY_HV2_INT_BIT_PREFIX(prefix_bit);
		size_t power = 0;

		while(cont){
			(*update)++;
			*pl = fly_update_chain(__c, *pl, 1);
			/*
			 *    0   1  2  3  4  5  6  7
			 *  --------------------------
			 *  |cont|   value-(2^N-1)   |
			 *  --------------------------
			 *
			 *  if bit in cont is 1, it continues to the next byte.
			 *
			 */
			cont = **pl & FLY_HV2_INT_CONTFLAG(prefix_bit) ? true : false;
			number += (((**pl)&FLY_HV2_INT_BIT_VALUE(prefix_bit)) * (1<<power));
			power += 7;
		}
		(*update)++;
		*pl = fly_update_chain(__c, *pl, 1);
		return number;
	}else{
		/* If no, decode only N-bit prefix */
		(*update)++;
		uint32_t number=0;
		number |= (uint32_t) **pl&(uint32_t) FLY_HV2_INT_BIT_PREFIX(prefix_bit);
		*pl = fly_update_chain(__c, *pl, 1);
		return number;
	}
}

static inline bool fly_hv2_is_huffman_encoding(uint8_t *pl)
{
	return (*pl & (1<<7)) ? true : false;
}


bool fly_hv2_is_header_literal(uint8_t *pl)
{
	return (*pl & ((1<<6)-1)) ? false : true;
}

int fly_hv2_parse_headers(fly_hv2_stream_t *stream, uint32_t length, uint8_t *payload, fly_buffer_c *__c)
{
	uint32_t total = 0;
	enum fly_hv2_index_type index_type;
	while(total < length){
		uint32_t index;
		bool huffman_name, huffman_value;
		uint32_t name_len, value_len;
		uint8_t *name, *value;
		if (fly_hv2_is_index_header_field(payload)){

			index = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_INDEX_PREFIX_BIT);
			index -= 1;
			/* found header field from static or dynamic table */
			if (fly_hv2_add_header_by_index(stream, index) == -1)
				return -1;
		}else{
			/* RFC7541: 6.2.1 */
			/* literal header field */
			/* update index */
			if (fly_hv2_is_index_header_update(payload)){
				/*
				 *   0   1   2 3 4 5 6 7
				 * |---------------------|
				 * | 0 | 1 |    index    |
				 * |---------------------|
				 */
				index_type = INDEX_UPDATE;
			}else if (fly_hv2_is_index_header_noupdate(payload))
				/*
				 *   0   1   2   3    4 5 6 7
				 * |----------------------------|
				 * | 0 | 0 | 0 | 0 |   index    |
				 * |----------------------------|
				 */
				index_type = INDEX_NOUPDATE;
			else if (fly_hv2_is_index_header_noindex(payload))
				/*
				 *   0   1   2   3    4 5 6 7
				 * |----------------------------|
				 * | 0 | 0 | 0 | 1 |   index    |
				 * |----------------------------|
				 */
				index_type = NOINDEX;
			else{
				/* invalid header field */
				return -1;
			}

			/* header field name by index */
			if (!fly_hv2_is_header_literal(payload)){
				uint8_t prefix_bit;
				switch (index_type){
				case INDEX_UPDATE:
					prefix_bit = FLY_HV2_LITERAL_UPDATE_PREFIX_BIT;
					break;
				case INDEX_NOUPDATE:
					prefix_bit = FLY_HV2_LITERAL_NOUPDATE_PREFIX_BIT;
					break;
				case NOINDEX:
					prefix_bit = FLY_HV2_LITERAL_NOINDEX_PREFIX_BIT;
					break;
				default:
					FLY_NOT_COME_HERE
				}
				index = fly_hv2_integer(&payload, &__c, &total, prefix_bit);
				index -= 1;
				huffman_value = fly_hv2_is_huffman_encoding(payload);
				value_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_VALUE_PREFIX_BIT);
				value = payload;
				if (fly_hv2_add_header_by_indexname(stream, index, value, value_len, huffman_value, __c, index_type) == -1)
					return -1;

				/* update ptr */
				total+=value_len;
				payload = fly_update_chain(&__c, payload, value_len);
			}else{
				fly_buffer_c *__nc;
				/* go forward a byte of index */
				payload = fly_update_chain_one(&__c, payload);
				total++;
				/* header field name by literal */
				huffman_name = fly_hv2_is_huffman_encoding(payload);
				name_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_NAME_PREFIX_BIT);
				name = payload;
				__nc = __c;
				/* update ptr */
				payload = fly_update_chain(&__c, name, name_len);
				total+=name_len;

				huffman_value = fly_hv2_is_huffman_encoding(payload);
				value_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_VALUE_PREFIX_BIT);
				value = payload;

				if (fly_hv2_add_header_by_name(stream, name, name_len, huffman_name, value, value_len, huffman_value, __nc, __c, index_type) == -1)
					return -1;

				/* update ptr */
				payload = fly_update_chain(&__c, value, value_len);
				total+=value_len;
			}
		}
	}
	return 0;
}

int fly_hv2_huffman_decode(fly_pool_t *pool, fly_buffer_t **res, uint32_t *decode_len, uint8_t *encoded, uint32_t len, fly_buffer_c *__c);

int fly_hv2_add_header_by_index(struct fly_hv2_stream *stream, uint32_t index)
{
	fly_hv2_state_t *state;
	const char *name=NULL, *value=NULL;
	size_t name_len=0, value_len=0;

	state = stream->state;
	/* static table */
	if (index < FLY_HV2_STATIC_TABLE_LENGTH){
		name = static_table[index].hname;
		name_len = strlen(name);

		value = static_table[index].hvalue;
		value_len = value ? strlen(value) : 0;
	}else{
	/* dynamic table */
		struct fly_hv2_dynamic_table *dt=state->dtable->next;

		for (size_t i=FLY_HV2_STATIC_TABLE_LENGTH; i<state->dtable_max_index; i++){
			if (i==index){
				name = dt->hname;
				name_len = dt->hname_len;
				value = dt->hvalue;
				value_len = dt->hvalue_len;
				break;
			}
			dt = dt->next;
		}
	}
	if (fly_header_add(stream->request->header, (fly_hdr_name *) name, name_len, (fly_hdr_value *) value, value_len) == -1)
		return -1;

	return 0;
}

int fly_hv2_add_header_by_indexname(struct fly_hv2_stream *stream, uint32_t index, uint8_t *value, uint32_t value_len, bool huffman_value, fly_buffer_c *__c, enum fly_hv2_index_type index_type)
{
	fly_buffer_t *buf;
	uint32_t len;
	fly_hv2_state_t *state;
	const char *name=NULL;
	size_t name_len=0;

	state = stream->state;
	/* static table */
	if (index < FLY_HV2_STATIC_TABLE_LENGTH){
		name = static_table[index].hname;
		name_len = strlen(name);
	}else{
	/* dynamic table */
		struct fly_hv2_dynamic_table *dt=state->dtable->next;

		for (size_t i=FLY_HV2_STATIC_TABLE_LENGTH; i<state->dtable_max_index; i++){
			if (i==index){
				name = dt->hname;
				name_len = dt->hname_len;
			}
			dt = dt->next;
		}
	}

	if (huffman_value){
		/* update: payload */
		if (fly_hv2_huffman_decode(stream->request->header->pool, &buf, &len, value, value_len, __c) == -1)
			return -1;
		/* header add */
		if (fly_header_addbv(fly_buffer_first_chain(buf), stream->request->header, (fly_hdr_name *) name, name_len, fly_buffer_first_useptr(buf), len) == -1)
			return -1;

		if (index_type == INDEX_UPDATE){
			if (fly_hv2_dynamic_table_add_entry_bv(state, (char *) name, name_len, fly_buffer_first_chain(buf), fly_buffer_first_useptr(buf), buf->use_len) == -1)
				return -1;
		}

		fly_buffer_release(buf);
	}else{
		/* header add */
		if (fly_header_addbv(__c, stream->request->header, (fly_hdr_name *) name, name_len, (fly_hdr_value *) value, value_len) == -1)
			return -1;

		if (index_type == INDEX_UPDATE){
			if (fly_hv2_dynamic_table_add_entry_bv(state, (char *) name, name_len, __c, value, value_len) == -1)
				return -1;
		}

	}

	return 0;
}

int fly_hv2_add_header_by_name(struct fly_hv2_stream *stream, uint8_t *name, uint32_t name_len, bool huffman_name, uint8_t *value, uint32_t value_len, bool huffman_value, fly_buffer_c *__nc, fly_buffer_c *__vc, enum fly_hv2_index_type index_type)
{
	fly_buffer_t *nbuf, *vbuf;
	uint32_t nlen, vlen;
	fly_hv2_state_t *state;
	char *__n, *__v;
	int res;

	state = stream->state;
	if (huffman_name && fly_hv2_huffman_decode(stream->request->header->pool, &nbuf, &nlen, name, name_len, __nc) == -1)
		return -1;

	if (huffman_value && fly_hv2_huffman_decode(stream->request->header->pool, &vbuf, &vlen, value, value_len, __vc) == -1)
		return -1;

	/* header add */
#define FLY_HV2_ADD_HEADER_NAME_LEN				\
		huffman_name ? nlen : name_len
#define FLY_HV2_ADD_HEADER_VALUE_LEN				\
		huffman_value ? vlen : value_len
	__n = fly_pballoc(stream->request->header->pool, FLY_HV2_ADD_HEADER_NAME_LEN);
	if (fly_unlikely_null(__n))
		return -1;
	__v = fly_pballoc(stream->request->header->pool, FLY_HV2_ADD_HEADER_VALUE_LEN);
	if (fly_unlikely_null(__v)){
		fly_pbfree(stream->request->header->pool, __n);
		return -1;
	}

	if (huffman_name)
		fly_buffer_memcpy(__n, fly_buffer_first_useptr(nbuf), fly_buffer_first_chain(nbuf), nlen);
	else
		memcpy(__n, name, name_len);

	if (huffman_value)
		fly_buffer_memcpy(__v, fly_buffer_first_useptr(vbuf), fly_buffer_first_chain(vbuf), vlen);
	else
		memcpy(__v, value, value_len);

	if (fly_header_add(stream->request->header, __n, FLY_HV2_ADD_HEADER_NAME_LEN, __v, FLY_HV2_ADD_HEADER_VALUE_LEN) == -1)
		goto error;

	if (index_type == INDEX_UPDATE){
		if (fly_hv2_dynamic_table_add_entry(state, (char *) __n, FLY_HV2_ADD_HEADER_NAME_LEN, (char *) __v, FLY_HV2_ADD_HEADER_VALUE_LEN) == -1)
			return -1;
	}
	goto success;
error:
	res = -1;
	goto end;
success:
	res = 0;
	goto end;

end:
	fly_pbfree(stream->request->header->pool, __n);
	fly_pbfree(stream->request->header->pool, __v);

#undef FLY_HV2_ADD_HEADER_NAME_LEN
#undef FLY_HV2_ADD_HEADER_VALUE_LEN
#undef FLY_HV2_ADD_HEADER_NAME_CPY
#undef FLY_HV2_ADD_HEADER_VALUE_CPY
	return res;
}


struct fly_hv2_huffman{
	uint16_t sym;
	uint64_t code_as_hex;
	int		 len_in_bits;
};

#define FLY_HV2_HUFFMAN_HUFFMAN_LEN		\
	(sizeof(huffman_codes)/sizeof(struct fly_hv2_huffman))
static struct fly_hv2_huffman huffman_codes[] = {
	{ 48,		0x0,				5 },
	{ 49, 		0x1, 				5 },
	{ 50, 		0x2, 				5 },
	{ 97, 		0x3, 				5 },
	{ 99, 		0x4, 				5 },
	{ 101,		0x5, 				5 },
	{ 105, 		0x6, 				5 },
	{ 111, 		0x7, 				5 },
	{ 115, 		0x8, 				5 },
	{ 116, 		0x9, 				5 },
	{ 32,		0x14,				6 },
	{ 37, 		0x15, 				6 },
	{ 45, 		0x16, 				6 },
	{ 46, 		0x17, 				6 },
	{ 47, 		0x18, 				6 },
	{ 51, 		0x19, 				6 },
	{ 52, 		0x1a, 				6 },
	{ 53, 		0x1b, 				6 },
	{ 54, 		0x1c, 				6 },
	{ 55, 		0x1d, 				6 },
	{ 56, 		0x1e, 				6 },
	{ 57, 		0x1f, 				6 },
	{ 61, 		0x20, 				6 },
	{ 65, 		0x21, 				6 },
	{ 95, 		0x22, 				6 },
	{ 98, 		0x23, 				6 },
	{ 100,		0x24, 				6 },
	{ 102, 		0x25, 				6 },
	{ 103, 		0x26, 				6 },
	{ 104, 		0x27, 				6 },
	{ 108, 		0x28, 				6 },
	{ 109, 		0x29, 				6 },
	{ 110, 		0x2a, 				6 },
	{ 112, 		0x2b, 				6 },
	{ 114, 		0x2c, 				6 },
	{ 117, 		0x2d, 				6 },
	{ 58,		0x5c, 				7 },
	{ 66, 		0x5d, 				7 },
	{ 67, 		0x5e, 				7 },
	{ 68, 		0x5f, 				7 },
	{ 69,		0x60, 				7 },
	{ 70,		0x61, 				7 },
	{ 71, 		0x62, 				7 },
	{ 72, 		0x63, 				7 },
	{ 73, 		0x64, 				7 },
	{ 74,		0x65, 				7 },
	{ 75, 		0x66, 				7 },
	{ 76, 		0x67, 				7 },
	{ 77, 		0x68, 				7 },
	{ 78, 		0x69, 				7 },
	{ 79, 		0x6a, 				7 },
	{ 80, 		0x6b, 				7 },
	{ 81, 		0x6c, 				7 },
	{ 82, 		0x6d, 				7 },
	{ 83, 		0x6e, 				7 },
	{ 84, 		0x6f, 				7 },
	{ 85, 		0x70, 				7 },
	{ 86, 		0x71, 				7 },
	{ 87, 		0x72, 				7 },
	{ 89, 		0x73, 				7 },
	{ 106,		0x74, 				7 },
	{ 107, 		0x75, 				7 },
	{ 113, 		0x76, 				7 },
	{ 118, 		0x77, 				7 },
	{ 119, 		0x78, 				7 },
	{ 120, 		0x79, 				7 },
	{ 121, 		0x7a, 				7 },
	{ 122, 		0x7b, 				7 },
	{ 38,		0xf8, 				8 },
	{ 42, 		0xf9, 				8 },
	{ 44, 		0xfa, 				8 },
	{ 59, 		0xfb, 				8 },
	{ 88, 		0xfc, 				8 },
	{ 90, 		0xfd, 				8 },
	{ 33, 		0x3f8,				10 },
	{ 34, 		0x3f9, 				10 },
	{ 40, 		0x3fa, 				10 },
	{ 41, 		0x3fb, 				10 },
	{ 63, 		0x3fc, 				10 },
	{ 39, 		0x7fa, 				11 },
	{ 43, 		0x7fb, 				11 },
	{ 124,		0x7fc, 				11 },
	{ 35,		0xffa, 				12 },
	{ 62, 		0xffb, 				12 },
	{ 0,		0x1ff8,				13 },
	{ 36,		0x1ff9, 			13 },
	{ 64, 		0x1ffa, 			13 },
	{ 91, 		0x1ffb, 			13 },
	{ 93, 		0x1ffc, 			13 },
	{ 126,		0x1ffd, 			13 },
	{ 94,		0x3ffc, 			14 },
	{ 125,		0x3ffd, 			14 },
	{ 60,		0x7ffc, 			15 },
	{ 96,		0x7ffd, 			15 },
	{ 123,		0x7ffe, 			15 },
	{ 92,		0x7fff0,			19 },
	{ 195,		0x7fff1, 			19 },
	{ 208,		0x7fff2, 			19 },
	{ 128,		0xfffe6,			20 },
	{ 130,		0xfffe7,			20 },
	{ 131,		0xfffe8, 			20 },
	{ 162,		0xfffe9, 			20 },
	{ 184,		0xfffea, 			20 },
	{ 194,		0xfffeb, 			20 },
	{ 224,		0xfffec, 			20 },
	{ 226,		0xfffed, 			20 },
	{ 153,		0x1fffdc,			21 },
	{ 161,		0x1fffdd, 			21 },
	{ 167,		0x1fffde, 			21 },
	{ 172,		0x1fffdf, 			21 },
	{ 176,		0x1fffe0, 			21 },
	{ 177,		0x1fffe1, 			21 },
	{ 179,		0x1fffe2, 			21 },
	{ 209,		0x1fffe3, 			21 },
	{ 216,		0x1fffe4, 			21 },
	{ 217, 		0x1fffe5, 			21 },
	{ 227, 		0x1fffe6, 			21 },
	{ 229, 		0x1fffe7, 			21 },
	{ 230, 		0x1fffe8, 			21 },
	{ 129, 		0x3fffd2, 			22 },
	{ 132, 		0x3fffd3, 			22 },
	{ 133, 		0x3fffd4, 			22 },
	{ 134, 		0x3fffd5, 			22 },
	{ 136, 		0x3fffd6, 			22 },
	{ 146, 		0x3fffd7, 			22 },
	{ 154, 		0x3fffd8, 			22 },
	{ 156, 		0x3fffd9, 			22 },
	{ 160, 		0x3fffda, 			22 },
	{ 163, 		0x3fffdb, 			22 },
	{ 164, 		0x3fffdc, 			22 },
	{ 169, 		0x3fffdd, 			22 },
	{ 170, 		0x3fffde, 			22 },
	{ 173, 		0x3fffdf, 			22 },
	{ 178, 		0x3fffe0, 			22 },
	{ 181, 		0x3fffe1, 			22 },
	{ 185, 		0x3fffe2, 			22 },
	{ 186, 		0x3fffe3, 			22 },
	{ 187, 		0x3fffe4, 			22 },
	{ 189, 		0x3fffe5, 			22 },
	{ 190, 		0x3fffe6, 			22 },
	{ 196, 		0x3fffe7, 			22 },
	{ 198, 		0x3fffe8, 			22 },
	{ 228, 		0x3fffe9, 			22 },
	{ 232, 		0x3fffea, 			22 },
	{ 233, 		0x3fffeb, 			22 },
	{ 1,		0x7fffd8, 			23 },
	{ 135,		0x7fffd9, 			23 },
	{ 137, 		0x7fffda, 			23 },
	{ 138, 		0x7fffdb, 			23 },
	{ 139, 		0x7fffdc, 			23 },
	{ 140, 		0x7fffdd, 			23 },
	{ 141, 		0x7fffde, 			23 },
	{ 143, 		0x7fffdf, 			23 },
	{ 147, 		0x7fffe0, 			23 },
	{ 149, 		0x7fffe1, 			23 },
	{ 150, 		0x7fffe2, 			23 },
	{ 151, 		0x7fffe3, 			23 },
	{ 152, 		0x7fffe4, 			23 },
	{ 155, 		0x7fffe5, 			23 },
	{ 157, 		0x7fffe6, 			23 },
	{ 158, 		0x7fffe7, 			23 },
	{ 165, 		0x7fffe8, 			23 },
	{ 166, 		0x7fffe9, 			23 },
	{ 168, 		0x7fffea, 			23 },
	{ 174, 		0x7fffeb, 			23 },
	{ 175, 		0x7fffec, 			23 },
	{ 180, 		0x7fffed, 			23 },
	{ 182, 		0x7fffee, 			23 },
	{ 183, 		0x7fffef, 			23 },
	{ 188, 		0x7ffff0, 			23 },
	{ 191, 		0x7ffff1, 			23 },
	{ 197, 		0x7ffff2, 			23 },
	{ 231, 		0x7ffff3, 			23 },
	{ 239, 		0x7ffff4, 			23 },
	{ 9,		0xffffea,			24 },
	{ 142,		0xffffeb,			24 },
	{ 144, 		0xffffec, 			24 },
	{ 145, 		0xffffed, 			24 },
	{ 148, 		0xffffee, 			24 },
	{ 159, 		0xffffef, 			24 },
	{ 171, 		0xfffff0, 			24 },
	{ 206, 		0xfffff1, 			24 },
	{ 215, 		0xfffff2, 			24 },
	{ 225, 		0xfffff3, 			24 },
	{ 236, 		0xfffff4, 			24 },
	{ 237, 		0xfffff5, 			24 },
	{ 199, 		0x1ffffec,			25 },
	{ 207, 		0x1ffffed, 			25 },
	{ 234, 		0x1ffffee, 			25 },
	{ 235, 		0x1ffffef, 			25 },
	{ 192, 		0x3ffffe0, 			26 },
	{ 193, 		0x3ffffe1, 			26 },
	{ 200, 		0x3ffffe2, 			26 },
	{ 201, 		0x3ffffe3, 			26 },
	{ 202, 		0x3ffffe4, 			26 },
	{ 205, 		0x3ffffe5, 			26 },
	{ 210, 		0x3ffffe6, 			26 },
	{ 213, 		0x3ffffe7, 			26 },
	{ 218, 		0x3ffffe8, 			26 },
	{ 219, 		0x3ffffe9, 			26 },
	{ 238, 		0x3ffffea, 			26 },
	{ 240, 		0x3ffffeb, 			26 },
	{ 242, 		0x3ffffec, 			26 },
	{ 243, 		0x3ffffed, 			26 },
	{ 255, 		0x3ffffee, 			26 },
	{ 203, 		0x7ffffde, 			27 },
	{ 204, 		0x7ffffdf, 			27 },
	{ 211, 		0x7ffffe0, 			27 },
	{ 212, 		0x7ffffe1, 			27 },
	{ 214, 		0x7ffffe2, 			27 },
	{ 221, 		0x7ffffe3, 			27 },
	{ 222, 		0x7ffffe4, 			27 },
	{ 223, 		0x7ffffe5, 			27 },
	{ 241, 		0x7ffffe6, 			27 },
	{ 244, 		0x7ffffe7, 			27 },
	{ 245, 		0x7ffffe8, 			27 },
	{ 246, 		0x7ffffe9, 			27 },
	{ 247, 		0x7ffffea, 			27 },
	{ 248, 		0x7ffffeb, 			27 },
	{ 250, 		0x7ffffec, 			27 },
	{ 251, 		0x7ffffed, 			27 },
	{ 252, 		0x7ffffee, 			27 },
	{ 253, 		0x7ffffef, 			27 },
	{ 254, 		0x7fffff0, 			27 },
	{ 2,		0xfffffe2, 			28 },
	{ 3, 		0xfffffe3, 			28 },
	{ 4, 		0xfffffe4, 			28 },
	{ 5, 		0xfffffe5, 			28 },
	{ 6, 		0xfffffe6, 			28 },
	{ 7, 		0xfffffe7, 			28 },
	{ 8, 		0xfffffe8, 			28 },
	{ 11,		0xfffffe9, 			28 },
	{ 12, 		0xfffffea, 			28 },
	{ 14, 		0xfffffeb, 			28 },
	{ 15, 		0xfffffec, 			28 },
	{ 16, 		0xfffffed, 			28 },
	{ 17, 		0xfffffee, 			28 },
	{ 18, 		0xfffffef, 			28 },
	{ 19, 		0xffffff0, 			28 },
	{ 20, 		0xffffff1, 			28 },
	{ 21, 		0xffffff2, 			28 },
	{ 23, 		0xffffff3, 			28 },
	{ 24, 		0xffffff4, 			28 },
	{ 25, 		0xffffff5, 			28 },
	{ 26, 		0xffffff6, 			28 },
	{ 27, 		0xffffff7, 			28 },
	{ 28, 		0xffffff8, 			28 },
	{ 29, 		0xffffff9, 			28 },
	{ 30, 		0xffffffa, 			28 },
	{ 31, 		0xffffffb, 			28 },
	{ 127,		0xffffffc, 			28 },
	{ 220, 		0xffffffd, 			28 },
	{ 249, 		0xffffffe, 			28 },
	{ 10,		0x3ffffffc,			30 },
	{ 13, 		0x3ffffefd, 		30 },
	{ 22, 		0x3ffffffe, 		30 },
	{ 256,		0x3fffffff,			30 },
	{ -1,		0x0,				-1 },
};

__fly_unused static inline uint8_t __fly_huffman_code_digit(uint8_t c)
{
	uint8_t i=0;

	while((1<<++i) <= c)
		;
	return i;
}

static inline uint8_t fly_huffman_decode_k_bit(int k, struct fly_hv2_huffman *code)
{
	uint64_t spbit;

	spbit = code->code_as_hex & (1<<(code->len_in_bits-k-1));
	return spbit ? 1 : 0;
}

static inline uint8_t fly_huffman_decode_buf_bit(uint8_t j, uint8_t *ptr)
{
	uint8_t spbit;

	spbit = *ptr & (1<<(FLY_HV2_OCTET_LEN-1-j));
	return spbit ? 1 : 0;
}

static inline uint8_t fly_huffman_last_padding(uint8_t j, uint8_t *ptr)
{
	uint8_t eos;

	eos = (*ptr)&((1<<(FLY_HV2_OCTET_LEN-j))-1);
	return eos == ((1<<(FLY_HV2_OCTET_LEN-j))-1);
}

int fly_hv2_huffman_decode(fly_pool_t *pool, fly_buffer_t **res, uint32_t *decode_len, uint8_t *encoded, uint32_t len, fly_buffer_c *__c)
{
#define FLY_HUFFMAN_DECODE_BUFFER_INITLEN			1
#define FLY_HUFFMAN_DECODE_BUFFER_MAXLEN			100
#define FLY_HUFFMAN_DECODE_BUFFER_PERLEN			200
	fly_buffer_t *buf;
	fly_buf_p *bufp;
	uint8_t *ptr=encoded;
	int start_bit=0;

	buf = fly_buffer_init(pool, FLY_HUFFMAN_DECODE_BUFFER_INITLEN, FLY_HUFFMAN_DECODE_BUFFER_MAXLEN, FLY_HUFFMAN_DECODE_BUFFER_PERLEN);
	if (fly_unlikely_null(buf))
		return -1;

	while(len){
		int step = 0;
		struct fly_hv2_huffman *code;
		bufp = fly_buffer_lunuse_ptr(buf);

		/* padding check */
		if (len-1==0 && fly_huffman_last_padding(start_bit, ptr)){
			/* eos */
			break;
		}

		for (code=huffman_codes; code->len_in_bits>0; code++){
			step = 0;
			int j=start_bit;
			fly_buffer_c *p=__c;
			uint8_t *tmp=ptr;

			for (int k=0; k<code->len_in_bits; k++){
				if (!(fly_huffman_decode_k_bit(k, code) == fly_huffman_decode_buf_bit(j, tmp))){
					goto next_code;
				}

				if (++j>=FLY_HV2_OCTET_LEN){
					tmp = (uint8_t *) fly_update_chain_one(&p, tmp);
					j=0;
					step++;
				}
			}

			/* found huffman code */
			*(uint8_t *) bufp = code->sym;
			if (fly_update_buffer(buf, 1) == -1)
				return -1;
			start_bit = j;
			ptr = tmp;
			len -= step;
			__c=p;
			break;
next_code:
			continue;
		}
		if (code->len_in_bits <= 0)
			FLY_NOT_COME_HERE
	}
	*decode_len = buf->use_len;
	*res = buf;

	return 0;
}

int fly_hv2_parse_data(fly_event_t *event, fly_hv2_stream_t *stream, uint32_t length, uint8_t *payload, fly_buffer_c *__c)
{
	if (length == 0)
		return FLY_HV2_PARSE_DATA_SUCCESS;

	fly_body_t *body;
	fly_bodyc_t *bc;
	fly_request_t *req;
	size_t content_length;

	req = stream->request;
	content_length = fly_content_length(req->header);
	if (req->discard_body){
		/*
		 * send window_udpate_frame and recover window size
		 */
		// window update for stream
		if (fly_send_window_update_frame(stream, length, false) == -1)
			return FLY_HV2_PARSE_DATA_ERROR;
		// widnwo update for connection
		if (fly_send_window_update_frame(FLY_HV2_ROOT_STREAM(stream->state), length, false) == -1)
			return FLY_HV2_PARSE_DATA_ERROR;
		event->read_or_write |= FLY_WRITE;
		return FLY_HV2_PARSE_DATA_SUCCESS;
	}else if (req->body == NULL){
		body = fly_body_init(req->ctx);
		if (fly_unlikely_null(body))
			return FLY_HV2_PARSE_DATA_ERROR;

		if (content_length > req->ctx->max_request_content_length){
			req->discard_body = true;
			/*
			 * send window_udpate_frame and recover window size
			 */
			// window update for stream
			if (fly_send_window_update_frame(stream, length, false) == -1)
				return FLY_HV2_PARSE_DATA_ERROR;
			// window update for connection
			if (fly_send_window_update_frame(FLY_HV2_ROOT_STREAM(stream->state), length, false) == -1)
				return FLY_HV2_PARSE_DATA_ERROR;
			event->read_or_write |= FLY_WRITE;
			return FLY_HV2_PARSE_DATA_SUCCESS;
		}else{
			req->body = body;

			bc = fly_pballoc(body->pool, sizeof(uint8_t)*content_length);
			if (fly_unlikely_null(bc))
				return FLY_HV2_PARSE_DATA_ERROR;

			fly_body_setting(body, bc, content_length);
			body->next_ptr = bc;
		}
	}else{
		body = req->body;
		bc = body->next_ptr;
	}

#ifdef DEBUG
	assert(!req->discard_body);
#endif
	/* body overflow check */
	if (body->next_ptr+length > (body->body+body->body_len))
		return FLY_HV2_PARSE_DATA_ERROR;

	/* copy receive buffer to body content. */
	fly_buffer_memcpy((char *) bc, (char *) payload, __c, length);
	body->next_ptr += length;

	/*
	 * send window_udpate_frame and recover window size
	 */
	// window update for connection
	if (fly_send_window_update_frame(FLY_HV2_ROOT_STREAM(stream->state), length, false) == -1)
		return FLY_HV2_PARSE_DATA_ERROR;
	// window update for stream
	if (fly_send_window_update_frame(stream, length, false) == -1)
		return FLY_HV2_PARSE_DATA_ERROR;

	event->read_or_write |= FLY_WRITE;
#ifdef DEBUG
	float percent;
	percent = 100*(body->next_ptr-body->body)/content_length;
	printf("HTTP2 Recv Body: %ld/%ld(%d)\n", body->next_ptr-body->body, content_length, (int) percent);
#endif
	return FLY_HV2_PARSE_DATA_SUCCESS;
}

/*
 *	Create/Send Response.
 */

static inline bool __fly_colon(char c)
{
	return c == 0x3A ? true : false;
}

static const char *fly_pseudo_request[] = {
	FLY_HV2_REQUEST_PSEUDO_HEADER_METHOD,
	FLY_HV2_REQUEST_PSEUDO_HEADER_SCHEME,
	FLY_HV2_REQUEST_PSEUDO_HEADER_AUTHORITY,
	FLY_HV2_REQUEST_PSEUDO_HEADER_PATH,
	NULL
};

static inline bool __fly_hv2_pseudo_header(fly_hdr_c *c)
{
	return __fly_colon(*c->name) ? true : false;
}

int fly_hv2_request_line_from_header(fly_request_t *req)
{

	fly_hdr_ci *ci = req->header;

	fly_request_line_init(req);
	req->request_line->version = fly_match_version_from_type(V2);
	if (fly_unlikely_null(req->request_line->version))
		return -1;
#ifdef DEBUG
	printf("WORKER: Parse HTTP2 request:\n");
	printf("\tRequest HTTP ver: %s\n", req->request_line->version->full);
#endif
	/* convert pseudo header to request line */
	fly_hdr_c *__c;
	struct fly_bllist *__b;
	fly_for_each_bllist(__b, &ci->chain){
		__c = fly_bllist_data(__b, fly_hdr_c, blelem);
		if (__c->name_len>0 && __fly_hv2_pseudo_header(__c)){
			for (char **__p=(char **) fly_pseudo_request; *__p; __p++){
				if (strncmp(*__p, __c->name, strlen(*__p)) == 0){
					if (strncmp(*__p, FLY_HV2_REQUEST_PSEUDO_HEADER_METHOD, strlen(FLY_HV2_REQUEST_PSEUDO_HEADER_METHOD)) == 0){
#ifdef DEBUG
						printf("\tRequest method: %s\n", *__p);
#endif
						req->request_line->method = fly_match_method_name(__c->value);
						if (!req->request_line->method){
#ifdef DEBUG
							printf("\tmethod is invalid.\n");
#endif
							/* invalid method */
							return -1;
						}
					}else if (strncmp(*__p, FLY_HV2_REQUEST_PSEUDO_HEADER_SCHEME, strlen(FLY_HV2_REQUEST_PSEUDO_HEADER_SCHEME)) == 0){
#ifdef DEBUG
						printf("\tRequest scheme: %s\n", *__p);
#endif
						req->request_line->scheme = fly_match_scheme_name(__c->value);
						if (!req->request_line->scheme){
#ifdef DEBUG
							printf("\tschemeis invalid.\n");
#endif
							/* invalid method */
							return -1;
						}
					}else if (strncmp(*__p, FLY_HV2_REQUEST_PSEUDO_HEADER_AUTHORITY, strlen(FLY_HV2_REQUEST_PSEUDO_HEADER_AUTHORITY)) == 0){
					}else if (strncmp(*__p, FLY_HV2_REQUEST_PSEUDO_HEADER_PATH, strlen(FLY_HV2_REQUEST_PSEUDO_HEADER_PATH)) == 0){
#ifdef DEBUG
						printf("\tRequest uri: %s\n", *__p);
#endif
						fly_uri_set(req, __c->value, __c->value_len);
//						if (fly_query_parse_from_uri(req, &req->request_line->uri) == -1)
//							return -1;
					}else{
						FLY_NOT_COME_HERE
					}

					goto next_header;
				}
			}

			/* invalid pseudo request header */
			return -1;
next_header:
			continue;
		}
	}

	/* check lack of request line */
#define __fly_uri					req->request_line->uri.ptr
#define __fly_request_method		req->request_line->method
#define __fly_scheme				req->request_line->scheme
	if (!req->request_line || !__fly_uri || !__fly_request_method || !__fly_scheme){
		/* invalid request */
		return -1;
	}
#undef __fly_uri
#undef __fly_request_method
#undef __fly_scheme
	return 0;
}


int fly_hv2_responses(fly_event_t *e, fly_hv2_state_t *state __fly_unused)
{
	struct fly_hv2_response *res;
	struct fly_queue *__q;

	__q = state->responses.next;
	res = fly_queue_data(__q, struct fly_hv2_response, qelem);
	//e->event_data = res->response;
	fly_event_data_set(e, __p, res->response);

	return fly_hv2_response_event(e);
}

void fly_hv2_add_response(fly_hv2_state_t *state, struct fly_hv2_response *res)
{
	fly_queue_push(&state->responses, &res->qelem);
	state->response_count++;
	return;
}

void fly_hv2_remove_hv2_response(fly_hv2_state_t *state, struct fly_hv2_response *res, struct fly_event *e)
{
	fly_response_t *__res;

	__res = res->response;
	/* Check end response, but yet log */
	if (fly_end_response_yet_log(__res)){
#ifdef DEBUG
		printf("End response, but yet log\n");
#endif
		/* Register log event */
		fly_response_log(__res, e);
	}
	fly_response_release(__res);
	fly_queue_remove(&res->qelem);
	fly_pbfree(state->pool, res);
	state->response_count--;
	return;
}

void fly_hv2_remove_all_response(fly_hv2_state_t *state, struct fly_event *e)
{
	struct fly_queue *__q;
	struct fly_hv2_response *__r;

	while(state->response_count > 0){
		__q = fly_queue_pop(&state->responses);
		__r = (struct fly_hv2_response *) fly_queue_data(__q, struct fly_hv2_response, qelem);
		fly_hv2_remove_hv2_response(state, __r, e);
	}
	return;
}

void fly_hv2_remove_response(fly_hv2_state_t *state, fly_response_t *res, struct fly_event *e)
{
	struct fly_queue *__q;
	struct fly_hv2_response *__r;

	fly_for_each_queue(__q, &state->responses){
		__r = fly_queue_data(__q, struct fly_hv2_response, qelem);
		if (__r->response == res)
			return fly_hv2_remove_hv2_response(state, __r, e);
	}
	return;
}

int fly_hv2_response_event_handler(fly_event_t *e, fly_hv2_stream_t *stream)
{
#ifdef DEBUG
	printf("WORKER: prepare for HTTP2 Response \n");
#endif
	fly_request_t *request;
	fly_response_t *response;

	request = stream->request;

	/*
	 *	create response from request.
	 */
	if (fly_hv2_request_line_from_header(request) == -1){
		goto response_400;
	}

	if (fly_hv2_request_target_parse(request) == -1){
		goto response_400;
	}

	if (request->header)
		fly_check_cookie(request->header);
	if (request->discard_body)
		goto response_413;

	/* accept encoding parse */
	switch(fly_accept_encoding(request)){
	case FLY_ACCEPT_ENCODING_SUCCESS:
		break;
	case FLY_ACCEPT_ENCODING_SYNTAX_ERROR:
		goto response_400;
	case FLY_ACCEPT_ENCODING_ERROR:
		goto error;
	case FLY_ACCEPT_ENCODING_NOT_ACCEPTABLE:
		goto response_406;
	default:
		FLY_NOT_COME_HERE
	}

	/* accept mime parse */
	switch(fly_accept_mime(request)){
	case FLY_ACCEPT_MIME_SUCCESS:
		break;
	case FLY_ACCEPT_MIME_SYNTAX_ERROR:
		goto response_400;
	case FLY_ACCEPT_MIME_ERROR:
		goto error;
	case FLY_ACCEPT_MIME_NOT_ACCEPTABLE:
		goto response_406;
	default:
		FLY_NOT_COME_HERE
	}

	/* accept charset parse */
	switch(fly_accept_charset(request)){
	case FLY_ACCEPT_CHARSET_SUCCESS:
		break;
	case FLY_ACCEPT_CHARSET_SYNTAX_ERROR:
		goto response_400;
	case FLY_ACCEPT_CHARSET_ERROR:
		goto error;
	case FLY_ACCEPT_CHARSET_NOT_ACCEPTABLE:
		goto response_406;
	default:
		FLY_NOT_COME_HERE
	}

	/* accept language parse */
	switch(fly_accept_language(request)){
	case FLY_ACCEPT_LANG_SUCCESS:
		break;
	case FLY_ACCEPT_LANG_SYNTAX_ERROR:
		goto response_400;
	case FLY_ACCEPT_LANG_ERROR:
		goto error;
	case FLY_ACCEPT_LANG_NOT_ACCEPTABLE:
		goto response_406;
	default:
		FLY_NOT_COME_HERE
	}

	/* check of having body */
	size_t content_length;
	content_length = fly_content_length(request->header);
	if (!content_length)
		goto __response;

	fly_hdr_value *ev;
	fly_encoding_type_t *et;
	ev = fly_content_encoding_s(request->header);
	if (ev){
		/* not supported encoding */
		et = fly_supported_content_encoding(ev);
		if (!et)
			return -1;
		if (fly_decode_nowbody(request, et) == NULL)
			return -1;
	}

	if (fly_is_multipart_form_data(request->header))
		fly_body_parse_multipart(request);
__response:
	fly_event_request_fase(e, RESPONSE);

	/* Success parse request */
	enum method_type __mtype;
	fly_route_reg_t *route_reg;
	fly_route_t *route;
	fly_mount_t *mount;
	struct fly_mount_parts_file *pf;

	__mtype = request->request_line->method->type;
	route_reg = e->manager->ctx->route_reg;
	/* search from registerd route uri */
	route = fly_found_route(route_reg, &request->request_line->uri, __mtype);
	mount = e->manager->ctx->mount;
	if (route == NULL){
		int found_res;
		/* search from uri */
		found_res = fly_found_content_from_path(mount, &request->request_line->uri, &pf);
		if (__mtype != GET && found_res){
			goto response_405;
		}else if (__mtype == GET && found_res)
			goto response_path;
		goto response_404;
	}

	/* defined handler */
	if (fly_path_param_count(route->path_param) > 0){
		if (fly_parse_path_params_from_request(request, route) == -1)
			goto response_500;
	}
	response = route->function(request, route, route->data);
	if (response == NULL)
		goto response_500;
	fly_response_header_init(response, request);
	fly_header_hv2_state(response->header, stream->state);

#define FLY_REQ_HV2				is_fly_request_http_v2(request)
	if (fly_add_date(response->header, FLY_REQ_HV2) == -1)
		goto response_500;
	if (fly_add_server(response->header, FLY_REQ_HV2) == -1)
		goto response_500;
	if (fly_add_content_type(response->header, &default_route_response_mime, FLY_REQ_HV2) == -1)
		goto response_500;

	response->request = request;
	response->version = V2;
	goto response;
	FLY_NOT_COME_HERE

response_path:
	if (fly_if_none_match(request->header, pf))
		goto response_304;
	if (fly_if_modified_since(request->header, pf))
		goto response_304;

	response = fly_respf(request, pf);
	if (response == NULL)
		goto error;

	goto __response_event;
response:
	goto __response_event;
response_304:
	response = fly_304_response(request, pf);
	goto __response_event;
response_400:
	response = fly_400_response(request);
	goto __response_event;
response_404:
	response = fly_404_response(request);
	goto __response_event;
response_405:
	response = fly_405_response(request);
	goto __response_event;
response_406:
	response = fly_406_response(request);
	goto __response_event;
response_413:
	response = fly_413_response(request);
	goto __response_event;
response_500:
	response = fly_500_response(request);
	goto __response_event;
__response_event:
	//e->event_data = (void *) response;
	fly_event_data_set(e, __p, response);
	return fly_hv2_response_event(e);
error:
	return -1;
}

/*
 *	HTTP2 Send Data Frame
 */

/*
 *	 add ":status " to the begining of header
 */
int fly_status_code_pseudo_headers(fly_response_t *res)
{
	const char *stcode_str;

	stcode_str = fly_status_code_str_from_type(res->status_code);

	return fly_header_add_v2(res->header, ":status", strlen(":status"), (fly_hdr_value *) stcode_str, strlen(stcode_str), true);
}

int __fly_hv2_blocking_event(fly_event_t *e, fly_hv2_stream_t *stream)
{
	e->read_or_write = FLY_READ|FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	fly_event_data_set(e, __p, stream->state->connect);
	fly_event_socket(e);

	return fly_event_register(e);
}


/*
 *
 *	HTTP2 response
 *
 */
int fly_hv2_response_event(fly_event_t *e)
{
#ifdef DEBUG
	printf("WORKER: HTTP2 Response event handler\n");
#endif
	fly_response_t *res;
	fly_request_t *req;
	fly_hv2_stream_t *stream;
	struct fly_hv2_response *v2_res;

	res = (fly_response_t *) fly_event_data_get(e, __p);
	if (res->header == NULL)
		res->header = fly_header_init(res->request->ctx);
	stream = res->request->stream;
	res->header->state = stream->state;

	FLY_EVENT_END_HANDLER(e, fly_hv2_end_handle, stream->state->connect);
	FLY_EVENT_EXPIRED_HANDLER(e, fly_hv2_timeout_handle, stream->state->connect);
	/* already send headers */
	if (stream->end_send_data)
		goto log;
	else if (stream->end_send_headers)
		goto send_body;
	else if (res->fase == FLY_RESPONSE_HEADER)
		goto register_handler;

	if (stream->stream_state != FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE)
		/* invalid state */
		goto response_500;

	v2_res = fly_pballoc(stream->state->pool, sizeof(struct fly_hv2_response));
	v2_res->response = res;
	fly_hv2_add_response(stream->state, v2_res);

	res->fase = FLY_RESPONSE_HEADER;
	/* response headers */
	if (fly_status_code_pseudo_headers(res) == -1)
		goto response_500;

	fly_rcbs_t *rcbs=NULL;
	if (res->de != NULL)
		goto send_header;
	/* if there is default content, add content-length header */
	if (res->body == NULL && res->pf == NULL){
		rcbs = fly_default_content_by_stcode_from_event(e, res->status_code);
		res->rcbs = rcbs;
		if (rcbs){
			if (fly_add_content_length_from_fd(res->header, rcbs->fd, true) == -1)
				goto response_500;
			if (fly_add_content_type(res->header, rcbs->mime, true) == -1)
				goto response_500;
		}
	}

	if (res->body){
		res->response_len = res->body->body_len;
		res->type = FLY_RESPONSE_TYPE_BODY;
	}else if (res->pf){
		if (res->pf->encoded){
			res->type = FLY_RESPONSE_TYPE_ENCODED;
			res->de = res->pf->de;
			res->response_len = res->de->contlen;
			res->original_response_len = res->pf->fs.st_size;
			res->encoded = true;
			res->encoding_type = res->de->etype;
		}else{
			res->response_len = res->count;
			res->original_response_len = res->response_len;
			res->type = FLY_RESPONSE_TYPE_PATH_FILE;
		}
	}else if (res->rcbs){
		if (res->rcbs->encoded){
			res->type = FLY_RESPONSE_TYPE_ENCODED;
			res->de = res->rcbs->de;
			res->response_len = res->de->contlen;
			res->original_response_len = res->rcbs->fs.st_size;
			res->encoded = true;
			res->encoding_type = res->de->etype;
		}else{
			res->response_len = rcbs->fs.st_size;
			res->original_response_len = res->response_len;
			res->type = FLY_RESPONSE_TYPE_DEFAULT;
		}
	}else{
		res->response_len = 0;
		res->type = FLY_RESPONSE_TYPE_NOCONTENT;
	}

	/* Response must not be encoded */
	if (res->dont_encode){
		res->encoded = false;
		fly_add_content_length(res->header, res->response_len, true);
		goto send_header;
	}
	/* encoding matching test */
	if (res->encoded \
			&& !fly_encoding_matching(res->request->encoding, res->encoding_type)){
		res->encoded = false;
		res->response_len = res->original_response_len;
	}

	if (res->encoded || fly_over_encoding_threshold_from_response(res)){
		if (!res->encoded)
			res->encoding_type = fly_decided_encoding_type(res->request->encoding);
		if (res->encoding_type != NULL)
			fly_add_content_encoding(res->header, res->encoding_type, true);
	}else
		res->encoding_type = NULL;

	/* if yet response body encoding */
	if (fly_encode_do(res) && !res->encoded){
		res->type = FLY_RESPONSE_TYPE_ENCODED;
		if (res->encoding_type->type == fly_identity){
			res->type = FLY_RESPONSE_TYPE_BODY;
			fly_add_content_length(res->header, res->response_len, true);
			goto send_header;
		}

		fly_de_t *__de;

		__de = fly_de_init(res->pool);
		if (res->pf){
			__de->decbuf = fly_buffer_init(__de->pool, FLY_RESPONSE_DECBUF_INIT_LEN, FLY_RESPONSE_DECBUF_CHAIN_MAX, FLY_RESPONSE_DECBUF_PER_LEN);
			__de->type = FLY_DE_FROM_PATH;
			__de->fd = res->pf->fd;
			__de->offset = res->offset;
			__de->count = res->pf->fs.st_size;
		}else if (res->rcbs){
			__de->decbuf = fly_buffer_init(__de->pool, FLY_RESPONSE_DECBUF_INIT_LEN, FLY_RESPONSE_DECBUF_CHAIN_MAX, FLY_RESPONSE_DECBUF_PER_LEN);
			__de->type = FLY_DE_FROM_PATH;
			__de->fd = res->rcbs->fd;
			__de->offset = 0;
			__de->count = res->rcbs->fs.st_size;
		}else if (res->body){
			__de->type = FLY_DE_ENCODE;
			__de->already_ptr = res->body->body;
			__de->already_len = res->body->body_len;
			__de->target_already_alloc = true;
		}else
			FLY_NOT_COME_HERE

		size_t __max;
		__max = fly_response_content_max_length();
		__de->encbuf = fly_buffer_init(__de->pool, FLY_RESPONSE_ENCBUF_INIT_LEN, FLY_RESPONSE_ENCBUF_CHAIN_MAX(__max), FLY_RESPONSE_ENCBUF_PER_LEN);
#ifdef DEBUG
		assert(__max < (size_t) (__de->encbuf->per_len*__de->encbuf->chain_max));
#endif
		__de->event = e;
		__de->response = res;
		__de->c_sockfd = e->fd;
		__de->etype = res->encoding_type;
		__de->bfs = 0;
		__de->end = false;
		res->de = __de;

		struct fly_err *__err;
		switch(res->encoding_type->encode(__de)){
		case FLY_ENCODE_SUCCESS:
			break;
		case FLY_ENCODE_ERROR:
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"Response encoding error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			goto response_500;
		case FLY_ENCODE_SEEK_ERROR:
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"Response encoding seek error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			goto response_500;
		case FLY_ENCODE_TYPE_ERROR:
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"Response encoding type error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			goto response_500;
		case FLY_ENCODE_READ_ERROR:
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"Response encoding read error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			goto response_500;
		case FLY_ENCODE_BUFFER_ERROR:
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"Response encoding buffer error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			goto response_413;
		default:
			FLY_NOT_COME_HERE
		}

		res->encoded = true;
		res->response_len = __de->contlen;
		res->type = FLY_RESPONSE_TYPE_ENCODED;
	}

	/* content length over limit */
	if (res->de && res->de->overflow)
		goto response_413;
	if (res->de)
		fly_add_content_length(res->header, res->de->contlen, true);
	else
		fly_add_content_length(res->header, res->response_len, true);
send_header:
#ifdef DEBUG
	printf("WORKER: Send Header\n");
#endif
	fly_send_headers_frame(stream, res);
	e->read_or_write |= FLY_WRITE;
	fly_event_data_set(e, __p, res->request->connect);
	goto register_handler;

send_body:
#ifdef DEBUG
	printf("WORKER: Send Body\n");
#endif
#ifdef DEBUG_SEND_DC
	int fd;
	printf("================  DEBUG SEND DISCONNECTION... ================\n");
	printf("WORKER PID: %d\n", getpid());
	printf("HOST: %s\n", res->request->connect->hostname);
	printf("PORT: %s\n", res->request->connect->servname);

	fd = res->request->connect->c_sockfd;
	/* DEBUG: Disconnection before response */
	shutdown(fd, SHUT_WR);
	printf("==========================================================================\n");
#endif
	/* only send */
	return fly_send_data_frame(e, res);

log:
#ifdef DEBUG
	printf("WORKER: Log HTTP2 Access\n");
#endif
	if (fly_response_log(res, e) == -1)
		return -1;

	/* Release response resources */
	res->fase = FLY_RESPONSE_RELEASE;
	fly_event_data_set(e, __p, res->request->connect);
	/* Release hv2 response/response */
	fly_hv2_remove_response(stream->state, res, e);
	fly_hv2_max_handled_sid(stream);
	fly_hv2_stream_end_request_response(stream);

	if (stream->state->response_count == 0 && \
			stream->state->send_count == 0)
		e->read_or_write &= ~FLY_WRITE;
	if (stream->stream_state == FLY_HV2_STREAM_STATE_CLOSED)
		if (fly_hv2_close_stream(stream) == -1)
			return -1;

#ifdef DEBUG
	printf("WORKER: End HTTP2 request response\n");
#endif
register_handler:
	e->read_or_write |= FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	return fly_event_register(e);

response_413:

	req = res->request;
	fly_hv2_remove_response(stream->state, res, e);
	fly_response_release(res);
	res = fly_413_response(req);
	fly_event_data_set(e, __p, res);
	return fly_hv2_response_event(e);

response_500:

	req = res->request;
	fly_hv2_remove_response(stream->state, res, e);
	fly_response_release(res);
	res = fly_500_response(req);
	fly_event_data_set(e, __p, res);
	return fly_hv2_response_event(e);
}

int fly_header_add_v2(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len, bool beginning)
{
	fly_hdr_c *__c;
	/* whether NULL header value of jtatic/dynamic table entry. */
	bool null_value = false;

	__c = fly_header_addc(chain_info, name, name_len, value, value_len, beginning);
	__c->name_index = false;
	__c->static_table = false;
	__c->dynamic_table = false;
	if (fly_unlikely_null(__c))
		return -1;

	for (size_t j=0; j<chain_info->state->dtable_max_index; j++){
		int name_cmp_res, value_cmp_res;

		if (j<FLY_HV2_STATIC_TABLE_LENGTH){
			name_cmp_res = static_table[j].hname ? strncmp(static_table[j].hname, name, strlen(static_table[j].hname)) : -1;
			value_cmp_res = static_table[j].hvalue ? strncmp(static_table[j].hvalue, value, strlen(static_table[j].hvalue)) : -1;
			if (static_table[j].hvalue == NULL)
				null_value = true;
			else
				null_value = false;
		}else{
			int d_index = j-FLY_HV2_STATIC_TABLE_LENGTH;
			struct fly_hv2_dynamic_table *dt=chain_info->state->dtable->next;

			while(d_index--)
				dt = dt->next;

			name_cmp_res = (dt->hname_len&&dt->hname) ? strncmp(dt->hname, name, dt->hname_len) : -1;
			value_cmp_res = (dt->hvalue_len&&dt->hvalue) ? strncmp(dt->hvalue, value, dt->hvalue_len) : -1;
			if (dt->hvalue == NULL)
				null_value = true;
			else
				null_value = false;
		}

		if (name_cmp_res == 0 && value_cmp_res == 0){
			__c->index = j;
			__c->name_len = strlen(name);
			__c->value_len = strlen(value);
			__c->name_index = false;
			if (j<FLY_HV2_STATIC_TABLE_LENGTH)
				__c->static_table = true;
			else
				__c->dynamic_table = true;
			break;
		}else if (null_value && name_cmp_res == 0){
			__c->index = j;
			__c->name_index = true;
			__c->value_len = strlen(value);
			__c->index_update = INDEX_UPDATE;
			break;
		}

	}
	return 0;
}

static inline void fly_header_hv2_state(fly_hdr_ci *__ci, struct fly_hv2_state *state)
{
	__ci->state = state;
}
