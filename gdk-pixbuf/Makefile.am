SUBDIRS = pixops

if BUILD_CANVAS_ITEM
CANVAS_PIXBUF_LIB = libgnomecanvaspixbuf.la
CANVAS_PIXBUF_HEADERFILES=gnome-canvas-pixbuf.h
libgnomecanvaspixbuf_la_SOURCES = gnome-canvas-pixbuf.c
EXTRA_GNOME_LIBS = $(GNOME_LIBS)
else
CANVAS_PIXBUF_LIB =
CANVAS_PIXBUF_HEADERFILES =
EXTRA_GNOME_LIBS =
endif

lib_LTLIBRARIES = 		\
	libgdk_pixbuf.la	\
	libgdk_pixbuf_xlib.la	\
	$(CANVAS_PIXBUF_LIB)

libgnomecanvaspixbuf_la_LDFLAGS = -version-info 1:0:0 $(EXTRA_GNOME_LIBS)

loaderdir = $(libdir)/gdk-pixbuf/loaders

#
# The PNG plugin.
#
libpixbufloader_png_la_SOURCES = io-png.c
libpixbufloader_png_la_LDFLAGS = -module -avoid-version
libpixbufloader_png_la_LIBADD = $(LIBPNG) libgdk_pixbuf.la $(GTK_LIBS)

#
# The JPEG loader
#
libpixbufloader_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_LDFLAGS = -module -avoid-version
libpixbufloader_jpeg_la_LIBADD = $(LIBJPEG) libgdk_pixbuf.la $(GTK_LIBS)

#
# The XPM loader
#
libpixbufloader_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_LDFLAGS = -module -avoid-version
libpixbufloader_xpm_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

#
# The GIF loader
#
libpixbufloader_gif_la_SOURCES = io-gif.c
libpixbufloader_gif_la_LDFLAGS = -module -avoid-version
libpixbufloader_gif_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

#
# The ICO loader
#
libpixbufloader_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_LDFLAGS = -module -avoid-version
libpixbufloader_ico_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

#
# The RAS loader
#
libpixbufloader_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_LDFLAGS = -module -avoid-version
libpixbufloader_ras_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

#
# The TIFF loader
#
libpixbufloader_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_LDFLAGS = -module -avoid-version
libpixbufloader_tiff_la_LIBADD = $(LIBTIFF) libgdk_pixbuf.la $(GTK_LIBS)

#
# The PNM loader
#
libpixbufloader_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_LDFLAGS = -module -avoid-version
libpixbufloader_pnm_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

#
# The BMP loader
#
libpixbufloader_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_LDFLAGS = -module -avoid-version
libpixbufloader_bmp_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

#
# The XBM loader
#
libpixbufloader_xbm_la_SOURCES = io-xbm.c
libpixbufloader_xbm_la_LDFLAGS = -module -avoid-version
libpixbufloader_xbm_la_LIBADD = libgdk_pixbuf.la $(GTK_LIBS)

if HAVE_PNG
PNG_LIB = libpixbufloader-png.la
endif

if HAVE_JPEG
JPEG_LIB = libpixbufloader-jpeg.la
endif

GIF_LIB = libpixbufloader-gif.la

ICO_LIB = libpixbufloader-ico.la

RAS_LIB = libpixbufloader-ras.la

if HAVE_TIFF
TIFF_LIB = libpixbufloader-tiff.la
endif

XPM_LIB = libpixbufloader-xpm.la

PNM_LIB = libpixbufloader-pnm.la

BMP_LIB = libpixbufloader-bmp.la

XBM_LIB = libpixbufloader-xbm.la

if BUILD_DYNAMIC_MODULES

loader_LTLIBRARIES = 	\
	$(PNG_LIB)	\
	$(JPEG_LIB)	\
	$(GIF_LIB)	\
	$(ICO_LIB)	\
	$(RAS_LIB)	\
	$(XPM_LIB)	\
	$(TIFF_LIB)	\
	$(PNM_LIB)	\
	$(BMP_LIB)	\
	$(XBM_LIB)


extra_sources = 

else
loader_LTLIBRARIES = 

extra_sources = $(libpixbufloader_png_la_SOURCES) 	\
		$(libpixbufloader_jpeg_la_SOURCES) 	\
		$(libpixbufloader_xpm_la_SOURCES) 	\
		$(libpixbufloader_gif_la_SOURCES) 	\
		$(libpixbufloader_ico_la_SOURCES) 	\
		$(libpixbufloader_ras_la_SOURCES) 	\
		$(libpixbufloader_tiff_la_SOURCES) 	\
		$(libpixbufloader_pnm_la_SOURCES) 	\
		$(libpixbufloader_bmp_la_SOURCES)	\
		$(libpixbufloader_xbm_la_SOURCES)

builtin_libraries = 
endif

noinst_PROGRAMS = test-gdk-pixbuf testpixbuf testpixbuf-drawable testanimation testpixbuf-scale
TESTS = test-gdk-pixbuf

DEPS = libgdk_pixbuf.la
INCLUDES = -I$(top_srcdir) -I$(top_builddir) \
	-I$(top_srcdir)/gdk-pixbuf \
	-I$(top_builddir)/gdk-pixbuf \
	$(X11_CFLAGS) $(GLIB_CFLAGS) $(GTK_CFLAGS) $(GNOME_CFLAGS)

AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(loaderdir)\""

LDADDS = libgdk_pixbuf.la $(GLIB_LIBS) $(GTK_LIBS) $(STATIC_LIB_DEPS)

test_gdk_pixbuf_LDADD = $(LDADDS) $(GNOME_LIBS) -lgmodule
testpixbuf_LDADD = $(LDADDS) $(GNOME_LIBS) -lgmodule
testpixbuf_drawable_LDADD = $(LDADDS) $(GNOME_LIBS)
testpixbuf_scale_LDADD = $(LDADDS) $(GNOME_LIBS)
testanimation_LDADD = $(LDADDS) $(GNOME_LIBS) -lgmodule

GDK_PIXBUF_LIBS = $(GLIB_LIBS) $(GTK_LIBS)

#
# The GdkPixBuf library
#

libgdk_pixbufincludedir = $(includedir)/gdk-pixbuf-1.0/gdk-pixbuf

libgdk_pixbuf_la_SOURCES =		\
	gdk-pixbuf.c			\
	gdk-pixbuf-animation.c		\
	gdk-pixbuf-data.c		\
	gdk-pixbuf-drawable.c		\
	gdk-pixbuf-io.c			\
	gdk-pixbuf-loader.c		\
	gdk-pixbuf-render.c		\
	gdk-pixbuf-scale.c		\
	gdk-pixbuf-util.c		\
	gdk-pixbuf-parse-color.c	\
	$(extra_sources)

libgdk_pixbuf_la_LDFLAGS = -version-info 2:0:0 -export-dynamic $(GLIB_LIBS) $(GTK_LIBS)
libgdk_pixbuf_la_LIBADD = pixops/libpixops.la

#
# The GdkPixbufXlib library
#

libgdk_pixbuf_xlib_la_SOURCES =		\
	gdk-pixbuf.c			\
	gdk-pixbuf-xlib.c		\
	gdk-pixbuf-animation.c		\
	gdk-pixbuf-data.c		\
	gdk-pixbuf-io.c			\
	gdk-pixbuf-xlib-render.c	\
	gdk-pixbuf-xlib-drawable.c	\
	gdk-pixbuf-scale.c		\
	gdk-pixbuf-util.c		\
	gdk-pixbuf-xlibrgb.c		\
	gdk-pixbuf-parse-color.c	\
	$(extra_sources)

libgdk_pixbuf_xlib_la_LDFLAGS = -version-info 2:0:0 -export-dynamic $(GLIB_LIBS)
libgdk_pixbuf_xlib_la_LIBADD = pixops/libpixops.la libgdk_pixbuf.la $(GTK_LIBS)


libgdk_pixbufinclude_HEADERS =		\
	gdk-pixbuf.h			\
	gdk-pixbuf-loader.h		\
	gdk-pixbuf-features.h		\
	gdk-pixbuf-xlib.h		\
	gdk-pixbuf-xlibrgb.h		\
	$(CANVAS_PIXBUF_HEADERFILES)

noinst_HEADERS =			\
	gdk-pixbuf-io.h			\
	gdk-pixbuf-private.h		\
	gdk-pixbuf-xlib-private.h
