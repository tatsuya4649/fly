
enum mime_type{
	discrete,
	multipart
};


typedef struct{
	enum mime_type type;
	char name[30];
	char subname[30];
} mime;

