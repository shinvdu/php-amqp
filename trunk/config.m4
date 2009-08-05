dnl $Id$
dnl config.m4 for extension amqp

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(amqp, for amqp support,
[  --with-amqp             Include amqp support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(amqp, whether to enable amqp support,
dnl Make sure that the comment is aligned:
dnl [  --enable-amqp           Enable amqp support])

if test "$PHP_AMQP" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-amqp -> check with-path
  SEARCH_PATH="/usr/local /usr /opt/local"     # you might want to change this
  SEARCH_FOR="/include/amqp.h"  # you most likely want to change this
  if test -r $PHP_AMQP/$SEARCH_FOR; then # path given as parameter
    AMQP_DIR=$PHP_AMQP
  else # search default path list
    AC_MSG_CHECKING([for amqp files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        AMQP_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi
 
  if test -z "$AMQP_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the amqp distribution])
  fi

  dnl # --with-amqp -> add include path
  PHP_ADD_INCLUDE($AMQP_DIR/include)

  dnl # --with-amqp -> check for lib and symbol presence
  LIBNAME=rabbitmq # you may want to change this
  LIBSYMBOL=amqp_version # you most likely want to change this 

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $AMQP_DIR/lib, AMQP_SHARED_LIBADD)
    AC_DEFINE(HAVE_AMQPLIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong amqp lib version or lib not found])
  ],[
    -L$AMQP_DIR/lib -lm -ldl
  ])
 
  PHP_SUBST(AMQP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(amqp, amqp.c, $ext_shared)
fi
