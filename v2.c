#include "v2.h"
#include "request.h"
#include "response.h"
#include "connect.h"
#include <sys/sendfile.h>

fly_hv2_stream_t *fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client);
int fly_hv2_request_event_handler(fly_event_t *e);
fly_hv2_stream_t *fly_hv2_stream_search_from_sid(fly_hv2_state_t *state, fly_sid_t sid);
void fly_send_settings_frame(fly_hv2_stream_t *stream, uint16_t *id, uint32_t *value, size_t count, bool ack);

#define FLY_HV2_SEND_FRAME_SUCCESS			(1)
#define FLY_HV2_SEND_FRAME_BLOCKING			(0)
#define FLY_HV2_SEND_FRAME_ERROR			(-1)
int fly_send_frame_event_handler(fly_event_t *e);
void fly_send_settings_frame_of_server(fly_hv2_stream_t *stream);
int fly_send_frame_event(fly_event_manager_t *manager, struct fly_hv2_send_frame *frame, int read_or_write);
#define __FLY_SEND_FRAME_READING_BLOCKING			(2)
#define __FLY_SEND_FRAME_WRITING_BLOCKING			(3)
#define __FLY_SEND_FRAME_ERROR						(-1)
#define __FLY_SEND_FRAME_SUCCESS					(1)
__fly_static int __fly_send_frame(struct fly_hv2_send_frame *frame);
#define FLY_SEND_FRAME_SUCCESS			(1)
#define FLY_SEND_FRAME_BLOCKING			(0)
#define FLY_SEND_FRAME_ERROR			(-1)
void fly_settings_frame_ack(fly_hv2_stream_t *stream);
void fly_send_frame_add_stream(struct fly_hv2_send_frame *frame, bool ack);
void fly_received_settings_frame_ack(fly_hv2_stream_t *stream);
int fly_hv2_dynamic_table_init(struct fly_hv2_state *state);
int fly_hv2_parse_headers(fly_hv2_stream_t *stream __unused, uint32_t length __unused, uint8_t *payload, fly_buffer_c *__c __unused);
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
void fly_hv2_emergency(fly_hv2_state_t *state);
void fly_hv2_dynamic_table_release(struct fly_hv2_state *state);

static inline uint32_t fly_hv2_length_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	uint32_t length=0;
	if (((fly_buf_p) __fh) + (int) (FLY_HV2_FRAME_HEADER_LENGTH_LEN/FLY_HV2_OCTET_LEN) > __c->lptr){
		length |= (uint32_t) (*__fh)[0] << 16;
		__fh = fly_update_chain(&__c, __fh, 1);
		length |= (uint32_t) (*__fh)[0] << 8;
		__fh = fly_update_chain(&__c, __fh, 1);
		length |= (uint32_t) (*__fh)[0];
		__fh = fly_update_chain(&__c, __fh, 1);
	}else{
		length |= (uint32_t) (*__fh)[0] << 16;
		length |= (uint32_t) (*__fh)[1] << 8;
		length |= (uint32_t) (*__fh)[2];
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

		__fh = fly_update_chain(&__c, __fh, 5);
		__sid |= (uint32_t) (*__fh)[0] & ((1<<7)-1) << 24;
		__fh = fly_update_chain(&__c, __fh, 1);
		__sid |= (uint32_t) (*__fh)[0] << 16;
		__fh = fly_update_chain(&__c, __fh, 1);
		__sid |= (uint32_t) (*__fh)[0] << 8;
		__fh = fly_update_chain(&__c, __fh, 1);
		__sid |= (uint32_t) (*__fh)[0];
	}else{
		/* clear R bit */
		__sid |= ((uint32_t) (*__fh)[5] & ((1<<7)-1)) << 24;
		__sid |= (uint32_t) (*__fh)[6] << 16;
		__sid |= (uint32_t) (*__fh)[7] << 8;
		__sid |= (uint32_t) (*__fh)[8];
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
	state->p_max_frame_size = FLY_HV2_MAX_FRAME_SIZE_DEFAULT;
	state->p_max_header_list_size = FLY_HV2_MAX_HEADER_LIST_SIZE_DEFAULT;
	state->p_enable_push = FLY_HV2_ENABLE_PUSH_DEFAULT;
	state->header_table_size = FLY_HV2_HEADER_TABLE_SIZE_DEFAULT;
	state->max_concurrent_streams = FLY_HV2_MAX_CONCURRENT_STREAMS_DEFAULT;
	state->initial_window_size = FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT;
	state->max_frame_size = FLY_HV2_MAX_FRAME_SIZE_DEFAULT;
	state->max_header_list_size = FLY_HV2_MAX_HEADER_LIST_SIZE_DEFAULT;
	state->enable_push = FLY_HV2_ENABLE_PUSH_DEFAULT;
	state->window_size = FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT;
}

void __fly_hv2_add_stream(fly_hv2_state_t *state, fly_hv2_stream_t *__s);

void __fly_hv2_add_reserved(fly_hv2_state_t *state, fly_hv2_stream_t *__s)
{
	state->lreserved->next = __s;
	state->lreserved= __s;
	state->reserved_count++;
}

int fly_hv2_stream_create_reserved(fly_hv2_state_t *state, fly_sid_t id, bool from_client)
{
	fly_hv2_stream_t *__s;

	__s = fly_hv2_create_stream(state, id, from_client);
	if (fly_unlikely_null(__s))
		return -1;
	__s->reserved = true;

	if (state->reserved_count == 0)
		state->reserved->next = __s;

	__fly_hv2_add_reserved(state, __s);
	return 0;
}

int fly_hv2_create_frame(fly_hv2_stream_t *stream, uint8_t type, uint32_t length, uint8_t flags, void *payload)
{
	__unused fly_hv2_frame_t *frame, *__f;

	frame = fly_pballoc(stream->state->pool, sizeof(struct fly_hv2_frame));
	if (fly_unlikely_null(frame))
		return -1;
	frame->stream = stream;
	frame->type = type;
	frame->length = length;
	frame->flags = flags;
	frame->payload = payload;

	frame->next = stream->frames;
	if (stream->frame_count){
		stream->lframe->next = frame;
	}else{
		stream->frames->next = frame;
	}
	stream->lframe = frame;
	stream->frame_count++;
	return 0;
}

void fly_hv2_release_frame(struct fly_hv2_frame *frame)
{
	fly_pbfree(frame->stream->state->pool, frame);
}

static fly_hv2_stream_t *__fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client)
{
	fly_hv2_stream_t *__s;
	__s = fly_pballoc(state->pool, sizeof(fly_hv2_stream_t));
	if (fly_unlikely_null(__s))
		return NULL;

	__s->id = id;
	__s->dependency_id = FLY_HV2_STREAM_ROOT_ID;
	__s->from_client = from_client ? 1 : 0;
	__s->dep_count = 0;
	__s->next = __s;
	__s->dnext = __s;
	__s->dprev = __s;
	__s->deps = __s;
	__s->stream_state = FLY_HV2_STREAM_STATE_IDLE;
	__s->reserved = false;
	__s->state = state;
	__s->frames = fly_pballoc(state->pool, sizeof(struct fly_hv2_frame));
	if (fly_unlikely_null(__s->frames))
		return NULL;
	__s->frames->next = __s->frames;
	__s->yetsend = fly_pballoc(state->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(__s->yetsend))
		return NULL;
	__s->yetsend->next = __s->yetsend;
	__s->yetsend->prev = __s->yetsend;
	__s->weight = FLY_HV2_STREAM_DEFAULT_WEIGHT;
	__s->lframe = __s->frames;
	__s->lyetsend = __s->yetsend;
	__s->frame_count = 0;
	__s->yetsend_count = 0;
	__s->end_send_headers = false;
	__s->window_size = state->p_initial_window_size;

	__s->yetack = fly_pballoc(state->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(__s->yetack))
		return NULL;
	__s->yetack->next = __s->yetack;
	__s->yetack->prev = __s->yetack;
	__s->lyetack = __s->yetack;
	__s->yetack_count = 0;
	__s->can_response = false;
	__s->end_request_response = false;

	__s->request = fly_request_init(state->connect);
	if (fly_unlikely_null(__s->request))
		return NULL;
	__s->request->stream = __s;

	__s->exclusive = false;
	return __s;
}

void __fly_hv2_add_stream(fly_hv2_state_t *state, fly_hv2_stream_t *__s)
{
	state->lstream->next = __s;
	__s->next = state->streams;

	state->lstream = __s;
	state->stream_count++;
}

void __fly_hv2_remove_stream(fly_hv2_state_t *state, fly_hv2_stream_t *__s)
{
	fly_hv2_stream_t *s, *prev=NULL;

	for (s=state->streams->next; s!=state->streams; s=s->next){
		if (s == __s){
			if (prev){
				if (s == state->lstream)
					state->lstream = prev;
				prev->next = s->next;
			}else{
				if (s == state->lstream)
					state->lstream = state->streams;
				state->streams->next = s;
			}
			state->stream_count--;
			return;
		}
		prev = s;
	}
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
	ns->request->header = fly_header_init();
	if (fly_unlikely_null(ns->request->header))
		return NULL;
	__fly_hv2_add_stream(state, ns);
	state->max_sid = id;

	return ns;
}

void fly_hv2_send_frame_release(struct fly_hv2_send_frame *__f);
int fly_hv2_close_stream(fly_hv2_stream_t *stream)
{
	fly_hv2_state_t *state;

	state = stream->state;

	__fly_hv2_remove_stream(state, stream);

	/* all frames remove */
	if (stream->frame_count){
		struct fly_hv2_frame *__n;
		for (struct fly_hv2_frame *__f=stream->frames->next; __n!=stream->frames;){
			__n = __f->next;
			fly_hv2_release_frame(__f);
		}
	}
	/* all yet send frames remove */
	if (stream->yetsend_count){
		struct fly_hv2_send_frame *__n;
		for (struct fly_hv2_send_frame *__f=stream->yetsend->next; __n!=stream->yetsend;){
			__n = __f->next;
			fly_hv2_send_frame_release(__f);
		}
	}
	/* all yet ack frames remove */
	if (stream->yetack_count){
		struct fly_hv2_send_frame *__n;
		for (struct fly_hv2_send_frame *__f=stream->yetack->next; __n!=stream->yetack;){
			__n = __f->next;
			fly_hv2_send_frame_release(__f);
		}
	}
	/* request release */
	if (!stream->end_request_response && fly_request_release(stream->request) == -1)
		return -1;

	fly_pbfree(state->pool, stream->frames);
	fly_pbfree(state->pool, stream->yetsend);
	fly_pbfree(state->pool, stream->yetack);

	fly_pbfree(state->pool, stream);

	return 0;
}

