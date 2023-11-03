# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%define kmod_name security-modules

%define debug_package %{nil}

Name: %{kmod_name}
Version: 1.0
Release:        1%{?dist}
Summary:security kernel drivers

License: GPLv2
Source0: %{name}-%{version}.tar.gz

BuildRequires: modules-signkey
BuildRequires: kernel-automotive-devel-uname-r = %{kversion}
Requires: kernel-automotive-core-uname-r = %{kversion}

%description
This is a rpm contains security out of tree kernel modules.

%prep
%setup -qn %{name}

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
make KDIR=${KSRC} modules

%post
depmod %{kversion}

%postun
depmod %{kversion}

%install
make KERNEL_VERSION=%{kversion} INSTALL_MOD_PATH="$RPM_BUILD_ROOT" modules_install
make KERNEL_VERSION=%{kversion} HDR_INSTAL_PATH=$RPM_BUILD_ROOT/usr/include headers_install

rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}/modules."*
mkdir -p  "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/tz_log.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/qtee_shmbridge.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/qcom_scm_oot.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/smcinvoke.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"

%clean
rm -rf $RPM_BUILD_ROOT

%files
/lib/modules/%{kversion}/extra/drivers/tz_log.ko
/lib/modules/%{kversion}/extra/drivers/qtee_shmbridge.ko
/lib/modules/%{kversion}/extra/drivers/smcinvoke.ko
/lib/modules/%{kversion}/extra/drivers/qcom_scm_oot.ko
%{_includedir}/linux/smcinvoke.h

/usr/lib/modules-load.d/tz_log.conf
/usr/lib/modules-load.d/qtee_shmbridge.conf
/usr/lib/modules-load.d/smcinvoke.conf
/usr/lib/modules-load.d/qcom_scm_oot.conf

%changelog
* Thu Sep 28 2023 Amit Blay <quic_ablay@quicinc.com> 1.0
- Add tz_log, qtee_shmbridge, smcinoke, and qcom_scm_oot modules.