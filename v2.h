#ifndef _V2_H
#define _V2_H

#include <stdint.h>
#include "alloc.h"
#include "util.h"
#include "event.h"
#include "request.h"
#include "bllist.h"

#define FLY_CONNECTION_PREFACE				\
	("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n")

#define FLY_SEND_FRAME_TIMEOUT				(5)
/*
 *	HTTP2 Frame Header:
 *	@ Length:    24bit
 *	@ Type:      8bit
 *	@ Flags:     8bit
 *	@ R:         1bit
 *	@ Stream ID: 31bit
 *
 */
/* unit: octet */
#define FLY_HV2_FRAME_HEADER_LENGTH			(9)
#define FLY_HV2_OCTET_LEN					(8)
/* unit: bit */
#define FLY_HV2_FRAME_HEADER_LENGTH_LEN		(24)
#define FLY_HV2_FRAME_HEADER_TYPE_LEN		(8)
#define FLY_HV2_FRAME_HEADER_FLAGS_LEN		(8)
#define FLY_HV2_FRAME_HEADER_R_LEN			(1)
#define FLY_HV2_FRAME_HEADER_SID_LEN		(31)

typedef uint32_t fly_settings_t;
typedef uint32_t fly_sid_t;
typedef fly_bit_t fly_settings_bit_t;
typedef char fly_hv2_frame_header_t[FLY_HV2_FRAME_HEADER_LENGTH];

struct fly_hv2_frame{
	uint8_t		type;
	uint32_t	length;
	uint8_t		flags;
	struct fly_hv2_stream *stream;
	uint8_t *payload;
	/* frames queue element */
	struct fly_queue			felem;
};

struct fly_hv2_stream;
struct fly_hv2_send_frame{
	struct fly_hv2_stream			*stream;
	fly_pool_t						*pool;
	fly_sid_t						sid;
	fly_hv2_frame_header_t			frame_header;
	uint8_t							*payload;
	uint8_t 						type;
	size_t							payload_len;
	size_t 							send_len;
	size_t 							can_send_len;
	enum {
		FLY_HV2_SEND_FRAME_FASE_FRAME_HEADER,
		FLY_HV2_SEND_FRAME_FASE_PAYLOAD,
		FLY_HV2_SEND_FRAME_FASE_END
	}								send_fase;

	/* send frame queue element */
	struct fly_queue				qelem;
	/* required ack send frame queue element */
	struct fly_bllist				aqelem;
	/* send frame queue of state element */
	struct fly_queue				sqelem;
	fly_bit_t						need_ack:	1;
};

/*
 *	HTTP2 Connection State
 */
enum fly_hv2_connection_state{
	FLY_HV2_CONNECTION_STATE_INIT,
	FLY_HV2_CONNECTION_STATE_CONNECTION_PREFACE,
	FLY_HV2_CONNECTION_STATE_COMMUNICATION,
	FLY_HV2_CONNECTION_STATE_END,
};

struct fly_hv2_response{
	fly_response_t		*response;
	struct fly_queue	qelem;
};

struct fly_hv2_state{
	fly_pool_t						*pool;
	fly_connect_t					*connect;
	enum fly_hv2_connection_state	connection_state;
	int								stream_count;
	int								reserved_count;
	fly_sid_t						max_sid;
	fly_sid_t						max_handled_sid;
	struct fly_bllist				streams;
	/* last handle stream */
	struct fly_hv2_stream			*lhstream;
	struct fly_queue				responses;
	int								response_count;
	struct fly_queue				reserved;

	struct fly_hv2_dynamic_table	*dtable;
	struct fly_hv2_dynamic_table	*ldtable;
	size_t							dtable_entry_count;
	size_t							dtable_size;
	size_t							dtable_max_index;

	/* send list */
	struct fly_queue				send;
	int								send_count;

	/* use when memory is low */
	void							*emergency_ptr;

	ssize_t							 window_size;
	/* connection setting value by SETTINGS frame */
#define FLY_HV2_HEADER_TABLE_SIZE_DEFAULT		(4096)	/* unit: octet*/
	fly_settings_t					p_header_table_size;
	fly_settings_t					header_table_size;
#define FLY_HV2_MAX_CONCURRENT_STREAMS_DEFAULT	(-1)	/* no limit */
#define FLY_HV2_MAX_CONCURRENT_STREAMS_INFINITY	(-1)	/* no limit */
	fly_settings_t					p_max_concurrent_streams;
	fly_settings_t					max_concurrent_streams;
#define FLY_HV2_INITIAL_WINDOW_SIZE_DEFAULT		((1<<16)-1) /* 65535 octet*/
#define FLY_HV2_WINDOW_SIZE_MAX					(((uint32_t) (1<<31))-1) /* 2147483648 octet*/

