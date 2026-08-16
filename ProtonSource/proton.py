#!/usr/bin/env python3

#script to launch Wine with the correct environment

import fcntl
import array
import fnmatch
import os
import shutil
import errno
import platform
import resource
import signal
import stat
import subprocess
import sys
import shlex
import uuid
import json
import time

from ctypes import CDLL
from ctypes import CFUNCTYPE
from ctypes import POINTER
from ctypes import Structure
from ctypes import addressof
from ctypes import cast
from ctypes import get_errno
from ctypes import sizeof
from ctypes import c_int
from ctypes import c_int64
from ctypes import c_uint
from ctypes import c_long
from ctypes import c_char_p
from ctypes import c_void_p
from ctypes import c_size_t
from ctypes import c_ssize_t

from filelock import FileLock
from random import randrange

#To enable debug logging, copy "user_settings.sample.py" to "user_settings.py"
#and edit it if needed.

VARIABLE999="hello binary inspecting people" # dont delete because i want whoever sees the binary code to see this
CURRENT_PREFIX_VERSION="11.0-100"

# Replaced with the real build version by the root build.sh right before the
# nuitka compile; reported via `main --version`.
TUXBLOX_VERSION = "unknown"

PFX="Proton: "
ld_path_var = "LD_LIBRARY_PATH"

def file_exists(s, *, follow_symlinks):
    if follow_symlinks:
        #'exists' returns False on broken symlinks
        return os.path.exists(s)
    #'lexists' returns True on broken symlinks
    return os.path.lexists(s)

def kill_process_group(pgid, sig):
    try:
        os.killpg(pgid, sig)
    except ProcessLookupError:
        pass

def nonzero(s):
    return len(s) > 0 and s != "0"

def prepend_to_env_str(env, variable, prepend_str, separator):
    if variable not in env:
        env[variable] = prepend_str
    else:
        env[variable] = prepend_str + separator + env[variable]

def append_to_env_str(env, variable, append_str, separator):
    if variable not in env:
        env[variable] = append_str
    else:
        env[variable] = env[variable] + separator + append_str

def log(msg):
    try:
        sys.stderr.write(PFX + msg + os.linesep)
        sys.stderr.flush()
    except OSError:
        # e.g. see https://github.com/ValveSoftware/Proton/issues/6277
        # There's not much we can usefully do about this: printing a
        # warning to stderr isn't going to work any better the second time
        pass

def file_is_wine_builtin_dll(path):
    if os.path.islink(path):
        contents = os.readlink(path)
        if os.path.dirname(contents).endswith((
                    '/lib/wine/i386-unix',
                    '/lib/wine/i386-windows',
                    '/lib/wine/x86_64-unix',
                    '/lib/wine/x86_64-windows',
                    '/lib/wine/aarch64-unix',
                    '/lib/wine/aarch64-windows',
                    # old paths
                    '/lib/wine',
                    '/lib/wine/fakedlls',
                    '/lib64/wine',
                    '/lib64/wine/fakedlls',
                    '/lib64/wine/x86_64-unix',
                    '/lib64/wine/x86_64-windows'
                )):
            # This may be a broken link to a dll in a removed Proton install
            return True
    if not file_exists(path, follow_symlinks=True):
        return False
    try:
        sfile = open(path, "rb")
        sfile.seek(0x40)
        tag = sfile.read(20)
        return tag.startswith((b"Wine placeholder DLL", b"Wine builtin DLL"))
    except IOError:
        return False

def makedirs(path):
    try:
        #replace broken symlinks with a new directory
        if os.path.islink(path) and not file_exists(path, follow_symlinks=True):
            os.remove(path)
        os.makedirs(path)
    except OSError:
        #already exists
        pass

def merge_user_dir(src, dst):
    extant_dirs = []
    for src_dir, dirs, files in os.walk(src):
        dst_dir = src_dir.replace(src, dst, 1)

        #as described below, avoid merging game save subdirs, too
        child_of_extant_dir = False
        for dir_ in extant_dirs:
            if dir_ in dst_dir:
                child_of_extant_dir = True
                break
        if child_of_extant_dir:
            continue

        #we only want to copy into directories which don't already exist. games
        #may not react well to two save directory instances being merged.
        if not file_exists(dst_dir, follow_symlinks=True) or os.path.samefile(dst_dir, dst):
            makedirs(dst_dir)
            for dir_ in dirs:
                src_file = os.path.join(src_dir, dir_)
                dst_file = os.path.join(dst_dir, dir_)
                if os.path.islink(src_file) and not file_exists(dst_file, follow_symlinks=True):
                    try_copy(src_file, dst_file, copy_metadata=True, follow_symlinks=False)
            for file_ in files:
                src_file = os.path.join(src_dir, file_)
                dst_file = os.path.join(dst_dir, file_)
                if not file_exists(dst_file, follow_symlinks=True):
                    try_copy(src_file, dst_file, copy_metadata=True, follow_symlinks=False)
        else:
            extant_dirs += dst_dir

def try_copy(src, dst, prefix=None, add_write_perm=True, copy_metadata=False, optional=False,
             follow_symlinks=True, track_file=None, link_debug=False):
    try:
        if prefix is not None:
            dst = os.path.join(prefix, dst)

        if os.path.isdir(dst):
            dst = os.path.join(dst, os.path.basename(src))

        if file_exists(dst, follow_symlinks=False):
            os.remove(dst)
        elif track_file and prefix is not None:
            track_file.write(os.path.relpath(dst, prefix) + '\n')

        if os.path.islink(src) and not follow_symlinks:
            shutil.copyfile(src, dst, follow_symlinks=False)
        else:
            copyfile(src, dst)

        if copy_metadata:
            shutil.copystat(src, dst, follow_symlinks=follow_symlinks)
        else:
            shutil.copymode(src, dst, follow_symlinks=follow_symlinks)

        if add_write_perm:
            new_mode = os.lstat(dst).st_mode | stat.S_IWUSR | stat.S_IWGRP
            os.chmod(dst, new_mode)

        if not file_exists(src + '.debug', follow_symlinks=True):
            link_debug = False

        if file_exists(dst + '.debug', follow_symlinks=False):
            os.remove(dst + '.debug')
        elif link_debug and track_file:
            track_file.write(os.path.relpath(dst + '.debug', prefix) + '\n')

        if link_debug:
            os.symlink(src + '.debug', dst + '.debug')

    except FileNotFoundError as e:
        if optional:
            log('Error while copying to \"' + dst + '\": ' + e.strerror)
        else:
            raise

    except PermissionError as e:
        if e.errno == errno.EPERM:
            #be forgiving about permissions errors; if it's a real problem, things will explode later anyway
            log('Error while copying to \"' + dst + '\": ' + e.strerror)
        else:
            raise

# copy_file_range implementation for old Python versions
__syscall__copy_file_range = None

def copy_file_range_ctypes(fd_in, fd_out, count):
    "Copy data using the copy_file_range syscall through ctypes, assuming x86_64 Linux"
    global __syscall__copy_file_range
    __NR_copy_file_range = 326

    if __syscall__copy_file_range is None:
        c_int64_p = POINTER(c_int64)
        prototype = CFUNCTYPE(c_ssize_t, c_long, c_int, c_int64_p,
            c_int, c_int64_p, c_size_t, c_uint, use_errno=True)
        __syscall__copy_file_range = prototype(('syscall', CDLL(None, use_errno=True)))

    while True:
        ret = __syscall__copy_file_range(__NR_copy_file_range, fd_in, None, fd_out, None, count, 0)
        if ret >= 0 or get_errno() != errno.EINTR:
            break

    if ret < 0:
        raise OSError(get_errno(), errno.errorcode.get(get_errno(), 'unknown'))

    return ret

def copyfile_reflink(srcname, dstname):
    "Copy srcname to dstname, making reflink if possible"
    global copyfile
    with open(srcname, 'rb', buffering=0) as src:
        bytes_to_copy = os.fstat(src.fileno()).st_size
        try:
            with open(dstname, 'wb', buffering=0) as dst:
                while bytes_to_copy > 0:
                    bytes_to_copy -= copy_file_range(src.fileno(), dst.fileno(), bytes_to_copy)
        except OSError as e:
            if e.errno not in (errno.EXDEV, errno.ENOSYS, errno.EINVAL, errno.EOPNOTSUPP):
                raise e
            if e.errno == errno.ENOSYS or e.errno == errno.EOPNOTSUPP:
                copyfile = shutil.copyfile
            shutil.copyfile(srcname, dstname)

if hasattr(os, 'copy_file_range'):
    copyfile = copyfile_reflink
    copy_file_range = os.copy_file_range
elif sys.platform == 'linux' and platform.machine() == 'x86_64' and sizeof(c_void_p) == 8:
    copyfile = copyfile_reflink
    copy_file_range = copy_file_range_ctypes
else:
    copyfile = shutil.copyfile

