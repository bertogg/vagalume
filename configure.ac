#######################################
### Initialization and basic checks ###
#######################################
AC_INIT([vagalume], [0.8.5])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_PROG_MKDIR_P
AC_HEADER_STDC
IT_PROG_INTLTOOL([0.35.0], [no-xml])

# Some variables
VAGALUME_CONFIG_DIR=".vagalume"
appdatadir="\${datadir}/vagalume"
themebigicondir="\${appdatadir}/icons/hicolor/scalable/apps"
themesmallicondir="\${appdatadir}/icons/hicolor/scalable/apps"

# Defines for config.h
AC_DEFINE_UNQUOTED([KERNEL_NAME],"`uname -s`",[Name of the kernel])
AC_DEFINE_UNQUOTED([KERNEL_NAME_LC],"`uname -s|tr A-Z a-z`",
                   [Name of the kernel (lowercase)])
AC_DEFINE_UNQUOTED([ARCH],"`uname -m`",[Architecture])
AC_DEFINE_UNQUOTED(VAGALUME_CONFIG_DIR, "$VAGALUME_CONFIG_DIR",
                   [Vagalume configuration directory])

# Extra flags for compiler and linker
EXTRA_CFLAGS=""
EXTRA_LIBS=""

#####################################
### Basic dependencies - Required ###
#####################################
PKG_CHECK_MODULES(libxml2, libxml-2.0)
EXTRA_CFLAGS="$EXTRA_CFLAGS $libxml2_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $libxml2_LIBS"

PKG_CHECK_MODULES(libcurl, libcurl)
EXTRA_CFLAGS="$EXTRA_CFLAGS $libcurl_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $libcurl_LIBS"

if $PKG_CONFIG --exists gstreamer-0.10; then
   DETECTEDGST=0.10
elif $PKG_CONFIG --exists gstreamer-1.0; then
   DETECTEDGST=1.0
else
   AC_MSG_ERROR([This program requires either GStreamer 0.10 or 1.0.])
fi

if $PKG_CONFIG --exists gtk+-3.0; then
   DETECTEDGTK=3
elif $PKG_CONFIG --exists gtk+-2.0; then
   DETECTEDGTK=2
else
   AC_MSG_ERROR([This program requires either GTK+ 2 or GTK+ 3.])
fi

# Platform support
AC_ARG_WITH(gtk-version, [
Gtk+ version:
  --with-gtk-version=2    Use GTK+ 2
  --with-gtk-version=3    Use GTK+ 3 (default if available)],
  GTKVERSION=$withval, GTKVERSION=$DETECTEDGTK)

GTKMOD="gtk+-${GTKVERSION}.0"
PKG_CHECK_MODULES(Gtk, $GTKMOD)
EXTRA_CFLAGS="$EXTRA_CFLAGS $Gtk_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $Gtk_LIBS"

echo "Compiling with GTK+ version $GTKVERSION"

AC_ARG_WITH(gstreamer, [
GStreamer version:
  --with-gstreamer=1.0    Use GStreamer 1.0
  --with-gstreamer=0.10   Use GStreamer 0.10 (default if available)],
  GSTVERSION=$withval, GSTVERSION=$DETECTEDGST)

GSTMOD="gstreamer-${GSTVERSION}"
PKG_CHECK_MODULES(GStreamer, $GSTMOD)
EXTRA_CFLAGS="$EXTRA_CFLAGS $GStreamer_CFLAGS"
EXTRA_LIBS="$EXTRA_LIBS $GStreamer_LIBS"

echo "Compiling with GStreamer version $GSTVERSION"
#####################################
### Other dependencies - Optional ###
#####################################
PKG_CHECK_MODULES(GIO, gio-2.0, [have_gio="yes"], [have_gio="no"])

PKG_CHECK_MODULES(dbus_glib, dbus-glib-1, [have_dbus_glib="yes"],
                  [have_dbus_glib="no"])