#define FLY_HV2_INIT_CONNECTION_PREFACE_CONTINUATION	(0)
#define FLY_HV2_INIT_CONNECTION_PREFACE_ERROR			(-1)
#define FLY_HV2_INIT_CONNECTION_PREFACE_SUCCESS			(1)
int fly_hv2_init_connection_preface(fly_connect_t *conn)
{
	fly_buffer_t *buf = conn->buffer;

	switch(fly_buffer_memcmp(FLY_CONNECTION_PREFACE, buf->first_useptr, buf->chain, strlen(FLY_CONNECTION_PREFACE))){
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
 *	@ state->reserved:		struct fly_hv2_stream
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

	state = fly_pballoc(conn->pool, sizeof(fly_hv2_state_t));
	state->pool = conn->pool;
	if (fly_unlikely_null(state))
		goto state_error;

	fly_hv2_default_settings(state);

	state->connect = conn;
	/* create root stream(0x0) */
	state->streams = __fly_hv2_create_stream(state, FLY_HV2_STREAM_ROOT_ID, true);
	if (fly_unlikely_null(state->streams))
		goto streams_error;
	state->stream_count = 1;
	state->reserved = __fly_hv2_create_stream(state, FLY_HV2_STREAM_ROOT_ID, false);
	if (fly_unlikely_null(state->reserved))
		goto reserved_error;
	state->lreserved = state->lreserved;
	state->responses = fly_pballoc(state->pool, sizeof(struct fly_hv2_response));
	if (fly_unlikely_null(state->responses))
		goto responses_error;
	state->responses->next = state->responses;
	state->responses->prev = state->responses;
	state->lresponse = state->responses;
	state->reserved_count = 0;
	state->connection_state = FLY_HV2_CONNECTION_STATE_INIT;
	state->lstream = state->streams;
	state->max_sid = FLY_HV2_STREAM_ROOT_ID;
	state->max_handled_sid = 0;
	state->goaway = false;
	state->response_count = 0;
	if (fly_hv2_dynamic_table_init(state) == -1)
		goto dynamic_table_error;
	state->emergency_ptr = fly_pballoc(state->pool, FLY_HV2_STATE_EMEPTR_MIN);
	if (fly_unlikely_null(state->emergency_ptr))
		goto emergency_error;

	conn->v2_state = state;
	return state;

emergency_error:
	fly_hv2_dynamic_table_release(state);
dynamic_table_error:
	fly_pbfree(conn->pool, state->responses);
responses_error:
	fly_pbfree(conn->pool, state->reserved);
reserved_error:
	fly_pbfree(conn->pool, state->streams);
streams_error:
	fly_pbfree(conn->pool, state);
state_error:
	return NULL;
}

/*
 *	release handler of HTTP2 connection state.
 */
void fly_hv2_dynamic_table_release(struct fly_hv2_state *state);
void fly_hv2_state_release(fly_hv2_state_t *state)
{
	fly_hv2_dynamic_table_release(state);
	fly_pbfree(state->pool, state->responses);
	fly_pbfree(state->pool, state->reserved);
	fly_pbfree(state->pool, state->streams);
	fly_pbfree(state->pool, state->emergency_ptr);
	fly_pbfree(state->pool, state);
}

int fly_hv2_init_handler(fly_event_t *e)
{
	fly_connect_t *conn;
	fly_buffer_t *buf;

	conn = (fly_connect_t *) e->event_data;
	buf = conn->buffer;

	if (!conn->v2_state){
		if (fly_unlikely_null(fly_hv2_state_init(conn)))
			goto error;
	}

	switch(fly_request_receive(conn->c_sockfd, conn)){
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
	default:
		FLY_NOT_COME_HERE
	}

connection_preface:
	/* invalid connection preface */
	switch (fly_hv2_init_connection_preface(conn)){
	case FLY_HV2_INIT_CONNECTION_PREFACE_CONTINUATION:
		goto read_continuation;
	case FLY_HV2_INIT_CONNECTION_PREFACE_ERROR:
		goto error;
	case FLY_HV2_INIT_CONNECTION_PREFACE_SUCCESS:
		break;
	default:
		FLY_NOT_COME_HERE
	}

	/* release connection preface from buffer */
	fly_buffer_chain_release_from_length(buf->first_chain, strlen(FLY_CONNECTION_PREFACE));

	conn->v2_state->connection_state = FLY_HV2_CONNECTION_STATE_CONNECTION_PREFACE;
	return fly_hv2_request_event_handler(e);

write_continuation:
	e->read_or_write = FLY_WRITE;
	goto continuation;
read_continuation:
	e->read_or_write = FLY_READ;
	goto continuation;
continuation:
	if (conn->buffer->use_len > strlen(FLY_CONNECTION_PREFACE))
		goto connection_preface;
	e->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	e->flag = FLY_MODIFY;
	FLY_EVENT_HANDLER(e, fly_hv2_init_handler);
	e->tflag = FLY_INHERIT;
	e->available = false;
	fly_event_socket(e);
	return fly_event_register(e);

disconnect:
error:
	fly_hv2_state_release(conn->v2_state);
	fly_connect_release(conn);

	e->tflag = 0;
	e->flag = FLY_CLOSE_EV;
	return fly_event_unregister(e);
}

void fly_hv2_send_frame_release(struct fly_hv2_send_frame *__f)
{
	if (__f->payload)
		fly_pbfree(__f->pool, __f->payload);
	fly_pbfree(__f->pool, __f);
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
	return **pl & 0x1 ? true : false;
}

static inline uint64_t fly_opaque_data(uint8_t **pl, fly_buffer_c **__c)
{
	uint64_t opaque_data=0;

	if ((fly_buf_p) (*pl+8) > (*__c)->lptr){
		opaque_data |= (uint64_t) (*pl)[0] << 56;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 48;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 40;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 32;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 24;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 16;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 8;
		*pl = fly_update_chain(__c, *pl, 1);
		opaque_data |= (uint64_t) (*pl)[0] << 0;
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		opaque_data |= (uint64_t) (*pl)[0] << 56;
		opaque_data |= (uint64_t) (*pl)[1] << 48;
		opaque_data |= (uint64_t) (*pl)[2] << 40;
		opaque_data |= (uint64_t) (*pl)[3] << 32;
		opaque_data |= (uint64_t) (*pl)[4] << 24;
		opaque_data |= (uint64_t) (*pl)[5] << 16;
		opaque_data |= (uint64_t) (*pl)[6] << 8;
		opaque_data |= (uint64_t) (*pl)[7] << 0;

		*pl += (int) (FLY_HV2_FRAME_TYPE_PING_OPEQUE_DATA_LEN)/FLY_HV2_OCTET_LEN;
	}
	return opaque_data;
}

static inline uint32_t __fly_hv2_32bit(uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t uint31 = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		uint31 |= (uint32_t) (*pl)[0] << 24;
		*pl = fly_update_chain(__c, *pl, 1);
		uint31 |= (uint32_t) (*pl)[0] << 16;
		*pl = fly_update_chain(__c, *pl, 1);
		uint31 |= (uint32_t) (*pl)[0] << 8;
		*pl = fly_update_chain(__c, *pl, 1);
		uint31 |= (uint32_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		uint31 |= (uint32_t) (*pl)[0] << 24;
		uint31 |= (uint32_t) (*pl)[1] << 16;
		uint31 |= (uint32_t) (*pl)[2] << 8;
		uint31 |= (uint32_t) (*pl)[3];

		*pl += (int) (FLY_HV2_FRAME_TYPE_PRIORITY_E_LEN+FLY_HV2_FRAME_TYPE_PRIORITY_STREAM_DEPENDENCY_LEN)/FLY_HV2_OCTET_LEN;
	}
	return uint31;
}

static inline uint32_t __fly_hv2_31bit(uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t uint31 = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		uint31 |= ((uint32_t) (*pl)[0] & ((1<<8)-1)) << 24;
		*pl = fly_update_chain(__c, *pl, 1);
		uint31 |= (uint32_t) (*pl)[0] << 16;
		*pl = fly_update_chain(__c, *pl, 1);
		uint31 |= (uint32_t) (*pl)[0] << 8;
		*pl = fly_update_chain(__c, *pl, 1);
		uint31 |= (uint32_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		uint31 |= ((uint32_t) (*pl)[0] & ((1<<8)-1)) << 24;
		uint31 |= (uint32_t) (*pl)[1] << 16;
		uint31 |= (uint32_t) (*pl)[2] << 8;
		uint31 |= (uint32_t) (*pl)[3];

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
	__p = (dsid == FLY_HV2_STREAM_ROOT_ID) ? __s->state->streams : fly_hv2_stream_search_from_sid(__s->state, dsid);

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
		id |= (*pl)[0] << 8;
		*pl = fly_update_chain(__c, *pl, 1);
		id |= (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		id |= (*pl)[0] << 8;
		id |= (*pl)[1];
		*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_ID_LEN/FLY_HV2_OCTET_LEN);
	}
	return id;
}

static inline uint16_t fly_hv2_settings_id_nb(uint8_t **pl)
{
	uint16_t id = 0;
	id |= (*pl)[0] << 8;
	id |= (*pl)[1];
	*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_ID_LEN/FLY_HV2_OCTET_LEN);
	return id;
}

static inline uint32_t fly_hv2_settings_value(__unused uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t value = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		value |= (*pl)[0] << 24;
		*pl = fly_update_chain(__c, *pl, 1);
		value |= (*pl)[0] << 16;
		*pl = fly_update_chain(__c, *pl, 1);
		value |= (*pl)[0] << 8;
		*pl = fly_update_chain(__c, *pl, 1);
		value |= (*pl)[0] << 0;
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		value |= (*pl)[0] << 24;
		value |= (*pl)[1] << 16;
		value |= (*pl)[2] << 8;
		value |= (*pl)[3] << 0;
		*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_VALUE_LEN/FLY_HV2_OCTET_LEN);
	}
	return value;
}

static inline uint16_t fly_hv2_settings_value_nb(uint8_t **pl)
{
	uint32_t value = 0;
	value |= (*pl)[0] << 24;
	value |= (*pl)[1] << 16;
	value |= (*pl)[2] << 8;
	value |= (*pl)[3] << 0;
	*pl += (int) (FLY_HV2_FRAME_TYPE_SETTINGS_VALUE_LEN/FLY_HV2_OCTET_LEN);
	return value;
}

#define FLY_HV2_SETTINGS_SUCCESS				(0)
int fly_hv2_peer_settings(fly_hv2_state_t *state, __unused fly_sid_t sid, uint8_t *payload, uint32_t len, fly_buffer_c *__c)
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

	frame = fly_pballoc(state->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(frame))
		return -1;

	/* root stream(0x0) */
	frame->stream = state->streams;
	frame->pool = state->pool;
	frame->sid = FLY_HV2_STREAM_ROOT_ID;
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_GOAWAY;
	frame->payload_len = \
		(int) (FLY_HV2_FRAME_TYPE_PING_OPEQUE_DATA_LEN)/FLY_HV2_OCTET_LEN;
	frame->payload = fly_pballoc(frame->pool, frame->payload_len);
	if (fly_unlikely_null(frame->payload))
		return -1;

	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, 0, false, frame->sid);

	/* opeque data setting */
	uint8_t *ptr = frame->payload;
	*ptr++ = *(((uint8_t *) &opaque_data)+7);
	*ptr++ = *(((uint8_t *) &opaque_data)+6);
	*ptr++ = *(((uint8_t *) &opaque_data)+5);
	*ptr++ = *(((uint8_t *) &opaque_data)+4);
	*ptr++ = *(((uint8_t *) &opaque_data)+3);
	*ptr++ = *(((uint8_t *) &opaque_data)+2);
	*ptr++ = *(((uint8_t *) &opaque_data)+1);
	*ptr   = *(((uint8_t *) &opaque_data)+0);
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
	*ptr++ |= *(lsid+3);
	*ptr++ = *(lsid+2);
	*ptr++ = *(lsid+1);
	*ptr++ = *(lsid+0);
	*ptr++ = *(ecode+3);
	*ptr++ = *(ecode+2);
	*ptr++ = *(ecode+1);
	*ptr   = *(ecode+0);
	state->goaway = true;
}

