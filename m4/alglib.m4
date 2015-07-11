AC_DEFUN([AX_BOOST_BASE],
[
    for ac_boost_path in /usr /usr/local /opt /opt/local ; do
        if test -d "$ac_boost_path" && test -r "$ac_boost_path"; then
            for i in `ls -d $ac_boost_path/include/boost-* 2>/dev/null`; do
                _version_tmp=`echo $i | sed "s#$ac_boost_path##" | sed
                 V_CHECK=`expr $_version_tmp \> $_version`
                 if test "$V_CHECK" = "1" ; then
                     _version=$_version_tmp
                     best_path=$ac_boost_path
                 fi
             done
         fi
    done

])