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
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifndef PHP_AMQP_H
#define PHP_AMQP_H

extern zend_module_entry amqp_module_entry;
#define phpext_amqp_ptr &amqp_module_entry

#ifdef PHP_WIN32
#define PHP_AMQP_API __declspec(dllexport)
#else
#define PHP_AMQP_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(amqp);
PHP_MSHUTDOWN_FUNCTION(amqp);
PHP_RINIT_FUNCTION(amqp);
PHP_RSHUTDOWN_FUNCTION(amqp);
PHP_MINFO_FUNCTION(amqp);

PHP_FUNCTION(amqp_connection_open);
PHP_FUNCTION(amqp_connection_popen);
PHP_FUNCTION(amqp_login);
PHP_FUNCTION(amqp_channel_open);
PHP_FUNCTION(amqp_channel_close);
PHP_FUNCTION(amqp_connection_close);
PHP_FUNCTION(amqp_basic_publish);
PHP_FUNCTION(amqp_exchange_declare);
PHP_FUNCTION(amqp_queue_declare);
PHP_FUNCTION(amqp_queue_bind);
PHP_FUNCTION(amqp_queue_unbind);        


/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
ZEND_BEGIN_MODULE_GLOBALS(amqp)
	long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(amqp)
*/

/* In every utility function you add that needs to use variables 
   in php_amqp_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as AMQP_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#define http_zset(t, z) _http_zset((t), (z))
static inline zval *_http_zset(int type, zval *z)
{
    if (Z_TYPE_P(z) != type) {
        switch (type) {
            case IS_NULL:   convert_to_null(z);     break;
            case IS_BOOL:   convert_to_boolean(z);  break;
            case IS_LONG:   convert_to_long(z);     break;
            case IS_DOUBLE: convert_to_double(z);   break;
            case IS_STRING: convert_to_string(z);   break;
            case IS_ARRAY:  convert_to_array(z);    break;
            case IS_OBJECT: convert_to_object(z);   break;
        }
    }
    return z;
}
#define http_zsep(t, z) _http_zsep_ex((t), (z), NULL)
#define http_zsep_ex(t, z, p) _http_zsep_ex((t), (z), (p))
static inline zval *_http_zsep_ex(int type, zval *z, zval **p) {
    SEPARATE_ARG_IF_REF(z);
    if (Z_TYPE_P(z) != type) {
        switch (type) {
            case IS_NULL:   convert_to_null_ex(&z);     break;
            case IS_BOOL:   convert_to_boolean_ex(&z);  break;
            case IS_LONG:   convert_to_long_ex(&z);     break;
            case IS_DOUBLE: convert_to_double_ex(&z);   break;
            case IS_STRING: convert_to_string_ex(&z);   break;
            case IS_ARRAY:  convert_to_array_ex(&z);    break;
            case IS_OBJECT: convert_to_object_ex(&z);   break;
        }
    }
    if (p) {
        *p = z;
    }
    return z;
}


   typedef struct _HashKey {
       char *str;
       uint len;
       ulong num;
       uint dup:1;
       uint type:31;
   } HashKey;
   #define initHashKey(dup) {NULL, 0, 0, (dup), 0}

   #define FOREACH_VAL(pos, array, val) FOREACH_HASH_VAL(pos, Z_ARRVAL_P(array), val)
   #define FOREACH_HASH_VAL(pos, hash, val) \
       for (   zend_hash_internal_pointer_reset_ex(hash, &pos); \
               zend_hash_get_current_data_ex(hash, (void *) &val, &pos) == SUCCESS; \
               zend_hash_move_forward_ex(hash, &pos))

   #define FOREACH_KEY(pos, array, key) FOREACH_HASH_KEY(pos, Z_ARRVAL_P(array), key)
   #define FOREACH_HASH_KEY(pos, hash, _key) \
       for (   zend_hash_internal_pointer_reset_ex(hash, &pos); \
               ((_key).type = zend_hash_get_current_key_ex(hash, &(_key).str, &(_key).len, &(_key).num, (zend_bool) (_key).dup, &pos)) != HASH_KEY_NON_EXISTANT; \ zend_hash_move_forward_ex(hash, &pos)) \

   #define FOREACH_KEYVAL(pos, array, key, val) FOREACH_HASH_KEYVAL(pos, Z_ARRVAL_P(array), key, val)
   #define FOREACH_HASH_KEYVAL(pos, hash, _key, val) \
       for (   zend_hash_internal_pointer_reset_ex(hash, &pos); \
               ((_key).type = zend_hash_get_current_key_ex(hash, &(_key).str, &(_key).len, &(_key).num, (zend_bool) (_key).dup, &pos)) != HASH_KEY_NON_EXISTANT && \
               zend_hash_get_current_data_ex(hash, (void *) &val, &pos) == SUCCESS; \
               zend_hash_move_forward_ex(hash, &pos))

#ifdef ZTS
#define AMQP_G(v) TSRMG(amqp_globals_id, zend_amqp_globals *, v)
#else
#define AMQP_G(v) (amqp_globals.v)
#endif

#endif	/* PHP_AMQP_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