	fly_settings_t					p_initial_window_size;
	fly_settings_t					initial_window_size;
#define FLY_HV2_MAX_FRAME_SIZE_DEFAULT			(1<<14) /*16384 octet*/
#define FLY_HV2_MAX_FRAME_SIZE_MAX				((1<<24)-1) /*16777215 octet*/
	fly_settings_t					p_max_frame_size;
	fly_settings_t					max_frame_size;
#define FLY_HV2_MAX_HEADER_LIST_SIZE_DEFAULT	(-1)	/* no limit */
	fly_settings_t					p_max_header_list_size;
	fly_settings_t					max_header_list_size;
#define FLY_HV2_ENABLE_PUSH_DEFAULT				(1)
	fly_settings_t					p_enable_push;
	fly_settings_t 					enable_push;

	/* after received GOAWAY*/
	fly_sid_t						goaway_lsid;
	fly_bit_t 						goaway: 1;
	fly_bit_t 						first_send_settings: 1;
};
typedef struct fly_hv2_state fly_hv2_state_t;
typedef struct fly_hv2_frame fly_hv2_frame_t;


/*
 * Frame Payload Size: SETTINGS_MAX_FRAME_SIZE(2^14~2^24-1 octet)
 */
enum fly_hv2_stream_state{
	FLY_HV2_STREAM_STATE_IDLE,
	FLY_HV2_STREAM_STATE_RESERVED_LOCAL,
	FLY_HV2_STREAM_STATE_RESERVED_REMOTE,
	FLY_HV2_STREAM_STATE_OPEN,
	FLY_HV2_STREAM_STATE_HALF_CLOSED_LOCAL,
	FLY_HV2_STREAM_STATE_HALF_CLOSED_REMOTE,
	FLY_HV2_STREAM_STATE_CLOSED,
};


struct fly_hv2_stream{
#define FLY_HV2_STREAM_ROOT_ID			(0x0)
	fly_sid_t						id;
	fly_sid_t						dependency_id;
	struct fly_hv2_state			*state;
	enum fly_hv2_stream_state		stream_state;
	struct fly_bllist				blelem;
	struct fly_queue				rqelem;
	fly_request_t					*request;
	int								dep_count;
	struct fly_hv2_stream			*deps;
	struct fly_hv2_stream			*dnext;
	struct fly_hv2_stream			*dprev;
	ssize_t							window_size;
	struct fly_queue				frames;
	int 							frame_count;
	/* sent frame that rave not yet received ack */
	struct fly_bllist				yetack;
	int								yetack_count;
	struct fly_queue				yetsend;
	int								yetsend_count;
	uint16_t						weight;

	fly_bit_t						from_client: 1;
	fly_bit_t 						reserved: 1;
	fly_bit_t 						exclusive: 1;
	fly_bit_t 						peer_end_headers: 1;
	fly_bit_t 						can_response: 1;
	fly_bit_t 						end_send_headers: 1;
	fly_bit_t 						end_send_data: 1;
	fly_bit_t 						end_request_response: 1;
};
typedef struct fly_hv2_stream fly_hv2_stream_t;
#define FLY_HV2_ROOT_STREAM(state)			\
		fly_queue_data((state)->streams.next, struct fly_hv2_stream, blelem)

/* 1~256 */
#define FLY_HV2_STREAM_DEFAULT_WEIGHT		(16)

enum fly_hv2_error_type{
	FLY_HV2_CONNECTION_ERROR,
	FLY_HV2_STREAM_ERROR,
};

typedef uint8_t fly_hv2_frame_type_t;
/*
 *	DATA Frame:
 *	@Pad Length: 8bit
 *	@DATA:
 *	@Padding:
 *
 *	Flag:
 *	@END_STREAM:	bit0
 *	@PADDED:		bit3
 *
 *	when only "open", "half-closed(remote)" state, data frame can send.
 */
#define FLY_HV2_FRAME_TYPE_DATA					(0x0)
#define FLY_HV2_FRAME_TYPE_DATA_END_STREAM		(1<<0)
#define FLY_HV2_FRAME_TYPE_DATA_PADDED			(1<<3)
#define FLY_HV2_FRAME_TYPE_DATA_PAD_LENGTH_LEN	(8)

/*
 *	HEADERS Frame:
 *	@Pad Length: 8bit
 *	@E:			 1bit
 *	@Stream Dependency: 31bit
 *	@Weight:			8bit	(0~255)
 *	@Header Block Fragment:
 *	@Padding:
 *
 *	Flag:
 *	@END_STREAM		bit0
 *	@END_HEADERS	bit2
 *	@PADDED			bit3
 *	@PRIORITY		bit5
 */