PKG_CHECK_MODULES(libnotify, libnotify >= 0.4.1,
                  [have_libnotify="yes"], [have_libnotify="no"])

if test "$GSTVERSION" = "1.0"; then
   have_gst_mixer="no"
else
   PKG_CHECK_MODULES(Gst_mixer, gstreamer-interfaces-0.10,
                     [have_gst_mixer="yes"], [have_gst_mixer="no"])
fi

PKG_CHECK_MODULES(libproxy, libproxy-1.0,
                  [have_libproxy="yes"], [have_libproxy="no"])

# Use libnotify only if it's linked against the same GTK+ version
if test "$have_libnotify" = "yes"; then
   if $PKG_CONFIG --print-requires libnotify | grep -q '^gtk+-.\.0'; then
     if ! $PKG_CONFIG --print-requires libnotify | grep -q ^$GTKMOD ; then
        echo "Disabling libnotify since it uses a different GTK+ version"
        have_libnotify="no"
     fi
  fi
fi

# D-BUS versions < 1.0 require DBUS_API_SUBJECT_TO_CHANGE
if test "$have_dbus_glib" = "yes"; then
   if ! $PKG_CONFIG --atleast-version=1.0 dbus-1; then
      dbus_glib_CFLAGS="$dbus_glib_CFLAGS -DDBUS_API_SUBJECT_TO_CHANGE"
   fi
fi

# Tray icon support requires Gtk 2.10 and libnotify
if test "$have_libnotify" = "yes"; then
   echo -n "checking for GTK+ >= 2.10... "
   if $PKG_CONFIG --atleast-version=2.10 "$GTKMOD"; then
      have_tray_icon="yes"
   else
      have_tray_icon="no"
   fi
   echo $have_tray_icon
fi

# gtk_entry_completion_set_inline_selection requires gtk 2.12
echo -n "checking for GTK+ >= 2.12... "
if $PKG_CONFIG --atleast-version=2.12 "$GTKMOD"; then
   have_gtk_2_12="yes"
   AC_DEFINE([HAVE_ENTRY_COMPLETION_INLINE_SELECTION],[1],
             [Defined if gtk_entry_completion_set_inline_selection () exists])
   AC_DEFINE([HAVE_GDK_THREADS_ADD_API],[1],
             [Defined if gtk_threads_add_* () exists])
   AC_DEFINE([HAVE_GTK_ABOUT_DIALOG_PROGRAM_NAME],[1],
             [Defined if GtkAboutDialog:program-name exists])
   AC_DEFINE([HAVE_GTK_TOOLTIP],[1],
             [Defined if gtk_widget_set_tooltip_text() exists])
else
   have_gtk_2_12="no"
fi
echo $have_gtk_2_12

# g_timeout_add_seconds requires gtk 2.14
echo -n "checking for GTK+ >= 2.14... "
if $PKG_CONFIG --atleast-version=2.14 "$GTKMOD"; then
   have_gtk_2_14="yes"
   AC_DEFINE([HAVE_GDK_THREADS_ADD_TIMEOUT_SECONDS],[1],
             [Defined if gdk_threads_add_timeout_seconds() exists])
   AC_DEFINE([HAVE_GTK_DIALOG_GET_CONTENT_AREA],[1],
             [Defined if gtk_dialog_get_content_area() exists])
else
   have_gtk_2_14="no"
fi
echo $have_gtk_2_14

# gtk_about_dialog_set_{url,email}_hook are deprecated in Gtk+ 2.18
echo -n "checking for GTK+ >= 2.18... "
if $PKG_CONFIG --atleast-version=2.18 "$GTKMOD"; then
   have_gtk_2_18="yes"
   have_gio="no"
else
   have_gtk_2_18="no"
fi
echo $have_gtk_2_18

# GChecksum and g_strcmp0 require glib 2.16
echo -n "checking for glib >= 2.16... "
if $PKG_CONFIG --atleast-version=2.16 glib-2.0; then
   have_glib_2_16="yes"
