#ifndef _MIME_H
#define _MIME_H

#include <stddef.h>
#include <stdbool.h>
#include "alloc.h"
#include "util.h"

enum __fly_mime_list{
	/* text */
	fly_mime_text_plain,
	fly_mime_text_csv,
	fly_mime_text_html,
	fly_mime_text_css,
	fly_mime_text_xml,
	fly_mime_text_javascript,
	fly_mime_text_richtext,
	fly_mime_text_tab_separated_values,
	fly_mime_text_vnd_wap_wml,
	fly_mime_text_vnd_wap_wmlscript,
	fly_mime_text_x_hdml,
	fly_mime_text_x_setext,
	fly_mime_text_x_sgml,

	/* application */
	fly_mime_application_zip,
	fly_mime_application_x_lzh,
	fly_mime_application_x_tar,
	fly_mime_application_octet_stream,
	fly_mime_application_json,
	fly_mime_application_pdf,
	fly_mime_application_vnd_ms_excel,
	fly_mime_application_vnd_openxmlformats_officedocument_spreadsheetml_sheet,
	fly_mime_application_vnd_ms_powerpoint,
	fly_mime_application_vnd_openxmlformats_officedocument_presentationml_presentation,
	fly_mime_application_msword,
	fly_mime_application_vnd_openxmlformats_officedocument_wordprocessingml_document,
	fly_mime_application_rtf,
	fly_mime_application_mac_binhex40,
	fly_mime_application_java_archive,

	/* image */
	fly_mime_image_jpeg,
	fly_mime_image_png,
	fly_mime_image_gif,
	fly_mime_image_bmp,
	fly_mime_image_svg,
	fly_mime_image_ief,
	fly_mime_image_tiff,
	fly_mime_image_x_cmu_raster,
	fly_mime_image_x_freehand,
	fly_mime_image_x_portable_anymap,
	fly_mime_image_x_portable_bitmap,
	fly_mime_image_x_portable_graymap,
	fly_mime_imgea_x_portable_pixmap,
	fly_mime_image_x_rgb,
	fly_mime_image_x_xbitmap,
	fly_mime_image_x_xpixmap,
	fly_mime_image_x_xwindowdump,

	/* audio */
	fly_mime_audio_basic,
	fly_mime_audio_x_aiff,
	fly_mime_audio_x_midi,
	fly_mime_audio_x_pn_realaudio,
	fly_mime_audio_x_pn_realaudio_plugin,
	fly_mime_audio_x_twinvq,
	fly_mime_audio_x_wav,
	fly_mime_audio_x_m4a,
	fly_mime_audio_mpeg,
	fly_mime_audio_ogg,
	fly_mime_audio_midi,

	/* video */
	fly_mime_video_3gpp,
	fly_mime_video_mp2t,
	fly_mime_video_mp4,
	fly_mime_video_mpeg,
	fly_mime_video_webm,
	fly_mime_video_quicktime,
	fly_mime_video_x_fly,
	fly_mime_video_x_m4v,
	fly_mime_video_x_mng,
	fly_mime_video_x_msvideo,
	fly_mime_video_x_ms_asf,
	fly_mime_video_x_ms_wmv,
	fly_mime_video_x_sgi_movie,

	/* unknown */
	fly_mime_unknown,
	fly_mime_noextension,
};
typedef enum __fly_mime_list fly_mime_e;
#define __FLY_MTYPE_SET(type, subtype)	fly_mime_ ## type ## _ ## subtype, #type "/" #subtype

typedef char fly_mime_c;
typedef char fly_ext_t;
#define FLY_MIME_NAME_LENGTH		50
struct fly_mime_type{
	fly_mime_e type;
	fly_mime_c name[FLY_MIME_NAME_LENGTH];
	fly_ext_t **extensions;
};
typedef struct fly_mime_type fly_mime_type_t;

fly_mime_type_t *fly_mime_from_type(fly_mime_e type);

struct fly_request;
typedef struct fly_request fly_request_t;

#define FLY_ACCEPT_PARAM_MAXLEN			(30)
#define FLY_ACCEPT_EXT_MAXLEN			(30)
struct __fly_accept_param{
	char token_l[FLY_ACCEPT_PARAM_MAXLEN];
	char token_r[FLY_ACCEPT_PARAM_MAXLEN];

	struct __fly_accept_param		*next;
};
struct __fly_accept_ext{
	char token_l[FLY_ACCEPT_EXT_MAXLEN];
	char token_r[FLY_ACCEPT_EXT_MAXLEN];

	struct __fly_accept_ext		*next;
};

struct __fly_mime_type{
	enum {
		fly_mime_type_text,
		fly_mime_type_image,
		fly_mime_type_application,
		fly_mime_type_asterisk,
		fly_mime_type_unknown,
	} type;
	char *type_name;
};
#define FLY_MIME_TYPE(n)				(fly_mime_type_ ## n)
#define __FLY_MIME_TYPE(n)				{fly_mime_type_ ## n, #n}
#define __FLY_MIME_NULL					{ -1, NULL }
#define FLY_MIME_TYPE_MAXLEN				30
#define FLY_MIME_SUBTYPE_MAXLEN				30
struct __fly_mime_subtype{
	char subtype[FLY_MIME_SUBTYPE_MAXLEN];
	fly_bit_t asterisk: 1;
};
#define FLY_MIME_ASTERISK(am)			\
	do{													\
		(am)->type.type = FLY_MIME_TYPE(asterisk);		\
		(am)->subtype.asterisk = true;					\
		(am)->next = NULL;								\
	} while(0)

/*
 * quality default value is 100.
 */
struct fly_mime;
struct __fly_mime{
	struct fly_mime				*mime;
	struct __fly_mime_type		type;
	struct __fly_mime_subtype	subtype;
	/* 0~100% */
	int							 quality_value;
	/* parametess */
	struct __fly_accept_param	*params;
	int							parqty;
	/* extensions */
	struct __fly_accept_ext		*extension;
	int							extqty;

	struct __fly_mime			*next;
};
#define fly_same_type(m1, m2)		\
		(((m1)->type.type == (m2)->type.type) && (strcmp((m1)->subtype.subtype, (m2)->subtype.subtype)))

#define FLY_ACCEPT_HEADER		"Accept"

struct fly_mime{
	fly_pool_t					*pool;
	fly_request_t				*request;

	struct __fly_mime			*accepts;
	/* accept quantity */
	int							acqty;
};

typedef struct fly_mime fly_mime_t;
int fly_accept_mime(fly_request_t *request);

#define FLY_MIMQVALUE_MAXLEN	(6)
#define FLY_DOT					(0x2E)

fly_mime_type_t *fly_mime_type_from_path_name(char *path);
bool fly_mime_invalid(fly_mime_type_t *type);
#endif