def try_copyfile(src, dst):
    try:
        if os.path.isdir(dst):
            dst = dst + "/" + os.path.basename(src)
        if file_exists(dst, follow_symlinks=False):
            os.remove(dst)
        copyfile(src, dst)
    except PermissionError as e:
        if e.errno == errno.EPERM:
            #be forgiving about permissions errors; if it's a real problem, things will explode later anyway
            log('Error while copying to \"' + dst + '\": ' + e.strerror)
        else:
            raise

def getmtimestr(*path_fragments):
    path = os.path.join(*path_fragments)
    try:
        return str(os.path.getmtime(path))
    except IOError:
        return "0"

def find_nvidia_wine_dll_dir():
    try:
        libdl = CDLL("libdl.so.2")
    except (OSError):
        return None

    try:
        libglx_nvidia = CDLL("libGLX_nvidia.so.0")
    except OSError:
        return None

    # from dlinfo(3)
    #
    # struct link_map {
    #     ElfW(Addr) l_addr;  /* Difference between the
    #                            address in the ELF file and
    #                            the address in memory */
    #     char      *l_name;  /* Absolute pathname where
    #                            object was found */
    #     ElfW(Dyn) *l_ld;    /* Dynamic section of the
    #                            shared object */
    #     struct link_map *l_next, *l_prev;
    #                         /* Chain of loaded objects */
    #
    #     /* Plus additional fields private to the
    #        implementation */
    # };
    RTLD_DI_LINKMAP = 2
    class link_map(Structure):
        _fields_ = [("l_addr", c_void_p), ("l_name", c_char_p), ("l_ld", c_void_p)]

    # from dlinfo(3)
    #
    # int dlinfo (void *restrict handle, int request, void *restrict info)
    dlinfo_func = libdl.dlinfo
    dlinfo_func.argtypes = c_void_p, c_int, c_void_p
    dlinfo_func.restype = c_int

    # Allocate a link_map object
    glx_nvidia_info_ptr = POINTER(link_map)()

    # Run dlinfo(3) on the handle to libGLX_nvidia.so.0, storing results at the
    # address represented by glx_nvidia_info_ptr
    if dlinfo_func(libglx_nvidia._handle,
                   RTLD_DI_LINKMAP,
                   addressof(glx_nvidia_info_ptr)) != 0:
        return None

    # Grab the contents our of our pointer
    glx_nvidia_info = cast(glx_nvidia_info_ptr, POINTER(link_map)).contents

    # Decode the path to our library to a str()
    if glx_nvidia_info.l_name is None:
        return None
    try:
        libglx_nvidia_path = os.fsdecode(glx_nvidia_info.l_name)
    except UnicodeDecodeError:
        return None

    # Follow any symlinks to the actual file
    libglx_nvidia_realpath = os.path.realpath(libglx_nvidia_path)

    # Go to the relative path ./nvidia/wine from our library
    nvidia_wine_dir = os.path.join(os.path.dirname(libglx_nvidia_realpath), "nvidia", "wine")

    # Check that nvngx.dll exists here, or fail
    if file_exists(os.path.join(nvidia_wine_dir, "nvngx.dll"), follow_symlinks=True):
        return nvidia_wine_dir

    return None

EXT2_IOC_GETFLAGS = 0x80086601
EXT2_IOC_SETFLAGS = 0x40086602

EXT4_CASEFOLD_FL = 0x40000000

def set_dir_casefold_bit(dir_path):
    dr = os.open(dir_path, 0o644)
    if dr < 0:
        return
    try:
        dat = array.array('I', [0])
        if fcntl.ioctl(dr, EXT2_IOC_GETFLAGS, dat, True) >= 0:
            dat[0] = dat[0] | EXT4_CASEFOLD_FL
            fcntl.ioctl(dr, EXT2_IOC_SETFLAGS, dat, False)
    except (OSError, IOError):
        #no problem
        pass
    os.close(dr)

def get_replace_reg_value(file, key, name, new_value=None):
    if not file_exists(file, follow_symlinks=True):
        return None
    replaced = False
    out = None
    if new_value is not None:
        out = open(file + ".new", "w")
    found_key = False
    old_value = None
    namestr="\"" + name + "\"="
    with open(file, "r") as reg_in:
        for line in reg_in:
            if not replaced:
                if line[0] == '[':
                    if found_key:
                        if out is not None:
                            out.close()
                        return None
                    if line[1:len(key) + 1] == key:
                        found_key = True
                elif found_key:
                    idx = line.find(namestr)
                    if idx != -1:
                        old_value = line[idx + len(namestr):]
                        if out is not None:
                            out.write(namestr + new_value)
                            replaced = True
                            continue
                        else:
                            return old_value
            if out is not None:
                out.write(line)

    if out is not None:
        out.close()

    if replaced:
        try:
            os.rename(file, file + ".old")
        except OSError:
            os.remove(file)
            pass

        try:
            os.rename(file + ".new", file)
        except OSError:
            log("Unable to write new registry file to " + file)
            pass

    return old_value

class Proton:
    def __init__(self, base_dir):
        self.base_dir = base_dir + "/"
        self.dist_dir = self.path("files/")
        self.bin_dir = self.path("files/bin/")
        self.lib_dir = self.path("files/lib/")
        self.fonts_dir = self.path("files/share/fonts/")
        self.media_dir = self.path("files/share/media/")
        self.wine_fonts_dir = self.path("files/share/wine/fonts/")
        self.wine_inf = self.path("files/share/wine/wine.inf")
        self.default_pfx_dir = self.path("files/share/default_pfx/")
        self.user_settings_file = self.path("user_settings.py")
        self.wine_bin = self.bin_dir + "wine"
        self.wineserver_bin = self.bin_dir + "wineserver"
        self.dist_lock = FileLock(self.path("dist.lock"), timeout=-1)
        self.host_pe_arch = "x86_64-windows"
        self.wow64_pe_arch = "i386-windows"

        if os.environ.get("PROTON_USE_ARM64", "1") == "1" and \
                platform.machine() == 'aarch64' and \
                file_exists(self.path("files/bin-arm64/"), follow_symlinks=True):
            self.host_pe_arch = "aarch64-windows"
            self.default_pfx_dir = self.path("files/share/default_pfx_arm64/")
            self.bin_dir = self.path("files/bin-arm64/")
            self.wine_bin = self.bin_dir + "wine"
            self.wineserver_bin = self.bin_dir + "wineserver"

    def path(self, d):
        return self.base_dir + d

    def arch_pe_dir(self, d, wow64):
        return self.lib_dir + d + "/" + (self.wow64_pe_arch if wow64 else self.host_pe_arch) + "/"

    def cleanup_legacy_dist(self):
        old_dist_dir = self.path("dist/")
        if file_exists(old_dist_dir, follow_symlinks=True):
            with self.dist_lock:
                if file_exists(old_dist_dir, follow_symlinks=True):
                    shutil.rmtree(old_dist_dir)

    def missing_default_prefix(self):
        '''Check if the default prefix dir is missing. Returns true if missing, false if present'''
        return not os.path.isdir(self.default_pfx_dir)

    def make_default_prefix(self):
        with self.dist_lock:
            local_env = dict(g_session.env)
            if self.missing_default_prefix():
                #make default prefix
                local_env["WINEPREFIX"] = self.default_pfx_dir
                local_env["WINEDEBUG"] = "-all"
                g_session.run_proc([self.wine_bin, "wineboot"], local_env)
                g_session.run_proc([self.wineserver_bin, "-w"], local_env)