int fly_send_goaway_frame(fly_hv2_state_t *state, bool r, uint32_t error_code)
{
	struct fly_hv2_send_frame *frame;

	frame = fly_pballoc(state->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(frame))
		return -1;

	/* root stream(0x0) */
	frame->stream = state->streams;
	frame->pool = state->pool;
	frame->sid = FLY_HV2_STREAM_ROOT_ID;
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

int fly_send_rst_stream_frame(fly_hv2_stream_t *stream, uint32_t error_code)
{
	struct fly_hv2_send_frame *frame;

	frame = fly_pballoc(stream->state->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(frame))
		return -1;

	/* root stream(0x0) */
	frame->stream = stream;
	frame->pool = stream->state->pool;
	frame->sid = stream->id;
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

	*ptr++ = *(ecode+3);
	*ptr++ = *(ecode+2);
	*ptr++ = *(ecode+1);
	*ptr++ = *(ecode+0);
	__fly_hv2_add_yet_send_frame(frame);

	stream->stream_state = FLY_HV2_STREAM_STATE_CLOSED;

	return 0;
}

int fly_send_goaway_frame(fly_hv2_state_t *state, bool r, uint32_t error_code);
int fly_hv2_response_event_handler(fly_event_t *e, fly_hv2_stream_t *stream);

int fly_hv2_close_handle(fly_event_t *e, fly_hv2_state_t *state)
{
	/* all frame/stream/request release */
	fly_hv2_stream_t *__s;
	fly_connect_t *conn;
	for (__s=state->streams->next; __s!=state->streams; __s=__s->next){
		if (fly_hv2_close_stream(__s) == -1)
			return -1;
	}

	conn = state->connect;
	/* state release */
	fly_hv2_state_release(state);
	/* connect release */
	if (fly_connect_release(conn) == -1)
		return -1;

	e->tflag = 0;
	e->flag = FLY_CLOSE_EV;
	return fly_event_unregister(e);
}

int fly_send_frame(fly_event_t *e, fly_hv2_stream_t *stream);
int fly_hv2_goaway_handle(fly_event_t *e, fly_hv2_state_t *state)
{
	if (state->goaway_lsid<state->max_handled_sid)
		goto close_connection;

	fly_hv2_stream_t *__s;
	for (__s=state->streams->next; __s!=state->streams; __s=__s->next){
		if (!__s->peer_end_headers || (!__s->can_response && !__s->yetsend_count))
			if (fly_hv2_close_stream(__s) == -1)
				return -1;

		if (__s->yetsend_count)
			return fly_send_frame(e, __s);

		/* can response */
		return fly_hv2_response_event_handler(e, __s);
	}
close_connection:
	/* close connection */
	return fly_hv2_close_handle(e, state);
}

int fly_hv2_goaway(fly_hv2_state_t *state, bool r, uint32_t last_stream_id, uint32_t error_code)
{
	__unused uint32_t max_handled_sid;
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
		return state->streams;
	for (__s=state->streams->next; __s!=state->streams; __s=__s->next){
		if (__s->id == sid)
			return __s;
	}
	/* not found */
	return NULL;
}

int fly_hv2_request_event_blocking_handler(fly_event_t *e)
{
	fly_connect_t *conn;

	conn = (fly_connect_t *) e->event_data;

	/* receive from peer */
	switch(fly_request_receive(conn->c_sockfd, conn)){
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
	default:
		FLY_NOT_COME_HERE
	}

	return fly_hv2_request_event_handler(e);

write_continuation:
	e->read_or_write |= FLY_WRITE;
	goto continuation;
read_continuation:
	e->read_or_write |= FLY_READ;
	goto continuation;
continuation:
	e->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	e->flag = FLY_MODIFY;
	e->event_data = conn;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->tflag = FLY_INHERIT;
	e->available = false;
	fly_event_socket(e);
	return fly_event_register(e);

disconnect:
	return fly_hv2_request_event_handler(e);
}

int fly_hv2_request_event_blocking(fly_event_t *e, fly_connect_t *conn)
{

	e->read_or_write |= FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->event_data = (void *) conn;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_blocking_handler);

	return fly_event_register(e);
}

static inline bool fly_hv2_settings_frame_ack_from_flags(uint8_t flags)
{
	return flags&FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK ? true : false;
}

int fly_hv2_parse_data(fly_hv2_stream_t *stream, uint32_t length, uint8_t *payload, fly_buffer_c *__c);
int fly_send_frame(fly_event_t *e, fly_hv2_stream_t *stream);
int fly_hv2_responses(fly_event_t *e, fly_hv2_state_t *state __unused);
int fly_hv2_response_event_handler(fly_event_t *e, fly_hv2_stream_t *stream);

int fly_hv2_request_event_handler(fly_event_t *event)
{
	/* event expired(idle timeout) */
	if (event->expired){
		fly_connect_t *conn;
		conn = (fly_connect_t *) event->event_data;
		return fly_hv2_close_handle(event, conn->v2_state);
	}

	do{
		fly_connect_t *conn;
		fly_buf_p *bufp;
		fly_buffer_c *bufc, *plbufc;
		fly_hv2_state_t *state;

		conn = (fly_connect_t *) event->event_data;
		state = conn->v2_state;
		if (state->goaway)
			goto goaway;
		if (conn->peer_closed)
			goto closed;

		if ((event->available&FLY_READ) && FLY_HV2_FRAME_HEADER_LENGTH<=conn->buffer->use_len)
			goto frame_header_parse;
		else if ((event->available&FLY_WRITE) && \
				state->response_count){
			return fly_hv2_responses(event, state);
		}else
			goto blocking;

frame_header_parse:
		bufp = conn->buffer->first_useptr;
		bufc = conn->buffer->first_chain;

		/* if frrme length more than limits, must send FRAME_SIZE_ERROR */
		fly_hv2_stream_t *__stream;
		fly_hv2_frame_header_t *__fh = (fly_hv2_frame_header_t *) bufp;
		uint32_t length = fly_hv2_length_from_frame_header(__fh, bufc);
		uint8_t type = fly_hv2_type_from_frame_header(__fh, bufc);
		uint8_t flags = fly_hv2_flags_from_frame_header(__fh, bufc);
		__unused bool r = fly_hv2_r_from_frame_header(__fh, bufc);
		uint32_t sid = fly_hv2_sid_from_frame_header(__fh, bufc);

		plbufc = bufc;
		uint8_t *pl = fly_hv2_frame_payload_from_frame_header(__fh, &plbufc);
		if (length > state->max_frame_size){
			fly_hv2_send_frame_size_error(__stream, FLY_HV2_CONNECTION_ERROR);
		} else if ((FLY_HV2_FRAME_HEADER_LENGTH+length) > conn->buffer->use_len)
			goto blocking;

		__stream = fly_hv2_stream_search_from_sid(state, sid);
		/* new stream */
		if (!__stream){
			__stream = fly_hv2_create_stream(state, sid, true);
			if (!__stream)
				continue;
		}

		if (sid!=FLY_HV2_STREAM_ROOT_ID && fly_hv2_create_frame(__stream, type, length, flags, pl) == -1)
			goto emergency;

		switch(state->connection_state){
		case FLY_HV2_CONNECTION_STATE_INIT:
		case FLY_HV2_CONNECTION_STATE_END:
			/* error */
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
		}

		/* receive message from peer. extension */
		if (fly_timeout_restart(event) == -1)
			return -1;

		switch(type){
		case FLY_HV2_FRAME_TYPE_DATA:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_LOCAL)){
				fly_hv2_send_stream_closed(__stream, FLY_HV2_STREAM_ERROR);
			}

			{
				__unused uint8_t pad_length;

				if (flags & FLY_HV2_FRAME_TYPE_DATA_PADDED){
					pad_length = fly_hv2_pad_length(&pl, &plbufc);
					length -= (int) (FLY_HV2_FRAME_TYPE_DATA_PAD_LENGTH_LEN/FLY_HV2_OCTET_LEN);
				}

				fly_hv2_parse_data(__stream, length, pl, plbufc);

				if (flags & FLY_HV2_FRAME_TYPE_DATA_END_STREAM)
					__stream->stream_state = FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE;

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
				__unused uint32_t hlen=length;
				__unused uint8_t pad_length;
				__unused bool e;
				__unused uint32_t stream_dependency;
				__unused uint8_t weight;

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

				fly_hv2_parse_headers(__stream, hlen, pl, plbufc);

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
				}

				/* send setting frame of server */
				fly_send_settings_frame_of_server(__stream);

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
				__unused bool r;
				__unused uint32_t last_stream_id;
				__unused uint32_t error_code;

				r = fly_hv2_flag(&pl);
				last_stream_id = fly_hv2_last_sid(&pl, &plbufc);
				error_code = fly_hv2_error_code(&pl, &plbufc);

				state->goaway = true;

				/* send GOAWAY frame to receiver and connection close */
				if (fly_hv2_goaway(state, r, last_stream_id, error_code) == -1)
					goto emergency;
			}

			break;
		case FLY_HV2_FRAME_TYPE_WINDOW_UPDATE:
			if (length != FLY_HV2_FRAME_TYPE_WINDOW_UPDATE_LENGTH)
				fly_hv2_send_frame_size_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			{
				__unused bool r;
				uint32_t window_size;

				r = fly_hv2_flag(&pl);
				window_size = fly_hv2_window_size_increment(&pl, &plbufc);
				if (window_size == 0)
					fly_hv2_send_protocol_error(__stream, FLY_HV2_STREAM_ERROR);

				if (sid == 0)
					state->window_size += (ssize_t) window_size;
				else
					__stream->window_size += (ssize_t) window_size;
			}
			break;
		case FLY_HV2_FRAME_TYPE_CONTINUATION:
			if (sid==FLY_HV2_STREAM_ROOT_ID)
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);

			if (!((__stream->lframe->type == FLY_HV2_FRAME_TYPE_HEADERS && !(__stream->lframe->flags&FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS)) || (__stream->lframe->type == FLY_HV2_FRAME_TYPE_PUSH_PROMISE && !(__stream->lframe->flags&FLY_HV2_FRAME_TYPE_PUSH_PROMISE_END_HEADERS)) || (__stream->lframe->type == FLY_HV2_FRAME_TYPE_CONTINUATION && !(__stream->lframe->flags&FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS)))){
				fly_hv2_send_protocol_error(FLY_HV2_ROOT_STREAM(state), FLY_HV2_CONNECTION_ERROR);
			}

			{
				fly_hv2_parse_headers(__stream, length, pl, plbufc);
				if (flags & FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS)
					__stream->peer_end_headers = true;

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
		fly_buffer_chain_release_from_length(bufc, FLY_HV2_FRAME_HEADER_LENGTH+length);
		bufc = bufc->buffer->first_chain;
		if (bufc->buffer->use_len == 0)
			fly_buffer_chain_refresh(bufc);

		/* Is there next frame header in buffer? */
		if (conn->buffer->use_len >= FLY_HV2_FRAME_HEADER_LENGTH)
			continue;
		else
			/* blocking event */
			goto blocking;

		/*
		 *	some frames have not been sent yet
		 */
		if (__stream->yetsend_count)
			return fly_send_frame(event, __stream);

		/*
		 *	closed stream (release stream resource)
		 */
		if (__stream->stream_state == FLY_HV2_STREAM_STATE_CLOSED)
			return fly_hv2_close_stream(__stream);

blocking:
		return fly_hv2_request_event_blocking(event, conn);
goaway:
		return fly_hv2_goaway_handle(event, state);
closed:
		return fly_hv2_close_handle(event, state);
emergency:
		fly_hv2_emergency(state);
		continue;

	} while (true);

	FLY_NOT_COME_HERE
}