else
   have_glib_2_16="no"
fi
echo $have_glib_2_16

# Check if we have getopt
AC_CHECK_LIB(c, getopt,
   AC_DEFINE([HAVE_GETOPT],[1],[Defined if getopt() exists]),
   have_getopt="no")

# Check if libcurl has curl_easy_escape()
AC_CHECK_LIB(curl, curl_easy_escape,
   AC_DEFINE([HAVE_CURL_EASY_ESCAPE],[1],[If curl_easy_escape() exists]),
   have_curl_easy_escape="no")

#################################
### User configurable options ###
#################################
# Autodetect platform (with ugly hack to detect scratchbox, I know)
if $PKG_CONFIG --exists libosso && readlink /etc|grep -q ^/targets; then
   DETECTED_PLATFORM=maemo
else
   DETECTED_PLATFORM=gnome
fi

# Platform support
AC_ARG_WITH(platform, [
Target platform:
  --with-platform=detect  detect target platform (default)
  --with-platform=gnome   compile for Gnome
  --with-platform=maemo   compile for Maemo
  --with-platform=moblin  compile for Moblin (1024x600),
  --with-platform=moblin2 compile for Moblin (800x480)],
  PLATFORM=$withval, PLATFORM=detect)

if test "$PLATFORM" = "detect"; then
   PLATFORM=$DETECTED_PLATFORM
elif test "$PLATFORM" = "moblin2"; then
   PLATFORM=moblin
   scr_resolution=800x480
fi
PLATFORMVERS=""