class CompatData:
    def __init__(self, compatdata):
        self.base_dir = compatdata + "/"
        self.prefix_dir = self.path("pfx/")
        self.creation_sync_guard = self.path("pfx/creation_sync_guard")
        self.version_file = self.path("version")
        self.config_info_file = self.path("config_info")
        self.fex_config_file = self.path("proton-fex-config.json")
        self.tracked_files_file = self.path("tracked_files")
        self.prefix_lock = FileLock(self.path("pfx.lock"), timeout=-1)
        self.old_machine_guid = None

    def path(self, d):
        return self.base_dir + d

    def remove_tracked_files(self):
        if not file_exists(self.tracked_files_file, follow_symlinks=True):
            log("Prefix has no tracked_files??")
            return

        with open(self.tracked_files_file, "r") as tracked_files:
            dirs = []
            for f in tracked_files:
                path = self.prefix_dir + f.strip()
                if file_exists(path, follow_symlinks=False):
                    if os.path.isfile(path) or os.path.islink(path):
                        os.remove(path)
                    else:
                        dirs.append(path)
            for d in dirs:
                try:
                    os.rmdir(d)
                except OSError:
                    #not empty
                    pass

        os.remove(self.tracked_files_file)
        os.remove(self.version_file)

    def upgrade_pfx(self, old_ver):
        if old_ver == CURRENT_PREFIX_VERSION:
            return

        log("Upgrading prefix from " + str(old_ver) + " to " + CURRENT_PREFIX_VERSION + " (" + self.base_dir + ")")

        if old_ver is None:
            return

        if '-' not in old_ver:
            #How can this happen??
            log("Prefix has an invalid version?! You may want to back up user files and delete this prefix.")
            #If it does, just let the Wine upgrade happen and hope it works...
            return

        try:
            old_proton_ver, old_prefix_ver = old_ver.split('-')
            old_proton_maj, old_proton_min = old_proton_ver.split('.')
            new_proton_ver, _              = CURRENT_PREFIX_VERSION.split('-')
            new_proton_maj, new_proton_min = new_proton_ver.split('.')

            if int(new_proton_maj) < int(old_proton_maj) or \
                    (int(new_proton_maj) == int(old_proton_maj) and \
                     int(new_proton_min) < int(old_proton_min)):
                log("Removing newer prefix")
                self.old_machine_guid = get_replace_reg_value(self.prefix_dir + "system.reg", "Software\\\\Microsoft\\\\Cryptography", "MachineGuid")
                self.remove_tracked_files()
                path = self.prefix_dir + "/drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/version.txt"
                if file_exists(path, follow_symlinks=False) and os.path.isfile(path):
                    os.remove(path)
                if file_exists(self.creation_sync_guard, follow_symlinks=False):
                    os.remove(self.creation_sync_guard)
                return

            if old_proton_ver == "3.7" and old_prefix_ver == "1":
                if not file_exists(self.prefix_dir + "/drive_c/windows/syswow64/kernel32.dll", follow_symlinks=True):
                    #shipped a busted 64-bit-only installation on 20180822. detect and wipe clean
                    log("Detected broken 64-bit-only installation, re-creating prefix.")
                    shutil.rmtree(self.prefix_dir)
                    return

            #replace broken .NET installations with wine-mono support
            if file_exists(self.prefix_dir + "/drive_c/windows/Microsoft.NET/NETFXRepair.exe", follow_symlinks=True) and \
                    file_is_wine_builtin_dll(self.prefix_dir + "/drive_c/windows/system32/mscoree.dll"):
                log("Broken .NET installation detected, switching to wine-mono.")
                #deleting this directory allows wine-mono to work
                shutil.rmtree(self.prefix_dir + "/drive_c/windows/Microsoft.NET")

            #prior to prefix version 4.11-2, all controllers were xbox controllers. wipe out the old registry entries.
            if (int(old_proton_maj) < 4 or (int(old_proton_maj) == 4 and int(old_proton_min) == 11)) and \
                    int(old_prefix_ver) < 2:
                log("Removing old xinput registry entries.")
                with open(self.prefix_dir + "system.reg", "r") as reg_in:
                    with open(self.prefix_dir + "system.reg.new", "w") as reg_out:
                        for line in reg_in:
                            if line[0] == '[' and "CurrentControlSet" in line and "IG_" in line:
                                if "DeviceClasses" in line:
                                    reg_out.write(line.replace("DeviceClasses", "DeviceClasses_old"))
                                elif "Enum" in line:
                                    reg_out.write(line.replace("Enum", "Enum_old"))
                            else:
                                reg_out.write(line)
                try:
                    os.rename(self.prefix_dir + "system.reg", self.prefix_dir + "system.reg.old")
                except OSError:
                    os.remove(self.prefix_dir + "system.reg")
                    pass

                try:
                    os.rename(self.prefix_dir + "system.reg.new", self.prefix_dir + "system.reg")
                except OSError:
                    log("Unable to write new registry file to " + self.prefix_dir + "system.reg")
                    pass

            delete_dde_keys = {}
            # Prior to prefix version 6.3-3, ShellExecute* APIs used DDE.
            # Wipe out old registry entries.
            if int(old_proton_maj) < 6 or (int(old_proton_maj) == 6 and int(old_proton_min) < 3) or \
                    (int(old_proton_maj) == 6 and int(old_proton_min) == 3 and int(old_prefix_ver) < 3):
                delete_dde_keys = {
                    "[Software\\\\Classes\\\\htmlfile\\\\shell\\\\open\\\\ddeexec",
                    "[Software\\\\Classes\\\\pdffile\\\\shell\\\\open\\\\ddeexec",
                    "[Software\\\\Classes\\\\xmlfile\\\\shell\\\\open\\\\ddeexec",
                    "[Software\\\\Classes\\\\ftp\\\\shell\\\\open\\\\ddeexec",
                    "[Software\\\\Classes\\\\http\\\\shell\\\\open\\\\ddeexec",
                    "[Software\\\\Classes\\\\https\\\\shell\\\\open\\\\ddeexec",
                }
                dde_wb = '@="\\"C:\\\\windows\\\\system32\\\\winebrowser.exe\\" -nohome"'
                log("Removing ShellExecute DDE registry entries.")

            delete_keys = {}
            # Remove shadergpures.sys service which was set up in prefix versions before 11
            if int(old_proton_maj) < 11:
                delete_keys = {
                    "[System\\\\ControlSet001\\\\Services\\\\SharedGpuResources]",
                    "[System\\\\CurrentControlSet\\\\Services\\\\SharedGpuResources]",
                }
                log("Removing sharedgpures.sys service.")

            if delete_dde_keys or delete_keys:
                sysreg_fp = self.prefix_dir + "system.reg"
                new_sysreg_fp = self.prefix_dir + "system.reg.new"


                with open(sysreg_fp, "r") as reg_in:
                    with open(new_sysreg_fp, "w") as reg_out:
                        deleting_key = False
                        for line in reg_in:
                            if deleting_key:
                                if line[0] != '[':
                                    continue
                                deleting_key = False
                            if line.split(' ')[0] in delete_keys:
                                deleting_key = True
                                continue
                            if not delete_dde_keys:
                                reg_out.write(line)
                                continue
                            if line[:line.find("ddeexec")+len("ddeexec")] in delete_dde_keys:
                                reg_out.write(line.replace("ddeexec", "ddeexec_old", 1))
                            elif line.rstrip() == dde_wb:
                                reg_out.write(line.replace("-nohome", "%1"))
                            else:
                                reg_out.write(line)

                # Slightly randomize backup file name to avoid colliding with
                # other backups.
                backup_sysreg_fp = "{}system.reg.{:x}.old".format(self.prefix_dir, randrange(16 ** 8))

                try:
                    os.rename(sysreg_fp, backup_sysreg_fp)
                except OSError:
                    log("Failed to back up old system.reg. Simply deleting it.")
                    os.remove(sysreg_fp)
                    pass

                try:
                    os.rename(new_sysreg_fp, sysreg_fp)
                except OSError:
                    log("Unable to write new registry file to " + sysreg_fp)
                    pass

            if int(old_proton_maj) < 10 or (int(old_proton_maj) == 10 and
                                            int(old_proton_min) == 0 and
                                            int(old_prefix_ver) < 105):
                with open(self.creation_sync_guard, "w"):
                    pass
                os.sync()

            stale_builtins = [self.prefix_dir + "/drive_c/windows/system32/amd_ags_x64.dll",
                              self.prefix_dir + "/drive_c/windows/syswow64/amd_ags_x64.dll",
                              self.prefix_dir + "/drive_c/windows/system32/ir50_32.dll",
                              self.prefix_dir + "/drive_c/windows/syswow64/ir50_32.dll" ]
            for builtin in stale_builtins:
                if file_exists(builtin, follow_symlinks=False) and file_is_wine_builtin_dll(builtin):
                    log("Removing stale builtin " + builtin)
                    os.remove(builtin)

            stale_vkd3d = [self.prefix_dir + "/drive_c/windows/system32/libvkd3d-1.dll",
                           self.prefix_dir + "/drive_c/windows/syswow64/libvkd3d-1.dll",
                           self.prefix_dir + "/drive_c/windows/system32/libvkd3d-shader-1.dll",
                           self.prefix_dir + "/drive_c/windows/syswow64/libvkd3d-shader-1.dll" ]
            for dll in stale_vkd3d:
                if file_exists(dll, follow_symlinks=False):
                    log("Removing stale vkd3d dll " + dll)
                    os.remove(dll)

        except ValueError:
            log("Prefix has an invalid version?! You may want to back up user files and delete this prefix.")
            #Just let the Wine upgrade happen and hope it works...
            with open(self.creation_sync_guard, "w"):
                pass
            os.sync()
            return

    def pfx_copy(self, src, dst, dll_copy=False):
        if os.path.islink(src):
            contents = os.readlink(src)
            if os.path.dirname(contents).endswith(('/lib/wine/i386-unix', '/lib/wine/i386-windows',
                                                   '/lib/wine/x86_64-unix', '/lib/wine/x86_64-windows',
                                                   '/lib/wine/aarch64-unix', '/lib/wine/aarch64-windows',
                                                   # old paths:
                                                   '/lib64/wine/x86_64-unix', '/lib64/wine/x86_64-windows')):
                # wine builtin dll
                # make the destination an absolute symlink
                contents = os.path.normpath(os.path.join(os.path.dirname(src), contents))
            if dll_copy:
                try_copyfile(src, dst)
            else:
                os.symlink(contents, dst)
        else:
            try_copyfile(src, dst)

    def copy_pfx(self):
        with open(self.tracked_files_file, "w") as tracked_files:
            for src_dir, dirs, files in os.walk(g_proton.default_pfx_dir):
                rel_dir = src_dir.replace(g_proton.default_pfx_dir, "", 1).lstrip('/')
                if len(rel_dir) > 0:
                    rel_dir = rel_dir + "/"
                dst_dir = src_dir.replace(g_proton.default_pfx_dir, self.prefix_dir, 1)
                if not file_exists(dst_dir, follow_symlinks=True):
                    makedirs(dst_dir)
                    tracked_files.write(rel_dir + "\n")
                for dir_ in dirs:
                    src_file = os.path.join(src_dir, dir_)
                    dst_file = os.path.join(dst_dir, dir_)
                    if os.path.islink(src_file) and not file_exists(dst_file, follow_symlinks=True):
                        self.pfx_copy(src_file, dst_file)
                for file_ in files:
                    src_file = os.path.join(src_dir, file_)
                    dst_file = os.path.join(dst_dir, file_)
                    if not file_exists(dst_file, follow_symlinks=True):
                        self.pfx_copy(src_file, dst_file)
                        tracked_files.write(rel_dir + file_ + "\n")
        # Set .update-timestamp so Wine doesn't try to update the prefix.
        # This is needed in case the mtime of wine.inf has changed in distribution.
        with open(os.path.join(self.prefix_dir, '.update-timestamp'), 'w') as update_timestamp:
            mtime = int(os.stat(g_proton.wine_inf).st_mtime)
            update_timestamp.write(str(mtime))

    def update_builtin_libs(self, dll_copy_patterns):
        dll_copy_patterns = dll_copy_patterns.split(',')
        prev_tracked_files = set()
        with open(self.tracked_files_file, "r") as tracked_files:
            for line in tracked_files:
                prev_tracked_files.add(line.strip())
        with open(self.tracked_files_file, "a") as tracked_files:
            for src_dir, _, files in os.walk(g_proton.default_pfx_dir):
                rel_dir = src_dir.replace(g_proton.default_pfx_dir, "", 1).lstrip('/')
                if len(rel_dir) > 0:
                    rel_dir = rel_dir + "/"
                dst_dir = src_dir.replace(g_proton.default_pfx_dir, self.prefix_dir, 1)
                if not file_exists(dst_dir, follow_symlinks=True):
                    makedirs(dst_dir)
                    tracked_files.write(rel_dir + "\n")
                for file_ in files:
                    src_file = os.path.join(src_dir, file_)
                    dst_file = os.path.join(dst_dir, file_)
                    if not file_is_wine_builtin_dll(src_file):
                        # Not a builtin library
                        continue
                    if file_is_wine_builtin_dll(dst_file):
                        os.unlink(dst_file)
                    elif file_exists(dst_file, follow_symlinks=False):
                        # builtin library was replaced
                        continue
                    else:
                        os.makedirs(dst_dir, exist_ok=True)
                    dll_copy = any(fnmatch.fnmatch(file_, pattern) for pattern in dll_copy_patterns)
                    self.pfx_copy(src_file, dst_file, dll_copy)
                    tracked_name = rel_dir + file_
                    if tracked_name not in prev_tracked_files:
                        tracked_files.write(tracked_name + "\n")

    def create_symlink(self, lname, fname):
        if file_exists(lname, follow_symlinks=False):
            if os.path.islink(lname):
                os.remove(lname)
                os.symlink(fname, lname)
        else:
            os.symlink(fname, lname)

    def create_fonts_symlinks(self):
        windowsfonts = self.prefix_dir + "/drive_c/windows/Fonts"
        makedirs(windowsfonts)
        for fonts_dir in [g_proton.fonts_dir, g_proton.wine_fonts_dir]:
            for font in os.listdir(fonts_dir):
                if not font.endswith('.ttf') and not font.endswith('.ttc'):
                    continue
                lname = os.path.join(windowsfonts, font)
                fname = os.path.join(fonts_dir, font)
                self.create_symlink(lname, fname)

    def migrate_user_paths(self):
        #move winxp-style paths to vista+ paths. we can't do this in
        #upgrade_pfx because the running app may drop files here at any time.
        for (old, new, link) in \
                [
                    ("drive_c/users/user/Local Settings/Application Data",
                        self.prefix_dir + "drive_c/users/user/AppData/Local",
                        "../AppData/Local"),
                    ("drive_c/users/user/Application Data",
                        self.prefix_dir + "drive_c/users/user/AppData/Roaming",
                        "./AppData/Roaming"),
                    ("drive_c/users/user/My Documents",
                        self.prefix_dir + "drive_c/users/user/Documents",
                        "./Documents"),
                ]:

            #running unofficial Proton/Wine builds against a Proton prefix could
            #create an infinite symlink loop. detect this and clean it up.
            if file_exists(new, follow_symlinks=False) and os.path.islink(new) and os.readlink(new).endswith(old):
                os.remove(new)

            old = self.prefix_dir + old

            if file_exists(old, follow_symlinks=False) and not os.path.islink(old):
                merge_user_dir(src=old, dst=new)
                os.rename(old, old + " BACKUP")
            if not file_exists(old, follow_symlinks=False):
                makedirs(os.path.dirname(old))
                os.symlink(src=link, dst=old)
            elif os.path.islink(old) and not (os.readlink(old) == link):
                os.remove(old)
                os.symlink(src=link, dst=old)

    def setup_prefix(self):
        with self.prefix_lock:
            if file_exists(self.version_file, follow_symlinks=True):
                with open(self.version_file, "r") as f:
                    old_ver = f.readline().strip()
            else:
                old_ver = None

            self.upgrade_pfx(old_ver)

            if not file_exists(self.creation_sync_guard, follow_symlinks=False):
                makedirs(self.prefix_dir + "/drive_c")
                set_dir_casefold_bit(self.prefix_dir + "/drive_c")

                self.copy_pfx()
                machine_guid = self.old_machine_guid
                if machine_guid is None:
                    machine_guid = "\"" + str(uuid.uuid4()) + "\""
                get_replace_reg_value(self.prefix_dir + "system.reg", "Software\\\\Microsoft\\\\Cryptography", "MachineGuid", machine_guid)
                os.sync()

                with open(self.creation_sync_guard, "w"):
                    pass
                os.sync()

            self.migrate_user_paths()

            if not file_exists(self.prefix_dir + "/dosdevices/c:", follow_symlinks=False):
                os.symlink("../drive_c", self.prefix_dir + "/dosdevices/c:")

            # No Z: -- mapping the whole host root as a drive letter is one of
            # the most common Wine-detection heuristics (enumerate drives,
            # walk one, look for /proc or /etc). This launcher only ever runs
            # Roblox, which lives entirely under C:, so nothing legitimately
            # needs Z:. Absolute unix paths still resolve via ntdll's
            # drive-independent \??\unix\ fallback, so this doesn't break
            # launching anything through wine.

            # collect configuration info
            use_wined3d = "wined3d" in g_session.compat_config
            use_dxvk_dxgi = not use_wined3d and \
                    not ("WINEDLLOVERRIDES" in g_session.env and "dxgi=b" in g_session.env["WINEDLLOVERRIDES"])
            use_nvapi = (('disablenvapi' not in g_session.compat_config or 'forcenvapi' in g_session.compat_config) and
                        g_proton.host_pe_arch != "aarch64-windows")
            use_dxvk_d3d8 = "dxvkd3d8" in g_session.compat_config

            builtin_dll_copy = os.environ.get("PROTON_DLL_COPY",
                    #dxsetup redist
                    "d3dcompiler_*.dll," +
                    "d3dcsx*.dll," +
                    "d3dx*.dll," +
                    "dx8vb.dll," +
                    "x3daudio*.dll," +
                    "xactengine*.dll," +
                    "xapofx*.dll," +
                    "xaudio*.dll," +
                    "xinput*.dll," +

                    #vcruntime redist
                    "atl1*.dll," +
                    "atl.dll," +
                    "concrt*.dll," +
                    "msvcp1*.dll," +
                    "msvcrt*.dll," +
                    "msvcp7*.dll," +
                    "msvcp6*.dll," +
                    "msvcp_win.dll," +
                    "msvcr1*.dll," +
                    "msvcrt*.dll," +
                    "msvcr7*.dll," +
                    "vcamp1*.dll," +
                    "vcomp1*.dll," +
                    "vccorlib1*.dll," +
                    "vcruntime1*.dll," +
                    "ucrtbase.dll," +

                    #there are different instances (comctl32.dll in system32 and a copy of comctl32_v6.dll
                    #in winsxs, handle that by leaving a copy)
                    "comctl32.dll," +

                    #some games balk at ntdll symlink(?)
                    "ntdll.dll," +

                    #some games require official vulkan loader
                    "vulkan-1.dll," +

                    #let the games install native
                    "ir50_32.dll"
                    )

            # If any of this info changes, we must rerun the tasks below
            prefix_info = '\n'.join((
                CURRENT_PREFIX_VERSION,
                g_proton.fonts_dir,
                g_proton.lib_dir,
                g_proton.default_pfx_dir,
                getmtimestr(g_proton.default_pfx_dir, 'system.reg'),
                str(use_wined3d),
                str(use_dxvk_dxgi),
                builtin_dll_copy,
                str(use_nvapi),
                str(use_dxvk_d3d8),
            ))

            # check whether any prefix config has changed
            try:
                with open(self.config_info_file, "r") as f:
                    old_prefix_info = f.read()
            except IOError:
                old_prefix_info = ""

            if old_ver != CURRENT_PREFIX_VERSION or old_prefix_info != prefix_info:
                # update builtin dll symlinks or copies
                self.update_builtin_libs(builtin_dll_copy)

                with open(self.config_info_file, "w") as f:
                    f.write(prefix_info)

            with open(self.version_file, "w") as f:
                f.write(CURRENT_PREFIX_VERSION + "\n")

            #create font files symlinks
            self.create_fonts_symlinks()

            with open(self.tracked_files_file, "a") as tracked_files:
                #copy openvr files into place
                # !!!REMOVED

                if use_wined3d:
                    dxvkfiles = []
                    vkd3d_protonfiles = []
                    wined3dfiles = ["d3d12", "d3d11", "d3d10", "d3d10core", "d3d10_1", "d3d9"]
                else:
                    dxvkfiles = ["d3d11", "d3d10core", "d3d9"]
                    vkd3d_protonfiles = ["d3d12", "d3d12core"]
                    wined3dfiles = []

                if use_dxvk_dxgi:
                    dxvkfiles.append("dxgi")
                else:
                    wined3dfiles.append("dxgi")

                if use_dxvk_d3d8:
                    dxvkfiles.append("d3d8")
                else:
                    wined3dfiles.append("d3d8")

                icufiles = ["icuin68", "icuuc68", "icudt68"]

                for f in wined3dfiles:
                    try_copy(g_proton.default_pfx_dir + "drive_c/windows/system32/" + f + ".dll", "drive_c/windows/system32",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)
                    try_copy(g_proton.default_pfx_dir + "drive_c/windows/syswow64/" + f + ".dll", "drive_c/windows/syswow64",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)

                for f in dxvkfiles:
                    try_copy(g_proton.arch_pe_dir("wine/dxvk", False) + f + ".dll", "drive_c/windows/system32",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)
                    try_copy(g_proton.arch_pe_dir("wine/dxvk", True) + f + ".dll", "drive_c/windows/syswow64",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)
                    g_session.dlloverrides[f] = "n"

                for f in vkd3d_protonfiles:
                    optional = False
                    if f == "d3d12core":
                        optional = True
                    try_copy(g_proton.arch_pe_dir("wine/vkd3d-proton", False) + f + ".dll", "drive_c/windows/system32",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True, optional=optional)
                    try_copy(g_proton.arch_pe_dir("wine/vkd3d-proton", True) + f + ".dll", "drive_c/windows/syswow64",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True, optional=optional)
                    g_session.dlloverrides[f] = "n"

                for f in icufiles:
                    dst = "drive_c/windows/system32/" + f + ".dll"
                    if not file_exists(self.prefix_dir + dst, follow_symlinks=False):
                        tracked_files.write(dst + '\n')
                    self.create_symlink(self.prefix_dir + dst, g_proton.arch_pe_dir("wine/icu", False) + f + ".dll")

                    dst = "drive_c/windows/syswow64/" + f + ".dll"
                    if not file_exists(self.prefix_dir + dst, follow_symlinks=False):
                        tracked_files.write(dst + '\n')
                    self.create_symlink(self.prefix_dir + dst, g_proton.arch_pe_dir("wine/icu", True) + f + ".dll")

                # If the user requested the NVAPI be available, copy it into place.
                # If they didn't, clean up any stray nvapi DLLs.
                if use_nvapi:
                    try_copy(g_proton.arch_pe_dir("wine/nvapi", False) + "nvapi64.dll", "drive_c/windows/system32",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)
                    try_copy(g_proton.arch_pe_dir("wine/nvapi", False) + "nvofapi64.dll", "drive_c/windows/system32",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)
                    try_copy(g_proton.arch_pe_dir("wine/nvapi", True) + "nvapi.dll", "drive_c/windows/syswow64",
                            prefix=self.prefix_dir, track_file=tracked_files, link_debug=True)
                    g_session.dlloverrides["nvapi64"] = "n"
                    g_session.dlloverrides["nvofapi64"] = "n"
                    g_session.dlloverrides["nvapi"] = "n"
                    g_session.dlloverrides["nvcuda"] = "b"
                else:
                    nvapi64_dll = self.prefix_dir + "drive_c/windows/system32/nvapi64.dll"
                    nvapi32_dll = self.prefix_dir + "drive_c/windows/syswow64/nvapi.dll"
                    if file_exists(nvapi64_dll, follow_symlinks=False):
                        os.unlink(nvapi64_dll)
                    if file_exists(nvapi64_dll + '.debug', follow_symlinks=False):
                        os.unlink(nvapi64_dll + '.debug')
                    if file_exists(nvapi32_dll, follow_symlinks=False):
                        os.unlink(nvapi32_dll)
                    if file_exists(nvapi32_dll + '.debug', follow_symlinks=False):
                        os.unlink(nvapi32_dll + '.debug')

                # Try to detect known DLLs that ship with the NVIDIA Linux Driver
                # and add them into the prefix
                if g_session.nvidia_wine_dll_dir:
                    for dll in ["_nvngx.dll", "nvngx.dll"]:
                        try_copy(g_session.nvidia_wine_dll_dir + "/" + dll,
                                 "drive_c/windows/system32",
                                 optional=True,
                                 prefix=self.prefix_dir,
                                 track_file=tracked_files,
                                 link_debug=True)