#define FLY_HV2_FRAME_TYPE_HEADERS				(0x1)
#define FLY_HV2_FRAME_TYPE_HEADERS_PAD_LENGTH_LEN	(8)
#define FLY_HV2_FRAME_TYPE_HEADERS_E_LEN		(1)
#define FLY_HV2_FRAME_TYPE_HEADERS_SID_LEN		(31)
#define FLY_HV2_FRAME_TYPE_HEADERS_WEIGHT_LEN	(8)
#define FLY_HV2_FRAME_TYPE_HEADERS_END_STREAM	(1<<0)
#define FLY_HV2_FRAME_TYPE_HEADERS_END_HEADERS	(1<<2)
#define FLY_HV2_FRAME_TYPE_HEADERS_PADDED		(1<<3)
#define FLY_HV2_FRAME_TYPE_HEADERS_PRIORITY		(1<<5)

/*
 *	PRIORITY Frame:
 *	@E:					1bit
 *	@Stream Dependency: 31bit
 *	@Weight:			8bit
 *
 *	No Flag
 *
 *	when all stream state, priority frame can send.
 */
#define FLY_HV2_FRAME_TYPE_PRIORITY				(0x2)
#define FLY_HV2_FRAME_TYPE_PRIORITY_LENGTH		(5)		/* unit: octet*/
#define FLY_HV2_FRAME_TYPE_PRIORITY_E_LEN		(1)
#define FLY_HV2_FRAME_TYPE_PRIORITY_STREAM_DEPENDENCY_LEN	(31)
#define FLY_HV2_FRAME_TYPE_PRIORITY_WEIGHT_LEN				(8)

/*
 *	RST_STREAM Frame:
 *	@Error Code:		32bit
 *
 *	No Flag
 *
 *	when idle state, rst_stream frame can't send.
 */
#define FLY_HV2_FRAME_TYPE_RST_STREAM			(0x3)
#define FLY_HV2_FRAME_TYPE_RST_STREAM_LENGTH	(4) /* unit: octet */

/*
 *	SETTINGS Frame:
 *	@Identifier:		16bit
 *	@Value:				32bit
 *
 *	Flag:
 *	@ACK				bit0
 */
#define FLY_HV2_FRAME_TYPE_SETTINGS				(0x4)
#define FLY_HV2_FRAME_TYPE_SETTINGS_LENGTH		(6)		/* unit: octet */
#define FLY_HV2_FRAME_TYPE_SETTINGS_ID_LEN			(16)
#define FLY_HV2_FRAME_TYPE_SETTINGS_VALUE_LEN		(32)
#define FLY_HV2_FRAME_TYPE_SETTINGS_FLAG_ACK		(0x1)

#define FLY_HV2_SETTINGS_FRAME_SETTINGS_HEADER_TABLE_SIZE		(0x1)
#define FLY_HV2_SETTINGS_FRAME_SETTINGS_ENABLE_PUSH				(0x2)
#define FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_CONCURRENT_STREAMS	(0x3)
#define FLY_HV2_SETTINGS_FRAME_SETTINGS_INITIAL_WINDOW_SIZE		(0x4)
#define FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_FRAME_SIZE			(0x5)
#define FLY_HV2_SETTINGS_FRAME_SETTINGS_MAX_HEADER_LIST_SIZE		(0x6)

/*
 *	PUSH_PROMISE Frame:
 *	@Pad Length:			8bit
 *	@R:						1bit
 *	@Promised Stream ID:	31bit
 *	@Header Block Fragment:
 *	@Padding:
 *
 *	Flag:
 *	@END_HEADERS:		bit2
 *	@PADDED:			bit3
 *
 *	when only "open", "half-closed(remote)" state, push_promise frame can send.
 */
#define FLY_HV2_FRAME_TYPE_PUSH_PROMISE			(0x5)
#define FLY_HV2_FRAME_TYPE_PUSH_PROMISE_END_HEADERS		(1<<2)
#define FLY_HV2_FRAME_TYPE_PUSH_PROMISE_PADDED			(1<<3)

/*
 *	PING Frame:
 *	@Opaque Data:			64bit
 *
 *	Flag:
 *	@ACK				bit0
 *
 */
#define FLY_HV2_FRAME_TYPE_PING					(0x6)
#define FLY_HV2_FRAME_TYPE_PING_OPEQUE_DATA_LEN	(64)
#define FLY_HV2_FRAME_TYPE_PING_LENGTH			(8)	 /* unit: octet */

