#Module-Specific definitions
%define mod_version 1.0
%define release 1mdk
%define mod_name mod_watchcat
%define mod_conf 55_%{mod_name}.conf
%define mod_so %{mod_name}.so
%define sourcename %{mod_name}-%{mod_version}

#New ADVX macros
%define ADVXdir %{_datadir}/ADVX
%{expand:%(cat %{ADVXdir}/ADVX-build)}
%{expand:%%global ap_version %(%{apxs} -q ap_version)}

# Standard Module Definitions
%define name %{ap_name}-%{mod_name}
%define version %{ap_version}_%{mod_version}

#Standard ADVX requires
Prereq:		%{ap_name} = %{ap_version}
Prereq:		%{ap_name}-conf
BuildPreReq:	ADVX-build
BuildRequires:	%{ap_name}-devel >= 2.0.43-5mdk
Provides: 	ADVXpackage
Provides:	AP20package

Summary:	Mod_watchcat is a DSO module for the %{ap_name} Web server.
Name:		%{name}
Version:	%{version}
Release:	%{release}
Group:		System/Servers
Source0:	%{sourcename}.tar.bz2
Source1:	%{mod_conf}.bz2
License:	GPL
URL:		http://oss.digirati.com.br/mod_watchcat/
BuildRequires:	libwcat-devel
BuildRoot:	%{_tmppath}/%{name}-buildroot
Requires:	libwcat watchcatd

%description
mod_watchcat allows the Apache webserver to register its processes
with the watchcatd daemon.

%prep

%setup -q -n %{sourcename}

%build
%{apxs} -c mod_watchcat.c -lwcat

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%ADVXinstlib
%ADVXinstconf %{SOURCE1} %{mod_conf}
%ADVXinstdoc %{name}-%{version}

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%post
%ADVXpost

%postun
%ADVXpost

%files
%defattr(-,root,root)
%config(noreplace) %{ap_confd}/%{mod_conf}
%{ap_extralibs}/%{mod_so}
%{ap_webdoc}/*
%doc COPYRIGHT

%changelog
* Fri Feb 05 2003 Andre Nathan <andre@digirati.com.br> 2.0.48_1.0-1mdk
- Adapt to Mandrake style.
* Thu Feb 05 2003 Andre Nathan <andre@digirati.com.br> 2.0.48_1.0-1mdk
- First version.