class Session:
    def __init__(self):
        self.log_file = None
        self.env = dict(os.environ)
        self.dlloverrides = {
                "dotnetfx35.exe": "b", #replace the broken installer, as does Windows
                "dotnetfx35setup.exe": "b",
                "beclient.dll": "b,n",
                "beclient_x64.dll": "b,n",
                "winebth.sys": "d", #disable winebth.sys as it crashes winedevice.exe
        }

        self.dlloverrides["opencl"] = "n,d"

        self.compat_config = set()
        self.cmdlineappend = []

        #large address aware on by default
        self.compat_config.add("forcelgadd")

        if "PROTON_CPU_TOPOLOGY" in self.env:
            self.env["WINE_CPU_TOPOLOGY"] = self.env["PROTON_CPU_TOPOLOGY"]

        if "WINE_HIDE_AMD_GPU" not in self.env and appid in [
                    "1282690",
                ]:
            self.env["WINE_HIDE_AMD_GPU"] = "1"

        if "WINE_DISABLE_GAMESCOPE_MAX_SIZE_HACK" not in self.env and appid in [
                    "3754990",
                ]:
            self.env["WINE_DISABLE_GAMESCOPE_MAX_SIZE_HACK"] = "1"

    def init_wine(self):
        self.env.pop("LC_ALL", "")

        self.env.pop("WINEARCH", "")
        if os.environ.get("PROTON_USE_WOW64", None) == "1" and platform.machine() == "x86_64":
            self.env["WINEARCH"] = "wow64"

        if 'ORIG_'+ld_path_var not in os.environ:
            # Allow wine to restore this when calling an external app.
            self.env['ORIG_'+ld_path_var] = os.environ.get(ld_path_var, '')

        ld_library_path = [g_proton.lib_dir + "x86_64-linux-gnu", g_proton.lib_dir + "aarch64-linux-gnu", g_proton.lib_dir + "i386-linux-gnu"]
        prepend_to_env_str(self.env, ld_path_var, ':'.join(ld_library_path), ":")

        dllpaths = [g_proton.lib_dir + "vkd3d", g_proton.lib_dir + "wine"]
        if "WINEDLLPATH" in os.environ:
            dllpaths.append(os.environ["WINEDLLPATH"])
        self.env["WINEDLLPATH"] = ':'.join(dllpaths)

        self.env["GST_PLUGIN_SYSTEM_PATH_1_0"] = ':'.join([lib + "/gstreamer-1.0" for lib in ld_library_path])
        self.env["WINE_GST_REGISTRY_DIR"] = g_compatdata.path("gstreamer-1.0/")

        # Root of the Plan-1 WebKitGTK bundle ($(DST_LIBDIR)/tuxblox-webview
        # in ProtonSource/Makefile.in). webview2loader's unixlib.c derives
        # every WebKit-specific relocation env var (WEBKIT_EXEC_PATH etc,
        # see webkitgtk-bundle/README.md's env-var contract table) from this
        # single value at first use, rather than each being set here too.
        self.env["TUXBLOX_WEBVIEW_DIR"] = g_proton.lib_dir + "tuxblox-webview/"

        if os.environ.get("PROTON_MEDIACONV_NO_VIDEO", "0") != "0":
            self.env["MEDIACONV_BLANK_VIDEO_FILE"] = g_proton.media_dir + "blank.mka"
        else:
            self.env["MEDIACONV_BLANK_VIDEO_FILE"] = g_proton.media_dir + "blank.mkv"
        self.env["MEDIACONV_BLANK_AUDIO_FILE"] = g_proton.media_dir + "blank.ptna"

        self.env["ESPEAK_DATA_PATH"] = g_proton.dist_dir + "share"

        prepend_to_env_str(self.env, "PATH", g_proton.bin_dir, ":")

    def check_environment(self, env_name, config_name):
        if env_name not in self.env:
            return False
        if nonzero(self.env[env_name]):
            self.compat_config.add(config_name)
        else:
            self.compat_config.discard(config_name)
        return True

    def try_log_slr_versions(self):
        try:
            if "PRESSURE_VESSEL_RUNTIME_BASE" in self.env:
                with open(self.env["PRESSURE_VESSEL_RUNTIME_BASE"] + "/VERSIONS.txt", "r") as f:
                    for line in f:
                        line = line.strip()
                        if len(line) > 0 and not line.startswith("#"):
                            cleaned = line.split("#")[0].strip().replace("\t", " ")
                            split = cleaned.split(" ", maxsplit=1)
                            self.log_file.write(split[0] + ": " + split[1] + "\n")
        except (OSError, IOError, TypeError, KeyError):
            pass

    def setup_logging(self, *, append_forever):
        basedir = self.env.get("PROTON_LOG_DIR", os.environ["HOME"])
        lfile_path = basedir + "/proton.log"

        if not append_forever:
            if file_exists(lfile_path, follow_symlinks=False):
                os.remove(lfile_path)

        makedirs(basedir)
        self.log_file = open(lfile_path, "a")
        return True

    def log_enabled_for(self, target: str, default_value: bool = False) -> bool:
        log_list = self.env["PROTON_LOG"]
        if nonzero(log_list):
            for entry in log_list.split(','):
                if entry[1:] == target:
                    if entry.startswith('+'):
                        return True
                    elif entry.startswith('-'):
                        return False
                    break
            return default_value # logging enabled, return specified default when not found
        return False # logging disabled, all false

    def generate_fex_app_config(self):
        """
        Translate some environment variables into FEX config which is in files.
        """
        app_config = {
            "Config": {},
            "ThunksDB": {}
        }

        if "PROTON_LOG" in self.env:
            app_config["Config"]["SilentLog"] = "0" if self.log_enabled_for("fex", True) else "1"

        return app_config

    def init_session(self, update_prefix_files):
        self.env["WINEPREFIX"] = g_compatdata.prefix_dir

        #load environment overrides
        used_user_settings = {}
        if file_exists(g_proton.user_settings_file, follow_symlinks=True):
            try:
                import user_settings # pyright: ignore [reportMissingImports]
                for key, value in user_settings.user_settings.items():
                    if key not in self.env:
                        self.env[key] = value
                        used_user_settings[key] = value
            except Exception:
                log("************************************************")
                log("THERE IS AN ERROR IN YOUR user_settings.py FILE:")
                log("%s" % sys.exc_info()[1])
                log("************************************************")

        if "PROTON_LOG" in self.env and nonzero(self.env["PROTON_LOG"]):
            self.env.setdefault("WINEDEBUG", "+timestamp,+pid,+tid,+seh,+unwind,+threadname,+debugstr,+loaddll,+mscoree")
            self.env.setdefault("DXVK_LOG_LEVEL", "info")
            self.env.setdefault("DXVK_NVAPI_LOG_LEVEL", "info")
            self.env.setdefault("VKD3D_DEBUG", "warn")
            self.env.setdefault("VKD3D_SHADER_DEBUG", "fixme")
            self.env.setdefault("WINE_MONO_TRACE", "E:System.NotImplementedException")

            if self.env["PROTON_LOG"] != "1":
                append_to_env_str(self.env, "WINEDEBUG", self.env["PROTON_LOG"], ",")

        #for performance, logging is disabled by default; override with user_settings.py
        self.env.setdefault("WINEDEBUG", "-all")
        self.env.setdefault("DXVK_LOG_LEVEL", "none")
        self.env.setdefault("VKD3D_DEBUG", "none")
        self.env.setdefault("VKD3D_SHADER_DEBUG", "none")

        if "wined3d11" in self.compat_config:
            self.compat_config.add("wined3d")

        if not self.check_environment("PROTON_USE_WINED3D", "wined3d"):
            self.check_environment("PROTON_USE_WINED3D11", "wined3d")
        self.check_environment("PROTON_DXVK_D3D8", "dxvkd3d8")
        self.check_environment("PROTON_NO_D3D11", "nod3d11")
        self.check_environment("PROTON_NO_D3D10", "nod3d10")
        self.check_environment("PROTON_NO_FSYNC", "nofsync")
        self.check_environment("PROTON_FORCE_LARGE_ADDRESS_AWARE", "forcelgadd")
        self.check_environment("PROTON_OLD_GL_STRING", "oldglstr")
        self.check_environment("PROTON_HIDE_NVIDIA_GPU", "hidenvgpu")
        self.check_environment("PROTON_HIDE_VANGOGH_GPU", "hidevggpu")
        self.check_environment("PROTON_HIDE_INTEL_GPU", "hideintelgpu")
        self.check_environment("PROTON_NO_XIM", "noxim")
        self.check_environment("PROTON_HEAP_DELAY_FREE", "heapdelayfree")
        self.check_environment("PROTON_HEAP_ZERO_MEMORY", "heapzeromemory")
        self.check_environment("PROTON_DISABLE_NVAPI", "disablenvapi")
        self.check_environment("PROTON_FORCE_NVAPI", "forcenvapi")
        self.check_environment("PROTON_HIDE_APU", "hideapu")

        if "nofsync" in self.compat_config:
            self.env.pop("WINEFSYNC", "")
        else:
            self.env["WINEFSYNC"] = "1"

        if "oldglstr" in self.compat_config:
            #mesa override
            self.env["MESA_EXTENSION_MAX_YEAR"] = "2003"
            #nvidia override
            self.env["__GL_ExtensionStringVersion"] = "17700"

        if "forcelgadd" in self.compat_config:
            self.env["WINE_LARGE_ADDRESS_AWARE"] = "1"
        else:
            if "noforcelgadd" in self.compat_config:
                self.env["WINE_LARGE_ADDRESS_AWARE"] = "0"

        if "heapdelayfree" in self.compat_config:
            self.env["WINE_HEAP_DELAY_FREE"] = "1"

        if "heapzeromemory" in self.compat_config:
            self.env["WINE_HEAP_ZERO_MEMORY"] = "1"

        if "heaptopdown" in self.compat_config:
            self.env["WINE_HEAP_TOP_DOWN"] = "1"

        if "vkd3dbindlesstb" in self.compat_config:
            append_to_env_str(self.env, "VKD3D_CONFIG", "force_bindless_texel_buffer", ",")

        if "vkd3dfl12" in self.compat_config:
            if "VKD3D_FEATURE_LEVEL" not in self.env:
                self.env["VKD3D_FEATURE_LEVEL"] = "12_0"

        if "hidevggpu" in self.compat_config:
            self.env["WINE_HIDE_VANGOGH_GPU"] = "1"

        if "hidenvgpu" in self.compat_config and "forcenvapi" not in self.compat_config:
            self.env["WINE_HIDE_NVIDIA_GPU"] = "1"

        if "hideintelgpu" in self.compat_config:
            self.env["WINE_HIDE_INTEL_GPU"] = "1"

        if "hideapu" in self.compat_config:
            self.env["WINE_HIDE_APU"] = "1"

        if "usenativexinput13" in self.compat_config:
            self.dlloverrides["xinput1_3"] = "n"

        if "disablelibglesv2" in self.compat_config:
            self.dlloverrides["libglesv2"] = "d"

        if "nomfdxgiman" in self.compat_config:
            self.env["WINE_DO_NOT_CREATE_DXGI_DEVICE_MANAGER"] = "1"

        if "noopwr" in self.compat_config:
            self.env["WINE_DISABLE_VULKAN_OPWR"] = "1"

        # xalia (gamepad-UI helper) is not shipped; keep it off unless the
        # user forces it via env.
        if "PROTON_USE_XALIA" not in self.env:
            self.env["PROTON_USE_XALIA"] = "0"

        if "nohardwarescheduling" in self.compat_config and "WINE_DISABLE_HARDWARE_SCHEDULING" not in self.env:
            self.env["WINE_DISABLE_HARDWARE_SCHEDULING"] = "1"

        if "PROTON_CRASH_REPORT_DIR" in self.env:
            self.env["WINE_CRASH_REPORT_DIR"] = self.env["PROTON_CRASH_REPORT_DIR"]

        if "fnad3d11" in self.compat_config and "FNA3D_FORCE_DRIVER" not in self.env:
            self.env["FNA3D_FORCE_DRIVER"] = "D3D11"

        if "__GLVND_DISALLOW_PATCHING" not in self.env:
            self.env["__GLVND_DISALLOW_PATCHING"] = "1"

        self.env.setdefault("WINE_MONO_HIDETYPES", "0")

        # NVIDIA software may check for the "DriverStore" by querying the
        # NGXCore\NGXPath registry key via `D3DDDI_QUERYREGISTRY_SERVICEKEY` for
        # a given adapter. In the case where this path cannot be found, the
        # `NVIDIA_WINE_DLL_DIR` environment variable is read as a fallback.
        #
        # TODO: Add support for populating NGXCore\NGXPath so we can remove the
        # NGX copies done in setup_prefix(), and this environment variable.
        self.nvidia_wine_dll_dir = find_nvidia_wine_dll_dir()
        if self.nvidia_wine_dll_dir:
            self.env["NVIDIA_WINE_DLL_DIR"] = self.nvidia_wine_dll_dir

        if "PROTON_LOG" in self.env and nonzero(self.env["PROTON_LOG"]):
            if self.setup_logging(append_forever=False):
                self.log_file.write("======================\n")
                self.log_file.write("Proton: " + TUXBLOX_VERSION + "\n")
                self.log_file.write("Command: " + str(sys.argv[2:] + self.cmdlineappend) + "\n")
                self.log_file.write("Options: " + str(self.compat_config) + "\n")

                self.try_log_slr_versions()

                try:
                    uname = os.uname()
                    kernel_version = f"{uname.sysname} {uname.release} {uname.version} {uname.machine}"
                except OSError:
                    kernel_version = "unknown"

                self.log_file.write(f"Kernel: {kernel_version}\n")
                self.log_file.write("Language: LC_ALL " + str(self.env.get("HOST_LC_ALL", None)) +
                                    ", LC_MESSAGES " + str(self.env.get("LC_MESSAGES", None)) +
                                    ", LC_CTYPE " + str(self.env.get("LC_CTYPE", None)) + "\n")
                self.log_file.write("PATH: " + str(self.env.get("PATH", None)) + "\n")

                #dump some important variables into the log header
                for var in ["WINEDLLOVERRIDES", "WINEDEBUG"]:
                    if var in os.environ:
                        self.log_file.write("System " + var + ": " + os.environ[var] + "\n")
                    if var in used_user_settings:
                        self.log_file.write("User settings " + var + ": " + used_user_settings[var] + "\n")
                    if var in self.env:
                        self.log_file.write("Effective " + var + ": " + self.env[var] + "\n")

                # check for low fd limit
                _soft_limit, hard_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
                if hard_limit < 524288:
                    self.log_file.write(f"WARNING: Low file descriptor limit: {hard_limit} (see https://github.com/ValveSoftware/Proton/wiki/File-Descriptors)\n")

                if os.path.exists("/proc/sys/vm/max_map_count"):
                    with open("/proc/sys/vm/max_map_count", "r") as f:
                        max_map_count = int(f.read())
                        if max_map_count < 1048576:
                            self.log_file.write(f"WARNING: Low /proc/sys/vm/max_map_count: {max_map_count} will prevent some games from working\n")

                self.log_file.write("======================\n")
                self.log_file.flush()
            else:
                self.env["WINEDEBUG"] = "-all"

        if "PROTON_REMOTE_DEBUG_CMD" in self.env:
            self.remote_debug_cmd = shlex.split(self.env["PROTON_REMOTE_DEBUG_CMD"])
        else:
            self.remote_debug_cmd = None

        if update_prefix_files:
            g_compatdata.setup_prefix()

        if "nod3d11" in self.compat_config:
            self.dlloverrides["d3d11"] = ""
            if "dxgi" in self.dlloverrides:
                del self.dlloverrides["dxgi"]

        if "nod3d10" in self.compat_config:
            self.dlloverrides["d3d10_1"] = ""
            self.dlloverrides["d3d10"] = ""
            self.dlloverrides["dxgi"] = ""

        if "nativevulkanloader" in self.compat_config:
            self.dlloverrides["vulkan-1"] = "n"

        if "disablenvapi" not in self.compat_config or "forcenvapi" in self.compat_config:
            self.env["DXVK_ENABLE_NVAPI"] = "1"

        if "forcenvapi" in self.compat_config:
            self.env["DXVK_NVAPI_ALLOW_OTHER_DRIVERS"] = "1"
            self.env["DXVK_NVAPI_DRIVER_VERSION"] = "99999"
            self.env["WINE_HIDE_AMD_GPU"] = "1"

        s = ""
        for dll in self.dlloverrides:
            setting = self.dlloverrides[dll]
            if len(s) > 0:
                s = s + ";" + dll + "=" + setting
            else:
                s = dll + "=" + setting
        append_to_env_str(self.env, "WINEDLLOVERRIDES", s, ";")

        if platform.machine() == 'aarch64':
            if not 'FEX_APP_CONFIG' in self.env:
                # custom per-app FEX config
                app_config = self.generate_fex_app_config()
                with open(g_compatdata.fex_config_file, 'w') as f:
                    f.write(json.dumps(app_config, indent=2))
                self.env['FEX_APP_CONFIG'] = g_compatdata.fex_config_file

            self.env["FEX_APP_CONFIG_LOCATION"] = os.path.join(g_proton.dist_dir, "share/fex-emu/")

    def run_proc(self, args, local_env=None):
        if local_env is None:
            local_env = self.env

        # TuxBlox: launch in its own process group so a Ctrl+C (or SIGTERM) sent
        # to proton takes the whole tree down with it -- wine, the game, and any
        # helper processes it spawns (e.g. WebView2's gpu-process/crashpad-handler) --
        # instead of leaving them running as orphans after proton itself exits.
        #
        # cwd is pinned to drive_c rather than inherited from wherever this
        # launcher itself was invoked from (normally the TuxBlox repo/install
        # dir, outside the prefix entirely). Without the Z: drive, a unix path
        # outside C: can only be represented via ntdll's \??\unix\ bridge form,
        # and RtlSetCurrentDirectory_U corrupts that string when storing it
        # (strips 4 chars assuming a \??\<drive>: prefix), breaking the
        # process's own working directory and leaking a malformed path into
        # any child that inherits it. Starting wine already inside C: avoids
        # that codepath for the process's own cwd entirely.
        proc = subprocess.Popen(args, env=local_env, stderr=self.log_file, stdout=self.log_file,
                                 cwd=g_compatdata.prefix_dir + "drive_c", start_new_session=True)

        def handle_signal(signum, frame):
            # Reap via raw os.waitpid(), NOT proc.wait()/.poll() -- this handler runs
            # nested inside the *already in-flight* outer proc.wait() call below
            # (Python dispatches signal handlers between bytecode steps, it doesn't
            # unwind the interrupted call first), and Popen.wait() blocks acquiring
            # its own internal _waitpid_lock, which the outer call already holds.
            # Calling proc.wait() here self-deadlocks forever, confirmed by reading
            # CPython's subprocess.py: Finding 2026-07-30, Ctrl+C repro hung
            # indefinitely past both 5s timeouts until this was changed to raw
            # os.waitpid(), which doesn't touch that lock at all.
            kill_process_group(proc.pid, signal.SIGTERM)
            deadline = time.time() + 5
            reaped = False
            while time.time() < deadline:
                try:
                    if os.waitpid(proc.pid, os.WNOHANG) != (0, 0):
                        reaped = True
                        break
                except ChildProcessError:
                    reaped = True
                    break
                time.sleep(0.1)
            if not reaped:
                kill_process_group(proc.pid, signal.SIGKILL)
                try:
                    os.waitpid(proc.pid, 0)
                except ChildProcessError:
                    pass
            # wineserver (and the winedevice/xalia helper processes connected to it)
            # are spawned independently of this invocation's own process group --
            # wine deliberately lets wineserver outlive any single client so it can
            # keep serving the prefix -- so killing our own process group above
            # leaves them running. Tear down the whole prefix session explicitly.
            try:
                subprocess.run([g_proton.wineserver_bin, "-k"], env=local_env, timeout=5)
            except (subprocess.TimeoutExpired, OSError):
                pass
            # TuxBlox: bucket 2 ("a process error not related to proton"),
            # not the raw 128+signum -- see the exit-code contract note on
            # Session.run(). A user-initiated Ctrl+C/SIGTERM mid-launch is
            # not a Proton bug, it's the wrapped process being torn down.
            sys.exit(2)

        old_handlers = (signal.signal(signal.SIGINT, handle_signal),
                        signal.signal(signal.SIGTERM, handle_signal))
        try:
            rc = proc.wait()
        finally:
            signal.signal(signal.SIGINT, old_handlers[0])
            signal.signal(signal.SIGTERM, old_handlers[1])

        self._relay_real_exit_code()
        return rc

    def _relay_real_exit_code(self):
        # TuxBlox: the wrapped process's real, non-truncated exit code (see
        # tuxblox_report_real_exit_code() in dlls/ntdll/unix/tuxblox_trace.c)
        # is written to *its own* stderr, which run_proc() redirects into
        # this run's own log file (self.log_file) -- not into Proton's own
        # stdout/stderr, which is all the launcher/launch.sh actually
        # capture. Relay it across that boundary: scan the log for the
        # marker line and re-emit it on Proton's own stderr. Harmless no-op
        # for any run_proc() call whose target never wrote the marker (e.g.
        # wineboot, wineserver -k) -- image_is_target() already scopes the
        # marker itself to Player/Studio/WebView2.
        if not self.log_file:
            return
        try:
            with open(self.log_file.name, "r", errors="replace") as f:
                real_code_line = None
                for line in f:
                    if line.startswith("TUXBLOX_REAL_EXIT_CODE="):
                        real_code_line = line.strip()
            if real_code_line:
                print(real_code_line, file=sys.stderr)
                sys.stderr.flush()
        except OSError:
            pass

    def _wait_for_prefix_drain(self, timeout):
        # TuxBlox: bounded counterpart to run_proc() for "wait until this
        # prefix session is truly finished" -- see the call site in run()
        # for why this exists. Deliberately not built on run_proc() itself:
        # that installs SIGINT/SIGTERM handlers tuned for "kill the thing
        # we're actively waiting on", which is the wrong shape here (a
        # timeout expiring is the *expected*, common case, not a signal).
        proc = subprocess.Popen([g_proton.wineserver_bin, "-w"], env=self.env,
                                 stderr=self.log_file, stdout=self.log_file,
                                 cwd=g_compatdata.prefix_dir + "drive_c", start_new_session=True)
        try:
            proc.wait(timeout=timeout)
            return
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            pass

        # Still-connected clients past the timeout: force wineserver to tear
        # the whole prefix down (same "-k" used by the Ctrl+C path above),
        # then give the "-w" watcher a short moment to notice and exit on its
        # own before killing it directly.
        try:
            subprocess.run([g_proton.wineserver_bin, "-k"], env=self.env, timeout=5)
        except (subprocess.TimeoutExpired, OSError):
            pass
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

    def run(self):
        adverb = []

        if self.remote_debug_cmd:
            remote_debug_cmd = self.remote_debug_cmd
            if not os.path.isabs(remote_debug_cmd[0]):
                remote_debug_cmd[0] = g_proton.path(remote_debug_cmd[0])
            remote_debug_proc = subprocess.Popen([g_proton.wine_bin] + self.remote_debug_cmd,
                                                 env=self.env, stderr=self.log_file, stdout=self.log_file)
        else:
            remote_debug_proc = None

        # TuxBlox: run the target directly instead of through Proton's built-in
        # "c:\windows\system32\steam.exe" helper (steam_helper, since removed). That helper
        # exists to emulate a live Steamworks client for real Steam games (achievements,
        # overlay, cloud saves, per-title workarounds like the CoD branch this replaces)
        # and, on some code paths, actively forwards to the host's native Steam client.
        # Roblox is not a Steam title and needs none of that -- see plan.txt item 22.
        if g_proton.host_pe_arch == "x86_64-windows":
            #run with winepreloader directly to avoid restart through start.exe
            self.env["WINELOADERNOEXEC"] = "1"
            argv = [g_proton.lib_dir + "/wine/x86_64-unix/wine-preloader", g_proton.lib_dir + "/wine/x86_64-unix/wine"]
        else:
            argv = [g_proton.wine_bin]

        rc = self.run_proc(adverb + argv + sys.argv[2:] + self.cmdlineappend)

        # TuxBlox: the process we just waited on above is only the ORIGINAL
        # unix process subprocess.Popen spawned. If the wrapped app
        # relaunches itself -- e.g. Roblox's own launcher/bootstrapper
        # spawning the real RobloxPlayerBeta.exe/RobloxStudioBeta.exe via
        # CreateProcess+ExitProcess, a normal Windows self-update pattern,
        # not a unix-style exec() that would keep the same pid -- that
        # original process exiting does NOT mean the session is over: a
        # replacement process (forked by wine's own NtCreateUserProcess,
        # see dlls/ntdll/unix/process.c) is still running, unix-orphaned
        # from the Popen handle above. Returning here regardless made
        # run()/__main__ treat the whole run as finished and exit -- while
        # that replacement, and any of its own helper processes (e.g.
        # WebView2's gpu-process/crashpad-handler), kept running in the same
        # process group and kept writing to the terminal this process had
        # already handed back to the user, corrupting it.
        #
        # wineserver, unlike the Popen handle above, tracks every Windows
        # process in the prefix regardless of which unix process spawned it,
        # and only exits once the last one disconnects -- so block on that
        # instead of trusting the original process's own exit as "done".
        # Same primitive already used by the "waitforexitandrun" verb above;
        # a no-op wait when there really is nothing left, since wineserver
        # would already be exiting on its own in that case.
        #
        # Bounded, though -- confirmed live (2026-08-09) that WebView2's own
        # crashpad-handler/gpu-process helpers routinely outlive the app that
        # spawned them by minutes and sometimes never disconnect on their own
        # at all: on real Windows a Job Object tears them down automatically
        # when their parent exits, and this prefix doesn't implement that, so
        # an unbounded wait here traded "orphans occasionally leak" for "the
        # terminal never comes back at all", strictly worse for the same
        # complaint this is meant to fix. self-relaunch (the case this exists
        # for) hands off to the replacement process within a couple seconds,
        # so a real relaunch is long since visible to wineserver well within
        # this timeout; anything still connected after it is exactly the
        # lingering-helper case, not a relaunch still in flight, so force the
        # issue the same way a user-initiated Ctrl+C already does elsewhere
        # in this file rather than hang the caller indefinitely.
        self._wait_for_prefix_drain(timeout=15)

        if remote_debug_proc:
            remote_debug_proc.kill()
            try:
                remote_debug_proc.communicate(timeout=2)
            except subprocess.TimeoutExpired:
                log("terminate remote debugger")
                remote_debug_proc.terminate()
                remote_debug_proc.communicate()

        return rc

