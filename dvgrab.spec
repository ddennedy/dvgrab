# spec file to create a RPM package (rpm -ba dvgrab.spec)

Name: 		dvgrab
Version: 	2.1
Release: 	1
Packager:	dan@dennedy.org
Copyright: 	2000-2005, Arne Schirmacher, Charles Yates, Dan Dennedy (GPL)
Group: 		Utilities/System
Source0: 	dvgrab-%{version}.tar.gz
URL: 		http://www.kinodv.org/
Summary: 	A program to copy Digital Video data from a DV camcorder
Requires:	libraw1394 >= 1.2.0
Requires:	libiec1394 >= 1.0.0
Requires:	libavc1394 >= 0.4.1
Requires:	libdv >= 0.103
Prefix:		/usr
BuildRoot: %{_tmppath}/%{name}-buildroot
BuildRequires: libraw1394-devel >= 1.2.0
BuildRequires: libiec1394-devel >= 1.0.0
BuildRequires: libavc1394-devel >= 0.4.1
BuildRequires: libdv-devel >= 0.103

%description
dvgrab copies digital video data from a DV camcorder.


%changelog
* Fri May 18 2001 Arne Schirmacher <dvgrab@schirmacher.de>
- minor change for libraw 0.9, bugfix
* Sat Feb 10 2001 Arne Schirmacher <dvgrab@schirmacher.de>
- initial package


%prep
rm -rf $RPM_BUILD_ROOT

%setup -q


%build
./configure --prefix=/usr
make


%install
mkdir -p %buildroot%{_bindir}
make DESTDIR=%buildroot install

%clean
rm -rf %buildroot


%post


%postun

%files
%defattr(-,root,root)
%{_bindir}/*
%{_mandir}/man1/*


%doc README ChangeLog NEWS