# Check whether to use dbus-glib
AC_ARG_ENABLE(dbus, [
  --enable-dbus           Compile with D-BUS support (default if available)
  --disable-dbus          Don't compile with D-BUS support],
  use_dbus_glib=$enableval, use_dbus_glib=$have_dbus_glib)

if test "$use_dbus_glib" = "yes" -a "$have_dbus_glib" = "no"; then
   AC_MSG_ERROR([dbus-glib not found, install it or use --disable-dbus])
elif test "$use_dbus_glib" = "yes"; then
   EXTRA_CFLAGS="$EXTRA_CFLAGS $dbus_glib_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $dbus_glib_LIBS"
fi

# Check whether to use the "Set IM status" feature
AC_ARG_ENABLE(im-status, [
  --enable-im-status      Enable "Set IM status" feature (default if available)
  --disable-im-status     Disable "Set IM status" feature],
  set_im_status=$enableval, set_im_status=$use_dbus_glib)

if test "$set_im_status" = "yes" -a "$use_dbus_glib" = "no"; then
   AC_MSG_ERROR([dbus-glib not found, install it or use --disable-im-status])
elif test "$set_im_status" = "yes"; then
   AC_DEFINE([SET_IM_STATUS],[1],[Defined if "Set IM Status" is enabled])
   echo "\"Set IM status\" feature enabled"
else
   echo "\"Set IM status\" feature disabled"
fi
AM_CONDITIONAL(SET_IM_STATUS, test "$set_im_status" = "yes")

# Check whether to enable the tray icon and libnotify support
AC_ARG_ENABLE(tray-icon, [
  --enable-tray-icon      Enable tray icon and libnotify support
                          (Gnome only, default if available)
  --disable-tray-icon     Disable tray icon and libnotify support],
  enable_tray_icon=$enableval, enable_tray_icon=$have_tray_icon)

AC_ARG_ENABLE(libproxy, [
  --enable-libproxy       Enable system-wide proxy support
                          (default if available)
  --disable-libproxy      Disable system-wide proxy support],
  use_libproxy=$enableval, use_libproxy=$have_libproxy)

if test "$use_libproxy" = "yes" -a "$have_libproxy" = "no"; then
   AC_MSG_ERROR([libproxy not found, install it or use --disable-libproxy])
fi

# Check whether to enable cast checks
AC_ARG_ENABLE(cast-checks, [
  --enable-cast-checks    Turn on cast checks
  --disable-cast-checks   Turn off cast checks (default)],
  enable_cast_checks=$enableval, enable_cast_checks=no)

if test "x$enable_cast_checks" != "xyes"; then
   EXTRA_CFLAGS="$EXTRA_CFLAGS -DG_DISABLE_CAST_CHECKS"
fi

#################################
### Platform dependent checks ###
#################################
if test "$PLATFORM" = "maemo"; then

   # Maemo screen resolution
   scr_resolution=800x480

   # Disable tray icon support on Maemo
   have_tray_icon="no"

   # Disable GStreamer mixer
   have_gst_mixer="no"

   # Disable GIO support
   have_gio="no"

   # Detect the version of the Maemo SDK
   libossovers=`$PKG_CONFIG libosso --modversion | cut -d . -f 1`

   if test "$have_curl_easy_escape" = "no"; then
      PLATFORMVERS=2
   elif test "$libossovers" = "1"; then
      PLATFORMVERS=3
   elif test "$libossovers" = "2"; then
      if test "$have_gtk_2_14" = "yes"; then
         PLATFORMVERS=5
      else
         PLATFORMVERS=4
      fi
   else
      AC_MSG_ERROR([Unknown Maemo version])
   fi
   echo "Compiling for Maemo $PLATFORMVERS"

   # These are required but their names vary between Maemo versions
   if test "$PLATFORMVERS" = "2" -o "$PLATFORMVERS" = "3"; then
      PKG_CHECK_MODULES(hildon, hildon-libs)
      PKG_CHECK_MODULES(hildonfm, hildon-fm)
      AC_DEFINE([HILDON_LIBS], [1], [Defined if using hildon-libs])
      AC_DEFINE([HILDON_FM], [1], [Defined if using hildon-fm])
   else
      PKG_CHECK_MODULES(hildon, hildon-1)
      PKG_CHECK_MODULES(hildonfm, hildon-fm-2)
      AC_DEFINE([HILDON_1], [1], [Defined if using hildon-1])
      AC_DEFINE([HILDON_FM_2], [1], [Defined if using hildon-fm-2])
   fi
   EXTRA_CFLAGS="$EXTRA_CFLAGS $hildon_CFLAGS $hildonfm_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $hildon_LIBS $hildonfm_LIBS"

   # Libosso is required
   PKG_CHECK_MODULES(libosso, libosso osso-af-settings)
   EXTRA_CFLAGS="$EXTRA_CFLAGS $libosso_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $libosso_LIBS"

   # Libconic is not present in all Maemo versions
   PKG_CHECK_MODULES(libconic, conic, [have_conic="yes"], [have_conic="no"])
   if test "$have_conic" = "yes"; then
      EXTRA_CFLAGS="$EXTRA_CFLAGS $libconic_CFLAGS"
      EXTRA_LIBS="$EXTRA_LIBS $libconic_LIBS"
   fi

   desktopentrydir=`$PKG_CONFIG osso-af-settings --variable=desktopentrydir`
   serviceentrydir=`$PKG_CONFIG osso-af-settings --variable=dbusservicedir`

   if test "x$desktopentrydir" = "x"; then
      AC_MSG_ERROR([Unable to find .desktop directory])
   elif test "x$serviceentrydir" = "x"; then
      AC_MSG_ERROR([Unable to find dbus service directory])
   fi

   # These are optional and only required for the Maemo status bar plugin
   if test "x$PLATFORMVERS" = "x5"; then
      echo "The Maemo status bar plugin is not available in Maemo 5"
      have_sb_plugin="no"
   else
      echo "Checking dependencies to compile the Maemo status bar plugin"
      have_sb_plugin="yes"
      PKG_CHECK_MODULES(hildondesktop, libhildondesktop, [], [have_sb_plugin="no"])
      PKG_CHECK_MODULES(hildonwm, libhildonwm, [], [have_sb_plugin="no"])
      PKG_CHECK_MODULES(gconf2, gconf-2.0, [], [have_sb_plugin="no"])
      PKG_CHECK_MODULES(dbus, dbus-1, [], [have_sb_plugin="no"])

      if test "$have_sb_plugin" = "yes"; then
         SBPLUGIN_CFLAGS="$Gtk_CFLAGS $hildon_CFLAGS $libosso_CFLAGS \
                          $hildondesktop_CFLAGS $hildonwm_CFLAGS \
                          $gconf2_CFLAGS $dbus_CFLAGS"
         SBPLUGIN_LIBS="$Gtk_LIBS $hildon_LIBS $libosso_LIBS \
                        $hildondesktop_LIBS $hildonwm_LIBS \
                        $gconf2_LIBS $dbus_LIBS"
         sbplugindesktopentrydir=`$PKG_CONFIG osso-af-settings \
                                  --variable=statusbardesktopentrydir`
         sbplugindir=`$PKG_CONFIG osso-af-settings --variable=hildondesktoplibdir`

         if test "x$sbplugindesktopentrydir" = "x"; then
            AC_MSG_ERROR([Unable to find .desktop directory])
         elif test "x$sbplugindir" = "x"; then
            AC_MSG_ERROR([Unable to find hildon desktop libs directory])
         fi
         echo "All required libs found: compiling Maemo status bar plugin"
      else
         echo "Some libs are missing: Unable to compile Maemo status bar plugin"
      fi
   fi

   AC_DEFINE([MAEMO], [], [Defined if building for Maemo])
   AC_DEFINE([USE_HILDON_WINDOW], [1], [Defined if using HildonWindow])
   if test "$PLATFORMVERS" = "2"; then
      AC_DEFINE([MAEMO2], [], [Defined if building for Maemo 2 (gregale)])
   elif test "$PLATFORMVERS" = "3"; then
      AC_DEFINE([MAEMO3], [], [Defined if building for Maemo 3 (bora)])
   elif test "$PLATFORMVERS" = "4"; then
      AC_DEFINE([MAEMO4], [], [Defined if building for Maemo 4 (chinook)])
   else
      AC_DEFINE([MAEMO5], [], [Defined if building for Maemo 5 (fremantle)])
      # Install data in /opt, since the rootfs in the N900 is small
      bindir="/opt/vagalume/bin"
      appdatadir="/opt/vagalume"
      localedir="/opt/vagalume/locale"
      # Sorry, no Last.fm allowed on mobile phones
      AC_SUBST(DISABLE_LAST_FM, "yes")
   fi

   if test "$PLATFORMVERS" -le "4"; then
      AC_DEFINE([HAVE_DSPMP3SINK], [1], [Defined if using dspmp3sink])
      enable_volume_key_handler="yes"
      themesmallicondir="\${appdatadir}/icons/hicolor/50x50/apps"
   fi

   VAGALUME_CONF_FULLPATH="/home/user/$VAGALUME_CONFIG_DIR"
   AC_SUBST(VAGALUME_CONF_FULLPATH)

   BACKUP_CONFIGURATION_DIR="/etc/osso-backup/applications"
   AC_SUBST(BACKUP_CONFIGURATION_DIR)

   RESTORE_SETTINGS_SCRIPTS_DIR="/etc/osso-rfs-scripts"
   AC_SUBST(RESTORE_SETTINGS_SCRIPTS_DIR)

   CLEAR_DEVICE_SCRIPTS_DIR="/etc/osso-cud-scripts"
   AC_SUBST(CLEAR_DEVICE_SCRIPTS_DIR)

elif test "$PLATFORM" = "gnome"; then

   echo "Compiling for Gnome"

   # Desktop screen resolution
   scr_resolution=desktop

   if test "$use_dbus_glib" = "yes"; then
      serviceentrydir=`$PKG_CONFIG dbus-1 --variable=session_bus_services_dir`
      AC_DEFINE([HAVE_GSD_MEDIA_PLAYER_KEYS], [1],
                [Media player keys in some keyboards])
   fi

   desktopentrydir="\${datadir}/applications"

   if test "$enable_tray_icon" = "yes" -a "$have_tray_icon" = "no"; then
      AC_MSG_ERROR([Unable to compile with tray icon and libnotify support])
   elif test "$enable_tray_icon" = "yes"; then
      echo "Compiling with tray icon and notifications"
      # Add extra required parameters
      EXTRA_CFLAGS="$EXTRA_CFLAGS $libnotify_CFLAGS"
      EXTRA_LIBS="$EXTRA_LIBS $libnotify_LIBS"
   else
      have_tray_icon="no"
      echo "Compiling without tray icon and notifications"
   fi

elif test "$PLATFORM" = "moblin"; then

   if test "x$scr_resolution" = "x"; then
      scr_resolution=1024x600
   fi

   # Disable tray icon support on Moblin
   have_tray_icon="no"

   if test "$use_dbus_glib" = "yes"; then
      serviceentrydir=`$PKG_CONFIG dbus-1 --variable=session_bus_services_dir`
   fi
   desktopentrydir="\${datadir}/applications"

   echo "Compiling for Moblin"

   PKG_CHECK_MODULES(hildon, hildon-1)
   EXTRA_CFLAGS="$EXTRA_CFLAGS $hildon_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $hildon_LIBS"

   AC_DEFINE([MOBLIN], [1], [Defined if building for Moblin])
   AC_DEFINE([HILDON_1], [1], [Defined if using hildon-1])
   AC_DEFINE([USE_HILDON_WINDOW], [1], [Defined if using HildonWindow])

else

   AC_MSG_ERROR([Unknown platform $PLATFORM selected])

fi

####################
### Translations ###
####################
vgllocaledir=`eval echo $localedir`
vgllocaledir=`eval echo $vgllocaledir`
AC_DEFINE_UNQUOTED(VAGALUME_LOCALE_DIR, "$vgllocaledir", [Locale dir])
AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=vagalume
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package])

