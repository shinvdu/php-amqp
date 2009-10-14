/*
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
#include "php_globals.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"



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
static int le_amqp_persistent;

#define PHP_AMQP_EXTENSION_VERSION "0.0.7"
#define PHP_AMQP_RES_NAME "Amqp Connection"


int _php_amqp_error(amqp_rpc_reply_t x, char const *context);
int _php_amqp_socket_error(int retval, char const *context);


typedef struct {
    amqp_connection_state_t amqp_conn;
    int sockfd;
    int num_channels;
    int logged_in;
} amqp_connection;

/**
 * Clean up the connection ...
 */
static void _close_amqp_connection(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    amqp_connection *amqp_conn = (amqp_connection *) rsrc->ptr;
    if (amqp_conn) {
        amqp_channel_close(amqp_conn->amqp_conn, 1, AMQP_REPLY_SUCCESS);
        if (amqp_conn->amqp_conn) {
            amqp_connection_close(amqp_conn->amqp_conn, AMQP_REPLY_SUCCESS);
            // amqp_destroy_connection(amqp_conn->amqp_conn);
        }
        if (amqp_conn->sockfd) {
            close(amqp_conn->sockfd);
        }
        efree(amqp_conn);
    }
}

/**
 * Clean up the persistent connection ...
 */
static void _close_amqp_pconnection(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    amqp_connection *amqp_conn = (amqp_connection *) rsrc->ptr;
    if (amqp_conn) {
        amqp_channel_close(amqp_conn->amqp_conn, 1, AMQP_REPLY_SUCCESS);
        if (amqp_conn->amqp_conn) {
            amqp_connection_close(amqp_conn->amqp_conn, AMQP_REPLY_SUCCESS);
            // amqp_destroy_connection(amqp_conn->amqp_conn);
        }
        if (amqp_conn->sockfd) {
            close(amqp_conn->sockfd);
        }
        pefree(amqp_conn, 1);
    }
}


/* {{{ amqp_functions[]
 *
 * Every user visible function must have an entry in amqp_functions[].
 */
zend_function_entry amqp_functions[] = {
    PHP_FE(amqp_connection_open, NULL)
    PHP_FE(amqp_connection_popen, NULL)    
    PHP_FE(amqp_login,  NULL)
    PHP_FE(amqp_channel_open,  NULL)
    PHP_FE(amqp_channel_close,  NULL)        
    PHP_FE(amqp_connection_close,   NULL)
    PHP_FE(amqp_basic_publish,  NULL)
    PHP_FE(amqp_exchange_declare, NULL)        
    PHP_FE(amqp_queue_declare, NULL)
    PHP_FE(amqp_queue_bind, NULL)
    PHP_FE(amqp_queue_unbind, NULL)
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
    PHP_RINIT(amqp),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(amqp),    /* Replace with NULL if there's nothing to do at request end */
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

    REGISTER_STRING_CONSTANT("AMQP_EXTENSION_VERSION", PHP_AMQP_EXTENSION_VERSION, CONST_CS | CONST_PERSISTENT);

    le_amqp = zend_register_list_destructors_ex(_close_amqp_connection, NULL, PHP_AMQP_RES_NAME, module_number);  
    le_amqp_persistent = zend_register_list_destructors_ex(_close_amqp_pconnection, NULL, PHP_AMQP_RES_NAME, module_number);  
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
    php_info_print_table_row(2, "Extension Version", PHP_AMQP_EXTENSION_VERSION);
    php_info_print_table_row(2, "librabbitmq version", amqp_version());
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */


/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/

/* {{{ proto resource amqp_connection_open(string hostname, int port)
    */
PHP_FUNCTION(amqp_connection_open)
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
    // php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Created normal NON persistent resource!!");
    
    amqp_conn->num_channels = 0;
    amqp_conn->logged_in = 0;
    
    ZEND_REGISTER_RESOURCE(return_value, amqp_conn, le_amqp);
}
/* }}} */


/* {{{ proto resource amqp_connection_popen(string hostname, int port)
    */
