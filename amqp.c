amqp/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Marcel Oelke <moelke@studivz.net>                            |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 moelke Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
  
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_amqp.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <sys/time.h>
#include <unistd.h>

/* If you declare any globals in php_amqp.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(amqp)
*/

/* True global resources - no need for thread safety here */
static int le_amqp;
#define le_amqp_name "Amqp Connection"

void _php_amqp_error(amqp_rpc_reply_t x, char const *context);
void _php_amqp_socket_error(int retval, char const *context);


typedef struct {
    amqp_connection_state_t amqp_conn;
    int sockfd;
} amqp_connection;

/**
 * Clean up the connection ...
 */
static void _close_amqp_connection(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    amqp_connection *amqp_conn = (amqp_connection *) rsrc->ptr;
    if (amqp_conn) {
        amqp_channel_close(amqp_conn->amqp_conn, 1, AMQP_REPLY_SUCCESS);
        amqp_connection_close(amqp_conn->amqp_conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(amqp_conn->amqp_conn);
        close(amqp_conn->sockfd);   
        efree(amqp_conn);
    }
}

/* {{{ amqp_functions[]
 *
 * Every user visible function must have an entry in amqp_functions[].
 */
zend_function_entry amqp_functions[] = {
    PHP_FE(confirm_amqp_compiled,   NULL)       /* For testing, remove later. */
    PHP_FE(amqp_new_connection, NULL)
    PHP_FE(amqp_login,  NULL)
    PHP_FE(amqp_connection_close,   NULL)
    PHP_FE(amqp_basic_publish,  NULL)
    {NULL, NULL, NULL}  /* Must be the last line in amqp_functions[] */
};
/* }}} */

/* {{{ amqp_module_entry
 */
zend_module_entry amqp_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "amqp",
    amqp_functions,
    PHP_MINIT(amqp),
    PHP_MSHUTDOWN(amqp),
    NULL, // PHP_RINIT(amqp),        /* Replace with NULL if there's nothing to do at request start */
    NULL, // PHP_RSHUTDOWN(amqp),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(amqp),
#if ZEND_MODULE_API_NO >= 20010901
    "0.1", /* Replace with version number for your extension */
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_AMQP
ZEND_GET_MODULE(amqp)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("amqp.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_amqp_globals, amqp_globals)
    STD_PHP_INI_ENTRY("amqp.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_amqp_globals, amqp_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_amqp_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_amqp_init_globals(zend_amqp_globals *amqp_globals)
{
    amqp_globals->global_value = 0;
    amqp_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(amqp)
{
    /* If you have INI entries, uncomment these lines 
    REGISTER_INI_ENTRIES();
    */
    
    le_amqp = zend_register_list_destructors_ex(_close_amqp_connection, NULL, "Amqp Connection", module_number);  
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(amqp)
{
    /* uncomment this line if you have INI entries
    UNREGISTER_INI_ENTRIES();
    */
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */

PHP_RINIT_FUNCTION(amqp)
{
    return SUCCESS;
}

/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */

PHP_RSHUTDOWN_FUNCTION(amqp)
{
    return SUCCESS;
}

/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(amqp)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "amqp support", "enabled");    
    php_info_print_table_row(2, "Extension Version", "0.0.2");
    php_info_print_table_row(2, "librabbitmq version", amqp_version());
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_amqp_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_amqp_compiled)
{
    char *arg = NULL;
    int arg_len, len;
    char *strg;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
        return;
    }

    len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "amqp", arg);
    RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/

/* {{{ proto resource amqp_new_connection(string hostname, int port)
    */
PHP_FUNCTION(amqp_new_connection)
{
    int argc = ZEND_NUM_ARGS();
    char *hostname = NULL;
    int hostname_len;
    int port = 5672;

    amqp_connection *amqp_conn = (amqp_connection*)emalloc(sizeof(amqp_connection));

    if (zend_parse_parameters(argc TSRMLS_CC, "sl", &hostname, &hostname_len, &port) == FAILURE) {
        return;
    }
    
    amqp_conn->amqp_conn = amqp_new_connection();
    if (!amqp_conn->amqp_conn) {
        efree(amqp_conn);
        php_error(E_WARNING, "Error opening connection.");
        RETURN_FALSE;
    }
    if (!(amqp_conn->sockfd = amqp_open_socket(hostname, port))) {
        efree(amqp_conn);
        php_error(E_WARNING, "Error opening socket.");        
        RETURN_FALSE;
    }
    amqp_set_sockfd(amqp_conn->amqp_conn, amqp_conn->sockfd);
    
    ZEND_REGISTER_RESOURCE(return_value, amqp_conn, le_amqp);
}
/* }}} */

/* {{{ proto bool amqp_login(resource connection, string user, string pass, string vhost, int channel_max, int frame_max, int amqp_sasl_method_enum)
    */
PHP_FUNCTION(amqp_login)
{        
    char *vhost, *user, *pass = NULL;
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    int vhost_len;
    int user_len;
    int pass_len;        
    
    
    long channel_max;
    long frame_max;
    long amqp_sasl_method_enum;
    zval *connection = NULL;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rssslll", &connection, &user, &user_len, &pass, &pass_len, &vhost, &vhost_len, &channel_max, &frame_max, &amqp_sasl_method_enum) == FAILURE) {
        return;
    }

    if (connection) {
        ZEND_FETCH_RESOURCE(amqp_conn, amqp_connection*, &connection, connection_id, "Amqp Connection", le_amqp);
    }

    _php_amqp_error(amqp_login(amqp_conn->amqp_conn, vhost, channel_max, frame_max, AMQP_SASL_METHOD_PLAIN, user, pass),
            "Logging in");
    amqp_channel_open(amqp_conn->amqp_conn, 1);
    _php_amqp_error(amqp_rpc_reply, "Opening channel");
    
    RETURN_TRUE;

//    php_error(E_WARNING, "amqp_login: not yet implemented");
}
/* }}} */

/* {{{ proto void amqp_connection_close(resource connection)
    */
PHP_FUNCTION(amqp_connection_close)
{
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    zval *connection = NULL;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "r", &connection) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE(amqp_conn, amqp_connection*, &connection, connection_id, "Amqp Connection", le_amqp);
    } else {
        return;
    }
    
//    _php_amqp_error(amqp_channel_close(amqp_conn->amqp_conn, 1, AMQP_REPLY_SUCCESS), "Closing channel");
//    _php_amqp_error(amqp_connection_close(amqp_conn->amqp_conn, AMQP_REPLY_SUCCESS), "Closing connection");
//    amqp_destroy_connection(amqp_conn->amqp_conn);
//    _php_amqp_socket_error(close(amqp_conn->sockfd), "Closing socket");

    amqp_channel_close(amqp_conn->amqp_conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(amqp_conn->amqp_conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(amqp_conn->amqp_conn);
    close(amqp_conn->sockfd);


}
/* }}} */

/* {{{ proto bool amqp_basic_publish(resource connection, int channel_id, string exchange, string routing_key, string body, bool mandatory, bool immediate)
    */
PHP_FUNCTION(amqp_basic_publish)
{
    zval *arr;
	
    char *exchange = NULL;
    char *routing_key = NULL;
    char *body = NULL;
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    int exchange_len;
    int routing_key_len;
    int body_len;
    long channel_id;
    zend_bool mandatory;
    zend_bool immediate;
    zval *connection = NULL;
    
    HashKey key = initHashKey(0);
    HashPosition pos;
    zval *opts = NULL, *old_opts, *new_opts, *add_opts, **opt;
    
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rlsssbb|a!/", &connection, &channel_id, &exchange, &exchange_len, &routing_key, &routing_key_len, &body, &body_len, &mandatory, &immediate, &opts) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE(amqp_conn, amqp_connection*, &connection, connection_id, "Amqp Connection", le_amqp);
    }
    
    amqp_basic_properties_t props;
    props._flags = 0;
    
    FOREACH_KEYVAL(pos, opts, key, opt) {        
        if (key.type == HASH_KEY_IS_STRING) {
            #define KEYMATCH(k, s) ((sizeof(s)==k.len) && !strcasecmp(k.str, s))
            if (KEYMATCH(key, "delivery_mode")) {
                props._flags += AMQP_BASIC_DELIVERY_MODE_FLAG;
                props.delivery_mode = (uint8_t) Z_LVAL_PP(opt);
            }
            if (KEYMATCH(key, "priority")) {
                props._flags += AMQP_BASIC_PRIORITY_FLAG;
                props.priority = (uint8_t) Z_LVAL_PP(opt);
            }
            if (KEYMATCH(key, "timestamp")) {
                props._flags += AMQP_BASIC_TIMESTAMP_FLAG;
                props.timestamp = (uint64_t) Z_LVAL_PP(opt);
            }

            if (KEYMATCH(key, "content_type")) {
                props._flags += AMQP_BASIC_CONTENT_TYPE_FLAG;
                props.content_type = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "content_encoding")) {
                props._flags += AMQP_BASIC_CONTENT_ENCODING_FLAG;
                props.content_encoding = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "correlation_id")) {
                props._flags += AMQP_BASIC_CORRELATION_ID_FLAG;
                props.correlation_id = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "reply_to")) {
                props._flags += AMQP_BASIC_REPLY_TO_FLAG;
                props.reply_to = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "expiration")) {
                props._flags += AMQP_BASIC_EXPIRATION_FLAG;
                props.expiration = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "message_id")) {
                props._flags += AMQP_BASIC_MESSAGE_ID_FLAG;
                props.message_id = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "type")) {
                props._flags += AMQP_BASIC_TYPE_FLAG;
                props.type = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "user_id")) {
                props._flags += AMQP_BASIC_USER_ID_FLAG;
                props.user_id = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "app_id")) {
                props._flags += AMQP_BASIC_APP_ID_FLAG;
                props.app_id = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
            if (KEYMATCH(key, "cluster_id")) {
                props._flags += AMQP_BASIC_CLUSTER_ID_FLAG;
                props.cluster_id = amqp_cstring_bytes(Z_STRVAL_PP(opt));
            }
        }
    }
    
    _php_amqp_socket_error(amqp_basic_publish(amqp_conn->amqp_conn,
            channel_id,
            amqp_cstring_bytes(exchange), // "amq.direct"
            amqp_cstring_bytes(routing_key),
            mandatory ? 1 : 0,
            immediate ? 1 : 0,
            &props,
            (amqp_bytes_t) {.len = body_len, .bytes = body}),
        "Publishing");    
}
/* }}} */


void _php_amqp_error(amqp_rpc_reply_t x, char const *context) {
    switch (x.reply_type) {
        case AMQP_RESPONSE_NORMAL:
            return;

        case AMQP_RESPONSE_NONE:
            php_error(E_ERROR, "%s: missing RPC reply type!", context);
            break;

        case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            php_error(E_ERROR, "%s: %s\n", context, x.library_errno ? strerror(x.library_errno) : "(end-of-stream)");
            break;

        case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                    amqp_connection_close_t *m = (amqp_connection_close_t *) x.reply.decoded;
                    php_error(E_ERROR, "%s: server connection error %d, message: %.*s", context, m->reply_code, (int) m->reply_text.len, (char *) m->reply_text.bytes);
                break;
            }
            case AMQP_CHANNEL_CLOSE_METHOD: {
                amqp_channel_close_t *m = (amqp_channel_close_t *) x.reply.decoded;
                php_error(E_ERROR, "%s: server channel error %d, message: %.*s", context, m->reply_code, (int) m->reply_text.len, (char *) m->reply_text.bytes);
                break;
            }
            default:
                php_error(E_ERROR, "%s: unknown server error, method id 0x%08X", context, x.reply.id);
                break;
            }
      break;
  }
}


void _php_amqp_socket_error(int retval, char const *context) {
    if (retval < 0) {
        php_error(E_ERROR, "%s: %s\n", context, strerror(-retval));
    }
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
