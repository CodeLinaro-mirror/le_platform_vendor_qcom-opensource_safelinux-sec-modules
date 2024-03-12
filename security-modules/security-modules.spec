# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%{!?with_oot_debug: %define with_oot_debug 0}

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
%if %{with_oot_debug}
KERNEL_SRC=%{_usrsrc}/kernels/%{kversion}+debug
%else
KERNEL_SRC=%{_usrsrc}/kernels/%{kversion}
%endif
make KDIR=${KERNEL_SRC} modules

%post
%if %{with_oot_debug}
depmod %{kversion}+debug
%else
depmod %{kversion}
%endif

%postun
%if %{with_oot_debug}
depmod %{kversion}+debug
%else
depmod %{kversion}
%endif

%install
%if %{with_oot_debug}
KERNEL_SRC=%{_usrsrc}/kernels/%{kversion}+debug
%else
KERNEL_SRC=%{_usrsrc}/kernels/%{kversion}
%endif
make KDIR=${KERNEL_SRC} INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install
make KDIR=${KERNEL_SRC} HDR_INSTAL_PATH=$RPM_BUILD_ROOT/usr/include headers_install

%if %{with_oot_debug}
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}+debug/modules."*
%else
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}/modules."*
%endif

mkdir -p  "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/tz_log.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/qtee_shmbridge.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/qcom_scm_oot.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/smcinvoke.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"

%clean
rm -rf $RPM_BUILD_ROOT

%files
%if %{with_oot_debug}
%define kernel_module_path /lib/modules/%{kversion}+debug
%else
%define kernel_module_path /lib/modules/%{kversion}
%endif
%{kernel_module_path}/extra/tz_log.ko
%{kernel_module_path}/extra/qtee_shmbridge.ko
%{kernel_module_path}/extra/smcinvoke.ko
%{kernel_module_path}/extra/qcom_scm_oot.ko

%{_includedir}/linux/smcinvoke.h

/usr/lib/modules-load.d/tz_log.conf
/usr/lib/modules-load.d/qtee_shmbridge.conf
/usr/lib/modules-load.d/smcinvoke.conf
/usr/lib/modules-load.d/qcom_scm_oot.conf

%changelog
* Thu Sep 28 2023 Amit Blay <quic_ablay@quicinc.com> 1.0
- Add tz_log, qtee_shmbridge, smcinoke, and qcom_scm_oot modules.