##################################################################
### Everything has been checked, now define Automake variables ###
##################################################################
AM_CONDITIONAL(MAEMO_PLATFORM, test "$PLATFORM" = "maemo")

AM_CONDITIONAL(HILDON_2_2, test "$PLATFORM" = "maemo" -a "$PLATFORMVERS" = "5")

AM_CONDITIONAL(USE_INTERNAL_MD5, test "x$have_glib_2_16" != "xyes")
if test "$have_glib_2_16" = "yes"; then
   AC_DEFINE([HAVE_GCHECKSUM],[1],[Defined if GLib has support for GChecksum])
   AC_DEFINE([HAVE_GSTRCMP0],[1],[Defined if GLib has support for g_strcmp0])
   md5_method="glib"
else
   md5_method="internal implementation"
fi

if test "$scr_resolution" = "desktop"; then
   AC_DEFINE([SCR_RESOLUTION_DESKTOP], [1], [Compiled for a desktop system])
elif test "$scr_resolution" = "800x480"; then
   AC_DEFINE([SCR_RESOLUTION_800X480], [1], [Compiled for 800x480])
elif test "$scr_resolution" = "1024x600"; then
   AC_DEFINE([SCR_RESOLUTION_1024X600], [1], [Compiled for 1024x600])
else
   AC_MSG_ERROR([Unknown screen resolution in the target platform])
fi
AC_SUBST(SCR_RESOLUTION, "$scr_resolution")

AM_CONDITIONAL(HAVE_DBUS_SUPPORT, test -n "$serviceentrydir")
if test -n "$serviceentrydir"; then
   AC_DEFINE([HAVE_DBUS_SUPPORT], [1], [Defined if compiling with D-BUS])
fi

AM_CONDITIONAL(HAVE_CONIC, test "$have_conic" = "yes")
if test "$have_conic" = "yes"; then
   AC_DEFINE([HAVE_CONIC], [1], [Defined if building with libconic])
fi

AM_CONDITIONAL(HAVE_MAEMO_SB_PLUGIN, test "$have_sb_plugin" = "yes")
if test "$have_sb_plugin" = "yes"; then
   AC_DEFINE([HAVE_MAEMO_SB_PLUGIN], [1],
             [Defined if building the Maemo status bar plugin])
   AC_SUBST(SBPLUGIN_LIBS)
   AC_SUBST(SBPLUGIN_CFLAGS)
   AC_SUBST(sbplugindesktopentrydir)
   AC_SUBST(sbplugindir)
fi

if test "$use_libproxy" = "yes"; then
   EXTRA_CFLAGS="$EXTRA_CFLAGS $libproxy_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $libproxy_LIBS"
   AC_DEFINE([HAVE_LIBPROXY],[1],[Defined if using libproxy])