void fly_fh_setting(fly_hv2_frame_header_t *__fh, uint32_t length, uint8_t type, uint8_t flags, bool r, uint32_t sid)
{
	*(((uint8_t *) (__fh))) = (uint8_t) (length >> 16);
	*(((uint8_t *) (__fh))+1) = (uint8_t) (length >> 8);
	*(((uint8_t *) (__fh))+2) = (uint8_t) (length >> 0);
	*(((uint8_t *) (__fh))+3) = type;
	*(((uint8_t *) (__fh))+4) = flags;
	if (r)
		*(((uint8_t *) (__fh))+5) |= 0x1; else *(((uint8_t *) (__fh))+5) |= 0x0; *(((uint8_t *) (__fh))+5) = (uint8_t) (sid >> 24);
	*(((uint8_t *) (__fh))+6) = (uint8_t) (sid >> 16);
	*(((uint8_t *) (__fh))+7) = (uint8_t) (sid >> 8);
	*(((uint8_t *) (__fh))+8) = (uint8_t) (sid);
}

void fly_settings_frame_payload_set(uint8_t *pl, uint16_t *ids, uint32_t *values, size_t count)
{
	size_t __n=0;

	while(__n<count){
		uint16_t id = ids[__n];
		uint32_t value = values[__n];

		/* identifier */
		*(((uint8_t *) (pl))+0)	= (uint8_t) (id >> 8);
		*(((uint8_t *) (pl))+1)	= (uint8_t) (id >> 0);
		/* value */
		*(((uint8_t *) (pl))+2)	= (uint8_t) (value >> 24);
		*(((uint8_t *) (pl))+3)	= (uint8_t) (value >> 16);
		*(((uint8_t *) (pl))+4) = (uint8_t) (value >> 8);
		*(((uint8_t *) (pl))+5) = (uint8_t) (value >> 0);

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

void fly_hv2_set_integer(uint32_t integer, uint8_t **pl, fly_buffer_c **__c, __unused uint32_t *update, uint8_t prefix_bit);

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
	fly_buffer_c *chain = buf->lchain;
	uint8_t *ptr = buf->lunuse_ptr;
	uint32_t total = 0;
	/* static index  */
	if (c->static_table){
		__fly_hv2_set_index_bit(ptr);
		fly_hv2_set_integer(FLY_HEADERS_INDEX(c->index), &ptr, &chain, &total, FLY_HV2_INDEX_PREFIX_BIT);
	/* static/dynamic index name, no index value  */
	}else if (c->name_index){
		size_t value_len;
		__unused char *value;
		/* set name index */
		fly_hv2_set_index(FLY_HEADERS_INDEX(c->index), c->index_update, &ptr, &chain, &total);
		/* set value length */
		if (c->huffman_value){
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
			ptr = (uint8_t *) buf->lunuse_ptr;
			total++;
		}
	/* no index name and value  */
	}else{
		size_t value_len, name_len;
		char *value, *name;
		/* set no index */
		fly_hv2_set_index(0, c->index_update, &ptr, &chain, &total);
		/* set name length */
		if (c->huffman_name){
			__fly_hv2_set_huffman_bit(buf->lunuse_ptr, true);
			fly_hv2_set_integer(c->hname_len, &ptr, &chain, &total, FLY_HV2_NAME_PREFIX_BIT);
			name_len = c->hname_len;
			name = c->hen_name;
		}else{
			__fly_hv2_set_huffman_bit(buf->lunuse_ptr, false);
			fly_hv2_set_integer(c->name_len, &ptr, &chain, &total, FLY_HV2_NAME_PREFIX_BIT);
			name_len = c->name_len;
			name = c->name;
		}
		/* set name */
		while(name_len--){
			*(char *) ptr = *name++;
			fly_update_buffer(buf, 1);
			ptr = (uint8_t *) buf->lunuse_ptr;
		}
		/* set value length */
		if (c->huffman_value){
			__fly_hv2_set_huffman_bit(buf->lunuse_ptr, true);
			fly_hv2_set_integer(c->hvalue_len, &ptr, &chain, &total, FLY_HV2_VALUE_PREFIX_BIT);
			value_len = c->hvalue_len;
			value = c->hen_value;
		}else{
			__fly_hv2_set_huffman_bit(buf->lunuse_ptr, false);
			fly_hv2_set_integer(c->value_len, &ptr, &chain, &total, FLY_HV2_VALUE_PREFIX_BIT);
			value_len = c->value_len;
			value = c->value;
		}
		/* set value */
		while(value_len--){
			*(char *) ptr = *value++;
			fly_update_buffer(buf, 1);
			ptr = (uint8_t *) buf->lunuse_ptr;
			total++;
		}
	}
	return total;
}

int fly_send_frame_h_event_handler(fly_event_t *e);
int fly_send_frame_h_event(fly_event_manager_t *manager, fly_hv2_stream_t *stream, int read_or_write)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	e->fd = stream->request->connect->c_sockfd;
	e->read_or_write = read_or_write;
	e->event_data = (void *) stream;
	FLY_EVENT_HANDLER(e, fly_send_frame_h_event_handler);
	e->flag = 0;
	e->tflag = 0;
	e->eflag = 0;
	fly_sec(&e->timeout, FLY_SEND_FRAME_TIMEOUT);
	fly_event_socket(e);

	return fly_event_register(e);
}

struct __fly_frame_header_data{
	fly_buffer_t *buf;
	size_t total;
	fly_hdr_c *header;
	fly_pool_t *pool;
};

int __fly_send_frame_h(fly_event_t *e, fly_response_t *res);
void __fly_hv2_remove_yet_send_frame(struct fly_hv2_send_frame *frame);

int fly_send_frame_h_event_handler(fly_event_t *e)
{
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	return __fly_send_frame_h(e, res);
}

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

	res = (fly_response_t *) e->event_data;
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
			numsend = send(c_sockfd, fh, FLY_HV2_FRAME_HEADER_LENGTH, 0);
			if (FLY_BLOCKING(numsend))
				goto write_blocking;
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

	e->read_or_write = FLY_READ|FLY_WRITE;
	e->event_data = (void *) res;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_send_data_frame_handler);
	return fly_event_register(e);
read_blocking:
	e->read_or_write = FLY_READ;
	goto blocking;
write_blocking:
	e->read_or_write = FLY_WRITE;
	goto blocking;
blocking:
	res->datai = flag;
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, __fly_send_data_fh_event_handler);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	return fly_event_register(e);
}

int fly_hv2_response_event(fly_event_t *e);
int fly_send_data_frame_handler(fly_event_t *e)
{
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;

	return fly_send_data_frame(e, res);
}

static inline void fly_hv2_window_size_update(fly_hv2_stream_t *stream, ssize_t send_size)
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
	if (stream->window_size <= 0 || state->window_size <= 0)
		goto cant_send;

	max_can_send = stream->window_size > state->window_size ? state->window_size : stream->window_size;
	switch (res->type){
	case FLY_RESPONSE_TYPE_ENCODED:
		send_len = (size_t) res->de->contlen > max_can_send ? max_can_send : (size_t) res->de->contlen;
		if ((size_t) res->de->contlen < max_can_send)
			flag |= FLY_HV2_FRAME_TYPE_DATA_END_STREAM;
		break;
	case FLY_RESPONSE_TYPE_BODY:
		send_len = (size_t) res->body->body_len > max_can_send ? max_can_send : (size_t) res->body->body_len;
		if ((size_t) res->body->body_len < max_can_send)
			flag |= FLY_HV2_FRAME_TYPE_DATA_END_STREAM;
		break;
	case FLY_RESPONSE_TYPE_PATH_FILE:
		send_len = (size_t) res->pf->fs.st_size > max_can_send ? max_can_send : (size_t) res->pf->fs.st_size;
		if ((size_t) res->pf->fs.st_size < max_can_send)
			flag |= FLY_HV2_FRAME_TYPE_DATA_END_STREAM;
		break;
	case FLY_RESPONSE_TYPE_DEFAULT:
		ssize_t file_size = fly_file_size(res->rcbs->content_path);
		if (file_size == -1)
			return -1;

		send_len = (size_t) file_size > max_can_send ? max_can_send : (size_t) file_size;
		if ((size_t) file_size < max_can_send)
			flag |= FLY_HV2_FRAME_TYPE_DATA_END_STREAM;
		break;
	default:
		FLY_NOT_COME_HERE
	}

	res->send_len = send_len;
	return __fly_send_data_fh(e, res, send_len, stream->id, flag);

send:
#define FLY_SENDLEN_UNTIL_CHAIN_LPTR(__c , __p)		\
		(((__c) != (__c)->buffer->lchain) ? \
	((void *) (__c)->unuse_ptr - (void *) (__p) + 1) : \
	((void *) (__c)->unuse_ptr - (void *) (__p)))

	size_t total = res->byte_from_start ? res->byte_from_start : 0;
	ssize_t numsend;
	send_len = res->send_len;
	fly_buffer_c *chain;
	fly_buf_p *send_ptr;

	switch(res->type){
	case FLY_RESPONSE_TYPE_ENCODED:
		{
			fly_de_t *de;
			de = res->de;

			chain = de->encbuf->first_chain;
			send_ptr = de->encbuf->first_ptr;
			numsend = 0;
			while(true){
				send_ptr = fly_update_chain(&chain, send_ptr, numsend);
				if (FLY_CONNECT_ON_SSL(res->request->connect)){
					SSL *ssl = res->request->connect->ssl;
					numsend = SSL_write(ssl, send_ptr, FLY_SENDLEN_UNTIL_CHAIN_LPTR(chain, send_ptr));

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
					numsend = send(c_sockfd, send_ptr, FLY_SENDLEN_UNTIL_CHAIN_LPTR(chain, send_ptr), 0);
					if (FLY_BLOCKING(numsend))
						goto write_blocking;
					else if (numsend == -1)
						return FLY_SEND_DATA_FH_ERROR;
				}
				res->byte_from_start += numsend;
				total += numsend;
				fly_hv2_window_size_update(stream, numsend);

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
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
				}else{
					numsend = send(e->fd, body->body+total, body->body_len-total, 0);
					if (FLY_BLOCKING(numsend)){
						goto write_blocking;
					}else if (numsend == -1)
						return FLY_RESPONSE_ERROR;
				}
				total += numsend;
				res->byte_from_start += numsend;

				fly_hv2_window_size_update(stream, numsend);
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
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
				}else{
					numsend = sendfile(c_sockfd, pf->fd, offset, res->count-total);
					if (FLY_BLOCKING(numsend)){
						goto write_blocking;
					}else if (numsend == -1)
						return FLY_SEND_DATA_FH_ERROR;
				}

				total += numsend;
				res->byte_from_start += numsend;

				fly_hv2_window_size_update(stream, numsend);
			}
		}
		break;
	case FLY_RESPONSE_TYPE_DEFAULT:
		{
			fly_rcbs_t *__r=res->rcbs;
			off_t *offset = &res->offset;
			struct stat sb;

			if (fstat(__r->fd, &sb) == -1)
				return FLY_SEND_DATA_FH_ERROR;
			if (sb.st_size == 0)
				goto success;

			while(total < res->count){
				if (FLY_CONNECT_ON_SSL(res->request->connect)){
					SSL *ssl=res->request->connect->ssl;
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
						return FLY_SEND_DATA_FH_ERROR;
					case SSL_ERROR_SSL:
						return FLY_SEND_DATA_FH_ERROR;
					default:
						/* unknown error */
						return FLY_SEND_DATA_FH_ERROR;
					}
					*offset += numsend;
				}else{
					numsend = sendfile(e->fd, __r->fd, offset, res->count-total);
					if (FLY_BLOCKING(numsend)){
						goto write_blocking;
					}else if (numsend == -1)
						return FLY_SEND_DATA_FH_ERROR;
				}

				fly_hv2_window_size_update(stream, numsend);
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
	if (stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE && \
			flag & FLY_HV2_FRAME_TYPE_DATA_END_STREAM)
		stream->stream_state = FLY_HV2_STREAM_STATE_CLOSED;
	stream->end_send_headers = true;
	stream->end_send_data = true;

	return fly_hv2_response_blocking_event(e, stream);

cant_send:
	return fly_hv2_response_blocking_event(e, stream);
read_blocking:
	return fly_hv2_response_blocking_event(e, stream);
write_blocking:
	return fly_hv2_response_blocking_event(e, stream);
}

