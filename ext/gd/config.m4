AC_DEFUN(PHP_GD_JPEG,[
        AC_MSG_CHECKING([for libjpeg (needed by gd-1.8+)])
        AC_ARG_WITH(jpeg-dir,
        [  --with-jpeg-dir[=DIR]   GD: jpeg dir for gd-1.8+],[
          AC_MSG_RESULT(yes)
          if test "$withval" = "yes"; then
            withval="/usr/local"
          fi
          jold_LIBS=$LIBS
          LIBS="$LIBS -L$withval/lib"
          AC_CHECK_LIB(jpeg,jpeg_read_header, [LIBS="$LIBS -ljpeg"],[AC_MSG_RESULT(no)],)
          LIBS=$jold_LIBS
          if test "$shared" = "yes"; then
            GD_LIBS="$GD_LIBS -ljpeg"
            GD_LFLAGS="$GD_LFLAGS -L$withval/lib"
          else
            PHP_ADD_LIBRARY_WITH_PATH(jpeg, $withval/lib)
          fi
          LIBS="$LIBS -L$withval/lib -ljpeg"
        ],[
          AC_MSG_RESULT(no)
          AC_MSG_WARN(If configure fails try --with-jpeg-dir=<DIR>)
        ]) 
])

AC_DEFUN(PHP_GD_PNG,[
        AC_MSG_CHECKING([for libpng (needed by gd-2.0)])
        AC_ARG_WITH(png-dir,
        [  --with-png-dir[=DIR]   GD: png dir for gd-2.0+],[
          AC_MSG_RESULT(yes)
          if test "$withval" = "yes"; then
            withval="/usr/local"
          fi
          jold_LIBS=$LIBS
          LIBS="$LIBS -L$withval/lib"
          AC_CHECK_LIB(png,png_info_init, [LIBS="$LIBS -lpng"],[AC_MSG_RESULT(no)],)
          LIBS=$jold_LIBS
          if test "$shared" = "yes"; then
            GD_LIBS="$GD_LIBS -lpng"
            GD_LFLAGS="$GD_LFLAGS -L$withval/lib"
          else
            PHP_ADD_LIBRARY_WITH_PATH(png, $withval/lib)
          fi
          LIBS="$LIBS -L$withval/lib -lpng"
        ],[
          AC_MSG_RESULT(no)
          AC_MSG_WARN(If configure fails try --with-png-dir=<DIR>)
        ]) 
])


AC_DEFUN(PHP_GD_XPM,[
        AC_MSG_CHECKING([for libXpm (needed by gd-1.8+)])
        AC_ARG_WITH(xpm-dir,
        [  --with-xpm-dir[=DIR]    GD: xpm dir for gd-1.8+],[
          AC_MSG_RESULT(yes)
          if test "$withval" = "yes"; then
            withval="/usr/X11R6"
          fi
          old_LIBS=$LIBS
          LIBS="$LIBS -L$withval/lib -lX11"
          AC_CHECK_LIB(Xpm,XpmFreeXpmImage, [LIBS="$LIBS -L$withval/lib -lXpm"],[AC_MSG_RESULT(no)],)
          LIBS=$old_LIBS
          PHP_ADD_LIBRARY_WITH_PATH(Xpm, $withval/lib)
          PHP_ADD_LIBRARY_WITH_PATH(X11, $withval/lib)
          LIBS="$LIBS -L$withval/lib -lXpm -L$withval/lib -lX11"
        ],[
          AC_MSG_RESULT(no)
          AC_MSG_WARN(If configure fails try --with-xpm-dir=<DIR>)
        ]) 
])

