#ifndef _MIME_H
#define _MIME_H

#include <stddef.h>

enum fly_mime_type{
	/* text */
	text_plain,
	text_csv,
	text_html,
	text_css,
	text_xml,
	text_javascript,
	text_richtext,
	text_tab_separated_values,
	text_vnd_wap_wml,
	text_vnd_wap_wmlscript,
	text_x_hdml,
	text_x_setext,
	text_x_sgml,

	/* application */
	application_zip,
	application_x_lzh,
	application_x_tar,
	application_octet_stream,
	application_json,
	application_pdf,
	application_vnd_ms_excel,
	application_vnd_openxmlformats_officedocument_spreadsheetml_sheet,
	application_vnd_ms_powerpoint,
	application_vnd_openxmlformats_officedocument_presentationml_presentation,
	application_msword,
	application_vnd_openxmlformats_officedocument_wordprocessingml_document,
	application_rtf,
	application_mac_binhex40,
	application_java_archive,
	
	/* image */
	image_jpeg,
	image_png,
	image_gif,
	image_bmp,
	image_svg,
	image_ief,
	image_tiff,
	image_x_cmu_raster,
	image_x_freehand,
	image_x_portable_anymap,
	image_x_portable_bitmap,
	image_x_portable_graymap,
	imgea_x_portable_pixmap,
	image_x_rgb,
	image_x_xbitmap,
	image_x_xpixmap,
	image_x_xwindowdump,

	/* audio */
	audio_basic,
	audio_x_aiff,
	audio_x_midi,
	audio_x_pn_realaudio,
	audio_x_pn_realaudio_plugin,
	audio_x_twinvq,
	audio_x_wav,
	audio_x_m4a,
	audio_mpeg,
	audio_ogg,
	audio_midi,

	/* video */
	video_3gpp,
	video_mp2t,
	video_mp4,
	video_mpeg,
	video_webm,
	video_quicktime,
	video_x_fly,
	video_x_m4v,
	video_x_mng,
	video_x_msvideo,
	video_x_ms_asf,
	video_x_ms_wmv,
	video_x_sgi_movie,
};
typedef enum fly_mime_type fly_mime_e;

typedef char fly_mime_c;
typedef char fly_ext_t;
#define FLY_MIME_NAME_LENGTH		50
struct fly_mime{
	fly_mime_e type;
	fly_mime_c name[FLY_MIME_NAME_LENGTH];
	fly_ext_t **extensions;
};
typedef struct fly_mime fly_mime_t;
extern fly_mime_t mimes[];


fly_mime_t *fly_mime_from_type(fly_mime_e type);

#endif
