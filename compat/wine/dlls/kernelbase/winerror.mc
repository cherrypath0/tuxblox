;
; Copyright 2000 Dave Pickles
;
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Lesser General Public
; License as published by the Free Software Foundation; either
; version 2.1 of the License, or (at your option) any later version.
;
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public
; License along with this library; if not, write to the Free Software
; Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
;

; The Win32 message text here matches Windows word for word on purpose. An application
; that formats an error code and then logs or transmits the result reports the wording it
; was given, so a paraphrase identifies the compatibility layer from a single line of
; output. The entries carrying Severity/Facility are HRESULTs in their own numbering
; space and keep their own text.

LanguageNames=(ENU=0x409:winerr)
SeverityNames=(CoError=0x2:STATUS_SEVERITY_COERROR)
FacilityNames=(Trust=0xb:FACILITY_CERT)

MessageId=0
SymbolicName=ERROR_SUCCESS
Language=ENU
The operation completed successfully.
.
MessageId=1
SymbolicName=ERROR_INVALID_FUNCTION
Language=ENU
Incorrect function.
.
MessageId=2
SymbolicName=ERROR_FILE_NOT_FOUND
Language=ENU
The system cannot find the file specified.
.
MessageId=3
SymbolicName=ERROR_PATH_NOT_FOUND
Language=ENU
The system cannot find the path specified.
.
MessageId=4
SymbolicName=ERROR_TOO_MANY_OPEN_FILES
Language=ENU
The system cannot open the file.
.
MessageId=5
SymbolicName=ERROR_ACCESS_DENIED
Language=ENU
Access is denied.
.
MessageId=6
SymbolicName=ERROR_INVALID_HANDLE
Language=ENU
The handle is invalid.
.
MessageId=7
SymbolicName=ERROR_ARENA_TRASHED
Language=ENU
The storage control blocks were destroyed.
.
MessageId=8
SymbolicName=ERROR_NOT_ENOUGH_MEMORY
Language=ENU
Not enough memory resources are available to process this command.
.
MessageId=9
SymbolicName=ERROR_INVALID_BLOCK
Language=ENU
The storage control block address is invalid.
.
MessageId=10
SymbolicName=ERROR_BAD_ENVIRONMENT
Language=ENU
The environment is incorrect.
.
MessageId=11
SymbolicName=ERROR_BAD_FORMAT
Language=ENU
An attempt was made to load a program with an incorrect format.
.
MessageId=12
SymbolicName=ERROR_INVALID_ACCESS
Language=ENU
The access code is invalid.
.
MessageId=13
SymbolicName=ERROR_INVALID_DATA
Language=ENU
The data is invalid.
.
MessageId=14
SymbolicName=ERROR_OUTOFMEMORY
Language=ENU
Not enough memory resources are available to complete this operation.
.
MessageId=15
SymbolicName=ERROR_INVALID_DRIVE
Language=ENU
The system cannot find the drive specified.
.
MessageId=16
SymbolicName=ERROR_CURRENT_DIRECTORY
Language=ENU
The directory cannot be removed.
.
MessageId=17
SymbolicName=ERROR_NOT_SAME_DEVICE
Language=ENU
The system cannot move the file to a different disk drive.
.
MessageId=18
SymbolicName=ERROR_NO_MORE_FILES
Language=ENU
There are no more files.
.
MessageId=19
SymbolicName=ERROR_WRITE_PROTECT
Language=ENU
The media is write protected.
.
MessageId=20
SymbolicName=ERROR_BAD_UNIT
Language=ENU
The system cannot find the device specified.
.
MessageId=21
SymbolicName=ERROR_NOT_READY
Language=ENU
The device is not ready.
.
MessageId=22
SymbolicName=ERROR_BAD_COMMAND
Language=ENU
The device does not recognize the command.
.
MessageId=23
SymbolicName=ERROR_CRC
Language=ENU
Data error (cyclic redundancy check).
.
MessageId=24
SymbolicName=ERROR_BAD_LENGTH
Language=ENU
The program issued a command but the command length is incorrect.
.
MessageId=25
SymbolicName=ERROR_SEEK
Language=ENU
The drive cannot locate a specific area or track on the disk.
.
MessageId=26
SymbolicName=ERROR_NOT_DOS_DISK
Language=ENU
The specified disk or diskette cannot be accessed.
.
MessageId=27
SymbolicName=ERROR_SECTOR_NOT_FOUND
Language=ENU
The drive cannot find the sector requested.
.
MessageId=28
SymbolicName=ERROR_OUT_OF_PAPER
Language=ENU
The printer is out of paper.
.
MessageId=29
SymbolicName=ERROR_WRITE_FAULT
Language=ENU
The system cannot write to the specified device.
.
MessageId=30
SymbolicName=ERROR_READ_FAULT
Language=ENU
The system cannot read from the specified device.
.
MessageId=31
SymbolicName=ERROR_GEN_FAILURE
Language=ENU
A device attached to the system is not functioning.
.
MessageId=32
SymbolicName=ERROR_SHARING_VIOLATION
Language=ENU
The process cannot access the file because it is being used by another process.
.
MessageId=33
SymbolicName=ERROR_LOCK_VIOLATION
Language=ENU
The process cannot access the file because another process has locked a portion of the file.
.
MessageId=34
SymbolicName=ERROR_WRONG_DISK
Language=ENU
The wrong diskette is in the drive.
.
MessageId=36
SymbolicName=ERROR_SHARING_BUFFER_EXCEEDED
Language=ENU
Too many files opened for sharing.
.
MessageId=38
SymbolicName=ERROR_HANDLE_EOF
Language=ENU
Reached the end of the file.
.
MessageId=39
SymbolicName=ERROR_HANDLE_DISK_FULL
Language=ENU
The disk is full.
.
MessageId=50
SymbolicName=ERROR_NOT_SUPPORTED
Language=ENU
The request is not supported.
.
MessageId=51
SymbolicName=ERROR_REM_NOT_LIST
Language=ENU
Windows cannot find the network path. Verify that the network path is correct and the destination computer is not busy or turned off. If Windows still cannot find the network path, contact your network administrator.
.
MessageId=52
SymbolicName=ERROR_DUP_NAME
Language=ENU
You were not connected because a duplicate name exists on the network. If joining a domain, go to System in Control Panel to change the computer name and try again. If joining a workgroup, choose another workgroup name.
.
MessageId=53
SymbolicName=ERROR_BAD_NETPATH
Language=ENU
The network path was not found.
.
MessageId=54
SymbolicName=ERROR_NETWORK_BUSY
Language=ENU
The network is busy.
.
MessageId=55
SymbolicName=ERROR_DEV_NOT_EXIST
Language=ENU
The specified network resource or device is no longer available.
.
MessageId=56
SymbolicName=ERROR_TOO_MANY_CMDS
Language=ENU
The network BIOS command limit has been reached.
.
MessageId=57
SymbolicName=ERROR_ADAP_HDW_ERR
Language=ENU
A network adapter hardware error occurred.
.
MessageId=58
SymbolicName=ERROR_BAD_NET_RESP
Language=ENU
The specified server cannot perform the requested operation.
.
MessageId=59
SymbolicName=ERROR_UNEXP_NET_ERR
Language=ENU
An unexpected network error occurred.
.
MessageId=60
SymbolicName=ERROR_BAD_REM_ADAP
Language=ENU
The remote adapter is not compatible.
.
MessageId=61
SymbolicName=ERROR_PRINTQ_FULL
Language=ENU
The printer queue is full.
.
MessageId=62
SymbolicName=ERROR_NO_SPOOL_SPACE
Language=ENU
Space to store the file waiting to be printed is not available on the server.
.
MessageId=63
SymbolicName=ERROR_PRINT_CANCELLED
Language=ENU
Your file waiting to be printed was deleted.
.
MessageId=64
SymbolicName=ERROR_NETNAME_DELETED
Language=ENU
The specified network name is no longer available.
.
MessageId=65
SymbolicName=ERROR_NETWORK_ACCESS_DENIED
Language=ENU
Network access is denied.
.
MessageId=66
SymbolicName=ERROR_BAD_DEV_TYPE
Language=ENU
The network resource type is not correct.
.
MessageId=67
SymbolicName=ERROR_BAD_NET_NAME
Language=ENU
The network name cannot be found.
.
MessageId=68
SymbolicName=ERROR_TOO_MANY_NAMES
Language=ENU
The name limit for the local computer network adapter card was exceeded.
.
MessageId=69
SymbolicName=ERROR_TOO_MANY_SESS
Language=ENU
The network BIOS session limit was exceeded.
.
MessageId=70
SymbolicName=ERROR_SHARING_PAUSED
Language=ENU
The remote server has been paused or is in the process of being started.
.
MessageId=71
SymbolicName=ERROR_REQ_NOT_ACCEP
Language=ENU
No more connections can be made to this remote computer at this time because there are already as many connections as the computer can accept.
.
MessageId=72
SymbolicName=ERROR_REDIR_PAUSED
Language=ENU
The specified printer or disk device has been paused.
.
MessageId=80
SymbolicName=ERROR_FILE_EXISTS
Language=ENU
The file exists.
.
MessageId=82
SymbolicName=ERROR_CANNOT_MAKE
Language=ENU
The directory or file cannot be created.
.
MessageId=83
SymbolicName=ERROR_FAIL_I24
Language=ENU
Fail on INT 24.
.
MessageId=84
SymbolicName=ERROR_OUT_OF_STRUCTURES
Language=ENU
Storage to process this request is not available.
.
MessageId=85
SymbolicName=ERROR_ALREADY_ASSIGNED
Language=ENU
The local device name is already in use.
.
MessageId=86
SymbolicName=ERROR_INVALID_PASSWORD
Language=ENU
The specified network password is not correct.
.
MessageId=87
SymbolicName=ERROR_INVALID_PARAMETER
Language=ENU
The parameter is incorrect.
.
MessageId=88
SymbolicName=ERROR_NET_WRITE_FAULT
Language=ENU
A write fault occurred on the network.
.
MessageId=89
SymbolicName=ERROR_NO_PROC_SLOTS
Language=ENU
The system cannot start another process at this time.
.
MessageId=100
SymbolicName=ERROR_TOO_MANY_SEMAPHORES
Language=ENU
Cannot create another system semaphore.
.
MessageId=101
SymbolicName=ERROR_EXCL_SEM_ALREADY_OWNED
Language=ENU
The exclusive semaphore is owned by another process.
.
MessageId=102
SymbolicName=ERROR_SEM_IS_SET
Language=ENU
The semaphore is set and cannot be closed.
.
MessageId=103
SymbolicName=ERROR_TOO_MANY_SEM_REQUESTS
Language=ENU
The semaphore cannot be set again.
.
MessageId=104
SymbolicName=ERROR_INVALID_AT_INTERRUPT_TIME
Language=ENU
Cannot request exclusive semaphores at interrupt time.
.
MessageId=105
SymbolicName=ERROR_SEM_OWNER_DIED
Language=ENU
The previous ownership of this semaphore has ended.
.
MessageId=106
SymbolicName=ERROR_SEM_USER_LIMIT
Language=ENU
Insert the diskette for drive %1.
.
MessageId=107
SymbolicName=ERROR_DISK_CHANGE
Language=ENU
The program stopped because an alternate diskette was not inserted.
.
MessageId=108
SymbolicName=ERROR_DRIVE_LOCKED
Language=ENU
The disk is in use or locked by another process.
.
MessageId=109
SymbolicName=ERROR_BROKEN_PIPE
Language=ENU
The pipe has been ended.
.
MessageId=110
SymbolicName=ERROR_OPEN_FAILED
Language=ENU
The system cannot open the device or file specified.
.
MessageId=111
SymbolicName=ERROR_BUFFER_OVERFLOW
Language=ENU
The file name is too long.
.
MessageId=112
SymbolicName=ERROR_DISK_FULL
Language=ENU
There is not enough space on the disk.
.
MessageId=113
SymbolicName=ERROR_NO_MORE_SEARCH_HANDLES
Language=ENU
No more internal file identifiers available.
.
MessageId=114
SymbolicName=ERROR_INVALID_TARGET_HANDLE
Language=ENU
The target internal file identifier is incorrect.
.
MessageId=117
SymbolicName=ERROR_INVALID_CATEGORY
Language=ENU
The IOCTL call made by the application program is not correct.
.
MessageId=118
SymbolicName=ERROR_INVALID_VERIFY_SWITCH
Language=ENU
The verify-on-write switch parameter value is not correct.
.
MessageId=119
SymbolicName=ERROR_BAD_DRIVER_LEVEL
Language=ENU
The system does not support the command requested.
.
MessageId=120
SymbolicName=ERROR_CALL_NOT_IMPLEMENTED
Language=ENU
This function is not supported on this system.
.
MessageId=121
SymbolicName=ERROR_SEM_TIMEOUT
Language=ENU
The semaphore timeout period has expired.
.
MessageId=122
SymbolicName=ERROR_INSUFFICIENT_BUFFER
Language=ENU
The data area passed to a system call is too small.
.
MessageId=123
SymbolicName=ERROR_INVALID_NAME
Language=ENU
The filename, directory name, or volume label syntax is incorrect.
.
MessageId=124
SymbolicName=ERROR_INVALID_LEVEL
Language=ENU
The system call level is not correct.
.
MessageId=125
SymbolicName=ERROR_NO_VOLUME_LABEL
Language=ENU
The disk has no volume label.
.
MessageId=126
SymbolicName=ERROR_MOD_NOT_FOUND
Language=ENU
The specified module could not be found.
.
MessageId=127
SymbolicName=ERROR_PROC_NOT_FOUND
Language=ENU
The specified procedure could not be found.
.
MessageId=128
SymbolicName=ERROR_WAIT_NO_CHILDREN
Language=ENU
There are no child processes to wait for.
.
MessageId=129
SymbolicName=ERROR_CHILD_NOT_COMPLETE
Language=ENU
The %1 application cannot be run in Win32 mode.
.
MessageId=130
SymbolicName=ERROR_DIRECT_ACCESS_HANDLE
Language=ENU
Attempt to use a file handle to an open disk partition for an operation other than raw disk I/O.
.
MessageId=131
SymbolicName=ERROR_NEGATIVE_SEEK
Language=ENU
An attempt was made to move the file pointer before the beginning of the file.
.
MessageId=132
SymbolicName=ERROR_SEEK_ON_DEVICE
Language=ENU
The file pointer cannot be set on the specified device or file.
.
MessageId=133
SymbolicName=ERROR_IS_JOIN_TARGET
Language=ENU
A JOIN or SUBST command cannot be used for a drive that contains previously joined drives.
.
MessageId=134
SymbolicName=ERROR_IS_JOINED
Language=ENU
An attempt was made to use a JOIN or SUBST command on a drive that has already been joined.
.
MessageId=135
SymbolicName=ERROR_IS_SUBSTED
Language=ENU
An attempt was made to use a JOIN or SUBST command on a drive that has already been substituted.
.
MessageId=136
SymbolicName=ERROR_NOT_JOINED
Language=ENU
The system tried to delete the JOIN of a drive that is not joined.
.
MessageId=137
SymbolicName=ERROR_NOT_SUBSTED
Language=ENU
The system tried to delete the substitution of a drive that is not substituted.
.
MessageId=138
SymbolicName=ERROR_JOIN_TO_JOIN
Language=ENU
The system tried to join a drive to a directory on a joined drive.
.
MessageId=139
SymbolicName=ERROR_SUBST_TO_SUBST
Language=ENU
The system tried to substitute a drive to a directory on a substituted drive.
.
MessageId=140
SymbolicName=ERROR_JOIN_TO_SUBST
Language=ENU
The system tried to join a drive to a directory on a substituted drive.
.
MessageId=141
SymbolicName=ERROR_SUBST_TO_JOIN
Language=ENU
The system tried to SUBST a drive to a directory on a joined drive.
.
MessageId=142
SymbolicName=ERROR_BUSY_DRIVE
Language=ENU
The system cannot perform a JOIN or SUBST at this time.
.
MessageId=143
SymbolicName=ERROR_SAME_DRIVE
Language=ENU
The system cannot join or substitute a drive to or for a directory on the same drive.
.
MessageId=144
SymbolicName=ERROR_DIR_NOT_ROOT
Language=ENU
The directory is not a subdirectory of the root directory.
.
MessageId=145
SymbolicName=ERROR_DIR_NOT_EMPTY
Language=ENU
The directory is not empty.
.
MessageId=146
SymbolicName=ERROR_IS_SUBST_PATH
Language=ENU
The path specified is being used in a substitute.
.
MessageId=147
SymbolicName=ERROR_IS_JOIN_PATH
Language=ENU
Not enough resources are available to process this command.
.
MessageId=148
SymbolicName=ERROR_PATH_BUSY
Language=ENU
The path specified cannot be used at this time.
.
MessageId=149
SymbolicName=ERROR_IS_SUBST_TARGET
Language=ENU
An attempt was made to join or substitute a drive for which a directory on the drive is the target of a previous substitute.
.
MessageId=150
SymbolicName=ERROR_SYSTEM_TRACE
Language=ENU
System trace information was not specified in your CONFIG.SYS file, or tracing is disallowed.
.
MessageId=151
SymbolicName=ERROR_INVALID_EVENT_COUNT
Language=ENU
The number of specified semaphore events for DosMuxSemWait is not correct.
.
MessageId=152
SymbolicName=ERROR_TOO_MANY_MUXWAITERS
Language=ENU
DosMuxSemWait did not execute; too many semaphores are already set.
.
MessageId=153
SymbolicName=ERROR_INVALID_LIST_FORMAT
Language=ENU
The DosMuxSemWait list is not correct.
.
MessageId=154
SymbolicName=ERROR_LABEL_TOO_LONG
Language=ENU
The volume label you entered exceeds the label character limit of the target file system.
.
MessageId=155
SymbolicName=ERROR_TOO_MANY_TCBS
Language=ENU
Cannot create another thread.
.
MessageId=156
SymbolicName=ERROR_SIGNAL_REFUSED
Language=ENU
The recipient process has refused the signal.
.
MessageId=157
SymbolicName=ERROR_DISCARDED
Language=ENU
The segment is already discarded and cannot be locked.
.
MessageId=158
SymbolicName=ERROR_NOT_LOCKED
Language=ENU
The segment is already unlocked.
.
MessageId=159
SymbolicName=ERROR_BAD_THREADID_ADDR
Language=ENU
The address for the thread ID is not correct.
.
MessageId=160
SymbolicName=ERROR_BAD_ARGUMENTS
Language=ENU
One or more arguments are not correct.
.
MessageId=161
SymbolicName=ERROR_BAD_PATHNAME
Language=ENU
The specified path is invalid.
.
MessageId=162
SymbolicName=ERROR_SIGNAL_PENDING
Language=ENU
A signal is already pending.
.
MessageId=164
SymbolicName=ERROR_MAX_THRDS_REACHED
Language=ENU
No more threads can be created in the system.
.
MessageId=167
SymbolicName=ERROR_LOCK_FAILED
Language=ENU
Unable to lock a region of a file.
.
MessageId=170
SymbolicName=ERROR_BUSY
Language=ENU
The requested resource is in use.
.
MessageId=171
SymbolicName=ERROR_DEVICE_SUPPORT_IN_PROGRESS
Language=ENU
Device's command support detection is in progress.
.
MessageId=173
SymbolicName=ERROR_CANCEL_VIOLATION
Language=ENU
A lock request was not outstanding for the supplied cancel region.
.
MessageId=174
SymbolicName=ERROR_ATOMIC_LOCKS_NOT_SUPPORTED
Language=ENU
The file system does not support atomic changes to the lock type.
.
MessageId=180
SymbolicName=ERROR_INVALID_SEGMENT_NUMBER
Language=ENU
The system detected a segment number that was not correct.
.
MessageId=182
SymbolicName=ERROR_INVALID_ORDINAL
Language=ENU
The operating system cannot run %1.
.
MessageId=183
SymbolicName=ERROR_ALREADY_EXISTS
Language=ENU
Cannot create a file when that file already exists.
.
MessageId=186
SymbolicName=ERROR_INVALID_FLAG_NUMBER
Language=ENU
The flag passed is not correct.
.
MessageId=187
SymbolicName=ERROR_SEM_NOT_FOUND
Language=ENU
The specified system semaphore name was not found.
.
MessageId=188
SymbolicName=ERROR_INVALID_STARTING_CODESEG
Language=ENU
The operating system cannot run %1.
.
MessageId=189
SymbolicName=ERROR_INVALID_STACKSEG
Language=ENU
The operating system cannot run %1.
.
MessageId=190
SymbolicName=ERROR_INVALID_MODULETYPE
Language=ENU
The operating system cannot run %1.
.
MessageId=191
SymbolicName=ERROR_INVALID_EXE_SIGNATURE
Language=ENU
Cannot run %1 in Win32 mode.
.
MessageId=192
SymbolicName=ERROR_EXE_MARKED_INVALID
Language=ENU
The operating system cannot run %1.
.
MessageId=193
SymbolicName=ERROR_BAD_EXE_FORMAT
Language=ENU
%1 is not a valid Win32 application.
.
MessageId=194
SymbolicName=ERROR_ITERATED_DATA_EXCEEDS_64k
Language=ENU
The operating system cannot run %1.
.
MessageId=195
SymbolicName=ERROR_INVALID_MINALLOCSIZE
Language=ENU
The operating system cannot run %1.
.
MessageId=196
SymbolicName=ERROR_DYNLINK_FROM_INVALID_RING
Language=ENU
The operating system cannot run this application program.
.
MessageId=197
SymbolicName=ERROR_IOPL_NOT_ENABLED
Language=ENU
The operating system is not presently configured to run this application.
.
MessageId=198
SymbolicName=ERROR_INVALID_SEGDPL
Language=ENU
The operating system cannot run %1.
.
MessageId=199
SymbolicName=ERROR_AUTODATASEG_EXCEEDS_64k
Language=ENU
The operating system cannot run this application program.
.
MessageId=200
SymbolicName=ERROR_RING2SEG_MUST_BE_MOVABLE
Language=ENU
The code segment cannot be greater than or equal to 64K.
.
MessageId=201
SymbolicName=ERROR_RELOC_CHAIN_XEEDS_SEGLIM
Language=ENU
The operating system cannot run %1.
.
MessageId=202
SymbolicName=ERROR_INFLOOP_IN_RELOC_CHAIN
Language=ENU
The operating system cannot run %1.
.
MessageId=203
SymbolicName=ERROR_ENVVAR_NOT_FOUND
Language=ENU
The system could not find the environment option that was entered.
.
MessageId=205
SymbolicName=ERROR_NO_SIGNAL_SENT
Language=ENU
No process in the command subtree has a signal handler.
.
MessageId=206
SymbolicName=ERROR_FILENAME_EXCED_RANGE
Language=ENU
The filename or extension is too long.
.
MessageId=207
SymbolicName=ERROR_RING2_STACK_IN_USE
Language=ENU
The ring 2 stack is in use.
.
MessageId=208
SymbolicName=ERROR_META_EXPANSION_TOO_LONG
Language=ENU
The global filename characters, * or ?, are entered incorrectly or too many global filename characters are specified.
.
MessageId=209
SymbolicName=ERROR_INVALID_SIGNAL_NUMBER
Language=ENU
The signal being posted is not correct.
.
MessageId=210
SymbolicName=ERROR_THREAD_1_INACTIVE
Language=ENU
The signal handler cannot be set.
.
MessageId=212
SymbolicName=ERROR_LOCKED
Language=ENU
The segment is locked and cannot be reallocated.
.
MessageId=214
SymbolicName=ERROR_TOO_MANY_MODULES
Language=ENU
Too many dynamic-link modules are attached to this program or dynamic-link module.
.
MessageId=215
SymbolicName=ERROR_NESTING_NOT_ALLOWED
Language=ENU
Cannot nest calls to LoadModule.
.
MessageId=216
SymbolicName=ERROR_EXE_MACHINE_TYPE_MISMATCH
Language=ENU
This version of %1 is not compatible with the version of Windows you're running. Check your computer's system information and then contact the software publisher.
.
MessageId=217
SymbolicName=ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY
Language=ENU
The image file %1 is signed, unable to modify.
.
MessageId=218
SymbolicName=ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY
Language=ENU
The image file %1 is strong signed, unable to modify.
.
MessageId=220
SymbolicName=ERROR_FILE_CHECKED_OUT
Language=ENU
This file is checked out or locked for editing by another user.
.
MessageId=221
SymbolicName=ERROR_CHECKOUT_REQUIRED
Language=ENU
The file must be checked out before saving changes.
.
MessageId=222
SymbolicName=ERROR_BAD_FILE_TYPE
Language=ENU
The file type being saved or retrieved has been blocked.
.
MessageId=223
SymbolicName=ERROR_FILE_TOO_LARGE
Language=ENU
The file size exceeds the limit allowed and cannot be saved.
.
MessageId=224
SymbolicName=ERROR_FORMS_AUTH_REQUIRED
Language=ENU
Access Denied. Before opening files in this location, you must first add the web site to your trusted sites list, browse to the web site, and select the option to login automatically.
.
MessageId=225
SymbolicName=ERROR_VIRUS_INFECTED
Language=ENU
Operation did not complete successfully because the file contains a virus or potentially unwanted software.
.
MessageId=226
SymbolicName=ERROR_VIRUS_DELETED
Language=ENU
This file contains a virus or potentially unwanted software and cannot be opened. Due to the nature of this virus or potentially unwanted software, the file has been removed from this location.
.
MessageId=229
SymbolicName=ERROR_PIPE_LOCAL
Language=ENU
The pipe is local.
.
MessageId=230
SymbolicName=ERROR_BAD_PIPE
Language=ENU
The pipe state is invalid.
.
MessageId=231
SymbolicName=ERROR_PIPE_BUSY
Language=ENU
All pipe instances are busy.
.
MessageId=232
SymbolicName=ERROR_NO_DATA
Language=ENU
The pipe is being closed.
.
MessageId=233
SymbolicName=ERROR_PIPE_NOT_CONNECTED
Language=ENU
No process is on the other end of the pipe.
.
MessageId=234
SymbolicName=ERROR_MORE_DATA
Language=ENU
More data is available.
.
MessageId=235
SymbolicName=ERROR_NO_WORK_DONE
Language=ENU
The action requested resulted in no work being done. Error-style clean-up has been performed.
.
MessageId=240
SymbolicName=ERROR_VC_DISCONNECTED
Language=ENU
The session was canceled.
.
MessageId=254
SymbolicName=ERROR_INVALID_EA_NAME
Language=ENU
The specified extended attribute name was invalid.
.
MessageId=255
SymbolicName=ERROR_EA_LIST_INCONSISTENT
Language=ENU
The extended attributes are inconsistent.
.
MessageId=258
SymbolicName=WAIT_TIMEOUT
Language=ENU
The wait operation timed out.
.
MessageId=259
SymbolicName=ERROR_NO_MORE_ITEMS
Language=ENU
No more data is available.
.
MessageId=266
SymbolicName=ERROR_CANNOT_COPY
Language=ENU
The copy functions cannot be used.
.
MessageId=267
SymbolicName=ERROR_DIRECTORY
Language=ENU
The directory name is invalid.
.
MessageId=275
SymbolicName=ERROR_EAS_DIDNT_FIT
Language=ENU
The extended attributes did not fit in the buffer.
.
MessageId=276
SymbolicName=ERROR_EA_FILE_CORRUPT
Language=ENU
The extended attribute file on the mounted file system is corrupt.
.
MessageId=277
SymbolicName=ERROR_EA_TABLE_FULL
Language=ENU
The extended attribute table file is full.
.
MessageId=278
SymbolicName=ERROR_INVALID_EA_HANDLE
Language=ENU
The specified extended attribute handle is invalid.
.
MessageId=282
SymbolicName=ERROR_EAS_NOT_SUPPORTED
Language=ENU
The mounted file system does not support extended attributes.
.
MessageId=288
SymbolicName=ERROR_NOT_OWNER
Language=ENU
Attempt to release mutex not owned by caller.
.
MessageId=298
SymbolicName=ERROR_TOO_MANY_POSTS
Language=ENU
Too many posts were made to a semaphore.
.
MessageId=299
SymbolicName=ERROR_PARTIAL_COPY
Language=ENU
Only part of a ReadProcessMemory or WriteProcessMemory request was completed.
.
MessageId=300
SymbolicName=ERROR_OPLOCK_NOT_GRANTED
Language=ENU
The oplock request is denied.
.
MessageId=301
SymbolicName=ERROR_INVALID_OPLOCK_PROTOCOL
Language=ENU
An invalid oplock acknowledgment was received by the system.
.
MessageId=302
SymbolicName=ERROR_DISK_TOO_FRAGMENTED
Language=ENU
The volume is too fragmented to complete this operation.
.
MessageId=303
SymbolicName=ERROR_DELETE_PENDING
Language=ENU
The file cannot be opened because it is in the process of being deleted.
.
MessageId=304
SymbolicName=ERROR_INCOMPATIBLE_WITH_GLOBAL_SHORT_NAME_REGISTRY_SETTING
Language=ENU
Short name settings may not be changed on this volume due to the global registry setting.
.
MessageId=305
SymbolicName=ERROR_SHORT_NAMES_NOT_ENABLED_ON_VOLUME
Language=ENU
Short names are not enabled on this volume.
.
MessageId=306
SymbolicName=ERROR_SECURITY_STREAM_IS_INCONSISTENT
Language=ENU
The security stream for the given volume is in an inconsistent state.
.
MessageId=307
SymbolicName=ERROR_INVALID_LOCK_RANGE
Language=ENU
A requested file lock operation cannot be processed due to an invalid byte range.
.
MessageId=308
SymbolicName=ERROR_IMAGE_SUBSYSTEM_NOT_PRESENT
Language=ENU
The subsystem needed to support the image type is not present.
.
MessageId=309
SymbolicName=ERROR_NOTIFICATION_GUID_ALREADY_DEFINED
Language=ENU
The specified file already has a notification GUID associated with it.
.
MessageId=310
SymbolicName=ERROR_INVALID_EXCEPTION_HANDLER
Language=ENU
An invalid exception handler routine has been detected.
.
MessageId=311
SymbolicName=ERROR_DUPLICATE_PRIVILEGES
Language=ENU
Duplicate privileges were specified for the token.
.
MessageId=312
SymbolicName=ERROR_NO_RANGES_PROCESSED
Language=ENU
No ranges for the specified operation were able to be processed.
.
MessageId=313
SymbolicName=ERROR_NOT_ALLOWED_ON_SYSTEM_FILE
Language=ENU
Operation is not allowed on a file system internal file.
.
MessageId=314
SymbolicName=ERROR_DISK_RESOURCES_EXHAUSTED
Language=ENU
The physical resources of this disk have been exhausted.
.
MessageId=315
SymbolicName=ERROR_INVALID_TOKEN
Language=ENU
The token representing the data is invalid.
.
MessageId=316
SymbolicName=ERROR_DEVICE_FEATURE_NOT_SUPPORTED
Language=ENU
The device does not support the command feature.
.
MessageId=317
SymbolicName=ERROR_MR_MID_NOT_FOUND
Language=ENU
The system cannot find message text for message number 0x%1 in the message file for %2.
.
MessageId=318
SymbolicName=ERROR_SCOPE_NOT_FOUND
Language=ENU
The scope specified was not found.
.
MessageId=319
SymbolicName=ERROR_UNDEFINED_SCOPE
Language=ENU
The Central Access Policy specified is not defined on the target machine.
.
MessageId=320
SymbolicName=ERROR_INVALID_CAP
Language=ENU
The Central Access Policy obtained from Active Directory is invalid.
.
MessageId=321
SymbolicName=ERROR_DEVICE_UNREACHABLE
Language=ENU
The device is unreachable.
.
MessageId=322
SymbolicName=ERROR_DEVICE_NO_RESOURCES
Language=ENU
The target device has insufficient resources to complete the operation.
.
MessageId=323
SymbolicName=ERROR_DATA_CHECKSUM_ERROR
Language=ENU
A data integrity checksum error occurred. Data in the file stream is corrupt.
.
MessageId=324
SymbolicName=ERROR_INTERMIXED_KERNEL_EA_OPERATION
Language=ENU
An attempt was made to modify both a KERNEL and normal Extended Attribute (EA) in the same operation.
.
MessageId=326
SymbolicName=ERROR_FILE_LEVEL_TRIM_NOT_SUPPORTED
Language=ENU
Device does not support file-level TRIM.
.
MessageId=327
SymbolicName=ERROR_OFFSET_ALIGNMENT_VIOLATION
Language=ENU
The command specified a data offset that does not align to the device's granularity/alignment.
.
MessageId=328
SymbolicName=ERROR_INVALID_FIELD_IN_PARAMETER_LIST
Language=ENU
The command specified an invalid field in its parameter list.
.
MessageId=329
SymbolicName=ERROR_OPERATION_IN_PROGRESS
Language=ENU
An operation is currently in progress with the device.
.
MessageId=330
SymbolicName=ERROR_BAD_DEVICE_PATH
Language=ENU
An attempt was made to send down the command via an invalid path to the target device.
.
MessageId=331
SymbolicName=ERROR_TOO_MANY_DESCRIPTORS
Language=ENU
The command specified a number of descriptors that exceeded the maximum supported by the device.
.
MessageId=332
SymbolicName=ERROR_SCRUB_DATA_DISABLED
Language=ENU
Scrub is disabled on the specified file.
.
MessageId=333
SymbolicName=ERROR_NOT_REDUNDANT_STORAGE
Language=ENU
The storage device does not provide redundancy.
.
MessageId=334
SymbolicName=ERROR_RESIDENT_FILE_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on a resident file.
.
MessageId=335
SymbolicName=ERROR_COMPRESSED_FILE_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on a compressed file.
.
MessageId=336
SymbolicName=ERROR_DIRECTORY_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on a directory.
.
MessageId=337
SymbolicName=ERROR_NOT_READ_FROM_COPY
Language=ENU
The specified copy of the requested data could not be read.
.
MessageId=338
SymbolicName=ERROR_FT_WRITE_FAILURE
Language=ENU
The specified data could not be written to any of the copies.
.
MessageId=339
SymbolicName=ERROR_FT_DI_SCAN_REQUIRED
Language=ENU
One or more copies of data on this device may be out of sync. No writes may be performed until a data integrity scan is completed.
.
MessageId=340
SymbolicName=ERROR_INVALID_KERNEL_INFO_VERSION
Language=ENU
The supplied kernel information version is invalid.
.
MessageId=341
SymbolicName=ERROR_INVALID_PEP_INFO_VERSION
Language=ENU
The supplied PEP information version is invalid.
.
MessageId=342
SymbolicName=ERROR_OBJECT_NOT_EXTERNALLY_BACKED
Language=ENU
This object is not externally backed by any provider.
.
MessageId=343
SymbolicName=ERROR_EXTERNAL_BACKING_PROVIDER_UNKNOWN
Language=ENU
The external backing provider is not recognized.
.
MessageId=344
SymbolicName=ERROR_COMPRESSION_NOT_BENEFICIAL
Language=ENU
Compressing this object would not save space.
.
MessageId=345
SymbolicName=ERROR_STORAGE_TOPOLOGY_ID_MISMATCH
Language=ENU
The request failed due to a storage topology ID mismatch.
.
MessageId=346
SymbolicName=ERROR_BLOCKED_BY_PARENTAL_CONTROLS
Language=ENU
The operation was blocked by parental controls.
.
MessageId=347
SymbolicName=ERROR_BLOCK_TOO_MANY_REFERENCES
Language=ENU
A file system block being referenced has already reached the maximum reference count and can't be referenced any further.
.
MessageId=348
SymbolicName=ERROR_MARKED_TO_DISALLOW_WRITES
Language=ENU
The requested operation failed because the file stream is marked to disallow writes.
.
MessageId=349
SymbolicName=ERROR_ENCLAVE_FAILURE
Language=ENU
The requested operation failed with an architecture-specific failure code.
.
MessageId=350
SymbolicName=ERROR_FAIL_NOACTION_REBOOT
Language=ENU
No action was taken as a system reboot is required.
.
MessageId=351
SymbolicName=ERROR_FAIL_SHUTDOWN
Language=ENU
The shutdown operation failed.
.
MessageId=352
SymbolicName=ERROR_FAIL_RESTART
Language=ENU
The restart operation failed.
.
MessageId=353
SymbolicName=ERROR_MAX_SESSIONS_REACHED
Language=ENU
The maximum number of sessions has been reached.
.
MessageId=354
SymbolicName=ERROR_NETWORK_ACCESS_DENIED_EDP
Language=ENU
Windows Information Protection policy does not allow access to this network resource.
.
MessageId=355
SymbolicName=ERROR_DEVICE_HINT_NAME_BUFFER_TOO_SMALL
Language=ENU
The device hint name buffer is too small to receive the remaining name.
.
MessageId=356
SymbolicName=ERROR_EDP_POLICY_DENIES_OPERATION
Language=ENU
The requested operation was blocked by Windows Information Protection policy. For more information, contact your system administrator.
.
MessageId=357
SymbolicName=ERROR_EDP_DPL_POLICY_CANT_BE_SATISFIED
Language=ENU
The requested operation cannot be performed because hardware or software configuration of the device does not comply with Windows Information Protection under Lock policy. Please, verify that user PIN has been created. For more information, contact your system administrator.
.
MessageId=358
SymbolicName=ERROR_CLOUD_FILE_SYNC_ROOT_METADATA_CORRUPT
Language=ENU
The cloud sync root metadata is corrupted.
.
MessageId=359
SymbolicName=ERROR_DEVICE_IN_MAINTENANCE
Language=ENU
The device is in maintenance mode.
.
MessageId=360
SymbolicName=ERROR_NOT_SUPPORTED_ON_DAX
Language=ENU
The specified operation is not supported on a DAX volume.
.
MessageId=361
SymbolicName=ERROR_DAX_MAPPING_EXISTS
Language=ENU
The volume has active DAX mappings.
.
MessageId=362
SymbolicName=ERROR_CLOUD_FILE_PROVIDER_NOT_RUNNING
Language=ENU
The cloud file provider is not running.
.
MessageId=363
SymbolicName=ERROR_CLOUD_FILE_METADATA_CORRUPT
Language=ENU
The cloud file metadata is corrupt and unreadable.
.
MessageId=364
SymbolicName=ERROR_CLOUD_FILE_METADATA_TOO_LARGE
Language=ENU
The cloud file metadata is too large.
.
MessageId=365
SymbolicName=ERROR_CLOUD_FILE_PROPERTY_BLOB_TOO_LARGE
Language=ENU
The cloud file property is too large.
.
MessageId=366
SymbolicName=ERROR_CLOUD_FILE_PROPERTY_BLOB_CHECKSUM_MISMATCH
Language=ENU
The cloud file property is possibly corrupt. The on-disk checksum does not match the computed checksum.
.
MessageId=367
SymbolicName=ERROR_CHILD_PROCESS_BLOCKED
Language=ENU
The process creation has been blocked.
.
MessageId=368
SymbolicName=ERROR_STORAGE_LOST_DATA_PERSISTENCE
Language=ENU
The storage device has lost data or persistence.
.
MessageId=369
SymbolicName=ERROR_FILE_SYSTEM_VIRTUALIZATION_UNAVAILABLE
Language=ENU
The provider that supports file system virtualization is temporarily unavailable.
.
MessageId=370
SymbolicName=ERROR_FILE_SYSTEM_VIRTUALIZATION_METADATA_CORRUPT
Language=ENU
The metadata for file system virtualization is corrupt and unreadable.
.
MessageId=371
SymbolicName=ERROR_FILE_SYSTEM_VIRTUALIZATION_BUSY
Language=ENU
The provider that supports file system virtualization is too busy to complete this operation.
.
MessageId=372
SymbolicName=ERROR_FILE_SYSTEM_VIRTUALIZATION_PROVIDER_UNKNOWN
Language=ENU
The provider that supports file system virtualization is unknown.
.
MessageId=373
SymbolicName=ERROR_GDI_HANDLE_LEAK
Language=ENU
GDI handles were potentially leaked by the application.
.
MessageId=374
SymbolicName=ERROR_CLOUD_FILE_TOO_MANY_PROPERTY_BLOBS
Language=ENU
The maximum number of cloud file properties has been reached.
.
MessageId=375
SymbolicName=ERROR_CLOUD_FILE_PROPERTY_VERSION_NOT_SUPPORTED
Language=ENU
The version of the cloud file property store is not supported.
.
MessageId=376
SymbolicName=ERROR_NOT_A_CLOUD_FILE
Language=ENU
The file is not a cloud file.
.
MessageId=377
SymbolicName=ERROR_CLOUD_FILE_NOT_IN_SYNC
Language=ENU
The file is not in sync with the cloud.
.
MessageId=378
SymbolicName=ERROR_CLOUD_FILE_ALREADY_CONNECTED
Language=ENU
The cloud sync root is already connected with another cloud sync provider.
.
MessageId=379
SymbolicName=ERROR_CLOUD_FILE_NOT_SUPPORTED
Language=ENU
The operation is not supported by the cloud sync provider.
.
MessageId=380
SymbolicName=ERROR_CLOUD_FILE_INVALID_REQUEST
Language=ENU
The cloud operation is invalid.
.
MessageId=381
SymbolicName=ERROR_CLOUD_FILE_READ_ONLY_VOLUME
Language=ENU
The cloud operation is not supported on a read-only volume.
.
MessageId=382
SymbolicName=ERROR_CLOUD_FILE_CONNECTED_PROVIDER_ONLY
Language=ENU
The operation is reserved for a connected cloud sync provider.
.
MessageId=383
SymbolicName=ERROR_CLOUD_FILE_VALIDATION_FAILED
Language=ENU
The cloud sync provider failed to validate the downloaded data.
.
MessageId=384
SymbolicName=ERROR_SMB1_NOT_AVAILABLE
Language=ENU
You can't connect to the file share because it's not secure. This share requires the obsolete SMB1 protocol, which is unsafe and could expose your system to attack.
.
MessageId=385
SymbolicName=ERROR_FILE_SYSTEM_VIRTUALIZATION_INVALID_OPERATION
Language=ENU
The virtualization operation is not allowed on the file in its current state.
.
MessageId=386
SymbolicName=ERROR_CLOUD_FILE_AUTHENTICATION_FAILED
Language=ENU
The cloud sync provider failed user authentication.
.
MessageId=387
SymbolicName=ERROR_CLOUD_FILE_INSUFFICIENT_RESOURCES
Language=ENU
The cloud sync provider failed to perform the operation due to low system resources.
.
MessageId=388
SymbolicName=ERROR_CLOUD_FILE_NETWORK_UNAVAILABLE
Language=ENU
The cloud sync provider failed to perform the operation due to network being unavailable.
.
MessageId=389
SymbolicName=ERROR_CLOUD_FILE_UNSUCCESSFUL
Language=ENU
The cloud operation was unsuccessful.
.
MessageId=390
SymbolicName=ERROR_CLOUD_FILE_NOT_UNDER_SYNC_ROOT
Language=ENU
The operation is only supported on files under a cloud sync root.
.
MessageId=391
SymbolicName=ERROR_CLOUD_FILE_IN_USE
Language=ENU
The operation cannot be performed on cloud files in use.
.
MessageId=392
SymbolicName=ERROR_CLOUD_FILE_PINNED
Language=ENU
The operation cannot be performed on pinned cloud files.
.
MessageId=393
SymbolicName=ERROR_CLOUD_FILE_REQUEST_ABORTED
Language=ENU
The cloud operation was aborted.
.
MessageId=394
SymbolicName=ERROR_CLOUD_FILE_PROPERTY_CORRUPT
Language=ENU
The cloud file's property store is corrupt.
.
MessageId=395
SymbolicName=ERROR_CLOUD_FILE_ACCESS_DENIED
Language=ENU
Access to the cloud file is denied.
.
MessageId=396
SymbolicName=ERROR_CLOUD_FILE_INCOMPATIBLE_HARDLINKS
Language=ENU
The cloud operation cannot be performed on a file with incompatible hardlinks.
.
MessageId=397
SymbolicName=ERROR_CLOUD_FILE_PROPERTY_LOCK_CONFLICT
Language=ENU
The operation failed due to a conflicting cloud file property lock.
.
MessageId=398
SymbolicName=ERROR_CLOUD_FILE_REQUEST_CANCELED
Language=ENU
The cloud operation was canceled by user.
.
MessageId=399
SymbolicName=ERROR_EXTERNAL_SYSKEY_NOT_SUPPORTED
Language=ENU
An externally encrypted syskey has been configured, but the system no longer supports this feature.  Please see https://go.microsoft.com/fwlink/?linkid=851152 for more information.
.
MessageId=400
SymbolicName=ERROR_THREAD_MODE_ALREADY_BACKGROUND
Language=ENU
The thread is already in background processing mode.
.
MessageId=401
SymbolicName=ERROR_THREAD_MODE_NOT_BACKGROUND
Language=ENU
The thread is not in background processing mode.
.
MessageId=402
SymbolicName=ERROR_PROCESS_MODE_ALREADY_BACKGROUND
Language=ENU
The process is already in background processing mode.
.
MessageId=403
SymbolicName=ERROR_PROCESS_MODE_NOT_BACKGROUND
Language=ENU
The process is not in background processing mode.
.
MessageId=404
SymbolicName=ERROR_CLOUD_FILE_PROVIDER_TERMINATED
Language=ENU
The cloud file provider exited unexpectedly.
.
MessageId=405
SymbolicName=ERROR_NOT_A_CLOUD_SYNC_ROOT
Language=ENU
The file is not a cloud sync root.
.
MessageId=406
SymbolicName=ERROR_FILE_PROTECTED_UNDER_DPL
Language=ENU
The read or write operation to an encrypted file could not be completed because the file can only be accessed when the device is unlocked.
.
MessageId=407
SymbolicName=ERROR_VOLUME_NOT_CLUSTER_ALIGNED
Language=ENU
The volume is not cluster aligned on the disk.
.
MessageId=408
SymbolicName=ERROR_NO_PHYSICALLY_ALIGNED_FREE_SPACE_FOUND
Language=ENU
No physically aligned free space was found on the volume.
.
MessageId=409
SymbolicName=ERROR_APPX_FILE_NOT_ENCRYPTED
Language=ENU
The APPX file can not be accessed because it is not encrypted as expected.
.
MessageId=410
SymbolicName=ERROR_RWRAW_ENCRYPTED_FILE_NOT_ENCRYPTED
Language=ENU
A read or write of raw encrypted data cannot be performed because the file is not encrypted.
.
MessageId=411
SymbolicName=ERROR_RWRAW_ENCRYPTED_INVALID_EDATAINFO_FILEOFFSET
Language=ENU
An invalid file offset in the encrypted data info block was passed for read or write operation of file's raw encrypted data.
.
MessageId=412
SymbolicName=ERROR_RWRAW_ENCRYPTED_INVALID_EDATAINFO_FILERANGE
Language=ENU
An invalid offset and length combination in the encrypted data info block was passed for read or write operation of file's raw encrypted data.
.
MessageId=413
SymbolicName=ERROR_RWRAW_ENCRYPTED_INVALID_EDATAINFO_PARAMETER
Language=ENU
An invalid parameter in the encrypted data info block was passed for read or write operation of file's raw encrypted data.
.
MessageId=414
SymbolicName=ERROR_LINUX_SUBSYSTEM_NOT_PRESENT
Language=ENU
The Windows Subsystem for Linux is not installed. You can install by running 'wsl.exe --install'.
.
MessageId=415
SymbolicName=ERROR_FT_READ_FAILURE
Language=ENU
The specified data could not be read from any of the copies.
.
MessageId=416
SymbolicName=ERROR_STORAGE_RESERVE_ID_INVALID
Language=ENU
The specified storage reserve ID is invalid.
.
MessageId=417
SymbolicName=ERROR_STORAGE_RESERVE_DOES_NOT_EXIST
Language=ENU
The specified storage reserve does not exist.
.
MessageId=418
SymbolicName=ERROR_STORAGE_RESERVE_ALREADY_EXISTS
Language=ENU
The specified storage reserve already exists.
.
MessageId=419
SymbolicName=ERROR_STORAGE_RESERVE_NOT_EMPTY
Language=ENU
The specified storage reserve is not empty.
.
MessageId=420
SymbolicName=ERROR_NOT_A_DAX_VOLUME
Language=ENU
This operation requires a DAX volume.
.
MessageId=421
SymbolicName=ERROR_NOT_DAX_MAPPABLE
Language=ENU
This stream is not DAX mappable.
.
MessageId=422
SymbolicName=ERROR_TIME_SENSITIVE_THREAD
Language=ENU
Operation cannot be performed on a time critical thread.
.
MessageId=423
SymbolicName=ERROR_DPL_NOT_SUPPORTED_FOR_USER
Language=ENU
User data protection is not supported for the current or provided user.
.
MessageId=424
SymbolicName=ERROR_CASE_DIFFERING_NAMES_IN_DIR
Language=ENU
This directory contains entries whose names differ only in case.
.
MessageId=425
SymbolicName=ERROR_FILE_NOT_SUPPORTED
Language=ENU
The file cannot be safely opened because it is not supported by this version of Windows.
.
MessageId=426
SymbolicName=ERROR_CLOUD_FILE_REQUEST_TIMEOUT
Language=ENU
The cloud operation was not completed before the time-out period expired.
.
MessageId=427
SymbolicName=ERROR_NO_TASK_QUEUE
Language=ENU
A task queue is required for this operation but none is available.
.
MessageId=428
SymbolicName=ERROR_SRC_SRV_DLL_LOAD_FAILED
Language=ENU
Failed loading a valid version of srcsrv.dll.
.
MessageId=429
SymbolicName=ERROR_NOT_SUPPORTED_WITH_BTT
Language=ENU
This operation is not supported with BTT enabled.
.
MessageId=430
SymbolicName=ERROR_ENCRYPTION_DISABLED
Language=ENU
This operation cannot be performed because encryption is currently disabled.
.
MessageId=431
SymbolicName=ERROR_ENCRYPTING_METADATA_DISALLOWED
Language=ENU
This encryption operation cannot be performed on filesystem metadata.
.
MessageId=432
SymbolicName=ERROR_CANT_CLEAR_ENCRYPTION_FLAG
Language=ENU
Encryption cannot be cleared on this file/directory because it still has an encrypted attribute.
.
MessageId=433
SymbolicName=ERROR_NO_SUCH_DEVICE
Language=ENU
A device which does not exist was specified.
.
MessageId=434
SymbolicName=ERROR_CLOUD_FILE_DEHYDRATION_DISALLOWED
Language=ENU
Dehydration of the cloud file is disallowed by the cloud sync provider.
.
MessageId=435
SymbolicName=ERROR_FILE_SNAP_IN_PROGRESS
Language=ENU
A file snapshot operation was attempted when one is already in progress.
.
MessageId=436
SymbolicName=ERROR_FILE_SNAP_USER_SECTION_NOT_SUPPORTED
Language=ENU
A snapshot of the file cannot be taken because a user-mapped section is present.
.
MessageId=437
SymbolicName=ERROR_FILE_SNAP_MODIFY_NOT_SUPPORTED
Language=ENU
The file snapshot operation was terminated because one of the files was modified in a way incompatible with a snapshot operation.  Please try again.
.
MessageId=438
SymbolicName=ERROR_FILE_SNAP_IO_NOT_COORDINATED
Language=ENU
An I/O request could not be coordinated with a file snapshot operation.
.
MessageId=439
SymbolicName=ERROR_FILE_SNAP_UNEXPECTED_ERROR
Language=ENU
An unexpected error occurred while processing a file snapshot operation.
.
MessageId=440
SymbolicName=ERROR_FILE_SNAP_INVALID_PARAMETER
Language=ENU
A file snapshot operation received an invalid parameter.
.
MessageId=441
SymbolicName=ERROR_UNSATISFIED_DEPENDENCIES
Language=ENU
The operation could not be completed due to one or more unsatisfied dependencies.
.
MessageId=442
SymbolicName=ERROR_CASE_SENSITIVE_PATH
Language=ENU
The file cannot be opened because the path has a case-sensitive directory.
.
MessageId=443
SymbolicName=ERROR_UNEXPECTED_NTCACHEMANAGER_ERROR
Language=ENU
The filesystem couldn't handle one of the CacheManager's callback error codes.
.
MessageId=444
SymbolicName=ERROR_LINUX_SUBSYSTEM_UPDATE_REQUIRED
Language=ENU
Windows Subsystem for Linux must be updated to the latest version to proceed. You can update by running 'wsl.exe --update'.
.
MessageId=445
SymbolicName=ERROR_DLP_POLICY_WARNS_AGAINST_OPERATION
Language=ENU
This action is blocked, but you can choose to allow it. Please refer to the data loss prevention notification for further information.
.
MessageId=446
SymbolicName=ERROR_DLP_POLICY_DENIES_OPERATION
Language=ENU
This action is blocked. Please refer to the data loss prevention notification for further information.
.
MessageId=447
SymbolicName=ERROR_SECURITY_DENIES_OPERATION
Language=ENU
Access is denied because the file contains potentially unwanted software or content the security administrator decided to block.
.
MessageId=448
SymbolicName=ERROR_UNTRUSTED_MOUNT_POINT
Language=ENU
The path cannot be traversed because it contains an untrusted mount point.
.
MessageId=449
SymbolicName=ERROR_DLP_POLICY_SILENTLY_FAIL
Language=ENU
This action is blocked. Please refer to the data loss prevention notification for further information.
.
MessageId=450
SymbolicName=ERROR_CAPAUTHZ_NOT_DEVUNLOCKED
Language=ENU
Neither developer unlocked mode nor side loading mode is enabled on the device.
.
MessageId=451
SymbolicName=ERROR_CAPAUTHZ_CHANGE_TYPE
Language=ENU
Can not change application type during upgrade or re-provision.
.
MessageId=452
SymbolicName=ERROR_CAPAUTHZ_NOT_PROVISIONED
Language=ENU
The application has not been provisioned.
.
MessageId=453
SymbolicName=ERROR_CAPAUTHZ_NOT_AUTHORIZED
Language=ENU
The requested capability can not be authorized for this application.
.
MessageId=454
SymbolicName=ERROR_CAPAUTHZ_NO_POLICY
Language=ENU
There is no capability authorization policy on the device.
.
MessageId=455
SymbolicName=ERROR_CAPAUTHZ_DB_CORRUPTED
Language=ENU
The capability authorization database has been corrupted.
.
MessageId=456
SymbolicName=ERROR_CAPAUTHZ_SCCD_INVALID_CATALOG
Language=ENU
The custom capability's SCCD has an invalid catalog.
.
MessageId=457
SymbolicName=ERROR_CAPAUTHZ_SCCD_NO_AUTH_ENTITY
Language=ENU
None of the authorized entity elements in the SCCD matched the app being installed; either the PFNs don't match, or the element's signature hash doesn't validate.
.
MessageId=458
SymbolicName=ERROR_CAPAUTHZ_SCCD_PARSE_ERROR
Language=ENU
The custom capability's SCCD failed to parse.
.
MessageId=459
SymbolicName=ERROR_CAPAUTHZ_SCCD_DEV_MODE_REQUIRED
Language=ENU
The custom capability's SCCD requires developer mode.
.
MessageId=460
SymbolicName=ERROR_CAPAUTHZ_SCCD_NO_CAPABILITY_MATCH
Language=ENU
There not all declared custom capabilities are found in the SCCD.
.
MessageId=470
SymbolicName=ERROR_CIMFS_IMAGE_CORRUPT
Language=ENU
The CimFS image is corrupt.
.
MessageId=471
SymbolicName=ERROR_CIMFS_IMAGE_VERSION_NOT_SUPPORTED
Language=ENU
The system does not support this version of the CimFS image.
.
MessageId=472
SymbolicName=ERROR_STORAGE_STACK_ACCESS_DENIED
Language=ENU
The storage stack returned STATUS_ACCESS_DENEID for the current operation.
.
MessageId=473
SymbolicName=ERROR_INSUFFICIENT_VIRTUAL_ADDR_RESOURCES
Language=ENU
Insufficient Virtual Address resources to complete the operation.
.
MessageId=474
SymbolicName=ERROR_INDEX_OUT_OF_BOUNDS
Language=ENU
The specified index is beyond the bounds of valid range.
.
MessageId=475
SymbolicName=ERROR_CLOUD_FILE_US_MESSAGE_TIMEOUT
Language=ENU
The cloud provider failed to acknowledge a message before the time-out expired.
.
MessageId=476
SymbolicName=WINERR_476
Language=ENU
This operation requires a developer volume.
.
MessageId=477
SymbolicName=WINERR_477
Language=ENU
The file system GUID in the per machine state did not match the one on disk.
.
MessageId=478
SymbolicName=WINERR_478
Language=ENU
The file system minifilter cannot attach to the developer volume.
.
MessageId=479
SymbolicName=WINERR_479
Language=ENU
The data stored in compressed memory falied to be decompressed.
.
MessageId=480
SymbolicName=ERROR_PNP_QUERY_REMOVE_DEVICE_TIMEOUT
Language=ENU
The operation timed out waiting for this device to complete a PnP query-remove request due to a potential hang in its device stack. The system may need to be rebooted to complete the request.
.
MessageId=481
SymbolicName=ERROR_PNP_QUERY_REMOVE_RELATED_DEVICE_TIMEOUT
Language=ENU
The operation timed out waiting for this device to complete a PnP query-remove request due to a potential hang in the device stack of a related device. The system may need to be rebooted to complete the operation.
.
MessageId=482
SymbolicName=ERROR_PNP_QUERY_REMOVE_UNRELATED_DEVICE_TIMEOUT
Language=ENU
The operation timed out waiting for this device to complete a PnP query-remove request due to a potential hang in the device stack of an unrelated device. The system may need to be rebooted to complete the operation.
.
MessageId=483
SymbolicName=ERROR_DEVICE_HARDWARE_ERROR
Language=ENU
The request failed due to a fatal device hardware error.
.
MessageId=484
SymbolicName=WINERR_484
Language=ENU
The request failed due to the device passively/actively NACKing the operation.
.
MessageId=487
SymbolicName=ERROR_INVALID_ADDRESS
Language=ENU
Attempt to access invalid address.
.
MessageId=488
SymbolicName=ERROR_HAS_SYSTEM_CRITICAL_FILES
Language=ENU
The volume contains paging, crash dump or other system critical files.
.
MessageId=489
SymbolicName=ERROR_ENCRYPTED_FILE_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on an encrypted file.
.
MessageId=490
SymbolicName=ERROR_SPARSE_FILE_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on a sparse file.
.
MessageId=491
SymbolicName=ERROR_PAGEFILE_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on a pagefile.
.
MessageId=492
SymbolicName=ERROR_VOLUME_NOT_SUPPORTED
Language=ENU
The specified operation is not supported on a volume.
.
MessageId=493
SymbolicName=ERROR_NOT_SUPPORTED_WITH_BYPASSIO
Language=ENU
The specified operation is not supported on a BypassIO enabled file.
.
MessageId=494
SymbolicName=ERROR_NO_BYPASSIO_DRIVER_SUPPORT
Language=ENU
The specified driver does not support BypassIO operations.
.
MessageId=495
SymbolicName=ERROR_NOT_SUPPORTED_WITH_ENCRYPTION
Language=ENU
The specified operation is not supported while encryption is enabled on the target object.
.
MessageId=496
SymbolicName=ERROR_NOT_SUPPORTED_WITH_COMPRESSION
Language=ENU
The specified operation is not supported while compression is enabled on the target object.
.
MessageId=497
SymbolicName=ERROR_NOT_SUPPORTED_WITH_REPLICATION
Language=ENU
The specified operation is not supported while replication is enabled on the target object.
.
MessageId=498
SymbolicName=ERROR_NOT_SUPPORTED_WITH_DEDUPLICATION
Language=ENU
The specified operation is not supported while deduplication is enabled on the target object.
.
MessageId=499
SymbolicName=ERROR_NOT_SUPPORTED_WITH_AUDITING
Language=ENU
The specified operation is not supported while auditing is enabled on the target object.
.
MessageId=500
SymbolicName=ERROR_USER_PROFILE_LOAD
Language=ENU
User profile cannot be loaded.
.
MessageId=501
SymbolicName=ERROR_SESSION_KEY_TOO_SHORT
Language=ENU
The negotiated session key does not meet the minimum length requirement.
.
MessageId=502
SymbolicName=ERROR_ACCESS_DENIED_APPDATA
Language=ENU
Access denied when accessing the user profile.
.
MessageId=503
SymbolicName=ERROR_NOT_SUPPORTED_WITH_MONITORING
Language=ENU
The specified operation is not supported while monitoring is enabled on the target object.
.
MessageId=504
SymbolicName=ERROR_NOT_SUPPORTED_WITH_SNAPSHOT
Language=ENU
The specified operation is not supported while snapshot is enabled on the target object.
.
MessageId=505
SymbolicName=ERROR_NOT_SUPPORTED_WITH_VIRTUALIZATION
Language=ENU
The specified operation is not supported while virtualization is enabled on the target object.
.
MessageId=506
SymbolicName=ERROR_BYPASSIO_FLT_NOT_SUPPORTED
Language=ENU
At least one minifilter does not support bypass IO.
.
MessageId=507
SymbolicName=ERROR_DEVICE_RESET_REQUIRED
Language=ENU
The device needs to be reset.
.
MessageId=508
SymbolicName=ERROR_VOLUME_WRITE_ACCESS_DENIED
Language=ENU
The volume is opened for exclusive write access, preventing files from being opened for write access.
.
MessageId=509
SymbolicName=ERROR_NOT_SUPPORTED_WITH_CACHED_HANDLE
Language=ENU
The specified operation is not supported on a file opened for cached IO.
.
MessageId=510
SymbolicName=ERROR_FS_METADATA_INCONSISTENT
Language=ENU
The file system encountered a metadata file with inconsistent data.
.
MessageId=511
SymbolicName=ERROR_BLOCK_WEAK_REFERENCE_INVALID
Language=ENU
A file system block being referenced has been modified after containing a weak reference.
.
MessageId=512
SymbolicName=ERROR_BLOCK_SOURCE_WEAK_REFERENCE_INVALID
Language=ENU
The source file system block being referenced has been modified after containing a weak reference.
.
MessageId=513
SymbolicName=ERROR_BLOCK_TARGET_WEAK_REFERENCE_INVALID
Language=ENU
The target file system block being referenced has been modified after containing a weak reference.
.
MessageId=514
SymbolicName=ERROR_BLOCK_SHARED
Language=ENU
The target file system block is shared between multiple extents.
.
MessageId=515
SymbolicName=WINERR_515
Language=ENU
Volume format is up to date already.
.
MessageId=516
SymbolicName=WINERR_516
Language=ENU
Volume upgrade is pending.  A reboot or re-mount of the volume is required.
.
MessageId=517
SymbolicName=WINERR_517
Language=ENU
Volume upgrade is disabled.
.
MessageId=518
SymbolicName=WINERR_518
Language=ENU
Volume upgrade is disabled until Windows OS downgrade period has expired.
.
MessageId=519
SymbolicName=WINERR_519
Language=ENU
The configured value is not valid.
.
MessageId=520
SymbolicName=WINERR_520
Language=ENU
The data stored in compressed memory falied to be decompressed due to encountering an uncorrectable hardware memory error.
.
MessageId=521
SymbolicName=WINERR_521
Language=ENU
The operation was aborted because the observed volume identity or current state was not expected.
.
MessageId=523
SymbolicName=WINERR_523
Language=ENU
Cloud file cannot be retrieved from your cloud provider. Try again after responding to your cloud provider's request for action.
.
MessageId=525
SymbolicName=WINERR_525
Language=ENU
The specified operation is not supported on a system file.
.
MessageId=534
SymbolicName=ERROR_ARITHMETIC_OVERFLOW
Language=ENU
Arithmetic result exceeded 32 bits.
.
MessageId=535
SymbolicName=ERROR_PIPE_CONNECTED
Language=ENU
There is a process on other end of the pipe.
.
MessageId=536
SymbolicName=ERROR_PIPE_LISTENING
Language=ENU
Waiting for a process to open the other end of the pipe.
.
MessageId=537
SymbolicName=ERROR_VERIFIER_STOP
Language=ENU
Application verifier has found an error in the current process.
.
MessageId=538
SymbolicName=ERROR_ABIOS_ERROR
Language=ENU
An error occurred in the ABIOS subsystem.
.
MessageId=539
SymbolicName=ERROR_WX86_WARNING
Language=ENU
A warning occurred in the WX86 subsystem.
.
MessageId=540
SymbolicName=ERROR_WX86_ERROR
Language=ENU
An error occurred in the WX86 subsystem.
.
MessageId=541
SymbolicName=ERROR_TIMER_NOT_CANCELED
Language=ENU
An attempt was made to cancel or set a timer that has an associated APC and the subject thread is not the thread that originally set the timer with an associated APC routine.
.
MessageId=542
SymbolicName=ERROR_UNWIND
Language=ENU
Unwind exception code.
.
MessageId=543
SymbolicName=ERROR_BAD_STACK
Language=ENU
An invalid or unaligned stack was encountered during an unwind operation.
.
MessageId=544
SymbolicName=ERROR_INVALID_UNWIND_TARGET
Language=ENU
An invalid unwind target was encountered during an unwind operation.
.
MessageId=545
SymbolicName=ERROR_INVALID_PORT_ATTRIBUTES
Language=ENU
Invalid Object Attributes specified to NtCreatePort or invalid Port Attributes specified to NtConnectPort
.
MessageId=546
SymbolicName=ERROR_PORT_MESSAGE_TOO_LONG
Language=ENU
Length of message passed to NtRequestPort or NtRequestWaitReplyPort was longer than the maximum message allowed by the port.
.
MessageId=547
SymbolicName=ERROR_INVALID_QUOTA_LOWER
Language=ENU
An attempt was made to lower a quota limit below the current usage.
.
MessageId=548
SymbolicName=ERROR_DEVICE_ALREADY_ATTACHED
Language=ENU
An attempt was made to attach to a device that was already attached to another device.
.
MessageId=549
SymbolicName=ERROR_INSTRUCTION_MISALIGNMENT
Language=ENU
An attempt was made to execute an instruction at an unaligned address and the host system does not support unaligned instruction references.
.
MessageId=550
SymbolicName=ERROR_PROFILING_NOT_STARTED
Language=ENU
Profiling not started.
.
MessageId=551
SymbolicName=ERROR_PROFILING_NOT_STOPPED
Language=ENU
Profiling not stopped.
.
MessageId=552
SymbolicName=ERROR_COULD_NOT_INTERPRET
Language=ENU
The passed ACL did not contain the minimum required information.
.
MessageId=553
SymbolicName=ERROR_PROFILING_AT_LIMIT
Language=ENU
The number of active profiling objects is at the maximum and no more may be started.
.
MessageId=554
SymbolicName=ERROR_CANT_WAIT
Language=ENU
Used to indicate that an operation cannot continue without blocking for I/O.
.
MessageId=555
SymbolicName=ERROR_CANT_TERMINATE_SELF
Language=ENU
Indicates that a thread attempted to terminate itself by default (called NtTerminateThread with NULL) and it was the last thread in the current process.
.
MessageId=556
SymbolicName=ERROR_UNEXPECTED_MM_CREATE_ERR
Language=ENU
If an MM error is returned which is not defined in the standard FsRtl filter, it is converted to one of the following errors which is guaranteed to be in the filter.
.
MessageId=557
SymbolicName=ERROR_UNEXPECTED_MM_MAP_ERROR
Language=ENU
If an MM error is returned which is not defined in the standard FsRtl filter, it is converted to one of the following errors which is guaranteed to be in the filter.
.
MessageId=558
SymbolicName=ERROR_UNEXPECTED_MM_EXTEND_ERR
Language=ENU
If an MM error is returned which is not defined in the standard FsRtl filter, it is converted to one of the following errors which is guaranteed to be in the filter.
.
MessageId=559
SymbolicName=ERROR_BAD_FUNCTION_TABLE
Language=ENU
A malformed function table was encountered during an unwind operation.
.
MessageId=560
SymbolicName=ERROR_NO_GUID_TRANSLATION
Language=ENU
Indicates that an attempt was made to assign protection to a file system file or directory and one of the SIDs in the security descriptor could not be translated into a GUID that could be stored by the file system.
.
MessageId=561
SymbolicName=ERROR_INVALID_LDT_SIZE
Language=ENU
Indicates that an attempt was made to grow an LDT by setting its size, or that the size was not an even number of selectors.
.
MessageId=563
SymbolicName=ERROR_INVALID_LDT_OFFSET
Language=ENU
Indicates that the starting value for the LDT information was not an integral multiple of the selector size.
.
MessageId=564
SymbolicName=ERROR_INVALID_LDT_DESCRIPTOR
Language=ENU
Indicates that the user supplied an invalid descriptor when trying to set up Ldt descriptors.
.
MessageId=565
SymbolicName=ERROR_TOO_MANY_THREADS
Language=ENU
Indicates a process has too many threads to perform the requested action. For example, assignment of a primary token may only be performed when a process has zero or one threads.
.
MessageId=566
SymbolicName=ERROR_THREAD_NOT_IN_PROCESS
Language=ENU
An attempt was made to operate on a thread within a specific process, but the thread specified is not in the process specified.
.
MessageId=567
SymbolicName=ERROR_PAGEFILE_QUOTA_EXCEEDED
Language=ENU
Pagefile quota was exceeded.
.
MessageId=568
SymbolicName=ERROR_LOGON_SERVER_CONFLICT
Language=ENU
The Netlogon service cannot start because another Netlogon service running in the domain conflicts with the specified role.
.
MessageId=569
SymbolicName=ERROR_SYNCHRONIZATION_REQUIRED
Language=ENU
The SAM database on a Windows Server is significantly out of synchronization with the copy on the Domain Controller. A complete synchronization is required.
.
MessageId=570
SymbolicName=ERROR_NET_OPEN_FAILED
Language=ENU
The NtCreateFile API failed. This error should never be returned to an application, it is a place holder for the Windows Lan Manager Redirector to use in its internal error mapping routines.
.
MessageId=571
SymbolicName=ERROR_IO_PRIVILEGE_FAILED
Language=ENU
{Privilege Failed}
.
MessageId=572
SymbolicName=ERROR_CONTROL_C_EXIT
Language=ENU
{Application Exit by CTRL+C}
.
MessageId=573
SymbolicName=ERROR_MISSING_SYSTEMFILE
Language=ENU
{Missing System File}
.
MessageId=574
SymbolicName=ERROR_UNHANDLED_EXCEPTION
Language=ENU
{Application Error}
.
MessageId=575
SymbolicName=ERROR_APP_INIT_FAILURE
Language=ENU
{Application Error}
.
MessageId=576
SymbolicName=ERROR_PAGEFILE_CREATE_FAILED
Language=ENU
{Unable to Create Paging File}
.
MessageId=577
SymbolicName=ERROR_INVALID_IMAGE_HASH
Language=ENU
Windows cannot verify the digital signature for this file. A recent hardware or software change might have installed a file that is signed incorrectly or damaged, or that might be malicious software from an unknown source.
.
MessageId=578
SymbolicName=ERROR_NO_PAGEFILE
Language=ENU
{No Paging File Specified}
.
MessageId=579
SymbolicName=ERROR_ILLEGAL_FLOAT_CONTEXT
Language=ENU
{EXCEPTION}
.
MessageId=580
SymbolicName=ERROR_NO_EVENT_PAIR
Language=ENU
An event pair synchronization operation was performed using the thread specific client/server event pair object, but no event pair object was associated with the thread.
.
MessageId=581
SymbolicName=ERROR_DOMAIN_CTRLR_CONFIG_ERROR
Language=ENU
A Windows Server has an incorrect configuration.
.
MessageId=582
SymbolicName=ERROR_ILLEGAL_CHARACTER
Language=ENU
An illegal character was encountered. For a multi-byte character set this includes a lead byte without a succeeding trail byte. For the Unicode character set this includes the characters 0xFFFF and 0xFFFE.
.
MessageId=583
SymbolicName=ERROR_UNDEFINED_CHARACTER
Language=ENU
The Unicode character is not defined in the Unicode character set installed on the system.
.
MessageId=584
SymbolicName=ERROR_FLOPPY_VOLUME
Language=ENU
The paging file cannot be created on a floppy diskette.
.
MessageId=585
SymbolicName=ERROR_BIOS_FAILED_TO_CONNECT_INTERRUPT
Language=ENU
The system BIOS failed to connect a system interrupt to the device or bus for which the device is connected.
.
MessageId=586
SymbolicName=ERROR_BACKUP_CONTROLLER
Language=ENU
This operation is only allowed for the Primary Domain Controller of the domain.
.
MessageId=587
SymbolicName=ERROR_MUTANT_LIMIT_EXCEEDED
Language=ENU
An attempt was made to acquire a mutant such that its maximum count would have been exceeded.
.
MessageId=588
SymbolicName=ERROR_FS_DRIVER_REQUIRED
Language=ENU
A volume has been accessed for which a file system driver is required that has not yet been loaded.
.
MessageId=589
SymbolicName=ERROR_CANNOT_LOAD_REGISTRY_FILE
Language=ENU
{Registry File Failure}
.
MessageId=590
SymbolicName=ERROR_DEBUG_ATTACH_FAILED
Language=ENU
{Unexpected Failure in DebugActiveProcess}
.
MessageId=591
SymbolicName=ERROR_SYSTEM_PROCESS_TERMINATED
Language=ENU
{Fatal System Error}
.
MessageId=592
SymbolicName=ERROR_DATA_NOT_ACCEPTED
Language=ENU
{Data Not Accepted}
.
MessageId=593
SymbolicName=ERROR_VDM_HARD_ERROR
Language=ENU
NTVDM encountered a hard error.
.
MessageId=594
SymbolicName=ERROR_DRIVER_CANCEL_TIMEOUT
Language=ENU
{Cancel Timeout}
.
MessageId=595
SymbolicName=ERROR_REPLY_MESSAGE_MISMATCH
Language=ENU
{Reply Message Mismatch}
.
MessageId=596
SymbolicName=ERROR_LOST_WRITEBEHIND_DATA
Language=ENU
{Delayed Write Failed}
.
MessageId=597
SymbolicName=ERROR_CLIENT_SERVER_PARAMETERS_INVALID
Language=ENU
The parameter(s) passed to the server in the client/server shared memory window were invalid. Too much data may have been put in the shared memory window.
.
MessageId=598
SymbolicName=ERROR_NOT_TINY_STREAM
Language=ENU
The stream is not a tiny stream.
.
MessageId=599
SymbolicName=ERROR_STACK_OVERFLOW_READ
Language=ENU
The request must be handled by the stack overflow code.
.
MessageId=600
SymbolicName=ERROR_CONVERT_TO_LARGE
Language=ENU
Internal OFS status codes indicating how an allocation operation is handled. Either it is retried after the containing onode is moved or the extent stream is converted to a large stream.
.
MessageId=601
SymbolicName=ERROR_FOUND_OUT_OF_SCOPE
Language=ENU
The attempt to find the object found an object matching by ID on the volume but it is out of the scope of the handle used for the operation.
.
MessageId=602
SymbolicName=ERROR_ALLOCATE_BUCKET
Language=ENU
The bucket array must be grown. Retry transaction after doing so.
.
MessageId=603
SymbolicName=ERROR_MARSHALL_OVERFLOW
Language=ENU
The user/kernel marshalling buffer has overflowed.
.
MessageId=604
SymbolicName=ERROR_INVALID_VARIANT
Language=ENU
The supplied variant structure contains invalid data.
.
MessageId=605
SymbolicName=ERROR_BAD_COMPRESSION_BUFFER
Language=ENU
The specified buffer contains ill-formed data.
.
MessageId=606
SymbolicName=ERROR_AUDIT_FAILED
Language=ENU
{Audit Failed}
.
MessageId=607
SymbolicName=ERROR_TIMER_RESOLUTION_NOT_SET
Language=ENU
The timer resolution was not previously set by the current process.
.
MessageId=608
SymbolicName=ERROR_INSUFFICIENT_LOGON_INFO
Language=ENU
There is insufficient account information to log you on.
.
MessageId=609
SymbolicName=ERROR_BAD_DLL_ENTRYPOINT
Language=ENU
{Invalid DLL Entrypoint}
.
MessageId=610
SymbolicName=ERROR_BAD_SERVICE_ENTRYPOINT
Language=ENU
{Invalid Service Callback Entrypoint}
.
MessageId=611
SymbolicName=ERROR_IP_ADDRESS_CONFLICT1
Language=ENU
There is an IP address conflict with another system on the network
.
MessageId=612
SymbolicName=ERROR_IP_ADDRESS_CONFLICT2
Language=ENU
There is an IP address conflict with another system on the network
.
MessageId=613
SymbolicName=ERROR_REGISTRY_QUOTA_LIMIT
Language=ENU
{Low On Registry Space}
.
MessageId=614
SymbolicName=ERROR_NO_CALLBACK_ACTIVE
Language=ENU
A callback return system service cannot be executed when no callback is active.
.
MessageId=615
SymbolicName=ERROR_PWD_TOO_SHORT
Language=ENU
The password provided is too short to meet the policy of your user account.
.
MessageId=616
SymbolicName=ERROR_PWD_TOO_RECENT
Language=ENU
The policy of your user account does not allow you to change passwords too frequently.
.
MessageId=617
SymbolicName=ERROR_PWD_HISTORY_CONFLICT
Language=ENU
You have attempted to change your password to one that you have used in the past.
.
MessageId=618
SymbolicName=ERROR_UNSUPPORTED_COMPRESSION
Language=ENU
The specified compression format is unsupported.
.
MessageId=619
SymbolicName=ERROR_INVALID_HW_PROFILE
Language=ENU
The specified hardware profile configuration is invalid.
.
MessageId=620
SymbolicName=ERROR_INVALID_PLUGPLAY_DEVICE_PATH
Language=ENU
The specified Plug and Play registry device path is invalid.
.
MessageId=621
SymbolicName=ERROR_QUOTA_LIST_INCONSISTENT
Language=ENU
The specified quota list is internally inconsistent with its descriptor.
.
MessageId=622
SymbolicName=ERROR_EVALUATION_EXPIRATION
Language=ENU
{Windows Evaluation Notification}
.
MessageId=623
SymbolicName=ERROR_ILLEGAL_DLL_RELOCATION
Language=ENU
{Illegal System DLL Relocation}
.
MessageId=624
SymbolicName=ERROR_DLL_INIT_FAILED_LOGOFF
Language=ENU
{DLL Initialization Failed}
.
MessageId=625
SymbolicName=ERROR_VALIDATE_CONTINUE
Language=ENU
The validation process needs to continue on to the next step.
.
MessageId=626
SymbolicName=ERROR_NO_MORE_MATCHES
Language=ENU
There are no more matches for the current index enumeration.
.
MessageId=627
SymbolicName=ERROR_RANGE_LIST_CONFLICT
Language=ENU
The range could not be added to the range list because of a conflict.
.
MessageId=628
SymbolicName=ERROR_SERVER_SID_MISMATCH
Language=ENU
The server process is running under a SID different than that required by client.
.
MessageId=629
SymbolicName=ERROR_CANT_ENABLE_DENY_ONLY
Language=ENU
A group marked use for deny only cannot be enabled.
.
MessageId=630
SymbolicName=ERROR_FLOAT_MULTIPLE_FAULTS
Language=ENU
{EXCEPTION}
.
MessageId=631
SymbolicName=ERROR_FLOAT_MULTIPLE_TRAPS
Language=ENU
{EXCEPTION}
.
MessageId=632
SymbolicName=ERROR_NOINTERFACE
Language=ENU
The requested interface is not supported.
.
MessageId=633
SymbolicName=ERROR_DRIVER_FAILED_SLEEP
Language=ENU
{System Standby Failed}
.
MessageId=634
SymbolicName=ERROR_CORRUPT_SYSTEM_FILE
Language=ENU
The system file %1 has become corrupt and has been replaced.
.
MessageId=635
SymbolicName=ERROR_COMMITMENT_MINIMUM
Language=ENU
{Virtual Memory Minimum Too Low}
.
MessageId=636
SymbolicName=ERROR_PNP_RESTART_ENUMERATION
Language=ENU
A device was removed so enumeration must be restarted.
.
MessageId=637
SymbolicName=ERROR_SYSTEM_IMAGE_BAD_SIGNATURE
Language=ENU
{Fatal System Error}
.
MessageId=638
SymbolicName=ERROR_PNP_REBOOT_REQUIRED
Language=ENU
Device will not start without a reboot.
.
MessageId=639
SymbolicName=ERROR_INSUFFICIENT_POWER
Language=ENU
There is not enough power to complete the requested operation.
.
MessageId=640
SymbolicName=ERROR_MULTIPLE_FAULT_VIOLATION
Language=ENU
ERROR_MULTIPLE_FAULT_VIOLATION
.
MessageId=641
SymbolicName=ERROR_SYSTEM_SHUTDOWN
Language=ENU
The system is in the process of shutting down.
.
MessageId=642
SymbolicName=ERROR_PORT_NOT_SET
Language=ENU
An attempt to remove a processes DebugPort was made, but a port was not already associated with the process.
.
MessageId=643
SymbolicName=ERROR_DS_VERSION_CHECK_FAILURE
Language=ENU
This version of Windows is not compatible with the behavior version of directory forest, domain or domain controller.
.
MessageId=644
SymbolicName=ERROR_RANGE_NOT_FOUND
Language=ENU
The specified range could not be found in the range list.
.
MessageId=646
SymbolicName=ERROR_NOT_SAFE_MODE_DRIVER
Language=ENU
The driver was not loaded because the system is booting into safe mode.
.
MessageId=647
SymbolicName=ERROR_FAILED_DRIVER_ENTRY
Language=ENU
The driver was not loaded because it failed its initialization call.
.
MessageId=648
SymbolicName=ERROR_DEVICE_ENUMERATION_ERROR
Language=ENU
The "%hs" encountered an error while applying power or reading the device configuration.
.
MessageId=649
SymbolicName=ERROR_MOUNT_POINT_NOT_RESOLVED
Language=ENU
The create operation failed because the name contained at least one mount point which resolves to a volume to which the specified device object is not attached.
.
MessageId=650
SymbolicName=ERROR_INVALID_DEVICE_OBJECT_PARAMETER
Language=ENU
The device object parameter is either not a valid device object or is not attached to the volume specified by the file name.
.
MessageId=651
SymbolicName=ERROR_MCA_OCCURED
Language=ENU
A Machine Check Error has occurred. Please check the system eventlog for additional information.
.
MessageId=652
SymbolicName=ERROR_DRIVER_DATABASE_ERROR
Language=ENU
There was error [%2] processing the driver database.
.
MessageId=653
SymbolicName=ERROR_SYSTEM_HIVE_TOO_LARGE
Language=ENU
System hive size has exceeded its limit.
.
MessageId=654
SymbolicName=ERROR_DRIVER_FAILED_PRIOR_UNLOAD
Language=ENU
The driver could not be loaded because a previous version of the driver is still in memory.
.
MessageId=655
SymbolicName=ERROR_VOLSNAP_PREPARE_HIBERNATE
Language=ENU
{Volume Shadow Copy Service}
.
MessageId=656
SymbolicName=ERROR_HIBERNATION_FAILURE
Language=ENU
The system has failed to hibernate (The error code is %hs). Hibernation will be disabled until the system is restarted.
.
MessageId=657
SymbolicName=ERROR_PWD_TOO_LONG
Language=ENU
The password provided is too long to meet the policy of your user account.
.
MessageId=665
SymbolicName=ERROR_FILE_SYSTEM_LIMITATION
Language=ENU
The requested operation could not be completed due to a file system limitation
.
MessageId=668
SymbolicName=ERROR_ASSERTION_FAILURE
Language=ENU
An assertion failure has occurred.
.
MessageId=669
SymbolicName=ERROR_ACPI_ERROR
Language=ENU
An error occurred in the ACPI subsystem.
.
MessageId=670
SymbolicName=ERROR_WOW_ASSERTION
Language=ENU
WOW Assertion Error.
.
MessageId=671
SymbolicName=ERROR_PNP_BAD_MPS_TABLE
Language=ENU
A device is missing in the system BIOS MPS table. This device will not be used.
.
MessageId=672
SymbolicName=ERROR_PNP_TRANSLATION_FAILED
Language=ENU
A translator failed to translate resources.
.
MessageId=673
SymbolicName=ERROR_PNP_IRQ_TRANSLATION_FAILED
Language=ENU
A IRQ translator failed to translate resources.
.
MessageId=674
SymbolicName=ERROR_PNP_INVALID_ID
Language=ENU
Driver %2 returned invalid ID for a child device (%3).
.
MessageId=675
SymbolicName=ERROR_WAKE_SYSTEM_DEBUGGER
Language=ENU
{Kernel Debugger Awakened}
.
MessageId=676
SymbolicName=ERROR_HANDLES_CLOSED
Language=ENU
{Handles Closed}
.
MessageId=677
SymbolicName=ERROR_EXTRANEOUS_INFORMATION
Language=ENU
{Too Much Information}
.
MessageId=678
SymbolicName=ERROR_RXACT_COMMIT_NECESSARY
Language=ENU
This warning level status indicates that the transaction state already exists for the registry sub-tree, but that a transaction commit was previously aborted.
.
MessageId=679
SymbolicName=ERROR_MEDIA_CHECK
Language=ENU
{Media Changed}
.
MessageId=680
SymbolicName=ERROR_GUID_SUBSTITUTION_MADE
Language=ENU
{GUID Substitution}
.
MessageId=681
SymbolicName=ERROR_STOPPED_ON_SYMLINK
Language=ENU
The create operation stopped after reaching a symbolic link
.
MessageId=682
SymbolicName=ERROR_LONGJUMP
Language=ENU
A long jump has been executed.
.
MessageId=683
SymbolicName=ERROR_PLUGPLAY_QUERY_VETOED
Language=ENU
The Plug and Play query operation was not successful.
.
MessageId=684
SymbolicName=ERROR_UNWIND_CONSOLIDATE
Language=ENU
A frame consolidation has been executed.
.
MessageId=685
SymbolicName=ERROR_REGISTRY_HIVE_RECOVERED
Language=ENU
{Registry Hive Recovered}
.
MessageId=686
SymbolicName=ERROR_DLL_MIGHT_BE_INSECURE
Language=ENU
The application is attempting to run executable code from the module %hs. This may be insecure. An alternative, %hs, is available. Should the application use the secure module %hs?
.
MessageId=687
SymbolicName=ERROR_DLL_MIGHT_BE_INCOMPATIBLE
Language=ENU
The application is loading executable code from the module %hs. This is secure, but may be incompatible with previous releases of the operating system. An alternative, %hs, is available. Should the application use the secure module %hs?
.
MessageId=688
SymbolicName=ERROR_DBG_EXCEPTION_NOT_HANDLED
Language=ENU
Debugger did not handle the exception.
.
MessageId=689
SymbolicName=ERROR_DBG_REPLY_LATER
Language=ENU
Debugger will reply later.
.
MessageId=690
SymbolicName=ERROR_DBG_UNABLE_TO_PROVIDE_HANDLE
Language=ENU
Debugger cannot provide handle.
.
MessageId=691
SymbolicName=ERROR_DBG_TERMINATE_THREAD
Language=ENU
Debugger terminated thread.
.
MessageId=692
SymbolicName=ERROR_DBG_TERMINATE_PROCESS
Language=ENU
Debugger terminated process.
.
MessageId=693
SymbolicName=ERROR_DBG_CONTROL_C
Language=ENU
Debugger got control C.
.
MessageId=694
SymbolicName=ERROR_DBG_PRINTEXCEPTION_C
Language=ENU
Debugger printed exception on control C.
.
MessageId=695
SymbolicName=ERROR_DBG_RIPEXCEPTION
Language=ENU
Debugger received RIP exception.
.
MessageId=696
SymbolicName=ERROR_DBG_CONTROL_BREAK
Language=ENU
Debugger received control break.
.
MessageId=697
SymbolicName=ERROR_DBG_COMMAND_EXCEPTION
Language=ENU
Debugger command communication exception.
.
MessageId=698
SymbolicName=ERROR_OBJECT_NAME_EXISTS
Language=ENU
{Object Exists}
.
MessageId=699
SymbolicName=ERROR_THREAD_WAS_SUSPENDED
Language=ENU
{Thread Suspended}
.
MessageId=700
SymbolicName=ERROR_IMAGE_NOT_AT_BASE
Language=ENU
{Image Relocated}
.
MessageId=701
SymbolicName=ERROR_RXACT_STATE_CREATED
Language=ENU
This informational level status indicates that a specified registry sub-tree transaction state did not yet exist and had to be created.
.
MessageId=702
SymbolicName=ERROR_SEGMENT_NOTIFICATION
Language=ENU
{Segment Load}
.
MessageId=703
SymbolicName=ERROR_BAD_CURRENT_DIRECTORY
Language=ENU
{Invalid Current Directory}
.
MessageId=704
SymbolicName=ERROR_FT_READ_RECOVERY_FROM_BACKUP
Language=ENU
{Redundant Read}
.
MessageId=705
SymbolicName=ERROR_FT_WRITE_RECOVERY
Language=ENU
{Redundant Write}
.
MessageId=706
SymbolicName=ERROR_IMAGE_MACHINE_TYPE_MISMATCH
Language=ENU
{Machine Type Mismatch}
.
MessageId=707
SymbolicName=ERROR_RECEIVE_PARTIAL
Language=ENU
{Partial Data Received}
.
MessageId=708
SymbolicName=ERROR_RECEIVE_EXPEDITED
Language=ENU
{Expedited Data Received}
.
MessageId=709
SymbolicName=ERROR_RECEIVE_PARTIAL_EXPEDITED
Language=ENU
{Partial Expedited Data Received}
.
MessageId=710
SymbolicName=ERROR_EVENT_DONE
Language=ENU
{TDI Event Done}
.
MessageId=711
SymbolicName=ERROR_EVENT_PENDING
Language=ENU
{TDI Event Pending}
.
MessageId=712
SymbolicName=ERROR_CHECKING_FILE_SYSTEM
Language=ENU
Checking file system on %wZ
.
MessageId=713
SymbolicName=ERROR_FATAL_APP_EXIT
Language=ENU
{Fatal Application Exit}
.
MessageId=714
SymbolicName=ERROR_PREDEFINED_HANDLE
Language=ENU
The specified registry key is referenced by a predefined handle.
.
MessageId=715
SymbolicName=ERROR_WAS_UNLOCKED
Language=ENU
{Page Unlocked}
.
MessageId=716
SymbolicName=ERROR_SERVICE_NOTIFICATION
Language=ENU
%hs
.
MessageId=717
SymbolicName=ERROR_WAS_LOCKED
Language=ENU
{Page Locked}
.
MessageId=718
SymbolicName=ERROR_LOG_HARD_ERROR
Language=ENU
Application popup: %1 : %2
.
MessageId=719
SymbolicName=ERROR_ALREADY_WIN32
Language=ENU
ERROR_ALREADY_WIN32
.
MessageId=720
SymbolicName=ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE
Language=ENU
{Machine Type Mismatch}
.
MessageId=721
SymbolicName=ERROR_NO_YIELD_PERFORMED
Language=ENU
A yield execution was performed and no thread was available to run.
.
MessageId=722
SymbolicName=ERROR_TIMER_RESUME_IGNORED
Language=ENU
The resumable flag to a timer API was ignored.
.
MessageId=723
SymbolicName=ERROR_ARBITRATION_UNHANDLED
Language=ENU
The arbiter has deferred arbitration of these resources to its parent
.
MessageId=724
SymbolicName=ERROR_CARDBUS_NOT_SUPPORTED
Language=ENU
The inserted CardBus device cannot be started because of a configuration error on "%hs".
.
MessageId=725
SymbolicName=ERROR_MP_PROCESSOR_MISMATCH
Language=ENU
The CPUs in this multiprocessor system are not all the same revision level. To use all processors the operating system restricts itself to the features of the least capable processor in the system. Should problems occur with this system, contact the CPU manufacturer to see if this mix of processors is supported.
.
MessageId=726
SymbolicName=ERROR_HIBERNATED
Language=ENU
The system was put into hibernation.
.
MessageId=727
SymbolicName=ERROR_RESUME_HIBERNATION
Language=ENU
The system was resumed from hibernation.
.
MessageId=728
SymbolicName=ERROR_FIRMWARE_UPDATED
Language=ENU
Windows has detected that the system firmware (BIOS) was updated [previous firmware date = %2, current firmware date %3].
.
MessageId=729
SymbolicName=ERROR_DRIVERS_LEAKING_LOCKED_PAGES
Language=ENU
A device driver is leaking locked I/O pages causing system degradation. The system has automatically enabled tracking code in order to try and catch the culprit.
.
MessageId=730
SymbolicName=ERROR_WAKE_SYSTEM
Language=ENU
The system has awoken
.
MessageId=731
SymbolicName=ERROR_WAIT_1
Language=ENU
ERROR_WAIT_1
.
MessageId=732
SymbolicName=ERROR_WAIT_2
Language=ENU
ERROR_WAIT_2
.
MessageId=733
SymbolicName=ERROR_WAIT_3
Language=ENU
ERROR_WAIT_3
.
MessageId=734
SymbolicName=ERROR_WAIT_63
Language=ENU
ERROR_WAIT_63
.
MessageId=735
SymbolicName=ERROR_ABANDONED_WAIT_0
Language=ENU
ERROR_ABANDONED_WAIT_0
.
MessageId=736
SymbolicName=ERROR_ABANDONED_WAIT_63
Language=ENU
ERROR_ABANDONED_WAIT_63
.
MessageId=737
SymbolicName=ERROR_USER_APC
Language=ENU
ERROR_USER_APC
.
MessageId=738
SymbolicName=ERROR_KERNEL_APC
Language=ENU
ERROR_KERNEL_APC
.
MessageId=739
SymbolicName=ERROR_ALERTED
Language=ENU
ERROR_ALERTED
.
MessageId=740
SymbolicName=ERROR_ELEVATION_REQUIRED
Language=ENU
The requested operation requires elevation.
.
MessageId=741
SymbolicName=ERROR_REPARSE
Language=ENU
A reparse should be performed by the Object Manager since the name of the file resulted in a symbolic link.
.
MessageId=742
SymbolicName=ERROR_OPLOCK_BREAK_IN_PROGRESS
Language=ENU
An open/create operation completed while an oplock break is underway.
.
MessageId=743
SymbolicName=ERROR_VOLUME_MOUNTED
Language=ENU
A new volume has been mounted by a file system.
.
MessageId=744
SymbolicName=ERROR_RXACT_COMMITTED
Language=ENU
This success level status indicates that the transaction state already exists for the registry sub-tree, but that a transaction commit was previously aborted.
.
MessageId=745
SymbolicName=ERROR_NOTIFY_CLEANUP
Language=ENU
This indicates that a notify change request has been completed due to closing the handle which made the notify change request.
.
MessageId=746
SymbolicName=ERROR_PRIMARY_TRANSPORT_CONNECT_FAILED
Language=ENU
{Connect Failure on Primary Transport}
.
MessageId=747
SymbolicName=ERROR_PAGE_FAULT_TRANSITION
Language=ENU
Page fault was a transition fault.
.
MessageId=748
SymbolicName=ERROR_PAGE_FAULT_DEMAND_ZERO
Language=ENU
Page fault was a demand zero fault.
.
MessageId=749
SymbolicName=ERROR_PAGE_FAULT_COPY_ON_WRITE
Language=ENU
Page fault was a demand zero fault.
.
MessageId=750
SymbolicName=ERROR_PAGE_FAULT_GUARD_PAGE
Language=ENU
Page fault was a demand zero fault.
.
MessageId=751
SymbolicName=ERROR_PAGE_FAULT_PAGING_FILE
Language=ENU
Page fault was satisfied by reading from a secondary storage device.
.
MessageId=752
SymbolicName=ERROR_CACHE_PAGE_LOCKED
Language=ENU
Cached page was locked during operation.
.
MessageId=753
SymbolicName=ERROR_CRASH_DUMP
Language=ENU
Crash dump exists in paging file.
.
MessageId=754
SymbolicName=ERROR_BUFFER_ALL_ZEROS
Language=ENU
Specified buffer contains all zeros.
.
MessageId=755
SymbolicName=ERROR_REPARSE_OBJECT
Language=ENU
A reparse should be performed by the Object Manager since the name of the file resulted in a symbolic link.
.
MessageId=756
SymbolicName=ERROR_RESOURCE_REQUIREMENTS_CHANGED
Language=ENU
The device has succeeded a query-stop and its resource requirements have changed.
.
MessageId=757
SymbolicName=ERROR_TRANSLATION_COMPLETE
Language=ENU
The translator has translated these resources into the global space and no further translations should be performed.
.
MessageId=758
SymbolicName=ERROR_NOTHING_TO_TERMINATE
Language=ENU
A process being terminated has no threads to terminate.
.
MessageId=759
SymbolicName=ERROR_PROCESS_NOT_IN_JOB
Language=ENU
The specified process is not part of a job.
.
MessageId=760
SymbolicName=ERROR_PROCESS_IN_JOB
Language=ENU
The specified process is part of a job.
.
MessageId=761
SymbolicName=ERROR_VOLSNAP_HIBERNATE_READY
Language=ENU
{Volume Shadow Copy Service}
.
MessageId=762
SymbolicName=ERROR_FSFILTER_OP_COMPLETED_SUCCESSFULLY
Language=ENU
A file system or file system filter driver has successfully completed an FsFilter operation.
.
MessageId=763
SymbolicName=ERROR_INTERRUPT_VECTOR_ALREADY_CONNECTED
Language=ENU
The specified interrupt vector was already connected.
.
MessageId=764
SymbolicName=ERROR_INTERRUPT_STILL_CONNECTED
Language=ENU
The specified interrupt vector is still connected.
.
MessageId=765
SymbolicName=ERROR_WAIT_FOR_OPLOCK
Language=ENU
An operation is blocked waiting for an oplock.
.
MessageId=766
SymbolicName=ERROR_DBG_EXCEPTION_HANDLED
Language=ENU
Debugger handled exception
.
MessageId=767
SymbolicName=ERROR_DBG_CONTINUE
Language=ENU
Debugger continued
.
MessageId=768
SymbolicName=ERROR_CALLBACK_POP_STACK
Language=ENU
An exception occurred in a user mode callback and the kernel callback frame should be removed.
.
MessageId=769
SymbolicName=ERROR_COMPRESSION_DISABLED
Language=ENU
Compression is disabled for this volume.
.
MessageId=770
SymbolicName=ERROR_CANTFETCHBACKWARDS
Language=ENU
The data provider cannot fetch backwards through a result set.
.
MessageId=771
SymbolicName=ERROR_CANTSCROLLBACKWARDS
Language=ENU
The data provider cannot scroll backwards through a result set.
.
MessageId=772
SymbolicName=ERROR_ROWSNOTRELEASED
Language=ENU
The data provider requires that previously fetched data is released before asking for more data.
.
MessageId=773
SymbolicName=ERROR_BAD_ACCESSOR_FLAGS
Language=ENU
The data provider was not able to interpret the flags set for a column binding in an accessor.
.
MessageId=774
SymbolicName=ERROR_ERRORS_ENCOUNTERED
Language=ENU
One or more errors occurred while processing the request.
.
MessageId=775
SymbolicName=ERROR_NOT_CAPABLE
Language=ENU
The implementation is not capable of performing the request.
.
MessageId=776
SymbolicName=ERROR_REQUEST_OUT_OF_SEQUENCE
Language=ENU
The client of a component requested an operation which is not valid given the state of the component instance.
.
MessageId=777
SymbolicName=ERROR_VERSION_PARSE_ERROR
Language=ENU
A version number could not be parsed.
.
MessageId=778
SymbolicName=ERROR_BADSTARTPOSITION
Language=ENU
The iterator's start position is invalid.
.
MessageId=779
SymbolicName=ERROR_MEMORY_HARDWARE
Language=ENU
The hardware has reported an uncorrectable memory error.
.
MessageId=780
SymbolicName=ERROR_DISK_REPAIR_DISABLED
Language=ENU
The attempted operation required self healing to be enabled.
.
MessageId=781
SymbolicName=ERROR_INSUFFICIENT_RESOURCE_FOR_SPECIFIED_SHARED_SECTION_SIZE
Language=ENU
The Desktop heap encountered an error while allocating session memory. There is more information in the system event log.
.
MessageId=782
SymbolicName=ERROR_SYSTEM_POWERSTATE_TRANSITION
Language=ENU
The system power state is transitioning from %2 to %3.
.
MessageId=783
SymbolicName=ERROR_SYSTEM_POWERSTATE_COMPLEX_TRANSITION
Language=ENU
The system power state is transitioning from %2 to %3 but could enter %4.
.
MessageId=784
SymbolicName=ERROR_MCA_EXCEPTION
Language=ENU
A thread is getting dispatched with MCA EXCEPTION because of MCA.
.
MessageId=785
SymbolicName=ERROR_ACCESS_AUDIT_BY_POLICY
Language=ENU
Access to %1 is monitored by policy rule %2.
.
MessageId=786
SymbolicName=ERROR_ACCESS_DISABLED_NO_SAFER_UI_BY_POLICY
Language=ENU
Access to %1 has been restricted by your Administrator by policy rule %2.
.
MessageId=787
SymbolicName=ERROR_ABANDON_HIBERFILE
Language=ENU
A valid hibernation file has been invalidated and should be abandoned.
.
MessageId=788
SymbolicName=ERROR_LOST_WRITEBEHIND_DATA_NETWORK_DISCONNECTED
Language=ENU
{Delayed Write Failed}
.
MessageId=789
SymbolicName=ERROR_LOST_WRITEBEHIND_DATA_NETWORK_SERVER_ERROR
Language=ENU
{Delayed Write Failed}
.
MessageId=790
SymbolicName=ERROR_LOST_WRITEBEHIND_DATA_LOCAL_DISK_ERROR
Language=ENU
{Delayed Write Failed}
.
MessageId=791
SymbolicName=ERROR_BAD_MCFG_TABLE
Language=ENU
The resources required for this device conflict with the MCFG table.
.
MessageId=792
SymbolicName=ERROR_DISK_REPAIR_REDIRECTED
Language=ENU
The volume repair could not be performed while it is online.
.
MessageId=793
SymbolicName=ERROR_DISK_REPAIR_UNSUCCESSFUL
Language=ENU
The volume repair was not successful.
.
MessageId=794
SymbolicName=ERROR_CORRUPT_LOG_OVERFULL
Language=ENU
One of the volume corruption logs is full. Further corruptions that may be detected won't be logged.
.
MessageId=795
SymbolicName=ERROR_CORRUPT_LOG_CORRUPTED
Language=ENU
One of the volume corruption logs is internally corrupted and needs to be recreated. The volume may contain undetected corruptions and must be scanned.
.
MessageId=796
SymbolicName=ERROR_CORRUPT_LOG_UNAVAILABLE
Language=ENU
One of the volume corruption logs is unavailable for being operated on.
.
MessageId=797
SymbolicName=ERROR_CORRUPT_LOG_DELETED_FULL
Language=ENU
One of the volume corruption logs was deleted while still having corruption records in them. The volume contains detected corruptions and must be scanned.
.
MessageId=798
SymbolicName=ERROR_CORRUPT_LOG_CLEARED
Language=ENU
One of the volume corruption logs was cleared by chkdsk and no longer contains real corruptions.
.
MessageId=799
SymbolicName=ERROR_ORPHAN_NAME_EXHAUSTED
Language=ENU
Orphaned files exist on the volume but could not be recovered because no more new names could be created in the recovery directory. Files must be moved from the recovery directory.
.
MessageId=800
SymbolicName=ERROR_OPLOCK_SWITCHED_TO_NEW_HANDLE
Language=ENU
The oplock that was associated with this handle is now associated with a different handle.
.
MessageId=801
SymbolicName=ERROR_CANNOT_GRANT_REQUESTED_OPLOCK
Language=ENU
An oplock of the requested level cannot be granted.  An oplock of a lower level may be available.
.
MessageId=802
SymbolicName=ERROR_CANNOT_BREAK_OPLOCK
Language=ENU
The operation did not complete successfully because it would cause an oplock to be broken. The caller has requested that existing oplocks not be broken.
.
MessageId=803
SymbolicName=ERROR_OPLOCK_HANDLE_CLOSED
Language=ENU
The handle with which this oplock was associated has been closed.  The oplock is now broken.
.
MessageId=804
SymbolicName=ERROR_NO_ACE_CONDITION
Language=ENU
The specified access control entry (ACE) does not contain a condition.
.
MessageId=805
SymbolicName=ERROR_INVALID_ACE_CONDITION
Language=ENU
The specified access control entry (ACE) contains an invalid condition.
.
MessageId=806
SymbolicName=ERROR_FILE_HANDLE_REVOKED
Language=ENU
Access to the specified file handle has been revoked.
.
MessageId=807
SymbolicName=ERROR_IMAGE_AT_DIFFERENT_BASE
Language=ENU
{Image Relocated}
.
MessageId=808
SymbolicName=ERROR_ENCRYPTED_IO_NOT_POSSIBLE
Language=ENU
The read or write operation to an encrypted file could not be completed because the file has not been opened for data access.
.
MessageId=809
SymbolicName=ERROR_FILE_METADATA_OPTIMIZATION_IN_PROGRESS
Language=ENU
File metadata optimization is already in progress.
.
MessageId=810
SymbolicName=ERROR_QUOTA_ACTIVITY
Language=ENU
The requested operation failed due to quota operation is still in progress.
.
MessageId=811
SymbolicName=ERROR_HANDLE_REVOKED
Language=ENU
Access to the specified handle has been revoked.
.
MessageId=812
SymbolicName=ERROR_CALLBACK_INVOKE_INLINE
Language=ENU
The callback function must be invoked inline.
.
MessageId=813
SymbolicName=ERROR_CPU_SET_INVALID
Language=ENU
The specified CPU Set IDs are invalid.
.
MessageId=814
SymbolicName=ERROR_ENCLAVE_NOT_TERMINATED
Language=ENU
The specified enclave has not yet been terminated.
.
MessageId=815
SymbolicName=ERROR_ENCLAVE_VIOLATION
Language=ENU
An attempt was made to access protected memory in violation of its secure access policy.
.
MessageId=816
SymbolicName=ERROR_SERVER_TRANSPORT_CONFLICT
Language=ENU
Multiple mappings to shared resource(s) on a server, using more than one transport, are not allowed. Use a single transport for all mappings to a server and try again.
.
MessageId=817
SymbolicName=ERROR_CERTIFICATE_VALIDATION_PREFERENCE_CONFLICT
Language=ENU
Multiple mappings to shared resource(s) on a server, using different certificate validation preferences, are not allowed. Use the same preference for all mappings to a server and try again.
.
MessageId=818
SymbolicName=ERROR_FT_READ_FROM_COPY_FAILURE
Language=ENU
The specified copy of the requested data could not be read.
.
MessageId=819
SymbolicName=ERROR_SECTION_DIRECT_MAP_ONLY
Language=ENU
The section creation request was failed because it would have been satisfied with a direct map and the caller explicitly signified this was not wanted.
.
MessageId=994
SymbolicName=ERROR_EA_ACCESS_DENIED
Language=ENU
Access to the extended attribute was denied.
.
MessageId=995
SymbolicName=ERROR_OPERATION_ABORTED
Language=ENU
The I/O operation has been aborted because of either a thread exit or an application request.
.
MessageId=996
SymbolicName=ERROR_IO_INCOMPLETE
Language=ENU
Overlapped I/O event is not in a signaled state.
.
MessageId=997
SymbolicName=ERROR_IO_PENDING
Language=ENU
Overlapped I/O operation is in progress.
.
MessageId=998
SymbolicName=ERROR_NOACCESS
Language=ENU
Invalid access to memory location.
.
MessageId=999
SymbolicName=ERROR_SWAPERROR
Language=ENU
Error performing inpage operation.
.
MessageId=1001
SymbolicName=ERROR_STACK_OVERFLOW
Language=ENU
Recursion too deep; the stack overflowed.
.
MessageId=1002
SymbolicName=ERROR_INVALID_MESSAGE
Language=ENU
The window cannot act on the sent message.
.
MessageId=1003
SymbolicName=ERROR_CAN_NOT_COMPLETE
Language=ENU
Cannot complete this function.
.
MessageId=1004
SymbolicName=ERROR_INVALID_FLAGS
Language=ENU
Invalid flags.
.
MessageId=1005
SymbolicName=ERROR_UNRECOGNIZED_VOLUME
Language=ENU
The volume does not contain a recognized file system.
.
MessageId=1006
SymbolicName=ERROR_FILE_INVALID
Language=ENU
The volume for a file has been externally altered so that the opened file is no longer valid.
.
MessageId=1007
SymbolicName=ERROR_FULLSCREEN_MODE
Language=ENU
The requested operation cannot be performed in full-screen mode.
.
MessageId=1008
SymbolicName=ERROR_NO_TOKEN
Language=ENU
An attempt was made to reference a token that does not exist.
.
MessageId=1009
SymbolicName=ERROR_BADDB
Language=ENU
The configuration registry database is corrupt.
.
MessageId=1010
SymbolicName=ERROR_BADKEY
Language=ENU
The configuration registry key is invalid.
.
MessageId=1011
SymbolicName=ERROR_CANTOPEN
Language=ENU
The configuration registry key could not be opened.
.
MessageId=1012
SymbolicName=ERROR_CANTREAD
Language=ENU
The configuration registry key could not be read.
.
MessageId=1013
SymbolicName=ERROR_CANTWRITE
Language=ENU
The configuration registry key could not be written.
.
MessageId=1014
SymbolicName=ERROR_REGISTRY_RECOVERED
Language=ENU
One of the files in the registry database had to be recovered by use of a log or alternate copy. The recovery was successful.
.
MessageId=1015
SymbolicName=ERROR_REGISTRY_CORRUPT
Language=ENU
The registry is corrupted. The structure of one of the files containing registry data is corrupted, or the system's memory image of the file is corrupted, or the file could not be recovered because the alternate copy or log was absent or corrupted.
.
MessageId=1016
SymbolicName=ERROR_REGISTRY_IO_FAILED
Language=ENU
An I/O operation initiated by the registry failed unrecoverably. The registry could not read in, or write out, or flush, one of the files that contain the system's image of the registry.
.
MessageId=1017
SymbolicName=ERROR_NOT_REGISTRY_FILE
Language=ENU
The system has attempted to load or restore a file into the registry, but the specified file is not in a registry file format.
.
MessageId=1018
SymbolicName=ERROR_KEY_DELETED
Language=ENU
Illegal operation attempted on a registry key that has been marked for deletion.
.
MessageId=1019
SymbolicName=ERROR_NO_LOG_SPACE
Language=ENU
System could not allocate the required space in a registry log.
.
MessageId=1020
SymbolicName=ERROR_KEY_HAS_CHILDREN
Language=ENU
Cannot create a symbolic link in a registry key that already has subkeys or values.
.
MessageId=1021
SymbolicName=ERROR_CHILD_MUST_BE_VOLATILE
Language=ENU
Cannot create a stable subkey under a volatile parent key.
.
MessageId=1022
SymbolicName=ERROR_NOTIFY_ENUM_DIR
Language=ENU
A notify change request is being completed and the information is not being returned in the caller's buffer. The caller now needs to enumerate the files to find the changes.
.
MessageId=1051
SymbolicName=ERROR_DEPENDENT_SERVICES_RUNNING
Language=ENU
A stop control has been sent to a service that other running services are dependent on.
.
MessageId=1052
SymbolicName=ERROR_INVALID_SERVICE_CONTROL
Language=ENU
The requested control is not valid for this service.
.
MessageId=1053
SymbolicName=ERROR_SERVICE_REQUEST_TIMEOUT
Language=ENU
The service did not respond to the start or control request in a timely fashion.
.
MessageId=1054
SymbolicName=ERROR_SERVICE_NO_THREAD
Language=ENU
A thread could not be created for the service.
.
MessageId=1055
SymbolicName=ERROR_SERVICE_DATABASE_LOCKED
Language=ENU
The service database is locked.
.
MessageId=1056
SymbolicName=ERROR_SERVICE_ALREADY_RUNNING
Language=ENU
An instance of the service is already running.
.
MessageId=1057
SymbolicName=ERROR_INVALID_SERVICE_ACCOUNT
Language=ENU
The account name is invalid or does not exist, or the password is invalid for the account name specified.
.
MessageId=1058
SymbolicName=ERROR_SERVICE_DISABLED
Language=ENU
The service cannot be started, either because it is disabled or because it has no enabled devices associated with it.
.
MessageId=1059
SymbolicName=ERROR_CIRCULAR_DEPENDENCY
Language=ENU
Circular service dependency was specified.
.
MessageId=1060
SymbolicName=ERROR_SERVICE_DOES_NOT_EXIST
Language=ENU
The specified service does not exist as an installed service.
.
MessageId=1061
SymbolicName=ERROR_SERVICE_CANNOT_ACCEPT_CTRL
Language=ENU
The service cannot accept control messages at this time.
.
MessageId=1062
SymbolicName=ERROR_SERVICE_NOT_ACTIVE
Language=ENU
The service has not been started.
.
MessageId=1063
SymbolicName=ERROR_FAILED_SERVICE_CONTROLLER_CONNECT
Language=ENU
The service process could not connect to the service controller.
.
MessageId=1064
SymbolicName=ERROR_EXCEPTION_IN_SERVICE
Language=ENU
An exception occurred in the service when handling the control request.
.
MessageId=1065
SymbolicName=ERROR_DATABASE_DOES_NOT_EXIST
Language=ENU
The database specified does not exist.
.
MessageId=1066
SymbolicName=ERROR_SERVICE_SPECIFIC_ERROR
Language=ENU
The service has returned a service-specific error code.
.
MessageId=1067
SymbolicName=ERROR_PROCESS_ABORTED
Language=ENU
The process terminated unexpectedly.
.
MessageId=1068
SymbolicName=ERROR_SERVICE_DEPENDENCY_FAIL
Language=ENU
The dependency service or group failed to start.
.
MessageId=1069
SymbolicName=ERROR_SERVICE_LOGON_FAILED
Language=ENU
The service did not start due to a logon failure.
.
MessageId=1070
SymbolicName=ERROR_SERVICE_START_HANG
Language=ENU
After starting, the service hung in a start-pending state.
.
MessageId=1071
SymbolicName=ERROR_INVALID_SERVICE_LOCK
Language=ENU
The specified service database lock is invalid.
.
MessageId=1072
SymbolicName=ERROR_SERVICE_MARKED_FOR_DELETE
Language=ENU
The specified service has been marked for deletion.
.
MessageId=1073
SymbolicName=ERROR_SERVICE_EXISTS
Language=ENU
The specified service already exists.
.
MessageId=1074
SymbolicName=ERROR_ALREADY_RUNNING_LKG
Language=ENU
The system is currently running with the last-known-good configuration.
.
MessageId=1075
SymbolicName=ERROR_SERVICE_DEPENDENCY_DELETED
Language=ENU
The dependency service does not exist or has been marked for deletion.
.
MessageId=1076
SymbolicName=ERROR_BOOT_ALREADY_ACCEPTED
Language=ENU
The current boot has already been accepted for use as the last-known-good control set.
.
MessageId=1077
SymbolicName=ERROR_SERVICE_NEVER_STARTED
Language=ENU
No attempts to start the service have been made since the last boot.
.
MessageId=1078
SymbolicName=ERROR_DUPLICATE_SERVICE_NAME
Language=ENU
The name is already in use as either a service name or a service display name.
.
MessageId=1079
SymbolicName=ERROR_DIFFERENT_SERVICE_ACCOUNT
Language=ENU
The account specified for this service is different from the account specified for other services running in the same process.
.
MessageId=1080
SymbolicName=ERROR_CANNOT_DETECT_DRIVER_FAILURE
Language=ENU
Failure actions can only be set for Win32 services, not for drivers.
.
MessageId=1081
SymbolicName=ERROR_CANNOT_DETECT_PROCESS_ABORT
Language=ENU
This service runs in the same process as the service control manager.
.
MessageId=1082
SymbolicName=ERROR_NO_RECOVERY_PROGRAM
Language=ENU
No recovery program has been configured for this service.
.
MessageId=1083
SymbolicName=ERROR_SERVICE_NOT_IN_EXE
Language=ENU
The executable program that this service is configured to run in does not implement the service.
.
MessageId=1084
SymbolicName=ERROR_NOT_SAFEBOOT_SERVICE
Language=ENU
This service cannot be started in Safe Mode
.
MessageId=1100
SymbolicName=ERROR_END_OF_MEDIA
Language=ENU
The physical end of the tape has been reached.
.
MessageId=1101
SymbolicName=ERROR_FILEMARK_DETECTED
Language=ENU
A tape access reached a filemark.
.
MessageId=1102
SymbolicName=ERROR_BEGINNING_OF_MEDIA
Language=ENU
The beginning of the tape or a partition was encountered.
.
MessageId=1103
SymbolicName=ERROR_SETMARK_DETECTED
Language=ENU
A tape access reached the end of a set of files.
.
MessageId=1104
SymbolicName=ERROR_NO_DATA_DETECTED
Language=ENU
No more data is on the tape.
.
MessageId=1105
SymbolicName=ERROR_PARTITION_FAILURE
Language=ENU
Tape could not be partitioned.
.
MessageId=1106
SymbolicName=ERROR_INVALID_BLOCK_LENGTH
Language=ENU
When accessing a new tape of a multivolume partition, the current block size is incorrect.
.
MessageId=1107
SymbolicName=ERROR_DEVICE_NOT_PARTITIONED
Language=ENU
Tape partition information could not be found when loading a tape.
.
MessageId=1108
SymbolicName=ERROR_UNABLE_TO_LOCK_MEDIA
Language=ENU
Unable to lock the media eject mechanism.
.
MessageId=1109
SymbolicName=ERROR_UNABLE_TO_UNLOAD_MEDIA
Language=ENU
Unable to unload the media.
.
MessageId=1110
SymbolicName=ERROR_MEDIA_CHANGED
Language=ENU
The media in the drive may have changed.
.
MessageId=1111
SymbolicName=ERROR_BUS_RESET
Language=ENU
The I/O bus was reset.
.
MessageId=1112
SymbolicName=ERROR_NO_MEDIA_IN_DRIVE
Language=ENU
No media in drive.
.
MessageId=1113
SymbolicName=ERROR_NO_UNICODE_TRANSLATION
Language=ENU
No mapping for the Unicode character exists in the target multi-byte code page.
.
MessageId=1114
SymbolicName=ERROR_DLL_INIT_FAILED
Language=ENU
A dynamic link library (DLL) initialization routine failed.
.
MessageId=1115
SymbolicName=ERROR_SHUTDOWN_IN_PROGRESS
Language=ENU
A system shutdown is in progress.
.
MessageId=1116
SymbolicName=ERROR_NO_SHUTDOWN_IN_PROGRESS
Language=ENU
Unable to abort the system shutdown because no shutdown was in progress.
.
MessageId=1117
SymbolicName=ERROR_IO_DEVICE
Language=ENU
The request could not be performed because of an I/O device error.
.
MessageId=1118
SymbolicName=ERROR_SERIAL_NO_DEVICE
Language=ENU
No serial device was successfully initialized. The serial driver will unload.
.
MessageId=1119
SymbolicName=ERROR_IRQ_BUSY
Language=ENU
Unable to open a device that was sharing an interrupt request (IRQ) with other devices. At least one other device that uses that IRQ was already opened.
.
MessageId=1120
SymbolicName=ERROR_MORE_WRITES
Language=ENU
A serial I/O operation was completed by another write to the serial port.
.
MessageId=1121
SymbolicName=ERROR_COUNTER_TIMEOUT
Language=ENU
A serial I/O operation completed because the timeout period expired.
.
MessageId=1122
SymbolicName=ERROR_FLOPPY_ID_MARK_NOT_FOUND
Language=ENU
No ID address mark was found on the floppy disk.
.
MessageId=1123
SymbolicName=ERROR_FLOPPY_WRONG_CYLINDER
Language=ENU
Mismatch between the floppy disk sector ID field and the floppy disk controller track address.
.
MessageId=1124
SymbolicName=ERROR_FLOPPY_UNKNOWN_ERROR
Language=ENU
The floppy disk controller reported an error that is not recognized by the floppy disk driver.
.
MessageId=1125
SymbolicName=ERROR_FLOPPY_BAD_REGISTERS
Language=ENU
The floppy disk controller returned inconsistent results in its registers.
.
MessageId=1126
SymbolicName=ERROR_DISK_RECALIBRATE_FAILED
Language=ENU
While accessing the hard disk, a recalibrate operation failed, even after retries.
.
MessageId=1127
SymbolicName=ERROR_DISK_OPERATION_FAILED
Language=ENU
While accessing the hard disk, a disk operation failed even after retries.
.
MessageId=1128
SymbolicName=ERROR_DISK_RESET_FAILED
Language=ENU
While accessing the hard disk, a disk controller reset was needed, but even that failed.
.
MessageId=1129
SymbolicName=ERROR_EOM_OVERFLOW
Language=ENU
Physical end of tape encountered.
.
MessageId=1130
SymbolicName=ERROR_NOT_ENOUGH_SERVER_MEMORY
Language=ENU
Not enough server memory resources are available to process this command.
.
MessageId=1131
SymbolicName=ERROR_POSSIBLE_DEADLOCK
Language=ENU
A potential deadlock condition has been detected.
.
MessageId=1132
SymbolicName=ERROR_MAPPED_ALIGNMENT
Language=ENU
The base address or the file offset specified does not have the proper alignment.
.
MessageId=1140
SymbolicName=ERROR_SET_POWER_STATE_VETOED
Language=ENU
An attempt to change the system power state was vetoed by another application or driver.
.
MessageId=1141
SymbolicName=ERROR_SET_POWER_STATE_FAILED
Language=ENU
The system BIOS failed an attempt to change the system power state.
.
MessageId=1142
SymbolicName=ERROR_TOO_MANY_LINKS
Language=ENU
An attempt was made to create more links on a file than the file system supports.
.
MessageId=1150
SymbolicName=ERROR_OLD_WIN_VERSION
Language=ENU
The specified program requires a newer version of Windows.
.
MessageId=1151
SymbolicName=ERROR_APP_WRONG_OS
Language=ENU
The specified program is not a Windows or MS-DOS program.
.
MessageId=1152
SymbolicName=ERROR_SINGLE_INSTANCE_APP
Language=ENU
Cannot start more than one instance of the specified program.
.
MessageId=1153
SymbolicName=ERROR_RMODE_APP
Language=ENU
The specified program was written for an earlier version of Windows.
.
MessageId=1154
SymbolicName=ERROR_INVALID_DLL
Language=ENU
One of the library files needed to run this application is damaged.
.
MessageId=1155
SymbolicName=ERROR_NO_ASSOCIATION
Language=ENU
No application is associated with the specified file for this operation.
.
MessageId=1156
SymbolicName=ERROR_DDE_FAIL
Language=ENU
An error occurred in sending the command to the application.
.
MessageId=1157
SymbolicName=ERROR_DLL_NOT_FOUND
Language=ENU
One of the library files needed to run this application cannot be found.
.
MessageId=1158
SymbolicName=ERROR_NO_MORE_USER_HANDLES
Language=ENU
The current process has used all of its system allowance of handles for Window Manager objects.
.
MessageId=1159
SymbolicName=ERROR_MESSAGE_SYNC_ONLY
Language=ENU
The message can be used only with synchronous operations.
.
MessageId=1160
SymbolicName=ERROR_SOURCE_ELEMENT_EMPTY
Language=ENU
The indicated source element has no media.
.
MessageId=1161
SymbolicName=ERROR_DESTINATION_ELEMENT_FULL
Language=ENU
The indicated destination element already contains media.
.
MessageId=1162
SymbolicName=ERROR_ILLEGAL_ELEMENT_ADDRESS
Language=ENU
The indicated element does not exist.
.
MessageId=1163
SymbolicName=ERROR_MAGAZINE_NOT_PRESENT
Language=ENU
The indicated element is part of a magazine that is not present.
.
MessageId=1164
SymbolicName=ERROR_DEVICE_REINITIALIZATION_NEEDED
Language=ENU
The indicated device requires reinitialization due to hardware errors.
.
MessageId=1165
SymbolicName=ERROR_DEVICE_REQUIRES_CLEANING
Language=ENU
The device has indicated that cleaning is required before further operations are attempted.
.
MessageId=1166
SymbolicName=ERROR_DEVICE_DOOR_OPEN
Language=ENU
The device has indicated that its door is open.
.
MessageId=1167
SymbolicName=ERROR_DEVICE_NOT_CONNECTED
Language=ENU
The device is not connected.
.
MessageId=1168
SymbolicName=ERROR_NOT_FOUND
Language=ENU
Element not found.
.
MessageId=1169
SymbolicName=ERROR_NO_MATCH
Language=ENU
There was no match for the specified key in the index.
.
MessageId=1170
SymbolicName=ERROR_SET_NOT_FOUND
Language=ENU
The property set specified does not exist on the object.
.
MessageId=1171
SymbolicName=ERROR_POINT_NOT_FOUND
Language=ENU
The point passed to GetMouseMovePoints is not in the buffer.
.
MessageId=1172
SymbolicName=ERROR_NO_TRACKING_SERVICE
Language=ENU
The tracking (workstation) service is not running.
.
MessageId=1173
SymbolicName=ERROR_NO_VOLUME_ID
Language=ENU
The Volume ID could not be found.
.
MessageId=1175
SymbolicName=ERROR_UNABLE_TO_REMOVE_REPLACED
Language=ENU
Unable to remove the file to be replaced.
.
MessageId=1176
SymbolicName=ERROR_UNABLE_TO_MOVE_REPLACEMENT
Language=ENU
Unable to move the replacement file to the file to be replaced. The file to be replaced has retained its original name.
.
MessageId=1177
SymbolicName=ERROR_UNABLE_TO_MOVE_REPLACEMENT_2
Language=ENU
Unable to move the replacement file to the file to be replaced. The file to be replaced has been renamed using the backup name.
.
MessageId=1178
SymbolicName=ERROR_JOURNAL_DELETE_IN_PROGRESS
Language=ENU
The volume change journal is being deleted.
.
MessageId=1179
SymbolicName=ERROR_JOURNAL_NOT_ACTIVE
Language=ENU
The volume change journal is not active.
.
MessageId=1180
SymbolicName=ERROR_POTENTIAL_FILE_FOUND
Language=ENU
A file was found, but it may not be the correct file.
.
MessageId=1181
SymbolicName=ERROR_JOURNAL_ENTRY_DELETED
Language=ENU
The journal entry has been deleted from the journal.
.
MessageId=1184
SymbolicName=ERROR_PARTITION_TERMINATING
Language=ENU
An attempt was made to access a partition that has begun termination.
.
MessageId=1190
SymbolicName=ERROR_SHUTDOWN_IS_SCHEDULED
Language=ENU
A system shutdown has already been scheduled.
.
MessageId=1191
SymbolicName=ERROR_SHUTDOWN_USERS_LOGGED_ON
Language=ENU
The system shutdown cannot be initiated because there are other users logged on to the computer.
.
MessageId=1192
SymbolicName=ERROR_SHUTDOWN_DISKS_NOT_IN_MAINTENANCE_MODE
Language=ENU
The system shutdown cannot safely proceed without enabling node maintenance mode for cluster node and waiting for the drain to complete.
.
MessageId=1200
SymbolicName=ERROR_BAD_DEVICE
Language=ENU
The specified device name is invalid.
.
MessageId=1201
SymbolicName=ERROR_CONNECTION_UNAVAIL
Language=ENU
The device is not currently connected but it is a remembered connection.
.
MessageId=1202
SymbolicName=ERROR_DEVICE_ALREADY_REMEMBERED
Language=ENU
The local device name has a remembered connection to another network resource.
.
MessageId=1203
SymbolicName=ERROR_NO_NET_OR_BAD_PATH
Language=ENU
The network path was either typed incorrectly, does not exist, or the network provider is not currently available. Please try retyping the path or contact your network administrator.
.
MessageId=1204
SymbolicName=ERROR_BAD_PROVIDER
Language=ENU
The specified network provider name is invalid.
.
MessageId=1205
SymbolicName=ERROR_CANNOT_OPEN_PROFILE
Language=ENU
Unable to open the network connection profile.
.
MessageId=1206
SymbolicName=ERROR_BAD_PROFILE
Language=ENU
The network connection profile is corrupted.
.
MessageId=1207
SymbolicName=ERROR_NOT_CONTAINER
Language=ENU
Cannot enumerate a noncontainer.
.
MessageId=1208
SymbolicName=ERROR_EXTENDED_ERROR
Language=ENU
An extended error has occurred.
.
MessageId=1209
SymbolicName=ERROR_INVALID_GROUPNAME
Language=ENU
The format of the specified group name is invalid.
.
MessageId=1210
SymbolicName=ERROR_INVALID_COMPUTERNAME
Language=ENU
The format of the specified computer name is invalid.
.
MessageId=1211
SymbolicName=ERROR_INVALID_EVENTNAME
Language=ENU
The format of the specified event name is invalid.
.
MessageId=1212
SymbolicName=ERROR_INVALID_DOMAINNAME
Language=ENU
The format of the specified domain name is invalid.
.
MessageId=1213
SymbolicName=ERROR_INVALID_SERVICENAME
Language=ENU
The format of the specified service name is invalid.
.
MessageId=1214
SymbolicName=ERROR_INVALID_NETNAME
Language=ENU
The format of the specified network name is invalid.
.
MessageId=1215
SymbolicName=ERROR_INVALID_SHARENAME
Language=ENU
The format of the specified share name is invalid.
.
MessageId=1216
SymbolicName=ERROR_INVALID_PASSWORDNAME
Language=ENU
The format of the specified password is invalid.
.
MessageId=1217
SymbolicName=ERROR_INVALID_MESSAGENAME
Language=ENU
The format of the specified message name is invalid.
.
MessageId=1218
SymbolicName=ERROR_INVALID_MESSAGEDEST
Language=ENU
The format of the specified message destination is invalid.
.
MessageId=1219
SymbolicName=ERROR_SESSION_CREDENTIAL_CONFLICT
Language=ENU
Multiple connections to a server or shared resource by the same user, using more than one user name, are not allowed. Disconnect all previous connections to the server or shared resource and try again.
.
MessageId=1220
SymbolicName=ERROR_REMOTE_SESSION_LIMIT_EXCEEDED
Language=ENU
An attempt was made to establish a session to a network server, but there are already too many sessions established to that server.
.
MessageId=1221
SymbolicName=ERROR_DUP_DOMAINNAME
Language=ENU
The workgroup or domain name is already in use by another computer on the network.
.
MessageId=1222
SymbolicName=ERROR_NO_NETWORK
Language=ENU
The network is not present or not started.
.
MessageId=1223
SymbolicName=ERROR_CANCELLED
Language=ENU
The operation was canceled by the user.
.
MessageId=1224
SymbolicName=ERROR_USER_MAPPED_FILE
Language=ENU
The requested operation cannot be performed on a file with a user-mapped section open.
.
MessageId=1225
SymbolicName=ERROR_CONNECTION_REFUSED
Language=ENU
The remote computer refused the network connection.
.
MessageId=1226
SymbolicName=ERROR_GRACEFUL_DISCONNECT
Language=ENU
The network connection was gracefully closed.
.
MessageId=1227
SymbolicName=ERROR_ADDRESS_ALREADY_ASSOCIATED
Language=ENU
The network transport endpoint already has an address associated with it.
.
MessageId=1228
SymbolicName=ERROR_ADDRESS_NOT_ASSOCIATED
Language=ENU
An address has not yet been associated with the network endpoint.
.
MessageId=1229
SymbolicName=ERROR_CONNECTION_INVALID
Language=ENU
An operation was attempted on a nonexistent network connection.
.
MessageId=1230
SymbolicName=ERROR_CONNECTION_ACTIVE
Language=ENU
An invalid operation was attempted on an active network connection.
.
MessageId=1231
SymbolicName=ERROR_NETWORK_UNREACHABLE
Language=ENU
The network location cannot be reached. For information about network troubleshooting, see Windows Help.
.
MessageId=1232
SymbolicName=ERROR_HOST_UNREACHABLE
Language=ENU
The network location cannot be reached. For information about network troubleshooting, see Windows Help.
.
MessageId=1233
SymbolicName=ERROR_PROTOCOL_UNREACHABLE
Language=ENU
The network location cannot be reached. For information about network troubleshooting, see Windows Help.
.
MessageId=1234
SymbolicName=ERROR_PORT_UNREACHABLE
Language=ENU
No service is operating at the destination network endpoint on the remote system.
.
MessageId=1235
SymbolicName=ERROR_REQUEST_ABORTED
Language=ENU
The request was aborted.
.
MessageId=1236
SymbolicName=ERROR_CONNECTION_ABORTED
Language=ENU
The network connection was aborted by the local system.
.
MessageId=1237
SymbolicName=ERROR_RETRY
Language=ENU
The operation could not be completed. A retry should be performed.
.
MessageId=1238
SymbolicName=ERROR_CONNECTION_COUNT_LIMIT
Language=ENU
A connection to the server could not be made because the limit on the number of concurrent connections for this account has been reached.
.
MessageId=1239
SymbolicName=ERROR_LOGIN_TIME_RESTRICTION
Language=ENU
Attempting to log in during an unauthorized time of day for this account.
.
MessageId=1240
SymbolicName=ERROR_LOGIN_WKSTA_RESTRICTION
Language=ENU
The account is not authorized to log in from this station.
.
MessageId=1241
SymbolicName=ERROR_INCORRECT_ADDRESS
Language=ENU
The network address could not be used for the operation requested.
.
MessageId=1242
SymbolicName=ERROR_ALREADY_REGISTERED
Language=ENU
The service is already registered.
.
MessageId=1243
SymbolicName=ERROR_SERVICE_NOT_FOUND
Language=ENU
The specified service does not exist.
.
MessageId=1244
SymbolicName=ERROR_NOT_AUTHENTICATED
Language=ENU
The operation being requested was not performed because the user has not been authenticated.
.
MessageId=1245
SymbolicName=ERROR_NOT_LOGGED_ON
Language=ENU
The operation being requested was not performed because the user has not logged on to the network. The specified service does not exist.
.
MessageId=1246
SymbolicName=ERROR_CONTINUE
Language=ENU
Continue with work in progress.
.
MessageId=1247
SymbolicName=ERROR_ALREADY_INITIALIZED
Language=ENU
An attempt was made to perform an initialization operation when initialization has already been completed.
.
MessageId=1248
SymbolicName=ERROR_NO_MORE_DEVICES
Language=ENU
No more local devices.
.
MessageId=1249
SymbolicName=ERROR_NO_SUCH_SITE
Language=ENU
The specified site does not exist.
.
MessageId=1250
SymbolicName=ERROR_DOMAIN_CONTROLLER_EXISTS
Language=ENU
A domain controller with the specified name already exists.
.
MessageId=1251
SymbolicName=ERROR_ONLY_IF_CONNECTED
Language=ENU
This operation is supported only when you are connected to the server.
.
MessageId=1252
SymbolicName=ERROR_OVERRIDE_NOCHANGES
Language=ENU
The group policy framework should call the extension even if there are no changes.
.
MessageId=1253
SymbolicName=ERROR_BAD_USER_PROFILE
Language=ENU
The specified user does not have a valid profile.
.
MessageId=1254
SymbolicName=ERROR_NOT_SUPPORTED_ON_SBS
Language=ENU
This operation is not supported on a computer running Windows Server 2003 for Small Business Server
.
MessageId=1255
SymbolicName=ERROR_SERVER_SHUTDOWN_IN_PROGRESS
Language=ENU
The server machine is shutting down.
.
MessageId=1256
SymbolicName=ERROR_HOST_DOWN
Language=ENU
The remote system is not available. For information about network troubleshooting, see Windows Help.
.
MessageId=1257
SymbolicName=ERROR_NON_ACCOUNT_SID
Language=ENU
The security identifier provided is not from an account domain.
.
MessageId=1258
SymbolicName=ERROR_NON_DOMAIN_SID
Language=ENU
The security identifier provided does not have a domain component.
.
MessageId=1259
SymbolicName=ERROR_APPHELP_BLOCK
Language=ENU
AppHelp dialog canceled thus preventing the application from starting.
.
MessageId=1260
SymbolicName=ERROR_ACCESS_DISABLED_BY_POLICY
Language=ENU
This program is blocked by group policy. For more information, contact your system administrator.
.
MessageId=1261
SymbolicName=ERROR_REG_NAT_CONSUMPTION
Language=ENU
A program attempt to use an invalid register value. Normally caused by an uninitialized register. This error is Itanium specific.
.
MessageId=1262
SymbolicName=ERROR_CSCSHARE_OFFLINE
Language=ENU
The share is currently offline or does not exist.
.
MessageId=1263
SymbolicName=ERROR_PKINIT_FAILURE
Language=ENU
The Kerberos protocol encountered an error while validating the KDC certificate during smartcard logon. There is more information in the system event log.
.
MessageId=1264
SymbolicName=ERROR_SMARTCARD_SUBSYSTEM_FAILURE
Language=ENU
The Kerberos protocol encountered an error while attempting to utilize the smartcard subsystem.
.
MessageId=1265
SymbolicName=ERROR_DOWNGRADE_DETECTED
Language=ENU
The system cannot contact a domain controller to service the authentication request. Please try again later.
.
MessageId=1271
SymbolicName=ERROR_MACHINE_LOCKED
Language=ENU
The machine is locked and cannot be shut down without the force option.
.
MessageId=1272
SymbolicName=ERROR_SMB_GUEST_LOGON_BLOCKED
Language=ENU
You can't access this shared folder because your organization's security policies block unauthenticated guest access. These policies help protect your PC from unsafe or malicious devices on the network.
.
MessageId=1273
SymbolicName=ERROR_CALLBACK_SUPPLIED_INVALID_DATA
Language=ENU
An application-defined callback gave invalid data when called.
.
MessageId=1274
SymbolicName=ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED
Language=ENU
The group policy framework should call the extension in the synchronous foreground policy refresh.
.
MessageId=1275
SymbolicName=ERROR_DRIVER_BLOCKED
Language=ENU
This driver has been blocked from loading
.
MessageId=1276
SymbolicName=ERROR_INVALID_IMPORT_OF_NON_DLL
Language=ENU
A dynamic link library (DLL) referenced a module that was neither a DLL nor the process's executable image.
.
MessageId=1277
SymbolicName=ERROR_ACCESS_DISABLED_WEBBLADE
Language=ENU
Windows cannot open this program since it has been disabled.
.
MessageId=1278
SymbolicName=ERROR_ACCESS_DISABLED_WEBBLADE_TAMPER
Language=ENU
Windows cannot open this program because the license enforcement system has been tampered with or become corrupted.
.
MessageId=1279
SymbolicName=ERROR_RECOVERY_FAILURE
Language=ENU
A transaction recover failed.
.
MessageId=1280
SymbolicName=ERROR_ALREADY_FIBER
Language=ENU
The current thread has already been converted to a fiber.
.
MessageId=1281
SymbolicName=ERROR_ALREADY_THREAD
Language=ENU
The current thread has already been converted from a fiber.
.
MessageId=1282
SymbolicName=ERROR_STACK_BUFFER_OVERRUN
Language=ENU
The system detected an overrun of a stack-based buffer in this application. This overrun could potentially allow a malicious user to gain control of this application.
.
MessageId=1283
SymbolicName=ERROR_PARAMETER_QUOTA_EXCEEDED
Language=ENU
Data present in one of the parameters is more than the function can operate on.
.
MessageId=1284
SymbolicName=ERROR_DEBUGGER_INACTIVE
Language=ENU
An attempt to do an operation on a debug object failed because the object is in the process of being deleted.
.
MessageId=1285
SymbolicName=ERROR_DELAY_LOAD_FAILED
Language=ENU
An attempt to delay-load a .dll or get a function address in a delay-loaded .dll failed.
.
MessageId=1286
SymbolicName=ERROR_VDM_DISALLOWED
Language=ENU
%1 is a 16-bit application. You do not have permissions to execute 16-bit applications. Check your permissions with your system administrator.
.
MessageId=1287
SymbolicName=ERROR_UNIDENTIFIED_ERROR
Language=ENU
Insufficient information exists to identify the cause of failure.
.
MessageId=1288
SymbolicName=ERROR_INVALID_CRUNTIME_PARAMETER
Language=ENU
The parameter passed to a C runtime function is incorrect.
.
MessageId=1289
SymbolicName=ERROR_BEYOND_VDL
Language=ENU
The operation occurred beyond the valid data length of the file.
.
MessageId=1290
SymbolicName=ERROR_INCOMPATIBLE_SERVICE_SID_TYPE
Language=ENU
The service start failed since one or more services in the same process have an incompatible service SID type setting. A service with restricted service SID type can only coexist in the same process with other services with a restricted SID type. If the service SID type for this service was just configured, the hosting process must be restarted in order to start this service.
.
MessageId=1291
SymbolicName=ERROR_DRIVER_PROCESS_TERMINATED
Language=ENU
The process hosting the driver for this device has been terminated.
.
MessageId=1292
SymbolicName=ERROR_IMPLEMENTATION_LIMIT
Language=ENU
An operation attempted to exceed an implementation-defined limit.
.
MessageId=1293
SymbolicName=ERROR_PROCESS_IS_PROTECTED
Language=ENU
Either the target process, or the target thread's containing process, is a protected process.
.
MessageId=1294
SymbolicName=ERROR_SERVICE_NOTIFY_CLIENT_LAGGING
Language=ENU
The service notification client is lagging too far behind the current state of services in the machine.
.
MessageId=1295
SymbolicName=ERROR_DISK_QUOTA_EXCEEDED
Language=ENU
The requested file operation failed because the storage quota was exceeded.
.
MessageId=1296
SymbolicName=ERROR_CONTENT_BLOCKED
Language=ENU
The requested file operation failed because the storage policy blocks that type of file. For more information, contact your system administrator.
.
MessageId=1297
SymbolicName=ERROR_INCOMPATIBLE_SERVICE_PRIVILEGE
Language=ENU
A privilege that the service requires to function properly does not exist in the service account configuration.
.
MessageId=1298
SymbolicName=ERROR_APP_HANG
Language=ENU
A thread involved in this operation appears to be unresponsive.
.
MessageId=1299
SymbolicName=ERROR_INVALID_LABEL
Language=ENU
Indicates a particular Security ID may not be assigned as the label of an object.
.
MessageId=1300
SymbolicName=ERROR_NOT_ALL_ASSIGNED
Language=ENU
Not all privileges or groups referenced are assigned to the caller.
.
MessageId=1301
SymbolicName=ERROR_SOME_NOT_MAPPED
Language=ENU
Some mapping between account names and security IDs was not done.
.
MessageId=1302
SymbolicName=ERROR_NO_QUOTAS_FOR_ACCOUNT
Language=ENU
No system quota limits are specifically set for this account.
.
MessageId=1303
SymbolicName=ERROR_LOCAL_USER_SESSION_KEY
Language=ENU
No encryption key is available. A well-known encryption key was returned.
.
MessageId=1304
SymbolicName=ERROR_NULL_LM_PASSWORD
Language=ENU
The password is too complex to be converted to a LAN Manager password. The LAN Manager password returned is a NULL string.
.
MessageId=1305
SymbolicName=ERROR_UNKNOWN_REVISION
Language=ENU
The revision level is unknown.
.
MessageId=1306
SymbolicName=ERROR_REVISION_MISMATCH
Language=ENU
Indicates two revision levels are incompatible.
.
MessageId=1307
SymbolicName=ERROR_INVALID_OWNER
Language=ENU
This security ID may not be assigned as the owner of this object.
.
MessageId=1308
SymbolicName=ERROR_INVALID_PRIMARY_GROUP
Language=ENU
This security ID may not be assigned as the primary group of an object.
.
MessageId=1309
SymbolicName=ERROR_NO_IMPERSONATION_TOKEN
Language=ENU
An attempt has been made to operate on an impersonation token by a thread that is not currently impersonating a client.
.
MessageId=1310
SymbolicName=ERROR_CANT_DISABLE_MANDATORY
Language=ENU
The group may not be disabled.
.
MessageId=1311
SymbolicName=ERROR_NO_LOGON_SERVERS
Language=ENU
We can't sign you in with this credential because your domain isn't available. Make sure your device is connected to your organization's network and try again. If you previously signed in on this device with another credential, you can sign in with that credential.
.
MessageId=1312
SymbolicName=ERROR_NO_SUCH_LOGON_SESSION
Language=ENU
A specified logon session does not exist. It may already have been terminated.
.
MessageId=1313
SymbolicName=ERROR_NO_SUCH_PRIVILEGE
Language=ENU
A specified privilege does not exist.
.
MessageId=1314
SymbolicName=ERROR_PRIVILEGE_NOT_HELD
Language=ENU
A required privilege is not held by the client.
.
MessageId=1315
SymbolicName=ERROR_INVALID_ACCOUNT_NAME
Language=ENU
The name provided is not a properly formed account name.
.
MessageId=1316
SymbolicName=ERROR_USER_EXISTS
Language=ENU
The specified account already exists.
.
MessageId=1317
SymbolicName=ERROR_NO_SUCH_USER
Language=ENU
The specified account does not exist.
.
MessageId=1318
SymbolicName=ERROR_GROUP_EXISTS
Language=ENU
The specified group already exists.
.
MessageId=1319
SymbolicName=ERROR_NO_SUCH_GROUP
Language=ENU
The specified group does not exist.
.
MessageId=1320
SymbolicName=ERROR_MEMBER_IN_GROUP
Language=ENU
Either the specified user account is already a member of the specified group, or the specified group cannot be deleted because it contains a member.
.
MessageId=1321
SymbolicName=ERROR_MEMBER_NOT_IN_GROUP
Language=ENU
The specified user account is not a member of the specified group account.
.
MessageId=1322
SymbolicName=ERROR_LAST_ADMIN
Language=ENU
This operation is disallowed as it could result in an administration account being disabled, deleted or unable to logon.
.
MessageId=1323
SymbolicName=ERROR_WRONG_PASSWORD
Language=ENU
Unable to update the password. The value provided as the current password is incorrect.
.
MessageId=1324
SymbolicName=ERROR_ILL_FORMED_PASSWORD
Language=ENU
Unable to update the password. The value provided for the new password contains values that are not allowed in passwords.
.
MessageId=1325
SymbolicName=ERROR_PASSWORD_RESTRICTION
Language=ENU
Unable to update the password. The value provided for the new password does not meet the length, complexity, or history requirements of the machine or domain. Try increasing the length of your password, along with including upper and lowercase characters, numbers, and symbols.
.
MessageId=1326
SymbolicName=ERROR_LOGON_FAILURE
Language=ENU
The user name or password is incorrect.
.
MessageId=1327
SymbolicName=ERROR_ACCOUNT_RESTRICTION
Language=ENU
Account restrictions are preventing this user from signing in. For example: blank passwords aren't allowed, sign-in times are limited, or a policy restriction has been enforced.
.
MessageId=1328
SymbolicName=ERROR_INVALID_LOGON_HOURS
Language=ENU
Your account has time restrictions that keep you from signing in right now.
.
MessageId=1329
SymbolicName=ERROR_INVALID_WORKSTATION
Language=ENU
This user isn't allowed to sign in to this computer.
.
MessageId=1330
SymbolicName=ERROR_PASSWORD_EXPIRED
Language=ENU
The password for this account has expired.
.
MessageId=1331
SymbolicName=ERROR_ACCOUNT_DISABLED
Language=ENU
This user can't sign in because this account is currently disabled.
.
MessageId=1332
SymbolicName=ERROR_NONE_MAPPED
Language=ENU
No mapping between account names and security IDs was done.
.
MessageId=1333
SymbolicName=ERROR_TOO_MANY_LUIDS_REQUESTED
Language=ENU
Too many local user identifiers (LUIDs) were requested at one time.
.
MessageId=1334
SymbolicName=ERROR_LUIDS_EXHAUSTED
Language=ENU
No more local user identifiers (LUIDs) are available.
.
MessageId=1335
SymbolicName=ERROR_INVALID_SUB_AUTHORITY
Language=ENU
The subauthority part of a security ID is invalid for this particular use.
.
MessageId=1336
SymbolicName=ERROR_INVALID_ACL
Language=ENU
The access control list (ACL) structure is invalid.
.
MessageId=1337
SymbolicName=ERROR_INVALID_SID
Language=ENU
The security ID structure is invalid.
.
MessageId=1338
SymbolicName=ERROR_INVALID_SECURITY_DESCR
Language=ENU
The security descriptor structure is invalid.
.
MessageId=1340
SymbolicName=ERROR_BAD_INHERITANCE_ACL
Language=ENU
The inherited access control list (ACL) or access control entry (ACE) could not be built.
.
MessageId=1341
SymbolicName=ERROR_SERVER_DISABLED
Language=ENU
The server is currently disabled.
.
MessageId=1342
SymbolicName=ERROR_SERVER_NOT_DISABLED
Language=ENU
The server is currently enabled.
.
MessageId=1343
SymbolicName=ERROR_INVALID_ID_AUTHORITY
Language=ENU
The value provided was an invalid value for an identifier authority.
.
MessageId=1344
SymbolicName=ERROR_ALLOTTED_SPACE_EXCEEDED
Language=ENU
No more memory is available for security information updates.
.
MessageId=1345
SymbolicName=ERROR_INVALID_GROUP_ATTRIBUTES
Language=ENU
The specified attributes are invalid, or incompatible with the attributes for the group as a whole.
.
MessageId=1346
SymbolicName=ERROR_BAD_IMPERSONATION_LEVEL
Language=ENU
Either a required impersonation level was not provided, or the provided impersonation level is invalid.
.
MessageId=1347
SymbolicName=ERROR_CANT_OPEN_ANONYMOUS
Language=ENU
Cannot open an anonymous level security token.
.
MessageId=1348
SymbolicName=ERROR_BAD_VALIDATION_CLASS
Language=ENU
The validation information class requested was invalid.
.
MessageId=1349
SymbolicName=ERROR_BAD_TOKEN_TYPE
Language=ENU
The type of the token is inappropriate for its attempted use.
.
MessageId=1350
SymbolicName=ERROR_NO_SECURITY_ON_OBJECT
Language=ENU
Unable to perform a security operation on an object that has no associated security.
.
MessageId=1351
SymbolicName=ERROR_CANT_ACCESS_DOMAIN_INFO
Language=ENU
Configuration information could not be read from the domain controller, either because the machine is unavailable, or access has been denied.
.
MessageId=1352
SymbolicName=ERROR_INVALID_SERVER_STATE
Language=ENU
The security account manager (SAM) or local security authority (LSA) server was in the wrong state to perform the security operation.
.
MessageId=1353
SymbolicName=ERROR_INVALID_DOMAIN_STATE
Language=ENU
The domain was in the wrong state to perform the security operation.
.
MessageId=1354
SymbolicName=ERROR_INVALID_DOMAIN_ROLE
Language=ENU
This operation is only allowed for the Primary Domain Controller of the domain.
.
MessageId=1355
SymbolicName=ERROR_NO_SUCH_DOMAIN
Language=ENU
The specified domain either does not exist or could not be contacted.
.
MessageId=1356
SymbolicName=ERROR_DOMAIN_EXISTS
Language=ENU
The specified domain already exists.
.
MessageId=1357
SymbolicName=ERROR_DOMAIN_LIMIT_EXCEEDED
Language=ENU
An attempt was made to exceed the limit on the number of domains per server.
.
MessageId=1358
SymbolicName=ERROR_INTERNAL_DB_CORRUPTION
Language=ENU
Unable to complete the requested operation because of either a catastrophic media failure or a data structure corruption on the disk.
.
MessageId=1359
SymbolicName=ERROR_INTERNAL_ERROR
Language=ENU
An internal error occurred.
.
MessageId=1360
SymbolicName=ERROR_GENERIC_NOT_MAPPED
Language=ENU
Generic access types were contained in an access mask which should already be mapped to nongeneric types.
.
MessageId=1361
SymbolicName=ERROR_BAD_DESCRIPTOR_FORMAT
Language=ENU
A security descriptor is not in the right format (absolute or self-relative).
.
MessageId=1362
SymbolicName=ERROR_NOT_LOGON_PROCESS
Language=ENU
The requested action is restricted for use by logon processes only. The calling process has not registered as a logon process.
.
MessageId=1363
SymbolicName=ERROR_LOGON_SESSION_EXISTS
Language=ENU
Cannot start a new logon session with an ID that is already in use.
.
MessageId=1364
SymbolicName=ERROR_NO_SUCH_PACKAGE
Language=ENU
A specified authentication package is unknown.
.
MessageId=1365
SymbolicName=ERROR_BAD_LOGON_SESSION_STATE
Language=ENU
The logon session is not in a state that is consistent with the requested operation.
.
MessageId=1366
SymbolicName=ERROR_LOGON_SESSION_COLLISION
Language=ENU
The logon session ID is already in use.
.
MessageId=1367
SymbolicName=ERROR_INVALID_LOGON_TYPE
Language=ENU
A logon request contained an invalid logon type value.
.
MessageId=1368
SymbolicName=ERROR_CANNOT_IMPERSONATE
Language=ENU
Unable to impersonate using a named pipe until data has been read from that pipe.
.
MessageId=1369
SymbolicName=ERROR_RXACT_INVALID_STATE
Language=ENU
The transaction state of a registry subtree is incompatible with the requested operation.
.
MessageId=1370
SymbolicName=ERROR_RXACT_COMMIT_FAILURE
Language=ENU
An internal security database corruption has been encountered.
.
MessageId=1371
SymbolicName=ERROR_SPECIAL_ACCOUNT
Language=ENU
Cannot perform this operation on built-in accounts.
.
MessageId=1372
SymbolicName=ERROR_SPECIAL_GROUP
Language=ENU
Cannot perform this operation on this built-in special group.
.
MessageId=1373
SymbolicName=ERROR_SPECIAL_USER
Language=ENU
Cannot perform this operation on this built-in special user.
.
MessageId=1374
SymbolicName=ERROR_MEMBERS_PRIMARY_GROUP
Language=ENU
The user cannot be removed from a group because the group is currently the user's primary group.
.
MessageId=1375
SymbolicName=ERROR_TOKEN_ALREADY_IN_USE
Language=ENU
The token is already in use as a primary token.
.
MessageId=1376
SymbolicName=ERROR_NO_SUCH_ALIAS
Language=ENU
The specified local group does not exist.
.
MessageId=1377
SymbolicName=ERROR_MEMBER_NOT_IN_ALIAS
Language=ENU
The specified account name is not a member of the group.
.
MessageId=1378
SymbolicName=ERROR_MEMBER_IN_ALIAS
Language=ENU
The specified account name is already a member of the group.
.
MessageId=1379
SymbolicName=ERROR_ALIAS_EXISTS
Language=ENU
The specified local group already exists.
.
MessageId=1380
SymbolicName=ERROR_LOGON_NOT_GRANTED
Language=ENU
Logon failure: the user has not been granted the requested logon type at this computer.
.
MessageId=1381
SymbolicName=ERROR_TOO_MANY_SECRETS
Language=ENU
The maximum number of secrets that may be stored in a single system has been exceeded.
.
MessageId=1382
SymbolicName=ERROR_SECRET_TOO_LONG
Language=ENU
The length of a secret exceeds the maximum length allowed.
.
MessageId=1383
SymbolicName=ERROR_INTERNAL_DB_ERROR
Language=ENU
The local security authority database contains an internal inconsistency.
.
MessageId=1384
SymbolicName=ERROR_TOO_MANY_CONTEXT_IDS
Language=ENU
During a logon attempt, the user's security context accumulated too many security IDs.
.
MessageId=1385
SymbolicName=ERROR_LOGON_TYPE_NOT_GRANTED
Language=ENU
Logon failure: the user has not been granted the requested logon type at this computer.
.
MessageId=1386
SymbolicName=ERROR_NT_CROSS_ENCRYPTION_REQUIRED
Language=ENU
A cross-encrypted password is necessary to change a user password.
.
MessageId=1387
SymbolicName=ERROR_NO_SUCH_MEMBER
Language=ENU
A member could not be added to or removed from the local group because the member does not exist.
.
MessageId=1388
SymbolicName=ERROR_INVALID_MEMBER
Language=ENU
A new member could not be added to a local group because the member has the wrong account type.
.
MessageId=1389
SymbolicName=ERROR_TOO_MANY_SIDS
Language=ENU
Too many security IDs have been specified.
.
MessageId=1390
SymbolicName=ERROR_LM_CROSS_ENCRYPTION_REQUIRED
Language=ENU
A cross-encrypted password is necessary to change this user password.
.
MessageId=1391
SymbolicName=ERROR_NO_INHERITANCE
Language=ENU
Indicates an ACL contains no inheritable components.
.
MessageId=1392
SymbolicName=ERROR_FILE_CORRUPT
Language=ENU
The file or directory is corrupted and unreadable.
.
MessageId=1393
SymbolicName=ERROR_DISK_CORRUPT
Language=ENU
The disk structure is corrupted and unreadable.
.
MessageId=1394
SymbolicName=ERROR_NO_USER_SESSION_KEY
Language=ENU
There is no user session key for the specified logon session.
.
MessageId=1395
SymbolicName=ERROR_LICENSE_QUOTA_EXCEEDED
Language=ENU
The service being accessed is licensed for a particular number of connections. No more connections can be made to the service at this time because there are already as many connections as the service can accept.
.
MessageId=1396
SymbolicName=ERROR_WRONG_TARGET_NAME
Language=ENU
The target account name is incorrect.
.
MessageId=1397
SymbolicName=ERROR_MUTUAL_AUTH_FAILED
Language=ENU
Mutual Authentication failed. The server's password is out of date at the domain controller.
.
MessageId=1398
SymbolicName=ERROR_TIME_SKEW
Language=ENU
There is a time and/or date difference between the client and server.
.
MessageId=1399
SymbolicName=ERROR_CURRENT_DOMAIN_NOT_ALLOWED
Language=ENU
This operation cannot be performed on the current domain.
.
MessageId=1400
SymbolicName=ERROR_INVALID_WINDOW_HANDLE
Language=ENU
Invalid window handle.
.
MessageId=1401
SymbolicName=ERROR_INVALID_MENU_HANDLE
Language=ENU
Invalid menu handle.
.
MessageId=1402
SymbolicName=ERROR_INVALID_CURSOR_HANDLE
Language=ENU
Invalid cursor handle.
.
MessageId=1403
SymbolicName=ERROR_INVALID_ACCEL_HANDLE
Language=ENU
Invalid accelerator table handle.
.
MessageId=1404
SymbolicName=ERROR_INVALID_HOOK_HANDLE
Language=ENU
Invalid hook handle.
.
MessageId=1405
SymbolicName=ERROR_INVALID_DWP_HANDLE
Language=ENU
Invalid handle to a multiple-window position structure.
.
MessageId=1406
SymbolicName=ERROR_TLW_WITH_WSCHILD
Language=ENU
Cannot create a top-level child window.
.
MessageId=1407
SymbolicName=ERROR_CANNOT_FIND_WND_CLASS
Language=ENU
Cannot find window class.
.
MessageId=1408
SymbolicName=ERROR_WINDOW_OF_OTHER_THREAD
Language=ENU
Invalid window; it belongs to other thread.
.
MessageId=1409
SymbolicName=ERROR_HOTKEY_ALREADY_REGISTERED
Language=ENU
Hot key is already registered.
.
MessageId=1410
SymbolicName=ERROR_CLASS_ALREADY_EXISTS
Language=ENU
Class already exists.
.
MessageId=1411
SymbolicName=ERROR_CLASS_DOES_NOT_EXIST
Language=ENU
Class does not exist.
.
MessageId=1412
SymbolicName=ERROR_CLASS_HAS_WINDOWS
Language=ENU
Class still has open windows.
.
MessageId=1413
SymbolicName=ERROR_INVALID_INDEX
Language=ENU
Invalid index.
.
MessageId=1414
SymbolicName=ERROR_INVALID_ICON_HANDLE
Language=ENU
Invalid icon handle.
.
MessageId=1415
SymbolicName=ERROR_PRIVATE_DIALOG_INDEX
Language=ENU
Using private DIALOG window words.
.
MessageId=1416
SymbolicName=ERROR_LISTBOX_ID_NOT_FOUND
Language=ENU
The list box identifier was not found.
.
MessageId=1417
SymbolicName=ERROR_NO_WILDCARD_CHARACTERS
Language=ENU
No wildcards were found.
.
MessageId=1418
SymbolicName=ERROR_CLIPBOARD_NOT_OPEN
Language=ENU
Thread does not have a clipboard open.
.
MessageId=1419
SymbolicName=ERROR_HOTKEY_NOT_REGISTERED
Language=ENU
Hot key is not registered.
.
MessageId=1420
SymbolicName=ERROR_WINDOW_NOT_DIALOG
Language=ENU
The window is not a valid dialog window.
.
MessageId=1421
SymbolicName=ERROR_CONTROL_ID_NOT_FOUND
Language=ENU
Control ID not found.
.
MessageId=1422
SymbolicName=ERROR_INVALID_COMBOBOX_MESSAGE
Language=ENU
Invalid message for a combo box because it does not have an edit control.
.
MessageId=1423
SymbolicName=ERROR_WINDOW_NOT_COMBOBOX
Language=ENU
The window is not a combo box.
.
MessageId=1424
SymbolicName=ERROR_INVALID_EDIT_HEIGHT
Language=ENU
Height must be less than 256.
.
MessageId=1425
SymbolicName=ERROR_DC_NOT_FOUND
Language=ENU
Invalid device context (DC) handle.
.
MessageId=1426
SymbolicName=ERROR_INVALID_HOOK_FILTER
Language=ENU
Invalid hook procedure type.
.
MessageId=1427
SymbolicName=ERROR_INVALID_FILTER_PROC
Language=ENU
Invalid hook procedure.
.
MessageId=1428
SymbolicName=ERROR_HOOK_NEEDS_HMOD
Language=ENU
Cannot set nonlocal hook without a module handle.
.
MessageId=1429
SymbolicName=ERROR_GLOBAL_ONLY_HOOK
Language=ENU
This hook procedure can only be set globally.
.
MessageId=1430
SymbolicName=ERROR_JOURNAL_HOOK_SET
Language=ENU
The journal hook procedure is already installed.
.
MessageId=1431
SymbolicName=ERROR_HOOK_NOT_INSTALLED
Language=ENU
The hook procedure is not installed.
.
MessageId=1432
SymbolicName=ERROR_INVALID_LB_MESSAGE
Language=ENU
Invalid message for single-selection list box.
.
MessageId=1433
SymbolicName=ERROR_SETCOUNT_ON_BAD_LB
Language=ENU
LB_SETCOUNT sent to non-lazy list box.
.
MessageId=1434
SymbolicName=ERROR_LB_WITHOUT_TABSTOPS
Language=ENU
This list box does not support tab stops.
.
MessageId=1435
SymbolicName=ERROR_DESTROY_OBJECT_OF_OTHER_THREAD
Language=ENU
Cannot destroy object created by another thread.
.
MessageId=1436
SymbolicName=ERROR_CHILD_WINDOW_MENU
Language=ENU
Child windows cannot have menus.
.
MessageId=1437
SymbolicName=ERROR_NO_SYSTEM_MENU
Language=ENU
The window does not have a system menu.
.
MessageId=1438
SymbolicName=ERROR_INVALID_MSGBOX_STYLE
Language=ENU
Invalid message box style.
.
MessageId=1439
SymbolicName=ERROR_INVALID_SPI_VALUE
Language=ENU
Invalid system-wide (SPI_*) parameter.
.
MessageId=1440
SymbolicName=ERROR_SCREEN_ALREADY_LOCKED
Language=ENU
Screen already locked.
.
MessageId=1441
SymbolicName=ERROR_HWNDS_HAVE_DIFF_PARENT
Language=ENU
All handles to windows in a multiple-window position structure must have the same parent.
.
MessageId=1442
SymbolicName=ERROR_NOT_CHILD_WINDOW
Language=ENU
The window is not a child window.
.
MessageId=1443
SymbolicName=ERROR_INVALID_GW_COMMAND
Language=ENU
Invalid GW_* command.
.
MessageId=1444
SymbolicName=ERROR_INVALID_THREAD_ID
Language=ENU
Invalid thread identifier.
.
MessageId=1445
SymbolicName=ERROR_NON_MDICHILD_WINDOW
Language=ENU
Cannot process a message from a window that is not a multiple document interface (MDI) window.
.
MessageId=1446
SymbolicName=ERROR_POPUP_ALREADY_ACTIVE
Language=ENU
Popup menu already active.
.
MessageId=1447
SymbolicName=ERROR_NO_SCROLLBARS
Language=ENU
The window does not have scroll bars.
.
MessageId=1448
SymbolicName=ERROR_INVALID_SCROLLBAR_RANGE
Language=ENU
Scroll bar range cannot be greater than MAXLONG.
.
MessageId=1449
SymbolicName=ERROR_INVALID_SHOWWIN_COMMAND
Language=ENU
Cannot show or remove the window in the way specified.
.
MessageId=1450
SymbolicName=ERROR_NO_SYSTEM_RESOURCES
Language=ENU
Insufficient system resources exist to complete the requested service.
.
MessageId=1451
SymbolicName=ERROR_NONPAGED_SYSTEM_RESOURCES
Language=ENU
Insufficient system resources exist to complete the requested service.
.
MessageId=1452
SymbolicName=ERROR_PAGED_SYSTEM_RESOURCES
Language=ENU
Insufficient system resources exist to complete the requested service.
.
MessageId=1453
SymbolicName=ERROR_WORKING_SET_QUOTA
Language=ENU
Insufficient quota to complete the requested service.
.
MessageId=1454
SymbolicName=ERROR_PAGEFILE_QUOTA
Language=ENU
Insufficient quota to complete the requested service.
.
MessageId=1455
SymbolicName=ERROR_COMMITMENT_LIMIT
Language=ENU
The paging file is too small for this operation to complete.
.
MessageId=1456
SymbolicName=ERROR_MENU_ITEM_NOT_FOUND
Language=ENU
A menu item was not found.
.
MessageId=1457
SymbolicName=ERROR_INVALID_KEYBOARD_HANDLE
Language=ENU
Invalid keyboard layout handle.
.
MessageId=1458
SymbolicName=ERROR_HOOK_TYPE_NOT_ALLOWED
Language=ENU
Hook type not allowed.
.
MessageId=1459
SymbolicName=ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION
Language=ENU
This operation requires an interactive window station.
.
MessageId=1460
SymbolicName=ERROR_TIMEOUT
Language=ENU
This operation returned because the timeout period expired.
.
MessageId=1461
SymbolicName=ERROR_INVALID_MONITOR_HANDLE
Language=ENU
Invalid monitor handle.
.
MessageId=1462
SymbolicName=ERROR_INCORRECT_SIZE
Language=ENU
Incorrect size argument.
.
MessageId=1463
SymbolicName=ERROR_SYMLINK_CLASS_DISABLED
Language=ENU
The symbolic link cannot be followed because its type is disabled.
.
MessageId=1464
SymbolicName=ERROR_SYMLINK_NOT_SUPPORTED
Language=ENU
This application does not support the current operation on symbolic links.
.
MessageId=1465
SymbolicName=ERROR_XML_PARSE_ERROR
Language=ENU
Windows was unable to parse the requested XML data.
.
MessageId=1466
SymbolicName=ERROR_XMLDSIG_ERROR
Language=ENU
An error was encountered while processing an XML digital signature.
.
MessageId=1467
SymbolicName=ERROR_RESTART_APPLICATION
Language=ENU
This application must be restarted.
.
MessageId=1468
SymbolicName=ERROR_WRONG_COMPARTMENT
Language=ENU
The caller made the connection request in the wrong routing compartment.
.
MessageId=1469
SymbolicName=ERROR_AUTHIP_FAILURE
Language=ENU
There was an AuthIP failure when attempting to connect to the remote host.
.
MessageId=1470
SymbolicName=ERROR_NO_NVRAM_RESOURCES
Language=ENU
Insufficient NVRAM resources exist to complete the requested service. A reboot might be required.
.
MessageId=1471
SymbolicName=ERROR_NOT_GUI_PROCESS
Language=ENU
Unable to finish the requested operation because the specified process is not a GUI process.
.
MessageId=1500
SymbolicName=ERROR_EVENTLOG_FILE_CORRUPT
Language=ENU
The event log file is corrupted.
.
MessageId=1501
SymbolicName=ERROR_EVENTLOG_CANT_START
Language=ENU
No event log file could be opened, so the event logging service did not start.
.
MessageId=1502
SymbolicName=ERROR_LOG_FILE_FULL
Language=ENU
The event log file is full.
.
MessageId=1503
SymbolicName=ERROR_EVENTLOG_FILE_CHANGED
Language=ENU
The event log file has changed between read operations.
.
MessageId=1504
SymbolicName=ERROR_CONTAINER_ASSIGNED
Language=ENU
The specified Job already has a container assigned to it.
.
MessageId=1505
SymbolicName=ERROR_JOB_NO_CONTAINER
Language=ENU
The specified Job does not have a container assigned to it.
.
MessageId=1550
SymbolicName=ERROR_INVALID_TASK_NAME
Language=ENU
The specified task name is invalid.
.
MessageId=1551
SymbolicName=ERROR_INVALID_TASK_INDEX
Language=ENU
The specified task index is invalid.
.
MessageId=1552
SymbolicName=ERROR_THREAD_ALREADY_IN_TASK
Language=ENU
The specified thread is already joining a task.
.
MessageId=1601
SymbolicName=ERROR_INSTALL_SERVICE_FAILURE
Language=ENU
The Windows Installer Service could not be accessed. This can occur if the Windows Installer is not correctly installed. Contact your support personnel for assistance.
.
MessageId=1602
SymbolicName=ERROR_INSTALL_USEREXIT
Language=ENU
User cancelled installation.
.
MessageId=1603
SymbolicName=ERROR_INSTALL_FAILURE
Language=ENU
Fatal error during installation.
.
MessageId=1604
SymbolicName=ERROR_INSTALL_SUSPEND
Language=ENU
Installation suspended, incomplete.
.
MessageId=1605
SymbolicName=ERROR_UNKNOWN_PRODUCT
Language=ENU
This action is only valid for products that are currently installed.
.
MessageId=1606
SymbolicName=ERROR_UNKNOWN_FEATURE
Language=ENU
Feature ID not registered.
.
MessageId=1607
SymbolicName=ERROR_UNKNOWN_COMPONENT
Language=ENU
Component ID not registered.
.
MessageId=1608
SymbolicName=ERROR_UNKNOWN_PROPERTY
Language=ENU
Unknown property.
.
MessageId=1609
SymbolicName=ERROR_INVALID_HANDLE_STATE
Language=ENU
Handle is in an invalid state.
.
MessageId=1610
SymbolicName=ERROR_BAD_CONFIGURATION
Language=ENU
The configuration data for this product is corrupt. Contact your support personnel.
.
MessageId=1611
SymbolicName=ERROR_INDEX_ABSENT
Language=ENU
Component qualifier not present.
.
MessageId=1612
SymbolicName=ERROR_INSTALL_SOURCE_ABSENT
Language=ENU
The installation source for this product is not available. Verify that the source exists and that you can access it.
.
MessageId=1613
SymbolicName=ERROR_INSTALL_PACKAGE_VERSION
Language=ENU
This installation package cannot be installed by the Windows Installer service. You must install a Windows service pack that contains a newer version of the Windows Installer service.
.
MessageId=1614
SymbolicName=ERROR_PRODUCT_UNINSTALLED
Language=ENU
Product is uninstalled.
.
MessageId=1615
SymbolicName=ERROR_BAD_QUERY_SYNTAX
Language=ENU
SQL query syntax invalid or unsupported.
.
MessageId=1616
SymbolicName=ERROR_INVALID_FIELD
Language=ENU
Record field does not exist.
.
MessageId=1617
SymbolicName=ERROR_DEVICE_REMOVED
Language=ENU
The device has been removed.
.
MessageId=1618
SymbolicName=ERROR_INSTALL_ALREADY_RUNNING
Language=ENU
Another installation is already in progress. Complete that installation before proceeding with this install.
.
MessageId=1619
SymbolicName=ERROR_INSTALL_PACKAGE_OPEN_FAILED
Language=ENU
This installation package could not be opened. Verify that the package exists and that you can access it, or contact the application vendor to verify that this is a valid Windows Installer package.
.
MessageId=1620
SymbolicName=ERROR_INSTALL_PACKAGE_INVALID
Language=ENU
This installation package could not be opened. Contact the application vendor to verify that this is a valid Windows Installer package.
.
MessageId=1621
SymbolicName=ERROR_INSTALL_UI_FAILURE
Language=ENU
There was an error starting the Windows Installer service user interface. Contact your support personnel.
.
MessageId=1622
SymbolicName=ERROR_INSTALL_LOG_FAILURE
Language=ENU
Error opening installation log file. Verify that the specified log file location exists and that you can write to it.
.
MessageId=1623
SymbolicName=ERROR_INSTALL_LANGUAGE_UNSUPPORTED
Language=ENU
The language of this installation package is not supported by your system.
.
MessageId=1624
SymbolicName=ERROR_INSTALL_TRANSFORM_FAILURE
Language=ENU
Error applying transforms. Verify that the specified transform paths are valid.
.
MessageId=1625
SymbolicName=ERROR_INSTALL_PACKAGE_REJECTED
Language=ENU
This installation is forbidden by system policy. Contact your system administrator.
.
MessageId=1626
SymbolicName=ERROR_FUNCTION_NOT_CALLED
Language=ENU
Function could not be executed.
.
MessageId=1627
SymbolicName=ERROR_FUNCTION_FAILED
Language=ENU
Function failed during execution.
.
MessageId=1628
SymbolicName=ERROR_INVALID_TABLE
Language=ENU
Invalid or unknown table specified.
.
MessageId=1629
SymbolicName=ERROR_DATATYPE_MISMATCH
Language=ENU
Data supplied is of wrong type.
.
MessageId=1630
SymbolicName=ERROR_UNSUPPORTED_TYPE
Language=ENU
Data of this type is not supported.
.
MessageId=1631
SymbolicName=ERROR_CREATE_FAILED
Language=ENU
The Windows Installer service failed to start. Contact your support personnel.
.
MessageId=1632
SymbolicName=ERROR_INSTALL_TEMP_UNWRITABLE
Language=ENU
The Temp folder is on a drive that is full or is inaccessible. Free up space on the drive or verify that you have write permission on the Temp folder.
.
MessageId=1633
SymbolicName=ERROR_INSTALL_PLATFORM_UNSUPPORTED
Language=ENU
This installation package is not supported by this processor type. Contact your product vendor.
.
MessageId=1634
SymbolicName=ERROR_INSTALL_NOTUSED
Language=ENU
Component not used on this computer.
.
MessageId=1635
SymbolicName=ERROR_PATCH_PACKAGE_OPEN_FAILED
Language=ENU
This update package could not be opened. Verify that the update package exists and that you can access it, or contact the application vendor to verify that this is a valid Windows Installer update package.
.
MessageId=1636
SymbolicName=ERROR_PATCH_PACKAGE_INVALID
Language=ENU
This update package could not be opened. Contact the application vendor to verify that this is a valid Windows Installer update package.
.
MessageId=1637
SymbolicName=ERROR_PATCH_PACKAGE_UNSUPPORTED
Language=ENU
This update package cannot be processed by the Windows Installer service. You must install a Windows service pack that contains a newer version of the Windows Installer service.
.
MessageId=1638
SymbolicName=ERROR_PRODUCT_VERSION
Language=ENU
Another version of this product is already installed. Installation of this version cannot continue. To configure or remove the existing version of this product, use Add/Remove Programs on the Control Panel.
.
MessageId=1639
SymbolicName=ERROR_INVALID_COMMAND_LINE
Language=ENU
Invalid command line argument. Consult the Windows Installer SDK for detailed command line help.
.
MessageId=1640
SymbolicName=ERROR_INSTALL_REMOTE_DISALLOWED
Language=ENU
Only administrators have permission to add, remove, or configure server software during a Terminal services remote session. If you want to install or configure software on the server, contact your network administrator.
.
MessageId=1641
SymbolicName=ERROR_SUCCESS_REBOOT_INITIATED
Language=ENU
The requested operation completed successfully. The system will be restarted so the changes can take effect.
.
MessageId=1642
SymbolicName=ERROR_PATCH_TARGET_NOT_FOUND
Language=ENU
The upgrade cannot be installed by the Windows Installer service because the program to be upgraded may be missing, or the upgrade may update a different version of the program. Verify that the program to be upgraded exists on your computer and that you have the correct upgrade.
.
MessageId=1643
SymbolicName=ERROR_PATCH_PACKAGE_REJECTED
Language=ENU
The update package is not permitted by software restriction policy.
.
MessageId=1644
SymbolicName=ERROR_INSTALL_TRANSFORM_REJECTED
Language=ENU
One or more customizations are not permitted by software restriction policy.
.
MessageId=1645
SymbolicName=ERROR_INSTALL_REMOTE_PROHIBITED
Language=ENU
The Windows Installer does not permit installation from a Remote Desktop Connection.
.
MessageId=1646
SymbolicName=ERROR_PATCH_REMOVAL_UNSUPPORTED
Language=ENU
Uninstallation of the update package is not supported.
.
MessageId=1647
SymbolicName=ERROR_UNKNOWN_PATCH
Language=ENU
The update is not applied to this product.
.
MessageId=1648
SymbolicName=ERROR_PATCH_NO_SEQUENCE
Language=ENU
No valid sequence could be found for the set of updates.
.
MessageId=1649
SymbolicName=ERROR_PATCH_REMOVAL_DISALLOWED
Language=ENU
Update removal was disallowed by policy.
.
MessageId=1650
SymbolicName=ERROR_INVALID_PATCH_XML
Language=ENU
The XML update data is invalid.
.
MessageId=1651
SymbolicName=ERROR_PATCH_MANAGED_ADVERTISED_PRODUCT
Language=ENU
Windows Installer does not permit updating of managed advertised products. At least one feature of the product must be installed before applying the update.
.
MessageId=1652
SymbolicName=ERROR_INSTALL_SERVICE_SAFEBOOT
Language=ENU
The Windows Installer service is not accessible in Safe Mode. Please try again when your computer is not in Safe Mode or you can use System Restore to return your machine to a previous good state.
.
MessageId=1653
SymbolicName=ERROR_FAIL_FAST_EXCEPTION
Language=ENU
A fail fast exception occurred. Exception handlers will not be invoked and the process will be terminated immediately.
.
MessageId=1654
SymbolicName=ERROR_INSTALL_REJECTED
Language=ENU
The app that you are trying to run is not supported on this version of Windows.
.
MessageId=1655
SymbolicName=ERROR_DYNAMIC_CODE_BLOCKED
Language=ENU
The operation was blocked as the process prohibits dynamic code generation.
.
MessageId=1656
SymbolicName=ERROR_NOT_SAME_OBJECT
Language=ENU
The objects are not identical.
.
MessageId=1657
SymbolicName=ERROR_STRICT_CFG_VIOLATION
Language=ENU
The specified image file was blocked from loading because it does not enable a feature required by the process: Control Flow Guard.
.
MessageId=1660
SymbolicName=ERROR_SET_CONTEXT_DENIED
Language=ENU
The thread context could not be updated because this has been restricted for the process.
.
MessageId=1661
SymbolicName=ERROR_CROSS_PARTITION_VIOLATION
Language=ENU
An invalid cross-partition private file/section access was attempted.
.
MessageId=1662
SymbolicName=ERROR_RETURN_ADDRESS_HIJACK_ATTEMPT
Language=ENU
A return address hijack is being attempted. This is supported by the operating system when user-mode shadow stacks are enabled.
.
MessageId=1700
SymbolicName=RPC_S_INVALID_STRING_BINDING
Language=ENU
The string binding is invalid.
.
MessageId=1701
SymbolicName=RPC_S_WRONG_KIND_OF_BINDING
Language=ENU
The binding handle is not the correct type.
.
MessageId=1702
SymbolicName=RPC_S_INVALID_BINDING
Language=ENU
The binding handle is invalid.
.
MessageId=1703
SymbolicName=RPC_S_PROTSEQ_NOT_SUPPORTED
Language=ENU
The RPC protocol sequence is not supported.
.
MessageId=1704
SymbolicName=RPC_S_INVALID_RPC_PROTSEQ
Language=ENU
The RPC protocol sequence is invalid.
.
MessageId=1705
SymbolicName=RPC_S_INVALID_STRING_UUID
Language=ENU
The string universal unique identifier (UUID) is invalid.
.
MessageId=1706
SymbolicName=RPC_S_INVALID_ENDPOINT_FORMAT
Language=ENU
The endpoint format is invalid.
.
MessageId=1707
SymbolicName=RPC_S_INVALID_NET_ADDR
Language=ENU
The network address is invalid.
.
MessageId=1708
SymbolicName=RPC_S_NO_ENDPOINT_FOUND
Language=ENU
No endpoint was found.
.
MessageId=1709
SymbolicName=RPC_S_INVALID_TIMEOUT
Language=ENU
The timeout value is invalid.
.
MessageId=1710
SymbolicName=RPC_S_OBJECT_NOT_FOUND
Language=ENU
The object universal unique identifier (UUID) was not found.
.
MessageId=1711
SymbolicName=RPC_S_ALREADY_REGISTERED
Language=ENU
The object universal unique identifier (UUID) has already been registered.
.
MessageId=1712
SymbolicName=RPC_S_TYPE_ALREADY_REGISTERED
Language=ENU
The type universal unique identifier (UUID) has already been registered.
.
MessageId=1713
SymbolicName=RPC_S_ALREADY_LISTENING
Language=ENU
The RPC server is already listening.
.
MessageId=1714
SymbolicName=RPC_S_NO_PROTSEQS_REGISTERED
Language=ENU
No protocol sequences have been registered.
.
MessageId=1715
SymbolicName=RPC_S_NOT_LISTENING
Language=ENU
The RPC server is not listening.
.
MessageId=1716
SymbolicName=RPC_S_UNKNOWN_MGR_TYPE
Language=ENU
The manager type is unknown.
.
MessageId=1717
SymbolicName=RPC_S_UNKNOWN_IF
Language=ENU
The interface is unknown.
.
MessageId=1718
SymbolicName=RPC_S_NO_BINDINGS
Language=ENU
There are no bindings.
.
MessageId=1719
SymbolicName=RPC_S_NO_PROTSEQS
Language=ENU
There are no protocol sequences.
.
MessageId=1720
SymbolicName=RPC_S_CANT_CREATE_ENDPOINT
Language=ENU
The endpoint cannot be created.
.
MessageId=1721
SymbolicName=RPC_S_OUT_OF_RESOURCES
Language=ENU
Not enough resources are available to complete this operation.
.
MessageId=1722
SymbolicName=RPC_S_SERVER_UNAVAILABLE
Language=ENU
The RPC server is unavailable.
.
MessageId=1723
SymbolicName=RPC_S_SERVER_TOO_BUSY
Language=ENU
The RPC server is too busy to complete this operation.
.
MessageId=1724
SymbolicName=RPC_S_INVALID_NETWORK_OPTIONS
Language=ENU
The network options are invalid.
.
MessageId=1725
SymbolicName=RPC_S_NO_CALL_ACTIVE
Language=ENU
There are no remote procedure calls active on this thread.
.
MessageId=1726
SymbolicName=RPC_S_CALL_FAILED
Language=ENU
The remote procedure call failed.
.
MessageId=1727
SymbolicName=RPC_S_CALL_FAILED_DNE
Language=ENU
The remote procedure call failed and did not execute.
.
MessageId=1728
SymbolicName=RPC_S_PROTOCOL_ERROR
Language=ENU
A remote procedure call (RPC) protocol error occurred.
.
MessageId=1729
SymbolicName=RPC_S_PROXY_ACCESS_DENIED
Language=ENU
Access to the HTTP proxy is denied.
.
MessageId=1730
SymbolicName=RPC_S_UNSUPPORTED_TRANS_SYN
Language=ENU
The transfer syntax is not supported by the RPC server.
.
MessageId=1732
SymbolicName=RPC_S_UNSUPPORTED_TYPE
Language=ENU
The universal unique identifier (UUID) type is not supported.
.
MessageId=1733
SymbolicName=RPC_S_INVALID_TAG
Language=ENU
The tag is invalid.
.
MessageId=1734
SymbolicName=RPC_S_INVALID_BOUND
Language=ENU
The array bounds are invalid.
.
MessageId=1735
SymbolicName=RPC_S_NO_ENTRY_NAME
Language=ENU
The binding does not contain an entry name.
.
MessageId=1736
SymbolicName=RPC_S_INVALID_NAME_SYNTAX
Language=ENU
The name syntax is invalid.
.
MessageId=1737
SymbolicName=RPC_S_UNSUPPORTED_NAME_SYNTAX
Language=ENU
The name syntax is not supported.
.
MessageId=1739
SymbolicName=RPC_S_UUID_NO_ADDRESS
Language=ENU
No network address is available to use to construct a universal unique identifier (UUID).
.
MessageId=1740
SymbolicName=RPC_S_DUPLICATE_ENDPOINT
Language=ENU
The endpoint is a duplicate.
.
MessageId=1741
SymbolicName=RPC_S_UNKNOWN_AUTHN_TYPE
Language=ENU
The authentication type is unknown.
.
MessageId=1742
SymbolicName=RPC_S_MAX_CALLS_TOO_SMALL
Language=ENU
The maximum number of calls is too small.
.
MessageId=1743
SymbolicName=RPC_S_STRING_TOO_LONG
Language=ENU
The string is too long.
.
MessageId=1744
SymbolicName=RPC_S_PROTSEQ_NOT_FOUND
Language=ENU
The RPC protocol sequence was not found.
.
MessageId=1745
SymbolicName=RPC_S_PROCNUM_OUT_OF_RANGE
Language=ENU
The procedure number is out of range.
.
MessageId=1746
SymbolicName=RPC_S_BINDING_HAS_NO_AUTH
Language=ENU
The binding does not contain any authentication information.
.
MessageId=1747
SymbolicName=RPC_S_UNKNOWN_AUTHN_SERVICE
Language=ENU
The authentication service is unknown.
.
MessageId=1748
SymbolicName=RPC_S_UNKNOWN_AUTHN_LEVEL
Language=ENU
The authentication level is unknown.
.
MessageId=1749
SymbolicName=RPC_S_INVALID_AUTH_IDENTITY
Language=ENU
The security context is invalid.
.
MessageId=1750
SymbolicName=RPC_S_UNKNOWN_AUTHZ_SERVICE
Language=ENU
The authorization service is unknown.
.
MessageId=1751
SymbolicName=EPT_S_INVALID_ENTRY
Language=ENU
The entry is invalid.
.
MessageId=1752
SymbolicName=EPT_S_CANT_PERFORM_OP
Language=ENU
The server endpoint cannot perform the operation.
.
MessageId=1753
SymbolicName=EPT_S_NOT_REGISTERED
Language=ENU
There are no more endpoints available from the endpoint mapper.
.
MessageId=1754
SymbolicName=RPC_S_NOTHING_TO_EXPORT
Language=ENU
No interfaces have been exported.
.
MessageId=1755
SymbolicName=RPC_S_INCOMPLETE_NAME
Language=ENU
The entry name is incomplete.
.
MessageId=1756
SymbolicName=RPC_S_INVALID_VERS_OPTION
Language=ENU
The version option is invalid.
.
MessageId=1757
SymbolicName=RPC_S_NO_MORE_MEMBERS
Language=ENU
There are no more members.
.
MessageId=1758
SymbolicName=RPC_S_NOT_ALL_OBJS_UNEXPORTED
Language=ENU
There is nothing to unexport.
.
MessageId=1759
SymbolicName=RPC_S_INTERFACE_NOT_FOUND
Language=ENU
The interface was not found.
.
MessageId=1760
SymbolicName=RPC_S_ENTRY_ALREADY_EXISTS
Language=ENU
The entry already exists.
.
MessageId=1761
SymbolicName=RPC_S_ENTRY_NOT_FOUND
Language=ENU
The entry is not found.
.
MessageId=1762
SymbolicName=RPC_S_NAME_SERVICE_UNAVAILABLE
Language=ENU
The name service is unavailable.
.
MessageId=1763
SymbolicName=RPC_S_INVALID_NAF_ID
Language=ENU
The network address family is invalid.
.
MessageId=1764
SymbolicName=RPC_S_CANNOT_SUPPORT
Language=ENU
The requested operation is not supported.
.
MessageId=1765
SymbolicName=RPC_S_NO_CONTEXT_AVAILABLE
Language=ENU
No security context is available to allow impersonation.
.
MessageId=1766
SymbolicName=RPC_S_INTERNAL_ERROR
Language=ENU
An internal error occurred in a remote procedure call (RPC).
.
MessageId=1767
SymbolicName=RPC_S_ZERO_DIVIDE
Language=ENU
The RPC server attempted an integer division by zero.
.
MessageId=1768
SymbolicName=RPC_S_ADDRESS_ERROR
Language=ENU
An addressing error occurred in the RPC server.
.
MessageId=1769
SymbolicName=RPC_S_FP_DIV_ZERO
Language=ENU
A floating-point operation at the RPC server caused a division by zero.
.
MessageId=1770
SymbolicName=RPC_S_FP_UNDERFLOW
Language=ENU
A floating-point underflow occurred at the RPC server.
.
MessageId=1771
SymbolicName=RPC_S_FP_OVERFLOW
Language=ENU
A floating-point overflow occurred at the RPC server.
.
MessageId=1772
SymbolicName=RPC_X_NO_MORE_ENTRIES
Language=ENU
The list of RPC servers available for the binding of auto handles has been exhausted.
.
MessageId=1773
SymbolicName=RPC_X_SS_CHAR_TRANS_OPEN_FAIL
Language=ENU
Unable to open the character translation table file.
.
MessageId=1774
SymbolicName=RPC_X_SS_CHAR_TRANS_SHORT_FILE
Language=ENU
The file containing the character translation table has fewer than 512 bytes.
.
MessageId=1775
SymbolicName=RPC_X_SS_IN_NULL_CONTEXT
Language=ENU
A null context handle was passed from the client to the host during a remote procedure call.
.
MessageId=1777
SymbolicName=RPC_X_SS_CONTEXT_DAMAGED
Language=ENU
The context handle changed during a remote procedure call.
.
MessageId=1778
SymbolicName=RPC_X_SS_HANDLES_MISMATCH
Language=ENU
The binding handles passed to a remote procedure call do not match.
.
MessageId=1779
SymbolicName=RPC_X_SS_CANNOT_GET_CALL_HANDLE
Language=ENU
The stub is unable to get the remote procedure call handle.
.
MessageId=1780
SymbolicName=RPC_X_NULL_REF_POINTER
Language=ENU
A null reference pointer was passed to the stub.
.
MessageId=1781
SymbolicName=RPC_X_ENUM_VALUE_OUT_OF_RANGE
Language=ENU
The enumeration value is out of range.
.
MessageId=1782
SymbolicName=RPC_X_BYTE_COUNT_TOO_SMALL
Language=ENU
The byte count is too small.
.
MessageId=1783
SymbolicName=RPC_X_BAD_STUB_DATA
Language=ENU
The stub received bad data.
.
MessageId=1784
SymbolicName=ERROR_INVALID_USER_BUFFER
Language=ENU
The supplied user buffer is not valid for the requested operation.
.
MessageId=1785
SymbolicName=ERROR_UNRECOGNIZED_MEDIA
Language=ENU
The disk media is not recognized. It may not be formatted.
.
MessageId=1786
SymbolicName=ERROR_NO_TRUST_LSA_SECRET
Language=ENU
The workstation does not have a trust secret.
.
MessageId=1787
SymbolicName=ERROR_NO_TRUST_SAM_ACCOUNT
Language=ENU
The security database on the server does not have a computer account for this workstation trust relationship.
.
MessageId=1788
SymbolicName=ERROR_TRUSTED_DOMAIN_FAILURE
Language=ENU
The trust relationship between the primary domain and the trusted domain failed.
.
MessageId=1789
SymbolicName=ERROR_TRUSTED_RELATIONSHIP_FAILURE
Language=ENU
The trust relationship between this workstation and the primary domain failed.
.
MessageId=1790
SymbolicName=ERROR_TRUST_FAILURE
Language=ENU
The network logon failed.
.
MessageId=1791
SymbolicName=RPC_S_CALL_IN_PROGRESS
Language=ENU
A remote procedure call is already in progress for this thread.
.
MessageId=1792
SymbolicName=ERROR_NETLOGON_NOT_STARTED
Language=ENU
An attempt was made to logon, but the network logon service was not started.
.
MessageId=1793
SymbolicName=ERROR_ACCOUNT_EXPIRED
Language=ENU
The user's account has expired.
.
MessageId=1794
SymbolicName=ERROR_REDIRECTOR_HAS_OPEN_HANDLES
Language=ENU
The redirector is in use and cannot be unloaded.
.
MessageId=1795
SymbolicName=ERROR_PRINTER_DRIVER_ALREADY_INSTALLED
Language=ENU
The specified printer driver is already installed.
.
MessageId=1796
SymbolicName=ERROR_UNKNOWN_PORT
Language=ENU
The specified port is unknown.
.
MessageId=1797
SymbolicName=ERROR_UNKNOWN_PRINTER_DRIVER
Language=ENU
The printer driver is unknown.
.
MessageId=1798
SymbolicName=ERROR_UNKNOWN_PRINTPROCESSOR
Language=ENU
The print processor is unknown.
.
MessageId=1799
SymbolicName=ERROR_INVALID_SEPARATOR_FILE
Language=ENU
The specified separator file is invalid.
.
MessageId=1800
SymbolicName=ERROR_INVALID_PRIORITY
Language=ENU
The specified priority is invalid.
.
MessageId=1801
SymbolicName=ERROR_INVALID_PRINTER_NAME
Language=ENU
The printer name is invalid.
.
MessageId=1802
SymbolicName=ERROR_PRINTER_ALREADY_EXISTS
Language=ENU
The printer already exists.
.
MessageId=1803
SymbolicName=ERROR_INVALID_PRINTER_COMMAND
Language=ENU
The printer command is invalid.
.
MessageId=1804
SymbolicName=ERROR_INVALID_DATATYPE
Language=ENU
The specified datatype is invalid.
.
MessageId=1805
SymbolicName=ERROR_INVALID_ENVIRONMENT
Language=ENU
The environment specified is invalid.
.
MessageId=1806
SymbolicName=RPC_S_NO_MORE_BINDINGS
Language=ENU
There are no more bindings.
.
MessageId=1807
SymbolicName=ERROR_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT
Language=ENU
The account used is an interdomain trust account. Use your global user account or local user account to access this server.
.
MessageId=1808
SymbolicName=ERROR_NOLOGON_WORKSTATION_TRUST_ACCOUNT
Language=ENU
The account used is a computer account. Use your global user account or local user account to access this server.
.
MessageId=1809
SymbolicName=ERROR_NOLOGON_SERVER_TRUST_ACCOUNT
Language=ENU
The account used is a server trust account. Use your global user account or local user account to access this server.
.
MessageId=1810
SymbolicName=ERROR_DOMAIN_TRUST_INCONSISTENT
Language=ENU
The name or security ID (SID) of the domain specified is inconsistent with the trust information for that domain.
.
MessageId=1811
SymbolicName=ERROR_SERVER_HAS_OPEN_HANDLES
Language=ENU
The server is in use and cannot be unloaded.
.
MessageId=1812
SymbolicName=ERROR_RESOURCE_DATA_NOT_FOUND
Language=ENU
The specified image file did not contain a resource section.
.
MessageId=1813
SymbolicName=ERROR_RESOURCE_TYPE_NOT_FOUND
Language=ENU
The specified resource type cannot be found in the image file.
.
MessageId=1814
SymbolicName=ERROR_RESOURCE_NAME_NOT_FOUND
Language=ENU
The specified resource name cannot be found in the image file.
.
MessageId=1815
SymbolicName=ERROR_RESOURCE_LANG_NOT_FOUND
Language=ENU
The specified resource language ID cannot be found in the image file.
.
MessageId=1816
SymbolicName=ERROR_NOT_ENOUGH_QUOTA
Language=ENU
Not enough quota is available to process this command.
.
MessageId=1817
SymbolicName=RPC_S_NO_INTERFACES
Language=ENU
No interfaces have been registered.
.
MessageId=1818
SymbolicName=RPC_S_CALL_CANCELLED
Language=ENU
The remote procedure call was cancelled.
.
MessageId=1819
SymbolicName=RPC_S_BINDING_INCOMPLETE
Language=ENU
The binding handle does not contain all required information.
.
MessageId=1820
SymbolicName=RPC_S_COMM_FAILURE
Language=ENU
A communications failure occurred during a remote procedure call.
.
MessageId=1821
SymbolicName=RPC_S_UNSUPPORTED_AUTHN_LEVEL
Language=ENU
The requested authentication level is not supported.
.
MessageId=1822
SymbolicName=RPC_S_NO_PRINC_NAME
Language=ENU
No principal name registered.
.
MessageId=1823
SymbolicName=RPC_S_NOT_RPC_ERROR
Language=ENU
The error specified is not a valid Windows RPC error code.
.
MessageId=1824
SymbolicName=RPC_S_UUID_LOCAL_ONLY
Language=ENU
A UUID that is valid only on this computer has been allocated.
.
MessageId=1825
SymbolicName=RPC_S_SEC_PKG_ERROR
Language=ENU
A security package specific error occurred.
.
MessageId=1826
SymbolicName=RPC_S_NOT_CANCELLED
Language=ENU
Thread is not canceled.
.
MessageId=1827
SymbolicName=RPC_X_INVALID_ES_ACTION
Language=ENU
Invalid operation on the encoding/decoding handle.
.
MessageId=1828
SymbolicName=RPC_X_WRONG_ES_VERSION
Language=ENU
Incompatible version of the serializing package.
.
MessageId=1829
SymbolicName=RPC_X_WRONG_STUB_VERSION
Language=ENU
Incompatible version of the RPC stub.
.
MessageId=1830
SymbolicName=RPC_X_INVALID_PIPE_OBJECT
Language=ENU
The RPC pipe object is invalid or corrupted.
.
MessageId=1831
SymbolicName=RPC_X_WRONG_PIPE_ORDER
Language=ENU
An invalid operation was attempted on an RPC pipe object.
.
MessageId=1832
SymbolicName=RPC_X_WRONG_PIPE_VERSION
Language=ENU
Unsupported RPC pipe version.
.
MessageId=1833
SymbolicName=RPC_S_COOKIE_AUTH_FAILED
Language=ENU
HTTP proxy server rejected the connection because the cookie authentication failed.
.
MessageId=1834
SymbolicName=RPC_S_DO_NOT_DISTURB
Language=ENU
The RPC server is suspended, and could not be resumed for this request. The call did not execute.
.
MessageId=1835
SymbolicName=RPC_S_SYSTEM_HANDLE_COUNT_EXCEEDED
Language=ENU
The RPC call contains too many handles to be transmitted in a single request.
.
MessageId=1836
SymbolicName=RPC_S_SYSTEM_HANDLE_TYPE_MISMATCH
Language=ENU
The RPC call contains a handle that differs from the declared handle type.
.
MessageId=1898
SymbolicName=RPC_S_GROUP_MEMBER_NOT_FOUND
Language=ENU
The group member was not found.
.
MessageId=1899
SymbolicName=EPT_S_CANT_CREATE
Language=ENU
The endpoint mapper database entry could not be created.
.
MessageId=1900
SymbolicName=RPC_S_INVALID_OBJECT
Language=ENU
The object universal unique identifier (UUID) is the nil UUID.
.
MessageId=1901
SymbolicName=ERROR_INVALID_TIME
Language=ENU
The specified time is invalid.
.
MessageId=1902
SymbolicName=ERROR_INVALID_FORM_NAME
Language=ENU
The specified form name is invalid.
.
MessageId=1903
SymbolicName=ERROR_INVALID_FORM_SIZE
Language=ENU
The specified form size is invalid.
.
MessageId=1904
SymbolicName=ERROR_ALREADY_WAITING
Language=ENU
The specified printer handle is already being waited on
.
MessageId=1905
SymbolicName=ERROR_PRINTER_DELETED
Language=ENU
The specified printer has been deleted.
.
MessageId=1906
SymbolicName=ERROR_INVALID_PRINTER_STATE
Language=ENU
The state of the printer is invalid.
.
MessageId=1907
SymbolicName=ERROR_PASSWORD_MUST_CHANGE
Language=ENU
The user's password must be changed before signing in.
.
MessageId=1908
SymbolicName=ERROR_DOMAIN_CONTROLLER_NOT_FOUND
Language=ENU
Could not find the domain controller for this domain.
.
MessageId=1909
SymbolicName=ERROR_ACCOUNT_LOCKED_OUT
Language=ENU
The referenced account is currently locked out and may not be logged on to.
.
MessageId=1910
SymbolicName=OR_INVALID_OXID
Language=ENU
The object exporter specified was not found.
.
MessageId=1911
SymbolicName=OR_INVALID_OID
Language=ENU
The object specified was not found.
.
MessageId=1912
SymbolicName=OR_INVALID_SET
Language=ENU
The object resolver set specified was not found.
.
MessageId=1913
SymbolicName=RPC_S_SEND_INCOMPLETE
Language=ENU
Some data remains to be sent in the request buffer.
.
MessageId=1914
SymbolicName=RPC_S_INVALID_ASYNC_HANDLE
Language=ENU
Invalid asynchronous remote procedure call handle.
.
MessageId=1915
SymbolicName=RPC_S_INVALID_ASYNC_CALL
Language=ENU
Invalid asynchronous RPC call handle for this operation.
.
MessageId=1916
SymbolicName=RPC_X_PIPE_CLOSED
Language=ENU
The RPC pipe object has already been closed.
.
MessageId=1917
SymbolicName=RPC_X_PIPE_DISCIPLINE_ERROR
Language=ENU
The RPC call completed before all pipes were processed.
.
MessageId=1918
SymbolicName=RPC_X_PIPE_EMPTY
Language=ENU
No more data is available from the RPC pipe.
.
MessageId=1919
SymbolicName=ERROR_NO_SITENAME
Language=ENU
No site name is available for this machine.
.
MessageId=1920
SymbolicName=ERROR_CANT_ACCESS_FILE
Language=ENU
The file cannot be accessed by the system.
.
MessageId=1921
SymbolicName=ERROR_CANT_RESOLVE_FILENAME
Language=ENU
The name of the file cannot be resolved by the system.
.
MessageId=1922
SymbolicName=RPC_S_ENTRY_TYPE_MISMATCH
Language=ENU
The entry is not of the expected type.
.
MessageId=1923
SymbolicName=RPC_S_NOT_ALL_OBJS_EXPORTED
Language=ENU
Not all object UUIDs could be exported to the specified entry.
.
MessageId=1924
SymbolicName=RPC_S_INTERFACE_NOT_EXPORTED
Language=ENU
Interface could not be exported to the specified entry.
.
MessageId=1925
SymbolicName=RPC_S_PROFILE_NOT_ADDED
Language=ENU
The specified profile entry could not be added.
.
MessageId=1926
SymbolicName=RPC_S_PRF_ELT_NOT_ADDED
Language=ENU
The specified profile element could not be added.
.
MessageId=1927
SymbolicName=RPC_S_PRF_ELT_NOT_REMOVED
Language=ENU
The specified profile element could not be removed.
.
MessageId=1928
SymbolicName=RPC_S_GRP_ELT_NOT_ADDED
Language=ENU
The group element could not be added.
.
MessageId=1929
SymbolicName=RPC_S_GRP_ELT_NOT_REMOVED
Language=ENU
The group element could not be removed.
.
MessageId=1930
SymbolicName=ERROR_KM_DRIVER_BLOCKED
Language=ENU
The printer driver is not compatible with a policy enabled on your computer that blocks NT 4.0 drivers.
.
MessageId=1931
SymbolicName=ERROR_CONTEXT_EXPIRED
Language=ENU
The context has expired and can no longer be used.
.
MessageId=1932
SymbolicName=ERROR_PER_USER_TRUST_QUOTA_EXCEEDED
Language=ENU
The current user's delegated trust creation quota has been exceeded.
.
MessageId=1933
SymbolicName=ERROR_ALL_USER_TRUST_QUOTA_EXCEEDED
Language=ENU
The total delegated trust creation quota has been exceeded.
.
MessageId=1934
SymbolicName=ERROR_USER_DELETE_TRUST_QUOTA_EXCEEDED
Language=ENU
The current user's delegated trust deletion quota has been exceeded.
.
MessageId=1935
SymbolicName=ERROR_AUTHENTICATION_FIREWALL_FAILED
Language=ENU
The computer you are signing into is protected by an authentication firewall. The specified account is not allowed to authenticate to the computer.
.
MessageId=1936
SymbolicName=ERROR_REMOTE_PRINT_CONNECTIONS_BLOCKED
Language=ENU
Remote connections to the Print Spooler are blocked by a policy set on your machine.
.
MessageId=1937
SymbolicName=ERROR_NTLM_BLOCKED
Language=ENU
Authentication failed because NTLM authentication has been disabled.
.
MessageId=1938
SymbolicName=ERROR_PASSWORD_CHANGE_REQUIRED
Language=ENU
Logon Failure: EAS policy requires that the user change their password before this operation can be performed.
.
MessageId=1939
SymbolicName=ERROR_LOST_MODE_LOGON_RESTRICTION
Language=ENU
An administrator has restricted sign in. To sign in, make sure your device is connected to the Internet, and have your administrator sign in first.
.
MessageId=2000
SymbolicName=ERROR_INVALID_PIXEL_FORMAT
Language=ENU
The pixel format is invalid.
.
MessageId=2001
SymbolicName=ERROR_BAD_DRIVER
Language=ENU
The specified driver is invalid.
.
MessageId=2002
SymbolicName=ERROR_INVALID_WINDOW_STYLE
Language=ENU
The window style or class attribute is invalid for this operation.
.
MessageId=2003
SymbolicName=ERROR_METAFILE_NOT_SUPPORTED
Language=ENU
The requested metafile operation is not supported.
.
MessageId=2004
SymbolicName=ERROR_TRANSFORM_NOT_SUPPORTED
Language=ENU
The requested transformation operation is not supported.
.
MessageId=2005
SymbolicName=ERROR_CLIPPING_NOT_SUPPORTED
Language=ENU
The requested clipping operation is not supported.
.
MessageId=2010
SymbolicName=ERROR_INVALID_CMM
Language=ENU
The specified color management module is invalid.
.
MessageId=2011
SymbolicName=ERROR_INVALID_PROFILE
Language=ENU
The specified color profile is invalid.
.
MessageId=2012
SymbolicName=ERROR_TAG_NOT_FOUND
Language=ENU
The specified tag was not found.
.
MessageId=2013
SymbolicName=ERROR_TAG_NOT_PRESENT
Language=ENU
A required tag is not present.
.
MessageId=2014
SymbolicName=ERROR_DUPLICATE_TAG
Language=ENU
The specified tag is already present.
.
MessageId=2015
SymbolicName=ERROR_PROFILE_NOT_ASSOCIATED_WITH_DEVICE
Language=ENU
The specified color profile is not associated with the specified device.
.
MessageId=2016
SymbolicName=ERROR_PROFILE_NOT_FOUND
Language=ENU
The specified color profile was not found.
.
MessageId=2017
SymbolicName=ERROR_INVALID_COLORSPACE
Language=ENU
The specified color space is invalid.
.
MessageId=2018
SymbolicName=ERROR_ICM_NOT_ENABLED
Language=ENU
Image Color Management is not enabled.
.
MessageId=2019
SymbolicName=ERROR_DELETING_ICM_XFORM
Language=ENU
There was an error while deleting the color transform.
.
MessageId=2020
SymbolicName=ERROR_INVALID_TRANSFORM
Language=ENU
The specified color transform is invalid.
.
MessageId=2021
SymbolicName=ERROR_COLORSPACE_MISMATCH
Language=ENU
The specified transform does not match the bitmap's color space.
.
MessageId=2022
SymbolicName=ERROR_INVALID_COLORINDEX
Language=ENU
The specified named color index is not present in the profile.
.
MessageId=2023
SymbolicName=ERROR_PROFILE_DOES_NOT_MATCH_DEVICE
Language=ENU
The specified profile is intended for a device of a different type than the specified device.
.
MessageId=2108
SymbolicName=ERROR_CONNECTED_OTHER_PASSWORD
Language=ENU
The network connection was made successfully, but the user had to be prompted for a password other than the one originally specified.
.
MessageId=2109
SymbolicName=ERROR_CONNECTED_OTHER_PASSWORD_DEFAULT
Language=ENU
The network connection was made successfully using default credentials.
.
MessageId=2202
SymbolicName=ERROR_BAD_USERNAME
Language=ENU
The specified username is invalid.
.
MessageId=2221
SymbolicName=NERR_UserNotFound
Language=ENU
The username could not be found.
.
MessageId=2250
SymbolicName=ERROR_NOT_CONNECTED
Language=ENU
This network connection does not exist.
.
MessageId=2401
SymbolicName=ERROR_OPEN_FILES
Language=ENU
This network connection has files open or requests pending.
.
MessageId=2402
SymbolicName=ERROR_ACTIVE_CONNECTIONS
Language=ENU
Active connections still exist.
.
MessageId=2404
SymbolicName=ERROR_DEVICE_IN_USE
Language=ENU
The device is in use by an active process and cannot be disconnected.
.
MessageId=3000
SymbolicName=ERROR_UNKNOWN_PRINT_MONITOR
Language=ENU
The specified print monitor is unknown.
.
MessageId=3001
SymbolicName=ERROR_PRINTER_DRIVER_IN_USE
Language=ENU
The specified printer driver is currently in use.
.
MessageId=3002
SymbolicName=ERROR_SPOOL_FILE_NOT_FOUND
Language=ENU
The spool file was not found.
.
MessageId=3003
SymbolicName=ERROR_SPL_NO_STARTDOC
Language=ENU
A StartDocPrinter call was not issued.
.
MessageId=3004
SymbolicName=ERROR_SPL_NO_ADDJOB
Language=ENU
An AddJob call was not issued.
.
MessageId=3005
SymbolicName=ERROR_PRINT_PROCESSOR_ALREADY_INSTALLED
Language=ENU
The specified print processor has already been installed.
.
MessageId=3006
SymbolicName=ERROR_PRINT_MONITOR_ALREADY_INSTALLED
Language=ENU
The specified print monitor has already been installed.
.
MessageId=3007
SymbolicName=ERROR_INVALID_PRINT_MONITOR
Language=ENU
The specified print monitor does not have the required functions.
.
MessageId=3008
SymbolicName=ERROR_PRINT_MONITOR_IN_USE
Language=ENU
The specified print monitor is currently in use.
.
MessageId=3009
SymbolicName=ERROR_PRINTER_HAS_JOBS_QUEUED
Language=ENU
The requested operation is not allowed when there are jobs queued to the printer.
.
MessageId=3010
SymbolicName=ERROR_SUCCESS_REBOOT_REQUIRED
Language=ENU
The requested operation is successful. Changes will not be effective until the system is rebooted.
.
MessageId=3011
SymbolicName=ERROR_SUCCESS_RESTART_REQUIRED
Language=ENU
The requested operation is successful. Changes will not be effective until the service is restarted.
.
MessageId=3012
SymbolicName=ERROR_PRINTER_NOT_FOUND
Language=ENU
No printers were found.
.
MessageId=3013
SymbolicName=ERROR_PRINTER_DRIVER_WARNED
Language=ENU
The printer driver is known to be unreliable.
.
MessageId=3014
SymbolicName=ERROR_PRINTER_DRIVER_BLOCKED
Language=ENU
The printer driver is known to harm the system.
.
MessageId=3015
SymbolicName=ERROR_PRINTER_DRIVER_PACKAGE_IN_USE
Language=ENU
The specified printer driver package is currently in use.
.
MessageId=3016
SymbolicName=ERROR_CORE_DRIVER_PACKAGE_NOT_FOUND
Language=ENU
Unable to find a core driver package that is required by the printer driver package.
.
MessageId=3017
SymbolicName=ERROR_FAIL_REBOOT_REQUIRED
Language=ENU
The requested operation failed. A system reboot is required to roll back changes made.
.
MessageId=3018
SymbolicName=ERROR_FAIL_REBOOT_INITIATED
Language=ENU
The requested operation failed. A system reboot has been initiated to roll back changes made.
.
MessageId=3019
SymbolicName=ERROR_PRINTER_DRIVER_DOWNLOAD_NEEDED
Language=ENU
The specified printer driver was not found on the system and needs to be downloaded.
.
MessageId=3020
SymbolicName=ERROR_PRINT_JOB_RESTART_REQUIRED
Language=ENU
The requested print job has failed to print. A print system update requires the job to be resubmitted.
.
MessageId=3021
SymbolicName=ERROR_INVALID_PRINTER_DRIVER_MANIFEST
Language=ENU
The printer driver does not contain a valid manifest, or contains too many manifests.
.
MessageId=3022
SymbolicName=ERROR_PRINTER_NOT_SHAREABLE
Language=ENU
The specified printer cannot be shared.
.
MessageId=3023
SymbolicName=ERROR_SERVER_SERVICE_CALL_REQUIRES_SMB1
Language=ENU
The requested function requires SMB1 to be present and enabled.
.
MessageId=3024
SymbolicName=ERROR_NETWORK_AUTHENTICATION_PROMPT_CANCELED
Language=ENU
The user canceled the authentication prompt to a remote server.
.
MessageId=3025
SymbolicName=WINERR_3025
Language=ENU
The requested operation failed. Remote mailslots have been deprecated.
.
MessageId=3050
SymbolicName=ERROR_REQUEST_PAUSED
Language=ENU
The operation was paused.
.
MessageId=3060
SymbolicName=ERROR_APPEXEC_CONDITION_NOT_SATISFIED
Language=ENU
The condition supplied for the app execution request was not satisfied, so the request was not performed.
.
MessageId=3061
SymbolicName=ERROR_APPEXEC_HANDLE_INVALIDATED
Language=ENU
The supplied handle has been invalidated and may not be used for the requested operation.
.
MessageId=3062
SymbolicName=ERROR_APPEXEC_INVALID_HOST_GENERATION
Language=ENU
The supplied host generation has been invalidated and may not be used for the requested operation.
.
MessageId=3063
SymbolicName=ERROR_APPEXEC_UNEXPECTED_PROCESS_REGISTRATION
Language=ENU
An attempt to register a process failed because the target host was not in a valid state to receive process registrations.
.
MessageId=3064
SymbolicName=ERROR_APPEXEC_INVALID_HOST_STATE
Language=ENU
The host is not in a valid state to support the execution request.
.
MessageId=3065
SymbolicName=ERROR_APPEXEC_NO_DONOR
Language=ENU
The operation was not completed because a required resource donor was not found for the host.
.
MessageId=3066
SymbolicName=ERROR_APPEXEC_HOST_ID_MISMATCH
Language=ENU
The operation was not completed because an unexpected host ID was encountered.
.
MessageId=3067
SymbolicName=ERROR_APPEXEC_UNKNOWN_USER
Language=ENU
The operation was not completed because the specified user was not known to the service.
.
MessageId=3068
SymbolicName=ERROR_APPEXEC_APP_COMPAT_BLOCK
Language=ENU
The application is blocked by app compat policy.
.
MessageId=3069
SymbolicName=ERROR_APPEXEC_CALLER_WAIT_TIMEOUT
Language=ENU
The caller specified wait timed out before the operation completed.
.
MessageId=3070
SymbolicName=ERROR_APPEXEC_CALLER_WAIT_TIMEOUT_TERMINATION
Language=ENU
The caller specified wait timed out before the operation completed because a host termination is in queued.
.
MessageId=3071
SymbolicName=ERROR_APPEXEC_CALLER_WAIT_TIMEOUT_LICENSING
Language=ENU
The caller specified wait timed out before the operation completed because a licensing operation is being performed.
.
MessageId=3072
SymbolicName=ERROR_APPEXEC_CALLER_WAIT_TIMEOUT_RESOURCES
Language=ENU
The caller specified wait timed out before the operation completed because resources are being acquired.
.
MessageId=3080
SymbolicName=ERROR_VRF_VOLATILE_CFG_AND_IO_ENABLED
Language=ENU
Enabling driver verification from volatile command is currently not supported when both CFG and IO are enabled.
.
MessageId=3081
SymbolicName=ERROR_VRF_VOLATILE_NOT_STOPPABLE
Language=ENU
Removal of current driver verification is not supported from volatile command.
.
MessageId=3082
SymbolicName=ERROR_VRF_VOLATILE_SAFE_MODE
Language=ENU
Enabling driver verification is not supported in safe mode.
.
MessageId=3083
SymbolicName=ERROR_VRF_VOLATILE_NOT_RUNNABLE_SYSTEM
Language=ENU
Enabling driver verification is not supported from volatile mode in current system.
.
MessageId=3084
SymbolicName=ERROR_VRF_VOLATILE_NOT_SUPPORTED_RULECLASS
Language=ENU
The specified rule class (a.k.a. flag) is not supported from volatile mode.
.
MessageId=3085
SymbolicName=ERROR_VRF_VOLATILE_PROTECTED_DRIVER
Language=ENU
The specified driver is protected and volatile verification is currently not supported.
.
MessageId=3086
SymbolicName=ERROR_VRF_VOLATILE_NMI_REGISTERED
Language=ENU
Enabling driver verification is not supported for a driver with  NMI callback(s) registered.
.
MessageId=3087
SymbolicName=ERROR_VRF_VOLATILE_SETTINGS_CONFLICT
Language=ENU
Volatile verification settings cannot be changed when verification is enabled from boot or DIF volatile verification is enabled.
.
MessageId=3088
SymbolicName=WINERR_3088
Language=ENU
Live memory dump generation is in progress.
.
MessageId=3187
SymbolicName=WINERR_3187
Language=ENU
There is no captured information.
.
MessageId=3188
SymbolicName=WINERR_3188
Language=ENU
The given rule class ID is not registered by the caller.
.
MessageId=3189
SymbolicName=WINERR_3189
Language=ENU
Driver thunks are not added when kernel verification is in effect.
.
MessageId=3190
SymbolicName=ERROR_DIF_IOCALLBACK_NOT_REPLACED
Language=ENU
The specified driver is not associated with driver object or driver extension.
.
MessageId=3191
SymbolicName=ERROR_DIF_LIVEDUMP_LIMIT_EXCEEDED
Language=ENU
Verifier's internal data size exceeds the limit of live dump secondary data.
.
MessageId=3192
SymbolicName=ERROR_DIF_VOLATILE_SECTION_NOT_LOCKED
Language=ENU
Verification cannot start because an attempt to lock code or data section failed.
.
MessageId=3193
SymbolicName=ERROR_DIF_VOLATILE_DRIVER_HOTPATCHED
Language=ENU
DIF volatile verification is not supported for hotpatched driver.
.
MessageId=3194
SymbolicName=ERROR_DIF_VOLATILE_INVALID_INFO
Language=ENU
The passed system DIF information is invalid.
.
MessageId=3195
SymbolicName=ERROR_DIF_VOLATILE_DRIVER_IS_NOT_RUNNING
Language=ENU
DIF volatile verification only supports on loaded drivers.
.
MessageId=3196
SymbolicName=ERROR_DIF_VOLATILE_PLUGIN_IS_NOT_RUNNING
Language=ENU
Currently no plugin is running.
.
MessageId=3197
SymbolicName=ERROR_DIF_VOLATILE_PLUGIN_CHANGE_NOT_ALLOWED
Language=ENU
Currently running plugin must be removed before applying a new plugin.
.
MessageId=3198
SymbolicName=ERROR_DIF_VOLATILE_NOT_ALLOWED
Language=ENU
The plugin is not allowed to run in volatile mode.
.
MessageId=3199
SymbolicName=ERROR_DIF_BINDING_API_NOT_FOUND
Language=ENU
One or more DDI is not yet supported by DIF.
.
MessageId=3950
SymbolicName=ERROR_IO_REISSUE_AS_CACHED
Language=ENU
Reissue the given operation as a cached IO operation
.
MessageId=4000
SymbolicName=ERROR_WINS_INTERNAL
Language=ENU
WINS encountered an error while processing the command.
.
MessageId=4001
SymbolicName=ERROR_CAN_NOT_DEL_LOCAL_WINS
Language=ENU
The local WINS cannot be deleted.
.
MessageId=4002
SymbolicName=ERROR_STATIC_INIT
Language=ENU
The importation from the file failed.
.
MessageId=4003
SymbolicName=ERROR_INC_BACKUP
Language=ENU
The backup failed. Was a full backup done before?
.
MessageId=4004
SymbolicName=ERROR_FULL_BACKUP
Language=ENU
The backup failed. Check the directory to which you are backing the database.
.
MessageId=4005
SymbolicName=ERROR_REC_NON_EXISTENT
Language=ENU
The name does not exist in the WINS database.
.
MessageId=4006
SymbolicName=ERROR_RPL_NOT_ALLOWED
Language=ENU
Replication with a nonconfigured partner is not allowed.
.
MessageId=4050
SymbolicName=WINERR_4050
Language=ENU
The version of the supplied content information is not supported.
.
MessageId=4051
SymbolicName=WINERR_4051
Language=ENU
The supplied content information is malformed.
.
MessageId=4052
SymbolicName=WINERR_4052
Language=ENU
The requested data cannot be found in local or peer caches.
.
MessageId=4053
SymbolicName=WINERR_4053
Language=ENU
No more data is available or required.
.
MessageId=4054
SymbolicName=WINERR_4054
Language=ENU
The supplied object has not been initialized.
.
MessageId=4055
SymbolicName=WINERR_4055
Language=ENU
The supplied object has already been initialized.
.
MessageId=4056
SymbolicName=WINERR_4056
Language=ENU
A shutdown operation is already in progress.
.
MessageId=4057
SymbolicName=WINERR_4057
Language=ENU
The supplied object has already been invalidated.
.
MessageId=4058
SymbolicName=WINERR_4058
Language=ENU
An element already exists and was not replaced.
.
MessageId=4059
SymbolicName=WINERR_4059
Language=ENU
Can not cancel the requested operation as it has already been completed.
.
MessageId=4060
SymbolicName=WINERR_4060
Language=ENU
Can not perform the requested operation because it has already been carried out.
.
MessageId=4061
SymbolicName=WINERR_4061
Language=ENU
An operation accessed data beyond the bounds of valid data.
.
MessageId=4062
SymbolicName=WINERR_4062
Language=ENU
The requested version is not supported.
.
MessageId=4063
SymbolicName=WINERR_4063
Language=ENU
A configuration value is invalid.
.
MessageId=4064
SymbolicName=WINERR_4064
Language=ENU
The SKU is not licensed.
.
MessageId=4065
SymbolicName=WINERR_4065
Language=ENU
PeerDist Service is still initializing and will be available shortly.
.
MessageId=4066
SymbolicName=WINERR_4066
Language=ENU
Communication with one or more computers will be temporarily blocked due to recent errors.
.
MessageId=4100
SymbolicName=ERROR_DHCP_ADDRESS_CONFLICT
Language=ENU
The DHCP client has obtained an IP address that is already in use on the network. The local interface will be disabled until the DHCP client can obtain a new address.
.
MessageId=4200
SymbolicName=ERROR_WMI_GUID_NOT_FOUND
Language=ENU
The GUID passed was not recognized as valid by a WMI data provider.
.
MessageId=4201
SymbolicName=ERROR_WMI_INSTANCE_NOT_FOUND
Language=ENU
The instance name passed was not recognized as valid by a WMI data provider.
.
MessageId=4202
SymbolicName=ERROR_WMI_ITEMID_NOT_FOUND
Language=ENU
The data item ID passed was not recognized as valid by a WMI data provider.
.
MessageId=4203
SymbolicName=ERROR_WMI_TRY_AGAIN
Language=ENU
The WMI request could not be completed and should be retried.
.
MessageId=4204
SymbolicName=ERROR_WMI_DP_NOT_FOUND
Language=ENU
The WMI data provider could not be located.
.
MessageId=4205
SymbolicName=ERROR_WMI_UNRESOLVED_INSTANCE_REF
Language=ENU
The WMI data provider references an instance set that has not been registered.
.
MessageId=4206
SymbolicName=ERROR_WMI_ALREADY_ENABLED
Language=ENU
The WMI data block or event notification has already been enabled.
.
MessageId=4207
SymbolicName=ERROR_WMI_GUID_DISCONNECTED
Language=ENU
The WMI data block is no longer available.
.
MessageId=4208
SymbolicName=ERROR_WMI_SERVER_UNAVAILABLE
Language=ENU
The WMI data service is not available.
.
MessageId=4209
SymbolicName=ERROR_WMI_DP_FAILED
Language=ENU
The WMI data provider failed to carry out the request.
.
MessageId=4210
SymbolicName=ERROR_WMI_INVALID_MOF
Language=ENU
The WMI MOF information is not valid.
.
MessageId=4211
SymbolicName=ERROR_WMI_INVALID_REGINFO
Language=ENU
The WMI registration information is not valid.
.
MessageId=4212
SymbolicName=ERROR_WMI_ALREADY_DISABLED
Language=ENU
The WMI data block or event notification has already been disabled.
.
MessageId=4213
SymbolicName=ERROR_WMI_READ_ONLY
Language=ENU
The WMI data item or data block is read only.
.
MessageId=4214
SymbolicName=ERROR_WMI_SET_FAILURE
Language=ENU
The WMI data item or data block could not be changed.
.
MessageId=4250
SymbolicName=ERROR_NOT_APPCONTAINER
Language=ENU
This operation is only valid in the context of an app container.
.
MessageId=4251
SymbolicName=ERROR_APPCONTAINER_REQUIRED
Language=ENU
This application can only run in the context of an app container.
.
MessageId=4252
SymbolicName=ERROR_NOT_SUPPORTED_IN_APPCONTAINER
Language=ENU
This functionality is not supported in the context of an app container.
.
MessageId=4253
SymbolicName=ERROR_INVALID_PACKAGE_SID_LENGTH
Language=ENU
The length of the SID supplied is not a valid length for app container SIDs.
.
MessageId=4300
SymbolicName=ERROR_INVALID_MEDIA
Language=ENU
The media identifier does not represent a valid medium.
.
MessageId=4301
SymbolicName=ERROR_INVALID_LIBRARY
Language=ENU
The library identifier does not represent a valid library.
.
MessageId=4302
SymbolicName=ERROR_INVALID_MEDIA_POOL
Language=ENU
The media pool identifier does not represent a valid media pool.
.
MessageId=4303
SymbolicName=ERROR_DRIVE_MEDIA_MISMATCH
Language=ENU
The drive and medium are not compatible or exist in different libraries.
.
MessageId=4304
SymbolicName=ERROR_MEDIA_OFFLINE
Language=ENU
The medium currently exists in an offline library and must be online to perform this operation.
.
MessageId=4305
SymbolicName=ERROR_LIBRARY_OFFLINE
Language=ENU
The operation cannot be performed on an offline library.
.
MessageId=4306
SymbolicName=ERROR_EMPTY
Language=ENU
The library, drive, or media pool is empty.
.
MessageId=4307
SymbolicName=ERROR_NOT_EMPTY
Language=ENU
The library, drive, or media pool must be empty to perform this operation.
.
MessageId=4308
SymbolicName=ERROR_MEDIA_UNAVAILABLE
Language=ENU
No media is currently available in this media pool or library.
.
MessageId=4309
SymbolicName=ERROR_RESOURCE_DISABLED
Language=ENU
A resource required for this operation is disabled.
.
MessageId=4310
SymbolicName=ERROR_INVALID_CLEANER
Language=ENU
The media identifier does not represent a valid cleaner.
.
MessageId=4311
SymbolicName=ERROR_UNABLE_TO_CLEAN
Language=ENU
The drive cannot be cleaned or does not support cleaning.
.
MessageId=4312
SymbolicName=ERROR_OBJECT_NOT_FOUND
Language=ENU
The object identifier does not represent a valid object.
.
MessageId=4313
SymbolicName=ERROR_DATABASE_FAILURE
Language=ENU
Unable to read from or write to the database.
.
MessageId=4314
SymbolicName=ERROR_DATABASE_FULL
Language=ENU
The database is full.
.
MessageId=4315
SymbolicName=ERROR_MEDIA_INCOMPATIBLE
Language=ENU
The medium is not compatible with the device or media pool.
.
MessageId=4316
SymbolicName=ERROR_RESOURCE_NOT_PRESENT
Language=ENU
The resource required for this operation does not exist.
.
MessageId=4317
SymbolicName=ERROR_INVALID_OPERATION
Language=ENU
The operation identifier is not valid.
.
MessageId=4318
SymbolicName=ERROR_MEDIA_NOT_AVAILABLE
Language=ENU
The media is not mounted or ready for use.
.
MessageId=4319
SymbolicName=ERROR_DEVICE_NOT_AVAILABLE
Language=ENU
The device is not ready for use.
.
MessageId=4320
SymbolicName=ERROR_REQUEST_REFUSED
Language=ENU
The operator or administrator has refused the request.
.
MessageId=4321
SymbolicName=ERROR_INVALID_DRIVE_OBJECT
Language=ENU
The drive identifier does not represent a valid drive.
.
MessageId=4322
SymbolicName=ERROR_LIBRARY_FULL
Language=ENU
Library is full. No slot is available for use.
.
MessageId=4323
SymbolicName=ERROR_MEDIUM_NOT_ACCESSIBLE
Language=ENU
The transport cannot access the medium.
.
MessageId=4324
SymbolicName=ERROR_UNABLE_TO_LOAD_MEDIUM
Language=ENU
Unable to load the medium into the drive.
.
MessageId=4325
SymbolicName=ERROR_UNABLE_TO_INVENTORY_DRIVE
Language=ENU
Unable to retrieve the drive status.
.
MessageId=4326
SymbolicName=ERROR_UNABLE_TO_INVENTORY_SLOT
Language=ENU
Unable to retrieve the slot status.
.
MessageId=4327
SymbolicName=ERROR_UNABLE_TO_INVENTORY_TRANSPORT
Language=ENU
Unable to retrieve status about the transport.
.
MessageId=4328
SymbolicName=ERROR_TRANSPORT_FULL
Language=ENU
Cannot use the transport because it is already in use.
.
MessageId=4329
SymbolicName=ERROR_CONTROLLING_IEPORT
Language=ENU
Unable to open or close the inject/eject port.
.
MessageId=4330
SymbolicName=ERROR_UNABLE_TO_EJECT_MOUNTED_MEDIA
Language=ENU
Unable to eject the medium because it is in a drive.
.
MessageId=4331
SymbolicName=ERROR_CLEANER_SLOT_SET
Language=ENU
A cleaner slot is already reserved.
.
MessageId=4332
SymbolicName=ERROR_CLEANER_SLOT_NOT_SET
Language=ENU
A cleaner slot is not reserved.
.
MessageId=4333
SymbolicName=ERROR_CLEANER_CARTRIDGE_SPENT
Language=ENU
The cleaner cartridge has performed the maximum number of drive cleanings.
.
MessageId=4334
SymbolicName=ERROR_UNEXPECTED_OMID
Language=ENU
Unexpected on-medium identifier.
.
MessageId=4335
SymbolicName=ERROR_CANT_DELETE_LAST_ITEM
Language=ENU
The last remaining item in this group or resource cannot be deleted.
.
MessageId=4336
SymbolicName=ERROR_MESSAGE_EXCEEDS_MAX_SIZE
Language=ENU
The message provided exceeds the maximum size allowed for this parameter.
.
MessageId=4337
SymbolicName=ERROR_VOLUME_CONTAINS_SYS_FILES
Language=ENU
The volume contains system or paging files.
.
MessageId=4338
SymbolicName=ERROR_INDIGENOUS_TYPE
Language=ENU
The media type cannot be removed from this library since at least one drive in the library reports it can support this media type.
.
MessageId=4339
SymbolicName=ERROR_NO_SUPPORTING_DRIVES
Language=ENU
This offline media cannot be mounted on this system since no enabled drives are present which can be used.
.
MessageId=4340
SymbolicName=ERROR_CLEANER_CARTRIDGE_INSTALLED
Language=ENU
A cleaner cartridge is present in the tape library.
.
MessageId=4341
SymbolicName=ERROR_IEPORT_FULL
Language=ENU
Cannot use the inject/eject port because it is not empty.
.
MessageId=4350
SymbolicName=ERROR_FILE_OFFLINE
Language=ENU
This file is currently not available for use on this computer.
.
MessageId=4351
SymbolicName=ERROR_REMOTE_STORAGE_NOT_ACTIVE
Language=ENU
The remote storage service is not operational at this time.
.
MessageId=4352
SymbolicName=ERROR_REMOTE_STORAGE_MEDIA_ERROR
Language=ENU
The remote storage service encountered a media error.
.
MessageId=4390
SymbolicName=ERROR_NOT_A_REPARSE_POINT
Language=ENU
The file or directory is not a reparse point.
.
MessageId=4391
SymbolicName=ERROR_REPARSE_ATTRIBUTE_CONFLICT
Language=ENU
The reparse point attribute cannot be set because it conflicts with an existing attribute.
.
MessageId=4392
SymbolicName=ERROR_INVALID_REPARSE_DATA
Language=ENU
The data present in the reparse point buffer is invalid.
.
MessageId=4393
SymbolicName=ERROR_REPARSE_TAG_INVALID
Language=ENU
The tag present in the reparse point buffer is invalid.
.
MessageId=4394
SymbolicName=ERROR_REPARSE_TAG_MISMATCH
Language=ENU
There is a mismatch between the tag specified in the request and the tag present in the reparse point.
.
MessageId=4395
SymbolicName=ERROR_REPARSE_POINT_ENCOUNTERED
Language=ENU
The object manager encountered a reparse point while retrieving an object.
.
MessageId=4400
SymbolicName=ERROR_APP_DATA_NOT_FOUND
Language=ENU
Fast Cache data not found.
.
MessageId=4401
SymbolicName=ERROR_APP_DATA_EXPIRED
Language=ENU
Fast Cache data expired.
.
MessageId=4402
SymbolicName=ERROR_APP_DATA_CORRUPT
Language=ENU
Fast Cache data corrupt.
.
MessageId=4403
SymbolicName=ERROR_APP_DATA_LIMIT_EXCEEDED
Language=ENU
Fast Cache data has exceeded its max size and cannot be updated.
.
MessageId=4404
SymbolicName=ERROR_APP_DATA_REBOOT_REQUIRED
Language=ENU
Fast Cache has been ReArmed and requires a reboot until it can be updated.
.
MessageId=4420
SymbolicName=ERROR_SECUREBOOT_ROLLBACK_DETECTED
Language=ENU
Secure Boot detected that rollback of protected data has been attempted.
.
MessageId=4421
SymbolicName=ERROR_SECUREBOOT_POLICY_VIOLATION
Language=ENU
The value is protected by Secure Boot policy and cannot be modified or deleted.
.
MessageId=4422
SymbolicName=ERROR_SECUREBOOT_INVALID_POLICY
Language=ENU
The Secure Boot policy is invalid.
.
MessageId=4423
SymbolicName=ERROR_SECUREBOOT_POLICY_PUBLISHER_NOT_FOUND
Language=ENU
A new Secure Boot policy did not contain the current publisher on its update list.
.
MessageId=4424
SymbolicName=ERROR_SECUREBOOT_POLICY_NOT_SIGNED
Language=ENU
The Secure Boot policy is either not signed or is signed by a non-trusted signer.
.
MessageId=4425
SymbolicName=ERROR_SECUREBOOT_NOT_ENABLED
Language=ENU
Secure Boot is not enabled on this machine.
.
MessageId=4426
SymbolicName=ERROR_SECUREBOOT_FILE_REPLACED
Language=ENU
Secure Boot requires that certain files and drivers are not replaced by other files or drivers.
.
MessageId=4427
SymbolicName=ERROR_SECUREBOOT_POLICY_NOT_AUTHORIZED
Language=ENU
The Secure Boot Supplemental Policy file was not authorized on this machine.
.
MessageId=4428
SymbolicName=ERROR_SECUREBOOT_POLICY_UNKNOWN
Language=ENU
The Supplemental Policy is not recognized on this device.
.
MessageId=4429
SymbolicName=ERROR_SECUREBOOT_POLICY_MISSING_ANTIROLLBACKVERSION
Language=ENU
The Antirollback version was not found in the Secure Boot Policy.
.
MessageId=4430
SymbolicName=ERROR_SECUREBOOT_PLATFORM_ID_MISMATCH
Language=ENU
The Platform ID specified in the Secure Boot policy does not match the Platform ID on this device.
.
MessageId=4431
SymbolicName=ERROR_SECUREBOOT_POLICY_ROLLBACK_DETECTED
Language=ENU
The Secure Boot policy file has an older Antirollback Version than this device.
.
MessageId=4432
SymbolicName=ERROR_SECUREBOOT_POLICY_UPGRADE_MISMATCH
Language=ENU
The Secure Boot policy file does not match the upgraded legacy policy.
.
MessageId=4433
SymbolicName=ERROR_SECUREBOOT_REQUIRED_POLICY_FILE_MISSING
Language=ENU
The Secure Boot policy file is required but could not be found.
.
MessageId=4434
SymbolicName=ERROR_SECUREBOOT_NOT_BASE_POLICY
Language=ENU
Supplemental Secure Boot policy file can not be loaded as a base Secure Boot policy.
.
MessageId=4435
SymbolicName=ERROR_SECUREBOOT_NOT_SUPPLEMENTAL_POLICY
Language=ENU
Base Secure Boot policy file can not be loaded as a Supplemental Secure Boot policy.
.
MessageId=4440
SymbolicName=ERROR_OFFLOAD_READ_FLT_NOT_SUPPORTED
Language=ENU
The copy offload read operation is not supported by a filter.
.
MessageId=4441
SymbolicName=ERROR_OFFLOAD_WRITE_FLT_NOT_SUPPORTED
Language=ENU
The copy offload write operation is not supported by a filter.
.
MessageId=4442
SymbolicName=ERROR_OFFLOAD_READ_FILE_NOT_SUPPORTED
Language=ENU
The copy offload read operation is not supported for the file.
.
MessageId=4443
SymbolicName=ERROR_OFFLOAD_WRITE_FILE_NOT_SUPPORTED
Language=ENU
The copy offload write operation is not supported for the file.
.
MessageId=4444
SymbolicName=ERROR_ALREADY_HAS_STREAM_ID
Language=ENU
This file is currently associated with a different stream id.
.
MessageId=4445
SymbolicName=ERROR_SMR_GARBAGE_COLLECTION_REQUIRED
Language=ENU
The volume must undergo garbage collection.
.
MessageId=4446
SymbolicName=ERROR_WOF_WIM_HEADER_CORRUPT
Language=ENU
The WOF driver encountered a corruption in WIM image's Header.
.
MessageId=4447
SymbolicName=ERROR_WOF_WIM_RESOURCE_TABLE_CORRUPT
Language=ENU
The WOF driver encountered a corruption in WIM image's Resource Table.
.
MessageId=4448
SymbolicName=ERROR_WOF_FILE_RESOURCE_TABLE_CORRUPT
Language=ENU
The WOF driver encountered a corruption in the compressed file's Resource Table.
.
MessageId=4449
SymbolicName=ERROR_OBJECT_IS_IMMUTABLE
Language=ENU
The request cannot be completed as it requires modifying an immutable object.
.
MessageId=4500
SymbolicName=ERROR_VOLUME_NOT_SIS_ENABLED
Language=ENU
Single Instance Storage is not available on this volume.
.
MessageId=4550
SymbolicName=ERROR_SYSTEM_INTEGRITY_ROLLBACK_DETECTED
Language=ENU
Application Control detected that policy rollback has been attempted.
.
MessageId=4551
SymbolicName=ERROR_SYSTEM_INTEGRITY_POLICY_VIOLATION
Language=ENU
An Application Control policy has blocked this file.
.
MessageId=4552
SymbolicName=ERROR_SYSTEM_INTEGRITY_INVALID_POLICY
Language=ENU
The Application Control policy is invalid.
.
MessageId=4553
SymbolicName=ERROR_SYSTEM_INTEGRITY_POLICY_NOT_SIGNED
Language=ENU
The Application Control policy is either not signed or is signed by a non-trusted signer.
.
MessageId=4554
SymbolicName=ERROR_SYSTEM_INTEGRITY_TOO_MANY_POLICIES
Language=ENU
The number of Application Control policies is out of limit.
.
MessageId=4555
SymbolicName=ERROR_SYSTEM_INTEGRITY_SUPPLEMENTAL_POLICY_NOT_AUTHORIZED
Language=ENU
The Application Control supplemental policy is not authorized by an Application Control base policy.
.
MessageId=4556
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_MALICIOUS
Language=ENU
An Application Control policy has blocked this file.  Malicious binary reputation.
.
MessageId=4557
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_PUA
Language=ENU
An Application Control policy has blocked this file.  Potentially unwanted application.
.
MessageId=4558
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_DANGEROUS_EXT
Language=ENU
An Application Control policy has blocked this file.  Dangerous file extension from the web.
.
MessageId=4559
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_OFFLINE
Language=ENU
An Application Control policy has blocked this file.  Unable to contact reputation service for unknown file.
.
MessageId=4560
SymbolicName=ERROR_VSM_NOT_INITIALIZED
Language=ENU
Virtual Secure Mode (VSM) is not initialized. The hypervisor or VSM may not be present or enabled.
.
MessageId=4561
SymbolicName=ERROR_VSM_DMA_PROTECTION_NOT_IN_USE
Language=ENU
The hypervisor is not protecting DMA because an IOMMU is not present or not enabled in the BIOS.
.
MessageId=4562
SymbolicName=WINERR_4562
Language=ENU
The Virtual Secure Mode (VSM) master encryption key was not provisioned due to an inadequate CI policy.
.
MessageId=4563
SymbolicName=WINERR_4563
Language=ENU
RSA identity key generation failed.
.
MessageId=4564
SymbolicName=WINERR_4564
Language=ENU
Exporting RSA key components failed.
.
MessageId=4565
SymbolicName=WINERR_4565
Language=ENU
Exported modulus size did not match expected key size.
.
MessageId=4566
SymbolicName=WINERR_4566
Language=ENU
The measurement file included in the firmware update capsule has an invalid format.
.
MessageId=4570
SymbolicName=ERROR_PLATFORM_MANIFEST_NOT_AUTHORIZED
Language=ENU
The Platform Manifest file was not authorized on this machine.
.
MessageId=4571
SymbolicName=ERROR_PLATFORM_MANIFEST_INVALID
Language=ENU
The Platform Manifest file was not valid.
.
MessageId=4572
SymbolicName=ERROR_PLATFORM_MANIFEST_FILE_NOT_AUTHORIZED
Language=ENU
The file is not authorized on this platform because an entry was not found in the Platform Manifest.
.
MessageId=4573
SymbolicName=ERROR_PLATFORM_MANIFEST_CATALOG_NOT_AUTHORIZED
Language=ENU
The catalog is not authorized on this platform because an entry was not found in the Platform Manifest.
.
MessageId=4574
SymbolicName=ERROR_PLATFORM_MANIFEST_BINARY_ID_NOT_FOUND
Language=ENU
The file is not authorized on this platform because a Binary ID was not found in the embedded signature.
.
MessageId=4575
SymbolicName=ERROR_PLATFORM_MANIFEST_NOT_ACTIVE
Language=ENU
No active Platform Manifest exists on this system.
.
MessageId=4576
SymbolicName=ERROR_PLATFORM_MANIFEST_NOT_SIGNED
Language=ENU
The Platform Manifest file was not properly signed.
.
MessageId=4580
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_UNFRIENDLY_FILE
Language=ENU
An Application Control policy has blocked this file.  Unfriendly file.
.
MessageId=4581
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_UNATTAINABLE
Language=ENU
An Application Control policy has blocked this file.  Failed to obtain file reputation because an infrastructure issue occurred. Try again later.
.
MessageId=4582
SymbolicName=ERROR_SYSTEM_INTEGRITY_REPUTATION_EXPLICIT_DENY_FILE
Language=ENU
An Application Control policy has blocked this file.  Explicit denied file.
.
MessageId=4583
SymbolicName=WINERR_4583
Language=ENU
An Application Control policy has blocked this file.  File did not pass WHQL-only requirements.
.
MessageId=5001
SymbolicName=ERROR_DEPENDENT_RESOURCE_EXISTS
Language=ENU
The operation cannot be completed because other resources are dependent on this resource.
.
MessageId=5002
SymbolicName=ERROR_DEPENDENCY_NOT_FOUND
Language=ENU
The cluster resource dependency cannot be found.
.
MessageId=5003
SymbolicName=ERROR_DEPENDENCY_ALREADY_EXISTS
Language=ENU
The cluster resource cannot be made dependent on the specified resource because it is already dependent.
.
MessageId=5004
SymbolicName=ERROR_RESOURCE_NOT_ONLINE
Language=ENU
The cluster resource is not online.
.
MessageId=5005
SymbolicName=ERROR_HOST_NODE_NOT_AVAILABLE
Language=ENU
A cluster node is not available for this operation.
.
MessageId=5006
SymbolicName=ERROR_RESOURCE_NOT_AVAILABLE
Language=ENU
The cluster resource is not available.
.
MessageId=5007
SymbolicName=ERROR_RESOURCE_NOT_FOUND
Language=ENU
The cluster resource could not be found.
.
MessageId=5008
SymbolicName=ERROR_SHUTDOWN_CLUSTER
Language=ENU
The cluster is being shut down.
.
MessageId=5009
SymbolicName=ERROR_CANT_EVICT_ACTIVE_NODE
Language=ENU
A cluster node cannot be evicted from the cluster unless the node is down or it is the last node.
.
MessageId=5010
SymbolicName=ERROR_OBJECT_ALREADY_EXISTS
Language=ENU
The object already exists.
.
MessageId=5011
SymbolicName=ERROR_OBJECT_IN_LIST
Language=ENU
The object is already in the list.
.
MessageId=5012
SymbolicName=ERROR_GROUP_NOT_AVAILABLE
Language=ENU
The cluster group is not available for any new requests.
.
MessageId=5013
SymbolicName=ERROR_GROUP_NOT_FOUND
Language=ENU
The cluster group could not be found.
.
MessageId=5014
SymbolicName=ERROR_GROUP_NOT_ONLINE
Language=ENU
The operation could not be completed because the cluster group is not online.
.
MessageId=5015
SymbolicName=ERROR_HOST_NODE_NOT_RESOURCE_OWNER
Language=ENU
The operation failed because either the specified cluster node is not the owner of the resource, or the node is not a possible owner of the resource.
.
MessageId=5016
SymbolicName=ERROR_HOST_NODE_NOT_GROUP_OWNER
Language=ENU
The operation failed because either the specified cluster node is not the owner of the group, or the node is not a possible owner of the group.
.
MessageId=5017
SymbolicName=ERROR_RESMON_CREATE_FAILED
Language=ENU
The cluster resource could not be created in the specified resource monitor.
.
MessageId=5018
SymbolicName=ERROR_RESMON_ONLINE_FAILED
Language=ENU
The cluster resource could not be brought online by the resource monitor.
.
MessageId=5019
SymbolicName=ERROR_RESOURCE_ONLINE
Language=ENU
The operation could not be completed because the cluster resource is online.
.
MessageId=5020
SymbolicName=ERROR_QUORUM_RESOURCE
Language=ENU
The cluster resource could not be deleted or brought offline because it is the quorum resource.
.
MessageId=5021
SymbolicName=ERROR_NOT_QUORUM_CAPABLE
Language=ENU
The cluster could not make the specified resource a quorum resource because it is not capable of being a quorum resource.
.
MessageId=5022
SymbolicName=ERROR_CLUSTER_SHUTTING_DOWN
Language=ENU
The cluster software is shutting down.
.
MessageId=5023
SymbolicName=ERROR_INVALID_STATE
Language=ENU
The group or resource is not in the correct state to perform the requested operation.
.
MessageId=5024
SymbolicName=ERROR_RESOURCE_PROPERTIES_STORED
Language=ENU
The properties were stored but not all changes will take effect until the next time the resource is brought online.
.
MessageId=5025
SymbolicName=ERROR_NOT_QUORUM_CLASS
Language=ENU
The cluster could not make the specified resource a quorum resource because it does not belong to a shared storage class.
.
MessageId=5026
SymbolicName=ERROR_CORE_RESOURCE
Language=ENU
The cluster resource could not be deleted since it is a core resource.
.
MessageId=5027
SymbolicName=ERROR_QUORUM_RESOURCE_ONLINE_FAILED
Language=ENU
The quorum resource failed to come online.
.
MessageId=5028
SymbolicName=ERROR_QUORUMLOG_OPEN_FAILED
Language=ENU
The quorum log could not be created or mounted successfully.
.
MessageId=5029
SymbolicName=ERROR_CLUSTERLOG_CORRUPT
Language=ENU
The cluster log is corrupt.
.
MessageId=5030
SymbolicName=ERROR_CLUSTERLOG_RECORD_EXCEEDS_MAXSIZE
Language=ENU
The record could not be written to the cluster log since it exceeds the maximum size.
.
MessageId=5031
SymbolicName=ERROR_CLUSTERLOG_EXCEEDS_MAXSIZE
Language=ENU
The cluster log exceeds its maximum size.
.
MessageId=5032
SymbolicName=ERROR_CLUSTERLOG_CHKPOINT_NOT_FOUND
Language=ENU
No checkpoint record was found in the cluster log.
.
MessageId=5033
SymbolicName=ERROR_CLUSTERLOG_NOT_ENOUGH_SPACE
Language=ENU
The minimum required disk space needed for logging is not available.
.
MessageId=5034
SymbolicName=ERROR_QUORUM_OWNER_ALIVE
Language=ENU
The cluster node failed to take control of the quorum resource because the resource is owned by another active node.
.
MessageId=5035
SymbolicName=ERROR_NETWORK_NOT_AVAILABLE
Language=ENU
A cluster network is not available for this operation.
.
MessageId=5036
SymbolicName=ERROR_NODE_NOT_AVAILABLE
Language=ENU
A cluster node is not available for this operation.
.
MessageId=5037
SymbolicName=ERROR_ALL_NODES_NOT_AVAILABLE
Language=ENU
All cluster nodes must be running to perform this operation.
.
MessageId=5038
SymbolicName=ERROR_RESOURCE_FAILED
Language=ENU
A cluster resource failed.
.
MessageId=5039
SymbolicName=ERROR_CLUSTER_INVALID_NODE
Language=ENU
The cluster node is not valid.
.
MessageId=5040
SymbolicName=ERROR_CLUSTER_NODE_EXISTS
Language=ENU
The cluster node already exists.
.
MessageId=5041
SymbolicName=ERROR_CLUSTER_JOIN_IN_PROGRESS
Language=ENU
A node is in the process of joining the cluster.
.
MessageId=5042
SymbolicName=ERROR_CLUSTER_NODE_NOT_FOUND
Language=ENU
The cluster node was not found.
.
MessageId=5043
SymbolicName=ERROR_CLUSTER_LOCAL_NODE_NOT_FOUND
Language=ENU
The cluster local node information was not found.
.
MessageId=5044
SymbolicName=ERROR_CLUSTER_NETWORK_EXISTS
Language=ENU
The cluster network already exists.
.
MessageId=5045
SymbolicName=ERROR_CLUSTER_NETWORK_NOT_FOUND
Language=ENU
The cluster network was not found.
.
MessageId=5046
SymbolicName=ERROR_CLUSTER_NETINTERFACE_EXISTS
Language=ENU
The cluster network interface already exists.
.
MessageId=5047
SymbolicName=ERROR_CLUSTER_NETINTERFACE_NOT_FOUND
Language=ENU
The cluster network interface was not found.
.
MessageId=5048
SymbolicName=ERROR_CLUSTER_INVALID_REQUEST
Language=ENU
The cluster request is not valid for this object.
.
MessageId=5049
SymbolicName=ERROR_CLUSTER_INVALID_NETWORK_PROVIDER
Language=ENU
The cluster network provider is not valid.
.
MessageId=5050
SymbolicName=ERROR_CLUSTER_NODE_DOWN
Language=ENU
The cluster node is down.
.
MessageId=5051
SymbolicName=ERROR_CLUSTER_NODE_UNREACHABLE
Language=ENU
The cluster node is not reachable.
.
MessageId=5052
SymbolicName=ERROR_CLUSTER_NODE_NOT_MEMBER
Language=ENU
The cluster node is not a member of the cluster.
.
MessageId=5053
SymbolicName=ERROR_CLUSTER_JOIN_NOT_IN_PROGRESS
Language=ENU
A cluster join operation is not in progress.
.
MessageId=5054
SymbolicName=ERROR_CLUSTER_INVALID_NETWORK
Language=ENU
The cluster network is not valid.
.
MessageId=5056
SymbolicName=ERROR_CLUSTER_NODE_UP
Language=ENU
The cluster node is up.
.
MessageId=5057
SymbolicName=ERROR_CLUSTER_IPADDR_IN_USE
Language=ENU
The cluster IP address is already in use.
.
MessageId=5058
SymbolicName=ERROR_CLUSTER_NODE_NOT_PAUSED
Language=ENU
The cluster node is not paused.
.
MessageId=5059
SymbolicName=ERROR_CLUSTER_NO_SECURITY_CONTEXT
Language=ENU
No cluster security context is available.
.
MessageId=5060
SymbolicName=ERROR_CLUSTER_NETWORK_NOT_INTERNAL
Language=ENU
The cluster network is not configured for internal cluster communication.
.
MessageId=5061
SymbolicName=ERROR_CLUSTER_NODE_ALREADY_UP
Language=ENU
The cluster node is already up.
.
MessageId=5062
SymbolicName=ERROR_CLUSTER_NODE_ALREADY_DOWN
Language=ENU
The cluster node is already down.
.
MessageId=5063
SymbolicName=ERROR_CLUSTER_NETWORK_ALREADY_ONLINE
Language=ENU
The cluster network is already online.
.
MessageId=5064
SymbolicName=ERROR_CLUSTER_NETWORK_ALREADY_OFFLINE
Language=ENU
The cluster network is already offline.
.
MessageId=5065
SymbolicName=ERROR_CLUSTER_NODE_ALREADY_MEMBER
Language=ENU
The cluster node is already a member of the cluster.
.
MessageId=5066
SymbolicName=ERROR_CLUSTER_LAST_INTERNAL_NETWORK
Language=ENU
The cluster network is the only one configured for internal cluster communication between two or more active cluster nodes. The internal communication capability cannot be removed from the network.
.
MessageId=5067
SymbolicName=ERROR_CLUSTER_NETWORK_HAS_DEPENDENTS
Language=ENU
One or more cluster resources depend on the network to provide service to clients. The client access capability cannot be removed from the network.
.
MessageId=5068
SymbolicName=ERROR_INVALID_OPERATION_ON_QUORUM
Language=ENU
This operation cannot currently be performed on the cluster group containing the quorum resource.
.
MessageId=5069
SymbolicName=ERROR_DEPENDENCY_NOT_ALLOWED
Language=ENU
The cluster quorum resource is not allowed to have any dependencies.
.
MessageId=5070
SymbolicName=ERROR_CLUSTER_NODE_PAUSED
Language=ENU
The cluster node is paused.
.
MessageId=5071
SymbolicName=ERROR_NODE_CANT_HOST_RESOURCE
Language=ENU
The cluster resource cannot be brought online. The owner node cannot run this resource.
.
MessageId=5072
SymbolicName=ERROR_CLUSTER_NODE_NOT_READY
Language=ENU
The cluster node is not ready to perform the requested operation.
.
MessageId=5073
SymbolicName=ERROR_CLUSTER_NODE_SHUTTING_DOWN
Language=ENU
The cluster node is shutting down.
.
MessageId=5074
SymbolicName=ERROR_CLUSTER_JOIN_ABORTED
Language=ENU
The cluster join operation was aborted.
.
MessageId=5075
SymbolicName=ERROR_CLUSTER_INCOMPATIBLE_VERSIONS
Language=ENU
The node failed to join the cluster because the joining node and other nodes in the cluster have incompatible operating system versions. To get more information about operating system versions of the cluster, run the Validate a Configuration Wizard or the Test-Cluster Windows PowerShell cmdlet.
.
MessageId=5076
SymbolicName=ERROR_CLUSTER_MAXNUM_OF_RESOURCES_EXCEEDED
Language=ENU
This resource cannot be created because the cluster has reached the limit on the number of resources it can monitor.
.
MessageId=5077
SymbolicName=ERROR_CLUSTER_SYSTEM_CONFIG_CHANGED
Language=ENU
The system configuration changed during the cluster join or form operation. The join or form operation was aborted.
.
MessageId=5078
SymbolicName=ERROR_CLUSTER_RESOURCE_TYPE_NOT_FOUND
Language=ENU
The specified resource type was not found.
.
MessageId=5079
SymbolicName=ERROR_CLUSTER_RESTYPE_NOT_SUPPORTED
Language=ENU
The specified node does not support a resource of this type. This may be due to version inconsistencies or due to the absence of the resource DLL on this node.
.
MessageId=5080
SymbolicName=ERROR_CLUSTER_RESNAME_NOT_FOUND
Language=ENU
The specified resource name is not supported by this resource DLL. This may be due to a bad (or changed) name supplied to the resource DLL.
.
MessageId=5081
SymbolicName=ERROR_CLUSTER_NO_RPC_PACKAGES_REGISTERED
Language=ENU
No authentication package could be registered with the RPC server.
.
MessageId=5082
SymbolicName=ERROR_CLUSTER_OWNER_NOT_IN_PREFLIST
Language=ENU
You cannot bring the group online because the owner of the group is not in the preferred list for the group. To change the owner node for the group, move the group.
.
MessageId=5083
SymbolicName=ERROR_CLUSTER_DATABASE_SEQMISMATCH
Language=ENU
The join operation failed because the cluster database sequence number has changed or is incompatible with the locker node. This may happen during a join operation if the cluster database was changing during the join.
.
MessageId=5084
SymbolicName=ERROR_RESMON_INVALID_STATE
Language=ENU
The resource monitor will not allow the fail operation to be performed while the resource is in its current state. This may happen if the resource is in a pending state.
.
MessageId=5085
SymbolicName=ERROR_CLUSTER_GUM_NOT_LOCKER
Language=ENU
A non locker code got a request to reserve the lock for making global updates.
.
MessageId=5086
SymbolicName=ERROR_QUORUM_DISK_NOT_FOUND
Language=ENU
The quorum disk could not be located by the cluster service.
.
MessageId=5087
SymbolicName=ERROR_DATABASE_BACKUP_CORRUPT
Language=ENU
The backed up cluster database is possibly corrupt.
.
MessageId=5088
SymbolicName=ERROR_CLUSTER_NODE_ALREADY_HAS_DFS_ROOT
Language=ENU
A DFS root already exists in this cluster node.
.
MessageId=5089
SymbolicName=ERROR_RESOURCE_PROPERTY_UNCHANGEABLE
Language=ENU
An attempt to modify a resource property failed because it conflicts with another existing property.
.
MessageId=5090
SymbolicName=ERROR_NO_ADMIN_ACCESS_POINT
Language=ENU
This operation is not supported on a cluster without an Administrator Access Point.
.
MessageId=5890
SymbolicName=ERROR_CLUSTER_MEMBERSHIP_INVALID_STATE
Language=ENU
An operation was attempted that is incompatible with the current membership state of the node.
.
MessageId=5891
SymbolicName=ERROR_CLUSTER_QUORUMLOG_NOT_FOUND
Language=ENU
The quorum resource does not contain the quorum log.
.
MessageId=5892
SymbolicName=ERROR_CLUSTER_MEMBERSHIP_HALT
Language=ENU
The membership engine requested shutdown of the cluster service on this node.
.
MessageId=5893
SymbolicName=ERROR_CLUSTER_INSTANCE_ID_MISMATCH
Language=ENU
The join operation failed because the cluster instance ID of the joining node does not match the cluster instance ID of the sponsor node.
.
MessageId=5894
SymbolicName=ERROR_CLUSTER_NETWORK_NOT_FOUND_FOR_IP
Language=ENU
A matching cluster network for the specified IP address could not be found.
.
MessageId=5895
SymbolicName=ERROR_CLUSTER_PROPERTY_DATA_TYPE_MISMATCH
Language=ENU
The actual data type of the property did not match the expected data type of the property.
.
MessageId=5896
SymbolicName=ERROR_CLUSTER_EVICT_WITHOUT_CLEANUP
Language=ENU
The cluster node was evicted from the cluster successfully, but the node was not cleaned up. To determine what cleanup steps failed and how to recover, see the Failover Clustering application event log using Event Viewer.
.
MessageId=5897
SymbolicName=ERROR_CLUSTER_PARAMETER_MISMATCH
Language=ENU
Two or more parameter values specified for a resource's properties are in conflict.
.
MessageId=5898
SymbolicName=ERROR_NODE_CANNOT_BE_CLUSTERED
Language=ENU
This computer cannot be made a member of a cluster.
.
MessageId=5899
SymbolicName=ERROR_CLUSTER_WRONG_OS_VERSION
Language=ENU
This computer cannot be made a member of a cluster because it does not have the correct version of Windows installed.
.
MessageId=5900
SymbolicName=ERROR_CLUSTER_CANT_CREATE_DUP_CLUSTER_NAME
Language=ENU
A cluster cannot be created with the specified cluster name because that cluster name is already in use. Specify a different name for the cluster.
.
MessageId=5901
SymbolicName=ERROR_CLUSCFG_ALREADY_COMMITTED
Language=ENU
The cluster configuration action has already been committed.
.
MessageId=5902
SymbolicName=ERROR_CLUSCFG_ROLLBACK_FAILED
Language=ENU
The cluster configuration action could not be rolled back.
.
MessageId=5903
SymbolicName=ERROR_CLUSCFG_SYSTEM_DISK_DRIVE_LETTER_CONFLICT
Language=ENU
The drive letter assigned to a system disk on one node conflicted with the drive letter assigned to a disk on another node.
.
MessageId=5904
SymbolicName=ERROR_CLUSTER_OLD_VERSION
Language=ENU
One or more nodes in the cluster are running a version of Windows that does not support this operation.
.
MessageId=5905
SymbolicName=ERROR_CLUSTER_MISMATCHED_COMPUTER_ACCT_NAME
Language=ENU
The name of the corresponding computer account doesn't match the Network Name for this resource.
.
MessageId=5906
SymbolicName=ERROR_CLUSTER_NO_NET_ADAPTERS
Language=ENU
No network adapters are available.
.
MessageId=5907
SymbolicName=ERROR_CLUSTER_POISONED
Language=ENU
The cluster node has been poisoned.
.
MessageId=5908
SymbolicName=ERROR_CLUSTER_GROUP_MOVING
Language=ENU
The group is unable to accept the request since it is moving to another node.
.
MessageId=5909
SymbolicName=ERROR_CLUSTER_RESOURCE_TYPE_BUSY
Language=ENU
The resource type cannot accept the request since is too busy performing another operation.
.
MessageId=5910
SymbolicName=ERROR_RESOURCE_CALL_TIMED_OUT
Language=ENU
The call to the cluster resource DLL timed out.
.
MessageId=5911
SymbolicName=ERROR_INVALID_CLUSTER_IPV6_ADDRESS
Language=ENU
The address is not valid for an IPv6 Address resource. A global IPv6 address is required, and it must match a cluster network. Compatibility addresses are not permitted.
.
MessageId=5912
SymbolicName=ERROR_CLUSTER_INTERNAL_INVALID_FUNCTION
Language=ENU
An internal cluster error occurred. A call to an invalid function was attempted.
.
MessageId=5913
SymbolicName=ERROR_CLUSTER_PARAMETER_OUT_OF_BOUNDS
Language=ENU
A parameter value is out of acceptable range.
.
MessageId=5914
SymbolicName=ERROR_CLUSTER_PARTIAL_SEND
Language=ENU
A network error occurred while sending data to another node in the cluster. The number of bytes transmitted was less than required.
.
MessageId=5915
SymbolicName=ERROR_CLUSTER_REGISTRY_INVALID_FUNCTION
Language=ENU
An invalid cluster registry operation was attempted.
.
MessageId=5916
SymbolicName=ERROR_CLUSTER_INVALID_STRING_TERMINATION
Language=ENU
An input string of characters is not properly terminated.
.
MessageId=5917
SymbolicName=ERROR_CLUSTER_INVALID_STRING_FORMAT
Language=ENU
An input string of characters is not in a valid format for the data it represents.
.
MessageId=5918
SymbolicName=ERROR_CLUSTER_DATABASE_TRANSACTION_IN_PROGRESS
Language=ENU
An internal cluster error occurred. A cluster database transaction was attempted while a transaction was already in progress.
.
MessageId=5919
SymbolicName=ERROR_CLUSTER_DATABASE_TRANSACTION_NOT_IN_PROGRESS
Language=ENU
An internal cluster error occurred. There was an attempt to commit a cluster database transaction while no transaction was in progress.
.
MessageId=5920
SymbolicName=ERROR_CLUSTER_NULL_DATA
Language=ENU
An internal cluster error occurred. Data was not properly initialized.
.
MessageId=5921
SymbolicName=ERROR_CLUSTER_PARTIAL_READ
Language=ENU
An error occurred while reading from a stream of data. An unexpected number of bytes was returned.
.
MessageId=5922
SymbolicName=ERROR_CLUSTER_PARTIAL_WRITE
Language=ENU
An error occurred while writing to a stream of data. The required number of bytes could not be written.
.
MessageId=5923
SymbolicName=ERROR_CLUSTER_CANT_DESERIALIZE_DATA
Language=ENU
An error occurred while deserializing a stream of cluster data.
.
MessageId=5924
SymbolicName=ERROR_DEPENDENT_RESOURCE_PROPERTY_CONFLICT
Language=ENU
One or more property values for this resource are in conflict with one or more property values associated with its dependent resource(s).
.
MessageId=5925
SymbolicName=ERROR_CLUSTER_NO_QUORUM
Language=ENU
A quorum of cluster nodes was not present to form a cluster.
.
MessageId=5926
SymbolicName=ERROR_CLUSTER_INVALID_IPV6_NETWORK
Language=ENU
The cluster network is not valid for an IPv6 Address resource, or it does not match the configured address.
.
MessageId=5927
SymbolicName=ERROR_CLUSTER_INVALID_IPV6_TUNNEL_NETWORK
Language=ENU
The cluster network is not valid for an IPv6 Tunnel resource. Check the configuration of the IP Address resource on which the IPv6 Tunnel resource depends.
.
MessageId=5928
SymbolicName=ERROR_QUORUM_NOT_ALLOWED_IN_THIS_GROUP
Language=ENU
Quorum resource cannot reside in the Available Storage group.
.
MessageId=5929
SymbolicName=ERROR_DEPENDENCY_TREE_TOO_COMPLEX
Language=ENU
The dependencies for this resource are nested too deeply.
.
MessageId=5930
SymbolicName=ERROR_EXCEPTION_IN_RESOURCE_CALL
Language=ENU
The call into the resource DLL raised an unhandled exception.
.
MessageId=5931
SymbolicName=ERROR_CLUSTER_RHS_FAILED_INITIALIZATION
Language=ENU
The RHS process failed to initialize.
.
MessageId=5932
SymbolicName=ERROR_CLUSTER_NOT_INSTALLED
Language=ENU
The Failover Clustering feature is not installed on this node.
.
MessageId=5933
SymbolicName=ERROR_CLUSTER_RESOURCES_MUST_BE_ONLINE_ON_THE_SAME_NODE
Language=ENU
The resources must be online on the same node for this operation
.
MessageId=5934
SymbolicName=ERROR_CLUSTER_MAX_NODES_IN_CLUSTER
Language=ENU
A new node can not be added since this cluster is already at its maximum number of nodes.
.
MessageId=5935
SymbolicName=ERROR_CLUSTER_TOO_MANY_NODES
Language=ENU
This cluster can not be created since the specified number of nodes exceeds the maximum allowed limit.
.
MessageId=5936
SymbolicName=ERROR_CLUSTER_OBJECT_ALREADY_USED
Language=ENU
An attempt to use the specified cluster name failed because an enabled computer object with the given name already exists in the domain.
.
MessageId=5937
SymbolicName=ERROR_NONCORE_GROUPS_FOUND
Language=ENU
This cluster cannot be destroyed. It has non-core application groups which must be deleted before the cluster can be destroyed.
.
MessageId=5938
SymbolicName=ERROR_FILE_SHARE_RESOURCE_CONFLICT
Language=ENU
File share associated with file share witness resource cannot be hosted by this cluster or any of its nodes.
.
MessageId=5939
SymbolicName=ERROR_CLUSTER_EVICT_INVALID_REQUEST
Language=ENU
Eviction of this node is invalid at this time. Due to quorum requirements node eviction will result in cluster shutdown.
.
MessageId=5940
SymbolicName=ERROR_CLUSTER_SINGLETON_RESOURCE
Language=ENU
Only one instance of this resource type is allowed in the cluster.
.
MessageId=5941
SymbolicName=ERROR_CLUSTER_GROUP_SINGLETON_RESOURCE
Language=ENU
Only one instance of this resource type is allowed per resource group.
.
MessageId=5942
SymbolicName=ERROR_CLUSTER_RESOURCE_PROVIDER_FAILED
Language=ENU
The resource failed to come online due to the failure of one or more provider resources.
.
MessageId=5943
SymbolicName=ERROR_CLUSTER_RESOURCE_CONFIGURATION_ERROR
Language=ENU
The resource has indicated that it cannot come online on any node.
.
MessageId=5944
SymbolicName=ERROR_CLUSTER_GROUP_BUSY
Language=ENU
The current operation cannot be performed on this group at this time.
.
MessageId=5945
SymbolicName=ERROR_CLUSTER_NOT_SHARED_VOLUME
Language=ENU
The directory or file is not located on a cluster shared volume.
.
MessageId=5946
SymbolicName=ERROR_CLUSTER_INVALID_SECURITY_DESCRIPTOR
Language=ENU
The Security Descriptor does not meet the requirements for a cluster.
.
MessageId=5947
SymbolicName=ERROR_CLUSTER_SHARED_VOLUMES_IN_USE
Language=ENU
There is one or more shared volumes resources configured in the cluster.
.
MessageId=5948
SymbolicName=ERROR_CLUSTER_USE_SHARED_VOLUMES_API
Language=ENU
This group or resource cannot be directly manipulated.
.
MessageId=5949
SymbolicName=ERROR_CLUSTER_BACKUP_IN_PROGRESS
Language=ENU
Back up is in progress. Please wait for backup completion before trying this operation again.
.
MessageId=5950
SymbolicName=ERROR_NON_CSV_PATH
Language=ENU
The path does not belong to a cluster shared volume.
.
MessageId=5951
SymbolicName=ERROR_CSV_VOLUME_NOT_LOCAL
Language=ENU
The cluster shared volume is not locally mounted on this node.
.
MessageId=5952
SymbolicName=ERROR_CLUSTER_WATCHDOG_TERMINATING
Language=ENU
The cluster watchdog is terminating.
.
MessageId=5953
SymbolicName=ERROR_CLUSTER_RESOURCE_VETOED_MOVE_INCOMPATIBLE_NODES
Language=ENU
A resource vetoed a move between two nodes because they are incompatible.
.
MessageId=5954
SymbolicName=ERROR_CLUSTER_INVALID_NODE_WEIGHT
Language=ENU
The request is invalid either because node weight cannot be changed while the cluster is in disk-only quorum mode, or because changing the node weight would violate the minimum cluster quorum requirements.
.
MessageId=5955
SymbolicName=ERROR_CLUSTER_RESOURCE_VETOED_CALL
Language=ENU
The resource vetoed the call.
.
MessageId=5956
SymbolicName=ERROR_RESMON_SYSTEM_RESOURCES_LACKING
Language=ENU
Resource could not start or run because it could not reserve sufficient system resources.
.
MessageId=5957
SymbolicName=ERROR_CLUSTER_RESOURCE_VETOED_MOVE_NOT_ENOUGH_RESOURCES_ON_DESTINATION
Language=ENU
A resource vetoed a move between two nodes because the destination currently does not have enough resources to complete the operation.
.
MessageId=6000
SymbolicName=ERROR_ENCRYPTION_FAILED
Language=ENU
The specified file could not be encrypted.
.
MessageId=6001
SymbolicName=ERROR_DECRYPTION_FAILED
Language=ENU
The specified file could not be decrypted.
.
MessageId=6002
SymbolicName=ERROR_FILE_ENCRYPTED
Language=ENU
The specified file is encrypted and the user does not have the ability to decrypt it.
.
MessageId=6003
SymbolicName=ERROR_NO_RECOVERY_POLICY
Language=ENU
There is no valid encryption recovery policy configured for this system.
.
MessageId=6004
SymbolicName=ERROR_NO_EFS
Language=ENU
The required encryption driver is not loaded for this system.
.
MessageId=6005
SymbolicName=ERROR_WRONG_EFS
Language=ENU
The file was encrypted with a different encryption driver than is currently loaded.
.
MessageId=6006
SymbolicName=ERROR_NO_USER_KEYS
Language=ENU
There are no EFS keys defined for the user.
.
MessageId=6007
SymbolicName=ERROR_FILE_NOT_ENCRYPTED
Language=ENU
The specified file is not encrypted.
.
MessageId=6008
SymbolicName=ERROR_NOT_EXPORT_FORMAT
Language=ENU
The specified file is not in the defined EFS export format.
.
MessageId=6009
SymbolicName=ERROR_FILE_READ_ONLY
Language=ENU
The specified file is read only.
.
MessageId=6010
SymbolicName=ERROR_DIR_EFS_DISALLOWED
Language=ENU
The directory has been disabled for encryption.
.
MessageId=6011
SymbolicName=ERROR_EFS_SERVER_NOT_TRUSTED
Language=ENU
The server is not trusted for remote encryption operation.
.
MessageId=6012
SymbolicName=ERROR_BAD_RECOVERY_POLICY
Language=ENU
Recovery policy configured for this system contains invalid recovery certificate.
.
MessageId=6013
SymbolicName=ERROR_EFS_ALG_BLOB_TOO_BIG
Language=ENU
The encryption algorithm used on the source file needs a bigger key buffer than the one on the destination file.
.
MessageId=6014
SymbolicName=ERROR_VOLUME_NOT_SUPPORT_EFS
Language=ENU
The disk partition does not support file encryption.
.
MessageId=6015
SymbolicName=ERROR_EFS_DISABLED
Language=ENU
This machine is disabled for file encryption.
.
MessageId=6016
SymbolicName=ERROR_EFS_VERSION_NOT_SUPPORT
Language=ENU
A newer system is required to decrypt this encrypted file.
.
MessageId=6017
SymbolicName=ERROR_CS_ENCRYPTION_INVALID_SERVER_RESPONSE
Language=ENU
The remote server sent an invalid response for a file being opened with Client Side Encryption.
.
MessageId=6018
SymbolicName=ERROR_CS_ENCRYPTION_UNSUPPORTED_SERVER
Language=ENU
Client Side Encryption is not supported by the remote server even though it claims to support it.
.
MessageId=6019
SymbolicName=ERROR_CS_ENCRYPTION_EXISTING_ENCRYPTED_FILE
Language=ENU
File is encrypted and should be opened in Client Side Encryption mode.
.
MessageId=6020
SymbolicName=ERROR_CS_ENCRYPTION_NEW_ENCRYPTED_FILE
Language=ENU
A new encrypted file is being created and a $EFS needs to be provided.
.
MessageId=6021
SymbolicName=ERROR_CS_ENCRYPTION_FILE_NOT_CSE
Language=ENU
The SMB client requested a CSE FSCTL on a non-CSE file.
.
MessageId=6022
SymbolicName=ERROR_ENCRYPTION_POLICY_DENIES_OPERATION
Language=ENU
The requested operation was blocked by policy. For more information, contact your system administrator.
.
MessageId=6023
SymbolicName=ERROR_WIP_ENCRYPTION_FAILED
Language=ENU
The specified file could not be encrypted with Windows Information Protection.
.
MessageId=6024
SymbolicName=WINERR_6024
Language=ENU
Personal Data Encryption keys to encrypt data are currently unavailable due to a failure.
.
MessageId=6025
SymbolicName=WINERR_6025
Language=ENU
Personal Data Encryption keys to decrypt data are currently unavailable due to a failure.
.
MessageId=6026
SymbolicName=WINERR_6026
Language=ENU
Personal Data Encryption keys to decrypt data are currently unavailable as the user's session is locked.
.
MessageId=6118
SymbolicName=ERROR_NO_BROWSER_SERVERS_FOUND
Language=ENU
The list of servers for this workgroup is not currently available
.
MessageId=6200
SymbolicName=WINERR_6200
Language=ENU
The Task Scheduler service must be configured to run in the System account to function properly. Individual tasks may be configured to run in other accounts.
.
MessageId=6251
SymbolicName=WINERR_6251
Language=ENU
A cluster native update template with that name already exists.
.
MessageId=6252
SymbolicName=WINERR_6252
Language=ENU
A cluster native update template with that name was not found.
.
MessageId=6253
SymbolicName=WINERR_6253
Language=ENU
A cluster native update run with the specified name was not found.
.
MessageId=6254
SymbolicName=WINERR_6254
Language=ENU
A cluster native update run is already in progress on the cluster.
.
MessageId=6255
SymbolicName=WINERR_6255
Language=ENU
There is currently no ongoing cluster native update run on the cluster.
.
MessageId=6256
SymbolicName=WINERR_6256
Language=ENU
Cluster native update not ready - run Enable-ClusterNativeUpdate to load.
.
MessageId=6350
SymbolicName=WINERR_6350
Language=ENU
A camera's configuration contains some invalid settings.
.
MessageId=6351
SymbolicName=WINERR_6351
Language=ENU
A camera interface doesn't have the desired bandwidth for data transfer.
.
MessageId=6600
SymbolicName=ERROR_LOG_SECTOR_INVALID
Language=ENU
Log service encountered an invalid log sector.
.
MessageId=6601
SymbolicName=ERROR_LOG_SECTOR_PARITY_INVALID
Language=ENU
Log service encountered a log sector with invalid block parity.
.
MessageId=6602
SymbolicName=ERROR_LOG_SECTOR_REMAPPED
Language=ENU
Log service encountered a remapped log sector.
.
MessageId=6603
SymbolicName=ERROR_LOG_BLOCK_INCOMPLETE
Language=ENU
Log service encountered a partial or incomplete log block.
.
MessageId=6604
SymbolicName=ERROR_LOG_INVALID_RANGE
Language=ENU
Log service encountered an attempt access data outside the active log range.
.
MessageId=6605
SymbolicName=ERROR_LOG_BLOCKS_EXHAUSTED
Language=ENU
Log service user marshalling buffers are exhausted.
.
MessageId=6606
SymbolicName=ERROR_LOG_READ_CONTEXT_INVALID
Language=ENU
Log service encountered an attempt read from a marshalling area with an invalid read context.
.
MessageId=6607
SymbolicName=ERROR_LOG_RESTART_INVALID
Language=ENU
Log service encountered an invalid log restart area.
.
MessageId=6608
SymbolicName=ERROR_LOG_BLOCK_VERSION
Language=ENU
Log service encountered an invalid log block version.
.
MessageId=6609
SymbolicName=ERROR_LOG_BLOCK_INVALID
Language=ENU
Log service encountered an invalid log block.
.
MessageId=6610
SymbolicName=ERROR_LOG_READ_MODE_INVALID
Language=ENU
Log service encountered an attempt to read the log with an invalid read mode.
.
MessageId=6611
SymbolicName=ERROR_LOG_NO_RESTART
Language=ENU
Log service encountered a log stream with no restart area.
.
MessageId=6612
SymbolicName=ERROR_LOG_METADATA_CORRUPT
Language=ENU
Log service encountered a corrupted metadata file.
.
MessageId=6613
SymbolicName=ERROR_LOG_METADATA_INVALID
Language=ENU
Log service encountered a metadata file that could not be created by the log file system.
.
MessageId=6614
SymbolicName=ERROR_LOG_METADATA_INCONSISTENT
Language=ENU
Log service encountered a metadata file with inconsistent data.
.
MessageId=6615
SymbolicName=ERROR_LOG_RESERVATION_INVALID
Language=ENU
Log service encountered an attempt to erroneous allocate or dispose reservation space.
.
MessageId=6616
SymbolicName=ERROR_LOG_CANT_DELETE
Language=ENU
Log service cannot delete log file or file system container.
.
MessageId=6617
SymbolicName=ERROR_LOG_CONTAINER_LIMIT_EXCEEDED
Language=ENU
Log service has reached the maximum allowable containers allocated to a log file.
.
MessageId=6618
SymbolicName=ERROR_LOG_START_OF_LOG
Language=ENU
Log service has attempted to read or write backward past the start of the log.
.
MessageId=6619
SymbolicName=ERROR_LOG_POLICY_ALREADY_INSTALLED
Language=ENU
Log policy could not be installed because a policy of the same type is already present.
.
MessageId=6620
SymbolicName=ERROR_LOG_POLICY_NOT_INSTALLED
Language=ENU
Log policy in question was not installed at the time of the request.
.
MessageId=6621
SymbolicName=ERROR_LOG_POLICY_INVALID
Language=ENU
The installed set of policies on the log is invalid.
.
MessageId=6622
SymbolicName=ERROR_LOG_POLICY_CONFLICT
Language=ENU
A policy on the log in question prevented the operation from completing.
.
MessageId=6623
SymbolicName=ERROR_LOG_PINNED_ARCHIVE_TAIL
Language=ENU
Log space cannot be reclaimed because the log is pinned by the archive tail.
.
MessageId=6624
SymbolicName=ERROR_LOG_RECORD_NONEXISTENT
Language=ENU
Log record is not a record in the log file.
.
MessageId=6625
SymbolicName=ERROR_LOG_RECORDS_RESERVED_INVALID
Language=ENU
Number of reserved log records or the adjustment of the number of reserved log records is invalid.
.
MessageId=6626
SymbolicName=ERROR_LOG_SPACE_RESERVED_INVALID
Language=ENU
Reserved log space or the adjustment of the log space is invalid.
.
MessageId=6627
SymbolicName=ERROR_LOG_TAIL_INVALID
Language=ENU
An new or existing archive tail or base of the active log is invalid.
.
MessageId=6628
SymbolicName=ERROR_LOG_FULL
Language=ENU
Log space is exhausted.
.
MessageId=6629
SymbolicName=ERROR_COULD_NOT_RESIZE_LOG
Language=ENU
The log could not be set to the requested size.
.
MessageId=6630
SymbolicName=ERROR_LOG_MULTIPLEXED
Language=ENU
Log is multiplexed, no direct writes to the physical log is allowed.
.
MessageId=6631
SymbolicName=ERROR_LOG_DEDICATED
Language=ENU
The operation failed because the log is a dedicated log.
.
MessageId=6632
SymbolicName=ERROR_LOG_ARCHIVE_NOT_IN_PROGRESS
Language=ENU
The operation requires an archive context.
.
MessageId=6633
SymbolicName=ERROR_LOG_ARCHIVE_IN_PROGRESS
Language=ENU
Log archival is in progress.
.
MessageId=6634
SymbolicName=ERROR_LOG_EPHEMERAL
Language=ENU
The operation requires a non-ephemeral log, but the log is ephemeral.
.
MessageId=6635
SymbolicName=ERROR_LOG_NOT_ENOUGH_CONTAINERS
Language=ENU
The log must have at least two containers before it can be read from or written to.
.
MessageId=6636
SymbolicName=ERROR_LOG_CLIENT_ALREADY_REGISTERED
Language=ENU
A log client has already registered on the stream.
.
MessageId=6637
SymbolicName=ERROR_LOG_CLIENT_NOT_REGISTERED
Language=ENU
A log client has not been registered on the stream.
.
MessageId=6638
SymbolicName=ERROR_LOG_FULL_HANDLER_IN_PROGRESS
Language=ENU
A request has already been made to handle the log full condition.
.
MessageId=6639
SymbolicName=ERROR_LOG_CONTAINER_READ_FAILED
Language=ENU
Log service encountered an error when attempting to read from a log container.
.
MessageId=6640
SymbolicName=ERROR_LOG_CONTAINER_WRITE_FAILED
Language=ENU
Log service encountered an error when attempting to write to a log container.
.
MessageId=6641
SymbolicName=ERROR_LOG_CONTAINER_OPEN_FAILED
Language=ENU
Log service encountered an error when attempting open a log container.
.
MessageId=6642
SymbolicName=ERROR_LOG_CONTAINER_STATE_INVALID
Language=ENU
Log service encountered an invalid container state when attempting a requested action.
.
MessageId=6643
SymbolicName=ERROR_LOG_STATE_INVALID
Language=ENU
Log service is not in the correct state to perform a requested action.
.
MessageId=6644
SymbolicName=ERROR_LOG_PINNED
Language=ENU
Log space cannot be reclaimed because the log is pinned.
.
MessageId=6645
SymbolicName=ERROR_LOG_METADATA_FLUSH_FAILED
Language=ENU
Log metadata flush failed.
.
MessageId=6646
SymbolicName=ERROR_LOG_INCONSISTENT_SECURITY
Language=ENU
Security on the log and its containers is inconsistent.
.
MessageId=6647
SymbolicName=ERROR_LOG_APPENDED_FLUSH_FAILED
Language=ENU
Records were appended to the log or reservation changes were made, but the log could not be flushed.
.
MessageId=6648
SymbolicName=ERROR_LOG_PINNED_RESERVATION
Language=ENU
The log is pinned due to reservation consuming most of the log space. Free some reserved records to make space available.
.
MessageId=6700
SymbolicName=ERROR_INVALID_TRANSACTION
Language=ENU
The transaction handle associated with this operation is not valid.
.
MessageId=6701
SymbolicName=ERROR_TRANSACTION_NOT_ACTIVE
Language=ENU
The requested operation was made in the context of a transaction that is no longer active.
.
MessageId=6702
SymbolicName=ERROR_TRANSACTION_REQUEST_NOT_VALID
Language=ENU
The requested operation is not valid on the Transaction object in its current state.
.
MessageId=6703
SymbolicName=ERROR_TRANSACTION_NOT_REQUESTED
Language=ENU
The caller has called a response API, but the response is not expected because the TM did not issue the corresponding request to the caller.
.
MessageId=6704
SymbolicName=ERROR_TRANSACTION_ALREADY_ABORTED
Language=ENU
It is too late to perform the requested operation, since the Transaction has already been aborted.
.
MessageId=6705
SymbolicName=ERROR_TRANSACTION_ALREADY_COMMITTED
Language=ENU
It is too late to perform the requested operation, since the Transaction has already been committed.
.
MessageId=6706
SymbolicName=ERROR_TM_INITIALIZATION_FAILED
Language=ENU
The Transaction Manager was unable to be successfully initialized. Transacted operations are not supported.
.
MessageId=6707
SymbolicName=ERROR_RESOURCEMANAGER_READ_ONLY
Language=ENU
The specified ResourceManager made no changes or updates to the resource under this transaction.
.
MessageId=6708
SymbolicName=ERROR_TRANSACTION_NOT_JOINED
Language=ENU
The resource manager has attempted to prepare a transaction that it has not successfully joined.
.
MessageId=6709
SymbolicName=ERROR_TRANSACTION_SUPERIOR_EXISTS
Language=ENU
The Transaction object already has a superior enlistment, and the caller attempted an operation that would have created a new superior. Only a single superior enlistment is allow.
.
MessageId=6710
SymbolicName=ERROR_CRM_PROTOCOL_ALREADY_EXISTS
Language=ENU
The RM tried to register a protocol that already exists.
.
MessageId=6711
SymbolicName=ERROR_TRANSACTION_PROPAGATION_FAILED
Language=ENU
The attempt to propagate the Transaction failed.
.
MessageId=6712
SymbolicName=ERROR_CRM_PROTOCOL_NOT_FOUND
Language=ENU
The requested propagation protocol was not registered as a CRM.
.
MessageId=6713
SymbolicName=ERROR_TRANSACTION_INVALID_MARSHALL_BUFFER
Language=ENU
The buffer passed in to PushTransaction or PullTransaction is not in a valid format.
.
MessageId=6714
SymbolicName=ERROR_CURRENT_TRANSACTION_NOT_VALID
Language=ENU
The current transaction context associated with the thread is not a valid handle to a transaction object.
.
MessageId=6715
SymbolicName=ERROR_TRANSACTION_NOT_FOUND
Language=ENU
The specified Transaction object could not be opened, because it was not found.
.
MessageId=6716
SymbolicName=ERROR_RESOURCEMANAGER_NOT_FOUND
Language=ENU
The specified ResourceManager object could not be opened, because it was not found.
.
MessageId=6717
SymbolicName=ERROR_ENLISTMENT_NOT_FOUND
Language=ENU
The specified Enlistment object could not be opened, because it was not found.
.
MessageId=6718
SymbolicName=ERROR_TRANSACTIONMANAGER_NOT_FOUND
Language=ENU
The specified TransactionManager object could not be opened, because it was not found.
.
MessageId=6719
SymbolicName=ERROR_TRANSACTIONMANAGER_NOT_ONLINE
Language=ENU
The object specified could not be created or opened, because its associated TransactionManager is not online.  The TransactionManager must be brought fully Online by calling RecoverTransactionManager to recover to the end of its LogFile before objects in its Transaction or ResourceManager namespaces can be opened.  In addition, errors in writing records to its LogFile can cause a TransactionManager to go offline.
.
MessageId=6720
SymbolicName=ERROR_TRANSACTIONMANAGER_RECOVERY_NAME_COLLISION
Language=ENU
The specified TransactionManager was unable to create the objects contained in its logfile in the Ob namespace. Therefore, the TransactionManager was unable to recover.
.
MessageId=6721
SymbolicName=ERROR_TRANSACTION_NOT_ROOT
Language=ENU
The call to create a superior Enlistment on this Transaction object could not be completed, because the Transaction object specified for the enlistment is a subordinate branch of the Transaction. Only the root of the Transaction can be enlisted on as a superior.
.
MessageId=6722
SymbolicName=ERROR_TRANSACTION_OBJECT_EXPIRED
Language=ENU
Because the associated transaction manager or resource manager has been closed, the handle is no longer valid.
.
MessageId=6723
SymbolicName=ERROR_TRANSACTION_RESPONSE_NOT_ENLISTED
Language=ENU
The specified operation could not be performed on this Superior enlistment, because the enlistment was not created with the corresponding completion response in the NotificationMask.
.
MessageId=6724
SymbolicName=ERROR_TRANSACTION_RECORD_TOO_LONG
Language=ENU
The specified operation could not be performed, because the record that would be logged was too long. This can occur because of two conditions: either there are too many Enlistments on this Transaction, or the combined RecoveryInformation being logged on behalf of those Enlistments is too long.
.
MessageId=6725
SymbolicName=ERROR_IMPLICIT_TRANSACTION_NOT_SUPPORTED
Language=ENU
Implicit transaction are not supported.
.
MessageId=6726
SymbolicName=ERROR_TRANSACTION_INTEGRITY_VIOLATED
Language=ENU
The kernel transaction manager had to abort or forget the transaction because it blocked forward progress.
.
MessageId=6727
SymbolicName=ERROR_TRANSACTIONMANAGER_IDENTITY_MISMATCH
Language=ENU
The TransactionManager identity that was supplied did not match the one recorded in the TransactionManager's log file.
.
MessageId=6728
SymbolicName=ERROR_RM_CANNOT_BE_FROZEN_FOR_SNAPSHOT
Language=ENU
This snapshot operation cannot continue because a transactional resource manager cannot be frozen in its current state.  Please try again.
.
MessageId=6729
SymbolicName=ERROR_TRANSACTION_MUST_WRITETHROUGH
Language=ENU
The transaction cannot be enlisted on with the specified EnlistmentMask, because the transaction has already completed the PrePrepare phase.  In order to ensure correctness, the ResourceManager must switch to a write-through mode and cease caching data within this transaction.  Enlisting for only subsequent transaction phases may still succeed.
.
MessageId=6730
SymbolicName=ERROR_TRANSACTION_NO_SUPERIOR
Language=ENU
The transaction does not have a superior enlistment.
.
MessageId=6731
SymbolicName=ERROR_HEURISTIC_DAMAGE_POSSIBLE
Language=ENU
The attempt to commit the Transaction completed, but it is possible that some portion of the transaction tree did not commit successfully due to heuristics.  Therefore it is possible that some data modified in the transaction may not have committed, resulting in transactional inconsistency.  If possible, check the consistency of the associated data.
.
MessageId=6800
SymbolicName=ERROR_TRANSACTIONAL_CONFLICT
Language=ENU
The function attempted to use a name that is reserved for use by another transaction.
.
MessageId=6801
SymbolicName=ERROR_RM_NOT_ACTIVE
Language=ENU
Transaction support within the specified resource manager is not started or was shut down due to an error.
.
MessageId=6802
SymbolicName=ERROR_RM_METADATA_CORRUPT
Language=ENU
The metadata of the RM has been corrupted. The RM will not function.
.
MessageId=6803
SymbolicName=ERROR_DIRECTORY_NOT_RM
Language=ENU
The specified directory does not contain a resource manager.
.
MessageId=6805
SymbolicName=ERROR_TRANSACTIONS_UNSUPPORTED_REMOTE
Language=ENU
The remote server or share does not support transacted file operations.
.
MessageId=6806
SymbolicName=ERROR_LOG_RESIZE_INVALID_SIZE
Language=ENU
The requested log size is invalid.
.
MessageId=6807
SymbolicName=ERROR_OBJECT_NO_LONGER_EXISTS
Language=ENU
The object (file, stream, link) corresponding to the handle has been deleted by a Transaction Savepoint Rollback.
.
MessageId=6808
SymbolicName=ERROR_STREAM_MINIVERSION_NOT_FOUND
Language=ENU
The specified file miniversion was not found for this transacted file open.
.
MessageId=6809
SymbolicName=ERROR_STREAM_MINIVERSION_NOT_VALID
Language=ENU
The specified file miniversion was found but has been invalidated. Most likely cause is a transaction savepoint rollback.
.
MessageId=6810
SymbolicName=ERROR_MINIVERSION_INACCESSIBLE_FROM_SPECIFIED_TRANSACTION
Language=ENU
A miniversion may only be opened in the context of the transaction that created it.
.
MessageId=6811
SymbolicName=ERROR_CANT_OPEN_MINIVERSION_WITH_MODIFY_INTENT
Language=ENU
It is not possible to open a miniversion with modify access.
.
MessageId=6812
SymbolicName=ERROR_CANT_CREATE_MORE_STREAM_MINIVERSIONS
Language=ENU
It is not possible to create any more miniversions for this stream.
.
MessageId=6814
SymbolicName=ERROR_REMOTE_FILE_VERSION_MISMATCH
Language=ENU
The remote server sent mismatching version number or Fid for a file opened with transactions.
.
MessageId=6815
SymbolicName=ERROR_HANDLE_NO_LONGER_VALID
Language=ENU
The handle has been invalidated by a transaction. The most likely cause is the presence of memory mapping on a file or an open handle when the transaction ended or rolled back to savepoint.
.
MessageId=6816
SymbolicName=ERROR_NO_TXF_METADATA
Language=ENU
There is no transaction metadata on the file.
.
MessageId=6817
SymbolicName=ERROR_LOG_CORRUPTION_DETECTED
Language=ENU
The log data is corrupt.
.
MessageId=6818
SymbolicName=ERROR_CANT_RECOVER_WITH_HANDLE_OPEN
Language=ENU
The file can't be recovered because there is a handle still open on it.
.
MessageId=6819
SymbolicName=ERROR_RM_DISCONNECTED
Language=ENU
The transaction outcome is unavailable because the resource manager responsible for it has disconnected.
.
MessageId=6820
SymbolicName=ERROR_ENLISTMENT_NOT_SUPERIOR
Language=ENU
The request was rejected because the enlistment in question is not a superior enlistment.
.
MessageId=6821
SymbolicName=ERROR_RECOVERY_NOT_NEEDED
Language=ENU
The transactional resource manager is already consistent. Recovery is not needed.
.
MessageId=6822
SymbolicName=ERROR_RM_ALREADY_STARTED
Language=ENU
The transactional resource manager has already been started.
.
MessageId=6823
SymbolicName=ERROR_FILE_IDENTITY_NOT_PERSISTENT
Language=ENU
The file cannot be opened transactionally, because its identity depends on the outcome of an unresolved transaction.
.
MessageId=6824
SymbolicName=ERROR_CANT_BREAK_TRANSACTIONAL_DEPENDENCY
Language=ENU
The operation cannot be performed because another transaction is depending on the fact that this property will not change.
.
MessageId=6825
SymbolicName=ERROR_CANT_CROSS_RM_BOUNDARY
Language=ENU
The operation would involve a single file with two transactional resource managers and is therefore not allowed.
.
MessageId=6826
SymbolicName=ERROR_TXF_DIR_NOT_EMPTY
Language=ENU
The $Txf directory must be empty for this operation to succeed.
.
MessageId=6827
SymbolicName=ERROR_INDOUBT_TRANSACTIONS_EXIST
Language=ENU
The operation would leave a transactional resource manager in an inconsistent state and is therefore not allowed.
.
MessageId=6828
SymbolicName=ERROR_TM_VOLATILE
Language=ENU
The operation could not be completed because the transaction manager does not have a log.
.
MessageId=6829
SymbolicName=ERROR_ROLLBACK_TIMER_EXPIRED
Language=ENU
A rollback could not be scheduled because a previously scheduled rollback has already executed or been queued for execution.
.
MessageId=6830
SymbolicName=ERROR_TXF_ATTRIBUTE_CORRUPT
Language=ENU
The transactional metadata attribute on the file or directory is corrupt and unreadable.
.
MessageId=6831
SymbolicName=ERROR_EFS_NOT_ALLOWED_IN_TRANSACTION
Language=ENU
The encryption operation could not be completed because a transaction is active.
.
MessageId=6832
SymbolicName=ERROR_TRANSACTIONAL_OPEN_NOT_ALLOWED
Language=ENU
This object is not allowed to be opened in a transaction.
.
MessageId=6833
SymbolicName=ERROR_LOG_GROWTH_FAILED
Language=ENU
An attempt to create space in the transactional resource manager's log failed. The failure status has been recorded in the event log.
.
MessageId=6834
SymbolicName=ERROR_TRANSACTED_MAPPING_UNSUPPORTED_REMOTE
Language=ENU
Memory mapping (creating a mapped section) a remote file under a transaction is not supported.
.
MessageId=6835
SymbolicName=ERROR_TXF_METADATA_ALREADY_PRESENT
Language=ENU
Transaction metadata is already present on this file and cannot be superseded.
.
MessageId=6836
SymbolicName=ERROR_TRANSACTION_SCOPE_CALLBACKS_NOT_SET
Language=ENU
A transaction scope could not be entered because the scope handler has not been initialized.
.
MessageId=6837
SymbolicName=ERROR_TRANSACTION_REQUIRED_PROMOTION
Language=ENU
Promotion was required in order to allow the resource manager to enlist, but the transaction was set to disallow it.
.
MessageId=6838
SymbolicName=ERROR_CANNOT_EXECUTE_FILE_IN_TRANSACTION
Language=ENU
This file is open for modification in an unresolved transaction and may be opened for execute only by a transacted reader.
.
MessageId=6839
SymbolicName=ERROR_TRANSACTIONS_NOT_FROZEN
Language=ENU
The request to thaw frozen transactions was ignored because transactions had not previously been frozen.
.
MessageId=6840
SymbolicName=ERROR_TRANSACTION_FREEZE_IN_PROGRESS
Language=ENU
Transactions cannot be frozen because a freeze is already in progress.
.
MessageId=6841
SymbolicName=ERROR_NOT_SNAPSHOT_VOLUME
Language=ENU
The target volume is not a snapshot volume. This operation is only valid on a volume mounted as a snapshot.
.
MessageId=6842
SymbolicName=ERROR_NO_SAVEPOINT_WITH_OPEN_FILES
Language=ENU
The savepoint operation failed because files are open on the transaction. This is not permitted.
.
MessageId=6843
SymbolicName=ERROR_DATA_LOST_REPAIR
Language=ENU
Windows has discovered corruption in a file, and that file has since been repaired. Data loss may have occurred.
.
MessageId=6844
SymbolicName=ERROR_SPARSE_NOT_ALLOWED_IN_TRANSACTION
Language=ENU
The sparse operation could not be completed because a transaction is active on the file.
.
MessageId=6845
SymbolicName=ERROR_TM_IDENTITY_MISMATCH
Language=ENU
The call to create a TransactionManager object failed because the Tm Identity stored in the logfile does not match the Tm Identity that was passed in as an argument.
.
MessageId=6846
SymbolicName=ERROR_FLOATED_SECTION
Language=ENU
I/O was attempted on a section object that has been floated as a result of a transaction ending. There is no valid data.
.
MessageId=6847
SymbolicName=ERROR_CANNOT_ACCEPT_TRANSACTED_WORK
Language=ENU
The transactional resource manager cannot currently accept transacted work due to a transient condition such as low resources.
.
MessageId=6848
SymbolicName=ERROR_CANNOT_ABORT_TRANSACTIONS
Language=ENU
The transactional resource manager had too many transactions outstanding that could not be aborted. The transactional resource manger has been shut down.
.
MessageId=6849
SymbolicName=ERROR_BAD_CLUSTERS
Language=ENU
The operation could not be completed due to bad clusters on disk.
.
MessageId=6850
SymbolicName=ERROR_COMPRESSION_NOT_ALLOWED_IN_TRANSACTION
Language=ENU
The compression operation could not be completed because a transaction is active on the file.
.
MessageId=6851
SymbolicName=ERROR_VOLUME_DIRTY
Language=ENU
The operation could not be completed because the volume is dirty. Please run chkdsk and try again.
.
MessageId=6852
SymbolicName=ERROR_NO_LINK_TRACKING_IN_TRANSACTION
Language=ENU
The link tracking operation could not be completed because a transaction is active.
.
MessageId=6853
SymbolicName=ERROR_OPERATION_NOT_SUPPORTED_IN_TRANSACTION
Language=ENU
This operation cannot be performed in a transaction.
.
MessageId=6854
SymbolicName=ERROR_EXPIRED_HANDLE
Language=ENU
The handle is no longer properly associated with its transaction.  It may have been opened in a transactional resource manager that was subsequently forced to restart.  Please close the handle and open a new one.
.
MessageId=6855
SymbolicName=ERROR_TRANSACTION_NOT_ENLISTED
Language=ENU
The specified operation could not be performed because the resource manager is not enlisted in the transaction.
.
MessageId=6856
SymbolicName=WINERR_6856
Language=ENU
The transaction is linked to an enlistment that is not fully initialized yet.
.
MessageId=7001
SymbolicName=ERROR_CTX_WINSTATION_NAME_INVALID
Language=ENU
The specified session name is invalid.
.
MessageId=7002
SymbolicName=ERROR_CTX_INVALID_PD
Language=ENU
The specified protocol driver is invalid.
.
MessageId=7003
SymbolicName=ERROR_CTX_PD_NOT_FOUND
Language=ENU
The specified protocol driver was not found in the system path.
.
MessageId=7004
SymbolicName=ERROR_CTX_WD_NOT_FOUND
Language=ENU
The specified terminal connection driver was not found in the system path.
.
MessageId=7005
SymbolicName=ERROR_CTX_CANNOT_MAKE_EVENTLOG_ENTRY
Language=ENU
A registry key for event logging could not be created for this session.
.
MessageId=7006
SymbolicName=ERROR_CTX_SERVICE_NAME_COLLISION
Language=ENU
A service with the same name already exists on the system.
.
MessageId=7007
SymbolicName=ERROR_CTX_CLOSE_PENDING
Language=ENU
A close operation is pending on the session.
.
MessageId=7008
SymbolicName=ERROR_CTX_NO_OUTBUF
Language=ENU
There are no free output buffers available.
.
MessageId=7009
SymbolicName=ERROR_CTX_MODEM_INF_NOT_FOUND
Language=ENU
The MODEM.INF file was not found.
.
MessageId=7010
SymbolicName=ERROR_CTX_INVALID_MODEMNAME
Language=ENU
The modem name was not found in MODEM.INF.
.
MessageId=7011
SymbolicName=ERROR_CTX_MODEM_RESPONSE_ERROR
Language=ENU
The modem did not accept the command sent to it. Verify that the configured modem name matches the attached modem.
.
MessageId=7012
SymbolicName=ERROR_CTX_MODEM_RESPONSE_TIMEOUT
Language=ENU
The modem did not respond to the command sent to it. Verify that the modem is properly cabled and powered on.
.
MessageId=7013
SymbolicName=ERROR_CTX_MODEM_RESPONSE_NO_CARRIER
Language=ENU
Carrier detect has failed or carrier has been dropped due to disconnect.
.
MessageId=7014
SymbolicName=ERROR_CTX_MODEM_RESPONSE_NO_DIALTONE
Language=ENU
Dial tone not detected within the required time. Verify that the phone cable is properly attached and functional.
.
MessageId=7015
SymbolicName=ERROR_CTX_MODEM_RESPONSE_BUSY
Language=ENU
Busy signal detected at remote site on callback.
.
MessageId=7016
SymbolicName=ERROR_CTX_MODEM_RESPONSE_VOICE
Language=ENU
Voice detected at remote site on callback.
.
MessageId=7017
SymbolicName=ERROR_CTX_TD_ERROR
Language=ENU
Transport driver error
.
MessageId=7022
SymbolicName=ERROR_CTX_WINSTATION_NOT_FOUND
Language=ENU
The specified session cannot be found.
.
MessageId=7023
SymbolicName=ERROR_CTX_WINSTATION_ALREADY_EXISTS
Language=ENU
The specified session name is already in use.
.
MessageId=7024
SymbolicName=ERROR_CTX_WINSTATION_BUSY
Language=ENU
The task you are trying to do can't be completed because Remote Desktop Services is currently busy. Please try again in a few minutes. Other users should still be able to log on.
.
MessageId=7025
SymbolicName=ERROR_CTX_BAD_VIDEO_MODE
Language=ENU
An attempt has been made to connect to a session whose video mode is not supported by the current client.
.
MessageId=7035
SymbolicName=ERROR_CTX_GRAPHICS_INVALID
Language=ENU
The application attempted to enable DOS graphics mode. DOS graphics mode is not supported.
.
MessageId=7037
SymbolicName=ERROR_CTX_LOGON_DISABLED
Language=ENU
Your interactive logon privilege has been disabled. Please contact your administrator.
.
MessageId=7038
SymbolicName=ERROR_CTX_NOT_CONSOLE
Language=ENU
The requested operation can be performed only on the system console. This is most often the result of a driver or system DLL requiring direct console access.
.
MessageId=7040
SymbolicName=ERROR_CTX_CLIENT_QUERY_TIMEOUT
Language=ENU
The client failed to respond to the server connect message.
.
MessageId=7041
SymbolicName=ERROR_CTX_CONSOLE_DISCONNECT
Language=ENU
Disconnecting the console session is not supported.
.
MessageId=7042
SymbolicName=ERROR_CTX_CONSOLE_CONNECT
Language=ENU
Reconnecting a disconnected session to the console is not supported.
.
MessageId=7044
SymbolicName=ERROR_CTX_SHADOW_DENIED
Language=ENU
The request to control another session remotely was denied.
.
MessageId=7045
SymbolicName=ERROR_CTX_WINSTATION_ACCESS_DENIED
Language=ENU
The requested session access is denied.
.
MessageId=7049
SymbolicName=ERROR_CTX_INVALID_WD
Language=ENU
The specified terminal connection driver is invalid.
.
MessageId=7050
SymbolicName=ERROR_CTX_SHADOW_INVALID
Language=ENU
The requested session cannot be controlled remotely.
.
MessageId=7051
SymbolicName=ERROR_CTX_SHADOW_DISABLED
Language=ENU
The requested session is not configured to allow remote control.
.
MessageId=7052
SymbolicName=ERROR_CTX_CLIENT_LICENSE_IN_USE
Language=ENU
Your request to connect to this Terminal Server has been rejected. Your Terminal Server client license number is currently being used by another user. Please call your system administrator to obtain a unique license number.
.
MessageId=7053
SymbolicName=ERROR_CTX_CLIENT_LICENSE_NOT_SET
Language=ENU
Your request to connect to this Terminal Server has been rejected. Your Terminal Server client license number has not been entered for this copy of the Terminal Server client. Please contact your system administrator.
.
MessageId=7054
SymbolicName=ERROR_CTX_LICENSE_NOT_AVAILABLE
Language=ENU
The number of connections to this computer is limited and all connections are in use right now. Try connecting later or contact your system administrator.
.
MessageId=7055
SymbolicName=ERROR_CTX_LICENSE_CLIENT_INVALID
Language=ENU
The client you are using is not licensed to use this system. Your logon request is denied.
.
MessageId=7056
SymbolicName=ERROR_CTX_LICENSE_EXPIRED
Language=ENU
The system license has expired. Your logon request is denied.
.
MessageId=7057
SymbolicName=ERROR_CTX_SHADOW_NOT_RUNNING
Language=ENU
Remote control could not be terminated because the specified session is not currently being remotely controlled.
.
MessageId=7058
SymbolicName=ERROR_CTX_SHADOW_ENDED_BY_MODE_CHANGE
Language=ENU
The remote control of the console was terminated because the display mode was changed. Changing the display mode in a remote control session is not supported.
.
MessageId=7059
SymbolicName=ERROR_ACTIVATION_COUNT_EXCEEDED
Language=ENU
Activation has already been reset the maximum number of times for this installation. Your activation timer will not be cleared.
.
MessageId=7060
SymbolicName=ERROR_CTX_WINSTATIONS_DISABLED
Language=ENU
Remote logins are currently disabled.
.
MessageId=7061
SymbolicName=ERROR_CTX_ENCRYPTION_LEVEL_REQUIRED
Language=ENU
You do not have the proper encryption level to access this Session.
.
MessageId=7062
SymbolicName=ERROR_CTX_SESSION_IN_USE
Language=ENU
The user %s\\%s is currently logged on to this computer. Only the current user or an administrator can log on to this computer.
.
MessageId=7063
SymbolicName=ERROR_CTX_NO_FORCE_LOGOFF
Language=ENU
The user %s\\%s is already logged on to the console of this computer. You do not have permission to log in at this time. To resolve this issue, contact %s\\%s and have them log off.
.
MessageId=7064
SymbolicName=ERROR_CTX_ACCOUNT_RESTRICTION
Language=ENU
Unable to log you on because of an account restriction.
.
MessageId=7065
SymbolicName=ERROR_RDP_PROTOCOL_ERROR
Language=ENU
The RDP protocol component %2 detected an error in the protocol stream and has disconnected the client.
.
MessageId=7066
SymbolicName=ERROR_CTX_CDM_CONNECT
Language=ENU
The Client Drive Mapping Service Has Connected on Terminal Connection.
.
MessageId=7067
SymbolicName=ERROR_CTX_CDM_DISCONNECT
Language=ENU
The Client Drive Mapping Service Has Disconnected on Terminal Connection.
.
MessageId=7068
SymbolicName=ERROR_CTX_SECURITY_LAYER_ERROR
Language=ENU
The Terminal Server security layer detected an error in the protocol stream and has disconnected the client.
.
MessageId=7069
SymbolicName=ERROR_TS_INCOMPATIBLE_SESSIONS
Language=ENU
The target session is incompatible with the current session.
.
MessageId=7070
SymbolicName=ERROR_TS_VIDEO_SUBSYSTEM_ERROR
Language=ENU
Windows can't connect to your session because a problem occurred in the Windows video subsystem. Try connecting again later, or contact the server administrator for assistance.
.
MessageId=8001
SymbolicName=WINERR_8001
Language=ENU
The file replication service API was called incorrectly.
.
MessageId=8002
SymbolicName=WINERR_8002
Language=ENU
The file replication service cannot be started.
.
MessageId=8003
SymbolicName=WINERR_8003
Language=ENU
The file replication service cannot be stopped.
.
MessageId=8004
SymbolicName=WINERR_8004
Language=ENU
The file replication service API terminated the request. The event log may have more information.
.
MessageId=8005
SymbolicName=WINERR_8005
Language=ENU
The file replication service terminated the request. The event log may have more information.
.
MessageId=8006
SymbolicName=WINERR_8006
Language=ENU
The file replication service cannot be contacted. The event log may have more information.
.
MessageId=8007
SymbolicName=WINERR_8007
Language=ENU
The file replication service cannot satisfy the request because the user has insufficient privileges. The event log may have more information.
.
MessageId=8008
SymbolicName=WINERR_8008
Language=ENU
The file replication service cannot satisfy the request because authenticated RPC is not available. The event log may have more information.
.
MessageId=8009
SymbolicName=WINERR_8009
Language=ENU
The file replication service cannot satisfy the request because the user has insufficient privileges on the domain controller. The event log may have more information.
.
MessageId=8010
SymbolicName=WINERR_8010
Language=ENU
The file replication service cannot satisfy the request because authenticated RPC is not available on the domain controller. The event log may have more information.
.
MessageId=8011
SymbolicName=WINERR_8011
Language=ENU
The file replication service cannot communicate with the file replication service on the domain controller. The event log may have more information.
.
MessageId=8012
SymbolicName=WINERR_8012
Language=ENU
The file replication service on the domain controller cannot communicate with the file replication service on this computer. The event log may have more information.
.
MessageId=8013
SymbolicName=WINERR_8013
Language=ENU
The file replication service cannot populate the system volume because of an internal error. The event log may have more information.
.
MessageId=8014
SymbolicName=WINERR_8014
Language=ENU
The file replication service cannot populate the system volume because of an internal timeout. The event log may have more information.
.
MessageId=8015
SymbolicName=WINERR_8015
Language=ENU
The file replication service cannot process the request. The system volume is busy with a previous request.
.
MessageId=8016
SymbolicName=WINERR_8016
Language=ENU
The file replication service cannot stop replicating the system volume because of an internal error. The event log may have more information.
.
MessageId=8017
SymbolicName=WINERR_8017
Language=ENU
The file replication service detected an invalid parameter.
.
MessageId=8200
SymbolicName=ERROR_DS_NOT_INSTALLED
Language=ENU
An error occurred while installing the directory service. For more information, see the event log.
.
MessageId=8201
SymbolicName=ERROR_DS_MEMBERSHIP_EVALUATED_LOCALLY
Language=ENU
The directory service evaluated group memberships locally.
.
MessageId=8202
SymbolicName=ERROR_DS_NO_ATTRIBUTE_OR_VALUE
Language=ENU
The specified directory service attribute or value does not exist.
.
MessageId=8203
SymbolicName=ERROR_DS_INVALID_ATTRIBUTE_SYNTAX
Language=ENU
The attribute syntax specified to the directory service is invalid.
.
MessageId=8204
SymbolicName=ERROR_DS_ATTRIBUTE_TYPE_UNDEFINED
Language=ENU
The attribute type specified to the directory service is not defined.
.
MessageId=8205
SymbolicName=ERROR_DS_ATTRIBUTE_OR_VALUE_EXISTS
Language=ENU
The specified directory service attribute or value already exists.
.
MessageId=8206
SymbolicName=ERROR_DS_BUSY
Language=ENU
The directory service is busy.
.
MessageId=8207
SymbolicName=ERROR_DS_UNAVAILABLE
Language=ENU
The directory service is unavailable.
.
MessageId=8208
SymbolicName=ERROR_DS_NO_RIDS_ALLOCATED
Language=ENU
The directory service was unable to allocate a relative identifier.
.
MessageId=8209
SymbolicName=ERROR_DS_NO_MORE_RIDS
Language=ENU
The directory service has exhausted the pool of relative identifiers.
.
MessageId=8210
SymbolicName=ERROR_DS_INCORRECT_ROLE_OWNER
Language=ENU
The requested operation could not be performed because the directory service is not the master for that type of operation.
.
MessageId=8211
SymbolicName=ERROR_DS_RIDMGR_INIT_ERROR
Language=ENU
The directory service was unable to initialize the subsystem that allocates relative identifiers.
.
MessageId=8212
SymbolicName=ERROR_DS_OBJ_CLASS_VIOLATION
Language=ENU
The requested operation did not satisfy one or more constraints associated with the class of the object.
.
MessageId=8213
SymbolicName=ERROR_DS_CANT_ON_NON_LEAF
Language=ENU
The directory service can perform the requested operation only on a leaf object.
.
MessageId=8214
SymbolicName=ERROR_DS_CANT_ON_RDN
Language=ENU
The directory service cannot perform the requested operation on the RDN attribute of an object.
.
MessageId=8215
SymbolicName=ERROR_DS_CANT_MOD_OBJ_CLASS
Language=ENU
The directory service detected an attempt to modify the object class of an object.
.
MessageId=8216
SymbolicName=ERROR_DS_CROSS_DOM_MOVE_ERROR
Language=ENU
The requested cross-domain move operation could not be performed.
.
MessageId=8217
SymbolicName=ERROR_DS_GC_NOT_AVAILABLE
Language=ENU
Unable to contact the global catalog server.
.
MessageId=8218
SymbolicName=ERROR_SHARED_POLICY
Language=ENU
The policy object is shared and can only be modified at the root.
.
MessageId=8219
SymbolicName=ERROR_POLICY_OBJECT_NOT_FOUND
Language=ENU
The policy object does not exist.
.
MessageId=8220
SymbolicName=ERROR_POLICY_ONLY_IN_DS
Language=ENU
The requested policy information is only in the directory service.
.
MessageId=8221
SymbolicName=ERROR_PROMOTION_ACTIVE
Language=ENU
A domain controller promotion is currently active.
.
MessageId=8222
SymbolicName=ERROR_NO_PROMOTION_ACTIVE
Language=ENU
A domain controller promotion is not currently active
.
MessageId=8224
SymbolicName=ERROR_DS_OPERATIONS_ERROR
Language=ENU
An operations error occurred.
.
MessageId=8225
SymbolicName=ERROR_DS_PROTOCOL_ERROR
Language=ENU
A protocol error occurred.
.
MessageId=8226
SymbolicName=ERROR_DS_TIMELIMIT_EXCEEDED
Language=ENU
The time limit for this request was exceeded.
.
MessageId=8227
SymbolicName=ERROR_DS_SIZELIMIT_EXCEEDED
Language=ENU
The size limit for this request was exceeded.
.
MessageId=8228
SymbolicName=ERROR_DS_ADMIN_LIMIT_EXCEEDED
Language=ENU
The administrative limit for this request was exceeded.
.
MessageId=8229
SymbolicName=ERROR_DS_COMPARE_FALSE
Language=ENU
The compare response was false.
.
MessageId=8230
SymbolicName=ERROR_DS_COMPARE_TRUE
Language=ENU
The compare response was true.
.
MessageId=8231
SymbolicName=ERROR_DS_AUTH_METHOD_NOT_SUPPORTED
Language=ENU
The requested authentication method is not supported by the server.
.
MessageId=8232
SymbolicName=ERROR_DS_STRONG_AUTH_REQUIRED
Language=ENU
A more secure authentication method is required for this server.
.
MessageId=8233
SymbolicName=ERROR_DS_INAPPROPRIATE_AUTH
Language=ENU
Inappropriate authentication.
.
MessageId=8234
SymbolicName=ERROR_DS_AUTH_UNKNOWN
Language=ENU
The authentication mechanism is unknown.
.
MessageId=8235
SymbolicName=ERROR_DS_REFERRAL
Language=ENU
A referral was returned from the server.
.
MessageId=8236
SymbolicName=ERROR_DS_UNAVAILABLE_CRIT_EXTENSION
Language=ENU
The server does not support the requested critical extension.
.
MessageId=8237
SymbolicName=ERROR_DS_CONFIDENTIALITY_REQUIRED
Language=ENU
This request requires a secure connection.
.
MessageId=8238
SymbolicName=ERROR_DS_INAPPROPRIATE_MATCHING
Language=ENU
Inappropriate matching.
.
MessageId=8239
SymbolicName=ERROR_DS_CONSTRAINT_VIOLATION
Language=ENU
A constraint violation occurred.
.
MessageId=8240
SymbolicName=ERROR_DS_NO_SUCH_OBJECT
Language=ENU
There is no such object on the server.
.
MessageId=8241
SymbolicName=ERROR_DS_ALIAS_PROBLEM
Language=ENU
There is an alias problem.
.
MessageId=8242
SymbolicName=ERROR_DS_INVALID_DN_SYNTAX
Language=ENU
An invalid dn syntax has been specified.
.
MessageId=8243
SymbolicName=ERROR_DS_IS_LEAF
Language=ENU
The object is a leaf object.
.
MessageId=8244
SymbolicName=ERROR_DS_ALIAS_DEREF_PROBLEM
Language=ENU
There is an alias dereferencing problem.
.
MessageId=8245
SymbolicName=ERROR_DS_UNWILLING_TO_PERFORM
Language=ENU
The server is unwilling to process the request.
.
MessageId=8246
SymbolicName=ERROR_DS_LOOP_DETECT
Language=ENU
A loop has been detected.
.
MessageId=8247
SymbolicName=ERROR_DS_NAMING_VIOLATION
Language=ENU
There is a naming violation.
.
MessageId=8248
SymbolicName=ERROR_DS_OBJECT_RESULTS_TOO_LARGE
Language=ENU
The result set is too large.
.
MessageId=8249
SymbolicName=ERROR_DS_AFFECTS_MULTIPLE_DSAS
Language=ENU
The operation affects multiple DSAs
.
MessageId=8250
SymbolicName=ERROR_DS_SERVER_DOWN
Language=ENU
The server is not operational.
.
MessageId=8251
SymbolicName=ERROR_DS_LOCAL_ERROR
Language=ENU
A local error has occurred.
.
MessageId=8252
SymbolicName=ERROR_DS_ENCODING_ERROR
Language=ENU
An encoding error has occurred.
.
MessageId=8253
SymbolicName=ERROR_DS_DECODING_ERROR
Language=ENU
A decoding error has occurred.
.
MessageId=8254
SymbolicName=ERROR_DS_FILTER_UNKNOWN
Language=ENU
The search filter cannot be recognized.
.
MessageId=8255
SymbolicName=ERROR_DS_PARAM_ERROR
Language=ENU
One or more parameters are illegal.
.
MessageId=8256
SymbolicName=ERROR_DS_NOT_SUPPORTED
Language=ENU
The specified method is not supported.
.
MessageId=8257
SymbolicName=ERROR_DS_NO_RESULTS_RETURNED
Language=ENU
No results were returned.
.
MessageId=8258
SymbolicName=ERROR_DS_CONTROL_NOT_FOUND
Language=ENU
The specified control is not supported by the server.
.
MessageId=8259
SymbolicName=ERROR_DS_CLIENT_LOOP
Language=ENU
A referral loop was detected by the client.
.
MessageId=8260
SymbolicName=ERROR_DS_REFERRAL_LIMIT_EXCEEDED
Language=ENU
The preset referral limit was exceeded.
.
MessageId=8261
SymbolicName=ERROR_DS_SORT_CONTROL_MISSING
Language=ENU
The search requires a SORT control.
.
MessageId=8262
SymbolicName=ERROR_DS_OFFSET_RANGE_ERROR
Language=ENU
The search results exceed the offset range specified.
.
MessageId=8263
SymbolicName=ERROR_DS_RIDMGR_DISABLED
Language=ENU
The directory service detected the subsystem that allocates relative identifiers is disabled. This can occur as a protective mechanism when the system determines a significant portion of relative identifiers (RIDs) have been exhausted. Please see http://go.microsoft.com/fwlink/?LinkId=228610 for recommended diagnostic steps and the procedure to re-enable account creation.
.
MessageId=8301
SymbolicName=ERROR_DS_ROOT_MUST_BE_NC
Language=ENU
The root object must be the head of a naming context. The root object cannot have an instantiated parent.
.
MessageId=8302
SymbolicName=ERROR_DS_ADD_REPLICA_INHIBITED
Language=ENU
The add replica operation cannot be performed. The naming context must be writeable in order to create the replica.
.
MessageId=8303
SymbolicName=ERROR_DS_ATT_NOT_DEF_IN_SCHEMA
Language=ENU
A reference to an attribute that is not defined in the schema occurred.
.
MessageId=8304
SymbolicName=ERROR_DS_MAX_OBJ_SIZE_EXCEEDED
Language=ENU
The maximum size of an object has been exceeded.
.
MessageId=8305
SymbolicName=ERROR_DS_OBJ_STRING_NAME_EXISTS
Language=ENU
An attempt was made to add an object to the directory with a name that is already in use.
.
MessageId=8306
SymbolicName=ERROR_DS_NO_RDN_DEFINED_IN_SCHEMA
Language=ENU
An attempt was made to add an object of a class that does not have an RDN defined in the schema.
.
MessageId=8307
SymbolicName=ERROR_DS_RDN_DOESNT_MATCH_SCHEMA
Language=ENU
An attempt was made to add an object using an RDN that is not the RDN defined in the schema.
.
MessageId=8308
SymbolicName=ERROR_DS_NO_REQUESTED_ATTS_FOUND
Language=ENU
None of the requested attributes were found on the objects.
.
MessageId=8309
SymbolicName=ERROR_DS_USER_BUFFER_TO_SMALL
Language=ENU
The user buffer is too small.
.
MessageId=8310
SymbolicName=ERROR_DS_ATT_IS_NOT_ON_OBJ
Language=ENU
The attribute specified in the operation is not present on the object.
.
MessageId=8311
SymbolicName=ERROR_DS_ILLEGAL_MOD_OPERATION
Language=ENU
Illegal modify operation. Some aspect of the modification is not permitted.
.
MessageId=8312
SymbolicName=ERROR_DS_OBJ_TOO_LARGE
Language=ENU
The specified object is too large.
.
MessageId=8313
SymbolicName=ERROR_DS_BAD_INSTANCE_TYPE
Language=ENU
The specified instance type is not valid.
.
MessageId=8314
SymbolicName=ERROR_DS_MASTERDSA_REQUIRED
Language=ENU
The operation must be performed at a master DSA.
.
MessageId=8315
SymbolicName=ERROR_DS_OBJECT_CLASS_REQUIRED
Language=ENU
The object class attribute must be specified.
.
MessageId=8316
SymbolicName=ERROR_DS_MISSING_REQUIRED_ATT
Language=ENU
A required attribute is missing.
.
MessageId=8317
SymbolicName=ERROR_DS_ATT_NOT_DEF_FOR_CLASS
Language=ENU
An attempt was made to modify an object to include an attribute that is not legal for its class.
.
MessageId=8318
SymbolicName=ERROR_DS_ATT_ALREADY_EXISTS
Language=ENU
The specified attribute is already present on the object.
.
MessageId=8320
SymbolicName=ERROR_DS_CANT_ADD_ATT_VALUES
Language=ENU
The specified attribute is not present, or has no values.
.
MessageId=8321
SymbolicName=ERROR_DS_SINGLE_VALUE_CONSTRAINT
Language=ENU
Multiple values were specified for an attribute that can have only one value.
.
MessageId=8322
SymbolicName=ERROR_DS_RANGE_CONSTRAINT
Language=ENU
A value for the attribute was not in the acceptable range of values.
.
MessageId=8323
SymbolicName=ERROR_DS_ATT_VAL_ALREADY_EXISTS
Language=ENU
The specified value already exists.
.
MessageId=8324
SymbolicName=ERROR_DS_CANT_REM_MISSING_ATT
Language=ENU
The attribute cannot be removed because it is not present on the object.
.
MessageId=8325
SymbolicName=ERROR_DS_CANT_REM_MISSING_ATT_VAL
Language=ENU
The attribute value cannot be removed because it is not present on the object.
.
MessageId=8326
SymbolicName=ERROR_DS_ROOT_CANT_BE_SUBREF
Language=ENU
The specified root object cannot be a subref.
.
MessageId=8327
SymbolicName=ERROR_DS_NO_CHAINING
Language=ENU
Chaining is not permitted.
.
MessageId=8328
SymbolicName=ERROR_DS_NO_CHAINED_EVAL
Language=ENU
Chained evaluation is not permitted.
.
MessageId=8329
SymbolicName=ERROR_DS_NO_PARENT_OBJECT
Language=ENU
The operation could not be performed because the object's parent is either uninstantiated or deleted.
.
MessageId=8330
SymbolicName=ERROR_DS_PARENT_IS_AN_ALIAS
Language=ENU
Having a parent that is an alias is not permitted. Aliases are leaf objects.
.
MessageId=8331
SymbolicName=ERROR_DS_CANT_MIX_MASTER_AND_REPS
Language=ENU
The object and parent must be of the same type, either both masters or both replicas.
.
MessageId=8332
SymbolicName=ERROR_DS_CHILDREN_EXIST
Language=ENU
The operation cannot be performed because child objects exist. This operation can only be performed on a leaf object.
.
MessageId=8333
SymbolicName=ERROR_DS_OBJ_NOT_FOUND
Language=ENU
Directory object not found.
.
MessageId=8334
SymbolicName=ERROR_DS_ALIASED_OBJ_MISSING
Language=ENU
The aliased object is missing.
.
MessageId=8335
SymbolicName=ERROR_DS_BAD_NAME_SYNTAX
Language=ENU
The object name has bad syntax.
.
MessageId=8336
SymbolicName=ERROR_DS_ALIAS_POINTS_TO_ALIAS
Language=ENU
It is not permitted for an alias to refer to another alias.
.
MessageId=8337
SymbolicName=ERROR_DS_CANT_DEREF_ALIAS
Language=ENU
The alias cannot be dereferenced.
.
MessageId=8338
SymbolicName=ERROR_DS_OUT_OF_SCOPE
Language=ENU
The operation is out of scope.
.
MessageId=8339
SymbolicName=ERROR_DS_OBJECT_BEING_REMOVED
Language=ENU
The operation cannot continue because the object is in the process of being removed.
.
MessageId=8340
SymbolicName=ERROR_DS_CANT_DELETE_DSA_OBJ
Language=ENU
The DSA object cannot be deleted.
.
MessageId=8341
SymbolicName=ERROR_DS_GENERIC_ERROR
Language=ENU
A directory service error has occurred.
.
MessageId=8342
SymbolicName=ERROR_DS_DSA_MUST_BE_INT_MASTER
Language=ENU
The operation can only be performed on an internal master DSA object.
.
MessageId=8343
SymbolicName=ERROR_DS_CLASS_NOT_DSA
Language=ENU
The object must be of class DSA.
.
MessageId=8344
SymbolicName=ERROR_DS_INSUFF_ACCESS_RIGHTS
Language=ENU
Insufficient access rights to perform the operation.
.
MessageId=8345
SymbolicName=ERROR_DS_ILLEGAL_SUPERIOR
Language=ENU
The object cannot be added because the parent is not on the list of possible superiors.
.
MessageId=8346
SymbolicName=ERROR_DS_ATTRIBUTE_OWNED_BY_SAM
Language=ENU
Access to the attribute is not permitted because the attribute is owned by the Security Accounts Manager (SAM).
.
MessageId=8347
SymbolicName=ERROR_DS_NAME_TOO_MANY_PARTS
Language=ENU
The name has too many parts.
.
MessageId=8348
SymbolicName=ERROR_DS_NAME_TOO_LONG
Language=ENU
The name is too long.
.
MessageId=8349
SymbolicName=ERROR_DS_NAME_VALUE_TOO_LONG
Language=ENU
The name value is too long.
.
MessageId=8350
SymbolicName=ERROR_DS_NAME_UNPARSEABLE
Language=ENU
The directory service encountered an error parsing a name.
.
MessageId=8351
SymbolicName=ERROR_DS_NAME_TYPE_UNKNOWN
Language=ENU
The directory service cannot get the attribute type for a name.
.
MessageId=8352
SymbolicName=ERROR_DS_NOT_AN_OBJECT
Language=ENU
The name does not identify an object; the name identifies a phantom.
.
MessageId=8353
SymbolicName=ERROR_DS_SEC_DESC_TOO_SHORT
Language=ENU
The security descriptor is too short.
.
MessageId=8354
SymbolicName=ERROR_DS_SEC_DESC_INVALID
Language=ENU
The security descriptor is invalid.
.
MessageId=8355
SymbolicName=ERROR_DS_NO_DELETED_NAME
Language=ENU
Failed to create name for deleted object.
.
MessageId=8356
SymbolicName=ERROR_DS_SUBREF_MUST_HAVE_PARENT
Language=ENU
The parent of a new subref must exist.
.
MessageId=8357
SymbolicName=ERROR_DS_NCNAME_MUST_BE_NC
Language=ENU
The object must be a naming context.
.
MessageId=8358
SymbolicName=ERROR_DS_CANT_ADD_SYSTEM_ONLY
Language=ENU
It is not permitted to add an attribute which is owned by the system.
.
MessageId=8359
SymbolicName=ERROR_DS_CLASS_MUST_BE_CONCRETE
Language=ENU
The class of the object must be structural; you cannot instantiate an abstract class.
.
MessageId=8360
SymbolicName=ERROR_DS_INVALID_DMD
Language=ENU
The schema object could not be found.
.
MessageId=8361
SymbolicName=ERROR_DS_OBJ_GUID_EXISTS
Language=ENU
A local object with this GUID (dead or alive) already exists.
.
MessageId=8362
SymbolicName=ERROR_DS_NOT_ON_BACKLINK
Language=ENU
The operation cannot be performed on a back link.
.
MessageId=8363
SymbolicName=ERROR_DS_NO_CROSSREF_FOR_NC
Language=ENU
The cross reference for the specified naming context could not be found.
.
MessageId=8364
SymbolicName=ERROR_DS_SHUTTING_DOWN
Language=ENU
The operation could not be performed because the directory service is shutting down.
.
MessageId=8365
SymbolicName=ERROR_DS_UNKNOWN_OPERATION
Language=ENU
The directory service request is invalid.
.
MessageId=8366
SymbolicName=ERROR_DS_INVALID_ROLE_OWNER
Language=ENU
The role owner attribute could not be read.
.
MessageId=8367
SymbolicName=ERROR_DS_COULDNT_CONTACT_FSMO
Language=ENU
The requested FSMO operation failed. The current FSMO holder could not be contacted.
.
MessageId=8368
SymbolicName=ERROR_DS_CROSS_NC_DN_RENAME
Language=ENU
Modification of a DN across a naming context is not permitted.
.
MessageId=8369
SymbolicName=ERROR_DS_CANT_MOD_SYSTEM_ONLY
Language=ENU
The attribute cannot be modified because it is owned by the system.
.
MessageId=8370
SymbolicName=ERROR_DS_REPLICATOR_ONLY
Language=ENU
Only the replicator can perform this function.
.
MessageId=8371
SymbolicName=ERROR_DS_OBJ_CLASS_NOT_DEFINED
Language=ENU
The specified class is not defined.
.
MessageId=8372
SymbolicName=ERROR_DS_OBJ_CLASS_NOT_SUBCLASS
Language=ENU
The specified class is not a subclass.
.
MessageId=8373
SymbolicName=ERROR_DS_NAME_REFERENCE_INVALID
Language=ENU
The name reference is invalid.
.
MessageId=8374
SymbolicName=ERROR_DS_CROSS_REF_EXISTS
Language=ENU
A cross reference already exists.
.
MessageId=8375
SymbolicName=ERROR_DS_CANT_DEL_MASTER_CROSSREF
Language=ENU
It is not permitted to delete a master cross reference.
.
MessageId=8376
SymbolicName=ERROR_DS_SUBTREE_NOTIFY_NOT_NC_HEAD
Language=ENU
Subtree notifications are only supported on NC heads.
.
MessageId=8377
SymbolicName=ERROR_DS_NOTIFY_FILTER_TOO_COMPLEX
Language=ENU
Notification filter is too complex.
.
MessageId=8378
SymbolicName=ERROR_DS_DUP_RDN
Language=ENU
Schema update failed: duplicate RDN.
.
MessageId=8379
SymbolicName=ERROR_DS_DUP_OID
Language=ENU
Schema update failed: duplicate OID.
.
MessageId=8380
SymbolicName=ERROR_DS_DUP_MAPI_ID
Language=ENU
Schema update failed: duplicate MAPI identifier.
.
MessageId=8381
SymbolicName=ERROR_DS_DUP_SCHEMA_ID_GUID
Language=ENU
Schema update failed: duplicate schema-id GUID.
.
MessageId=8382
SymbolicName=ERROR_DS_DUP_LDAP_DISPLAY_NAME
Language=ENU
Schema update failed: duplicate LDAP display name.
.
MessageId=8383
SymbolicName=ERROR_DS_SEMANTIC_ATT_TEST
Language=ENU
Schema update failed: range-lower less than range upper.
.
MessageId=8384
SymbolicName=ERROR_DS_SYNTAX_MISMATCH
Language=ENU
Schema update failed: syntax mismatch.
.
MessageId=8385
SymbolicName=ERROR_DS_EXISTS_IN_MUST_HAVE
Language=ENU
Schema deletion failed: attribute is used in must-contain.
.
MessageId=8386
SymbolicName=ERROR_DS_EXISTS_IN_MAY_HAVE
Language=ENU
Schema deletion failed: attribute is used in may-contain.
.
MessageId=8387
SymbolicName=ERROR_DS_NONEXISTENT_MAY_HAVE
Language=ENU
Schema update failed: attribute in may-contain does not exist.
.
MessageId=8388
SymbolicName=ERROR_DS_NONEXISTENT_MUST_HAVE
Language=ENU
Schema update failed: attribute in must-contain does not exist.
.
MessageId=8389
SymbolicName=ERROR_DS_AUX_CLS_TEST_FAIL
Language=ENU
Schema update failed: class in aux-class list does not exist or is not an auxiliary class.
.
MessageId=8390
SymbolicName=ERROR_DS_NONEXISTENT_POSS_SUP
Language=ENU
Schema update failed: class in poss-superiors does not exist.
.
MessageId=8391
SymbolicName=ERROR_DS_SUB_CLS_TEST_FAIL
Language=ENU
Schema update failed: class in subclassof list does not exist or does not satisfy hierarchy rules.
.
MessageId=8392
SymbolicName=ERROR_DS_BAD_RDN_ATT_ID_SYNTAX
Language=ENU
Schema update failed: Rdn-Att-Id has wrong syntax.
.
MessageId=8393
SymbolicName=ERROR_DS_EXISTS_IN_AUX_CLS
Language=ENU
Schema deletion failed: class is used as auxiliary class.
.
MessageId=8394
SymbolicName=ERROR_DS_EXISTS_IN_SUB_CLS
Language=ENU
Schema deletion failed: class is used as sub class.
.
MessageId=8395
SymbolicName=ERROR_DS_EXISTS_IN_POSS_SUP
Language=ENU
Schema deletion failed: class is used as poss superior.
.
MessageId=8396
SymbolicName=ERROR_DS_RECALCSCHEMA_FAILED
Language=ENU
Schema update failed in recalculating validation cache.
.
MessageId=8397
SymbolicName=ERROR_DS_TREE_DELETE_NOT_FINISHED
Language=ENU
The tree deletion is not finished. The request must be made again to continue deleting the tree.
.
MessageId=8398
SymbolicName=ERROR_DS_CANT_DELETE
Language=ENU
The requested delete operation could not be performed.
.
MessageId=8399
SymbolicName=ERROR_DS_ATT_SCHEMA_REQ_ID
Language=ENU
Cannot read the governs class identifier for the schema record.
.
MessageId=8400
SymbolicName=ERROR_DS_BAD_ATT_SCHEMA_SYNTAX
Language=ENU
The attribute schema has bad syntax.
.
MessageId=8401
SymbolicName=ERROR_DS_CANT_CACHE_ATT
Language=ENU
The attribute could not be cached.
.
MessageId=8402
SymbolicName=ERROR_DS_CANT_CACHE_CLASS
Language=ENU
The class could not be cached.
.
MessageId=8403
SymbolicName=ERROR_DS_CANT_REMOVE_ATT_CACHE
Language=ENU
The attribute could not be removed from the cache.
.
MessageId=8404
SymbolicName=ERROR_DS_CANT_REMOVE_CLASS_CACHE
Language=ENU
The class could not be removed from the cache.
.
MessageId=8405
SymbolicName=ERROR_DS_CANT_RETRIEVE_DN
Language=ENU
The distinguished name attribute could not be read.
.
MessageId=8406
SymbolicName=ERROR_DS_MISSING_SUPREF
Language=ENU
No superior reference has been configured for the directory service. The directory service is therefore unable to issue referrals to objects outside this forest.
.
MessageId=8407
SymbolicName=ERROR_DS_CANT_RETRIEVE_INSTANCE
Language=ENU
The instance type attribute could not be retrieved.
.
MessageId=8408
SymbolicName=ERROR_DS_CODE_INCONSISTENCY
Language=ENU
An internal error has occurred.
.
MessageId=8409
SymbolicName=ERROR_DS_DATABASE_ERROR
Language=ENU
A database error has occurred.
.
MessageId=8410
SymbolicName=ERROR_DS_GOVERNSID_MISSING
Language=ENU
The attribute GOVERNSID is missing.
.
MessageId=8411
SymbolicName=ERROR_DS_MISSING_EXPECTED_ATT
Language=ENU
An expected attribute is missing.
.
MessageId=8412
SymbolicName=ERROR_DS_NCNAME_MISSING_CR_REF
Language=ENU
The specified naming context is missing a cross reference.
.
MessageId=8413
SymbolicName=ERROR_DS_SECURITY_CHECKING_ERROR
Language=ENU
A security checking error has occurred.
.
MessageId=8414
SymbolicName=ERROR_DS_SCHEMA_NOT_LOADED
Language=ENU
The schema is not loaded.
.
MessageId=8415
SymbolicName=ERROR_DS_SCHEMA_ALLOC_FAILED
Language=ENU
Schema allocation failed. Please check if the machine is running low on memory.
.
MessageId=8416
SymbolicName=ERROR_DS_ATT_SCHEMA_REQ_SYNTAX
Language=ENU
Failed to obtain the required syntax for the attribute schema.
.
MessageId=8417
SymbolicName=ERROR_DS_GCVERIFY_ERROR
Language=ENU
The global catalog verification failed. The global catalog is not available or does not support the operation. Some part of the directory is currently not available.
.
MessageId=8418
SymbolicName=ERROR_DS_DRA_SCHEMA_MISMATCH
Language=ENU
The replication operation failed because of a schema mismatch between the servers involved.
.
MessageId=8419
SymbolicName=ERROR_DS_CANT_FIND_DSA_OBJ
Language=ENU
The DSA object could not be found.
.
MessageId=8420
SymbolicName=ERROR_DS_CANT_FIND_EXPECTED_NC
Language=ENU
The naming context could not be found.
.
MessageId=8421
SymbolicName=ERROR_DS_CANT_FIND_NC_IN_CACHE
Language=ENU
The naming context could not be found in the cache.
.
MessageId=8422
SymbolicName=ERROR_DS_CANT_RETRIEVE_CHILD
Language=ENU
The child object could not be retrieved.
.
MessageId=8423
SymbolicName=ERROR_DS_SECURITY_ILLEGAL_MODIFY
Language=ENU
The modification was not permitted for security reasons.
.
MessageId=8424
SymbolicName=ERROR_DS_CANT_REPLACE_HIDDEN_REC
Language=ENU
The operation cannot replace the hidden record.
.
MessageId=8425
SymbolicName=ERROR_DS_BAD_HIERARCHY_FILE
Language=ENU
The hierarchy file is invalid.
.
MessageId=8426
SymbolicName=ERROR_DS_BUILD_HIERARCHY_TABLE_FAILED
Language=ENU
The attempt to build the hierarchy table failed.
.
MessageId=8427
SymbolicName=ERROR_DS_CONFIG_PARAM_MISSING
Language=ENU
The directory configuration parameter is missing from the registry.
.
MessageId=8428
SymbolicName=ERROR_DS_COUNTING_AB_INDICES_FAILED
Language=ENU
The attempt to count the address book indices failed.
.
MessageId=8429
SymbolicName=ERROR_DS_HIERARCHY_TABLE_MALLOC_FAILED
Language=ENU
The allocation of the hierarchy table failed.
.
MessageId=8430
SymbolicName=ERROR_DS_INTERNAL_FAILURE
Language=ENU
The directory service encountered an internal failure.
.
MessageId=8431
SymbolicName=ERROR_DS_UNKNOWN_ERROR
Language=ENU
The directory service encountered an unknown failure.
.
MessageId=8432
SymbolicName=ERROR_DS_ROOT_REQUIRES_CLASS_TOP
Language=ENU
A root object requires a class of 'top'.
.
MessageId=8433
SymbolicName=ERROR_DS_REFUSING_FSMO_ROLES
Language=ENU
This directory server is shutting down, and cannot take ownership of new floating single-master operation roles.
.
MessageId=8434
SymbolicName=ERROR_DS_MISSING_FSMO_SETTINGS
Language=ENU
The directory service is missing mandatory configuration information, and is unable to determine the ownership of floating single-master operation roles.
.
MessageId=8435
SymbolicName=ERROR_DS_UNABLE_TO_SURRENDER_ROLES
Language=ENU
The directory service was unable to transfer ownership of one or more floating single-master operation roles to other servers.
.
MessageId=8436
SymbolicName=ERROR_DS_DRA_GENERIC
Language=ENU
The replication operation failed.
.
MessageId=8437
SymbolicName=ERROR_DS_DRA_INVALID_PARAMETER
Language=ENU
An invalid parameter was specified for this replication operation.
.
MessageId=8438
SymbolicName=ERROR_DS_DRA_BUSY
Language=ENU
The directory service is too busy to complete the replication operation at this time.
.
MessageId=8439
SymbolicName=ERROR_DS_DRA_BAD_DN
Language=ENU
The distinguished name specified for this replication operation is invalid.
.
MessageId=8440
SymbolicName=ERROR_DS_DRA_BAD_NC
Language=ENU
The naming context specified for this replication operation is invalid.
.
MessageId=8441
SymbolicName=ERROR_DS_DRA_DN_EXISTS
Language=ENU
The distinguished name specified for this replication operation already exists.
.
MessageId=8442
SymbolicName=ERROR_DS_DRA_INTERNAL_ERROR
Language=ENU
The replication system encountered an internal error.
.
MessageId=8443
SymbolicName=ERROR_DS_DRA_INCONSISTENT_DIT
Language=ENU
The replication operation encountered a database inconsistency.
.
MessageId=8444
SymbolicName=ERROR_DS_DRA_CONNECTION_FAILED
Language=ENU
The server specified for this replication operation could not be contacted.
.
MessageId=8445
SymbolicName=ERROR_DS_DRA_BAD_INSTANCE_TYPE
Language=ENU
The replication operation encountered an object with an invalid instance type.
.
MessageId=8446
SymbolicName=ERROR_DS_DRA_OUT_OF_MEM
Language=ENU
The replication operation failed to allocate memory.
.
MessageId=8447
SymbolicName=ERROR_DS_DRA_MAIL_PROBLEM
Language=ENU
The replication operation encountered an error with the mail system.
.
MessageId=8448
SymbolicName=ERROR_DS_DRA_REF_ALREADY_EXISTS
Language=ENU
The replication reference information for the target server already exists.
.
MessageId=8449
SymbolicName=ERROR_DS_DRA_REF_NOT_FOUND
Language=ENU
The replication reference information for the target server does not exist.
.
MessageId=8450
SymbolicName=ERROR_DS_DRA_OBJ_IS_REP_SOURCE
Language=ENU
The naming context cannot be removed because it is replicated to another server.
.
MessageId=8451
SymbolicName=ERROR_DS_DRA_DB_ERROR
Language=ENU
The replication operation encountered a database error.
.
MessageId=8452
SymbolicName=ERROR_DS_DRA_NO_REPLICA
Language=ENU
The naming context is in the process of being removed or is not replicated from the specified server.
.
MessageId=8453
SymbolicName=ERROR_DS_DRA_ACCESS_DENIED
Language=ENU
Replication access was denied.
.
MessageId=8454
SymbolicName=ERROR_DS_DRA_NOT_SUPPORTED
Language=ENU
The requested operation is not supported by this version of the directory service.
.
MessageId=8455
SymbolicName=ERROR_DS_DRA_RPC_CANCELLED
Language=ENU
The replication remote procedure call was cancelled.
.
MessageId=8456
SymbolicName=ERROR_DS_DRA_SOURCE_DISABLED
Language=ENU
The source server is currently rejecting replication requests.
.
MessageId=8457
SymbolicName=ERROR_DS_DRA_SINK_DISABLED
Language=ENU
The destination server is currently rejecting replication requests.
.
MessageId=8458
SymbolicName=ERROR_DS_DRA_NAME_COLLISION
Language=ENU
The replication operation failed due to a collision of object names.
.
MessageId=8459
SymbolicName=ERROR_DS_DRA_SOURCE_REINSTALLED
Language=ENU
The replication source has been reinstalled.
.
MessageId=8460
SymbolicName=ERROR_DS_DRA_MISSING_PARENT
Language=ENU
The replication operation failed because a required parent object is missing.
.
MessageId=8461
SymbolicName=ERROR_DS_DRA_PREEMPTED
Language=ENU
The replication operation was preempted.
.
MessageId=8462
SymbolicName=ERROR_DS_DRA_ABANDON_SYNC
Language=ENU
The replication synchronization attempt was abandoned because of a lack of updates.
.
MessageId=8463
SymbolicName=ERROR_DS_DRA_SHUTDOWN
Language=ENU
The replication operation was terminated because the system is shutting down.
.
MessageId=8464
SymbolicName=ERROR_DS_DRA_INCOMPATIBLE_PARTIAL_SET
Language=ENU
Synchronization attempt failed because the destination DC is currently waiting to synchronize new partial attributes from source. This condition is normal if a recent schema change modified the partial attribute set. The destination partial attribute set is not a subset of source partial attribute set.
.
MessageId=8465
SymbolicName=ERROR_DS_DRA_SOURCE_IS_PARTIAL_REPLICA
Language=ENU
The replication synchronization attempt failed because a master replica attempted to sync from a partial replica.
.
MessageId=8466
SymbolicName=ERROR_DS_DRA_EXTN_CONNECTION_FAILED
Language=ENU
The server specified for this replication operation was contacted, but that server was unable to contact an additional server needed to complete the operation.
.
MessageId=8467
SymbolicName=ERROR_DS_INSTALL_SCHEMA_MISMATCH
Language=ENU
The version of the directory service schema of the source forest is not compatible with the version of directory service on this computer.
.
MessageId=8468
SymbolicName=ERROR_DS_DUP_LINK_ID
Language=ENU
Schema update failed: An attribute with the same link identifier already exists.
.
MessageId=8469
SymbolicName=ERROR_DS_NAME_ERROR_RESOLVING
Language=ENU
Name translation: Generic processing error.
.
MessageId=8470
SymbolicName=ERROR_DS_NAME_ERROR_NOT_FOUND
Language=ENU
Name translation: Could not find the name or insufficient right to see name.
.
MessageId=8471
SymbolicName=ERROR_DS_NAME_ERROR_NOT_UNIQUE
Language=ENU
Name translation: Input name mapped to more than one output name.
.
MessageId=8472
SymbolicName=ERROR_DS_NAME_ERROR_NO_MAPPING
Language=ENU
Name translation: Input name found, but not the associated output format.
.
MessageId=8473
SymbolicName=ERROR_DS_NAME_ERROR_DOMAIN_ONLY
Language=ENU
Name translation: Unable to resolve completely, only the domain was found.
.
MessageId=8474
SymbolicName=ERROR_DS_NAME_ERROR_NO_SYNTACTICAL_MAPPING
Language=ENU
Name translation: Unable to perform purely syntactical mapping at the client without going out to the wire.
.
MessageId=8475
SymbolicName=ERROR_DS_CONSTRUCTED_ATT_MOD
Language=ENU
Modification of a constructed attribute is not allowed.
.
MessageId=8476
SymbolicName=ERROR_DS_WRONG_OM_OBJ_CLASS
Language=ENU
The OM-Object-Class specified is incorrect for an attribute with the specified syntax.
.
MessageId=8477
SymbolicName=ERROR_DS_DRA_REPL_PENDING
Language=ENU
The replication request has been posted; waiting for reply.
.
MessageId=8478
SymbolicName=ERROR_DS_DS_REQUIRED
Language=ENU
The requested operation requires a directory service, and none was available.
.
MessageId=8479
SymbolicName=ERROR_DS_INVALID_LDAP_DISPLAY_NAME
Language=ENU
The LDAP display name of the class or attribute contains non-ASCII characters.
.
MessageId=8480
SymbolicName=ERROR_DS_NON_BASE_SEARCH
Language=ENU
The requested search operation is only supported for base searches.
.
MessageId=8481
SymbolicName=ERROR_DS_CANT_RETRIEVE_ATTS
Language=ENU
The search failed to retrieve attributes from the database.
.
MessageId=8482
SymbolicName=ERROR_DS_BACKLINK_WITHOUT_LINK
Language=ENU
The schema update operation tried to add a backward link attribute that has no corresponding forward link.
.
MessageId=8483
SymbolicName=ERROR_DS_EPOCH_MISMATCH
Language=ENU
Source and destination of a cross-domain move do not agree on the object's epoch number. Either source or destination does not have the latest version of the object.
.
MessageId=8484
SymbolicName=ERROR_DS_SRC_NAME_MISMATCH
Language=ENU
Source and destination of a cross-domain move do not agree on the object's current name. Either source or destination does not have the latest version of the object.
.
MessageId=8485
SymbolicName=ERROR_DS_SRC_AND_DST_NC_IDENTICAL
Language=ENU
Source and destination for the cross-domain move operation are identical. Caller should use local move operation instead of cross-domain move operation.
.
MessageId=8486
SymbolicName=ERROR_DS_DST_NC_MISMATCH
Language=ENU
Source and destination for a cross-domain move are not in agreement on the naming contexts in the forest. Either source or destination does not have the latest version of the Partitions container.
.
MessageId=8487
SymbolicName=ERROR_DS_NOT_AUTHORITIVE_FOR_DST_NC
Language=ENU
Destination of a cross-domain move is not authoritative for the destination naming context.
.
MessageId=8488
SymbolicName=ERROR_DS_SRC_GUID_MISMATCH
Language=ENU
Source and destination of a cross-domain move do not agree on the identity of the source object. Either source or destination does not have the latest version of the source object.
.
MessageId=8489
SymbolicName=ERROR_DS_CANT_MOVE_DELETED_OBJECT
Language=ENU
Object being moved across-domains is already known to be deleted by the destination server. The source server does not have the latest version of the source object.
.
MessageId=8490
SymbolicName=ERROR_DS_PDC_OPERATION_IN_PROGRESS
Language=ENU
Another operation which requires exclusive access to the PDC FSMO is already in progress.
.
MessageId=8491
SymbolicName=ERROR_DS_CROSS_DOMAIN_CLEANUP_REQD
Language=ENU
A cross-domain move operation failed such that two versions of the moved object exist - one each in the source and destination domains. The destination object needs to be removed to restore the system to a consistent state.
.
MessageId=8492
SymbolicName=ERROR_DS_ILLEGAL_XDOM_MOVE_OPERATION
Language=ENU
This object may not be moved across domain boundaries either because cross-domain moves for this class are disallowed, or the object has some special characteristics, e.g.: trust account or restricted RID, which prevent its move.
.
MessageId=8493
SymbolicName=ERROR_DS_CANT_WITH_ACCT_GROUP_MEMBERSHPS
Language=ENU
Can't move objects with memberships across domain boundaries as once moved, this would violate the membership conditions of the account group. Remove the object from any account group memberships and retry.
.
MessageId=8494
SymbolicName=ERROR_DS_NC_MUST_HAVE_NC_PARENT
Language=ENU
A naming context head must be the immediate child of another naming context head, not of an interior node.
.
MessageId=8495
SymbolicName=ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE
Language=ENU
The directory cannot validate the proposed naming context name because it does not hold a replica of the naming context above the proposed naming context. Please ensure that the domain naming master role is held by a server that is configured as a global catalog server, and that the server is up to date with its replication partners. (Applies only to Windows 2000 Domain Naming masters)
.
MessageId=8496
SymbolicName=ERROR_DS_DST_DOMAIN_NOT_NATIVE
Language=ENU
Destination domain must be in native mode.
.
MessageId=8497
SymbolicName=ERROR_DS_MISSING_INFRASTRUCTURE_CONTAINER
Language=ENU
The operation cannot be performed because the server does not have an infrastructure container in the domain of interest.
.
MessageId=8498
SymbolicName=ERROR_DS_CANT_MOVE_ACCOUNT_GROUP
Language=ENU
Cross-domain move of non-empty account groups is not allowed.
.
MessageId=8499
SymbolicName=ERROR_DS_CANT_MOVE_RESOURCE_GROUP
Language=ENU
Cross-domain move of non-empty resource groups is not allowed.
.
MessageId=8500
SymbolicName=ERROR_DS_INVALID_SEARCH_FLAG
Language=ENU
The search flags for the attribute are invalid. The ANR bit is valid only on attributes of Unicode or Teletex strings.
.
MessageId=8501
SymbolicName=ERROR_DS_NO_TREE_DELETE_ABOVE_NC
Language=ENU
Tree deletions starting at an object which has an NC head as a descendant are not allowed.
.
MessageId=8502
SymbolicName=ERROR_DS_COULDNT_LOCK_TREE_FOR_DELETE
Language=ENU
The directory service failed to lock a tree in preparation for a tree deletion because the tree was in use.
.
MessageId=8503
SymbolicName=ERROR_DS_COULDNT_IDENTIFY_OBJECTS_FOR_TREE_DELETE
Language=ENU
The directory service failed to identify the list of objects to delete while attempting a tree deletion.
.
MessageId=8504
SymbolicName=ERROR_DS_SAM_INIT_FAILURE
Language=ENU
Security Accounts Manager initialization failed because of the following error: %1.
.
MessageId=8505
SymbolicName=ERROR_DS_SENSITIVE_GROUP_VIOLATION
Language=ENU
Only an administrator can modify the membership list of an administrative group.
.
MessageId=8506
SymbolicName=ERROR_DS_CANT_MOD_PRIMARYGROUPID
Language=ENU
Cannot change the primary group ID of a domain controller account.
.
MessageId=8507
SymbolicName=ERROR_DS_ILLEGAL_BASE_SCHEMA_MOD
Language=ENU
An attempt is made to modify the base schema.
.
MessageId=8508
SymbolicName=ERROR_DS_NONSAFE_SCHEMA_CHANGE
Language=ENU
Adding a new mandatory attribute to an existing class, deleting a mandatory attribute from an existing class, or adding an optional attribute to the special class Top that is not a backlink attribute (directly or through inheritance, for example, by adding or deleting an auxiliary class) is not allowed.
.
MessageId=8509
SymbolicName=ERROR_DS_SCHEMA_UPDATE_DISALLOWED
Language=ENU
Schema update is not allowed on this DC because the DC is not the schema FSMO Role Owner.
.
MessageId=8510
SymbolicName=ERROR_DS_CANT_CREATE_UNDER_SCHEMA
Language=ENU
An object of this class cannot be created under the schema container. You can only create attribute-schema and class-schema objects under the schema container.
.
MessageId=8511
SymbolicName=ERROR_DS_INSTALL_NO_SRC_SCH_VERSION
Language=ENU
The replica/child install failed to get the objectVersion attribute on the schema container on the source DC. Either the attribute is missing on the schema container or the credentials supplied do not have permission to read it.
.
MessageId=8512
SymbolicName=ERROR_DS_INSTALL_NO_SCH_VERSION_IN_INIFILE
Language=ENU
The replica/child install failed to read the objectVersion attribute in the SCHEMA section of the file schema.ini in the system32 directory.
.
MessageId=8513
SymbolicName=ERROR_DS_INVALID_GROUP_TYPE
Language=ENU
The specified group type is invalid.
.
MessageId=8514
SymbolicName=ERROR_DS_NO_NEST_GLOBALGROUP_IN_MIXEDDOMAIN
Language=ENU
You cannot nest global groups in a mixed domain if the group is security-enabled.
.
MessageId=8515
SymbolicName=ERROR_DS_NO_NEST_LOCALGROUP_IN_MIXEDDOMAIN
Language=ENU
You cannot nest local groups in a mixed domain if the group is security-enabled.
.
MessageId=8516
SymbolicName=ERROR_DS_GLOBAL_CANT_HAVE_LOCAL_MEMBER
Language=ENU
A global group cannot have a local group as a member.
.
MessageId=8517
SymbolicName=ERROR_DS_GLOBAL_CANT_HAVE_UNIVERSAL_MEMBER
Language=ENU
A global group cannot have a universal group as a member.
.
MessageId=8518
SymbolicName=ERROR_DS_UNIVERSAL_CANT_HAVE_LOCAL_MEMBER
Language=ENU
A universal group cannot have a local group as a member.
.
MessageId=8519
SymbolicName=ERROR_DS_GLOBAL_CANT_HAVE_CROSSDOMAIN_MEMBER
Language=ENU
A global group cannot have a cross-domain member.
.
MessageId=8520
SymbolicName=ERROR_DS_LOCAL_CANT_HAVE_CROSSDOMAIN_LOCAL_MEMBER
Language=ENU
A local group cannot have another cross domain local group as a member.
.
MessageId=8521
SymbolicName=ERROR_DS_HAVE_PRIMARY_MEMBERS
Language=ENU
A group with primary members cannot change to a security-disabled group.
.
MessageId=8522
SymbolicName=ERROR_DS_STRING_SD_CONVERSION_FAILED
Language=ENU
The schema cache load failed to convert the string default SD on a class-schema object.
.
MessageId=8523
SymbolicName=ERROR_DS_NAMING_MASTER_GC
Language=ENU
Only DSAs configured to be Global Catalog servers should be allowed to hold the Domain Naming Master FSMO role. (Applies only to Windows 2000 servers)
.
MessageId=8524
SymbolicName=ERROR_DS_DNS_LOOKUP_FAILURE
Language=ENU
The DSA operation is unable to proceed because of a DNS lookup failure.
.
MessageId=8525
SymbolicName=ERROR_DS_COULDNT_UPDATE_SPNS
Language=ENU
While processing a change to the DNS Host Name for an object, the Service Principal Name values could not be kept in sync.
.
MessageId=8526
SymbolicName=ERROR_DS_CANT_RETRIEVE_SD
Language=ENU
The Security Descriptor attribute could not be read.
.
MessageId=8527
SymbolicName=ERROR_DS_KEY_NOT_UNIQUE
Language=ENU
The object requested was not found, but an object with that key was found.
.
MessageId=8528
SymbolicName=ERROR_DS_WRONG_LINKED_ATT_SYNTAX
Language=ENU
The syntax of the linked attribute being added is incorrect. Forward links can only have syntax 2.5.5.1, 2.5.5.7, and 2.5.5.14, and backlinks can only have syntax 2.5.5.1
.
MessageId=8529
SymbolicName=ERROR_DS_SAM_NEED_BOOTKEY_PASSWORD
Language=ENU
Security Account Manager needs to get the boot password.
.
MessageId=8530
SymbolicName=ERROR_DS_SAM_NEED_BOOTKEY_FLOPPY
Language=ENU
Security Account Manager needs to get the boot key from floppy disk.
.
MessageId=8531
SymbolicName=ERROR_DS_CANT_START
Language=ENU
Directory Service cannot start.
.
MessageId=8532
SymbolicName=ERROR_DS_INIT_FAILURE
Language=ENU
Directory Services could not start.
.
MessageId=8533
SymbolicName=ERROR_DS_NO_PKT_PRIVACY_ON_CONNECTION
Language=ENU
The connection between client and server requires packet privacy or better.
.
MessageId=8534
SymbolicName=ERROR_DS_SOURCE_DOMAIN_IN_FOREST
Language=ENU
The source domain may not be in the same forest as destination.
.
MessageId=8535
SymbolicName=ERROR_DS_DESTINATION_DOMAIN_NOT_IN_FOREST
Language=ENU
The destination domain must be in the forest.
.
MessageId=8536
SymbolicName=ERROR_DS_DESTINATION_AUDITING_NOT_ENABLED
Language=ENU
The operation requires that destination domain auditing be enabled.
.
MessageId=8537
SymbolicName=ERROR_DS_CANT_FIND_DC_FOR_SRC_DOMAIN
Language=ENU
The operation couldn't locate a DC for the source domain.
.
MessageId=8538
SymbolicName=ERROR_DS_SRC_OBJ_NOT_GROUP_OR_USER
Language=ENU
The source object must be a group or user.
.
MessageId=8539
SymbolicName=ERROR_DS_SRC_SID_EXISTS_IN_FOREST
Language=ENU
The source object's SID already exists in destination forest.
.
MessageId=8540
SymbolicName=ERROR_DS_SRC_AND_DST_OBJECT_CLASS_MISMATCH
Language=ENU
The source and destination object must be of the same type.
.
MessageId=8541
SymbolicName=ERROR_SAM_INIT_FAILURE
Language=ENU
Security Accounts Manager initialization failed because of the following error: %1.
.
MessageId=8542
SymbolicName=ERROR_DS_DRA_SCHEMA_INFO_SHIP
Language=ENU
Schema information could not be included in the replication request.
.
MessageId=8543
SymbolicName=ERROR_DS_DRA_SCHEMA_CONFLICT
Language=ENU
The replication operation could not be completed due to a schema incompatibility.
.
MessageId=8544
SymbolicName=ERROR_DS_DRA_EARLIER_SCHEMA_CONFLICT
Language=ENU
The replication operation could not be completed due to a previous schema incompatibility.
.
MessageId=8545
SymbolicName=ERROR_DS_DRA_OBJ_NC_MISMATCH
Language=ENU
The replication update could not be applied because either the source or the destination has not yet received information regarding a recent cross-domain move operation.
.
MessageId=8546
SymbolicName=ERROR_DS_NC_STILL_HAS_DSAS
Language=ENU
The requested domain could not be deleted because there exist domain controllers that still host this domain.
.
MessageId=8547
SymbolicName=ERROR_DS_GC_REQUIRED
Language=ENU
The requested operation can be performed only on a global catalog server.
.
MessageId=8548
SymbolicName=ERROR_DS_LOCAL_MEMBER_OF_LOCAL_ONLY
Language=ENU
A local group can only be a member of other local groups in the same domain.
.
MessageId=8549
SymbolicName=ERROR_DS_NO_FPO_IN_UNIVERSAL_GROUPS
Language=ENU
Foreign security principals cannot be members of universal groups.
.
MessageId=8550
SymbolicName=ERROR_DS_CANT_ADD_TO_GC
Language=ENU
The attribute is not allowed to be replicated to the GC because of security reasons.
.
MessageId=8551
SymbolicName=ERROR_DS_NO_CHECKPOINT_WITH_PDC
Language=ENU
The checkpoint with the PDC could not be taken because there too many modifications being processed currently.
.
MessageId=8552
SymbolicName=ERROR_DS_SOURCE_AUDITING_NOT_ENABLED
Language=ENU
The operation requires that source domain auditing be enabled.
.
MessageId=8553
SymbolicName=ERROR_DS_CANT_CREATE_IN_NONDOMAIN_NC
Language=ENU
Security principal objects can only be created inside domain naming contexts.
.
MessageId=8554
SymbolicName=ERROR_DS_INVALID_NAME_FOR_SPN
Language=ENU
A Service Principal Name (SPN) could not be constructed because the provided hostname is not in the necessary format.
.
MessageId=8555
SymbolicName=ERROR_DS_FILTER_USES_CONTRUCTED_ATTRS
Language=ENU
A Filter was passed that uses constructed attributes.
.
MessageId=8556
SymbolicName=ERROR_DS_UNICODEPWD_NOT_IN_QUOTES
Language=ENU
The unicodePwd attribute value must be enclosed in double quotes.
.
MessageId=8557
SymbolicName=ERROR_DS_MACHINE_ACCOUNT_QUOTA_EXCEEDED
Language=ENU
Your computer could not be joined to the domain. You have exceeded the maximum number of computer accounts you are allowed to create in this domain. Contact your system administrator to have this limit reset or increased.
.
MessageId=8558
SymbolicName=ERROR_DS_MUST_BE_RUN_ON_DST_DC
Language=ENU
For security reasons, the operation must be run on the destination DC.
.
MessageId=8559
SymbolicName=ERROR_DS_SRC_DC_MUST_BE_SP4_OR_GREATER
Language=ENU
For security reasons, the source DC must be NT4SP4 or greater.
.
MessageId=8560
SymbolicName=ERROR_DS_CANT_TREE_DELETE_CRITICAL_OBJ
Language=ENU
Critical Directory Service System objects cannot be deleted during tree delete operations. The tree delete may have been partially performed.
.
MessageId=8561
SymbolicName=ERROR_DS_INIT_FAILURE_CONSOLE
Language=ENU
Directory Services could not start because of the following error: %1.
.
MessageId=8562
SymbolicName=ERROR_DS_SAM_INIT_FAILURE_CONSOLE
Language=ENU
Security Accounts Manager initialization failed because of the following error: %1.
.
MessageId=8563
SymbolicName=ERROR_DS_FOREST_VERSION_TOO_HIGH
Language=ENU
The version of the operating system is incompatible with the current AD DS forest functional level or AD LDS Configuration Set functional level. You must upgrade to a new version of the operating system before this server can become an AD DS Domain Controller or add an AD LDS Instance in this AD DS Forest or AD LDS Configuration Set.
.
MessageId=8564
SymbolicName=ERROR_DS_DOMAIN_VERSION_TOO_HIGH
Language=ENU
The version of the operating system installed is incompatible with the current domain functional level. You must upgrade to a new version of the operating system before this server can become a domain controller in this domain.
.
MessageId=8565
SymbolicName=ERROR_DS_FOREST_VERSION_TOO_LOW
Language=ENU
The version of the operating system installed on this server no longer supports the current AD DS Forest functional level or AD LDS Configuration Set functional level. You must raise the AD DS Forest functional level or AD LDS Configuration Set functional level before this server can become an AD DS Domain Controller or an AD LDS Instance in this Forest or Configuration Set.
.
MessageId=8566
SymbolicName=ERROR_DS_DOMAIN_VERSION_TOO_LOW
Language=ENU
The version of the operating system installed on this server no longer supports the current domain functional level. You must raise the domain functional level before this server can become a domain controller in this domain.
.
MessageId=8567
SymbolicName=ERROR_DS_INCOMPATIBLE_VERSION
Language=ENU
The version of the operating system installed on this server is incompatible with the functional level of the domain or forest.
.
MessageId=8568
SymbolicName=ERROR_DS_LOW_DSA_VERSION
Language=ENU
The functional level of the domain (or forest) cannot be raised to the requested value, because there exist one or more domain controllers in the domain (or forest) that are at a lower incompatible functional level.
.
MessageId=8569
SymbolicName=ERROR_DS_NO_BEHAVIOR_VERSION_IN_MIXEDDOMAIN
Language=ENU
The forest functional level cannot be raised to the requested value since one or more domains are still in mixed domain mode. All domains in the forest must be in native mode, for you to raise the forest functional level.
.
MessageId=8570
SymbolicName=ERROR_DS_NOT_SUPPORTED_SORT_ORDER
Language=ENU
The sort order requested is not supported.
.
MessageId=8571
SymbolicName=ERROR_DS_NAME_NOT_UNIQUE
Language=ENU
The requested name already exists as a unique identifier.
.
MessageId=8572
SymbolicName=ERROR_DS_MACHINE_ACCOUNT_CREATED_PRENT4
Language=ENU
The machine account was created pre-NT4. The account needs to be recreated.
.
MessageId=8573
SymbolicName=ERROR_DS_OUT_OF_VERSION_STORE
Language=ENU
The database is out of version store.
.
MessageId=8574
SymbolicName=ERROR_DS_INCOMPATIBLE_CONTROLS_USED
Language=ENU
Unable to continue operation because multiple conflicting controls were used.
.
MessageId=8575
SymbolicName=ERROR_DS_NO_REF_DOMAIN
Language=ENU
Unable to find a valid security descriptor reference domain for this partition.
.
MessageId=8576
SymbolicName=ERROR_DS_RESERVED_LINK_ID
Language=ENU
Schema update failed: The link identifier is reserved.
.
MessageId=8577
SymbolicName=ERROR_DS_LINK_ID_NOT_AVAILABLE
Language=ENU
Schema update failed: There are no link identifiers available.
.
MessageId=8578
SymbolicName=ERROR_DS_AG_CANT_HAVE_UNIVERSAL_MEMBER
Language=ENU
An account group cannot have a universal group as a member.
.
MessageId=8579
SymbolicName=ERROR_DS_MODIFYDN_DISALLOWED_BY_INSTANCE_TYPE
Language=ENU
Rename or move operations on naming context heads or read-only objects are not allowed.
.
MessageId=8580
SymbolicName=ERROR_DS_NO_OBJECT_MOVE_IN_SCHEMA_NC
Language=ENU
Move operations on objects in the schema naming context are not allowed.
.
MessageId=8581
SymbolicName=ERROR_DS_MODIFYDN_DISALLOWED_BY_FLAG
Language=ENU
A system flag has been set on the object and does not allow the object to be moved or renamed.
.
MessageId=8582
SymbolicName=ERROR_DS_MODIFYDN_WRONG_GRANDPARENT
Language=ENU
This object is not allowed to change its grandparent container. Moves are not forbidden on this object, but are restricted to sibling containers.
.
MessageId=8583
SymbolicName=ERROR_DS_NAME_ERROR_TRUST_REFERRAL
Language=ENU
Unable to resolve completely, a referral to another forest is generated.
.
MessageId=8584
SymbolicName=ERROR_NOT_SUPPORTED_ON_STANDARD_SERVER
Language=ENU
The requested action is not supported on standard server.
.
MessageId=8585
SymbolicName=ERROR_DS_CANT_ACCESS_REMOTE_PART_OF_AD
Language=ENU
Could not access a partition of the directory service located on a remote server. Make sure at least one server is running for the partition in question.
.
MessageId=8586
SymbolicName=ERROR_DS_CR_IMPOSSIBLE_TO_VALIDATE_V2
Language=ENU
The directory cannot validate the proposed naming context (or partition) name because it does not hold a replica nor can it contact a replica of the naming context above the proposed naming context. Please ensure that the parent naming context is properly registered in DNS, and at least one replica of this naming context is reachable by the Domain Naming master.
.
MessageId=8587
SymbolicName=ERROR_DS_THREAD_LIMIT_EXCEEDED
Language=ENU
The thread limit for this request was exceeded.
.
MessageId=8588
SymbolicName=ERROR_DS_NOT_CLOSEST
Language=ENU
The Global catalog server is not in the closest site.
.
MessageId=8589
SymbolicName=ERROR_DS_CANT_DERIVE_SPN_WITHOUT_SERVER_REF
Language=ENU
The DS cannot derive a service principal name (SPN) with which to mutually authenticate the target server because the corresponding server object in the local DS database has no serverReference attribute.
.
MessageId=8590
SymbolicName=ERROR_DS_SINGLE_USER_MODE_FAILED
Language=ENU
The Directory Service failed to enter single user mode.
.
MessageId=8591
SymbolicName=ERROR_DS_NTDSCRIPT_SYNTAX_ERROR
Language=ENU
The Directory Service cannot parse the script because of a syntax error.
.
MessageId=8592
SymbolicName=ERROR_DS_NTDSCRIPT_PROCESS_ERROR
Language=ENU
The Directory Service cannot process the script because of an error.
.
MessageId=8593
SymbolicName=ERROR_DS_DIFFERENT_REPL_EPOCHS
Language=ENU
The directory service cannot perform the requested operation because the servers involved are of different replication epochs (which is usually related to a domain rename that is in progress).
.
MessageId=8594
SymbolicName=ERROR_DS_DRS_EXTENSIONS_CHANGED
Language=ENU
The directory service binding must be renegotiated due to a change in the server extensions information.
.
MessageId=8595
SymbolicName=ERROR_DS_REPLICA_SET_CHANGE_NOT_ALLOWED_ON_DISABLED_CR
Language=ENU
Operation not allowed on a disabled cross ref.
.
MessageId=8596
SymbolicName=ERROR_DS_NO_MSDS_INTID
Language=ENU
Schema update failed: No values for msDS-IntId are available.
.
MessageId=8597
SymbolicName=ERROR_DS_DUP_MSDS_INTID
Language=ENU
Schema update failed: Duplicate msDS-INtId. Retry the operation.
.
MessageId=8598
SymbolicName=ERROR_DS_EXISTS_IN_RDNATTID
Language=ENU
Schema deletion failed: attribute is used in rDNAttID.
.
MessageId=8599
SymbolicName=ERROR_DS_AUTHORIZATION_FAILED
Language=ENU
The directory service failed to authorize the request.
.
MessageId=8600
SymbolicName=ERROR_DS_INVALID_SCRIPT
Language=ENU
The Directory Service cannot process the script because it is invalid.
.
MessageId=8601
SymbolicName=ERROR_DS_REMOTE_CROSSREF_OP_FAILED
Language=ENU
The remote create cross reference operation failed on the Domain Naming Master FSMO. The operation's error is in the extended data.
.
MessageId=8602
SymbolicName=ERROR_DS_CROSS_REF_BUSY
Language=ENU
A cross reference is in use locally with the same name.
.
MessageId=8603
SymbolicName=ERROR_DS_CANT_DERIVE_SPN_FOR_DELETED_DOMAIN
Language=ENU
The DS cannot derive a service principal name (SPN) with which to mutually authenticate the target server because the server's domain has been deleted from the forest.
.
MessageId=8604
SymbolicName=ERROR_DS_CANT_DEMOTE_WITH_WRITEABLE_NC
Language=ENU
Writeable NCs prevent this DC from demoting.
.
MessageId=8605
SymbolicName=ERROR_DS_DUPLICATE_ID_FOUND
Language=ENU
The requested object has a non-unique identifier and cannot be retrieved.
.
MessageId=8606
SymbolicName=ERROR_DS_INSUFFICIENT_ATTR_TO_CREATE_OBJECT
Language=ENU
Insufficient attributes were given to create an object. This object may not exist because it may have been deleted and already garbage collected.
.
MessageId=8607
SymbolicName=ERROR_DS_GROUP_CONVERSION_ERROR
Language=ENU
The group cannot be converted due to attribute restrictions on the requested group type.
.
MessageId=8608
SymbolicName=ERROR_DS_CANT_MOVE_APP_BASIC_GROUP
Language=ENU
Cross-domain move of non-empty basic application groups is not allowed.
.
MessageId=8609
SymbolicName=ERROR_DS_CANT_MOVE_APP_QUERY_GROUP
Language=ENU
Cross-domain move of non-empty query based application groups is not allowed.
.
MessageId=8610
SymbolicName=ERROR_DS_ROLE_NOT_VERIFIED
Language=ENU
The FSMO role ownership could not be verified because its directory partition has not replicated successfully with at least one replication partner.
.
MessageId=8611
SymbolicName=ERROR_DS_WKO_CONTAINER_CANNOT_BE_SPECIAL
Language=ENU
The target container for a redirection of a well known object container cannot already be a special container.
.
MessageId=8612
SymbolicName=ERROR_DS_DOMAIN_RENAME_IN_PROGRESS
Language=ENU
The Directory Service cannot perform the requested operation because a domain rename operation is in progress.
.
MessageId=8613
SymbolicName=ERROR_DS_EXISTING_AD_CHILD_NC
Language=ENU
The directory service detected a child partition below the requested partition name. The partition hierarchy must be created in a top down method.
.
MessageId=8614
SymbolicName=ERROR_DS_REPL_LIFETIME_EXCEEDED
Language=ENU
The directory service cannot replicate with this server because the time since the last replication with this server has exceeded the tombstone lifetime.
.
MessageId=8615
SymbolicName=ERROR_DS_DISALLOWED_IN_SYSTEM_CONTAINER
Language=ENU
The requested operation is not allowed on an object under the system container.
.
MessageId=8616
SymbolicName=ERROR_DS_LDAP_SEND_QUEUE_FULL
Language=ENU
The LDAP servers network send queue has filled up because the client is not processing the results of its requests fast enough. No more requests will be processed until the client catches up. If the client does not catch up then it will be disconnected.
.
MessageId=8617
SymbolicName=ERROR_DS_DRA_OUT_SCHEDULE_WINDOW
Language=ENU
The scheduled replication did not take place because the system was too busy to execute the request within the schedule window. The replication queue is overloaded. Consider reducing the number of partners or decreasing the scheduled replication frequency.
.
MessageId=8618
SymbolicName=ERROR_DS_POLICY_NOT_KNOWN
Language=ENU
At this time, it cannot be determined if the branch replication policy is available on the hub domain controller. Please retry at a later time to account for replication latencies.
.
MessageId=8619
SymbolicName=ERROR_NO_SITE_SETTINGS_OBJECT
Language=ENU
The site settings object for the specified site does not exist.
.
MessageId=8620
SymbolicName=ERROR_NO_SECRETS
Language=ENU
The local account store does not contain secret material for the specified account.
.
MessageId=8621
SymbolicName=ERROR_NO_WRITABLE_DC_FOUND
Language=ENU
Could not find a writable domain controller in the domain.
.
MessageId=8622
SymbolicName=ERROR_DS_NO_SERVER_OBJECT
Language=ENU
The server object for the domain controller does not exist.
.
MessageId=8623
SymbolicName=ERROR_DS_NO_NTDSA_OBJECT
Language=ENU
The NTDS Settings object for the domain controller does not exist.
.
MessageId=8624
SymbolicName=ERROR_DS_NON_ASQ_SEARCH
Language=ENU
The requested search operation is not supported for ASQ searches.
.
MessageId=8625
SymbolicName=ERROR_DS_AUDIT_FAILURE
Language=ENU
A required audit event could not be generated for the operation.
.
MessageId=8626
SymbolicName=ERROR_DS_INVALID_SEARCH_FLAG_SUBTREE
Language=ENU
The search flags for the attribute are invalid. The subtree index bit is valid only on single valued attributes.
.
MessageId=8627
SymbolicName=ERROR_DS_INVALID_SEARCH_FLAG_TUPLE
Language=ENU
The search flags for the attribute are invalid. The tuple index bit is valid only on attributes of Unicode strings.
.
MessageId=8628
SymbolicName=ERROR_DS_HIERARCHY_TABLE_TOO_DEEP
Language=ENU
The address books are nested too deeply. Failed to build the hierarchy table.
.
MessageId=8629
SymbolicName=ERROR_DS_DRA_CORRUPT_UTD_VECTOR
Language=ENU
The specified up-to-date-ness vector is corrupt.
.
MessageId=8630
SymbolicName=ERROR_DS_DRA_SECRETS_DENIED
Language=ENU
The request to replicate secrets is denied.
.
MessageId=8631
SymbolicName=ERROR_DS_RESERVED_MAPI_ID
Language=ENU
Schema update failed: The MAPI identifier is reserved.
.
MessageId=8632
SymbolicName=ERROR_DS_MAPI_ID_NOT_AVAILABLE
Language=ENU
Schema update failed: There are no MAPI identifiers available.
.
MessageId=8633
SymbolicName=ERROR_DS_DRA_MISSING_KRBTGT_SECRET
Language=ENU
The replication operation failed because the required attributes of the local krbtgt object are missing.
.
MessageId=8634
SymbolicName=ERROR_DS_DOMAIN_NAME_EXISTS_IN_FOREST
Language=ENU
The domain name of the trusted domain already exists in the forest.
.
MessageId=8635
SymbolicName=ERROR_DS_FLAT_NAME_EXISTS_IN_FOREST
Language=ENU
The flat name of the trusted domain already exists in the forest.
.
MessageId=8636
SymbolicName=ERROR_INVALID_USER_PRINCIPAL_NAME
Language=ENU
The User Principal Name (UPN) is invalid.
.
MessageId=8637
SymbolicName=ERROR_DS_OID_MAPPED_GROUP_CANT_HAVE_MEMBERS
Language=ENU
OID mapped groups cannot have members.
.
MessageId=8638
SymbolicName=ERROR_DS_OID_NOT_FOUND
Language=ENU
The specified OID cannot be found.
.
MessageId=8639
SymbolicName=ERROR_DS_DRA_RECYCLED_TARGET
Language=ENU
The replication operation failed because the target object referred by a link value is recycled.
.
MessageId=8640
SymbolicName=ERROR_DS_DISALLOWED_NC_REDIRECT
Language=ENU
The redirect operation failed because the target object is in a NC different from the domain NC of the current domain controller.
.
MessageId=8641
SymbolicName=ERROR_DS_HIGH_ADLDS_FFL
Language=ENU
The functional level of the AD LDS configuration set cannot be lowered to the requested value.
.
MessageId=8642
SymbolicName=ERROR_DS_HIGH_DSA_VERSION
Language=ENU
The functional level of the domain (or forest) cannot be set to the requested value.
.
MessageId=8643
SymbolicName=ERROR_DS_LOW_ADLDS_FFL
Language=ENU
The functional level of the AD LDS configuration set cannot be raised to the requested value, because there exist one or more ADLDS instances that are at a lower incompatible functional level.
.
MessageId=8644
SymbolicName=ERROR_DOMAIN_SID_SAME_AS_LOCAL_WORKSTATION
Language=ENU
The domain join cannot be completed because the SID of the domain you attempted to join was identical to the SID of this machine. This is a symptom of an improperly cloned operating system install.  You should run sysprep on this machine in order to generate a new machine SID. Please see http://go.microsoft.com/fwlink/?LinkId=168895 for more information.
.
MessageId=8645
SymbolicName=ERROR_DS_UNDELETE_SAM_VALIDATION_FAILED
Language=ENU
The undelete operation failed because the Sam Account Name or Additional Sam Account Name of the object being undeleted conflicts with an existing live object.
.
MessageId=8646
SymbolicName=ERROR_INCORRECT_ACCOUNT_TYPE
Language=ENU
The system is not authoritative for the specified account and therefore cannot complete the operation. Please retry the operation using the provider associated with this account. If this is an online provider please use the provider's online site.
.
MessageId=8647
SymbolicName=ERROR_DS_SPN_VALUE_NOT_UNIQUE_IN_FOREST
Language=ENU
The operation failed because SPN value provided for addition/modification is not unique forest-wide.
.
MessageId=8648
SymbolicName=ERROR_DS_UPN_VALUE_NOT_UNIQUE_IN_FOREST
Language=ENU
The operation failed because UPN value provided for addition/modification is not unique forest-wide.
.
MessageId=8649
SymbolicName=ERROR_DS_MISSING_FOREST_TRUST
Language=ENU
The operation failed because the addition/modification referenced an inbound forest-wide trust that is not present.
.
MessageId=8650
SymbolicName=ERROR_DS_VALUE_KEY_NOT_UNIQUE
Language=ENU
The link value specified was not found, but a link value with that key was found.
.
MessageId=8651
SymbolicName=ERROR_WEAK_WHFBKEY_BLOCKED
Language=ENU
The Security Account Manager blocked the use of a weak Windows Hello for Business key.
.
MessageId=8652
SymbolicName=ERROR_DS_PER_ATTRIBUTE_AUTHZ_FAILED_DURING_ADD
Language=ENU
The add object operation failed because the caller was not authorized to add one or more attributes included in the request.
.
MessageId=8653
SymbolicName=ERROR_LOCAL_POLICY_MODIFICATION_NOT_SUPPORTED
Language=ENU
The local account policy modification request was rejected because the policy is controlled by a regional authority.
.
MessageId=8654
SymbolicName=ERROR_POLICY_CONTROLLED_ACCOUNT
Language=ENU
The account is controlled by external policy and cannot be modified.
.
MessageId=8655
SymbolicName=ERROR_LAPS_LEGACY_SCHEMA_MISSING
Language=ENU
The Local Administrator Password Solution password update operation failed because the legacy LAPS schema needs to be added to Active Directory.
.
MessageId=8656
SymbolicName=ERROR_LAPS_SCHEMA_MISSING
Language=ENU
The Local Administrator Password Solution password update operation failed because the Windows LAPS schema needs to be added to Active Directory.
.
MessageId=8657
SymbolicName=ERROR_LAPS_ENCRYPTION_REQUIRES_2016_DFL
Language=ENU
The Local Administrator Password Solution encrypted password update operation failed because Active Directory is not yet running at the minimum required domain functional level (2016).
.
MessageId=8658
SymbolicName=WINERR_8658
Language=ENU
The process was terminated by Windows Local Administrator Password Solution per the configured post-authentication-action policy.
.
MessageId=8659
SymbolicName=WINERR_8659
Language=ENU
The Active Directory JET database page size limit for this request was exceeded.
.
MessageId=8660
SymbolicName=WINERR_8660
Language=ENU
The Active Directory JET database page size does not match on all Domain Controllers in this forest.
.
MessageId=9001
SymbolicName=DNS_ERROR_RCODE_FORMAT_ERROR
Language=ENU
DNS server unable to interpret format.
.
MessageId=9002
SymbolicName=DNS_ERROR_RCODE_SERVER_FAILURE
Language=ENU
DNS server failure.
.
MessageId=9003
SymbolicName=DNS_ERROR_RCODE_NAME_ERROR
Language=ENU
DNS name does not exist.
.
MessageId=9004
SymbolicName=DNS_ERROR_RCODE_NOT_IMPLEMENTED
Language=ENU
DNS request not supported by name server.
.
MessageId=9005
SymbolicName=DNS_ERROR_RCODE_REFUSED
Language=ENU
DNS operation refused.
.
MessageId=9006
SymbolicName=DNS_ERROR_RCODE_YXDOMAIN
Language=ENU
DNS name that ought not exist, does exist.
.
MessageId=9007
SymbolicName=DNS_ERROR_RCODE_YXRRSET
Language=ENU
DNS RR set that ought not exist, does exist.
.
MessageId=9008
SymbolicName=DNS_ERROR_RCODE_NXRRSET
Language=ENU
DNS RR set that ought to exist, does not exist.
.
MessageId=9009
SymbolicName=DNS_ERROR_RCODE_NOTAUTH
Language=ENU
DNS server not authoritative for zone.
.
MessageId=9010
SymbolicName=DNS_ERROR_RCODE_NOTZONE
Language=ENU
DNS name in update or prereq is not in zone.
.
MessageId=9016
SymbolicName=DNS_ERROR_RCODE_BADSIG
Language=ENU
DNS signature failed to verify.
.
MessageId=9017
SymbolicName=DNS_ERROR_RCODE_BADKEY
Language=ENU
DNS bad key.
.
MessageId=9018
SymbolicName=DNS_ERROR_RCODE_BADTIME
Language=ENU
DNS signature validity expired.
.
MessageId=9101
SymbolicName=DNS_ERROR_KEYMASTER_REQUIRED
Language=ENU
Only the DNS server acting as the key master for the zone may perform this operation.
.
MessageId=9102
SymbolicName=DNS_ERROR_NOT_ALLOWED_ON_SIGNED_ZONE
Language=ENU
This operation is not allowed on a zone that is signed or has signing keys.
.
MessageId=9103
SymbolicName=DNS_ERROR_NSEC3_INCOMPATIBLE_WITH_RSA_SHA1
Language=ENU
NSEC3 is not compatible with the RSA-SHA-1 algorithm. Choose a different algorithm or use NSEC.
.
MessageId=9104
SymbolicName=DNS_ERROR_NOT_ENOUGH_SIGNING_KEY_DESCRIPTORS
Language=ENU
The zone does not have enough signing keys. There must be at least one key signing key (KSK) and at least one zone signing key (ZSK).
.
MessageId=9105
SymbolicName=DNS_ERROR_UNSUPPORTED_ALGORITHM
Language=ENU
The specified algorithm is not supported.
.
MessageId=9106
SymbolicName=DNS_ERROR_INVALID_KEY_SIZE
Language=ENU
The specified key size is not supported.
.
MessageId=9107
SymbolicName=DNS_ERROR_SIGNING_KEY_NOT_ACCESSIBLE
Language=ENU
One or more of the signing keys for a zone are not accessible to the DNS server. Zone signing will not be operational until this error is resolved.
.
MessageId=9108
SymbolicName=DNS_ERROR_KSP_DOES_NOT_SUPPORT_PROTECTION
Language=ENU
The specified key storage provider does not support DPAPI++ data protection. Zone signing will not be operational until this error is resolved.
.
MessageId=9109
SymbolicName=DNS_ERROR_UNEXPECTED_DATA_PROTECTION_ERROR
Language=ENU
An unexpected DPAPI++ error was encountered. Zone signing will not be operational until this error is resolved.
.
MessageId=9110
SymbolicName=DNS_ERROR_UNEXPECTED_CNG_ERROR
Language=ENU
An unexpected crypto error was encountered. Zone signing may not be operational until this error is resolved.
.
MessageId=9111
SymbolicName=DNS_ERROR_UNKNOWN_SIGNING_PARAMETER_VERSION
Language=ENU
The DNS server encountered a signing key with an unknown version. Zone signing will not be operational until this error is resolved.
.
MessageId=9112
SymbolicName=DNS_ERROR_KSP_NOT_ACCESSIBLE
Language=ENU
The specified key service provider cannot be opened by the DNS server.
.
MessageId=9113
SymbolicName=DNS_ERROR_TOO_MANY_SKDS
Language=ENU
The DNS server cannot accept any more signing keys with the specified algorithm and KSK flag value for this zone.
.
MessageId=9114
SymbolicName=DNS_ERROR_INVALID_ROLLOVER_PERIOD
Language=ENU
The specified rollover period is invalid.
.
MessageId=9115
SymbolicName=DNS_ERROR_INVALID_INITIAL_ROLLOVER_OFFSET
Language=ENU
The specified initial rollover offset is invalid.
.
MessageId=9116
SymbolicName=DNS_ERROR_ROLLOVER_IN_PROGRESS
Language=ENU
The specified signing key is already in process of rolling over keys.
.
MessageId=9117
SymbolicName=DNS_ERROR_STANDBY_KEY_NOT_PRESENT
Language=ENU
The specified signing key does not have a standby key to revoke.
.
MessageId=9118
SymbolicName=DNS_ERROR_NOT_ALLOWED_ON_ZSK
Language=ENU
This operation is not allowed on a zone signing key (ZSK).
.
MessageId=9119
SymbolicName=DNS_ERROR_NOT_ALLOWED_ON_ACTIVE_SKD
Language=ENU
This operation is not allowed on an active signing key.
.
MessageId=9120
SymbolicName=DNS_ERROR_ROLLOVER_ALREADY_QUEUED
Language=ENU
The specified signing key is already queued for rollover.
.
MessageId=9121
SymbolicName=DNS_ERROR_NOT_ALLOWED_ON_UNSIGNED_ZONE
Language=ENU
This operation is not allowed on an unsigned zone.
.
MessageId=9122
SymbolicName=DNS_ERROR_BAD_KEYMASTER
Language=ENU
This operation could not be completed because the DNS server listed as the current key master for this zone is down or misconfigured. Resolve the problem on the current key master for this zone or use another DNS server to seize the key master role.
.
MessageId=9123
SymbolicName=DNS_ERROR_INVALID_SIGNATURE_VALIDITY_PERIOD
Language=ENU
The specified signature validity period is invalid.
.
MessageId=9124
SymbolicName=DNS_ERROR_INVALID_NSEC3_ITERATION_COUNT
Language=ENU
The specified NSEC3 iteration count is higher than allowed by the minimum key length used in the zone.
.
MessageId=9125
SymbolicName=DNS_ERROR_DNSSEC_IS_DISABLED
Language=ENU
This operation could not be completed because the DNS server has been configured with DNSSEC features disabled. Enable DNSSEC on the DNS server.
.
MessageId=9126
SymbolicName=DNS_ERROR_INVALID_XML
Language=ENU
This operation could not be completed because the XML stream received is empty or syntactically invalid.
.
MessageId=9127
SymbolicName=DNS_ERROR_NO_VALID_TRUST_ANCHORS
Language=ENU
This operation completed, but no trust anchors were added because all of the trust anchors received were either invalid, unsupported, expired, or would not become valid in less than 30 days.
.
MessageId=9128
SymbolicName=DNS_ERROR_ROLLOVER_NOT_POKEABLE
Language=ENU
The specified signing key is not waiting for parental DS update.
.
MessageId=9129
SymbolicName=DNS_ERROR_NSEC3_NAME_COLLISION
Language=ENU
Hash collision detected during NSEC3 signing. Specify a different user-provided salt, or use a randomly generated salt, and attempt to sign the zone again.
.
MessageId=9130
SymbolicName=DNS_ERROR_NSEC_INCOMPATIBLE_WITH_NSEC3_RSA_SHA1
Language=ENU
NSEC is not compatible with the NSEC3-RSA-SHA-1 algorithm. Choose a different algorithm or use NSEC3.
.
MessageId=9501
SymbolicName=WINERR_9501
Language=ENU
No records found for given DNS query.
.
MessageId=9502
SymbolicName=DNS_ERROR_BAD_PACKET
Language=ENU
Bad DNS packet.
.
MessageId=9503
SymbolicName=DNS_ERROR_NO_PACKET
Language=ENU
No DNS packet.
.
MessageId=9504
SymbolicName=DNS_ERROR_RCODE
Language=ENU
DNS error, check rcode.
.
MessageId=9505
SymbolicName=DNS_ERROR_UNSECURE_PACKET
Language=ENU
Unsecured DNS packet.
.
MessageId=9506
SymbolicName=WINERR_9506
Language=ENU
DNS query request is pending.
.
MessageId=9551
SymbolicName=DNS_ERROR_INVALID_TYPE
Language=ENU
Invalid DNS type.
.
MessageId=9552
SymbolicName=DNS_ERROR_INVALID_IP_ADDRESS
Language=ENU
Invalid IP address.
.
MessageId=9553
SymbolicName=DNS_ERROR_INVALID_PROPERTY
Language=ENU
Invalid property.
.
MessageId=9554
SymbolicName=DNS_ERROR_TRY_AGAIN_LATER
Language=ENU
Try DNS operation again later.
.
MessageId=9555
SymbolicName=DNS_ERROR_NOT_UNIQUE
Language=ENU
Record for given name and type is not unique.
.
MessageId=9556
SymbolicName=DNS_ERROR_NON_RFC_NAME
Language=ENU
DNS name does not comply with RFC specifications.
.
MessageId=9557
SymbolicName=WINERR_9557
Language=ENU
DNS name is a fully-qualified DNS name.
.
MessageId=9558
SymbolicName=WINERR_9558
Language=ENU
DNS name is dotted (multi-label).
.
MessageId=9559
SymbolicName=WINERR_9559
Language=ENU
DNS name is a single-part name.
.
MessageId=9560
SymbolicName=DNS_ERROR_INVALID_NAME_CHAR
Language=ENU
DNS name contains an invalid character.
.
MessageId=9561
SymbolicName=DNS_ERROR_NUMERIC_NAME
Language=ENU
DNS name is entirely numeric.
.
MessageId=9562
SymbolicName=DNS_ERROR_NOT_ALLOWED_ON_ROOT_SERVER
Language=ENU
The operation requested is not permitted on a DNS root server.
.
MessageId=9563
SymbolicName=DNS_ERROR_NOT_ALLOWED_UNDER_DELEGATION
Language=ENU
The record could not be created because this part of the DNS namespace has been delegated to another server.
.
MessageId=9564
SymbolicName=DNS_ERROR_CANNOT_FIND_ROOT_HINTS
Language=ENU
The DNS server could not find a set of root hints.
.
MessageId=9565
SymbolicName=DNS_ERROR_INCONSISTENT_ROOT_HINTS
Language=ENU
The DNS server found root hints but they were not consistent across all adapters.
.
MessageId=9566
SymbolicName=DNS_ERROR_DWORD_VALUE_TOO_SMALL
Language=ENU
The specified value is too small for this parameter.
.
MessageId=9567
SymbolicName=DNS_ERROR_DWORD_VALUE_TOO_LARGE
Language=ENU
The specified value is too large for this parameter.
.
MessageId=9568
SymbolicName=DNS_ERROR_BACKGROUND_LOADING
Language=ENU
This operation is not allowed while the DNS server is loading zones in the background. Please try again later.
.
MessageId=9569
SymbolicName=DNS_ERROR_NOT_ALLOWED_ON_RODC
Language=ENU
The operation requested is not permitted on against a DNS server running on a read-only DC.
.
MessageId=9570
SymbolicName=DNS_ERROR_NOT_ALLOWED_UNDER_DNAME
Language=ENU
No data is allowed to exist underneath a DNAME record.
.
MessageId=9571
SymbolicName=DNS_ERROR_DELEGATION_REQUIRED
Language=ENU
This operation requires credentials delegation.
.
MessageId=9572
SymbolicName=DNS_ERROR_INVALID_POLICY_TABLE
Language=ENU
Name resolution policy table has been corrupted. DNS resolution will fail until it is fixed. Contact your network administrator.
.
MessageId=9573
SymbolicName=DNS_ERROR_ADDRESS_REQUIRED
Language=ENU
Not allowed to remove all addresses.
.
MessageId=9601
SymbolicName=DNS_ERROR_ZONE_DOES_NOT_EXIST
Language=ENU
DNS zone does not exist.
.
MessageId=9602
SymbolicName=DNS_ERROR_NO_ZONE_INFO
Language=ENU
DNS zone information not available.
.
MessageId=9603
SymbolicName=DNS_ERROR_INVALID_ZONE_OPERATION
Language=ENU
Invalid operation for DNS zone.
.
MessageId=9604
SymbolicName=DNS_ERROR_ZONE_CONFIGURATION_ERROR
Language=ENU
Invalid DNS zone configuration.
.
MessageId=9605
SymbolicName=DNS_ERROR_ZONE_HAS_NO_SOA_RECORD
Language=ENU
DNS zone has no start of authority (SOA) record.
.
MessageId=9606
SymbolicName=DNS_ERROR_ZONE_HAS_NO_NS_RECORDS
Language=ENU
DNS zone has no Name Server (NS) record.
.
MessageId=9607
SymbolicName=DNS_ERROR_ZONE_LOCKED
Language=ENU
DNS zone is locked.
.
MessageId=9608
SymbolicName=DNS_ERROR_ZONE_CREATION_FAILED
Language=ENU
DNS zone creation failed.
.
MessageId=9609
SymbolicName=DNS_ERROR_ZONE_ALREADY_EXISTS
Language=ENU
DNS zone already exists.
.
MessageId=9610
SymbolicName=DNS_ERROR_AUTOZONE_ALREADY_EXISTS
Language=ENU
DNS automatic zone already exists.
.
MessageId=9611
SymbolicName=DNS_ERROR_INVALID_ZONE_TYPE
Language=ENU
Invalid DNS zone type.
.
MessageId=9612
SymbolicName=DNS_ERROR_SECONDARY_REQUIRES_MASTER_IP
Language=ENU
Secondary DNS zone requires master IP address.
.
MessageId=9613
SymbolicName=DNS_ERROR_ZONE_NOT_SECONDARY
Language=ENU
DNS zone not secondary.
.
MessageId=9614
SymbolicName=DNS_ERROR_NEED_SECONDARY_ADDRESSES
Language=ENU
Need secondary IP address.
.
MessageId=9615
SymbolicName=DNS_ERROR_WINS_INIT_FAILED
Language=ENU
WINS initialization failed.
.
MessageId=9616
SymbolicName=DNS_ERROR_NEED_WINS_SERVERS
Language=ENU
Need WINS servers.
.
MessageId=9617
SymbolicName=DNS_ERROR_NBSTAT_INIT_FAILED
Language=ENU
NBTSTAT initialization call failed.
.
MessageId=9618
SymbolicName=DNS_ERROR_SOA_DELETE_INVALID
Language=ENU
Invalid delete of start of authority (SOA)
.
MessageId=9619
SymbolicName=DNS_ERROR_FORWARDER_ALREADY_EXISTS
Language=ENU
A conditional forwarding zone already exists for that name.
.
MessageId=9620
SymbolicName=DNS_ERROR_ZONE_REQUIRES_MASTER_IP
Language=ENU
This zone must be configured with one or more master DNS server IP addresses.
.
MessageId=9621
SymbolicName=DNS_ERROR_ZONE_IS_SHUTDOWN
Language=ENU
The operation cannot be performed because this zone is shut down.
.
MessageId=9622
SymbolicName=DNS_ERROR_ZONE_LOCKED_FOR_SIGNING
Language=ENU
This operation cannot be performed because the zone is currently being signed. Please try again later.
.
MessageId=9651
SymbolicName=DNS_ERROR_PRIMARY_REQUIRES_DATAFILE
Language=ENU
Primary DNS zone requires datafile.
.
MessageId=9652
SymbolicName=DNS_ERROR_INVALID_DATAFILE_NAME
Language=ENU
Invalid datafile name for DNS zone.
.
MessageId=9653
SymbolicName=DNS_ERROR_DATAFILE_OPEN_FAILURE
Language=ENU
Failed to open datafile for DNS zone.
.
MessageId=9654
SymbolicName=DNS_ERROR_FILE_WRITEBACK_FAILED
Language=ENU
Failed to write datafile for DNS zone.
.
MessageId=9655
SymbolicName=DNS_ERROR_DATAFILE_PARSING
Language=ENU
Failure while reading datafile for DNS zone.
.
MessageId=9701
SymbolicName=DNS_ERROR_RECORD_DOES_NOT_EXIST
Language=ENU
DNS record does not exist.
.
MessageId=9702
SymbolicName=DNS_ERROR_RECORD_FORMAT
Language=ENU
DNS record format error.
.
MessageId=9703
SymbolicName=DNS_ERROR_NODE_CREATION_FAILED
Language=ENU
Node creation failure in DNS.
.
MessageId=9704
SymbolicName=DNS_ERROR_UNKNOWN_RECORD_TYPE
Language=ENU
Unknown DNS record type.
.
MessageId=9705
SymbolicName=DNS_ERROR_RECORD_TIMED_OUT
Language=ENU
DNS record timed out.
.
MessageId=9706
SymbolicName=DNS_ERROR_NAME_NOT_IN_ZONE
Language=ENU
Name not in DNS zone.
.
MessageId=9707
SymbolicName=DNS_ERROR_CNAME_LOOP
Language=ENU
CNAME loop detected.
.
MessageId=9708
SymbolicName=DNS_ERROR_NODE_IS_CNAME
Language=ENU
Node is a CNAME DNS record.
.
MessageId=9709
SymbolicName=DNS_ERROR_CNAME_COLLISION
Language=ENU
A CNAME record already exists for given name.
.
MessageId=9710
SymbolicName=DNS_ERROR_RECORD_ONLY_AT_ZONE_ROOT
Language=ENU
Record only at DNS zone root.
.
MessageId=9711
SymbolicName=DNS_ERROR_RECORD_ALREADY_EXISTS
Language=ENU
DNS record already exists.
.
MessageId=9712
SymbolicName=DNS_ERROR_SECONDARY_DATA
Language=ENU
Secondary DNS zone data error.
.
MessageId=9713
SymbolicName=DNS_ERROR_NO_CREATE_CACHE_DATA
Language=ENU
Could not create DNS cache data.
.
MessageId=9714
SymbolicName=DNS_ERROR_NAME_DOES_NOT_EXIST
Language=ENU
DNS name does not exist.
.
MessageId=9715
SymbolicName=WINERR_9715
Language=ENU
Could not create pointer (PTR) record.
.
MessageId=9716
SymbolicName=WINERR_9716
Language=ENU
DNS domain was undeleted.
.
MessageId=9717
SymbolicName=DNS_ERROR_DS_UNAVAILABLE
Language=ENU
The directory service is unavailable.
.
MessageId=9718
SymbolicName=DNS_ERROR_DS_ZONE_ALREADY_EXISTS
Language=ENU
DNS zone already exists in the directory service.
.
MessageId=9719
SymbolicName=DNS_ERROR_NO_BOOTFILE_IF_DS_ZONE
Language=ENU
DNS server not creating or reading the boot file for the directory service integrated DNS zone.
.
MessageId=9720
SymbolicName=DNS_ERROR_NODE_IS_DNAME
Language=ENU
Node is a DNAME DNS record.
.
MessageId=9721
SymbolicName=DNS_ERROR_DNAME_COLLISION
Language=ENU
A DNAME record already exists for given name.
.
MessageId=9722
SymbolicName=DNS_ERROR_ALIAS_LOOP
Language=ENU
An alias loop has been detected with either CNAME or DNAME records.
.
MessageId=9751
SymbolicName=WINERR_9751
Language=ENU
DNS AXFR (zone transfer) complete.
.
MessageId=9752
SymbolicName=DNS_ERROR_AXFR
Language=ENU
DNS zone transfer failed.
.
MessageId=9753
SymbolicName=WINERR_9753
Language=ENU
Added local WINS server.
.
MessageId=9801
SymbolicName=WINERR_9801
Language=ENU
Secure update call needs to continue update request.
.
MessageId=9851
SymbolicName=DNS_ERROR_NO_TCPIP
Language=ENU
TCP/IP network protocol not installed.
.
MessageId=9852
SymbolicName=DNS_ERROR_NO_DNS_SERVERS
Language=ENU
No DNS servers configured for local system.
.
MessageId=9901
SymbolicName=DNS_ERROR_DP_DOES_NOT_EXIST
Language=ENU
The specified directory partition does not exist.
.
MessageId=9902
SymbolicName=DNS_ERROR_DP_ALREADY_EXISTS
Language=ENU
The specified directory partition already exists.
.
MessageId=9903
SymbolicName=DNS_ERROR_DP_NOT_ENLISTED
Language=ENU
This DNS server is not enlisted in the specified directory partition.
.
MessageId=9904
SymbolicName=DNS_ERROR_DP_ALREADY_ENLISTED
Language=ENU
This DNS server is already enlisted in the specified directory partition.
.
MessageId=9905
SymbolicName=DNS_ERROR_DP_NOT_AVAILABLE
Language=ENU
The directory partition is not available at this time. Please wait a few minutes and try again.
.
MessageId=9906
SymbolicName=DNS_ERROR_DP_FSMO_ERROR
Language=ENU
The operation failed because the domain naming master FSMO role could not be reached. The domain controller holding the domain naming master FSMO role is down or unable to service the request or is not running Windows Server 2003 or later.
.
MessageId=9911
SymbolicName=DNS_ERROR_RRL_NOT_ENABLED
Language=ENU
The RRL is not enabled.
.
MessageId=9912
SymbolicName=DNS_ERROR_RRL_INVALID_WINDOW_SIZE
Language=ENU
The window size parameter is invalid. It should be greater than or equal to 1.
.
MessageId=9913
SymbolicName=DNS_ERROR_RRL_INVALID_IPV4_PREFIX
Language=ENU
The IPv4 prefix length parameter is invalid. It should be less than or equal to 32.
.
MessageId=9914
SymbolicName=DNS_ERROR_RRL_INVALID_IPV6_PREFIX
Language=ENU
The IPv6 prefix length parameter is invalid. It should be less than or equal to 128.
.
MessageId=9915
SymbolicName=DNS_ERROR_RRL_INVALID_TC_RATE
Language=ENU
The TC Rate parameter is invalid. It should be less than 10.
.
MessageId=9916
SymbolicName=DNS_ERROR_RRL_INVALID_LEAK_RATE
Language=ENU
The Leak Rate parameter is invalid. It should be either 0, or between 2 and 10.
.
MessageId=9917
SymbolicName=DNS_ERROR_RRL_LEAK_RATE_LESSTHAN_TC_RATE
Language=ENU
The Leak Rate or TC Rate parameter is invalid. Leak Rate should be greater than TC Rate.
.
MessageId=9921
SymbolicName=DNS_ERROR_VIRTUALIZATION_INSTANCE_ALREADY_EXISTS
Language=ENU
The virtualization instance already exists.
.
MessageId=9922
SymbolicName=DNS_ERROR_VIRTUALIZATION_INSTANCE_DOES_NOT_EXIST
Language=ENU
The virtualization instance does not exist.
.
MessageId=9923
SymbolicName=DNS_ERROR_VIRTUALIZATION_TREE_LOCKED
Language=ENU
The virtualization tree is locked.
.
MessageId=9924
SymbolicName=DNS_ERROR_INVAILD_VIRTUALIZATION_INSTANCE_NAME
Language=ENU
Invalid virtualization instance name.
.
MessageId=9925
SymbolicName=DNS_ERROR_DEFAULT_VIRTUALIZATION_INSTANCE
Language=ENU
The default virtualization instance cannot be added, removed or modified.
.
MessageId=9951
SymbolicName=DNS_ERROR_ZONESCOPE_ALREADY_EXISTS
Language=ENU
The scope already exists for the zone.
.
MessageId=9952
SymbolicName=DNS_ERROR_ZONESCOPE_DOES_NOT_EXIST
Language=ENU
The scope does not exist for the zone.
.
MessageId=9953
SymbolicName=DNS_ERROR_DEFAULT_ZONESCOPE
Language=ENU
The scope is the same as the default zone scope.
.
MessageId=9954
SymbolicName=DNS_ERROR_INVALID_ZONESCOPE_NAME
Language=ENU
The scope name contains invalid characters.
.
MessageId=9955
SymbolicName=DNS_ERROR_NOT_ALLOWED_WITH_ZONESCOPES
Language=ENU
Operation not allowed when the zone has scopes.
.
MessageId=9956
SymbolicName=DNS_ERROR_LOAD_ZONESCOPE_FAILED
Language=ENU
Failed to load zone scope.
.
MessageId=9957
SymbolicName=DNS_ERROR_ZONESCOPE_FILE_WRITEBACK_FAILED
Language=ENU
Failed to write data file for DNS zone scope. Please verify the file exists and is writable.
.
MessageId=9958
SymbolicName=DNS_ERROR_INVALID_SCOPE_NAME
Language=ENU
The scope name contains invalid characters.
.
MessageId=9959
SymbolicName=DNS_ERROR_SCOPE_DOES_NOT_EXIST
Language=ENU
The scope does not exist.
.
MessageId=9960
SymbolicName=DNS_ERROR_DEFAULT_SCOPE
Language=ENU
The scope is the same as the default scope.
.
MessageId=9961
SymbolicName=DNS_ERROR_INVALID_SCOPE_OPERATION
Language=ENU
The operation is invalid on the scope.
.
MessageId=9962
SymbolicName=DNS_ERROR_SCOPE_LOCKED
Language=ENU
The scope is locked.
.
MessageId=9963
SymbolicName=DNS_ERROR_SCOPE_ALREADY_EXISTS
Language=ENU
The scope already exists.
.
MessageId=9971
SymbolicName=DNS_ERROR_POLICY_ALREADY_EXISTS
Language=ENU
A policy with the same name already exists on this level (server level or zone level) on the DNS server.
.
MessageId=9972
SymbolicName=DNS_ERROR_POLICY_DOES_NOT_EXIST
Language=ENU
No policy with this name exists on this level (server level or zone level) on the DNS server.
.
MessageId=9973
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA
Language=ENU
The criteria provided in the policy are invalid.
.
MessageId=9974
SymbolicName=DNS_ERROR_POLICY_INVALID_SETTINGS
Language=ENU
At least one of the settings of this policy is invalid.
.
MessageId=9975
SymbolicName=DNS_ERROR_CLIENT_SUBNET_IS_ACCESSED
Language=ENU
The client subnet cannot be deleted while it is being accessed by a policy.
.
MessageId=9976
SymbolicName=DNS_ERROR_CLIENT_SUBNET_DOES_NOT_EXIST
Language=ENU
The client subnet does not exist on the DNS server.
.
MessageId=9977
SymbolicName=DNS_ERROR_CLIENT_SUBNET_ALREADY_EXISTS
Language=ENU
A client subnet with this name already exists on the DNS server.
.
MessageId=9978
SymbolicName=DNS_ERROR_SUBNET_DOES_NOT_EXIST
Language=ENU
The IP subnet specified does not exist in the client subnet.
.
MessageId=9979
SymbolicName=DNS_ERROR_SUBNET_ALREADY_EXISTS
Language=ENU
The IP subnet that is being added, already exists in the client subnet.
.
MessageId=9980
SymbolicName=DNS_ERROR_POLICY_LOCKED
Language=ENU
The policy is locked.
.
MessageId=9981
SymbolicName=DNS_ERROR_POLICY_INVALID_WEIGHT
Language=ENU
The weight of the scope in the policy is invalid.
.
MessageId=9982
SymbolicName=DNS_ERROR_POLICY_INVALID_NAME
Language=ENU
The DNS policy name is invalid.
.
MessageId=9983
SymbolicName=DNS_ERROR_POLICY_MISSING_CRITERIA
Language=ENU
The policy is missing criteria.
.
MessageId=9984
SymbolicName=DNS_ERROR_INVALID_CLIENT_SUBNET_NAME
Language=ENU
The name of the the client subnet record is invalid.
.
MessageId=9985
SymbolicName=DNS_ERROR_POLICY_PROCESSING_ORDER_INVALID
Language=ENU
Invalid policy processing order.
.
MessageId=9986
SymbolicName=DNS_ERROR_POLICY_SCOPE_MISSING
Language=ENU
The scope information has not been provided for a policy that requires it.
.
MessageId=9987
SymbolicName=DNS_ERROR_POLICY_SCOPE_NOT_ALLOWED
Language=ENU
The scope information has been provided for a policy that does not require it.
.
MessageId=9988
SymbolicName=DNS_ERROR_SERVERSCOPE_IS_REFERENCED
Language=ENU
The server scope cannot be deleted because it is referenced by a DNS Policy.
.
MessageId=9989
SymbolicName=DNS_ERROR_ZONESCOPE_IS_REFERENCED
Language=ENU
The zone scope cannot be deleted because it is referenced by a DNS Policy.
.
MessageId=9990
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_CLIENT_SUBNET
Language=ENU
The criterion client subnet provided in the policy is invalid.
.
MessageId=9991
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_TRANSPORT_PROTOCOL
Language=ENU
The criterion transport protocol provided in the policy is invalid.
.
MessageId=9992
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_NETWORK_PROTOCOL
Language=ENU
The criterion network protocol provided in the policy is invalid.
.
MessageId=9993
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_INTERFACE
Language=ENU
The criterion interface provided in the policy is invalid.
.
MessageId=9994
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_FQDN
Language=ENU
The criterion FQDN provided in the policy is invalid.
.
MessageId=9995
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_QUERY_TYPE
Language=ENU
The criterion query type provided in the policy is invalid.
.
MessageId=9996
SymbolicName=DNS_ERROR_POLICY_INVALID_CRITERIA_TIME_OF_DAY
Language=ENU
The criterion time of day provided in the policy is invalid.
.
MessageId=10004
SymbolicName=WSAEINTR
Language=ENU
A blocking operation was interrupted by a call to WSACancelBlockingCall.
.
MessageId=10009
SymbolicName=WSAEBADF
Language=ENU
The file handle supplied is not valid.
.
MessageId=10013
SymbolicName=WSAEACCES
Language=ENU
An attempt was made to access a socket in a way forbidden by its access permissions.
.
MessageId=10014
SymbolicName=WSAEFAULT
Language=ENU
The system detected an invalid pointer address in attempting to use a pointer argument in a call.
.
MessageId=10022
SymbolicName=WSAEINVAL
Language=ENU
An invalid argument was supplied.
.
MessageId=10024
SymbolicName=WSAEMFILE
Language=ENU
Too many open sockets.
.
MessageId=10035
SymbolicName=WSAEWOULDBLOCK
Language=ENU
A non-blocking socket operation could not be completed immediately.
.
MessageId=10036
SymbolicName=WSAEINPROGRESS
Language=ENU
A blocking operation is currently executing.
.
MessageId=10037
SymbolicName=WSAEALREADY
Language=ENU
An operation was attempted on a non-blocking socket that already had an operation in progress.
.
MessageId=10038
SymbolicName=WSAENOTSOCK
Language=ENU
An operation was attempted on something that is not a socket.
.
MessageId=10039
SymbolicName=WSAEDESTADDRREQ
Language=ENU
A required address was omitted from an operation on a socket.
.
MessageId=10040
SymbolicName=WSAEMSGSIZE
Language=ENU
A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself.
.
MessageId=10041
SymbolicName=WSAEPROTOTYPE
Language=ENU
A protocol was specified in the socket function call that does not support the semantics of the socket type requested.
.
MessageId=10042
SymbolicName=WSAENOPROTOOPT
Language=ENU
An unknown, invalid, or unsupported option or level was specified in a getsockopt or setsockopt call.
.
MessageId=10043
SymbolicName=WSAEPROTONOSUPPORT
Language=ENU
The requested protocol has not been configured into the system, or no implementation for it exists.
.
MessageId=10044
SymbolicName=WSAESOCKTNOSUPPORT
Language=ENU
The support for the specified socket type does not exist in this address family.
.
MessageId=10045
SymbolicName=WSAEOPNOTSUPP
Language=ENU
The attempted operation is not supported for the type of object referenced.
.
MessageId=10046
SymbolicName=WSAEPFNOSUPPORT
Language=ENU
The protocol family has not been configured into the system or no implementation for it exists.
.
MessageId=10047
SymbolicName=WSAEAFNOSUPPORT
Language=ENU
An address incompatible with the requested protocol was used.
.
MessageId=10048
SymbolicName=WSAEADDRINUSE
Language=ENU
Only one usage of each socket address (protocol/network address/port) is normally permitted.
.
MessageId=10049
SymbolicName=WSAEADDRNOTAVAIL
Language=ENU
The requested address is not valid in its context.
.
MessageId=10050
SymbolicName=WSAENETDOWN
Language=ENU
A socket operation encountered a dead network.
.
MessageId=10051
SymbolicName=WSAENETUNREACH
Language=ENU
A socket operation was attempted to an unreachable network.
.
MessageId=10052
SymbolicName=WSAENETRESET
Language=ENU
The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress.
.
MessageId=10053
SymbolicName=WSAECONNABORTED
Language=ENU
An established connection was aborted by the software in your host machine.
.
MessageId=10054
SymbolicName=WSAECONNRESET
Language=ENU
An existing connection was forcibly closed by the remote host.
.
MessageId=10055
SymbolicName=WSAENOBUFS
Language=ENU
An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full.
.
MessageId=10056
SymbolicName=WSAEISCONN
Language=ENU
A connect request was made on an already connected socket.
.
MessageId=10057
SymbolicName=WSAENOTCONN
Language=ENU
A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied.
.
MessageId=10058
SymbolicName=WSAESHUTDOWN
Language=ENU
A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call.
.
MessageId=10059
SymbolicName=WSAETOOMANYREFS
Language=ENU
Too many references to some kernel object.
.
MessageId=10060
SymbolicName=WSAETIMEDOUT
Language=ENU
A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond.
.
MessageId=10061
SymbolicName=WSAECONNREFUSED
Language=ENU
No connection could be made because the target machine actively refused it.
.
MessageId=10062
SymbolicName=WSAELOOP
Language=ENU
Cannot translate name.
.
MessageId=10063
SymbolicName=WSAENAMETOOLONG
Language=ENU
Name component or name was too long.
.
MessageId=10064
SymbolicName=WSAEHOSTDOWN
Language=ENU
A socket operation failed because the destination host was down.
.
MessageId=10065
SymbolicName=WSAEHOSTUNREACH
Language=ENU
A socket operation was attempted to an unreachable host.
.
MessageId=10066
SymbolicName=WSAENOTEMPTY
Language=ENU
Cannot remove a directory that is not empty.
.
MessageId=10067
SymbolicName=WSAEPROCLIM
Language=ENU
A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously.
.
MessageId=10068
SymbolicName=WSAEUSERS
Language=ENU
Ran out of quota.
.
MessageId=10069
SymbolicName=WSAEDQUOT
Language=ENU
Ran out of disk quota.
.
MessageId=10070
SymbolicName=WSAESTALE
Language=ENU
File handle reference is no longer available.
.
MessageId=10071
SymbolicName=WSAEREMOTE
Language=ENU
Item is not available locally.
.
MessageId=10091
SymbolicName=WSASYSNOTREADY
Language=ENU
WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable.
.
MessageId=10092
SymbolicName=WSAVERNOTSUPPORTED
Language=ENU
The Windows Sockets version requested is not supported.
.
MessageId=10093
SymbolicName=WSANOTINITIALISED
Language=ENU
Either the application has not called WSAStartup, or WSAStartup failed.
.
MessageId=10101
SymbolicName=WSAEDISCON
Language=ENU
Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence.
.
MessageId=10102
SymbolicName=WINERR_10102
Language=ENU
No more results can be returned by WSALookupServiceNext.
.
MessageId=10103
SymbolicName=WINERR_10103
Language=ENU
A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled.
.
MessageId=10104
SymbolicName=WINERR_10104
Language=ENU
The procedure call table is invalid.
.
MessageId=10105
SymbolicName=WINERR_10105
Language=ENU
The requested service provider is invalid.
.
MessageId=10106
SymbolicName=WINERR_10106
Language=ENU
The requested service provider could not be loaded or initialized.
.
MessageId=10107
SymbolicName=WINERR_10107
Language=ENU
A system call has failed.
.
MessageId=10108
SymbolicName=WINERR_10108
Language=ENU
No such service is known. The service cannot be found in the specified name space.
.
MessageId=10109
SymbolicName=WINERR_10109
Language=ENU
The specified class was not found.
.
MessageId=10110
SymbolicName=WINERR_10110
Language=ENU
No more results can be returned by WSALookupServiceNext.
.
MessageId=10111
SymbolicName=WINERR_10111
Language=ENU
A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled.
.
MessageId=10112
SymbolicName=WINERR_10112
Language=ENU
A database query failed because it was actively refused.
.
MessageId=11001
SymbolicName=WSAHOST_NOT_FOUND
Language=ENU
No such host is known.
.
MessageId=11002
SymbolicName=WSATRY_AGAIN
Language=ENU
This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server.
.
MessageId=11003
SymbolicName=WSANO_RECOVERY
Language=ENU
A non-recoverable error occurred during a database lookup.
.
MessageId=11004
SymbolicName=WSANO_DATA
Language=ENU
The requested name is valid, but no data of the requested type was found.
.
MessageId=11005
SymbolicName=WSA_QOS_RECEIVERS
Language=ENU
At least one reserve has arrived.
.
MessageId=11006
SymbolicName=WSA_QOS_SENDERS
Language=ENU
At least one path has arrived.
.
MessageId=11007
SymbolicName=WSA_QOS_NO_SENDERS
Language=ENU
There are no senders.
.
MessageId=11008
SymbolicName=WSA_QOS_NO_RECEIVERS
Language=ENU
There are no receivers.
.
MessageId=11009
SymbolicName=WSA_QOS_REQUEST_CONFIRMED
Language=ENU
Reserve has been confirmed.
.
MessageId=11010
SymbolicName=WSA_QOS_ADMISSION_FAILURE
Language=ENU
Error due to lack of resources.
.
MessageId=11011
SymbolicName=WSA_QOS_POLICY_FAILURE
Language=ENU
Rejected for administrative reasons - bad credentials.
.
MessageId=11012
SymbolicName=WSA_QOS_BAD_STYLE
Language=ENU
Unknown or conflicting style.
.
MessageId=11013
SymbolicName=WSA_QOS_BAD_OBJECT
Language=ENU
Problem with some part of the filterspec or providerspecific buffer in general.
.
MessageId=11014
SymbolicName=WSA_QOS_TRAFFIC_CTRL_ERROR
Language=ENU
Problem with some part of the flowspec.
.
MessageId=11015
SymbolicName=WSA_QOS_GENERIC_ERROR
Language=ENU
General QOS error.
.
MessageId=11016
SymbolicName=WSA_QOS_ESERVICETYPE
Language=ENU
An invalid or unrecognized service type was found in the flowspec.
.
MessageId=11017
SymbolicName=WSA_QOS_EFLOWSPEC
Language=ENU
An invalid or inconsistent flowspec was found in the QOS structure.
.
MessageId=11018
SymbolicName=WSA_QOS_EPROVSPECBUF
Language=ENU
Invalid QOS provider-specific buffer.
.
MessageId=11019
SymbolicName=WSA_QOS_EFILTERSTYLE
Language=ENU
An invalid QOS filter style was used.
.
MessageId=11020
SymbolicName=WSA_QOS_EFILTERTYPE
Language=ENU
An invalid QOS filter type was used.
.
MessageId=11021
SymbolicName=WSA_QOS_EFILTERCOUNT
Language=ENU
An incorrect number of QOS FILTERSPECs were specified in the FLOWDESCRIPTOR.
.
MessageId=11022
SymbolicName=WSA_QOS_EOBJLENGTH
Language=ENU
An object with an invalid ObjectLength field was specified in the QOS provider-specific buffer.
.
MessageId=11023
SymbolicName=WSA_QOS_EFLOWCOUNT
Language=ENU
An incorrect number of flow descriptors was specified in the QOS structure.
.
MessageId=11024
SymbolicName=WSA_QOS_EUNKOWNPSOBJ
Language=ENU
An unrecognized object was found in the QOS provider-specific buffer.
.
MessageId=11025
SymbolicName=WSA_QOS_EPOLICYOBJ
Language=ENU
An invalid policy object was found in the QOS provider-specific buffer.
.
MessageId=11026
SymbolicName=WSA_QOS_EFLOWDESC
Language=ENU
An invalid QOS flow descriptor was found in the flow descriptor list.
.
MessageId=11027
SymbolicName=WSA_QOS_EPSFLOWSPEC
Language=ENU
An invalid or inconsistent flowspec was found in the QOS provider specific buffer.
.
MessageId=11028
SymbolicName=WSA_QOS_EPSFILTERSPEC
Language=ENU
An invalid FILTERSPEC was found in the QOS provider-specific buffer.
.
MessageId=11029
SymbolicName=WSA_QOS_ESDMODEOBJ
Language=ENU
An invalid shape discard mode object was found in the QOS provider specific buffer.
.
MessageId=11030
SymbolicName=WSA_QOS_ESHAPERATEOBJ
Language=ENU
An invalid shaping rate object was found in the QOS provider-specific buffer.
.
MessageId=11031
SymbolicName=WSA_QOS_RESERVED_PETYPE
Language=ENU
A reserved policy element was found in the QOS provider-specific buffer.
.
MessageId=11032
SymbolicName=WSA_SECURE_HOST_NOT_FOUND
Language=ENU
No such host is known securely.
.
MessageId=11033
SymbolicName=WSA_IPSEC_NAME_POLICY_ERROR
Language=ENU
Name based IPSEC policy could not be added.
.
MessageId=13000
SymbolicName=ERROR_IPSEC_QM_POLICY_EXISTS
Language=ENU
The specified quick mode policy already exists.
.
MessageId=13001
SymbolicName=ERROR_IPSEC_QM_POLICY_NOT_FOUND
Language=ENU
The specified quick mode policy was not found.
.
MessageId=13002
SymbolicName=ERROR_IPSEC_QM_POLICY_IN_USE
Language=ENU
The specified quick mode policy is being used.
.
MessageId=13003
SymbolicName=ERROR_IPSEC_MM_POLICY_EXISTS
Language=ENU
The specified main mode policy already exists.
.
MessageId=13004
SymbolicName=ERROR_IPSEC_MM_POLICY_NOT_FOUND
Language=ENU
The specified main mode policy was not found
.
MessageId=13005
SymbolicName=ERROR_IPSEC_MM_POLICY_IN_USE
Language=ENU
The specified main mode policy is being used.
.
MessageId=13006
SymbolicName=ERROR_IPSEC_MM_FILTER_EXISTS
Language=ENU
The specified main mode filter already exists.
.
MessageId=13007
SymbolicName=ERROR_IPSEC_MM_FILTER_NOT_FOUND
Language=ENU
The specified main mode filter was not found.
.
MessageId=13008
SymbolicName=ERROR_IPSEC_TRANSPORT_FILTER_EXISTS
Language=ENU
The specified transport mode filter already exists.
.
MessageId=13009
SymbolicName=ERROR_IPSEC_TRANSPORT_FILTER_NOT_FOUND
Language=ENU
The specified transport mode filter does not exist.
.
MessageId=13010
SymbolicName=ERROR_IPSEC_MM_AUTH_EXISTS
Language=ENU
The specified main mode authentication list exists.
.
MessageId=13011
SymbolicName=ERROR_IPSEC_MM_AUTH_NOT_FOUND
Language=ENU
The specified main mode authentication list was not found.
.
MessageId=13012
SymbolicName=ERROR_IPSEC_MM_AUTH_IN_USE
Language=ENU
The specified main mode authentication list is being used.
.
MessageId=13013
SymbolicName=ERROR_IPSEC_DEFAULT_MM_POLICY_NOT_FOUND
Language=ENU
The specified default main mode policy was not found.
.
MessageId=13014
SymbolicName=ERROR_IPSEC_DEFAULT_MM_AUTH_NOT_FOUND
Language=ENU
The specified default main mode authentication list was not found.
.
MessageId=13015
SymbolicName=ERROR_IPSEC_DEFAULT_QM_POLICY_NOT_FOUND
Language=ENU
The specified default quick mode policy was not found.
.
MessageId=13016
SymbolicName=ERROR_IPSEC_TUNNEL_FILTER_EXISTS
Language=ENU
The specified tunnel mode filter exists.
.
MessageId=13017
SymbolicName=ERROR_IPSEC_TUNNEL_FILTER_NOT_FOUND
Language=ENU
The specified tunnel mode filter was not found.
.
MessageId=13018
SymbolicName=ERROR_IPSEC_MM_FILTER_PENDING_DELETION
Language=ENU
The Main Mode filter is pending deletion.
.
MessageId=13019
SymbolicName=ERROR_IPSEC_TRANSPORT_FILTER_PENDING_DELETION
Language=ENU
The transport filter is pending deletion.
.
MessageId=13020
SymbolicName=ERROR_IPSEC_TUNNEL_FILTER_PENDING_DELETION
Language=ENU
The tunnel filter is pending deletion.
.
MessageId=13021
SymbolicName=ERROR_IPSEC_MM_POLICY_PENDING_DELETION
Language=ENU
The Main Mode policy is pending deletion.
.
MessageId=13022
SymbolicName=ERROR_IPSEC_MM_AUTH_PENDING_DELETION
Language=ENU
The Main Mode authentication bundle is pending deletion.
.
MessageId=13023
SymbolicName=ERROR_IPSEC_QM_POLICY_PENDING_DELETION
Language=ENU
The Quick Mode policy is pending deletion.
.
MessageId=13024
SymbolicName=WINERR_13024
Language=ENU
The Main Mode policy was successfully added, but some of the requested offers are not supported.
.
MessageId=13025
SymbolicName=WINERR_13025
Language=ENU
The Quick Mode policy was successfully added, but some of the requested offers are not supported.
.
MessageId=13800
SymbolicName=ERROR_IPSEC_IKE_NEG_STATUS_BEGIN
Language=ENU
ERROR_IPSEC_IKE_NEG_STATUS_BEGIN
.
MessageId=13801
SymbolicName=ERROR_IPSEC_IKE_AUTH_FAIL
Language=ENU
IKE authentication credentials are unacceptable
.
MessageId=13802
SymbolicName=ERROR_IPSEC_IKE_ATTRIB_FAIL
Language=ENU
IKE security attributes are unacceptable
.
MessageId=13803
SymbolicName=ERROR_IPSEC_IKE_NEGOTIATION_PENDING
Language=ENU
IKE Negotiation in progress
.
MessageId=13804
SymbolicName=ERROR_IPSEC_IKE_GENERAL_PROCESSING_ERROR
Language=ENU
General processing error
.
MessageId=13805
SymbolicName=ERROR_IPSEC_IKE_TIMED_OUT
Language=ENU
Negotiation timed out
.
MessageId=13806
SymbolicName=ERROR_IPSEC_IKE_NO_CERT
Language=ENU
IKE failed to find valid machine certificate. Contact your Network Security Administrator about installing a valid certificate in the appropriate Certificate Store.
.
MessageId=13807
SymbolicName=ERROR_IPSEC_IKE_SA_DELETED
Language=ENU
IKE SA deleted by peer before establishment completed
.
MessageId=13808
SymbolicName=ERROR_IPSEC_IKE_SA_REAPED
Language=ENU
IKE SA deleted before establishment completed
.
MessageId=13809
SymbolicName=ERROR_IPSEC_IKE_MM_ACQUIRE_DROP
Language=ENU
Negotiation request sat in Queue too long
.
MessageId=13810
SymbolicName=ERROR_IPSEC_IKE_QM_ACQUIRE_DROP
Language=ENU
Negotiation request sat in Queue too long
.
MessageId=13811
SymbolicName=ERROR_IPSEC_IKE_QUEUE_DROP_MM
Language=ENU
Negotiation request sat in Queue too long
.
MessageId=13812
SymbolicName=ERROR_IPSEC_IKE_QUEUE_DROP_NO_MM
Language=ENU
Negotiation request sat in Queue too long
.
MessageId=13813
SymbolicName=ERROR_IPSEC_IKE_DROP_NO_RESPONSE
Language=ENU
No response from peer
.
MessageId=13814
SymbolicName=ERROR_IPSEC_IKE_MM_DELAY_DROP
Language=ENU
Negotiation took too long
.
MessageId=13815
SymbolicName=ERROR_IPSEC_IKE_QM_DELAY_DROP
Language=ENU
Negotiation took too long
.
MessageId=13816
SymbolicName=ERROR_IPSEC_IKE_ERROR
Language=ENU
Unknown error occurred
.
MessageId=13817
SymbolicName=ERROR_IPSEC_IKE_CRL_FAILED
Language=ENU
Certificate Revocation Check failed
.
MessageId=13818
SymbolicName=ERROR_IPSEC_IKE_INVALID_KEY_USAGE
Language=ENU
Invalid certificate key usage
.
MessageId=13819
SymbolicName=ERROR_IPSEC_IKE_INVALID_CERT_TYPE
Language=ENU
Invalid certificate type
.
MessageId=13820
SymbolicName=ERROR_IPSEC_IKE_NO_PRIVATE_KEY
Language=ENU
IKE negotiation failed because the machine certificate used does not have a private key. IPsec certificates require a private key. Contact your Network Security administrator about replacing with a certificate that has a private key.
.
MessageId=13821
SymbolicName=ERROR_IPSEC_IKE_SIMULTANEOUS_REKEY
Language=ENU
Simultaneous rekeys were detected.
.
MessageId=13822
SymbolicName=ERROR_IPSEC_IKE_DH_FAIL
Language=ENU
Failure in Diffie-Hellman computation
.
MessageId=13823
SymbolicName=ERROR_IPSEC_IKE_CRITICAL_PAYLOAD_NOT_RECOGNIZED
Language=ENU
Don't know how to process critical payload
.
MessageId=13824
SymbolicName=ERROR_IPSEC_IKE_INVALID_HEADER
Language=ENU
Invalid header
.
MessageId=13825
SymbolicName=ERROR_IPSEC_IKE_NO_POLICY
Language=ENU
No policy configured
.
MessageId=13826
SymbolicName=ERROR_IPSEC_IKE_INVALID_SIGNATURE
Language=ENU
Failed to verify signature
.
MessageId=13827
SymbolicName=ERROR_IPSEC_IKE_KERBEROS_ERROR
Language=ENU
Failed to authenticate using Kerberos
.
MessageId=13828
SymbolicName=ERROR_IPSEC_IKE_NO_PUBLIC_KEY
Language=ENU
Peer's certificate did not have a public key
.
MessageId=13829
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR
Language=ENU
Error processing error payload
.
MessageId=13830
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_SA
Language=ENU
Error processing SA payload
.
MessageId=13831
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_PROP
Language=ENU
Error processing Proposal payload
.
MessageId=13832
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_TRANS
Language=ENU
Error processing Transform payload
.
MessageId=13833
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_KE
Language=ENU
Error processing KE payload
.
MessageId=13834
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_ID
Language=ENU
Error processing ID payload
.
MessageId=13835
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_CERT
Language=ENU
Error processing Cert payload
.
MessageId=13836
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_CERT_REQ
Language=ENU
Error processing Certificate Request payload
.
MessageId=13837
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_HASH
Language=ENU
Error processing Hash payload
.
MessageId=13838
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_SIG
Language=ENU
Error processing Signature payload
.
MessageId=13839
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_NONCE
Language=ENU
Error processing Nonce payload
.
MessageId=13840
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_NOTIFY
Language=ENU
Error processing Notify payload
.
MessageId=13841
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_DELETE
Language=ENU
Error processing Delete Payload
.
MessageId=13842
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_VENDOR
Language=ENU
Error processing VendorId payload
.
MessageId=13843
SymbolicName=ERROR_IPSEC_IKE_INVALID_PAYLOAD
Language=ENU
Invalid payload received
.
MessageId=13844
SymbolicName=ERROR_IPSEC_IKE_LOAD_SOFT_SA
Language=ENU
Soft SA loaded
.
MessageId=13845
SymbolicName=ERROR_IPSEC_IKE_SOFT_SA_TORN_DOWN
Language=ENU
Soft SA torn down
.
MessageId=13846
SymbolicName=ERROR_IPSEC_IKE_INVALID_COOKIE
Language=ENU
Invalid cookie received.
.
MessageId=13847
SymbolicName=ERROR_IPSEC_IKE_NO_PEER_CERT
Language=ENU
Peer failed to send valid machine certificate
.
MessageId=13848
SymbolicName=ERROR_IPSEC_IKE_PEER_CRL_FAILED
Language=ENU
Certification Revocation check of peer's certificate failed
.
MessageId=13849
SymbolicName=ERROR_IPSEC_IKE_POLICY_CHANGE
Language=ENU
New policy invalidated SAs formed with old policy
.
MessageId=13850
SymbolicName=ERROR_IPSEC_IKE_NO_MM_POLICY
Language=ENU
There is no available Main Mode IKE policy.
.
MessageId=13851
SymbolicName=ERROR_IPSEC_IKE_NOTCBPRIV
Language=ENU
Failed to enabled TCB privilege.
.
MessageId=13852
SymbolicName=ERROR_IPSEC_IKE_SECLOADFAIL
Language=ENU
Failed to load SECURITY.DLL.
.
MessageId=13853
SymbolicName=ERROR_IPSEC_IKE_FAILSSPINIT
Language=ENU
Failed to obtain security function table dispatch address from SSPI.
.
MessageId=13854
SymbolicName=ERROR_IPSEC_IKE_FAILQUERYSSP
Language=ENU
Failed to query Kerberos package to obtain max token size.
.
MessageId=13855
SymbolicName=ERROR_IPSEC_IKE_SRVACQFAIL
Language=ENU
Failed to obtain Kerberos server credentials for ISAKMP/ERROR_IPSEC_IKE service. Kerberos authentication will not function. The most likely reason for this is lack of domain membership. This is normal if your computer is a member of a workgroup.
.
MessageId=13856
SymbolicName=ERROR_IPSEC_IKE_SRVQUERYCRED
Language=ENU
Failed to determine SSPI principal name for ISAKMP/ERROR_IPSEC_IKE service (QueryCredentialsAttributes).
.
MessageId=13857
SymbolicName=ERROR_IPSEC_IKE_GETSPIFAIL
Language=ENU
Failed to obtain new SPI for the inbound SA from IPsec driver. The most common cause for this is that the driver does not have the correct filter. Check your policy to verify the filters.
.
MessageId=13858
SymbolicName=ERROR_IPSEC_IKE_INVALID_FILTER
Language=ENU
Given filter is invalid
.
MessageId=13859
SymbolicName=ERROR_IPSEC_IKE_OUT_OF_MEMORY
Language=ENU
Memory allocation failed.
.
MessageId=13860
SymbolicName=ERROR_IPSEC_IKE_ADD_UPDATE_KEY_FAILED
Language=ENU
Failed to add Security Association to IPsec Driver. The most common cause for this is if the IKE negotiation took too long to complete. If the problem persists, reduce the load on the faulting machine.
.
MessageId=13861
SymbolicName=ERROR_IPSEC_IKE_INVALID_POLICY
Language=ENU
Invalid policy
.
MessageId=13862
SymbolicName=ERROR_IPSEC_IKE_UNKNOWN_DOI
Language=ENU
Invalid DOI
.
MessageId=13863
SymbolicName=ERROR_IPSEC_IKE_INVALID_SITUATION
Language=ENU
Invalid situation
.
MessageId=13864
SymbolicName=ERROR_IPSEC_IKE_DH_FAILURE
Language=ENU
Diffie-Hellman failure
.
MessageId=13865
SymbolicName=ERROR_IPSEC_IKE_INVALID_GROUP
Language=ENU
Invalid Diffie-Hellman group
.
MessageId=13866
SymbolicName=ERROR_IPSEC_IKE_ENCRYPT
Language=ENU
Error encrypting payload
.
MessageId=13867
SymbolicName=ERROR_IPSEC_IKE_DECRYPT
Language=ENU
Error decrypting payload
.
MessageId=13868
SymbolicName=ERROR_IPSEC_IKE_POLICY_MATCH
Language=ENU
Policy match error
.
MessageId=13869
SymbolicName=ERROR_IPSEC_IKE_UNSUPPORTED_ID
Language=ENU
Unsupported ID
.
MessageId=13870
SymbolicName=ERROR_IPSEC_IKE_INVALID_HASH
Language=ENU
Hash verification failed
.
MessageId=13871
SymbolicName=ERROR_IPSEC_IKE_INVALID_HASH_ALG
Language=ENU
Invalid hash algorithm
.
MessageId=13872
SymbolicName=ERROR_IPSEC_IKE_INVALID_HASH_SIZE
Language=ENU
Invalid hash size
.
MessageId=13873
SymbolicName=ERROR_IPSEC_IKE_INVALID_ENCRYPT_ALG
Language=ENU
Invalid encryption algorithm
.
MessageId=13874
SymbolicName=ERROR_IPSEC_IKE_INVALID_AUTH_ALG
Language=ENU
Invalid authentication algorithm
.
MessageId=13875
SymbolicName=ERROR_IPSEC_IKE_INVALID_SIG
Language=ENU
Invalid certificate signature
.
MessageId=13876
SymbolicName=ERROR_IPSEC_IKE_LOAD_FAILED
Language=ENU
Load failed
.
MessageId=13877
SymbolicName=ERROR_IPSEC_IKE_RPC_DELETE
Language=ENU
Deleted via RPC call
.
MessageId=13878
SymbolicName=ERROR_IPSEC_IKE_BENIGN_REINIT
Language=ENU
Temporary state created to perform reinitialization. This is not a real failure.
.
MessageId=13879
SymbolicName=ERROR_IPSEC_IKE_INVALID_RESPONDER_LIFETIME_NOTIFY
Language=ENU
The lifetime value received in the Responder Lifetime Notify is below the Windows 2000 configured minimum value. Please fix the policy on the peer machine.
.
MessageId=13880
SymbolicName=ERROR_IPSEC_IKE_INVALID_MAJOR_VERSION
Language=ENU
The recipient cannot handle version of IKE specified in the header.
.
MessageId=13881
SymbolicName=ERROR_IPSEC_IKE_INVALID_CERT_KEYLEN
Language=ENU
Key length in certificate is too small for configured security requirements.
.
MessageId=13882
SymbolicName=ERROR_IPSEC_IKE_MM_LIMIT
Language=ENU
Max number of established MM SAs to peer exceeded.
.
MessageId=13883
SymbolicName=ERROR_IPSEC_IKE_NEGOTIATION_DISABLED
Language=ENU
IKE received a policy that disables negotiation.
.
MessageId=13884
SymbolicName=ERROR_IPSEC_IKE_QM_LIMIT
Language=ENU
Reached maximum quick mode limit for the main mode. New main mode will be started.
.
MessageId=13885
SymbolicName=ERROR_IPSEC_IKE_MM_EXPIRED
Language=ENU
Main mode SA lifetime expired or peer sent a main mode delete.
.
MessageId=13886
SymbolicName=ERROR_IPSEC_IKE_PEER_MM_ASSUMED_INVALID
Language=ENU
Main mode SA assumed to be invalid because peer stopped responding.
.
MessageId=13887
SymbolicName=ERROR_IPSEC_IKE_CERT_CHAIN_POLICY_MISMATCH
Language=ENU
Certificate doesn't chain to a trusted root in IPsec policy.
.
MessageId=13888
SymbolicName=ERROR_IPSEC_IKE_UNEXPECTED_MESSAGE_ID
Language=ENU
Received unexpected message ID.
.
MessageId=13889
SymbolicName=ERROR_IPSEC_IKE_INVALID_AUTH_PAYLOAD
Language=ENU
Received invalid authentication offers.
.
MessageId=13890
SymbolicName=ERROR_IPSEC_IKE_DOS_COOKIE_SENT
Language=ENU
Sent DoS cookie notify to initiator.
.
MessageId=13891
SymbolicName=ERROR_IPSEC_IKE_SHUTTING_DOWN
Language=ENU
IKE service is shutting down.
.
MessageId=13892
SymbolicName=ERROR_IPSEC_IKE_CGA_AUTH_FAILED
Language=ENU
Could not verify binding between CGA address and certificate.
.
MessageId=13893
SymbolicName=ERROR_IPSEC_IKE_PROCESS_ERR_NATOA
Language=ENU
Error processing NatOA payload.
.
MessageId=13894
SymbolicName=ERROR_IPSEC_IKE_INVALID_MM_FOR_QM
Language=ENU
Parameters of the main mode are invalid for this quick mode.
.
MessageId=13895
SymbolicName=ERROR_IPSEC_IKE_QM_EXPIRED
Language=ENU
Quick mode SA was expired by IPsec driver.
.
MessageId=13896
SymbolicName=ERROR_IPSEC_IKE_TOO_MANY_FILTERS
Language=ENU
Too many dynamically added IKEEXT filters were detected.
.
MessageId=13897
SymbolicName=ERROR_IPSEC_IKE_NEG_STATUS_END
Language=ENU
ERROR_IPSEC_IKE_NEG_STATUS_END
.
MessageId=13898
SymbolicName=ERROR_IPSEC_IKE_KILL_DUMMY_NAP_TUNNEL
Language=ENU
NAP reauth succeeded and must delete the dummy NAP IKEv2 tunnel.
.
MessageId=13899
SymbolicName=ERROR_IPSEC_IKE_INNER_IP_ASSIGNMENT_FAILURE
Language=ENU
Error in assigning inner IP address to initiator in tunnel mode.
.
MessageId=13900
SymbolicName=ERROR_IPSEC_IKE_REQUIRE_CP_PAYLOAD_MISSING
Language=ENU
Require configuration payload missing.
.
MessageId=13901
SymbolicName=ERROR_IPSEC_KEY_MODULE_IMPERSONATION_NEGOTIATION_PENDING
Language=ENU
A negotiation running as the security principle who issued the connection is in progress
.
MessageId=13902
SymbolicName=ERROR_IPSEC_IKE_COEXISTENCE_SUPPRESS
Language=ENU
SA was deleted due to IKEv1/AuthIP co-existence suppress check.
.
MessageId=13903
SymbolicName=ERROR_IPSEC_IKE_RATELIMIT_DROP
Language=ENU
Incoming SA request was dropped due to peer IP address rate limiting.
.
MessageId=13904
SymbolicName=ERROR_IPSEC_IKE_PEER_DOESNT_SUPPORT_MOBIKE
Language=ENU
Peer does not support MOBIKE.
.
MessageId=13905
SymbolicName=ERROR_IPSEC_IKE_AUTHORIZATION_FAILURE
Language=ENU
SA establishment is not authorized.
.
MessageId=13906
SymbolicName=ERROR_IPSEC_IKE_STRONG_CRED_AUTHORIZATION_FAILURE
Language=ENU
SA establishment is not authorized because there is not a sufficiently strong PKINIT-based credential.
.
MessageId=13907
SymbolicName=ERROR_IPSEC_IKE_AUTHORIZATION_FAILURE_WITH_OPTIONAL_RETRY
Language=ENU
SA establishment is not authorized.  You may need to enter updated or different credentials such as a smartcard.
.
MessageId=13908
SymbolicName=ERROR_IPSEC_IKE_STRONG_CRED_AUTHORIZATION_AND_CERTMAP_FAILURE
Language=ENU
SA establishment is not authorized because there is not a sufficiently strong PKINIT-based credential. This might be related to certificate-to-account mapping failure for the SA.
.
MessageId=13909
SymbolicName=ERROR_IPSEC_IKE_NEG_STATUS_EXTENDED_END
Language=ENU
ERROR_IPSEC_IKE_NEG_STATUS_EXTENDED_END
.
MessageId=13910
SymbolicName=ERROR_IPSEC_BAD_SPI
Language=ENU
The SPI in the packet does not match a valid IPsec SA.
.
MessageId=13911
SymbolicName=ERROR_IPSEC_SA_LIFETIME_EXPIRED
Language=ENU
Packet was received on an IPsec SA whose lifetime has expired.
.
MessageId=13912
SymbolicName=ERROR_IPSEC_WRONG_SA
Language=ENU
Packet was received on an IPsec SA that does not match the packet characteristics.
.
MessageId=13913
SymbolicName=ERROR_IPSEC_REPLAY_CHECK_FAILED
Language=ENU
Packet sequence number replay check failed.
.
MessageId=13914
SymbolicName=ERROR_IPSEC_INVALID_PACKET
Language=ENU
IPsec header and/or trailer in the packet is invalid.
.
MessageId=13915
SymbolicName=ERROR_IPSEC_INTEGRITY_CHECK_FAILED
Language=ENU
IPsec integrity check failed.
.
MessageId=13916
SymbolicName=ERROR_IPSEC_CLEAR_TEXT_DROP
Language=ENU
IPsec dropped a clear text packet.
.
MessageId=13917
SymbolicName=ERROR_IPSEC_AUTH_FIREWALL_DROP
Language=ENU
IPsec dropped an incoming ESP packet in authenticated firewall mode. This drop is benign.
.
MessageId=13918
SymbolicName=ERROR_IPSEC_THROTTLE_DROP
Language=ENU
IPsec dropped a packet due to DoS throttling.
.
MessageId=13925
SymbolicName=ERROR_IPSEC_DOSP_BLOCK
Language=ENU
IPsec DoS Protection matched an explicit block rule.
.
MessageId=13926
SymbolicName=ERROR_IPSEC_DOSP_RECEIVED_MULTICAST
Language=ENU
IPsec DoS Protection received an IPsec specific multicast packet which is not allowed.
.
MessageId=13927
SymbolicName=ERROR_IPSEC_DOSP_INVALID_PACKET
Language=ENU
IPsec DoS Protection received an incorrectly formatted packet.
.
MessageId=13928
SymbolicName=ERROR_IPSEC_DOSP_STATE_LOOKUP_FAILED
Language=ENU
IPsec DoS Protection failed to look up state.
.
MessageId=13929
SymbolicName=ERROR_IPSEC_DOSP_MAX_ENTRIES
Language=ENU
IPsec DoS Protection failed to create state because the maximum number of entries allowed by policy has been reached.
.
MessageId=13930
SymbolicName=ERROR_IPSEC_DOSP_KEYMOD_NOT_ALLOWED
Language=ENU
IPsec DoS Protection received an IPsec negotiation packet for a keying module which is not allowed by policy.
.
MessageId=13931
SymbolicName=ERROR_IPSEC_DOSP_NOT_INSTALLED
Language=ENU
IPsec DoS Protection has not been enabled.
.
MessageId=13932
SymbolicName=ERROR_IPSEC_DOSP_MAX_PER_IP_RATELIMIT_QUEUES
Language=ENU
IPsec DoS Protection failed to create a per internal IP rate limit queue because the maximum number of queues allowed by policy has been reached.
.
MessageId=14000
SymbolicName=ERROR_SXS_SECTION_NOT_FOUND
Language=ENU
The requested section was not present in the activation context.
.
MessageId=14001
SymbolicName=ERROR_SXS_CANT_GEN_ACTCTX
Language=ENU
The application has failed to start because its side-by-side configuration is incorrect. Please see the application event log or use the command-line sxstrace.exe tool for more detail.
.
MessageId=14002
SymbolicName=ERROR_SXS_INVALID_ACTCTXDATA_FORMAT
Language=ENU
The application binding data format is invalid.
.
MessageId=14003
SymbolicName=ERROR_SXS_ASSEMBLY_NOT_FOUND
Language=ENU
The referenced assembly is not installed on your system.
.
MessageId=14004
SymbolicName=ERROR_SXS_MANIFEST_FORMAT_ERROR
Language=ENU
The manifest file does not begin with the required tag and format information.
.
MessageId=14005
SymbolicName=ERROR_SXS_MANIFEST_PARSE_ERROR
Language=ENU
The manifest file contains one or more syntax errors.
.
MessageId=14006
SymbolicName=ERROR_SXS_ACTIVATION_CONTEXT_DISABLED
Language=ENU
The application attempted to activate a disabled activation context.
.
MessageId=14007
SymbolicName=ERROR_SXS_KEY_NOT_FOUND
Language=ENU
The requested lookup key was not found in any active activation context.
.
MessageId=14008
SymbolicName=ERROR_SXS_VERSION_CONFLICT
Language=ENU
A component version required by the application conflicts with another component version already active.
.
MessageId=14009
SymbolicName=ERROR_SXS_WRONG_SECTION_TYPE
Language=ENU
The type requested activation context section does not match the query API used.
.
MessageId=14010
SymbolicName=ERROR_SXS_THREAD_QUERIES_DISABLED
Language=ENU
Lack of system resources has required isolated activation to be disabled for the current thread of execution.
.
MessageId=14011
SymbolicName=ERROR_SXS_PROCESS_DEFAULT_ALREADY_SET
Language=ENU
An attempt to set the process default activation context failed because the process default activation context was already set.
.
MessageId=14012
SymbolicName=ERROR_SXS_UNKNOWN_ENCODING_GROUP
Language=ENU
The encoding group identifier specified is not recognized.
.
MessageId=14013
SymbolicName=ERROR_SXS_UNKNOWN_ENCODING
Language=ENU
The encoding requested is not recognized.
.
MessageId=14014
SymbolicName=ERROR_SXS_INVALID_XML_NAMESPACE_URI
Language=ENU
The manifest contains a reference to an invalid URI.
.
MessageId=14015
SymbolicName=ERROR_SXS_ROOT_MANIFEST_DEPENDENCY_NOT_INSTALLED
Language=ENU
The application manifest contains a reference to a dependent assembly which is not installed
.
MessageId=14016
SymbolicName=ERROR_SXS_LEAF_MANIFEST_DEPENDENCY_NOT_INSTALLED
Language=ENU
The manifest for an assembly used by the application has a reference to a dependent assembly which is not installed
.
MessageId=14017
SymbolicName=ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE
Language=ENU
The manifest contains an attribute for the assembly identity which is not valid.
.
MessageId=14018
SymbolicName=ERROR_SXS_MANIFEST_MISSING_REQUIRED_DEFAULT_NAMESPACE
Language=ENU
The manifest is missing the required default namespace specification on the assembly element.
.
MessageId=14019
SymbolicName=ERROR_SXS_MANIFEST_INVALID_REQUIRED_DEFAULT_NAMESPACE
Language=ENU
The manifest has a default namespace specified on the assembly element but its value is not "urn:schemas-microsoft-com:asm.v1".
.
MessageId=14020
SymbolicName=ERROR_SXS_PRIVATE_MANIFEST_CROSS_PATH_WITH_REPARSE_POINT
Language=ENU
The private manifest probed has crossed a path with an unsupported reparse point.
.
MessageId=14021
SymbolicName=ERROR_SXS_DUPLICATE_DLL_NAME
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have files by the same name.
.
MessageId=14022
SymbolicName=ERROR_SXS_DUPLICATE_WINDOWCLASS_NAME
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have window classes with the same name.
.
MessageId=14023
SymbolicName=ERROR_SXS_DUPLICATE_CLSID
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have the same COM server CLSIDs.
.
MessageId=14024
SymbolicName=ERROR_SXS_DUPLICATE_IID
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have proxies for the same COM interface IIDs.
.
MessageId=14025
SymbolicName=ERROR_SXS_DUPLICATE_TLBID
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have the same COM type library TLBIDs.
.
MessageId=14026
SymbolicName=ERROR_SXS_DUPLICATE_PROGID
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have the same COM ProgIDs.
.
MessageId=14027
SymbolicName=ERROR_SXS_DUPLICATE_ASSEMBLY_NAME
Language=ENU
Two or more components referenced directly or indirectly by the application manifest are different versions of the same component which is not permitted.
.
MessageId=14028
SymbolicName=ERROR_SXS_FILE_HASH_MISMATCH
Language=ENU
A component's file does not match the verification information present in the component manifest.
.
MessageId=14029
SymbolicName=ERROR_SXS_POLICY_PARSE_ERROR
Language=ENU
The policy manifest contains one or more syntax errors.
.
MessageId=14030
SymbolicName=ERROR_SXS_XML_E_MISSINGQUOTE
Language=ENU
Manifest Parse Error : A string literal was expected, but no opening quote character was found.
.
MessageId=14031
SymbolicName=ERROR_SXS_XML_E_COMMENTSYNTAX
Language=ENU
Manifest Parse Error : Incorrect syntax was used in a comment.
.
MessageId=14032
SymbolicName=ERROR_SXS_XML_E_BADSTARTNAMECHAR
Language=ENU
Manifest Parse Error : A name was started with an invalid character.
.
MessageId=14033
SymbolicName=ERROR_SXS_XML_E_BADNAMECHAR
Language=ENU
Manifest Parse Error : A name contained an invalid character.
.
MessageId=14034
SymbolicName=ERROR_SXS_XML_E_BADCHARINSTRING
Language=ENU
Manifest Parse Error : A string literal contained an invalid character.
.
MessageId=14035
SymbolicName=ERROR_SXS_XML_E_XMLDECLSYNTAX
Language=ENU
Manifest Parse Error : Invalid syntax for an xml declaration.
.
MessageId=14036
SymbolicName=ERROR_SXS_XML_E_BADCHARDATA
Language=ENU
Manifest Parse Error : An Invalid character was found in text content.
.
MessageId=14037
SymbolicName=ERROR_SXS_XML_E_MISSINGWHITESPACE
Language=ENU
Manifest Parse Error : Required white space was missing.
.
MessageId=14038
SymbolicName=ERROR_SXS_XML_E_EXPECTINGTAGEND
Language=ENU
Manifest Parse Error : The character '>' was expected.
.
MessageId=14039
SymbolicName=ERROR_SXS_XML_E_MISSINGSEMICOLON
Language=ENU
Manifest Parse Error : A semi colon character was expected.
.
MessageId=14040
SymbolicName=ERROR_SXS_XML_E_UNBALANCEDPAREN
Language=ENU
Manifest Parse Error : Unbalanced parentheses.
.
MessageId=14041
SymbolicName=ERROR_SXS_XML_E_INTERNALERROR
Language=ENU
Manifest Parse Error : Internal error.
.
MessageId=14042
SymbolicName=ERROR_SXS_XML_E_UNEXPECTED_WHITESPACE
Language=ENU
Manifest Parse Error : Whitespace is not allowed at this location.
.
MessageId=14043
SymbolicName=ERROR_SXS_XML_E_INCOMPLETE_ENCODING
Language=ENU
Manifest Parse Error : End of file reached in invalid state for current encoding.
.
MessageId=14044
SymbolicName=ERROR_SXS_XML_E_MISSING_PAREN
Language=ENU
Manifest Parse Error : Missing parenthesis.
.
MessageId=14045
SymbolicName=ERROR_SXS_XML_E_EXPECTINGCLOSEQUOTE
Language=ENU
Manifest Parse Error : A single or double closing quote character (\' or \") is missing.
.
MessageId=14046
SymbolicName=ERROR_SXS_XML_E_MULTIPLE_COLONS
Language=ENU
Manifest Parse Error : Multiple colons are not allowed in a name.
.
MessageId=14047
SymbolicName=ERROR_SXS_XML_E_INVALID_DECIMAL
Language=ENU
Manifest Parse Error : Invalid character for decimal digit.
.
MessageId=14048
SymbolicName=ERROR_SXS_XML_E_INVALID_HEXIDECIMAL
Language=ENU
Manifest Parse Error : Invalid character for hexadecimal digit.
.
MessageId=14049
SymbolicName=ERROR_SXS_XML_E_INVALID_UNICODE
Language=ENU
Manifest Parse Error : Invalid unicode character value for this platform.
.
MessageId=14050
SymbolicName=ERROR_SXS_XML_E_WHITESPACEORQUESTIONMARK
Language=ENU
Manifest Parse Error : Expecting whitespace or '?'.
.
MessageId=14051
SymbolicName=ERROR_SXS_XML_E_UNEXPECTEDENDTAG
Language=ENU
Manifest Parse Error : End tag was not expected at this location.
.
MessageId=14052
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDTAG
Language=ENU
Manifest Parse Error : The following tags were not closed: %1.
.
MessageId=14053
SymbolicName=ERROR_SXS_XML_E_DUPLICATEATTRIBUTE
Language=ENU
Manifest Parse Error : Duplicate attribute.
.
MessageId=14054
SymbolicName=ERROR_SXS_XML_E_MULTIPLEROOTS
Language=ENU
Manifest Parse Error : Only one top level element is allowed in an XML document.
.
MessageId=14055
SymbolicName=ERROR_SXS_XML_E_INVALIDATROOTLEVEL
Language=ENU
Manifest Parse Error : Invalid at the top level of the document.
.
MessageId=14056
SymbolicName=ERROR_SXS_XML_E_BADXMLDECL
Language=ENU
Manifest Parse Error : Invalid xml declaration.
.
MessageId=14057
SymbolicName=ERROR_SXS_XML_E_MISSINGROOT
Language=ENU
Manifest Parse Error : XML document must have a top level element.
.
MessageId=14058
SymbolicName=ERROR_SXS_XML_E_UNEXPECTEDEOF
Language=ENU
Manifest Parse Error : Unexpected end of file.
.
MessageId=14059
SymbolicName=ERROR_SXS_XML_E_BADPEREFINSUBSET
Language=ENU
Manifest Parse Error : Parameter entities cannot be used inside markup declarations in an internal subset.
.
MessageId=14060
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDSTARTTAG
Language=ENU
Manifest Parse Error : Element was not closed.
.
MessageId=14061
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDENDTAG
Language=ENU
Manifest Parse Error : End element was missing the character '>'.
.
MessageId=14062
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDSTRING
Language=ENU
Manifest Parse Error : A string literal was not closed.
.
MessageId=14063
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDCOMMENT
Language=ENU
Manifest Parse Error : A comment was not closed.
.
MessageId=14064
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDDECL
Language=ENU
Manifest Parse Error : A declaration was not closed.
.
MessageId=14065
SymbolicName=ERROR_SXS_XML_E_UNCLOSEDCDATA
Language=ENU
Manifest Parse Error : A CDATA section was not closed.
.
MessageId=14066
SymbolicName=ERROR_SXS_XML_E_RESERVEDNAMESPACE
Language=ENU
Manifest Parse Error : The namespace prefix is not allowed to start with the reserved string "xml".
.
MessageId=14067
SymbolicName=ERROR_SXS_XML_E_INVALIDENCODING
Language=ENU
Manifest Parse Error : System does not support the specified encoding.
.
MessageId=14068
SymbolicName=ERROR_SXS_XML_E_INVALIDSWITCH
Language=ENU
Manifest Parse Error : Switch from current encoding to specified encoding not supported.
.
MessageId=14069
SymbolicName=ERROR_SXS_XML_E_BADXMLCASE
Language=ENU
Manifest Parse Error : The name 'xml' is reserved and must be lower case.
.
MessageId=14070
SymbolicName=ERROR_SXS_XML_E_INVALID_STANDALONE
Language=ENU
Manifest Parse Error : The standalone attribute must have the value 'yes' or 'no'.
.
MessageId=14071
SymbolicName=ERROR_SXS_XML_E_UNEXPECTED_STANDALONE
Language=ENU
Manifest Parse Error : The standalone attribute cannot be used in external entities.
.
MessageId=14072
SymbolicName=ERROR_SXS_XML_E_INVALID_VERSION
Language=ENU
Manifest Parse Error : Invalid version number.
.
MessageId=14073
SymbolicName=ERROR_SXS_XML_E_MISSINGEQUALS
Language=ENU
Manifest Parse Error : Missing equals sign between attribute and attribute value.
.
MessageId=14074
SymbolicName=ERROR_SXS_PROTECTION_RECOVERY_FAILED
Language=ENU
Assembly Protection Error : Unable to recover the specified assembly.
.
MessageId=14075
SymbolicName=ERROR_SXS_PROTECTION_PUBLIC_KEY_TOO_SHORT
Language=ENU
Assembly Protection Error : The public key for an assembly was too short to be allowed.
.
MessageId=14076
SymbolicName=ERROR_SXS_PROTECTION_CATALOG_NOT_VALID
Language=ENU
Assembly Protection Error : The catalog for an assembly is not valid, or does not match the assembly's manifest.
.
MessageId=14077
SymbolicName=ERROR_SXS_UNTRANSLATABLE_HRESULT
Language=ENU
An HRESULT could not be translated to a corresponding Win32 error code.
.
MessageId=14078
SymbolicName=ERROR_SXS_PROTECTION_CATALOG_FILE_MISSING
Language=ENU
Assembly Protection Error : The catalog for an assembly is missing.
.
MessageId=14079
SymbolicName=ERROR_SXS_MISSING_ASSEMBLY_IDENTITY_ATTRIBUTE
Language=ENU
The supplied assembly identity is missing one or more attributes which must be present in this context.
.
MessageId=14080
SymbolicName=ERROR_SXS_INVALID_ASSEMBLY_IDENTITY_ATTRIBUTE_NAME
Language=ENU
The supplied assembly identity has one or more attribute names that contain characters not permitted in XML names.
.
MessageId=14081
SymbolicName=ERROR_SXS_ASSEMBLY_MISSING
Language=ENU
The referenced assembly could not be found.
.
MessageId=14082
SymbolicName=ERROR_SXS_CORRUPT_ACTIVATION_STACK
Language=ENU
The activation context activation stack for the running thread of execution is corrupt.
.
MessageId=14083
SymbolicName=ERROR_SXS_CORRUPTION
Language=ENU
The application isolation metadata for this process or thread has become corrupt.
.
MessageId=14084
SymbolicName=ERROR_SXS_EARLY_DEACTIVATION
Language=ENU
The activation context being deactivated is not the most recently activated one.
.
MessageId=14085
SymbolicName=ERROR_SXS_INVALID_DEACTIVATION
Language=ENU
The activation context being deactivated is not active for the current thread of execution.
.
MessageId=14086
SymbolicName=ERROR_SXS_MULTIPLE_DEACTIVATION
Language=ENU
The activation context being deactivated has already been deactivated.
.
MessageId=14087
SymbolicName=ERROR_SXS_PROCESS_TERMINATION_REQUESTED
Language=ENU
A component used by the isolation facility has requested to terminate the process.
.
MessageId=14088
SymbolicName=ERROR_SXS_RELEASE_ACTIVATION_CONTEXT
Language=ENU
A kernel mode component is releasing a reference on an activation context.
.
MessageId=14089
SymbolicName=ERROR_SXS_SYSTEM_DEFAULT_ACTIVATION_CONTEXT_EMPTY
Language=ENU
The activation context of system default assembly could not be generated.
.
MessageId=14090
SymbolicName=ERROR_SXS_INVALID_IDENTITY_ATTRIBUTE_VALUE
Language=ENU
The value of an attribute in an identity is not within the legal range.
.
MessageId=14091
SymbolicName=ERROR_SXS_INVALID_IDENTITY_ATTRIBUTE_NAME
Language=ENU
The name of an attribute in an identity is not within the legal range.
.
MessageId=14092
SymbolicName=ERROR_SXS_IDENTITY_DUPLICATE_ATTRIBUTE
Language=ENU
An identity contains two definitions for the same attribute.
.
MessageId=14093
SymbolicName=ERROR_SXS_IDENTITY_PARSE_ERROR
Language=ENU
The identity string is malformed. This may be due to a trailing comma, more than two unnamed attributes, missing attribute name or missing attribute value.
.
MessageId=14094
SymbolicName=ERROR_MALFORMED_SUBSTITUTION_STRING
Language=ENU
A string containing localized substitutable content was malformed. Either a dollar sign ($) was followed by something other than a left parenthesis or another dollar sign or an substitution's right parenthesis was not found.
.
MessageId=14095
SymbolicName=ERROR_SXS_INCORRECT_PUBLIC_KEY_TOKEN
Language=ENU
The public key token does not correspond to the public key specified.
.
MessageId=14096
SymbolicName=ERROR_UNMAPPED_SUBSTITUTION_STRING
Language=ENU
A substitution string had no mapping.
.
MessageId=14097
SymbolicName=ERROR_SXS_ASSEMBLY_NOT_LOCKED
Language=ENU
The component must be locked before making the request.
.
MessageId=14098
SymbolicName=ERROR_SXS_COMPONENT_STORE_CORRUPT
Language=ENU
The component store has been corrupted.
.
MessageId=14099
SymbolicName=ERROR_ADVANCED_INSTALLER_FAILED
Language=ENU
An advanced installer failed during setup or servicing.
.
MessageId=14100
SymbolicName=ERROR_XML_ENCODING_MISMATCH
Language=ENU
The character encoding in the XML declaration did not match the encoding used in the document.
.
MessageId=14101
SymbolicName=ERROR_SXS_MANIFEST_IDENTITY_SAME_BUT_CONTENTS_DIFFERENT
Language=ENU
The identities of the manifests are identical but their contents are different.
.
MessageId=14102
SymbolicName=ERROR_SXS_IDENTITIES_DIFFERENT
Language=ENU
The component identities are different.
.
MessageId=14103
SymbolicName=ERROR_SXS_ASSEMBLY_IS_NOT_A_DEPLOYMENT
Language=ENU
The assembly is not a deployment.
.
MessageId=14104
SymbolicName=ERROR_SXS_FILE_NOT_PART_OF_ASSEMBLY
Language=ENU
The file is not a part of the assembly.
.
MessageId=14105
SymbolicName=ERROR_SXS_MANIFEST_TOO_BIG
Language=ENU
The size of the manifest exceeds the maximum allowed.
.
MessageId=14106
SymbolicName=ERROR_SXS_SETTING_NOT_REGISTERED
Language=ENU
The setting is not registered.
.
MessageId=14107
SymbolicName=ERROR_SXS_TRANSACTION_CLOSURE_INCOMPLETE
Language=ENU
One or more required members of the transaction are not present.
.
MessageId=14108
SymbolicName=ERROR_SMI_PRIMITIVE_INSTALLER_FAILED
Language=ENU
The SMI primitive installer failed during setup or servicing.
.
MessageId=14109
SymbolicName=ERROR_GENERIC_COMMAND_FAILED
Language=ENU
A generic command executable returned a result that indicates failure.
.
MessageId=14110
SymbolicName=ERROR_SXS_FILE_HASH_MISSING
Language=ENU
A component is missing file verification information in its manifest.
.
MessageId=14111
SymbolicName=ERROR_SXS_DUPLICATE_ACTIVATABLE_CLASS
Language=ENU
Two or more components referenced directly or indirectly by the application manifest have the same WinRT ActivatableClass IDs.
.
MessageId=15000
SymbolicName=ERROR_EVT_INVALID_CHANNEL_PATH
Language=ENU
The specified channel path is invalid.
.
MessageId=15001
SymbolicName=ERROR_EVT_INVALID_QUERY
Language=ENU
The specified query is invalid.
.
MessageId=15002
SymbolicName=ERROR_EVT_PUBLISHER_METADATA_NOT_FOUND
Language=ENU
The publisher metadata cannot be found in the resource.
.
MessageId=15003
SymbolicName=ERROR_EVT_EVENT_TEMPLATE_NOT_FOUND
Language=ENU
The template for an event definition cannot be found in the resource (error = %1).
.
MessageId=15004
SymbolicName=ERROR_EVT_INVALID_PUBLISHER_NAME
Language=ENU
The specified publisher name is invalid.
.
MessageId=15005
SymbolicName=ERROR_EVT_INVALID_EVENT_DATA
Language=ENU
The event data raised by the publisher is not compatible with the event template definition in the publisher's manifest.
.
MessageId=15007
SymbolicName=ERROR_EVT_CHANNEL_NOT_FOUND
Language=ENU
The specified channel could not be found.
.
MessageId=15008
SymbolicName=ERROR_EVT_MALFORMED_XML_TEXT
Language=ENU
The specified XML text was not well-formed. See Extended Error for more details.
.
MessageId=15009
SymbolicName=ERROR_EVT_SUBSCRIPTION_TO_DIRECT_CHANNEL
Language=ENU
The events for a direct channel go directly to a log file and cannot be subscribed to.
.
MessageId=15010
SymbolicName=ERROR_EVT_CONFIGURATION_ERROR
Language=ENU
Configuration error.
.
MessageId=15011
SymbolicName=ERROR_EVT_QUERY_RESULT_STALE
Language=ENU
The query result is stale or invalid and must be recreated. This may be due to the log being cleared or rolling over after the query result was created.
.
MessageId=15012
SymbolicName=ERROR_EVT_QUERY_RESULT_INVALID_POSITION
Language=ENU
The query result is currently at an invalid position.
.
MessageId=15013
SymbolicName=ERROR_EVT_NON_VALIDATING_MSXML
Language=ENU
Registered MSXML doesn't support validation.
.
MessageId=15014
SymbolicName=ERROR_EVT_FILTER_ALREADYSCOPED
Language=ENU
An expression can only be followed by a change-of-scope operation if the expression evaluates to a node set and is not already part of another change-of-scope operation.
.
MessageId=15015
SymbolicName=ERROR_EVT_FILTER_NOTELTSET
Language=ENU
Cannot perform a step operation from a term that does not represent an element set.
.
MessageId=15016
SymbolicName=ERROR_EVT_FILTER_INVARG
Language=ENU
Left-hand side arguments to binary operators must be either attributes, nodes or variables. Right-hand side arguments must be constants.
.
MessageId=15017
SymbolicName=ERROR_EVT_FILTER_INVTEST
Language=ENU
A step operation must involve a node test or, in the case of a predicate, an algebraic expression against which to test each node in the preceeding node set.
.
MessageId=15018
SymbolicName=ERROR_EVT_FILTER_INVTYPE
Language=ENU
This data type is currently unsupported.
.
MessageId=15019
SymbolicName=ERROR_EVT_FILTER_PARSEERR
Language=ENU
A syntax error occurred at position %1!d!
.
MessageId=15020
SymbolicName=ERROR_EVT_FILTER_UNSUPPORTEDOP
Language=ENU
This operator is unsupported by this implementation of the filter.
.
MessageId=15021
SymbolicName=ERROR_EVT_FILTER_UNEXPECTEDTOKEN
Language=ENU
An unexpected token was encountered.
.
MessageId=15022
SymbolicName=ERROR_EVT_INVALID_OPERATION_OVER_ENABLED_DIRECT_CHANNEL
Language=ENU
The requested operation cannot be performed over an enabled direct channel. The channel must first be disabled.
.
MessageId=15023
SymbolicName=ERROR_EVT_INVALID_CHANNEL_PROPERTY_VALUE
Language=ENU
Channel property %1!s! contains an invalid value. The value has an invalid type, is outside of its valid range, cannot be changed, or is not supported by this type of channel.
.
MessageId=15024
SymbolicName=ERROR_EVT_INVALID_PUBLISHER_PROPERTY_VALUE
Language=ENU
Publisher property %1!s! contains an invalid value. The value has an invalid type, is outside of its valid range, cannot be changed, or is not supported by this type of publisher.
.
MessageId=15025
SymbolicName=ERROR_EVT_CHANNEL_CANNOT_ACTIVATE
Language=ENU
The channel failed to activate.
.
MessageId=15026
SymbolicName=ERROR_EVT_FILTER_TOO_COMPLEX
Language=ENU
The XPath expression exceeded the supported complexity. Simplify the expression or split it into multiple expressions.
.
MessageId=15027
SymbolicName=ERROR_EVT_MESSAGE_NOT_FOUND
Language=ENU
The message resource is present but the message was not found in the message table.
.
MessageId=15028
SymbolicName=ERROR_EVT_MESSAGE_ID_NOT_FOUND
Language=ENU
The message ID for the desired message could not be found.
.
MessageId=15029
SymbolicName=ERROR_EVT_UNRESOLVED_VALUE_INSERT
Language=ENU
The substitution string for insert index (%1) could not be found.
.
MessageId=15030
SymbolicName=ERROR_EVT_UNRESOLVED_PARAMETER_INSERT
Language=ENU
The description string for parameter reference (%1) could not be found.
.
MessageId=15031
SymbolicName=ERROR_EVT_MAX_INSERTS_REACHED
Language=ENU
The maximum number of replacements has been reached.
.
MessageId=15032
SymbolicName=ERROR_EVT_EVENT_DEFINITION_NOT_FOUND
Language=ENU
The event definition could not be found for event ID (%1).
.
MessageId=15033
SymbolicName=ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND
Language=ENU
The locale specific resource for the desired message is not present.
.
MessageId=15034
SymbolicName=ERROR_EVT_VERSION_TOO_OLD
Language=ENU
The resource is too old and is not supported.
.
MessageId=15035
SymbolicName=ERROR_EVT_VERSION_TOO_NEW
Language=ENU
The resource is too new and is not supported.
.
MessageId=15036
SymbolicName=ERROR_EVT_CANNOT_OPEN_CHANNEL_OF_QUERY
Language=ENU
The channel at index %1!d! of the query can't be opened.
.
MessageId=15037
SymbolicName=ERROR_EVT_PUBLISHER_DISABLED
Language=ENU
The publisher has been disabled and its resource is not available. This usually occurs when the publisher is in the process of being uninstalled or upgraded.
.
MessageId=15038
SymbolicName=ERROR_EVT_FILTER_OUT_OF_RANGE
Language=ENU
Attempted to create a numeric type that is outside of its valid range.
.
MessageId=15080
SymbolicName=ERROR_EC_SUBSCRIPTION_CANNOT_ACTIVATE
Language=ENU
The subscription fails to activate.
.
MessageId=15081
SymbolicName=ERROR_EC_LOG_DISABLED
Language=ENU
The log of the subscription is in disabled state, and can not be used to forward events to. The log must first be enabled before the subscription can be activated.
.
MessageId=15082
SymbolicName=ERROR_EC_CIRCULAR_FORWARDING
Language=ENU
When forwarding events from local machine to itself, the query of the subscription can't contain target log of the subscription.
.
MessageId=15083
SymbolicName=ERROR_EC_CREDSTORE_FULL
Language=ENU
The credential store that is used to save credentials is full.
.
MessageId=15084
SymbolicName=ERROR_EC_CRED_NOT_FOUND
Language=ENU
The credential used by this subscription can't be found in credential store.
.
MessageId=15085
SymbolicName=ERROR_EC_NO_ACTIVE_CHANNEL
Language=ENU
No active channel is found for the query.
.
MessageId=15100
SymbolicName=ERROR_MUI_FILE_NOT_FOUND
Language=ENU
The resource loader failed to find MUI file.
.
MessageId=15101
SymbolicName=ERROR_MUI_INVALID_FILE
Language=ENU
The resource loader failed to load MUI file because the file fail to pass validation.
.
MessageId=15102
SymbolicName=ERROR_MUI_INVALID_RC_CONFIG
Language=ENU
The RC Manifest is corrupted with garbage data or unsupported version or missing required item.
.
MessageId=15103
SymbolicName=ERROR_MUI_INVALID_LOCALE_NAME
Language=ENU
The RC Manifest has invalid culture name.
.
MessageId=15104
SymbolicName=ERROR_MUI_INVALID_ULTIMATEFALLBACK_NAME
Language=ENU
The RC Manifest has invalid ultimatefallback name.
.
MessageId=15105
SymbolicName=ERROR_MUI_FILE_NOT_LOADED
Language=ENU
The resource loader cache doesn't have loaded MUI entry.
.
MessageId=15106
SymbolicName=ERROR_RESOURCE_ENUM_USER_STOP
Language=ENU
User stopped resource enumeration.
.
MessageId=15107
SymbolicName=ERROR_MUI_INTLSETTINGS_UILANG_NOT_INSTALLED
Language=ENU
UI language installation failed.
.
MessageId=15108
SymbolicName=ERROR_MUI_INTLSETTINGS_INVALID_LOCALE_NAME
Language=ENU
Locale installation failed.
.
MessageId=15110
SymbolicName=ERROR_MRM_RUNTIME_NO_DEFAULT_OR_NEUTRAL_RESOURCE
Language=ENU
A resource does not have default or neutral value.
.
MessageId=15111
SymbolicName=ERROR_MRM_INVALID_PRICONFIG
Language=ENU
Invalid PRI config file.
.
MessageId=15112
SymbolicName=ERROR_MRM_INVALID_FILE_TYPE
Language=ENU
Invalid file type.
.
MessageId=15113
SymbolicName=ERROR_MRM_UNKNOWN_QUALIFIER
Language=ENU
Unknown qualifier.
.
MessageId=15114
SymbolicName=ERROR_MRM_INVALID_QUALIFIER_VALUE
Language=ENU
Invalid qualifier value.
.
MessageId=15115
SymbolicName=ERROR_MRM_NO_CANDIDATE
Language=ENU
No Candidate found.
.
MessageId=15116
SymbolicName=ERROR_MRM_NO_MATCH_OR_DEFAULT_CANDIDATE
Language=ENU
The ResourceMap or NamedResource has an item that does not have default or neutral resource..
.
MessageId=15117
SymbolicName=ERROR_MRM_RESOURCE_TYPE_MISMATCH
Language=ENU
Invalid ResourceCandidate type.
.
MessageId=15118
SymbolicName=ERROR_MRM_DUPLICATE_MAP_NAME
Language=ENU
Duplicate Resource Map.
.
MessageId=15119
SymbolicName=ERROR_MRM_DUPLICATE_ENTRY
Language=ENU
Duplicate Entry.
.
MessageId=15120
SymbolicName=ERROR_MRM_INVALID_RESOURCE_IDENTIFIER
Language=ENU
Invalid Resource Identifier.
.
MessageId=15121
SymbolicName=ERROR_MRM_FILEPATH_TOO_LONG
Language=ENU
Filepath too long.
.
MessageId=15122
SymbolicName=ERROR_MRM_UNSUPPORTED_DIRECTORY_TYPE
Language=ENU
Unsupported directory type.
.
MessageId=15126
SymbolicName=ERROR_MRM_INVALID_PRI_FILE
Language=ENU
Invalid PRI File.
.
MessageId=15127
SymbolicName=ERROR_MRM_NAMED_RESOURCE_NOT_FOUND
Language=ENU
NamedResource Not Found.
.
MessageId=15135
SymbolicName=ERROR_MRM_MAP_NOT_FOUND
Language=ENU
ResourceMap Not Found.
.
MessageId=15136
SymbolicName=ERROR_MRM_UNSUPPORTED_PROFILE_TYPE
Language=ENU
Unsupported MRT profile type.
.
MessageId=15137
SymbolicName=ERROR_MRM_INVALID_QUALIFIER_OPERATOR
Language=ENU
Invalid qualifier operator.
.
MessageId=15138
SymbolicName=ERROR_MRM_INDETERMINATE_QUALIFIER_VALUE
Language=ENU
Unable to determine qualifier value or qualifier value has not been set.
.
MessageId=15139
SymbolicName=ERROR_MRM_AUTOMERGE_ENABLED
Language=ENU
Automerge is enabled in the PRI file.
.
MessageId=15140
SymbolicName=ERROR_MRM_TOO_MANY_RESOURCES
Language=ENU
Too many resources defined for package.
.
MessageId=15141
SymbolicName=ERROR_MRM_UNSUPPORTED_FILE_TYPE_FOR_MERGE
Language=ENU
Resource File can not be used for merge operation.
.
MessageId=15142
SymbolicName=ERROR_MRM_UNSUPPORTED_FILE_TYPE_FOR_LOAD_UNLOAD_PRI_FILE
Language=ENU
Load/UnloadPriFiles cannot be used with resource packages.
.
MessageId=15143
SymbolicName=ERROR_MRM_NO_CURRENT_VIEW_ON_THREAD
Language=ENU
Resource Contexts may not be created on threads that do not have a CoreWindow.
.
MessageId=15144
SymbolicName=ERROR_DIFFERENT_PROFILE_RESOURCE_MANAGER_EXIST
Language=ENU
The singleton Resource Manager with different profile is already created.
.
MessageId=15145
SymbolicName=ERROR_OPERATION_NOT_ALLOWED_FROM_SYSTEM_COMPONENT
Language=ENU
The system component cannot operate given API operation
.
MessageId=15146
SymbolicName=ERROR_MRM_DIRECT_REF_TO_NON_DEFAULT_RESOURCE
Language=ENU
The resource is a direct reference to a non-default resource candidate.
.
MessageId=15147
SymbolicName=ERROR_MRM_GENERATION_COUNT_MISMATCH
Language=ENU
Resource Map has been re-generated and the query string is not valid anymore.
.
MessageId=15148
SymbolicName=ERROR_PRI_MERGE_VERSION_MISMATCH
Language=ENU
The PRI files to be merged have incompatible versions.
.
MessageId=15149
SymbolicName=ERROR_PRI_MERGE_MISSING_SCHEMA
Language=ENU
The primary PRI files to be merged does not contain a schema.
.
MessageId=15150
SymbolicName=ERROR_PRI_MERGE_LOAD_FILE_FAILED
Language=ENU
Unable to load one of the PRI files to be merged.
.
MessageId=15151
SymbolicName=ERROR_PRI_MERGE_ADD_FILE_FAILED
Language=ENU
Unable to add one of the PRI files to the merged file.
.
MessageId=15152
SymbolicName=ERROR_PRI_MERGE_WRITE_FILE_FAILED
Language=ENU
Unable to create the merged PRI file.
.
MessageId=15153
SymbolicName=ERROR_PRI_MERGE_MULTIPLE_PACKAGE_FAMILIES_NOT_ALLOWED
Language=ENU
Packages for a PRI file merge must all be from the same package family.
.
MessageId=15154
SymbolicName=ERROR_PRI_MERGE_MULTIPLE_MAIN_PACKAGES_NOT_ALLOWED
Language=ENU
Packages for a PRI file merge must not include multiple main packages.
.
MessageId=15155
SymbolicName=ERROR_PRI_MERGE_BUNDLE_PACKAGES_NOT_ALLOWED
Language=ENU
Packages for a PRI file merge must not include bundle packages.
.
MessageId=15156
SymbolicName=ERROR_PRI_MERGE_MAIN_PACKAGE_REQUIRED
Language=ENU
Packages for a PRI file merge must include one main package.
.
MessageId=15157
SymbolicName=ERROR_PRI_MERGE_RESOURCE_PACKAGE_REQUIRED
Language=ENU
Packages for a PRI file merge must include at least one resource package.
.
MessageId=15158
SymbolicName=ERROR_PRI_MERGE_INVALID_FILE_NAME
Language=ENU
Invalid name supplied for a canonical merged PRI file.
.
MessageId=15159
SymbolicName=ERROR_MRM_PACKAGE_NOT_FOUND
Language=ENU
Unable to find the specified package.
.
MessageId=15160
SymbolicName=ERROR_MRM_MISSING_DEFAULT_LANGUAGE
Language=ENU
No default value for language was specified.
.
MessageId=15161
SymbolicName=ERROR_MRM_SCOPE_ITEM_CONFLICT
Language=ENU
An entity was defined as both resource and scope, which is not allowed.
.
MessageId=15200
SymbolicName=ERROR_MCA_INVALID_CAPABILITIES_STRING
Language=ENU
The monitor returned a DDC/CI capabilities string that did not comply with the ACCESS.bus 3.0, DDC/CI 1.1 or MCCS 2 Revision 1 specification.
.
MessageId=15201
SymbolicName=ERROR_MCA_INVALID_VCP_VERSION
Language=ENU
The monitor's VCP Version (0xDF) VCP code returned an invalid version value.
.
MessageId=15202
SymbolicName=ERROR_MCA_MONITOR_VIOLATES_MCCS_SPECIFICATION
Language=ENU
The monitor does not comply with the MCCS specification it claims to support.
.
MessageId=15203
SymbolicName=ERROR_MCA_MCCS_VERSION_MISMATCH
Language=ENU
The MCCS version in a monitor's mccs_ver capability does not match the MCCS version the monitor reports when the VCP Version (0xDF) VCP code is used.
.
MessageId=15204
SymbolicName=ERROR_MCA_UNSUPPORTED_MCCS_VERSION
Language=ENU
The Monitor Configuration API only works with monitors that support the MCCS 1.0 specification, MCCS 2.0 specification or the MCCS 2.0 Revision 1 specification.
.
MessageId=15205
SymbolicName=ERROR_MCA_INTERNAL_ERROR
Language=ENU
An internal Monitor Configuration API error occurred.
.
MessageId=15206
SymbolicName=ERROR_MCA_INVALID_TECHNOLOGY_TYPE_RETURNED
Language=ENU
The monitor returned an invalid monitor technology type. CRT, Plasma and LCD (TFT) are examples of monitor technology types. This error implies that the monitor violated the MCCS 2.0 or MCCS 2.0 Revision 1 specification.
.
MessageId=15207
SymbolicName=ERROR_MCA_UNSUPPORTED_COLOR_TEMPERATURE
Language=ENU
The caller of SetMonitorColorTemperature specified a color temperature that the current monitor did not support. This error implies that the monitor violated the MCCS 2.0 or MCCS 2.0 Revision 1 specification.
.
MessageId=15250
SymbolicName=ERROR_AMBIGUOUS_SYSTEM_DEVICE
Language=ENU
The requested system device cannot be identified due to multiple indistinguishable devices potentially matching the identification criteria.
.
MessageId=15299
SymbolicName=ERROR_SYSTEM_DEVICE_NOT_FOUND
Language=ENU
The requested system device cannot be found.
.
MessageId=15300
SymbolicName=ERROR_HASH_NOT_SUPPORTED
Language=ENU
Hash generation for the specified hash version and hash type is not enabled on the server.
.
MessageId=15301
SymbolicName=ERROR_HASH_NOT_PRESENT
Language=ENU
The hash requested from the server is not available or no longer valid.
.
MessageId=15321
SymbolicName=ERROR_SECONDARY_IC_PROVIDER_NOT_REGISTERED
Language=ENU
The secondary interrupt controller instance that manages the specified interrupt is not registered.
.
MessageId=15322
SymbolicName=ERROR_GPIO_CLIENT_INFORMATION_INVALID
Language=ENU
The information supplied by the GPIO client driver is invalid.
.
MessageId=15323
SymbolicName=ERROR_GPIO_VERSION_NOT_SUPPORTED
Language=ENU
The version specified by the GPIO client driver is not supported.
.
MessageId=15324
SymbolicName=ERROR_GPIO_INVALID_REGISTRATION_PACKET
Language=ENU
The registration packet supplied by the GPIO client driver is not valid.
.
MessageId=15325
SymbolicName=ERROR_GPIO_OPERATION_DENIED
Language=ENU
The requested operation is not supported for the specified handle.
.
MessageId=15326
SymbolicName=ERROR_GPIO_INCOMPATIBLE_CONNECT_MODE
Language=ENU
The requested connect mode conflicts with an existing mode on one or more of the specified pins.
.
MessageId=15327
SymbolicName=ERROR_GPIO_INTERRUPT_ALREADY_UNMASKED
Language=ENU
The interrupt requested to be unmasked is not masked.
.
MessageId=15380
SymbolicName=WINERR_15380
Language=ENU
An API Set schema extension failed to compose.
.
MessageId=15381
SymbolicName=WINERR_15381
Language=ENU
The API Set schema version is not supported.
.
MessageId=15400
SymbolicName=ERROR_CANNOT_SWITCH_RUNLEVEL
Language=ENU
The requested run level switch cannot be completed successfully.
.
MessageId=15401
SymbolicName=ERROR_INVALID_RUNLEVEL_SETTING
Language=ENU
The service has an invalid run level setting. The run level for a service
.
MessageId=15402
SymbolicName=ERROR_RUNLEVEL_SWITCH_TIMEOUT
Language=ENU
The requested run level switch cannot be completed successfully since
.
MessageId=15403
SymbolicName=ERROR_RUNLEVEL_SWITCH_AGENT_TIMEOUT
Language=ENU
A run level switch agent did not respond within the specified timeout.
.
MessageId=15404
SymbolicName=ERROR_RUNLEVEL_SWITCH_IN_PROGRESS
Language=ENU
A run level switch is currently in progress.
.
MessageId=15405
SymbolicName=ERROR_SERVICES_FAILED_AUTOSTART
Language=ENU
One or more services failed to start during the service startup phase of a run level switch.
.
MessageId=15501
SymbolicName=ERROR_COM_TASK_STOP_PENDING
Language=ENU
The task stop request cannot be completed immediately since
.
MessageId=15600
SymbolicName=ERROR_INSTALL_OPEN_PACKAGE_FAILED
Language=ENU
Package could not be opened.
.
MessageId=15601
SymbolicName=ERROR_INSTALL_PACKAGE_NOT_FOUND
Language=ENU
Package was not found.
.
MessageId=15602
SymbolicName=ERROR_INSTALL_INVALID_PACKAGE
Language=ENU
Package data is invalid.
.
MessageId=15603
SymbolicName=ERROR_INSTALL_RESOLVE_DEPENDENCY_FAILED
Language=ENU
Package failed updates, dependency or conflict validation.
.
MessageId=15604
SymbolicName=ERROR_INSTALL_OUT_OF_DISK_SPACE
Language=ENU
There is not enough disk space on your computer. Please free up some space and try again.
.
MessageId=15605
SymbolicName=ERROR_INSTALL_NETWORK_FAILURE
Language=ENU
There was a problem downloading your product.
.
MessageId=15606
SymbolicName=ERROR_INSTALL_REGISTRATION_FAILURE
Language=ENU
Package could not be registered.
.
MessageId=15607
SymbolicName=ERROR_INSTALL_DEREGISTRATION_FAILURE
Language=ENU
Package could not be unregistered.
.
MessageId=15608
SymbolicName=ERROR_INSTALL_CANCEL
Language=ENU
User cancelled the install request.
.
MessageId=15609
SymbolicName=ERROR_INSTALL_FAILED
Language=ENU
Install failed. Please contact your software vendor.
.
MessageId=15610
SymbolicName=ERROR_REMOVE_FAILED
Language=ENU
Removal failed. Please contact your software vendor.
.
MessageId=15611
SymbolicName=ERROR_PACKAGE_ALREADY_EXISTS
Language=ENU
The provided package is already installed, and reinstallation of the package was blocked. Check the AppXDeployment-Server event log for details.
.
MessageId=15612
SymbolicName=ERROR_NEEDS_REMEDIATION
Language=ENU
The application cannot be started. Try reinstalling the application to fix the problem.
.
MessageId=15613
SymbolicName=ERROR_INSTALL_PREREQUISITE_FAILED
Language=ENU
A Prerequisite for an install could not be satisfied.
.
MessageId=15614
SymbolicName=ERROR_PACKAGE_REPOSITORY_CORRUPTED
Language=ENU
The package repository is corrupted.
.
MessageId=15615
SymbolicName=ERROR_INSTALL_POLICY_FAILURE
Language=ENU
To install this application you need either a Windows developer license or a sideloading-enabled system.
.
MessageId=15616
SymbolicName=ERROR_PACKAGE_UPDATING
Language=ENU
The application cannot be started because it is currently updating.
.
MessageId=15617
SymbolicName=ERROR_DEPLOYMENT_BLOCKED_BY_POLICY
Language=ENU
The package deployment operation is blocked by policy. Please contact your system administrator.
.
MessageId=15618
SymbolicName=ERROR_PACKAGES_IN_USE
Language=ENU
The package could not be installed because resources it modifies are currently in use.
.
MessageId=15619
SymbolicName=ERROR_RECOVERY_FILE_CORRUPT
Language=ENU
The package could not be recovered because necessary data for recovery have been corrupted.
.
MessageId=15620
SymbolicName=ERROR_INVALID_STAGED_SIGNATURE
Language=ENU
The signature is invalid. To register in developer mode, AppxSignature.p7x and AppxBlockMap.xml must be valid or should not be present.
.
MessageId=15621
SymbolicName=ERROR_DELETING_EXISTING_APPLICATIONDATA_STORE_FAILED
Language=ENU
An error occurred while deleting the package's previously existing application data.
.
MessageId=15622
SymbolicName=ERROR_INSTALL_PACKAGE_DOWNGRADE
Language=ENU
The package could not be installed because a higher version of this package is already installed.
.
MessageId=15623
SymbolicName=ERROR_SYSTEM_NEEDS_REMEDIATION
Language=ENU
An error in a system binary was detected. Try refreshing the PC to fix the problem.
.
MessageId=15624
SymbolicName=ERROR_APPX_INTEGRITY_FAILURE_CLR_NGEN
Language=ENU
A corrupted CLR NGEN binary was detected on the system.
.
MessageId=15625
SymbolicName=ERROR_RESILIENCY_FILE_CORRUPT
Language=ENU
The operation could not be resumed because necessary data for recovery have been corrupted.
.
MessageId=15626
SymbolicName=ERROR_INSTALL_FIREWALL_SERVICE_NOT_RUNNING
Language=ENU
The package could not be installed because the Windows Firewall service is not running. Enable the Windows Firewall service and try again.
.
MessageId=15627
SymbolicName=ERROR_PACKAGE_MOVE_FAILED
Language=ENU
Package move failed.
.
MessageId=15628
SymbolicName=ERROR_INSTALL_VOLUME_NOT_EMPTY
Language=ENU
The deployment operation failed because the volume is not empty.
.
MessageId=15629
SymbolicName=ERROR_INSTALL_VOLUME_OFFLINE
Language=ENU
The deployment operation failed because the volume is offline. For a package update, the volume refers to the installed volume of all package versions.
.
MessageId=15630
SymbolicName=ERROR_INSTALL_VOLUME_CORRUPT
Language=ENU
The deployment operation failed because the specified volume is corrupt.
.
MessageId=15631
SymbolicName=ERROR_NEEDS_REGISTRATION
Language=ENU
The deployment operation failed because the specified application needs to be registered first.
.
MessageId=15632
SymbolicName=ERROR_INSTALL_WRONG_PROCESSOR_ARCHITECTURE
Language=ENU
The deployment operation failed because the package targets the wrong processor architecture.
.
MessageId=15633
SymbolicName=ERROR_DEV_SIDELOAD_LIMIT_EXCEEDED
Language=ENU
You have reached the maximum number of developer sideloaded packages allowed on this device. Please uninstall a sideloaded package and try again.
.
MessageId=15634
SymbolicName=ERROR_INSTALL_OPTIONAL_PACKAGE_REQUIRES_MAIN_PACKAGE
Language=ENU
A main app package is required to install this optional package.  Install the main package first and try again.
.
MessageId=15635
SymbolicName=ERROR_PACKAGE_NOT_SUPPORTED_ON_FILESYSTEM
Language=ENU
This app package type is not supported on this filesystem
.
MessageId=15636
SymbolicName=ERROR_PACKAGE_MOVE_BLOCKED_BY_STREAMING
Language=ENU
Package move operation is blocked until the application has finished streaming
.
MessageId=15637
SymbolicName=ERROR_INSTALL_OPTIONAL_PACKAGE_APPLICATIONID_NOT_UNIQUE
Language=ENU
A main or another optional app package has the same application ID as this optional package.  Change the application ID for the optional package to avoid conflicts.
.
MessageId=15638
SymbolicName=ERROR_PACKAGE_STAGING_ONHOLD
Language=ENU
This staging session has been held to allow another staging operation to be prioritized.
.
MessageId=15639
SymbolicName=ERROR_INSTALL_INVALID_RELATED_SET_UPDATE
Language=ENU
A related set cannot be updated because the updated set is invalid. All packages in the related set must be updated at the same time.
.
MessageId=15640
SymbolicName=ERROR_INSTALL_OPTIONAL_PACKAGE_REQUIRES_MAIN_PACKAGE_FULLTRUST_CAPABILITY
Language=ENU
An optional package with a FullTrust entry point requires the main package to have the runFullTrust capability.
.
MessageId=15641
SymbolicName=ERROR_DEPLOYMENT_BLOCKED_BY_USER_LOG_OFF
Language=ENU
An error occurred because a user was logged off.
.
MessageId=15642
SymbolicName=ERROR_PROVISION_OPTIONAL_PACKAGE_REQUIRES_MAIN_PACKAGE_PROVISIONED
Language=ENU
An optional package provision requires the dependency main package to also be provisioned.
.
MessageId=15643
SymbolicName=ERROR_PACKAGES_REPUTATION_CHECK_FAILED
Language=ENU
The packages failed the SmartScreen reputation check.
.
MessageId=15644
SymbolicName=ERROR_PACKAGES_REPUTATION_CHECK_TIMEDOUT
Language=ENU
The SmartScreen reputation check operation timed out.
.
MessageId=15645
SymbolicName=ERROR_DEPLOYMENT_OPTION_NOT_SUPPORTED
Language=ENU
The current deployment option is not supported.
.
MessageId=15646
SymbolicName=ERROR_APPINSTALLER_ACTIVATION_BLOCKED
Language=ENU
Activation is blocked due to the .appinstaller update settings for this app.
.
MessageId=15647
SymbolicName=ERROR_REGISTRATION_FROM_REMOTE_DRIVE_NOT_SUPPORTED
Language=ENU
Remote drives are not supported; use \\server\share to register a remote package.
.
MessageId=15648
SymbolicName=ERROR_APPX_RAW_DATA_WRITE_FAILED
Language=ENU
Failed to process and write downloaded APPX package data to disk.
.
MessageId=15649
SymbolicName=ERROR_DEPLOYMENT_BLOCKED_BY_VOLUME_POLICY_PACKAGE
Language=ENU
The deployment operation was blocked due to a per-package-family policy restricting deployments on a non-system volume. Per policy, this app must be installed to the system drive, but that's not set as the default. In Storage Settings, make the system drive the default location to save new content, then retry the install.
.
MessageId=15650
SymbolicName=ERROR_DEPLOYMENT_BLOCKED_BY_VOLUME_POLICY_MACHINE
Language=ENU
The deployment operation was blocked due to a machine-wide policy restricting deployments on a non-system volume. Per policy, this app must be installed to the system drive, but that's not set as the default. In Storage Settings, make the system drive the default location to save new content, then retry the install.
.
MessageId=15651
SymbolicName=ERROR_DEPLOYMENT_BLOCKED_BY_PROFILE_POLICY
Language=ENU
The deployment operation was blocked because Special profile deployment is not allowed. Please try logging into an account that is not a Special profile. You can try logging out and logging back into the current account, or try logging into a different account.
.
MessageId=15652
SymbolicName=ERROR_DEPLOYMENT_FAILED_CONFLICTING_MUTABLE_PACKAGE_DIRECTORY
Language=ENU
The deployment operation failed due to a conflicting package's mutable package directory. To install this package remove the existing package with the conflicting mutable package directory.
.
MessageId=15653
SymbolicName=ERROR_SINGLETON_RESOURCE_INSTALLED_IN_ACTIVE_USER
Language=ENU
The package installation failed because a singleton resource was specified and another user with that package installed is logged in. Please make sure that all active users with the package installed are logged out and retry installation.
.
MessageId=15654
SymbolicName=ERROR_DIFFERENT_VERSION_OF_PACKAGED_SERVICE_INSTALLED
Language=ENU
The package installation failed because a different version of the service is installed. Try installing a newer version of the package.
.
MessageId=15655
SymbolicName=ERROR_SERVICE_EXISTS_AS_NON_PACKAGED_SERVICE
Language=ENU
The package installation failed because a version of the service exists outside of APPX packaging. Please contact your software vendor.
.
MessageId=15656
SymbolicName=ERROR_PACKAGED_SERVICE_REQUIRES_ADMIN_PRIVILEGES
Language=ENU
The package installation failed because administrator privileges are required. Please contact an administrator to install this package.
.
MessageId=15657
SymbolicName=ERROR_REDIRECTION_TO_DEFAULT_ACCOUNT_NOT_ALLOWED
Language=ENU
The package deployment failed because the operation would have redirected to default account, when the caller said not to do so.
.
MessageId=15658
SymbolicName=ERROR_PACKAGE_LACKS_CAPABILITY_TO_DEPLOY_ON_HOST
Language=ENU
The package deployment failed because the package requires a capability to natively target this host.
.
MessageId=15659
SymbolicName=ERROR_UNSIGNED_PACKAGE_INVALID_CONTENT
Language=ENU
The package deployment failed because its content is not valid for an unsigned package.
.
MessageId=15660
SymbolicName=ERROR_UNSIGNED_PACKAGE_INVALID_PUBLISHER_NAMESPACE
Language=ENU
The package deployment failed because its publisher is not in the unsigned namespace.
.
MessageId=15661
SymbolicName=ERROR_SIGNED_PACKAGE_INVALID_PUBLISHER_NAMESPACE
Language=ENU
The package deployment failed because its publisher is not in the signed namespace.
.
MessageId=15662
SymbolicName=ERROR_PACKAGE_EXTERNAL_LOCATION_NOT_ALLOWED
Language=ENU
The package deployment failed because it must allow external content to be deployed with an external location.
.
MessageId=15663
SymbolicName=ERROR_INSTALL_FULLTRUST_HOSTRUNTIME_REQUIRES_MAIN_PACKAGE_FULLTRUST_CAPABILITY
Language=ENU
A host runtime dependency resolving to a package with full trust content requires the main package to have the runFullTrust capability.
.
MessageId=15664
SymbolicName=ERROR_PACKAGE_LACKS_CAPABILITY_FOR_MANDATORY_STARTUPTASKS
Language=ENU
The package deployment failed because the package requires a capability for mandatory startup tasks.
.
MessageId=15665
SymbolicName=ERROR_INSTALL_RESOLVE_HOSTRUNTIME_DEPENDENCY_FAILED
Language=ENU
Package failed host runtime dependency or conflict validation.
.
MessageId=15666
SymbolicName=ERROR_MACHINE_SCOPE_NOT_ALLOWED
Language=ENU
The package deployment failed because it uses a machine scope but doesn't have the required capability.
.
MessageId=15667
SymbolicName=ERROR_CLASSIC_COMPAT_MODE_NOT_ALLOWED
Language=ENU
The package deployment failed because it uses classic compatmode but doesn't have the required capability.
.
MessageId=15668
SymbolicName=ERROR_STAGEFROMUPDATEAGENT_PACKAGE_NOT_APPLICABLE
Language=ENU
AppxUpdateAgent attempted to stage a package that is not applicable.
.
MessageId=15669
SymbolicName=ERROR_PACKAGE_NOT_REGISTERED_FOR_USER
Language=ENU
The application cannot be started for the target user.  Please have the user explicitly install this package.
.
MessageId=15670
SymbolicName=ERROR_PACKAGE_NAME_MISMATCH
Language=ENU
The provided package name does not match the expected package name. Check the AppXDeployment-Server event log for details.
.
MessageId=15671
SymbolicName=ERROR_APPINSTALLER_URI_IN_USE
Language=ENU
The provided .appinstaller URI is already being used by another package family. Check the AppXDeployment-Server event log for details.
.
MessageId=15672
SymbolicName=ERROR_APPINSTALLER_IS_MANAGED_BY_SYSTEM
Language=ENU
The package family's auto update settings are being managed at system priority and cannot be changed at default priority. Please contact your system administrator for help with the error.
.
MessageId=15673
SymbolicName=WINERR_15673
Language=ENU
Service is not available while sysprep is running.
.
MessageId=15674
SymbolicName=WINERR_15674
Language=ENU
The activation failed because the package targets the arm32 architecture, which is no longer supported. Try repairing or reinstalling the application to fix the problem.
.
MessageId=15675
SymbolicName=WINERR_15675
Language=ENU
The product is not applicable or cannot be found.
.
MessageId=15676
SymbolicName=WINERR_15676
Language=ENU
The deployment operation on this package is blocked because it or a package in the same family is pending removal. Please log off and log on again to finish the deferred package removal and try again.
.
MessageId=15677
SymbolicName=WINERR_15677
Language=ENU
The package repository root is corrupted.
.
MessageId=15678
SymbolicName=WINERR_15678
Language=ENU
The package manifest is not found.
.
MessageId=15679
SymbolicName=WINERR_15679
Language=ENU
The deployment operation on this package is blocked because the package family name is in RemoveDefaultMicrosoftStorePackages policy override.
.
MessageId=15680
SymbolicName=WINERR_15680
Language=ENU
The source uri is blocked by MsixAllowedZones policy. Please contact your system administrator.
.
MessageId=15681
SymbolicName=WINERR_15681
Language=ENU
The source uri is recommended to be blocked by SmartScreen. Please contact your system administrator.
.
MessageId=15700
SymbolicName=WINERR_15700
Language=ENU
The process has no package identity.
.
MessageId=15701
SymbolicName=WINERR_15701
Language=ENU
The package runtime information is corrupted.
.
MessageId=15702
SymbolicName=WINERR_15702
Language=ENU
The package identity is corrupted.
.
MessageId=15703
SymbolicName=WINERR_15703
Language=ENU
The process has no application identity.
.
MessageId=15704
SymbolicName=WINERR_15704
Language=ENU
One or more AppModel Runtime group policy values could not be read. Please contact your system administrator with the contents of your AppModel Runtime event log.
.
MessageId=15705
SymbolicName=WINERR_15705
Language=ENU
One or more AppModel Runtime group policy values are invalid. Please contact your system administrator with the contents of your AppModel Runtime event log.
.
MessageId=15706
SymbolicName=WINERR_15706
Language=ENU
The package is currently not available.
.
MessageId=15707
SymbolicName=WINERR_15707
Language=ENU
The package does not have a mutable directory.
.
MessageId=15800
SymbolicName=ERROR_STATE_LOAD_STORE_FAILED
Language=ENU
Loading the state store failed.
.
MessageId=15801
SymbolicName=ERROR_STATE_GET_VERSION_FAILED
Language=ENU
Retrieving the state version for the application failed.
.
MessageId=15802
SymbolicName=ERROR_STATE_SET_VERSION_FAILED
Language=ENU
Setting the state version for the application failed.
.
MessageId=15803
SymbolicName=ERROR_STATE_STRUCTURED_RESET_FAILED
Language=ENU
Resetting the structured state of the application failed.
.
MessageId=15804
SymbolicName=ERROR_STATE_OPEN_CONTAINER_FAILED
Language=ENU
State Manager failed to open the container.
.
MessageId=15805
SymbolicName=ERROR_STATE_CREATE_CONTAINER_FAILED
Language=ENU
State Manager failed to create the container.
.
MessageId=15806
SymbolicName=ERROR_STATE_DELETE_CONTAINER_FAILED
Language=ENU
State Manager failed to delete the container.
.
MessageId=15807
SymbolicName=ERROR_STATE_READ_SETTING_FAILED
Language=ENU
State Manager failed to read the setting.
.
MessageId=15808
SymbolicName=ERROR_STATE_WRITE_SETTING_FAILED
Language=ENU
State Manager failed to write the setting.
.
MessageId=15809
SymbolicName=ERROR_STATE_DELETE_SETTING_FAILED
Language=ENU
State Manager failed to delete the setting.
.
MessageId=15810
SymbolicName=ERROR_STATE_QUERY_SETTING_FAILED
Language=ENU
State Manager failed to query the setting.
.
MessageId=15811
SymbolicName=ERROR_STATE_READ_COMPOSITE_SETTING_FAILED
Language=ENU
State Manager failed to read the composite setting.
.
MessageId=15812
SymbolicName=ERROR_STATE_WRITE_COMPOSITE_SETTING_FAILED
Language=ENU
State Manager failed to write the composite setting.
.
MessageId=15813
SymbolicName=ERROR_STATE_ENUMERATE_CONTAINER_FAILED
Language=ENU
State Manager failed to enumerate the containers.
.
MessageId=15814
SymbolicName=ERROR_STATE_ENUMERATE_SETTINGS_FAILED
Language=ENU
State Manager failed to enumerate the settings.
.
MessageId=15815
SymbolicName=ERROR_STATE_COMPOSITE_SETTING_VALUE_SIZE_LIMIT_EXCEEDED
Language=ENU
The size of the state manager composite setting value has exceeded the limit.
.
MessageId=15816
SymbolicName=ERROR_STATE_SETTING_VALUE_SIZE_LIMIT_EXCEEDED
Language=ENU
The size of the state manager setting value has exceeded the limit.
.
MessageId=15817
SymbolicName=ERROR_STATE_SETTING_NAME_SIZE_LIMIT_EXCEEDED
Language=ENU
The length of the state manager setting name has exceeded the limit.
.
MessageId=15818
SymbolicName=ERROR_STATE_CONTAINER_NAME_SIZE_LIMIT_EXCEEDED
Language=ENU
The length of the state manager container name has exceeded the limit.
.
MessageId=15841
SymbolicName=ERROR_API_UNAVAILABLE
Language=ENU
This API cannot be used in the context of the caller's application type.
.
MessageId=15861
SymbolicName=WINERR_15861
Language=ENU
This PC does not have a valid license for the application or product.
.
MessageId=15862
SymbolicName=WINERR_15862
Language=ENU
The authenticated user does not have a valid license for the application or product.
.
MessageId=15863
SymbolicName=WINERR_15863
Language=ENU
The commerce transaction associated with this license is still pending verification.
.
MessageId=15864
SymbolicName=WINERR_15864
Language=ENU
The license has been revoked for this user.
.
MessageId=0x100
Severity=CoError
Facility=Trust
SymbolicName=TRUST_E_NOSIGNATURE
Language=ENU
No Signature found in file.
.
MessageId=0x4001
Severity=CoError
Facility=Null
SymbolicName=E_NOTIMPL
Language=ENU
Not implemented.
.
MessageId=0x1
Severity=CoError
Facility=Dxgi
SymbolicName=DXGI_ERROR_INVALID_CALL
Language=ENU
Invalid call.
.
MessageId=0x22
Severity=CoError
Facility=Dxgi
SymbolicName=DXGI_ERROR_NOT_CURRENTLY_AVAILABLE
Language=ENU
Resource is not currently available.
.
MessageId=0x4005
Severity=CoError
Facility=Null
SymbolicName=E_FAIL
Language=ENU
Call failed.
.