AC_DEFUN(PHP_GD_FREETYPE,[
		AC_MSG_CHECKING([for freetype(2) (needed by gd 2.0+)])
		AC_ARG_WITH(freetype-dir,
		[  --with-freetype-dir[=DIR]    GD: freetype 2 dir for gd 2.0+],[
			for i in /usr /usr/local "$CHECK_FREETYPE" ; do
			if test -f "$i/include/freetype2/freetype/freetype.h"; then
				FREETYPE2_DIR="$i"
				FREETYPE2_INC_DIR="$i/include/freetype2/freetype"
			fi
			done
			if test -n "$FREETYPE2_DIR" ; then
				AC_DEFINE(HAVE_LIBFREETYPE,1,[ ])
				PHP_ADD_LIBRARY_WITH_PATH(freetype, $FREETYPE2_DIR/lib)
				PHP_ADD_INCLUDE($FREETYPE2_INC_DIR)
    			AC_DEFINE(USE_GD_IMGSTRTTF, 1, [ ])
				AC_MSG_RESULT(yes)
			else
				AC_MSG_RESULT(no (freetype2 not found))
			fi
		],[
			AC_MSG_RESULT(no)
			AC_MSG_RESULT(If configure fails, try --with-freetype-dir=<DIR>)
	   ])
])
 
AC_DEFUN(PHP_GD_CHECK_VERSION,[
        AC_CHECK_LIB(z,  compress,      LIBS="-lz $LIBS",,)
        AC_CHECK_LIB(png,png_info_init, LIBS="-lpng $LIBS",,)
        AC_CHECK_LIB(gd, gdImageString16,        [AC_DEFINE(HAVE_LIBGD13,1,[ ]) ])
        AC_CHECK_LIB(gd, gdImagePaletteCopy,     [AC_DEFINE(HAVE_LIBGD15,1,[ ]) ])
        AC_CHECK_LIB(gd, gdImageColorClosestHWB, [AC_DEFINE(HAVE_COLORCLOSESTHWB,1,[ ]) ])
        AC_CHECK_LIB(gd, gdImageColorResolve,    [AC_DEFINE(HAVE_GDIMAGECOLORRESOLVE,1,[ ])])
        AC_CHECK_LIB(gd, gdImageCreateFromPng,   [AC_DEFINE(HAVE_GD_PNG,  1, [ ])])
        AC_CHECK_LIB(gd, gdImageCreateFromGif,   [AC_DEFINE(HAVE_GD_GIF,  1, [ ])])
        AC_CHECK_LIB(gd, gdImageWBMP,            [AC_DEFINE(HAVE_GD_WBMP, 1, [ ])])
        AC_CHECK_LIB(gd, gdImageCreateFromJpeg,  [AC_DEFINE(HAVE_GD_JPG, 1, [ ])])
        AC_CHECK_LIB(gd, gdImageCreateFromXpm,   [AC_DEFINE(HAVE_GD_XPM, 1, [ ])])
        AC_CHECK_LIB(gd, gdImageCreateTrueColor,   [AC_DEFINE(HAVE_LIBGD20, 1, [ ])])
        AC_CHECK_LIB(gd, gdImageSetTile,   			[AC_DEFINE(HAVE_GD_IMAGESETTILE, 1, [ ])])
        AC_CHECK_LIB(gd, gdImageSetBrush,   			[AC_DEFINE(HAVE_GD_IMAGESETBRUSH, 1, [ ])])
        AC_CHECK_LIB(gd, gdImageStringFTEx,   			[AC_DEFINE(HAVE_GD_STRINGFTEX, 1, [ ])])
])


AC_MSG_CHECKING(whether to enable truetype string function in gd)
AC_ARG_ENABLE(gd-native-ttf,
[  --enable-gd-native-ttf   Enable TrueType string function in gd],[
  if test "$enableval" = "yes" ; then
    AC_DEFINE(USE_GD_IMGSTRTTF, 1, [ ])
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
],[
  AC_MSG_RESULT(no)
])


