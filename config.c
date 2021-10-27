#include <assert.h>
#include <errno.h>
#include "char.h"
#include "config.h"

struct fly_config configs[] = {
#include "__fly.conf"
	FLY_CONFIG(NULL, NULL, NULL, -1)
};


static void fly_syntax_error_invalid_item_name(int lines, char *name, size_t name_len);
static void fly_syntax_error_no_name(int lines);
static void fly_syntax_error_no_value(int lines);
static void fly_set_config_value(int lines, struct fly_config *config, char *value, size_t value_len);

static inline char *fly_config_path(void)
{
	char *__p;

	__p = getenv(FLY_CONFIG_PATH);
	if (__p)
		return __p;
	else
		return (char *) FLY_CONFIG_DEFAULT_PATH;
}

FILE *fly_open_config_file(void)
{
	FILE *__cf;
	char *__path;

	__path = fly_config_path();
	__cf = fopen(__path, "r");

	return __cf;
}

struct fly_config *fly_config_item_search(char *item_name, size_t item_name_len)
{
	for (struct fly_config *__c=configs; __c->name; __c++){
		if (item_name_len == strlen(__c->name) && strncmp(item_name, __c->name, item_name_len) == 0)
			return __c;
	}

	/* not found item name */
	return NULL;
}

#define FLY_ENV_OVERWRITE					1
void fly_config_item_default_setting(void)
{
	for (struct fly_config *__c=configs; __c->name; __c++)
		assert(setenv(__c->env_name, __c->env_value, FLY_ENV_OVERWRITE) != -1);

	return;
}

#define FLY_PARSE_CONFIG_END_PROCESS(__e)	\
	do{															\
		fprintf(stderr, "end process by config parse error.\n");\
		exit(-1*__e);							\
	} while(0)
#define FLY_PARSE_CONFIG_NOTFOUND			(-2)
#define FLY_PARSE_CONFIG_ERROR				(-1)
#define FLY_PARSE_CONFIG_SUCCESS			(1)
#define FLY_PARSE_CONFIG_SYNTAX_ERROR		(0)
int fly_parse_config_file(void)
{
#define FLY_CONFIG_BUF_LENGTH				1024
	char config_buf[FLY_CONFIG_BUF_LENGTH];
#define FLY_CONFIG_BUF_LAST_PTR				(config_buf+FLY_CONFIG_BUF_LENGTH-1)
	FILE *__cf;
	int lines;
	enum {
		INIT,
		COMMENT,
		NEWLINE,
		NAME,
		NAME_END,
		EQUAL,
		VALUE,
		VALUE_END,
	} state;

	fly_config_item_default_setting();

	__cf = fly_open_config_file();
	if (__cf == NULL && errno == ENOENT)
		return FLY_PARSE_CONFIG_NOTFOUND;
	if (__cf == NULL)
		return FLY_PARSE_CONFIG_ERROR;

	lines = 0;
	memset(config_buf, '\0', FLY_CONFIG_BUF_LENGTH);
	while (fgets(config_buf, FLY_CONFIG_BUF_LENGTH, __cf)){
		lines++;
		char *ptr = config_buf, *name, *value;
		size_t name_len, value_len;
		struct fly_config *config;

		ptr = config_buf;
		state = INIT;
		name = NULL;
		name_len = 0;
		value = NULL;
		value_len = 0;
		while(true){
#define FLY_PARSE_CONFIG_SPACE(__ptr)					\
			(fly_space(*(__ptr)) || fly_ht(*(__ptr)))
#define FLY_PARSE_CONFIG_NAME_CHAR(__ptr)				\
			(fly_alpha(*(__ptr)) || fly_underscore(*(__ptr)))
#define FLY_PARSE_CONFIG_VALUE_CHAR(__ptr)				\
			(fly_alpha(*(__ptr)) || fly_numeral(*(__ptr)) || \
			 fly_dot(*(__ptr)))
			switch(state){
			case INIT:
				if (FLY_PARSE_CONFIG_SPACE(ptr)){
					ptr++;
					break;
				} else if (FLY_PARSE_CONFIG_NAME_CHAR(ptr)){
					state = NAME;
					name = ptr;
					break;
				} else if (fly_sharp(*ptr)){
					state = COMMENT;
					break;
				} else if (fly_cr(*ptr)){
					ptr++;
					break;
				} else if (fly_lf(*ptr)){
					state = NEWLINE;
					break;
				}

				goto syntax_error;
			case COMMENT:
				goto comment;
			case NEWLINE:
				goto newline;
			case NAME:
				if (FLY_PARSE_CONFIG_SPACE(ptr)){
					name_len = ptr - name;
					state = NAME_END;
					break;
				} else if (FLY_PARSE_CONFIG_NAME_CHAR(ptr)){
					ptr++;
					break;
				}

				goto syntax_error;
			case NAME_END:
				if (FLY_PARSE_CONFIG_SPACE(ptr)){
					ptr++;
					break;
				}else if (fly_equal(*ptr)){
					ptr++;
					state = EQUAL;
					break;
				}

				goto syntax_error;
			case EQUAL:
				if (FLY_PARSE_CONFIG_SPACE(ptr)){
					ptr++;
					break;
				}else if (FLY_PARSE_CONFIG_VALUE_CHAR(ptr)){
					state = VALUE;
					value = ptr;
					break;
				}

				goto syntax_error;
			case VALUE:
				if (FLY_PARSE_CONFIG_VALUE_CHAR(ptr)){
					ptr++;
					break;
				} else if (FLY_PARSE_CONFIG_SPACE(ptr)){
					value_len = ptr-value;
					state = VALUE_END;
					break;
				} else if (fly_cr(*ptr) || fly_lf(*ptr)){
					value_len = ptr-value;
					state = VALUE_END;
					break;
				}

				goto syntax_error;
			case VALUE_END:
				goto end_line;
			default:
				FLY_NOT_COME_HERE
			}

			if (ptr == FLY_CONFIG_BUF_LAST_PTR)
				break;
		}

end_line:
		/* syntax check */
		if (!name || name_len == 0)
			fly_syntax_error_no_name(lines);
		if (!value || value_len==0)
			fly_syntax_error_no_value(lines);

		config = fly_config_item_search(name, name_len);
		if (config != NULL){
			fly_set_config_value(lines, config, value, value_len);
		}else{
			/* unknown item name */
			fly_syntax_error_invalid_item_name(lines, name, name_len);
			goto syntax_error;
		}

comment:
newline:
		memset(config_buf, '\0', FLY_CONFIG_BUF_LENGTH);
	}

	if (feof(__cf)){
		return FLY_PARSE_CONFIG_SUCCESS;
	}else
		goto error;
syntax_error:
	FLY_PARSE_CONFIG_END_PROCESS(FLY_PARSE_CONFIG_ERROR);
error:
	FLY_PARSE_CONFIG_END_PROCESS(FLY_PARSE_CONFIG_SYNTAX_ERROR);
}