PHP_FUNCTION(amqp_connection_popen)
{
    int argc = ZEND_NUM_ARGS();
    char *hostname = NULL;
    int hostname_len;
    int port = 5672;
    
    char *key;
    int  key_len;
    list_entry *le, new_le;

    if (zend_parse_parameters(argc TSRMLS_CC, "sl", &hostname, &hostname_len, &port) == FAILURE) {
        return;
    }

    /* Look for an established resource */
    key_len = spprintf(&key, 0, "amqp_%s_%d ", hostname, port);
    if (zend_hash_find(&EG(persistent_list), key, key_len + 1, (void **) &le) == SUCCESS) {
        // php_error_docref(NULL TSRMLS_CC, E_NOTICE, "RE-Using persistent resource!! %s", key);
        ZEND_REGISTER_RESOURCE(return_value, le->ptr, le_amqp_persistent);
        efree(key);
        return;
    }
    
    // php_error_docref(NULL TSRMLS_CC, E_NOTICE, "NOT!!! RE-Using persistent resource!! %s", key);
    
    amqp_connection *amqp_conn = (amqp_connection*)pemalloc(sizeof(amqp_connection), 1);    
    ZEND_REGISTER_RESOURCE(return_value, amqp_conn, le_amqp_persistent);
    
    amqp_conn->amqp_conn = amqp_new_connection();
    if (!amqp_conn->amqp_conn) {
        pefree(amqp_conn, 1);
        php_error(E_WARNING, "Error opening connection.");
        RETURN_FALSE;
    }
    if (!(amqp_conn->sockfd = amqp_open_socket(hostname, port))) {
        pefree(amqp_conn, 1);
        php_error(E_WARNING, "Error opening socket.");
        RETURN_FALSE;
    }
    amqp_set_sockfd(amqp_conn->amqp_conn, amqp_conn->sockfd);
    amqp_conn->num_channels = 0;
    amqp_conn->logged_in = 0;    
    
    /* Store a reference in the persistence list */
    new_le.ptr = amqp_conn;
    new_le.type = le_amqp_persistent;
    zend_hash_add(&EG(persistent_list), key, key_len + 1,  &new_le, sizeof(list_entry), NULL);
    efree(key);    
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

    long channel_max = 0;
    long frame_max = 131072;
    long amqp_sasl_method_enum = 0;
    zval *connection = NULL;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rsss|lll", &connection, &user, &user_len, &pass, &pass_len, &vhost, &vhost_len, &channel_max, &frame_max, &amqp_sasl_method_enum) == FAILURE) {
        return;
    }

    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }
    
    if (amqp_conn->logged_in) {
        // already logged in, return silently
        RETURN_TRUE;
    }

    if (!_php_amqp_error(amqp_login(amqp_conn->amqp_conn, vhost, channel_max, frame_max, AMQP_SASL_METHOD_PLAIN, user, pass), "Logging in")) {
        amqp_conn->logged_in = 0;
        RETURN_FALSE;
    };
    
    amqp_conn->logged_in = 1;
    RETURN_TRUE;
}
/* }}} */


PHP_FUNCTION(amqp_channel_open)
{
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;

    int channel_id = 1;
    amqp_connection *amqp_conn;    
    zval *connection = NULL;

    if (zend_parse_parameters(argc TSRMLS_CC, "rl", &connection, &channel_id) == FAILURE) {
        return;
    }
    
    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }
    if (amqp_conn->num_channels == 0) {
        amqp_channel_open(amqp_conn->amqp_conn, channel_id);    
        amqp_conn->num_channels = amqp_conn->num_channels +1;
    }
    RETURN_TRUE;
}


PHP_FUNCTION(amqp_channel_close)
{
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;

    int channel_id = 1;
    amqp_connection *amqp_conn;    
    zval *connection = NULL;
    
    if (zend_parse_parameters(argc TSRMLS_CC, "rl", &connection, &channel_id) == FAILURE) {
        return;
    }
    
    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }
    
    if (amqp_conn->num_channels > 0) {
        amqp_channel_close(amqp_conn->amqp_conn, channel_id, AMQP_REPLY_SUCCESS);
    }
    amqp_conn->num_channels = amqp_conn->num_channels  - 1;
    RETURN_TRUE;
}


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
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }

    int i = 0;
    if (amqp_conn->num_channels > 0) {
        for (i = amqp_conn->num_channels; i > 0; i--) {
           amqp_channel_close(amqp_conn->amqp_conn, i, AMQP_REPLY_SUCCESS);
        }
    }
    
    amqp_conn->num_channels = 0;
    amqp_conn->logged_in = 0;

    amqp_connection_close(amqp_conn->amqp_conn, AMQP_REPLY_SUCCESS);
    // this cause segfaults ?!
    // amqp_destroy_connection(amqp_conn->amqp_conn);
    close(amqp_conn->sockfd);
    RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool amqp_basic_publish(resource connection, int channel_id, string exchange, string routing_key, string body, bool mandatory, bool immediate, array options)
    */