shared=no
AC_ARG_WITH(gd,
[  --with-gd[=DIR]         Include GD support (DIR is GD's install dir).
                          Set DIR to "shared" to build as a dl, or 
                          "shared,DIR" to build as a dl and still specify DIR.],
[
  PHP_WITH_SHARED
  old_withval=$withval
  PHP_GD_JPEG
  PHP_GD_PNG
  PHP_GD_XPM
  PHP_GD_FREETYPE
  withval=$old_withval

  AC_MSG_CHECKING(whether to include GD support)

  case "$withval" in
    no)
      AC_MSG_RESULT(no) 
    ;;
    yes)
      AC_DEFINE(HAVE_LIBGD,1,[ ])
      if test "$shared" = "yes"; then
        AC_MSG_RESULT(yes (shared))
        GD_LIBS="-lgd $GD_LIBS"
      else
        AC_MSG_RESULT(yes (static))
        PHP_ADD_LIBRARY(gd)
      fi

      old_LDFLAGS=$LDFLAGS
      old_LIBS=$LIBS

dnl Check the capabilities of GD lib...
      PHP_GD_CHECK_VERSION
        
      LIBS=$old_LIBS
      LDFLAGS=$old_LDFLAGS
      if test "$ac_cv_lib_gd_gdImageCreateFromPng" = "yes"; then
        PHP_ADD_LIBRARY(png)
        PHP_ADD_LIBRARY(z)
        if test "$shared" = "yes"; then
          GD_LIBS="$GD_LIBS -lpng -lz"
        fi
      fi

      ac_cv_lib_gd_gdImageLine=yes
    ;;

    *)
dnl A whole whack of possible places where this might be
      test -f $withval/include/gd1.3/gd.h && GD_INCLUDE="$withval/include/gd1.3"
      test -f $withval/include/gd/gd.h && GD_INCLUDE="$withval/include/gd"
      test -f $withval/include/gd.h && GD_INCLUDE="$withval/include"
      test -f $withval/gd1.3/gd.h && GD_INCLUDE="$withval/gd1.3"
      test -f $withval/gd/gd.h && GD_INCLUDE="$withval/gd"
      test -f $withval/gd.h && GD_INCLUDE="$withval"

      test -f $withval/lib/libgd.so && GD_LIB="$withval/lib"
      test -f $withval/lib/gd/libgd.so && GD_LIB="$withval/lib/gd"
      test -f $withval/lib/gd1.3/libgd.so && GD_LIB="$withval/lib/gd1.3"
      test -f $withval/libgd.so && GD_LIB="$withval"
      test -f $withval/gd/libgd.so && GD_LIB="$withval/gd"
      test -f $withval/gd1.3/libgd.so && GD_LIB="$withval/gd1.3"

      test -f $withval/lib/libgd.a && GD_LIB="$withval/lib"
      test -f $withval/lib/gd/libgd.a && GD_LIB="$withval/lib/gd"
      test -f $withval/lib/gd1.3/libgd.a && GD_LIB="$withval/lib/gd1.3"
      test -f $withval/libgd.a && GD_LIB="$withval"
      test -f $withval/gd/libgd.a && GD_LIB="$withval/gd"
      test -f $withval/gd1.3/libgd.a && GD_LIB="$withval/gd1.3"

      if test -n "$GD_INCLUDE" && test -n "$GD_LIB" ; then
        AC_DEFINE(HAVE_LIBGD,1,[ ])
        if test "$shared" = "yes"; then
          AC_MSG_RESULT(yes (shared))
          GD_LIBS="-lgd $GD_LIBS"
          GD_LFLAGS="-L$GD_LIB $GD_LFLAGS"
        else
          AC_MSG_RESULT(yes (static))
          PHP_ADD_LIBRARY_WITH_PATH(gd, $GD_LIB)
        fi
        old_LDFLAGS=$LDFLAGS
        LDFLAGS="$LDFLAGS -L$GD_LIB"
		old_LIBS=$LIBS

dnl Check the capabilities of GD lib...
        PHP_GD_CHECK_VERSION
        
        LIBS=$old_LIBS
        LDFLAGS=$old_LDFLAGS
        if test "$ac_cv_lib_gd_gdImageCreateFromPng" = "yes"; then
          PHP_ADD_LIBRARY(z)
          PHP_ADD_LIBRARY(png)
          if test "$shared" = "yes"; then
            GD_LIBS="$GD_LIBS -lpng -lz"
          fi
        fi

        ac_cv_lib_gd_gdImageLine=yes
      else
        AC_MSG_ERROR([Unable to find libgd.(a|so) anywhere under $withval])
      fi 
    ;;
  esac
],[])