int fly_send_frame(fly_event_t *e, fly_hv2_stream_t *stream)
{
	struct fly_hv2_send_frame *__s;
	for (__s=stream->yetsend->next; __s!=stream->yetsend; __s=__s->next){
		switch(__fly_send_frame(__s)){
		case __FLY_SEND_FRAME_READING_BLOCKING:
			goto read_blocking;
		case __FLY_SEND_FRAME_WRITING_BLOCKING:
			goto write_blocking;
		case __FLY_SEND_FRAME_ERROR:
			return FLY_SEND_FRAME_ERROR;
		case __FLY_SEND_FRAME_SUCCESS:
			break;
		}
		/* end of sending */
		/* release resources */
		fly_pbfree(__s->pool, __s->payload);
		fly_pbfree(__s->pool, __s);
		stream->yetsend_count--;
		__fly_hv2_remove_yet_send_frame(__s);
	}

	goto success;

success:
	e->read_or_write = FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	e->event_data = (void *) stream->state->connect;
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
	e->event_data = (void *) stream->state->connect;
	fly_event_socket(e);
	return fly_event_register(e);
}

int __fly_send_frame_h(fly_event_t *e, fly_response_t *res)
{
	fly_hv2_stream_t *stream;
	struct fly_hv2_send_frame *__s;

	stream = res->request->stream;
	if (stream->yetsend_count == 0)
		goto success;

	stream->end_send_headers = false;
	for (__s=stream->yetsend->next; __s!=stream->yetsend; __s=__s->next){
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
		}
		/* end of sending */
		/* release resources */
		fly_pbfree(__s->pool, __s->payload);
		fly_pbfree(__s->pool, __s);
		stream->yetsend_count--;
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

	frame = fly_pballoc(pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(frame))
		return NULL;

	frame->stream = stream;
	frame->pool = pool;
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->sid = stream->id;
	frame->send_len = 0;
	if (over){
		frame->type = FLY_HV2_FRAME_TYPE_CONTINUATION;
	}else{
		frame->type = FLY_HV2_FRAME_TYPE_HEADERS;
	}
	frame->payload_len = total;
	frame->payload = fly_pballoc(pool, sizeof(uint8_t)*frame->payload_len);
	fly_buffer_memcpy((char *) frame->payload, buf->first_useptr, buf->first_chain, total);
	fly_fh_setting(&frame->frame_header, frame->payload_len, frame->type, flag, false, stream->id);

	return frame;
}

void __fly_hv2_remove_yet_send_frame(struct fly_hv2_send_frame *frame)
{
	fly_hv2_stream_t *stream;

	stream = frame->stream;

	frame->prev->next = frame->next;
	frame->next->prev = frame->prev;

	if (frame == stream->lyetsend)
		stream->lyetsend = frame->prev;
	stream->yetsend_count--;
}

void __fly_hv2_add_yet_send_frame(struct fly_hv2_send_frame *frame)
{
	fly_hv2_stream_t *stream;

	stream = frame->stream;

	frame->next = stream->yetsend;
	frame->prev = stream->lyetsend;

	if (stream->yetsend_count == 0){
		stream->yetsend->next = frame;
	}else{
		stream->lyetsend->next = frame;
	}

	stream->lyetsend = frame;
	stream->yetsend->prev = frame;
	stream->yetsend_count++;
}

int fly_send_headers_frame(fly_hv2_stream_t *stream, fly_pool_t *pool, fly_event_t *e __unused, fly_hdr_ci *ci, bool exist_content)
{
#define FLY_SEND_HEADERS_FRAME_BUFFER_INIT_LEN		1
#define FLY_SEND_HEADERS_FRAME_BUFFER_CHAIN_MAX		100
#define FLY_SEND_HEADERS_FRAME_BUFFER_PER_LEN		10
	struct fly_hv2_send_frame *__f;
	__unused size_t max_payload;
	size_t total;
	bool over=false;
	int flag = 0;
	fly_buffer_t *buf;
	fly_hdr_c *__h=ci->dummy->next;

	max_payload = stream->state->max_frame_size;
	buf = fly_buffer_init(pool, FLY_SEND_HEADERS_FRAME_BUFFER_INIT_LEN, FLY_SEND_HEADERS_FRAME_BUFFER_CHAIN_MAX, FLY_SEND_HEADERS_FRAME_BUFFER_PER_LEN);
	if (fly_unlikely_null(buf))
		return -1;

	total = 0;
	for (; __h!=ci->dummy; __h=__h->next){
		size_t len;
		len = __fly_payload_from_headers(buf, __h);
		/* over limit of payload length */
		if (total+len >= max_payload){
			__f = __fly_send_headers_frame(stream, pool, buf, total, over, flag);
			__fly_hv2_add_yet_send_frame(__f);
			fly_buffer_chain_release_from_length(buf->first_chain, total);
			over = true;
			total = 0;
		}

		total += len;
	}

	flag = FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS;
	if (!exist_content)
		flag |= FLY_HV2_FRAME_TYPE_HEADERS_END_STREAM;

	__f = __fly_send_headers_frame(stream, pool, buf, total, over, flag);
	__fly_hv2_add_yet_send_frame(__f);
	fly_buffer_release(buf);
	return 0;
}

void fly_send_settings_frame(fly_hv2_stream_t *stream, uint16_t *id, uint32_t *value, size_t count, bool ack)
{
	struct fly_hv2_send_frame *frame;
	uint8_t flag=0;

	frame = fly_pballoc(stream->request->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(frame))
		return;
	frame->stream = stream;
	frame->pool = stream->request->pool;
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->sid = FLY_HV2_STREAM_ROOT_ID;
	frame->payload_len = !ack ? count*FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH : 0;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_SETTINGS;
	frame->next = stream->yetack;
	frame->payload = (!ack && count) ? fly_pballoc(stream->request->pool, frame->payload_len) : NULL;
	if ((!ack && count) && fly_unlikely_null(frame->payload))
		return;

	if (ack)
		flag |= FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK;

	fly_fh_setting(&frame->frame_header, frame->payload_len, FLY_HV2_FRAME_TYPE_SETTINGS, flag, false, FLY_HV2_STREAM_ROOT_ID);
	fly_send_frame_add_stream(frame, ack);

	/* SETTING FRAME payload setting */
	if (!ack && count)
		fly_settings_frame_payload_set(frame->payload, id, value, count);

	__fly_hv2_add_yet_send_frame(frame);
	return;
}

__fly_static int __fly_send_frame(struct fly_hv2_send_frame *frame)
{
	size_t total = 0;
	ssize_t numsend;
	int c_sockfd;

	while(!(frame->send_fase == FLY_HV2_SEND_FRAME_FASE_PAYLOAD && total>=frame->payload_len)){
		if (FLY_CONNECT_ON_SSL(frame->stream->request->connect)){
			SSL *ssl = frame->stream->request->connect->ssl;
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
				numsend = send(c_sockfd, frame->payload+total, frame->payload_len-total, 0);
			if (FLY_BLOCKING(numsend))
				goto write_blocking;
			else if (numsend == -1)
				return __FLY_SEND_FRAME_ERROR;
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
}

/*
 *	No Blocking I/O.
 */
int fly_send_frame_event_handler(fly_event_t *e)
{
	struct fly_hv2_send_frame *frame;
	frame = (struct fly_hv2_send_frame *) e->event_data;

	switch(__fly_send_frame(frame)){
	case __FLY_SEND_FRAME_READING_BLOCKING:
		goto read_blocking;
	case __FLY_SEND_FRAME_WRITING_BLOCKING:
		goto write_blocking;
	case __FLY_SEND_FRAME_ERROR:
		return FLY_HV2_SEND_FRAME_ERROR;
	case __FLY_SEND_FRAME_SUCCESS:
		return FLY_HV2_SEND_FRAME_SUCCESS;
	}

	return FLY_HV2_SEND_FRAME_SUCCESS;
read_blocking:
	e->read_or_write = FLY_READ;
	goto blocking;
write_blocking:
	e->read_or_write = FLY_WRITE;
	goto blocking;
blocking:
	e->tflag = FLY_INHERIT;
	e->flag = FLY_MODIFY;
	e->event_data = (void *) frame;
	FLY_EVENT_HANDLER(e, fly_send_frame_event_handler);
	if (fly_event_register(e) == -1)
		return FLY_HV2_SEND_FRAME_ERROR;
	return FLY_HV2_SEND_FRAME_BLOCKING;
}

int fly_send_frame_event(fly_event_manager_t *manager, struct fly_hv2_send_frame *frame, int read_or_write)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	e->fd = frame->stream->request->connect->c_sockfd;
	e->read_or_write = read_or_write;
	e->event_data = (void *) frame;
	FLY_EVENT_HANDLER(e, fly_send_frame_event_handler);
	e->flag = 0;
	e->tflag = 0;
	e->eflag = 0;
	fly_sec(&e->timeout, FLY_SEND_FRAME_TIMEOUT);
	fly_event_socket(e);

	return fly_event_register(e);
}

#define FLY_HV2_SF_SETTINGS_HEADER_TABLE_SIZE_ENV		"FLY_SETTINGS_FRAME_HEADER_TABLE_SIZE"
#define FLY_HV2_SF_SETTINGS_ENABLE_PUSH_ENV				"FLY_SETTINGS_FRAME_ENABLE_PUSH"
#define FLY_HV2_SF_SETTINGS_MAX_CONCURRENT_STREAMS_ENV	"FLY_SETTINGS_MAX_CONCURRENT_STREAMS"
#define FLY_HV2_SF_SETTINGS_INITIAL_WINDOW_SIZE_ENV		"FLY_SETTINGS_INITIAL_WINDOW_SIZE"
#define FLY_HV2_SF_SETTINGS_MAX_FRAME_SIZE_ENV			"FLY_SETTINGS_MAX_FRAME_SIZE"
#define FLY_HV2_SF_SETTINGS_MAX_HEADER_LIST_SIZE_ENV	"FLY_SETTINGS_MAX_HEADER_LIST_SIZE"


void fly_send_settings_frame_of_server(fly_hv2_stream_t *stream)
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
#undef __FLY_HV2_SETTINGS_EID
}

void fly_send_frame_add_stream(struct fly_hv2_send_frame *frame, bool ack)
{
	fly_hv2_stream_t *stream;

	if (ack)
		return;
	stream = frame->stream;
	frame->next = stream->lyetack->next;
	frame->prev = stream->lyetack;
	stream->lyetack->next = frame;
	stream->lyetack = frame;
	stream->yetack->prev = frame;
	stream->yetack_count++;
}

void fly_received_settings_frame_ack(fly_hv2_stream_t *stream)
{
	struct fly_hv2_send_frame *__yack;

	for (__yack=stream->yetack->next; __yack!=stream->yetack; __yack=__yack->next){
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

			/* remove */
			__yack->prev->next = __yack->next;
			__yack->next->prev = __yack->prev;

			if (__yack == stream->lyetack)
				stream->lyetack = __yack->prev;
			stream->yetack_count--;

			/* TODO: release __yack resource */
			return;
		}
	}
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
void fly_hv2_emergency(fly_hv2_state_t *state)
{
	struct fly_hv2_send_frame *frame;

	frame = state->emergency_ptr;
	frame->stream = state->streams;
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

	__fly_hv2_add_yet_send_frame(frame);
	state->goaway = true;
}

int fly_hv2_add_header_by_index(struct fly_hv2_stream *stream, uint32_t index);
int fly_hv2_add_header_by_indexname(struct fly_hv2_stream *stream, __unused uint32_t index, __unused uint8_t *value, __unused uint32_t value_len, __unused bool huffman_value, fly_buffer_c *__c, enum fly_hv2_index_type index_type);
int fly_hv2_add_header_by_name(__unused struct fly_hv2_stream *stream, __unused uint8_t *name, __unused uint32_t name_len, __unused bool huffman_name, __unused uint8_t *value, __unused uint32_t value_len, __unused bool huffman_value, __unused fly_buffer_c *__c, enum fly_hv2_index_type index_type);

#define FLY_HV2_STATIC_TABLE_LENGTH			\
	((int) sizeof(static_table)/sizeof(struct fly_hv2_static_table))

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
	{-1, NULL								, NULL},
};

int fly_hv2_dynamic_table_init(struct fly_hv2_state *state)
{
	struct fly_hv2_dynamic_table *dt;

	dt = fly_pballoc(state->pool, sizeof(struct fly_hv2_dynamic_table));
	if (fly_unlikely_null(dt))
		return -1;
	dt->entry_size = 0;
	dt->prev = dt->prev;
	dt->next = dt->next;
	state->dtable = dt;
	state->dtable_entry_count = 0;
	state->dtable_size = 0;

	return 0;
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
	state->dtable_max_index = state->dtable_entry_count+FLY_HV2_STATIC_TABLE_LENGTH-1;
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

static inline bool fly_hv2_is_index_header_field(uint8_t *pl)
{
	return *pl & (1<<7) ? true : false;
}

static inline bool fly_hv2_is_index_header_update(uint8_t *pl)
{
	return (*pl ^ (1<<7)) && (*pl & (1<<6)) ? true : false;
}

static inline bool fly_hv2_is_index_hedaer_noupdate(uint8_t *pl)
{
	return !(((*pl)>>4) & ((1<<4)-1)) ? true : false;
}

static inline bool fly_hv2_is_index_hedaer_noindex(uint8_t *pl)
{
	return !(((*pl)>>4) == 0x1) ? true : false;
}

#define FLY_HV2_INT_CONTFLAG(p)			(1<<(7))
#define FLY_HV2_INT_BIT_PREFIX(p)		((1<<(p)) - 1)
#define FLY_HV2_INT_BIT_VALUE(p)			((1<<(7)) - 1)
void fly_hv2_set_integer(uint32_t integer, uint8_t **pl, fly_buffer_c **__c, __unused uint32_t *update, uint8_t prefix_bit)
{
	**pl &= (~FLY_HV2_INT_BIT_PREFIX(prefix_bit));
	if (integer < (uint32_t) FLY_HV2_INT_BIT_PREFIX(prefix_bit)){
		if (update)
			(*update)++;
		**pl |=  (uint8_t) integer;
		fly_update_buffer((*__c)->buffer, 1);
		*pl = (uint8_t *) (*__c)->buffer->lunuse_ptr;
		*__c = (*__c)->buffer->lchain;
	}else{
		int now = integer - FLY_HV2_INT_BIT_PREFIX(prefix_bit);
		**pl |=  (uint8_t) integer;
		if (update)
			(*update)++;
		fly_update_buffer((*__c)->buffer, 1);
		*pl = (uint8_t *) (*__c)->buffer->lunuse_ptr;
		*__c = (*__c)->buffer->lchain;

		while(now>=FLY_HV2_INT_CONTFLAG(prefix_bit)){
			**pl = (now%FLY_HV2_INT_CONTFLAG(prefix_bit))+FLY_HV2_INT_CONTFLAG(prefix_bit);
			if (update)
				(*update)++;
			**pl |= FLY_HV2_INT_CONTFLAG(prefix_bit);
			fly_update_buffer((*__c)->buffer, 1);
			*pl = (uint8_t *) (*__c)->buffer->lunuse_ptr;
			*__c = (*__c)->buffer->lchain;
			now /= FLY_HV2_INT_CONTFLAG(prefix_bit);
		}

		**pl = now;
		**pl &= FLY_HV2_INT_BIT_VALUE(prefix_bit);
		fly_update_buffer((*__c)->buffer, 1);
		*pl = (uint8_t *) (*__c)->buffer->lunuse_ptr;
		*__c = (*__c)->buffer->lchain;
		if (update)
			(*update)++;
	}
	return;
}

uint32_t fly_hv2_integer(uint8_t **pl, fly_buffer_c **__c, __unused uint32_t *update, uint8_t prefix_bit)
{
	if (((*pl)[0]&FLY_HV2_INT_BIT_PREFIX(prefix_bit)) == FLY_HV2_INT_BIT_PREFIX(prefix_bit)){
		bool cont = true;
		uint32_t number=(uint32_t) FLY_HV2_INT_BIT_PREFIX(prefix_bit);
		size_t power = 0;

		while(cont){
			(*update)++;
			*pl = fly_update_chain(__c, *pl, 1);
			cont = **pl & FLY_HV2_INT_CONTFLAG(prefix_bit) ? true : false;
			number += (((**pl)&FLY_HV2_INT_BIT_VALUE(prefix_bit)) * (1<<power));
			power += 7;
		}
		(*update)++;
		*pl = fly_update_chain(__c, *pl, 1);
		return number;
	}else{
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
			/* TODO: found header field from static or dynamic table */
			if (fly_hv2_add_header_by_index(stream, index) == -1)
				return -1;
		}else{
			/* literal header field */
			/* update index */
			if (fly_hv2_is_index_header_update(payload)){
				index_type = INDEX_UPDATE;
			}else if (fly_hv2_is_index_hedaer_noupdate(payload))
				index_type = INDEX_NOUPDATE;
			else if (fly_hv2_is_index_hedaer_noindex(payload))
				index_type = NOINDEX;
			else{
				/* invalid header field */
				continue;
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
				payload = fly_update_chain(&__c, value, value_len);
			}else{
			/* header field name by literal */
				huffman_name = fly_hv2_is_huffman_encoding(payload);
				name_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_NAME_PREFIX_BIT);
				name = payload;
				/* update ptr */
				payload = fly_update_chain(&__c, name, name_len);

				huffman_value = fly_hv2_is_huffman_encoding(payload);
				value_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_VALUE_PREFIX_BIT);
				value = payload;

				if (fly_hv2_add_header_by_name(stream, name, name_len, huffman_name, value, value_len, huffman_value, __c, index_type) == -1)
					return -1;

				/* update ptr */
				total+=value_len;
				total+=name_len;
				payload = fly_update_chain(&__c, value, value_len);
			}
		}
	}
	return 0;
}

int fly_hv2_huffman_decode(fly_pool_t *pool, fly_buffer_t **res, uint32_t *decode_len, uint8_t *encoded, uint32_t len, fly_buffer_c *__c);

int fly_hv2_add_header_by_index(struct fly_hv2_stream *stream, uint32_t index)
{
	fly_hv2_state_t *state;
	const char *name, *value;
	size_t name_len, value_len;

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
				value_len = value ? strlen(value) : 0;
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
	const char *name;
	size_t name_len;

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
		if (fly_header_addbv(buf->first_chain, stream->request->header, (fly_hdr_name *) name, name_len, buf->first_useptr, len) == -1)
			return -1;

		if (index_type == INDEX_UPDATE){
			if (fly_hv2_dynamic_table_add_entry_bv(state, (char *) name, name_len, buf->first_chain, buf->first_useptr, buf->use_len) == -1)
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

int fly_hv2_add_header_by_name(__unused struct fly_hv2_stream *stream, __unused uint8_t *name, __unused uint32_t name_len, __unused bool huffman_name, __unused uint8_t *value, __unused uint32_t value_len, __unused bool huffman_value, __unused fly_buffer_c *__c, enum fly_hv2_index_type index_type __unused)
{
	fly_buffer_t *nbuf, *vbuf;
	uint32_t nlen, vlen;
	__unused fly_hv2_state_t *state;

	if (huffman_name && fly_hv2_huffman_decode(stream->request->header->pool, &nbuf, &nlen, name, name_len, __c) == -1)
		return -1;

	if (huffman_value && fly_hv2_huffman_decode(stream->request->header->pool, &vbuf, &vlen, value, value_len, __c) == -1)
		return -1;

	/* header add */

	return 0;
}


struct fly_hv2_huffman{
	uint16_t sym;
	const char *code_as_hex;
	uint32_t len_in_bits;
};

#define FLY_HV2_HUFFMAN_HUFFMAN_LEN		\
	(sizeof(huffman_codes)/sizeof(struct fly_hv2_huffman))
static struct fly_hv2_huffman huffman_codes[] = {
	{ 48, "\x0", 5 },
	{ 49, "\x1", 5 },
	{ 50, "\x2", 5 },
	{ 97, "\x3", 5 },
	{ 99, "\x4", 5 },
	{ 101, "\x5", 5 },
	{ 105, "\x6", 5 },
	{ 111, "\x7", 5 },
	{ 115, "\x8", 5 },
	{ 116, "\x9", 5 },
	{ 32, "\x14", 6 },
	{ 37, "\x15", 6 },
	{ 45, "\x16", 6 },
	{ 46, "\x17", 6 },
	{ 47, "\x18", 6 },
	{ 51, "\x19", 6 },
	{ 52, "\x1a", 6 },
	{ 53, "\x1b", 6 },
	{ 54, "\x1c", 6 },
	{ 55, "\x1d", 6 },
	{ 56, "\x1e", 6 },
	{ 57, "\x1f", 6 },
	{ 61, "\x20", 6 },
	{ 65, "\x21", 6 },
	{ 95, "\x22", 6 },
	{ 98, "\x23", 6 },
	{ 100, "\x24", 6 },
	{ 102, "\x25", 6 },
	{ 103, "\x26", 6 },
	{ 104, "\x27", 6 },
	{ 108, "\x28", 6 },
	{ 109, "\x29", 6 },
	{ 110, "\x2a", 6 },
	{ 112, "\x2b", 6 },
	{ 114, "\x2c", 6 },
	{ 117, "\x2d", 6 },
	{ 58, "\x5c", 7 },
	{ 66, "\x5d", 7 },
	{ 67, "\x5e", 7 },
	{ 68, "\x5f", 7 },
	{ 69, "\x60", 7 },
	{ 70, "\x61", 7 },
	{ 71, "\x62", 7 },
	{ 72, "\x63", 7 },
	{ 73, "\x64", 7 },
	{ 74, "\x65", 7 },
	{ 75, "\x66", 7 },
	{ 76, "\x67", 7 },
	{ 77, "\x68", 7 },
	{ 78, "\x69", 7 },
	{ 79, "\x6a", 7 },
	{ 80, "\x6b", 7 },
	{ 81, "\x6c", 7 },
	{ 82, "\x6d", 7 },
	{ 83, "\x6e", 7 },
	{ 84, "\x6f", 7 },
	{ 85, "\x70", 7 },
	{ 86, "\x71", 7 },
	{ 87, "\x72", 7 },
	{ 89, "\x73", 7 },
	{ 106, "\x74", 7 },
	{ 107, "\x75", 7 },
	{ 113, "\x76", 7 },
	{ 118, "\x77", 7 },
	{ 119, "\x78", 7 },
	{ 120, "\x79", 7 },
	{ 121, "\x7a", 7 },
	{ 122, "\x7b", 7 },
	{ 38, "\xf8", 8 },
	{ 42, "\xf9", 8 },
	{ 44, "\xfa", 8 },
	{ 59, "\xfb", 8 },
	{ 88, "\xfc", 8 },
	{ 90, "\xfd", 8 },
	{ 33, "\x3f\x8", 10 },
	{ 34, "\x3f\x9", 10 },
	{ 40, "\x3f\xa", 10 },
	{ 41, "\x3f\xb", 10 },
	{ 63, "\x3f\xc", 10 },
	{ 39, "\x7f\xa", 11 },
	{ 43, "\x7f\xb", 11 },
	{ 124, "\x7f\xc", 11 },
	{ 35, "\xff\xa", 12 },
	{ 62, "\xff\xb", 12 },
	{ 0, "\x1f\xf8", 13 },
	{ 36, "\x1f\xf9", 13 },
	{ 64, "\x1f\xfa", 13 },
	{ 91, "\x1f\xfb", 13 },
	{ 93, "\x1f\xfc", 13 },
	{ 126, "\x1f\xfd", 13 },
	{ 94, "\x3f\xfc", 14 },
	{ 125, "\x3f\xfd", 14 },
	{ 60, "\x7f\xfc", 15 },
	{ 96, "\x7f\xfd", 15 },
	{ 123, "\x7f\xfe", 15 },
	{ 92, "\x7f\xff\x0", 19 },
	{ 195, "\x7f\xff\x1", 19 },
	{ 208, "\x7f\xff\x2", 19 },
	{ 128, "\xff\xfe\x6", 20 },
	{ 130, "\xff\xfe\x7", 20 },
	{ 131, "\xff\xfe\x8", 20 },
	{ 162, "\xff\xfe\x9", 20 },
	{ 184, "\xff\xfe\xa", 20 },
	{ 194, "\xff\xfe\xb", 20 },
	{ 224, "\xff\xfe\xc", 20 },
	{ 226, "\xff\xfe\xd", 20 },
	{ 153, "\x1f\xff\xdc", 21 },
	{ 161, "\x1f\xff\xdd", 21 },
	{ 167, "\x1f\xff\xde", 21 },
	{ 172, "\x1f\xff\xdf", 21 },
	{ 176, "\x1f\xff\xe0", 21 },
	{ 177, "\x1f\xff\xe1", 21 },
	{ 179, "\x1f\xff\xe2", 21 },
	{ 209, "\x1f\xff\xe3", 21 },
	{ 216, "\x1f\xff\xe4", 21 },
	{ 217, "\x1f\xff\xe5", 21 },
	{ 227, "\x1f\xff\xe6", 21 },
	{ 229, "\x1f\xff\xe7", 21 },
	{ 230, "\x1f\xff\xe8", 21 },
	{ 129, "\x3f\xff\xd2", 22 },
	{ 132, "\x3f\xff\xd3", 22 },
	{ 133, "\x3f\xff\xd4", 22 },
	{ 134, "\x3f\xff\xd5", 22 },
	{ 136, "\x3f\xff\xd6", 22 },
	{ 146, "\x3f\xff\xd7", 22 },
	{ 154, "\x3f\xff\xd8", 22 },
	{ 156, "\x3f\xff\xd9", 22 },
	{ 160, "\x3f\xff\xda", 22 },
	{ 163, "\x3f\xff\xdb", 22 },
	{ 164, "\x3f\xff\xdc", 22 },
	{ 169, "\x3f\xff\xdd", 22 },
	{ 170, "\x3f\xff\xde", 22 },
	{ 173, "\x3f\xff\xdf", 22 },
	{ 178, "\x3f\xff\xe0", 22 },
	{ 181, "\x3f\xff\xe1", 22 },
	{ 185, "\x3f\xff\xe2", 22 },
	{ 186, "\x3f\xff\xe3", 22 },
	{ 187, "\x3f\xff\xe4", 22 },
	{ 189, "\x3f\xff\xe5", 22 },
	{ 190, "\x3f\xff\xe6", 22 },
	{ 196, "\x3f\xff\xe7", 22 },
	{ 198, "\x3f\xff\xe8", 22 },
	{ 228, "\x3f\xff\xe9", 22 },
	{ 232, "\x3f\xff\xea", 22 },
	{ 233, "\x3f\xff\xeb", 22 },
	{ 1, "\x7f\xff\xd8", 23 },
	{ 135, "\x7f\xff\xd9", 23 },
	{ 137, "\x7f\xff\xda", 23 },
	{ 138, "\x7f\xff\xdb", 23 },
	{ 139, "\x7f\xff\xdc", 23 },
	{ 140, "\x7f\xff\xdd", 23 },
	{ 141, "\x7f\xff\xde", 23 },
	{ 143, "\x7f\xff\xdf", 23 },
	{ 147, "\x7f\xff\xe0", 23 },
	{ 149, "\x7f\xff\xe1", 23 },
	{ 150, "\x7f\xff\xe2", 23 },
	{ 151, "\x7f\xff\xe3", 23 },
	{ 152, "\x7f\xff\xe4", 23 },
	{ 155, "\x7f\xff\xe5", 23 },
	{ 157, "\x7f\xff\xe6", 23 },
	{ 158, "\x7f\xff\xe7", 23 },
	{ 165, "\x7f\xff\xe8", 23 },
	{ 166, "\x7f\xff\xe9", 23 },
	{ 168, "\x7f\xff\xea", 23 },
	{ 174, "\x7f\xff\xeb", 23 },
	{ 175, "\x7f\xff\xec", 23 },
	{ 180, "\x7f\xff\xed", 23 },
	{ 182, "\x7f\xff\xee", 23 },
	{ 183, "\x7f\xff\xef", 23 },
	{ 188, "\x7f\xff\xf0", 23 },
	{ 191, "\x7f\xff\xf1", 23 },
	{ 197, "\x7f\xff\xf2", 23 },
	{ 231, "\x7f\xff\xf3", 23 },
	{ 239, "\x7f\xff\xf4", 23 },
	{ 9, "\xff\xff\xea", 24 },
	{ 142, "\xff\xff\xeb", 24 },
	{ 144, "\xff\xff\xec", 24 },
	{ 145, "\xff\xff\xed", 24 },
	{ 148, "\xff\xff\xee", 24 },
	{ 159, "\xff\xff\xef", 24 },
	{ 171, "\xff\xff\xf0", 24 },
	{ 206, "\xff\xff\xf1", 24 },
	{ 215, "\xff\xff\xf2", 24 },
	{ 225, "\xff\xff\xf3", 24 },
	{ 236, "\xff\xff\xf4", 24 },
	{ 237, "\xff\xff\xf5", 24 },
	{ 199, "\x1f\xff\xfe\xc", 25 },
	{ 207, "\x1f\xff\xfe\xd", 25 },
	{ 234, "\x1f\xff\xfe\xe", 25 },
	{ 235, "\x1f\xff\xfe\xf", 25 },
	{ 192, "\x3f\xff\xfe\x0", 26 },
	{ 193, "\x3f\xff\xfe\x1", 26 },
	{ 200, "\x3f\xff\xfe\x2", 26 },
	{ 201, "\x3f\xff\xfe\x3", 26 },
	{ 202, "\x3f\xff\xfe\x4", 26 },
	{ 205, "\x3f\xff\xfe\x5", 26 },
	{ 210, "\x3f\xff\xfe\x6", 26 },
	{ 213, "\x3f\xff\xfe\x7", 26 },
	{ 218, "\x3f\xff\xfe\x8", 26 },
	{ 219, "\x3f\xff\xfe\x9", 26 },
	{ 238, "\x3f\xff\xfe\xa", 26 },
	{ 240, "\x3f\xff\xfe\xb", 26 },
	{ 242, "\x3f\xff\xfe\xc", 26 },
	{ 243, "\x3f\xff\xfe\xd", 26 },
	{ 255, "\x3f\xff\xfe\xe", 26 },
	{ 203, "\x7f\xff\xfd\xe", 27 },
	{ 204, "\x7f\xff\xfd\xf", 27 },
	{ 211, "\x7f\xff\xfe\x0", 27 },
	{ 212, "\x7f\xff\xfe\x1", 27 },
	{ 214, "\x7f\xff\xfe\x2", 27 },
	{ 221, "\x7f\xff\xfe\x3", 27 },
	{ 222, "\x7f\xff\xfe\x4", 27 },
	{ 223, "\x7f\xff\xfe\x5", 27 },
	{ 241, "\x7f\xff\xfe\x6", 27 },
	{ 244, "\x7f\xff\xfe\x7", 27 },
	{ 245, "\x7f\xff\xfe\x8", 27 },
	{ 246, "\x7f\xff\xfe\x9", 27 },
	{ 247, "\x7f\xff\xfe\xa", 27 },
	{ 248, "\x7f\xff\xfe\xb", 27 },
	{ 250, "\x7f\xff\xfe\xc", 27 },
	{ 251, "\x7f\xff\xfe\xd", 27 },
	{ 252, "\x7f\xff\xfe\xe", 27 },
	{ 253, "\x7f\xff\xfe\xf", 27 },
	{ 254, "\x7f\xff\xff\x0", 27 },
	{ 2, "\xff\xff\xfe\x2", 28 },
	{ 3, "\xff\xff\xfe\x3", 28 },
	{ 4, "\xff\xff\xfe\x4", 28 },
	{ 5, "\xff\xff\xfe\x5", 28 },
	{ 6, "\xff\xff\xfe\x6", 28 },
	{ 7, "\xff\xff\xfe\x7", 28 },
	{ 8, "\xff\xff\xfe\x8", 28 },
	{ 11, "\xff\xff\xfe\x9", 28 },
	{ 12, "\xff\xff\xfe\xa", 28 },
	{ 14, "\xff\xff\xfe\xb", 28 },
	{ 15, "\xff\xff\xfe\xc", 28 },
	{ 16, "\xff\xff\xfe\xd", 28 },
	{ 17, "\xff\xff\xfe\xe", 28 },
	{ 18, "\xff\xff\xfe\xf", 28 },
	{ 19, "\xff\xff\xff\x0", 28 },
	{ 20, "\xff\xff\xff\x1", 28 },
	{ 21, "\xff\xff\xff\x2", 28 },
	{ 23, "\xff\xff\xff\x3", 28 },
	{ 24, "\xff\xff\xff\x4", 28 },
	{ 25, "\xff\xff\xff\x5", 28 },
	{ 26, "\xff\xff\xff\x6", 28 },
	{ 27, "\xff\xff\xff\x7", 28 },
	{ 28, "\xff\xff\xff\x8", 28 },
	{ 29, "\xff\xff\xff\x9", 28 },
	{ 30, "\xff\xff\xff\xa", 28 },
	{ 31, "\xff\xff\xff\xb", 28 },
	{ 127, "\xff\xff\xff\xc", 28 },
	{ 220, "\xff\xff\xff\xd", 28 },
	{ 249, "\xff\xff\xff\xe", 28 },
	{ 10, "\x3f\xff\xff\xfc", 30 },
	{ 13, "\x3f\xff\xfe\xfd", 30 },
	{ 22, "\x3f\xff\xff\xfe", 30 },
	{ 256, "\x3f\xff\xff\xff", 30 },
	{ -1, NULL, -1 },
};

static inline uint8_t fly_huffman_decode_k_bit(int k, struct fly_hv2_huffman *code)
{
	uint32_t divided = k/FLY_HV2_OCTET_LEN;
	uint8_t spbyte, spbit;

	if ((divided*FLY_HV2_OCTET_LEN + FLY_HV2_OCTET_LEN) > code->len_in_bits)
		spbyte = code->len_in_bits%FLY_HV2_OCTET_LEN;
	else
		spbyte = FLY_HV2_OCTET_LEN;

	spbit = code->code_as_hex[k/8] & (1<<(spbyte-1-k%8));
	return spbit == 0x0;
}

static inline uint8_t fly_huffman_decode_buf_bit(uint8_t j, uint8_t *ptr)
{
	uint8_t spbit;

	spbit = *ptr & (1<<(FLY_HV2_OCTET_LEN-1-j));
	return spbit == 0x0;
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
		bufp = buf->lunuse_ptr;

		/* padding check */
		if (len-1==0 && fly_huffman_last_padding(start_bit, ptr)){
			/* eos */
			break;
		}

		for (code=huffman_codes; code->code_as_hex; code++){
			step = 0;
			int j=start_bit;
			fly_buffer_c *p=__c;
			uint8_t *tmp=ptr;

			for (uint32_t k=0; k<code->len_in_bits; k++){
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

		if (!code->code_as_hex)
			FLY_NOT_COME_HERE
	}
	*decode_len = buf->use_len;
	*res = buf;

	return 0;
}

int fly_hv2_parse_data(fly_hv2_stream_t *stream, uint32_t length, uint8_t *payload, fly_buffer_c *__c)
{
	fly_body_t *body;
	fly_bodyc_t *bc;
	fly_request_t *req;

	req = stream->request;
	body = fly_body_init();
	if (fly_unlikely_null(body))
		return -1;

	req->body = body;
	bc = fly_pballoc(body->pool, sizeof(uint8_t)*length);
	if (fly_unlikely_null(bc))
		return -1;

	/* copy receive buffer to body content. */
	fly_buffer_memcpy((char *) bc, (char *) payload, __c, length);
	if (fly_body_setting(body, bc, length) == -1)
		return -1;

	return 0;
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
	req->request_line = fly_pballoc(req->pool, sizeof(struct fly_request_line));
	req->request_line->request_line = NULL;
	if (fly_unlikely_null(req->request_line))
		return -1;
	req->request_line->version = fly_match_version_from_type(V2);
	if (fly_unlikely_null(req->request_line->version))
		return -1;

	/* convert pseudo header to request line */
	for (fly_hdr_c *__c=ci->dummy->next; __c!=ci->dummy; __c=__c->next){
		if (__fly_hv2_pseudo_header(__c)){
			for (char **__p=(char **) fly_pseudo_request; *__p; __p++){
				if (strncmp(*__p, __c->name, strlen(*__p)) == 0){
					if (*__p == (char *) FLY_HV2_REQUEST_PSEUDO_HEADER_METHOD){
						req->request_line->method = fly_match_method_name(__c->value);
						if (!req->request_line->method){
							/* invalid method */
						}
					}else if (*__p == (char *) FLY_HV2_REQUEST_PSEUDO_HEADER_SCHEME){
						req->request_line->scheme = fly_match_scheme_name(__c->value);
						if (!req->request_line->scheme){
							/* invalid method */
						}
					}else if (*__p == (char *) FLY_HV2_REQUEST_PSEUDO_HEADER_AUTHORITY){
					}else if (*__p == (char *) FLY_HV2_REQUEST_PSEUDO_HEADER_PATH){
						req->request_line->uri.ptr = __c->value;
						req->request_line->uri.len = strlen(__c->value);
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
	if (!__fly_uri || !__fly_request_method || !__fly_scheme){
		/* invalid request */
		return -1;
	}
#undef __fly_uri
#undef __fly_request_method
#undef __fly_scheme
	return 0;
}

int fly_hv2_response_event(fly_event_t *e);

int fly_hv2_responses(fly_event_t *e, fly_hv2_state_t *state __unused)
{
	fly_response_t *res;

	res = state->responses->next->response;
	e->event_data = res;

	return fly_hv2_response_event(e);
}

void fly_hv2_add_response(fly_hv2_state_t *state, struct fly_hv2_response *res)
{
	state->lresponse->next = res;
	res->next = state->responses;
	res->prev = state->lresponse;
	state->responses->prev = res;
	state->response_count++;
	state->lresponse = res;
	return;
}

void fly_hv2_remove_hv2_response(fly_hv2_state_t *state, struct fly_hv2_response *res)
{
	struct fly_hv2_response *__r;

	for (__r=state->responses->next; __r!=state->responses; __r=__r->next){
		if (__r == res){
			__r->prev->next = __r->next;
			__r->next->prev = __r->prev;
			if (__r == state->lresponse)
				state->lresponse = __r->prev;

			fly_pbfree(state->pool, res);
			state->response_count--;
			return;
		}
	}
	return;
}

void fly_hv2_remove_response(fly_hv2_state_t *state, fly_response_t *res)
{
	struct fly_hv2_response *__r;

	for (__r=state->responses->next; __r!=state->responses; __r=__r->next){
		if (__r->response == res)
			return fly_hv2_remove_hv2_response(state, __r);
	}
	return;
}

int fly_hv2_response_event_handler(fly_event_t *e, fly_hv2_stream_t *stream)
{
	fly_request_t *request;
	fly_response_t *response;

	request = stream->request;
	/*
	 *	create response from request.
	 */
	if (fly_hv2_request_line_from_header(request) == -1){
		/* TODO: response 400 error */
		goto response_400;
	}

	if (fly_hv2_request_target_parse(request) == -1){
		/* TODO: response 400 error */
		goto response_400;
	}

	/* accept encoding parse */
	if (fly_accept_encoding(request) == -1)
		goto error;

	/* accept mime parse */
	if (fly_accept_mime(request) == -1)
		goto error;

	/* accept charset parse */
	if (fly_accept_charset(request) == -1)
		goto error;

	/* accept language parse */
	if (fly_accept_language(request) == -1)
		goto error;

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

__response:
	fly_event_fase(e, RESPONSE);

	/* Success parse request */
	enum method_type __mtype;
	fly_route_reg_t *route_reg;
	fly_route_t *route;
	fly_mount_t *mount;
	struct fly_mount_parts_file *pf;

	__mtype = request->request_line->method->type;
	route_reg = e->manager->ctx->route_reg;
	/* search from registerd route uri */
	route = fly_found_route(route_reg, request->request_line->uri.ptr, __mtype);
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
	response = route->function(request);
	if (response == NULL)
		goto response_500;

	response->request = request;
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

	e->event_data = response;
	return fly_hv2_response_event(e);
response:
	return 0;
response_304:
	return 0;
response_400:
	return 0;
response_404:
	return 0;
response_405:
	return 0;
response_500:
	return 0;
error:
	return -1;
}

/*
 *	HTTP2 Send Data Frame
 */

/*
 *	 add ":status " to the begining of header
 */
int fly_status_code_pseudo_headers(fly_response_t *res __unused)
{
	__unused const char *stcode_str;

	stcode_str = fly_status_code_str_from_type(res->status_code);

	return fly_header_add_v2(res->header, ":status", strlen(":status"), (fly_hdr_value *) stcode_str, strlen(stcode_str), true);
}

int fly_hv2_response_blocking_event(fly_event_t *e, fly_hv2_stream_t *stream)
{
	e->read_or_write = FLY_READ|FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	e->available = false;
	e->event_data = (void *) stream->state->connect;
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
	fly_response_t *res;
	fly_hv2_stream_t *stream;
	struct fly_hv2_response *v2_res;

	res = (fly_response_t *) e->event_data;
	stream = res->request->stream;

	if (stream->stream_state != FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE){
		/* invalid state */
	}

	/* already send headers */
	if (stream->end_send_data)
		goto log;
	else if (stream->end_send_headers)
		goto send_body;

	v2_res = fly_pballoc(stream->state->pool, sizeof(struct fly_hv2_response));
	if (fly_unlikely_null(v2_res))
		return -1;
	v2_res->response = res;
	fly_hv2_add_response(stream->state, v2_res);

	res->fase = FLY_RESPONSE_HEADER;
	/* response headers */
	if (fly_status_code_pseudo_headers(res) == -1)
		return -1;

	fly_rcbs_t *rcbs=NULL;
	if (res->de != NULL)
		goto end_of_encoding;
	res->fase = FLY_RESPONSE_BODY;
	/* if there is default content, add content-length header */
	if (res->body == NULL && res->pf == NULL){
		rcbs = fly_default_content_by_stcode_from_event(e, res->status_code);
		if (rcbs){
			if (fly_add_content_length_from_fd(res->header, rcbs->fd, false) == -1)
				return -1;
			if (fly_add_content_type(res->header, rcbs->mime, false) == -1)
				return -1;
		}
	}

	if (__fly_encode_do(res)){
		res->type = FLY_RESPONSE_TYPE_ENCODED;
		fly_encoding_type_t *enctype=NULL;
		enctype = fly_decided_encoding_type(res->encoding);
		if (fly_unlikely_null(enctype))
			return -1;

		if (enctype->type == fly_identity)
			goto end_of_encoding;

		fly_de_t *__de;

		__de = fly_de_init(res->pool);
		fly_e_buf_add(__de);
		fly_d_buf_add(__de);
		__de->type = FLY_DE_ESEND_FROM_PATH;
		__de->fd = res->pf->fd;
		__de->offset = res->offset;
		__de->count = res->pf->fs.st_size;
		__de->event = e;
		__de->response = res;
		__de->c_sockfd = e->fd;
		__de->etype = enctype;
		__de->fase = FLY_DE_INIT;
		__de->bfs = 0;
		__de->end = false;
		res->de = __de;

		if (fly_unlikely_null(__de->decbuf) || \
				fly_unlikely_null(__de->encbuf))
			return -1;
		if (enctype->encode(__de) == -1)
			return -1;
	} else if (res->body)
		res->type = FLY_RESPONSE_TYPE_BODY;
	else if (res->pf)
		res->type = FLY_RESPONSE_TYPE_PATH_FILE;
	else if (res->rcbs)
		res->type = FLY_RESPONSE_TYPE_DEFAULT;

end_of_encoding:
	if (fly_send_headers_frame(stream, res->pool, e, res->header, (res->body!=NULL || res->pf!=NULL)))
		return -1;

	/* send response header */
	return __fly_send_frame_h(e, res);
send_body:
	/* only send */
	return fly_send_data_frame(e, res);

log:
	if (__fly_response_log(res, e) == -1)
		return -1;

	/* release response resources */
	e->event_data = (void *) res->request->connect;
	fly_hv2_remove_response(stream->state, res);
	fly_response_release(res);
	fly_request_release(res->request);
	if (stream->id > stream->state->max_handled_sid)
		stream->state->max_handled_sid = stream->id;
	stream->end_request_response = true;

	e->read_or_write |= FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);
	return fly_event_register(e);
}