PHP_FUNCTION(amqp_basic_publish)
{
    // zval *arr;
    
    char *exchange = NULL;
    char *routing_key = NULL;
    char *body = NULL;
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    int exchange_len;
    int routing_key_len;
    int body_len;
    long channel_id = 1;
    zend_bool mandatory = 0;
    zend_bool immediate = 0;
    zval *connection = NULL;
        
    HashKey key = initHashKey(0);
    HashPosition pos;
    zval *opts = NULL, *old_opts, *new_opts, *add_opts, **opt;
    
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rlsss|bba!/", &connection, &channel_id, &exchange, &exchange_len, &routing_key, &routing_key_len, &body, &body_len, &mandatory, &immediate, &opts) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }
    
    // php_error_docref(NULL TSRMLS_CC, E_NOTICE, "num_channels: %d, logged_in: %d", amqp_conn->num_channels, amqp_conn->logged_in);    
    
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
    
    int rpc_result = amqp_basic_publish(
        amqp_conn->amqp_conn,
        channel_id,
        amqp_cstring_bytes(exchange), // "amq.direct"
        amqp_cstring_bytes(routing_key),
        mandatory ? 1 : 0,
        immediate ? 1 : 0,
        &props,
        (amqp_bytes_t) {.len = body_len, .bytes = body});

    if (_php_amqp_socket_error(rpc_result, "BasicPublish")) {
        RETURN_FALSE;
    }
    RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool amqp_exchange_declare(resource connection, int channel_id, string exchange, string type, bool passive, bool durable, bool auto_delete)

amqp_connection_state_t state,
amqp_channel_t channel,
amqp_bytes_t exchange,
amqp_bytes_t type,
amqp_boolean_t passive,
amqp_boolean_t durable,
amqp_boolean_t auto_delete,
amqp_table_t arguments
    */
PHP_FUNCTION(amqp_exchange_declare)
{
    
    zval *connection = NULL;
    
    long channel_id = 1;
    
    char *exchange = NULL;
    int exchange_len;
    
    char *type = NULL;
    int type_len;
    
    zend_bool passive = 0;
    zend_bool durable = 0;
    zend_bool auto_delete = 0;
    
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rlss|bbb", &connection, &channel_id, &exchange, &exchange_len, &type, &type_len, &passive, &durable, &auto_delete) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }
    
    amqp_exchange_declare_t s =  (amqp_exchange_declare_t) {
        .ticket = 0,
        .exchange = { .len = exchange_len, .bytes = exchange },
        .type = { .len = type_len, .bytes = type },
        .passive = (passive)? 1:0,
        .durable =  (durable)? 1:0,
        .auto_delete =  (auto_delete)? 1:0,
        .internal = 0,
        .nowait = 0,
        .arguments = {.num_entries = 0, .entries = NULL}
    };
    
    amqp_rpc_reply_t rpc_reply;
    rpc_reply = amqp_simple_rpc(
        amqp_conn->amqp_conn,
        channel_id,
        AMQP_EXCHANGE_DECLARE_METHOD,
        AMQP_EXCHANGE_DECLARE_OK_METHOD,
        &s);
    
  if (_php_amqp_error(rpc_reply, "ExchangeDeclare")) {
      RETURN_TRUE;
  }
  RETURN_FALSE;
}
/* }}} */



/* {{{ proto bool amqp_queue_declare(resource connection, int channel_id, string queue, bool passive, bool durable, bool exclusive, bool auto_delete)
amqp_connection_state_t state,
amqp_channel_t channel,
amqp_bytes_t queue,
amqp_boolean_t passive,
amqp_boolean_t durable,
amqp_boolean_t exclusive,
amqp_boolean_t auto_delete,
amqp_table_t arguments
    */
PHP_FUNCTION(amqp_queue_declare)
{
    
    zval *connection = NULL;    
    long channel_id = 1;
    char *queue = NULL;
    int queue_len;
    
    zend_bool passive = 0;
    zend_bool durable = 0;
    zend_bool exclusive = 0;
    zend_bool auto_delete = 1;    
    
    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rls|bbbb", &connection, &channel_id, &queue, &queue_len, &passive, &durable, &exclusive, &auto_delete) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }
    
    amqp_queue_declare_t s = (amqp_queue_declare_t) {
        .ticket = 0,
        .queue = {.len = queue_len, .bytes = queue},
        .passive = passive ? 1:0,
        .durable = durable ? 1:0,
        .exclusive = exclusive ? 1:0,
        .auto_delete = auto_delete? 1:0,
        .nowait = 0,    
        .arguments = {.num_entries = 0, .entries = NULL}
    };
    
    amqp_rpc_reply_t rpc_reply;
    rpc_reply = amqp_simple_rpc(
        amqp_conn->amqp_conn,
        channel_id,
        AMQP_QUEUE_DECLARE_METHOD,
        AMQP_QUEUE_DECLARE_OK_METHOD,
        &s);

    if (_php_amqp_error(rpc_reply, "QueueDeclare")) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
/* }}} */


/* {{{ proto bool amqp_queue_bind(resource connection, int channel_id, string queue, string exchange, string routing_key)
amqp_connection_state_t state,
amqp_channel_t channel,
amqp_bytes_t queue,
amqp_bytes_t exchange,
amqp_bytes_t routing_key,
amqp_table_t arguments
*/
PHP_FUNCTION(amqp_queue_bind)
{
    zval *connection = NULL;    
    long channel_id = 1;

    char *queue = NULL;
    int queue_len;
    
    char *exchange = NULL;
    int exchange_len;
    
    char *routing_key = NULL;
    int routing_key_len;    

    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rlsss", &connection, &channel_id, &queue, &queue_len, &exchange, &exchange_len, &routing_key, &routing_key_len) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }

    amqp_queue_bind_t s = (amqp_queue_bind_t) {
        .ticket = 0,
        .queue = { .len = queue_len, .bytes = queue },
        .exchange = { .len = exchange_len, .bytes = exchange }, 
        .routing_key = { .len = routing_key_len, .bytes = routing_key }, 
        .nowait = 0 ,
        .arguments = {.num_entries = 0, .entries = NULL}
    };
    
    amqp_rpc_reply_t rpc_reply;       
    rpc_reply = amqp_simple_rpc(
        amqp_conn->amqp_conn,
        channel_id,
        AMQP_QUEUE_BIND_METHOD,
        AMQP_QUEUE_BIND_OK_METHOD,
        &s);
    
    if (_php_amqp_error(rpc_reply, "QueueBind")) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
/* }}} */




/* {{{ proto bool amqp_queue_unbind(resource connection, int channel_id, string queue, string exchange, string routing_key)
amqp_connection_state_t state,
amqp_channel_t channel,
amqp_bytes_t queue,
amqp_bytes_t exchange,
amqp_bytes_t routing_key,
amqp_table_t arguments
*/
PHP_FUNCTION(amqp_queue_unbind)
{
    zval *connection = NULL;    
    long channel_id = 1;

    char *queue = NULL;
    int queue_len;
    
    char *exchange = NULL;
    int exchange_len;
    
    char *routing_key = NULL;
    int routing_key_len;    

    int argc = ZEND_NUM_ARGS();
    int connection_id = -1;
    
    amqp_connection *amqp_conn;

    if (zend_parse_parameters(argc TSRMLS_CC, "rlsss", &connection, &channel_id, &queue, &queue_len, &exchange, &exchange_len, &routing_key, &routing_key_len) == FAILURE) 
        return;

    if (connection) {
        ZEND_FETCH_RESOURCE2(amqp_conn, amqp_connection*, &connection, connection_id, PHP_AMQP_RES_NAME, le_amqp, le_amqp_persistent);
    } else {
        php_error(E_WARNING, "Could not load connection AMQP resource.");
        RETURN_FALSE;
    }

    amqp_queue_unbind_t s = (amqp_queue_unbind_t) {
        .ticket = 0,
        .queue = { .len = queue_len, .bytes = queue },
        .exchange = { .len = exchange_len, .bytes = exchange }, 
        .routing_key = { .len = routing_key_len, .bytes = routing_key }, 
        .arguments = {.num_entries = 0, .entries = NULL}
    };
    
    amqp_rpc_reply_t rpc_reply;       
    rpc_reply = amqp_simple_rpc(
        amqp_conn->amqp_conn,
        channel_id,
        AMQP_QUEUE_UNBIND_METHOD,
        AMQP_QUEUE_UNBIND_OK_METHOD,
        &s);
    
    if (_php_amqp_error(rpc_reply, "QueueUnbind")) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
/* }}} */




int _php_amqp_error(amqp_rpc_reply_t x, char const *context) {
    switch (x.reply_type) {
        case AMQP_RESPONSE_NORMAL:
            // no error, return 1 / true
            return 1;

        case AMQP_RESPONSE_NONE:
            php_error(E_WARNING, "%s: missing RPC reply type!", context);
            break;

        case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            php_error(E_WARNING, "%s: %s\n", context, x.library_errno ? strerror(x.library_errno) : "(end-of-stream)");
            break;

        case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                    amqp_connection_close_t *m = (amqp_connection_close_t *) x.reply.decoded;
                    php_error(E_WARNING, "%s: server connection error %d, message: %.*s", context, m->reply_code, (int) m->reply_text.len, (char *) m->reply_text.bytes);
                break;
            }
            case AMQP_CHANNEL_CLOSE_METHOD: {
                amqp_channel_close_t *m = (amqp_channel_close_t *) x.reply.decoded;
                php_error(E_WARNING, "%s: server channel error %d, message: %.*s", context, m->reply_code, (int) m->reply_text.len, (char *) m->reply_text.bytes);
                break;
            }
            default:
                php_error(E_WARNING, "%s: unknown server error, method id 0x%08X", context, x.reply.id);
                break;
            }
      break;
  }
  // at this stage everything is an error.
  return 0;
}


int _php_amqp_socket_error(int retval, char const *context) {
    if (retval < 0) {
        php_error(E_WARNING, "%s: %s\n", context, strerror(-retval));
        return 0;
    }
    return 1;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
