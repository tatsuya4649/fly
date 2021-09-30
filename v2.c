#include "v2.h"
#include "request.h"
#include "connect.h"

fly_hv2_stream_t *fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client);
int fly_hv2_request_event_handler(fly_event_t *e);
fly_hv2_stream_t *fly_hv2_stream_search_from_sid(fly_hv2_state_t *state, fly_sid_t sid);
int fly_send_settings_frame(fly_hv2_stream_t *stream, fly_event_t *e, fly_request_t *req, uint16_t *id, uint32_t *value, size_t count, bool ack);
#define FLY_HV2_SEND_FRAME_SUCCESS			(1)
#define FLY_HV2_SEND_FRAME_BLOCKING			(0)
#define FLY_HV2_SEND_FRAME_ERROR			(-1)
int fly_send_frame_event_handler(fly_event_t *e);
int fly_send_settings_frame_of_server(fly_hv2_stream_t *stream, fly_event_t *e, fly_request_t *req);
int fly_send_frame_event(fly_event_manager_t *manager, struct fly_hv2_send_frame *frame, int read_or_write);
#define __FLY_SEND_FRAME_READING_BLOCKING			(2)
#define __FLY_SEND_FRAME_WRITING_BLOCKING			(3)
#define __FLY_SEND_FRAME_ERROR						(-1)
#define __FLY_SEND_FRAME_SUCCESS					(1)
__fly_static int __fly_send_frame(struct fly_hv2_send_frame *frame);
#define FLY_SEND_SETTINGS_FRAME_SUCCESS			(1)
#define FLY_SEND_SETTINGS_FRAME_BLOCKING		(0)
#define FLY_SEND_SETTINGS_FRAME_ERROR			(-1)
int fly_settings_frame_ack(fly_hv2_stream_t *stream, fly_event_t *e, fly_request_t *req);
void fly_send_frame_add_stream(struct fly_hv2_send_frame *frame);
void fly_received_settings_frame_ack(fly_hv2_stream_t *stream);
int fly_hv2_dynamic_table_init(struct fly_hv2_state *state);
int fly_hv2_parse_headers(fly_request_t *req, uint32_t length, uint8_t *payload, fly_buffer_c *__c);

static inline uint32_t fly_hv2_length_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	uint32_t length=0;
	if (((fly_buf_p) __fh) + (int) (FLY_HV2_FRAME_HEADER_LENGTH_LEN/FLY_HV2_OCTET_LEN) > __c->lptr){
		length |= (uint32_t) (*__fh)[0] << 16;
		__fh = fly_update_chain(&__c, __fh, 1);
		length |= (uint32_t) (*__fh)[0] << 8;
		__fh = fly_update_chain(&__c, __fh, 1);
		length |= (uint32_t) (*__fh)[0];
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

static inline uint8_t fly_hv2_flags_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c) {
	uint8_t flags;

	if ((fly_buf_p) __fh + 4 > __c->lptr){
		__fh = fly_update_chain(&__c, __fh, 4);
		flags = (uint8_t) (*__fh)[0];
	}else
		flags = (uint8_t) (*__fh)[4];

	return flags;
}