if __name__ == "__main__":
    # Handled before anything else: needs no env vars, no prefix, no session.
    if "--version" in sys.argv:
        print(TUXBLOX_VERSION)
        sys.exit(0)

    if "TUXBLOX_PREFIX" not in os.environ:
        log("TUXBLOX_PREFIX is not set?")
        sys.exit(1)

    g_proton = Proton(os.path.dirname(sys.argv[0]))

    g_proton.cleanup_legacy_dist()

    g_compatdata = CompatData(os.environ["TUXBLOX_PREFIX"])

    g_session = Session()

    g_session.init_wine()

    if g_proton.missing_default_prefix():
        g_proton.make_default_prefix()

    g_session.init_session(sys.argv[1] != "runinprefix")

    #determine mode
    rc = 0
    if sys.argv[1] == "run":
        #start target app
        rc = g_session.run()
    elif sys.argv[1] == "waitforexitandrun":
        #wait for wineserver to shut down
        g_session.run_proc([g_proton.wineserver_bin, "-w"])
        #then run
        rc = g_session.run()
    elif sys.argv[1] == "runinprefix":
        rc = g_session.run_proc([g_proton.wine_bin] + sys.argv[2:])
    elif sys.argv[1] == "destroyprefix":
        g_compatdata.remove_tracked_files()
    elif sys.argv[1] == "getcompatpath":
        #linux -> windows path
        path = subprocess.check_output([g_proton.wine_bin, "winepath", "-w", sys.argv[2]], env=g_session.env, stderr=g_session.log_file)
        sys.stdout.buffer.write(path)
    elif sys.argv[1] == "getnativepath":
        #windows -> linux path
        path = subprocess.check_output([g_proton.wine_bin, "winepath", sys.argv[2]], env=g_session.env, stderr=g_session.log_file)
        sys.stdout.buffer.write(path)
    else:
        log("Need a verb.")
        sys.exit(1)

    # TuxBlox: fixed exit-code contract for Proton's own process, which the
    # launcher relies on to tell "Proton itself broke" apart from "the thing
    # Proton ran broke" -- the real, non-truncated exit code of the wrapped
    # process (which this collapses to 0/2) still reaches the launcher
    # separately, via Session._relay_real_exit_code()'s marker line.
    #   0 - the wrapped process exited cleanly (its own exit code was 0)
    #   1 - Proton itself failed (every early sys.exit(1) above, plus any
    #       uncaught exception escaping to here, already uses this value)
    #   2 - the wrapped process exited abnormally -- not a Proton bug
    if sys.argv[1] in ("run", "waitforexitandrun", "runinprefix"):
        rc = 0 if rc == 0 else 2

    sys.exit(rc)

#pylint --disable=C0301,C0326,C0330,C0111,C0103,R0902,C1801,R0914,R0912,R0915
# vim: set syntax=python:
