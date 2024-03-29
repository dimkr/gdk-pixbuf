# Note that this is NOT a relocatable package
%define name     gdk-pixbuf
%define ver      0.22.0
%define  RELEASE 1
%define  rel     %{?CUSTOM_RELEASE} %{!?CUSTOM_RELEASE:%RELEASE}
%define prefix   /usr

Name: %name
Summary: The GdkPixBuf image handling library
Version: %ver
Release: %rel
Copyright: LGPL
Group: System Environment/Libraries
Source: ftp://ftp.gnome.org/pub/GNOME/sources/%{name}/%{name}-%{ver}.tar.gz
URL: http://www.gnome.org/
BuildRoot: /var/tmp/%{name}-root
#Requires: gtk+ >= 1.2
Docdir: %{prefix}/doc

%description
The GdkPixBuf library provides a number of features, including :

- GdkPixbuf structure for representing images.
- Image loading facilities.
- Rendering of a GdkPixBuf into various formats:
  drawables (windows, pixmaps), GdkRGB buffers.
- Fast scaling and compositing of pixbufs.
- Simple animation loading (ie. animated gifs)

In addition, this module also provides a little libgnomecanvaspixbuf
library, which contains a GNOME Canvas item to display pixbufs with
full affine transformations.

%package devel
Summary: Libraries and include files for developing GdkPixBuf applications.
Group: Development/Libraries
#Requires: %name = %{PACKAGE_VERSION}
#Obsoletes: %name-devel

%description devel
Libraries and include files for developing GdkPixBuf applications.

%changelog
* Sat Jan 22 2000 Ross Golder <rossigee@bigfoot.com>

- Borrowed from gnome-libs to integrate into gdk-pixbuf source tree

%prep
%setup -q

%build
%ifarch alpha
 MYARCH_FLAGS="--host=alpha-redhat-linux"
%endif
# Needed for snapshot releases.

MYCFLAGS="$RPM_OPT_FLAGS"

if [ ! -f configure ]; then
  CFLAGS="$MYCFLAGS" ./autogen.sh $MYARCH_FLAGS --prefix=%prefix --localstatedir=/var/lib --sysconfdir=/etc
else
  CFLAGS="$MYCFLAGS" ./configure $MYARCH_FLAGS --prefix=%prefix --localstatedir=/var/lib --sysconfdir=/etc
fi

if [ "$SMP" != "" ]; then
  make -j$SMP "MAKE=make -j$SMP"
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make sysconfdir=$RPM_BUILD_ROOT/etc prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%post 
if ! grep %{prefix}/lib /etc/ld.so.conf > /dev/null ; then
  echo "%{prefix}/lib" >> /etc/ld.so.conf
fi

/sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS COPYING COPYING.LIB ChangeLog NEWS README TODO doc/*.txt html
%{prefix}/lib/lib*.so.*
%{prefix}/lib/%{name}/loaders/lib*.so*

%files devel
%defattr(-, root, root)

#%doc HACKING MAINTAINERS
%{prefix}/bin/*
%{prefix}/lib/lib*.so
%{prefix}/lib/%{name}/loaders/lib*.a
%{prefix}/lib/*.a
%{prefix}/lib/*.sh
%{prefix}/include/*
%{prefix}/share/aclocal/*
%{prefix}/share/gnome/html/*


