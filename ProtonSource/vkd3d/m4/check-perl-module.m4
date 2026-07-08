dnl VKD3D_PERL_MODULE(module, action-if-not-found)
AC_DEFUN([VKD3D_CHECK_PERL_MODULE],
         [AC_MSG_CHECKING([for perl module $1])
          AS_IF([$PERL -e "use $1;" 2>&AS_MESSAGE_LOG_FD],
                [AC_MSG_RESULT([yes])],
                [AC_MSG_RESULT([no])]
                [$2])])