/*
 *	GOAWAY Frame:
 *	@R:						1bit
 *	@Last-Stream-ID:		31bit
 *	@Error Code:			32bit
 *	@Additional Debug Data:
 *
 *	No Flag
 */
#define FLY_HV2_FRAME_TYPE_GOAWAY				(0x7)
#define FLY_HV2_FRAME_TYPE_GOAWAY_R_LEN			(1)
#define FLY_HV2_FRAME_TYPE_GOAWAY_LSID_LEN		(31)
#define FLY_HV2_FRAME_TYPE_GOAWAY_ERROR_CODE_LEN			(32)

/*
 *	WINDOW_UPDATE Frame:
 *	@R:						1bit
 *	@Window Size Increment: 31bit
 *
 *	No Flag
 */
#define FLY_HV2_FRAME_TYPE_WINDOW_UPDATE		(0x8)
#define FLY_HV2_FRAME_TYPE_WINDOW_UPDATE_LENGTH		(4)

/*
 *	CONTINUATION Frame:
 *	@Header Block Fragment:	32bit
 *
 *	Flag:
 *	@END_HEADERS:			bit2
 */
#define FLY_HV2_FRAME_TYPE_CONTINUATION			(0x9)
#define FLY_HV2_FRAME_TYPE_CONTINUATION_END_HEADERS		(1<<2)

/*
 *	Error code
 */
#define FLY_HV2_NO_ERROR						(0x0)
#define FLY_HV2_PROTOCOL_ERROR					(0x1)
#define FLY_HV2_INTERNAL_ERROR					(0x2)
#define FLY_HV2_FLOW_CONTROL_ERROR				(0x3)
#define FLY_HV2_SETTINGS_TIMEOUT				(0x4)
#define FLY_HV2_STREAM_CLOSED					(0x5)
#define FLY_HV2_FRAME_SIZE_ERROR				(0x6)
#define FLY_HV2_REFUSED_STREAM					(0x7)
#define FLY_HV2_CANCEL							(0x8)
#define FLY_HV2_COMPRESSION_ERROR				(0x9)
#define FLY_HV2_CONNECT_ERROR					(0xa)
#define FLY_HV2_ENHANCE_YOUR_CALM				(0xb)
#define FLY_HV2_INADEQUATE_SECURITY				(0xc)
#define FLY_HV2_HTTP_1_1_REQUIRED				(0xd)


/* Request Pseudo Header Field */
#define FLY_HV2_REQUEST_PSEUDO_HEADER_METHOD	":method"
#define FLY_HV2_REQUEST_PSEUDO_HEADER_SCHEME	":scheme"
#define FLY_HV2_REQUEST_PSEUDO_HEADER_AUTHORITY	":authority"
#define FLY_HV2_REQUEST_PSEUDO_HEADER_PATH		":path"

/* Response Pseudo Header Field */
#define FLY_HV2_RESPONSE_PSEUDO_HEADER_STATUS	":status"


int fly_hv2_init_handler(fly_event_t *e);

struct fly_hv2_static_table{
	int index;
	const char *hname;
	const char *hvalue;
};

/*
 *	duplicatable entry.
 */
struct fly_hv2_dynamic_table{
	/* no huffman encoding */
	char		*hname;
	size_t		 hname_len;
	char		*hvalue;
	size_t		 hvalue_len;
	size_t		 entry_size;

	struct fly_hv2_dynamic_table *prev;
	struct fly_hv2_dynamic_table *next;
};

/* 0:	H
 * 1~7: String Length (7+)
 */
typedef uint8_t fly_hpack_string_header;
typedef uint8_t fly_hpack_string_data;
/* can't use index 0. */
typedef uint8_t fly_hpack_index_header_field;

enum fly_hv2_index_type{
	INDEX_UPDATE,
	INDEX_NOUPDATE,
	NOINDEX,
};
#define FLY_HV2_INDEX_PREFIX_BIT			7
#define FLY_HV2_NAME_PREFIX_BIT				7
#define FLY_HV2_VALUE_PREFIX_BIT			7
#define FLY_HV2_LITERAL_UPDATE_PREFIX_BIT			6
#define FLY_HV2_LITERAL_NOUPDATE_PREFIX_BIT			4
#define FLY_HV2_LITERAL_NOINDEX_PREFIX_BIT			4

extern struct fly_hv2_static_table static_table[];
#define FLY_HV2_STATIC_TABLE_LENGTH			\
	((int) sizeof(static_table)/sizeof(struct fly_hv2_static_table))
int fly_header_add_v2(fly_hdr_ci *chain_info, fly_hdr_name *name, int name_len, fly_hdr_value *value, int value_len, bool beginning);

#endif
