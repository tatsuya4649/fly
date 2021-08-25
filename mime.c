#include "mime.h"
#include "util.h"

fly_mime_t mimes[] = {
	{text_plain, "text/plain", FLY_STRING_ARRAY("txt", NULL)},
	{0, "", NULL}
};

fly_mime_t *fly_mime_from_type(fly_mime_e type)
{
	for (fly_mime_t *m=mimes; m->extensions!=NULL; m++){
		if (m->type == type)
			return m;
	}
	return NULL;
}