#define FLY_CONFIG_PARSE_ERROR_STRING(__l)				\
	do{													\
		char *__fpath = fly_config_path();				\
		fprintf(stderr, "config file parse error(%s,line. %d): ", __fpath, __l);	\
	}while(0)

static void fly_syntax_error_invalid_item_name(int lines, char *name, size_t name_len)
{
	name[name_len] = '\0';

	FLY_CONFIG_PARSE_ERROR_STRING(lines);
	fprintf(stderr, "invalid item name(%s)\n", name);
}

static void fly_syntax_error_no_name(int lines)
{
	FLY_CONFIG_PARSE_ERROR_STRING(lines);
	fprintf(stderr, "no name\n");
}

static void fly_syntax_error_no_value(int lines)
{
	FLY_CONFIG_PARSE_ERROR_STRING(lines);
	fprintf(stderr, "no value\n");
}

static void fly_syntax_error_invalid_value(int lines, char *value, size_t value_len, struct fly_config *config)
{
	value[value_len] = '\0';

	FLY_CONFIG_PARSE_ERROR_STRING(lines);
	fprintf(stderr, "invalid value(%s). ", value);

	switch (config->flag){
	case FLY_CONFIG_INTEGER:
		fprintf(stderr, "\"%s\" must be integer.\n", config->name);
		break;
	case FLY_CONFIG_STRING:
		fprintf(stderr, "\"%s\" must be string.\n", config->name);
		break;
	case FLY_CONFIG_BOOL:
		fprintf(stderr, "\"%s\" must be bool(true or false).\n", config->name);
		break;
	default:
		FLY_NOT_COME_HERE;
	}
}

static void fly_set_config_value(int lines, struct fly_config *config, char *value, size_t value_len)
{
	value[value_len] = '\0';

	switch (config->flag){
	case FLY_CONFIG_INTEGER:
		if (atoi(value) == 0 && (*value != 0x30)){
			fly_syntax_error_invalid_value(lines, value, value_len, config);
			FLY_PARSE_CONFIG_END_PROCESS(FLY_PARSE_CONFIG_SYNTAX_ERROR);
		}
		break;
	case FLY_CONFIG_BOOL:
		if (strncmp(value, FLY_CONFIG_BOOL_TRUE, strlen(FLY_CONFIG_BOOL_TRUE)) && \
				strncmp(value, FLY_CONFIG_BOOL_FALSE, strlen(FLY_CONFIG_BOOL_FALSE))){
			fly_syntax_error_invalid_value(lines, value, value_len, config);
			FLY_PARSE_CONFIG_END_PROCESS(FLY_PARSE_CONFIG_SYNTAX_ERROR);
		}
		break;
	case FLY_CONFIG_STRING:
		break;
	default:
		FLY_NOT_COME_HERE;
	}
	if (setenv(config->env_name, value, FLY_ENV_OVERWRITE) == -1)
		FLY_PARSE_CONFIG_END_PROCESS(FLY_PARSE_CONFIG_SYNTAX_ERROR);
}

char *fly_config_value_str(char *name)
{
	char *env_value;
	for (struct fly_config *__c=configs; __c->name; __c++){
		if (strlen(name) == strlen(__c->env_name) && strncmp(name, __c->env_name, strlen(name)) == 0){
			env_value = getenv(name);
			assert(env_value != NULL);
			return env_value;
		}
	}
	FLY_NOT_COME_HERE
}

int fly_config_value_int(char *name)
{
	char *env_value;
	for (struct fly_config *__c=configs; __c->name; __c++){
		if (strlen(name) == strlen(__c->env_name) && strncmp(name, __c->env_name, strlen(name)) == 0){
			env_value = getenv(name);
			assert(env_value != NULL);
			return atoi(env_value);
		}
	}
	FLY_NOT_COME_HERE
}

bool fly_config_value_bool(char *name)
{
	char *env_value;
	for (struct fly_config *__c=configs; __c->name; __c++){
		if (strlen(name) == strlen(__c->env_name) && strncmp(name, __c->env_name, strlen(name)) == 0){
			env_value = getenv(name);
			assert(env_value != NULL);
			if (strncmp(env_value, FLY_CONFIG_BOOL_TRUE, strlen(FLY_CONFIG_BOOL_TRUE)) == 0)
				return true;
			else
				return false;
		}
	}
	FLY_NOT_COME_HERE
}