fi

if test "$have_gio" = "yes"; then
   AC_DEFINE([HAVE_GIO], [1], [Defined if building with GIO support])
   EXTRA_CFLAGS="$EXTRA_CFLAGS $GIO_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $GIO_LIBS"
fi

AM_CONDITIONAL(HAVE_TRAY_ICON, test "$have_tray_icon" = "yes")
if test "$have_tray_icon" = "yes"; then
   AC_DEFINE([HAVE_TRAY_ICON], [1], [Defined if building the FD tray icon])
fi

if test "$have_gst_mixer" = "yes"; then
   enable_volume_key_handler="yes"
   AC_DEFINE([HAVE_GST_MIXER], [1], [Defined if buiding with GStreamer mixer])
   EXTRA_CFLAGS="$EXTRA_CFLAGS $Gst_mixer_CFLAGS"
   EXTRA_LIBS="$EXTRA_LIBS $Gst_mixer_LIBS"
fi

if test "$enable_volume_key_handler" = "yes"; then
   AC_DEFINE([ENABLE_VOLUME_KEY_HANDLER], [1],
             [Defined if using a handler for volume keys])
fi

AC_SUBST(AM_MAKEFLAGS, "itlocaledir=\${localedir}")
AC_SUBST(appdatadir)
AC_SUBST(themebigicondir)
AC_SUBST(themesmallicondir)
AC_SUBST(pixmapdir, "\${datadir}/pixmaps")
AC_SUBST(icon48dir, "\${datadir}/icons/hicolor/48x48/apps")
AC_SUBST(serviceentrydir)
AC_SUBST(desktopentrydir)
AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(EXTRA_LIBS)
AC_SUBST(PLATFORM)
AC_SUBST(PLATFORMVERS)

############################
### Produce output files ###
############################
AC_CONFIG_FILES([
   Makefile
   po/Makefile.in
   src/Makefile
   data/vagalume-rfs.sh
   data/vagalume.conf
])
AC_OUTPUT

######################
### Status summary ###
######################
echo ""
echo "****************************************"
echo "*** Vagalume is ready to be compiled ***"
echo "****************************************"
echo ""
echo "Target platform: $PLATFORM $PLATFORMVERS"
echo "GTK+ version: $GTKVERSION"
echo "GStreamer version: $GSTVERSION"
echo "Method to compute MD5 sums: $md5_method"
echo "\"Set IM status\" enabled: $set_im_status"
echo "System-wide proxy support: $use_libproxy"
if test "$PLATFORM" = "gnome"; then
   echo "Tray icon and notifications enabled: $have_tray_icon"
elif test "$PLATFORM" = "maemo"; then
   echo "Maemo status bar plugin enabled: $have_sb_plugin"
   echo "Libconic support: $have_conic"
fi
echo "D-Bus Support enabled:" $use_dbus_glib
echo "Mixer support enabled:" $have_gst_mixer
echo "Target display: $scr_resolution"
echo ""
echo "Type 'make' to compile Vagalume"
echo ""
