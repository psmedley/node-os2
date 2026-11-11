# RPM spec for Node.js on OS/2 (ArcaOS / eComStation), built from
# https://github.com/psmedley/node-os2.
#
# Build the source tarball from a node-os2 checkout with:
#   git archive --format=tar.gz --prefix=node-os2-24.22.2/ \
#     -o node-os2-24.22.2.tar.gz main
# put it in your rpmbuild SOURCES directory, then run: rpmbuild -ba nodejs.spec
# (RPM expands macros even in comments, hence the literal values here.)

%global nodejs_version   24.22.2
%global nodejs_release   1
%global npm_version      11.19.1
%global corepack_version 0.36.0

Name:           nodejs
Version:        %{nodejs_version}
Release:        %{nodejs_release}%{?dist}
Summary:        JavaScript runtime built on the V8 JavaScript engine
License:        MIT AND ISC AND BSD-2-Clause AND BSD-3-Clause AND Apache-2.0 AND Unicode-3.0
URL:            https://github.com/psmedley/node-os2
Source0:        node-os2-%{nodejs_version}.tar.gz

BuildRequires:  gcc >= 13
BuildRequires:  gcc-c++ >= 13
BuildRequires:  make >= 4.4
BuildRequires:  python3
BuildRequires:  libcx-devel
BuildRequires:  pthread-devel
BuildRequires:  openssl-devel >= 3
BuildRequires:  zlib-devel

# npm is the default package manager and is expected by most users.
Recommends:     npm = 1:%{npm_version}-%{nodejs_release}%{?dist}

%description
Node.js is a free, open-source, cross-platform JavaScript runtime environment
that lets developers create servers, web apps, command line tools and scripts.

This is a port of Node.js %{nodejs_version} (LTS) to OS/2. Known limitations of the
port: no IPv6 support, no OpenSSL ENGINE support (crypto.setEngine()), and
thread names/priorities are not supported.

%package devel
Summary:        Header files for building Node.js native add-ons
Requires:       %{name}%{?_isa} = %{nodejs_version}-%{nodejs_release}%{?dist}

%description devel
Header files (Node.js, V8, libuv, ...) needed to build native add-ons for
Node.js with node-gyp.

%package -n npm
Summary:        Node.js package manager
Epoch:          1
Version:        %{npm_version}
# Inside this subpackage the Version is npm's, so name node's explicitly.
Requires:       %{name} = %{nodejs_version}-%{nodejs_release}%{?dist}
BuildArch:      noarch

%description -n npm
npm is the package manager for Node.js. It also provides npx, which runs
binaries from npm packages.

%prep
%autosetup -n node-os2-%{nodejs_version}

%build
# Node's build system is gyp/make based; configure picks up OS/2 itself.
# System OpenSSL (without ENGINE support) and zlib are used, bundled ICU
# (full data) provides Intl.
python3 ./configure \
    --prefix=%{_prefix} \
    --shared-openssl \
    --shared-zlib \
    --with-intl=full-icu

make %{?_smp_mflags} V=1

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}

# The gdb/lldb helpers are only useful when debugging V8 itself.
rm -f %{buildroot}%{_datadir}/doc/node/gdbinit
rm -f %{buildroot}%{_datadir}/doc/node/lldb_commands.py

%check
# Quick smoke test that the installed binary starts and has its built-in
# snapshot, OpenSSL and ICU.
%{buildroot}%{_bindir}/node.exe -e "console.log(process.version, process.platform)"
%{buildroot}%{_bindir}/node.exe -e "require('crypto').createHash('sha256').update('x').digest('hex')"
%{buildroot}%{_bindir}/node.exe -e "new Intl.DateTimeFormat('de-DE').format(new Date())"

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/node.exe
%{_bindir}/corepack
%{_bindir}/corepack.cmd
%dir %{_prefix}/lib/node_modules
%{_prefix}/lib/node_modules/corepack/
%{_mandir}/man1/node.1*

%files devel
%{_includedir}/node/

%files -n npm
%{_bindir}/npm
%{_bindir}/npm.cmd
%{_bindir}/npx
%{_bindir}/npx.cmd
%{_prefix}/lib/node_modules/npm/

%changelog
* Tue Nov 11 2026 Paul Smedley <paul@smedley.id.au> - 24.22.2-1
- Initial OS/2 package of Node.js 24.22.2 (LTS), based on the bitwiseworks
  OS/2 patches (psmedley/node-os2-bww) and libuv-os2.