__unused static inline bool fly_hv2_r_from_frame_header(fly_hv2_frame_header_t *__fh, fly_buffer_c *__c)
{
	bool __r;

	if ((fly_buf_p) __fh + 5 > __c->lptr){ __fh = fly_update_chain(&__c, __fh, 5);
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

		__sid |= (uint32_t) (*__fh)[0] & ((1<<8)-1) << 24;
		__fh = fly_update_chain(&__c, __fh, 1);
		__sid |= (uint32_t) (*__fh)[0] << 16;
		__fh = fly_update_chain(&__c, __fh, 1);
		__sid |= (uint32_t) (*__fh)[0] << 8;
		__fh = fly_update_chain(&__c, __fh, 1);
		__sid |= (uint32_t) (*__fh)[0];
		__fh = fly_update_chain(&__c, __fh, 1);
	}else{
		/* clear R bit */
		__sid |= ((uint32_t) (*__fh)[0] & ((1<<8)-1)) << 24;
		__sid |= (uint32_t) (*__fh)[1] << 16;
		__sid |= (uint32_t) (*__fh)[2] << 8;
		__sid |= (uint32_t) (*__fh)[3];
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

	state->reserved_count++;
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
	__s->weight = FLY_HV2_STREAM_DEFAULT_WEIGHT;
	__s->lframe = __s->frames;
	__s->frame_count = 0;

	__s->yetack = fly_pballoc(state->pool, sizeof(struct fly_hv2_send_frame));
	__s->yetack->next = __s->yetack;
	__s->lyetack = __s->yetack;
	__s->yetack_count = 0;

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

fly_hv2_stream_t *fly_hv2_create_stream(fly_hv2_state_t *state, fly_sid_t id, bool from_client)
{
	fly_hv2_stream_t *ns;
	if (state->max_sid >= id)
		/* TODO: connection error(PROTOCOL_ERROR) */
		return NULL;
	if (state->max_concurrent_streams != (fly_settings_t) FLY_HV2_MAX_CONCURRENT_STREAMS_INFINITY && \
			(fly_settings_t) state->stream_count >= state->max_concurrent_streams)
		/* TODO: connection error(PROTOCOL_ERROR) */
		return NULL;

	/*
	 *	stream id:
	 *	from client -> must be odd.
	 *	from server -> must be even.
	 */
	if (id && from_client && id%2 == 0){
		/* TODO: connection error(PROTOCOL_ERROR) */
		return NULL;
	}else if (id && !from_client && id%2){
		/* TODO: connection error(PROTOCOL_ERROR) */
		return NULL;
	}

	ns = __fly_hv2_create_stream(state, id, from_client);
	__fly_hv2_add_stream(state, ns);
	state->max_sid = id;

	return ns;
}

#define FLY_HV2_INIT_CONNECTION_PREFACE_CONTINUATION	(0)
#define FLY_HV2_INIT_CONNECTION_PREFACE_ERROR			(-1)
#define FLY_HV2_INIT_CONNECTION_PREFACE_SUCCESS			(1)
int fly_hv2_init_connection_preface(fly_request_t *req)
{
	fly_buffer_t *buf = req->buffer;

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
int fly_hv2_init_handler(fly_event_t *e)
{
	fly_connect_t *conn;
	fly_request_t *req;
	fly_pool_t *__pool;
	fly_buffer_t *buf;

	req = (fly_request_t *) e->event_data;
	buf = req->buffer;
	conn = req->connect;

	__pool = conn->pool;

	if (!conn->v2_state){
		fly_hv2_state_t *hv2_s;

		hv2_s = fly_pballoc(__pool, sizeof(fly_hv2_state_t));
		hv2_s->pool = __pool;
		if (fly_unlikely_null(hv2_s))
			return -1;

		fly_hv2_default_settings(hv2_s);

		/* create root stream(0x0) */
		hv2_s->streams = __fly_hv2_create_stream(hv2_s, FLY_HV2_STREAM_ROOT_ID, true);
		if (fly_unlikely_null(hv2_s->streams))
			return -1;
		hv2_s->stream_count = 1;
		hv2_s->reserved = __fly_hv2_create_stream(hv2_s, FLY_HV2_STREAM_ROOT_ID, false);
		if (fly_unlikely_null(hv2_s->reserved))
			return -1;
		hv2_s->reserved_count = 0;
		hv2_s->connection_state = FLY_HV2_CONNECTION_STATE_INIT;
		hv2_s->lstream = hv2_s->streams;
		hv2_s->max_sid = FLY_HV2_STREAM_ROOT_ID;
		if (fly_hv2_dynamic_table_init(hv2_s) == -1)
			return -1;

		conn->v2_state = hv2_s;
	}

	switch(fly_request_receive(conn->c_sockfd, req)){
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

connection_preface:
	/* invalid connection preface */
	switch (fly_hv2_init_connection_preface(req)){
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
	if (req->buffer->use_len > strlen(FLY_CONNECTION_PREFACE))
		goto connection_preface;
	e->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	e->flag = FLY_MODIFY;
	FLY_EVENT_HANDLER(e, fly_hv2_init_handler);
	e->tflag = FLY_INHERIT;
	e->available = false;
	fly_event_socket(e);
	return fly_event_register(e);

disconnect:
	return -1;

error:
	return -1;
}

static inline uint8_t fly_hv2_headers_pad_length(uint8_t **pl, fly_buffer_c **__c)
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

static inline uint32_t fly_hv2_stream_dependency(uint8_t **pl, fly_buffer_c **__c)
{
	uint32_t sid = 0;

	if ((fly_buf_p) (*pl+4) > (*__c)->lptr){
		sid |= ((uint32_t) (*pl)[0] & ((1<<8)-1)) << 24;
		*pl = fly_update_chain(__c, *pl, 1);
		sid |= (uint32_t) (*pl)[0] << 16;
		*pl = fly_update_chain(__c, *pl, 1);
		sid |= (uint32_t) (*pl)[0] << 8;
		*pl = fly_update_chain(__c, *pl, 1);
		sid |= (uint32_t) (*pl)[0];
		*pl = fly_update_chain(__c, *pl, 1);
	}else{
		sid |= ((uint32_t) (*pl)[0] & ((1<<8)-1)) << 24;
		sid |= (uint32_t) (*pl)[1] << 16;
		sid |= (uint32_t) (*pl)[2] << 8;
		sid |= (uint32_t) (*pl)[3];

		*pl += (int) (FLY_HV2_FRAME_TYPE_PRIORITY_E_LEN+FLY_HV2_FRAME_TYPE_PRIORITY_STREAM_DEPENDENCY_LEN)/FLY_HV2_OCTET_LEN;
	}
	return sid;
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
			state->p_initial_window_size = value;
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

int fly_hv2_request_event_blocking(fly_event_t *e, fly_request_t *req)
{
	e->read_or_write = FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->event_data = (void *) req;
	FLY_EVENT_HANDLER(e, fly_hv2_request_event_handler);

	return fly_event_register(e);
}

static inline bool fly_hv2_settings_frame_ack_from_flags(uint8_t flags)
{
	return flags&FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK ? true : false;
}

int fly_hv2_request_event_handler(fly_event_t *e)
{
	do{
		fly_request_t *req;
		fly_buf_p *bufp;
		fly_buffer_c *bufc, *plbufc;
		fly_hv2_state_t *state;

		req = (fly_request_t *) e->event_data;
		state = req->connect->v2_state;
		bufp = req->buffer->first_useptr;
		bufc = req->buffer->first_chain;

		fly_hv2_stream_t *__stream;
		fly_hv2_frame_header_t *__fh = (fly_hv2_frame_header_t *) bufp;
		/* if frame length more than limits, must send FRAME_SIZE_ERROR */
		uint32_t length = fly_hv2_length_from_frame_header(__fh, bufc);
		uint8_t type = fly_hv2_type_from_frame_header(__fh, bufc);
		uint32_t sid = fly_hv2_sid_from_frame_header(__fh, bufc);
		uint8_t flags = fly_hv2_flags_from_frame_header(__fh, bufc);

		plbufc = bufc;
		uint8_t *pl = fly_hv2_frame_payload_from_frame_header(__fh, &plbufc);
		if (length > state->max_frame_size){
			/* TODO: send FRAME_SIZE_ERROR */
			/* TODO: treat as CONNECTION ERROR */
		} else if ((FLY_HV2_FRAME_HEADER_LENGTH+length) > req->buffer->use_len)
			goto blocking;

		__stream = fly_hv2_stream_search_from_sid(state, sid);
		/* new stream */
		if (!__stream){
			__stream = fly_hv2_create_stream(state, sid, true);
			/* TODO: connection error(PROTOCOL_ERROR) */
			if (!__stream){

			}
		}

		if (sid!=FLY_HV2_STREAM_ROOT_ID && fly_hv2_create_frame(__stream, type, length, flags, pl) == -1)
			return -1;

		switch(state->connection_state){
		case FLY_HV2_CONNECTION_STATE_INIT:
		case FLY_HV2_CONNECTION_STATE_END:
			/* error */
			return -1;
		case FLY_HV2_CONNECTION_STATE_CONNECTION_PREFACE:
			/* only SETTINGS Frame */
			if (type != FLY_HV2_FRAME_TYPE_SETTINGS){
				/* TODO: PROTOCOL ERROR */
				return -1;
			}
			break;
		case FLY_HV2_CONNECTION_STATE_COMMUNICATION:
			break;
		}
		switch(type){
		case FLY_HV2_FRAME_TYPE_DATA:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID){
				/* TODO: PROTOCOL ERROR */
			}
			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_LOCAL)){
				/* TODO: STREAM_CLOSED ERROR */
			}

			break;
		case FLY_HV2_FRAME_TYPE_HEADERS:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID){ /* TODO: PROTOCOL ERROR */
			}
			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_IDLE || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_RESERVED_LOCAL || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE)){
				/* STREAM_CLOSED ERROR */
			}

			{
				__unused uint8_t pad_length;
				__unused bool e;
				__unused uint32_t stream_dependency;
				__unused uint8_t weight;

				if (flags & FLY_HV2_FRAME_TYPE_HEADERS_PADDED)
					pad_length = fly_hv2_headers_pad_length(&pl, &plbufc);
				if (flags & FLY_HV2_FRAME_TYPE_HEADERS_PRIORITY){
					e = fly_hv2_flag(&pl);
					stream_dependency = fly_hv2_stream_dependency(&pl, &plbufc);
					weight = fly_hv2_weight(&pl, &plbufc);
				}

				fly_hv2_parse_headers(req, length, pl, plbufc);
			}

			break;
		case FLY_HV2_FRAME_TYPE_PRIORITY:
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID){
				/* TODO: PROTOCOL ERROR */
			}
			/* can receive at any stream state */
			if (length != FLY_HV2_FRAME_TYPE_PRIORITY_LENGTH){
				/* TODO: FRAME_SIZE_ERROR */

			}

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
			if (__stream->id == FLY_HV2_STREAM_ROOT_ID){
				/* TODO: PROTOCOL ERROR */
			}
			if (__stream->stream_state == FLY_HV2_STREAM_STATE_IDLE){ /* TODO: PROTOCOL_ERROR */ }

			__stream->stream_state = FLY_HV2_STREAM_STATE_CLOSED;
			break;
		case FLY_HV2_FRAME_TYPE_SETTINGS:
			{
				bool ack;
				if (sid!=FLY_HV2_STREAM_ROOT_ID){
					/* TODO: connection error(PROTOCOL_ERROR) */
				}
				if (length % FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH){
					/* TODO: FRAME_SIZE_ERROR */
				}

				ack = fly_hv2_settings_frame_ack_from_flags(flags);

				if (ack)
					fly_received_settings_frame_ack(__stream);
				else{
					/* can receive at any stream state */
					switch(fly_hv2_peer_settings(state, sid, pl, length, plbufc)){
					case FLY_HV2_FLOW_CONTROL_ERROR:
						/* TODO: FLOW_CONTROL_ERROR */
						break;
					case FLY_HV2_PROTOCOL_ERROR:
						/* TODO: PROTOCOL_ERROR */
						break;
					case FLY_HV2_SETTINGS_SUCCESS:
						break;
					default:
						FLY_NOT_COME_HERE
					}
				}

				/* TODO: send setting frame of server */
				switch (fly_send_settings_frame_of_server(__stream, e, req)){
				case FLY_SEND_SETTINGS_FRAME_SUCCESS:
				case FLY_SEND_SETTINGS_FRAME_BLOCKING:
					break;
				case FLY_SEND_SETTINGS_FRAME_ERROR:
					return -1;
				}

				/* TODO: send ack */
				switch (fly_settings_frame_ack(__stream, e, req)){
				case FLY_SEND_SETTINGS_FRAME_SUCCESS:
				case FLY_SEND_SETTINGS_FRAME_BLOCKING:
					break;
				case FLY_SEND_SETTINGS_FRAME_ERROR:
					return -1;
				default:
					FLY_NOT_COME_HERE
				}

				/* connection state => FLY_HV2_CONNECTION_STATE_COMMUNICATION */
				state->connection_state = FLY_HV2_CONNECTION_STATE_COMMUNICATION;
			}


			break;
		case FLY_HV2_FRAME_TYPE_PUSH_PROMISE:
			if (sid==FLY_HV2_STREAM_ROOT_ID){
				/* TODO: connection error(PROTOCOL_ERROR) */
			}
			if (!(__stream->stream_state == FLY_HV2_STREAM_STATE_OPEN || \
					__stream->stream_state == FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE)){
				/* STREAM_CLOSED ERROR */
			}
			if (!state->enable_push){
				/* TODO: PROTOCOL_ERROR */
			}

			/* stream id must be new one */
			if (fly_hv2_stream_search_from_sid(state, sid)){
				/* TODO: PROTOCOL_ERROR */
			}

			/* create reserved stream */
			if (fly_hv2_stream_create_reserved(state, sid, true) == -1){
			}
			break;
		case FLY_HV2_FRAME_TYPE_PING:
			if (sid!=FLY_HV2_STREAM_ROOT_ID){
				/* TODO: connection error(PROTOCOL_ERROR) */
			}
			if (length != FLY_HV2_FRAME_TYPE_PING_LENGTH){
				/* TODO: FRAME_SIZE_ERROR */
			}
			break;
		case FLY_HV2_FRAME_TYPE_GOAWAY:
			if (sid!=FLY_HV2_STREAM_ROOT_ID){
				/* TODO: connection error(PROTOCOL_ERROR) */
			}
			break;
		case FLY_HV2_FRAME_TYPE_WINDOW_UPDATE:
			if (length != FLY_HV2_FRAME_TYPE_WINDOW_UPDATE_LENGTH){
				/* TODO: FRAME_SIZE_ERROR */
			}
			{
				uint32_t window_size;

				window_size = (*(uint32_t *) pl) >> 1;
				if (sid == 0)
					state->window_size = window_size;
				else
					__stream->window_size = window_size;
			}
			break;
		case FLY_HV2_FRAME_TYPE_CONTINUATION:
			if (sid==FLY_HV2_STREAM_ROOT_ID){
				/* TODO: connection error(PROTOCOL_ERROR) */
			}

			if (!((__stream->lframe->type == FLY_HV2_FRAME_TYPE_HEADERS && !(__stream->lframe->flags&FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS)) || (__stream->lframe->type == FLY_HV2_FRAME_TYPE_PUSH_PROMISE && !(__stream->lframe->flags&FLY_HV2_FRAME_TYPE_PUSH_PROMISE_END_HEADERS)) || (__stream->lframe->type == FLY_HV2_FRAME_TYPE_CONTINUATION && !(__stream->lframe->flags&FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS)))){
				/* TODO: PROTOCOL_ERROR */
			}
			break;
		default:
			/* unknown type. must ignore */
			return -1;
		}

		/*
		 * release SETTINGS Frame resource.
		 * release start point:		__fh.
		 * release length:			Frame Header(9octet) + Length of PayLoad.
		 */
		fly_buffer_chain_release_from_length(bufc, FLY_HV2_FRAME_HEADER_LENGTH+length);
		/* Is there next frame header in buffer? */
		if (bufc->use_len > FLY_HV2_FRAME_HEADER_LENGTH)
			continue;
		else
			/* blocking event */
			goto blocking;
blocking:
	return fly_hv2_request_event_blocking(e, req);

	} while (true);
	return 0;
}

void fly_fh_setting(fly_hv2_frame_header_t *__fh, uint32_t length, uint8_t type, uint8_t flags, bool r, uint32_t sid)
{
	*(((uint8_t *) (__fh))) = (uint8_t) (length >> 16);
	*(((uint8_t *) (__fh))+1) = (uint8_t) (length >> 8);
	*(((uint8_t *) (__fh))+2) = (uint8_t) (length >> 0);
	*(((uint8_t *) (__fh))+3) = type;
	*(((uint8_t *) (__fh))+4) = flags;
	if (r)
		*(((uint8_t *) (__fh))+5) |= 0x1;
	else
		*(((uint8_t *) (__fh))+5) |= 0x0;
	*(((uint8_t *) (__fh))+5) = (uint8_t) (sid >> 24);
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

int fly_settings_frame_ack(fly_hv2_stream_t *stream, fly_event_t *e, fly_request_t *req)
{
	return fly_send_settings_frame(stream, e, req, NULL, NULL, 0, true);
}
int fly_send_settings_frame(fly_hv2_stream_t *stream, fly_event_t *e, fly_request_t *req, uint16_t *id, uint32_t *value, size_t count, bool ack)
{
	struct fly_hv2_send_frame *frame;
	uint8_t flag=0;

	frame = fly_pballoc(req->pool, sizeof(struct fly_hv2_send_frame));
	if (fly_unlikely_null(frame))
		return -1;
	frame->stream = stream;
	frame->request = req;
	frame->pool = req->pool;
	frame->send_fase = FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER;
	frame->sid = FLY_HV2_STREAM_ROOT_ID;
	frame->payload_len = !ack ? count*FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH : 0;
	frame->send_len = 0;
	frame->type = FLY_HV2_FRAME_TYPE_SETTINGS;
	frame->next = stream->yetack;
	frame->payload = (!ack && count) ? fly_pballoc(req->pool, frame->payload_len) : NULL;
	if ((!ack && count) && fly_unlikely_null(frame->payload))
		return -1;

	if (ack)
		flag |= FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK;

	fly_fh_setting(&frame->frame_header, frame->payload_len, FLY_HV2_FRAME_TYPE_SETTINGS, flag, false, FLY_HV2_STREAM_ROOT_ID);
	fly_send_frame_add_stream(frame);

	/* TODO: SETTING FRAME payload setting */
	if (!ack && count)
		fly_settings_frame_payload_set(frame->payload, id, value, count);

	switch(__fly_send_frame(frame)){
	case __FLY_SEND_FRAME_READING_BLOCKING:
		goto read_blocking;
	case __FLY_SEND_FRAME_WRITING_BLOCKING:
		goto write_blocking;
	case __FLY_SEND_FRAME_ERROR:
		return FLY_SEND_SETTINGS_FRAME_ERROR;
	case __FLY_SEND_FRAME_SUCCESS:
		return FLY_SEND_SETTINGS_FRAME_SUCCESS;
	}

read_blocking:
	if (fly_send_frame_event(e->manager, frame, FLY_READ) == -1)
		return FLY_SEND_SETTINGS_FRAME_ERROR;
	return FLY_SEND_SETTINGS_FRAME_BLOCKING;

write_blocking:
	if (fly_send_frame_event(e->manager, frame, FLY_WRITE) == -1)
		return FLY_SEND_SETTINGS_FRAME_ERROR;
	return FLY_SEND_SETTINGS_FRAME_BLOCKING;
}

__fly_static int __fly_send_frame(struct fly_hv2_send_frame *frame)
{
	size_t total = 0;
	ssize_t numsend;
	int c_sockfd;

	while(!(frame->send_fase == FLY_HV2_SEND_FRAME_FASE_PAYLOAD && total>=frame->payload_len)){
		if (FLY_CONNECT_ON_SSL(frame->request)){
			SSL *ssl = frame->request->connect->ssl;
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
			c_sockfd = frame->request->connect->c_sockfd;
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
	/* TODO: make event */
	fly_event_t *e;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	e->fd = frame->request->connect->c_sockfd;
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


int fly_send_settings_frame_of_server(fly_hv2_stream_t *stream, fly_event_t *e, fly_request_t *req)
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
	return fly_send_settings_frame(stream, e, req, ids, values, count, false);
#undef __FLY_HV2_SETTINGS_EID
}

void fly_send_frame_add_stream(struct fly_hv2_send_frame *frame)
{
	fly_hv2_stream_t *stream;

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

int fly_hv2_add_header_by_index(uint32_t index, uint8_t *value, uint32_t value_len, bool huffman_value);
int fly_hv2_add_header_by_name(uint8_t *name, uint32_t name_len, bool huffman_name, uint8_t *value, uint32_t value_len, bool huffman_value);

#define FLY_HV2_STATIC_TABLE_LENGTH			\
	((int) sizeof(static_table)/sizeof(struct fly_hv2_static_table))

__unused static struct fly_hv2_static_table static_table[] = {
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

int fly_hv2_dynamic_table_init(struct fly_hv2_state *state)
{
	struct fly_hv2_dynamic_table *dt;

	dt = fly_pballoc(state->pool, sizeof(struct fly_hv2_dynamic_table));
	if (fly_unlikely_null(dt))
		return -1;
	dt->next = dt;
	dt->prev = dt;
	state->dtable = dt;
	state->dtable_entry_count = 0;

	return 0;
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

uint32_t fly_hv2_integer(uint8_t **pl, fly_buffer_c **__c, __unused uint32_t *update, uint8_t prefix_bit)
{
#define FLY_HV2_INT_CONTFLAG(p)			(1<<(p))
#define FLY_HV2_INT_BIT_PREFIX(p)		((1<<(p)) - 1)
#define FLY_HV2_INT_BIT_VALUE(p)			((1<<(p)) - 1)
	if (((*pl)[0]|FLY_HV2_INT_BIT_PREFIX(prefix_bit)) == FLY_HV2_INT_BIT_PREFIX(prefix_bit)){
		bool cont = false;
		uint32_t number=(uint32_t) FLY_HV2_INT_BIT_PREFIX(prefix_bit);
		size_t power = 7;

		while(cont){
			(*update)++;
			*pl = fly_update_chain(__c, *pl, 1);
			cont = **pl & FLY_HV2_INT_CONTFLAG(prefix_bit) ? true : false;
			number += ((**pl)&FLY_HV2_INT_BIT_VALUE(prefix_bit)) * (1<<power);
			power += 7;
		}
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

int fly_hv2_parse_headers(fly_request_t *req __unused, uint32_t length __unused, uint8_t *payload, fly_buffer_c *__c __unused)
{
	uint32_t total = 0;
	enum{
		INDEX_UPDATE,
		INDEX_NOUPDATE,
		NOINDEX,
	} index_type;

	while(total < length){
		__unused uint32_t index;
		__unused bool huffman_name, huffman_value;
		__unused uint32_t name_len, value_len;
		__unused uint8_t *name, *value;
		if (fly_hv2_is_index_header_field(payload)){
#define FLY_HV2_INDEX_PREFIX_BIT			7
#define FLY_HV2_NAME_PREFIX_BIT				7
#define FLY_HV2_VALUE_PREFIX_BIT			7

			index = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_INDEX_PREFIX_BIT);
			/* TODO: found header field from static or dynamic table */
		}else{
			/* literal header field */
			/* update index */
			if (fly_hv2_is_index_header_update(payload)){
#define FLY_HV2_LITERAL_UPDATE_PREFIX_BIT			6
#define FLY_HV2_LITERAL_NOUPDATE_PREFIX_BIT			4
#define FLY_HV2_LITERAL_NOINDEX_PREFIX_BIT			4
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
				huffman_value = fly_hv2_is_huffman_encoding(payload);
				value_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_VALUE_PREFIX_BIT);
				value = payload;
				if (fly_hv2_add_header_by_index(index, value, value_len, huffman_value) == -1)
					return -1;
			}else{
			/* header field name by literal */
				huffman_name = fly_hv2_is_huffman_encoding(payload);
				name_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_NAME_PREFIX_BIT);
				name = payload;

				huffman_value = fly_hv2_is_huffman_encoding(payload);
				value_len = fly_hv2_integer(&payload, &__c, &total, FLY_HV2_VALUE_PREFIX_BIT);
				value = payload;

				if (fly_hv2_add_header_by_name(name, name_len, huffman_name, value, value_len, huffman_value) == -1)
					return -1;
			}
		}
	}
	return 0;
}

int fly_hv2_add_header_by_index(__unused uint32_t index, __unused uint8_t *value, __unused uint32_t value_len, __unused bool huffman_value)
{
	return 0;
}

int fly_hv2_add_header_by_name(__unused uint8_t *name, __unused uint32_t name_len, __unused bool huffman_name, __unused uint8_t *value, __unused uint32_t value_len, __unused bool huffman_value)
{
	return 0;
}

struct fly_hv2_huffman{
	uint16_t sym;
	const char *code_as_hex;
};

#define FLY_HV2_HUFFMAN_HUFFMAN_LEN		\
	(sizeof(huffman_codes)/sizeof(struct fly_hv2_huffman))
struct fly_hv2_huffman huffman_codes[] = {
	{ 0, "\x1f\xf8" },
	{ 1, "\x7f\xff\xd8" },
	{ 2, "\xff\xff\xfe\x2" },
	{ 3, "\xff\xff\xfe\x3" },
	{ 4, "\xff\xff\xfe\x4" },
	{ 5, "\xff\xff\xfe\x5" },
	{ 6, "\xff\xff\xfe\x6" },
	{ 7, "\xff\xff\xfe\x7" },
	{ 8, "\xff\xff\xfe\x8" },
	{ 9, "\xff\xff\xea" },
	{ 10, "\x3f\xff\xff\xfc" },
	{ 11, "\xff\xff\xfe\x9" },
	{ 12, "\xff\xff\xfe\xa" },
	{ 13, "\x3f\xff\xfe\xfd" },
	{ 14, "\xff\xff\xfe\xb" },
	{ 15, "\xff\xff\xfe\xc" },
	{ 16, "\xff\xff\xfe\xd" },
	{ 17, "\xff\xff\xfe\xe" },
	{ 18, "\xff\xff\xfe\xf" },
	{ 19, "\xff\xff\xff\x0" },
	{ 20, "\xff\xff\xff\x1" },
	{ 21, "\xff\xff\xff\x2" },
	{ 22, "\xff\xff\xff\x3" },
	{ 23, "\xff\xff\xff\x4" },
	{ 24, "\xff\xff\xff\x5" },
	{ 25, "\xff\xff\xff\x6" },
	{ 26, "\xff\xff\xff\x7" },
	{ 27, "\xff\xff\xff\x8" },
	{ 28, "\xff\xff\xff\x9" },
	{ 29, "\xff\xff\xff\xa" },
	{ 30, "\xff\xff\xff\xb" },
	{ 31, "\xff\xff\xff\xc" },
	{ 32, "\x14" },
	{ 33, "\x3f\x8" },
	{ 34, "\x3f\x9" },
	{ 35, "\xff\xa" },
	{ 36, "\x1f\xf9" },
	{ 37, "\x15" },
	{ 38, "\xf8" },
	{ 39, "\x7f\xa" },
	{ 40, "\x3f\xa" },
	{ 41, "\x3f\xb" },
	{ 42, "\xf9" },
	{ 43, "\x7f\xb" },
	{ 44, "\xfa" },
	{ 45, "\x16" },
	{ 46, "\x17" },
	{ 47, "\x18" },
	{ 48, "\x0" },
	{ 49, "\x1" },
	{ 50, "\x2" },
	{ 51, "\x19" },
	{ 52, "\x1a" },
	{ 53, "\x1b" },
	{ 54, "\x1c" },
	{ 55, "\x1d" },
	{ 56, "\x1e" },
	{ 57, "\x1f" },
	{ 58, "\x5c" },
	{ 59, "\xfb" },
	{ 60, "\x7f\xfc" },
	{ 61, "\x20" },
	{ 62, "\xff\xb" },
	{ 63, "\x3f\xc" },
	{ 64, "\x1f\xfa" },
	{ 65, "\x21" },
	{ 66, "\x5d" },
	{ 67, "\x5e" },
	{ 68, "\x5f" },
	{ 69, "\x60" },
	{ 70, "\x61" },
	{ 71, "\x62" },
	{ 72, "\x63" },
	{ 73, "\x64" },
	{ 74, "\x65" },
	{ 75, "\x66" },
	{ 76, "\x67" },
	{ 77, "\x68" },
	{ 78, "\x69" },
	{ 79, "\x6a" },
	{ 80, "\x6b" },
	{ 81, "\x6c" },
	{ 82, "\x6d" },
	{ 83, "\x6e" },
	{ 84, "\x6f" },
	{ 85, "\x70" },
	{ 86, "\x71" },
	{ 87, "\x72" },
	{ 88, "\xfc" },
	{ 89, "\x73" },
	{ 90, "\xfd" },
	{ 91, "\x1f\xfb" },
	{ 92, "\x7f\xff\x0" },
	{ 93, "\x1f\xfc" },
	{ 94, "\x3f\xfc" },
	{ 95, "\x22" },
	{ 96, "\x7f\xfd" },
	{ 97, "\x3" },
	{ 98, "\x23" },
	{ 99, "\x4" },
	{ 100, "\x24" },
	{ 101, "\x5" },
	{ 102, "\x25" },
	{ 103, "\x26" },
	{ 104, "\x27" },
	{ 105, "\x6" },
	{ 106, "\x74" },
	{ 107, "\x75" },
	{ 108, "\x28" },
	{ 109, "\x29" },
	{ 110, "\x2a" },
	{ 111, "\x7" },
	{ 112, "\x2b" },
	{ 113, "\x76" },
	{ 114, "\x2c" },
	{ 115, "\x8" },
	{ 116, "\x9" },
	{ 117, "\x2d" },
	{ 118, "\x77" },
	{ 119, "\x78" },
	{ 120, "\x79" },
	{ 121, "\x7a" },
	{ 122, "\x7b" },
	{ 123, "\x7f\xfe" },
	{ 124, "\x7f\xc" },
	{ 125, "\x3f\xfd" },
	{ 126, "\x1f\xfd" },
	{ 127, "\xff\xff\xff\xc" },
	{ 128, "\xff\xfe\x6" },
	{ 129, "\x3f\xff\xd2" },
	{ 130, "\xff\xfe\x7" },
	{ 131, "\xff\xfe\x8" },
	{ 132, "\x3f\xff\xd3" },
	{ 133, "\x3f\xff\xd4" },
	{ 134, "\x3f\xff\xd5" },
	{ 135, "\x7f\xff\xd9" },
	{ 136, "\x3f\xff\xd6" },
	{ 137, "\x7f\xff\xda" },
	{ 138, "\x7f\xff\xdb" },
	{ 139, "\x7f\xff\xdc" },
	{ 140, "\x7f\xff\xdd" },
	{ 141, "\x7f\xff\xde" },
	{ 142, "\xff\xff\xeb" },
	{ 143, "\x7f\xff\xdf" },
	{ 144, "\xff\xff\xec" },
	{ 145, "\xff\xff\xed" },
	{ 146, "\x3f\xff\xd7" },
	{ 147, "\x7f\xff\xe0" },
	{ 148, "\xff\xff\xee" },
	{ 149, "\x7f\xff\xe1" },
	{ 150, "\x7f\xff\xe2" },
	{ 151, "\x7f\xff\xe3" },
	{ 152, "\x7f\xff\xe4" },
	{ 153, "\x1f\xff\xdc" },
	{ 154, "\x3f\xff\xd8" },
	{ 155, "\x7f\xff\xe5" },
	{ 156, "\x3f\xff\xd9" },
	{ 157, "\x7f\xff\xe6" },
	{ 158, "\x7f\xff\xe7" },
	{ 159, "\xff\xff\xef" },
	{ 160, "\x3f\xff\xda" },
	{ 161, "\x1f\xff\xdd" },
	{ 162, "\xff\xfe\x9" },
	{ 163, "\x3f\xff\xdb" },
	{ 164, "\x3f\xff\xdc" },
	{ 165, "\x7f\xff\xe8" },
	{ 166, "\x7f\xff\xe9" },
	{ 167, "\x1f\xff\xde" },
	{ 168, "\x7f\xff\xea" },
	{ 169, "\x3f\xff\xdd" },
	{ 170, "\x3f\xff\xde" },
	{ 171, "\xff\xff\xf0" },
	{ 172, "\x1f\xff\xdf" },
	{ 173, "\x3f\xff\xdf" },
	{ 174, "\x7f\xff\xeb" },
	{ 175, "\x7f\xff\xec" },
	{ 176, "\x1f\xff\xe0" },
	{ 177, "\x1f\xff\xe1" },
	{ 178, "\x3f\xff\xe0" },
	{ 179, "\x1f\xff\xe2" },
	{ 180, "\x7f\xff\xed" },
	{ 181, "\x3f\xff\xe1" },
	{ 182, "\x7f\xff\xee" },
	{ 183, "\x7f\xff\xef" },
	{ 184, "\xff\xfe\xa" },
	{ 185, "\x3f\xff\xe2" },
	{ 186, "\x3f\xff\xe3" },
	{ 187, "\x3f\xff\xe4" },
	{ 188, "\x7f\xff\xf0" },
	{ 189, "\x3f\xff\xe5" },
	{ 190, "\x3f\xff\xe6" },
	{ 191, "\x7f\xff\xf1" },
	{ 192, "\x3f\xff\xfe\x0" },
	{ 193, "\x3f\xff\xfe\x1" },
	{ 194, "\xff\xfe\xb" },
	{ 195, "\x7f\xff\x1" },
	{ 196, "\x3f\xff\xe7" },
	{ 197, "\x7f\xff\xf2" },
	{ 198, "\x3f\xff\xe8" },
	{ 199, "\x1f\xff\xfe\xc" },
	{ 200, "\x3f\xff\xfe\x2" },
	{ 201, "\x3f\xff\xfe\x3" },
	{ 202, "\x3f\xff\xfe\x4" },
	{ 203, "\x7f\xff\xfd\xe" },
	{ 204, "\x7f\xff\xfd\xf" },
	{ 205, "\x3f\xff\xfe\x5" },
	{ 206, "\xff\xff\xf1" },
	{ 207, "\x1f\xff\xfe\xd" },
	{ 208, "\x7f\xff\x2" },
	{ 209, "\x1f\xff\xe3" },
	{ 210, "\x3f\xff\xfe\x6" },
	{ 211, "\x7f\xff\xfe\x0" },
	{ 212, "\x7f\xff\xfe\x1" },
	{ 213, "\x3f\xff\xfe\x7" },
	{ 214, "\x7f\xff\xfe\x2" },
	{ 215, "\xff\xff\xf2" },
	{ 216, "\x1f\xff\xe4" },
	{ 217, "\x1f\xff\xe5" },
	{ 218, "\x3f\xff\xfe\x8" },
	{ 219, "\x3f\xff\xfe\x9" },
	{ 220, "\xff\xff\xff\xd" },
	{ 221, "\x7f\xff\xfe\x3" },
	{ 222, "\x7f\xff\xfe\x4" },
	{ 223, "\x7f\xff\xfe\x5" },
	{ 224, "\xff\xfe\xc" },
	{ 225, "\xff\xff\xf3" },
	{ 226, "\xff\xfe\xd" },
	{ 227, "\x1f\xff\xe6" },
	{ 228, "\x3f\xff\xe9" },
	{ 229, "\x1f\xff\xe7" },
	{ 230, "\x1f\xff\xe8" },
	{ 231, "\x7f\xff\xf3" },
	{ 232, "\x3f\xff\xea" },
	{ 233, "\x3f\xff\xeb" },
	{ 234, "\x1f\xff\xfe\xe" },
	{ 235, "\x1f\xff\xfe\xf" },
	{ 236, "\xff\xff\xf4" },
	{ 237, "\xff\xff\xf5" },
	{ 238, "\x3f\xff\xfe\xa" },
	{ 239, "\x7f\xff\xf4" },
	{ 240, "\x3f\xff\xfe\xb" },
	{ 241, "\x7f\xff\xfe\x6" },
	{ 242, "\x3f\xff\xfe\xc" },
	{ 243, "\x3f\xff\xfe\xd" },
	{ 244, "\x7f\xff\xfe\x7" },
	{ 245, "\x7f\xff\xfe\x8" },
	{ 246, "\x7f\xff\xfe\x9" },
	{ 247, "\x7f\xff\xfe\xa" },
	{ 248, "\x7f\xff\xfe\xb" },
	{ 249, "\xff\xff\xff\xe" },
	{ 250, "\x7f\xff\xfe\xc" },
	{ 251, "\x7f\xff\xfe\xd" },
	{ 252, "\x7f\xff\xfe\xe" },
	{ 253, "\x7f\xff\xfe\xf" },
	{ 254, "\x7f\xff\xff\x0" },
	{ 255, "\x3f\xff\xfe\xe" },
	{ 256, "\x3f\xff\xff\xff" },
}
