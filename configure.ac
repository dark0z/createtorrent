AC_INIT([createtorrent],[1.1.4-r1])
AC_CONFIG_SRCDIR([main.c])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_CHECK_LIB([crypto],[SHA1],,[AC_MSG_ERROR([error, OpenSSL required])])
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADER([openssl/sha.h],,[AC_MSG_ERROR([Error, OpenSSL header file sha.h not found.])])
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_PROG_GCC_TRADITIONAL
AC_FUNC_STAT
AC_OUTPUT(Makefile)
