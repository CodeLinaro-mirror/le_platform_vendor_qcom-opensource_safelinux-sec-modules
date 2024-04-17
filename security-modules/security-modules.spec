# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%{!?with_oot_debug: %define with_oot_debug 0}

%define kmod_name security-modules

%define debug_package %{nil}

%if %{with_oot_debug}
    %define kpackage kernel-automotive-debug
    %define kversion_with_debug %{kversion}+debug
%else
    %define kpackage kernel-automotive
    %define kversion_with_debug %{kversion}
%endif

Name: %{kmod_name}
Version: 1.0
Release:        1%{?dist}
Summary:security kernel drivers

License: GPLv2
Source0: %{name}-%{version}.tar.gz

BuildRequires: modules-signkey
BuildRequires: kernel-automotive-devel-uname-r = %{kversion_with_debug}
Requires: %{kpackage}-core-uname-r = %{kversion_with_debug}

%description
This is a rpm contains security out of tree kernel modules.

%prep
%setup -qn %{name}

%build
KERNEL_SRC=%{_usrsrc}/kernels/%{kversion_with_debug}
make KDIR=${KERNEL_SRC} modules

%post
depmod %{kversion_with_debug}

%postun
depmod %{kversion_with_debug}

%install
KERNEL_SRC=%{_usrsrc}/kernels/%{kversion_with_debug}
make KDIR=${KERNEL_SRC} INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install
make KDIR=${KERNEL_SRC} HDR_INSTAL_PATH=$RPM_BUILD_ROOT/usr/include headers_install
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion_with_debug}/modules."*

mkdir -p  "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/tz_log.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/qtee_shmbridge.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/qcom_scm_oot.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"
cp modules-load/smcinvoke.conf "$RPM_BUILD_ROOT/usr/lib/modules-load.d"

%clean
rm -rf $RPM_BUILD_ROOT

%files
%define kernel_module_path /lib/modules/%{kversion_with_debug}
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