if test "$with_gd" != "no" && test "$ac_cv_lib_gd_gdImageLine" = "yes"; then
  CHECK_TTF="yes"

 
  AC_ARG_WITH(ttf,
  [  --with-ttf[=DIR]        GD: Include FreeType 1.x support],[
    if test $withval = "no" ; then
      CHECK_TTF=""
    else
      CHECK_TTF="$withval"
    fi
  ])

  AC_MSG_CHECKING(whether to include FreeType 1.x support)
  if test "$with_freetype_dir" = "no" -o "$with_freetype_dir" = ""; then
    if test -n "$CHECK_TTF" ; then
			for i in /usr /usr/local "$CHECK_TTF" ; do
				if test -f "$i/include/freetype.h" ; then
					TTF_DIR="$i"
					unset TTF_INC_DIR
				fi
				if test -f "$i/include/freetype/freetype.h"; then
					TTF_DIR="$i"
					TTF_INC_DIR="$i/include/freetype"
				fi
			done
			if test -n "$TTF_DIR" ; then
				AC_DEFINE(HAVE_LIBTTF,1,[ ])
				if test "$shared" = "yes"; then
					GD_LIBS="$GD_LIBS -lttf"
					GD_LFLAGS="$GD_LFLAGS -L$TTF_DIR/lib"
				else
					PHP_ADD_LIBRARY_WITH_PATH(ttf, $TTF_DIR/lib)
				fi
				if test -z "$TTF_INC_DIR"; then
					TTF_INC_DIR="$TTF_DIR/include"
				fi
				PHP_ADD_INCLUDE($TTF_INC_DIR)
				AC_MSG_RESULT(yes)
			else
				AC_MSG_RESULT(no)
			fi
		else
			AC_MSG_RESULT(no)
		fi
	else
		AC_MSG_RESULT(no - FreeType 2.x is to be used instead)
	fi
  
  AC_MSG_CHECKING(for T1lib support)
  AC_ARG_WITH(t1lib,
  [  --with-t1lib[=DIR]      GD: Include T1lib support.],
  [
    if test "$withval" != "no"; then
      if test "$withval" = "yes"; then
        for i in /usr/local /usr; do
          if test -f "$i/include/t1lib.h"; then
            T1_DIR="$i"
          fi
        done
      else
        if test -f "$withval/include/t1lib.h"; then
          T1_DIR="$withval"
        fi
      fi
      
      if test "$T1_DIR" != "no"; then
        PHP_ADD_INCLUDE("$T1_DIR/include")
        if test "$shared" = "yes"; then
          GD_LIBS="$GD_LIBS -lt1"
          GD_LFLAGS="$GD_LFLAGS -L$T1_DIR/lib"
        else
          PHP_ADD_LIBRARY_WITH_PATH(t1, "$T1_DIR/lib")
        fi
        LIBS="$LIBS -L$T1_DIR/lib -lt1"
      fi 

      AC_MSG_RESULT(yes)
      AC_CHECK_LIB(t1, T1_GetExtend, [AC_DEFINE(HAVE_LIBT1,1,[ ])])
    else
      AC_MSG_RESULT(no)
    fi
  ],[
    AC_MSG_RESULT(no)
  ])

dnl NetBSD package structure
  if test -f /usr/pkg/include/gd/gd.h -a -z "$GD_INCLUDE" ; then
    GD_INCLUDE="/usr/pkg/include/gd"
  fi

dnl SuSE 6.x package structure
  if test -f /usr/include/gd/gd.h -a -z "$GD_INCLUDE" ; then
    GD_INCLUDE="/usr/include/gd"
  fi

  PHP_EXPAND_PATH($GD_INCLUDE, GD_INCLUDE)
  PHP_ADD_INCLUDE($GD_INCLUDE)
  PHP_EXTENSION(gd, $shared)
  if test "$shared" != "yes"; then
    GD_STATIC="libphpext_gd.la"
  else 
    GD_SHARED="gd.la"
  fi
fi

PHP_SUBST(GD_LFLAGS)
PHP_SUBST(GD_LIBS)
