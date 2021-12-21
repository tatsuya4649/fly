#include <assert.h>
#include <errno.h>
#include <limits.h>
#include "char.h"
#include "conf.h"

struct fly_config configs[] = {
#include "../fly.conf"
	FLY_CONFIG(NULL, NULL, NULL, -1)
};


static void fly_set_config_value(int lines, struct fly_config *config, char *value, size_t value_len);

static inline char *fly_config_path(void)
{
	char *__p;

	__p = getenv(FLY_CONFIG_PATH);
	return __p;
}

FILE *fly_open_config_file(void)
{
	FILE *__cf;
	char *__path;

	__path = fly_config_path();
	if (__path == NULL)
		return NULL;
	__cf = fopen(__path, "r");

#ifdef DEBUG
	printf("CONFIGURE PATH: %s\n",__path);
#endif
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
		if (__c->env_value)
			assert(setenv(__c->env_name, __c->env_value, FLY_ENV_OVERWRITE) != -1);
		else
			assert(unsetenv(__c->env_name) != -1);

	return;
}

#define FLY_PARSE_CONFIG_END_PROCESS(__e)	\
	do{															\
		fprintf(stderr, "end process by config parse error.\n");\
		exit(-1*__e);							\
	} while(0)
int fly_parse_config_file(struct fly_err *err)
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
	errno = 0;
	if ((__cf == NULL && errno == ENOENT) || (__cf == NULL && errno == 0)){
		if (err != NULL){
			fly_error(
				err, errno,
				FLY_ERR_WARN,
				"Not found config file(%s).",
				fly_config_path() != NULL ? fly_config_path() : ""
			);
		}
		return FLY_PARSE_CONFIG_NOTFOUND;
	}
	if (__cf == NULL){
		if (err != NULL){
			fly_error(
				err, errno,
				FLY_ERR_ERR,
				"Open config file error(%s). %s",
				fly_config_path() != NULL ? fly_config_path() : "",
				strerror(errno)
			);
		}
		return FLY_PARSE_CONFIG_ERROR;
	}

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
			 fly_dot(*(__ptr)) || fly_slash(*(__ptr)) || \
			 fly_minus(*(__ptr)) || fly_underscore(*(__ptr)))
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

				if (err != NULL){
					fly_error(
						err, errno, FLY_ERR_ERR,
						"Syntax error in configure file (%s). Line %d: \"%s\". Invalid character(\"%c\")",
						fly_config_path() != NULL ? fly_config_path() : "",
						lines, config_buf, *ptr
					);
				}
				goto syntax_error;
			case COMMENT:
				goto comment;
			case NEWLINE:
				goto newline;
			case NAME:
				if (FLY_PARSE_CONFIG_SPACE(ptr)){
#ifdef DEBUG
					printf("NAME-PARSE\n");
#endif
					name_len = ptr - name;
					state = NAME_END;
					break;
				} else if (fly_equal(*ptr)){
					name_len = ptr - name;
#ifdef DEBUG
					printf("NAME-EQUAL. name length %ld\n", name_len);
#endif
					ptr++;
					state = EQUAL;
					break;
				} else if (FLY_PARSE_CONFIG_NAME_CHAR(ptr)){
					ptr++;
					break;
				}

				if (err != NULL){
					fly_error(
						err, errno, FLY_ERR_ERR,
						"Syntax error in configure file (%s). Line %d: %d, \"%s\". Invalid name character(\"%c\")",
						fly_config_path() != NULL ? fly_config_path() : "",
						lines, ptr-config_buf+1, config_buf, *ptr
					);
				}
				goto syntax_error;
			case NAME_END:
				if (FLY_PARSE_CONFIG_SPACE(ptr)){
#ifdef DEBUG
					printf("NAME_END-SPACE.\n");
#endif
					ptr++;
					break;
				}else if (fly_equal(*ptr)){
#ifdef DEBUG
					printf("NAME_END-EQUAL. name length %ld\n", name_len);
#endif
					ptr++;
					state = EQUAL;
					break;
				}

				if (err != NULL){
					fly_error(
						err, errno, FLY_ERR_ERR,
						"Syntax error in configure file (%s). Line %d: %d, \"%s\". Invalid name end character(\"%c\"). Must be equal or space.",
						fly_config_path() != NULL ? fly_config_path() : "",
						lines, ptr-config_buf+1, config_buf, *ptr
					);
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

				if (err != NULL){
					fly_error(
						err, errno, FLY_ERR_ERR,
						"Syntax error in configure file (%s). Line %d: %d, \"%s\". Invalid character(\"%c\"). Must be space or value character.",
						fly_config_path() != NULL ? fly_config_path() : "",
						lines, ptr-config_buf+1, config_buf, *ptr
					);
				}
				goto syntax_error;
			case VALUE:
				if (FLY_PARSE_CONFIG_VALUE_CHAR(ptr)){
					ptr++;
					break;
				} else if (FLY_PARSE_CONFIG_SPACE(ptr)){
					value_len = ptr-value;
					state = VALUE_END;
#ifdef DEBUG
					printf("VALUE: value len %ld\n", value_len);
#endif
					break;
				} else if (fly_cr(*ptr) || fly_lf(*ptr)){
					value_len = ptr-value;
#ifdef DEBUG
					printf("VALUE: value len %ld\n", value_len);
#endif
					state = VALUE_END;
					break;
				}

				if (err != NULL){
					fly_error(
						err, errno, FLY_ERR_ERR,
						"Syntax error in configure file (%s). Line %d: %d, \"%s\". Invalid value character(\"%c\").",
						fly_config_path() != NULL ? fly_config_path() : "",
						lines, ptr-config_buf+1, config_buf, *ptr
					);
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
		;
#ifdef DEBUG
		printf("PARSE RESULT=> name: %.*s, value=: %.*s|\n", (int) name_len, name, (int) value_len, value);
#endif
		char *__cptr=config_buf;
		while(!fly_cr(*__cptr) && !fly_lf(*__cptr))
			__cptr++;
		*__cptr = '\0';
		/* syntax check */
		if (!name || name_len == 0){
			if (err != NULL){
				fly_error(
					err, errno, FLY_ERR_ERR,
					"Parse error in configure file (%s). Line %d, \"%s\". Not found name of configure item.",
					fly_config_path() != NULL ? fly_config_path() : "",
					lines, config_buf
				);
			}
			goto syntax_error;
		}
		if (!value || value_len==0){
			if (err != NULL){
				fly_error(
					err, errno, FLY_ERR_ERR,
					"Parse error in configure file (%s). Line %d, \"%s\". Not found value of configure item.",
					fly_config_path() != NULL ? fly_config_path() : "",
					lines, config_buf
				);
			}
			goto syntax_error;
		}

		config = fly_config_item_search(name, name_len);
		if (config != NULL){
			fly_set_config_value(lines, config, value, value_len);
		}else{
			/* unknown item name */
			if (err != NULL){
				fly_error(
					err, errno, FLY_ERR_ERR,
					"Parse error in configure file (%s). Line %d, \"%s\". Unknown configure item.",
					fly_config_path() != NULL ? fly_config_path() : "",
					lines, config_buf
				);
			}
			goto syntax_error;
		}

		goto newline;
comment:
newline:
		memset(config_buf, '\0', FLY_CONFIG_BUF_LENGTH);
	}

	if (feof(__cf)){
#ifdef DEBUG
		printf("END CONFIGURE FILE PARSE\n");
#endif
		return FLY_PARSE_CONFIG_SUCCESS;
	}else
		goto error;
syntax_error:
	return FLY_PARSE_CONFIG_ERROR;
error:
	if (err != NULL){
		fly_error(
			err, errno,
			FLY_ERR_ERR,
			"Parse config file error(%s). %s",
			fly_config_path() != NULL ? fly_config_path() : "",
			strerror(errno)
		);
	}
	return FLY_PARSE_CONFIG_SYNTAX_ERROR;
}

#define FLY_CONFIG_PARSE_ERROR_STRING(__l)				\
	do{													\
		char *__fpath = fly_config_path();				\
		fprintf(stderr, "Config file parse error(%s,line. %d): ", __fpath, __l);	\
	}while(0)

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
				strncmp(value, FLY_CONFIG_BOOL_UPPER_TRUE, strlen(FLY_CONFIG_BOOL_UPPER_TRUE)) && \
				strncmp(value, FLY_CONFIG_BOOL_FALSE, strlen(FLY_CONFIG_BOOL_FALSE)) && \
				strncmp(value, FLY_CONFIG_BOOL_UPPER_FALSE, strlen(FLY_CONFIG_BOOL_UPPER_FALSE))){
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
			return env_value;
		}
	}
	FLY_NOT_COME_HERE
}

long fly_config_value_long(char *name)
{
	long res;
	char *env_value;
	for (struct fly_config *__c=configs; __c->name; __c++){
		if (strlen(name) == strlen(__c->env_name) && strncmp(name, __c->env_name, strlen(name)) == 0){
			env_value = getenv(name);
			assert(env_value != NULL);
			errno = 0;
			res = strtol(env_value, NULL, 10);
			if (errno == ERANGE)
				return LONG_MAX;
			else
				return res;
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
			if (strncmp(env_value, FLY_CONFIG_BOOL_TRUE, strlen(FLY_CONFIG_BOOL_TRUE)) == 0 || \
					strncmp(env_value, FLY_CONFIG_BOOL_UPPER_TRUE, strlen(FLY_CONFIG_BOOL_TRUE)) == 0)
				return true;
			else
				return false;
		}
	}
	FLY_NOT_COME_HERE
}
