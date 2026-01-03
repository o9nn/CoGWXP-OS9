/**
 * @file w7_compat.h
 * @brief Windows 7 (NT 6.1) Compatibility Layer for CogW7OS
 * 
 * This header provides compatibility definitions and API extensions
 * to support Windows 7 era applications and drivers within the
 * CogW7OS cognitive operating system.
 * 
 * Based on patterns from VxKex and ReactOS projects.
 * 
 * @copyright Research/Educational Use
 */

#ifndef _COGW7OS_W7_COMPAT_H_
#define _COGW7OS_W7_COMPAT_H_

#include "../kernel/cogw7os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * NT Version Constants
 *===========================================================================*/

/* Windows 7 = NT 6.1 */
#define COGW7_NT_MAJOR_VERSION      6
#define COGW7_NT_MINOR_VERSION      1
#define COGW7_NT_BUILD_NUMBER       7601    /* SP1 */

/* Windows XP = NT 5.1 (base compatibility) */
#define COGW7_XP_MAJOR_VERSION      5
#define COGW7_XP_MINOR_VERSION      1
#define COGW7_XP_BUILD_NUMBER       2600    /* SP3 */

/*===========================================================================
 * Extended API Flags
 *===========================================================================*/

/* Process creation flags (Windows 8+) */
#define COGW7_PROCESS_CREATION_MITIGATION_POLICY_DEP_ENABLE             0x00000001
#define COGW7_PROCESS_CREATION_MITIGATION_POLICY_DEP_ATL_THUNK_ENABLE   0x00000002
#define COGW7_PROCESS_CREATION_MITIGATION_POLICY_SEHOP_ENABLE           0x00000004

/* Thread flags */
#define COGW7_THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH    0x00000002
#define COGW7_THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER    0x00000004

/* Memory flags */
#define COGW7_MEM_EXTENDED_PARAMETER_TYPE_BITS          8
#define COGW7_MEM_EXTENDED_PARAMETER_GRAPHICS           0x00000001
#define COGW7_MEM_EXTENDED_PARAMETER_NONPAGED           0x00000002

/*===========================================================================
 * Extended Kernel32 APIs
 *===========================================================================*/

/* Pseudo-console support (Windows 10+) */
typedef void* COGW7_HPCON;

COGUTIL_API cog_result_t cogw7_CreatePseudoConsole(
    uint32_t size_x,
    uint32_t size_y,
    cogw7_handle_t input_handle,
    cogw7_handle_t output_handle,
    uint32_t flags,
    COGW7_HPCON* phPC
);

COGUTIL_API cog_result_t cogw7_ResizePseudoConsole(
    COGW7_HPCON hPC,
    uint32_t size_x,
    uint32_t size_y
);

COGUTIL_API void cogw7_ClosePseudoConsole(COGW7_HPCON hPC);

/* Thread pool work (Windows Vista+) */
typedef void* COGW7_PTP_WORK;
typedef void (*COGW7_PTP_WORK_CALLBACK)(void* Instance, void* Context, COGW7_PTP_WORK Work);

COGUTIL_API COGW7_PTP_WORK cogw7_CreateThreadpoolWork(
    COGW7_PTP_WORK_CALLBACK pfnwk,
    void* pv,
    void* pcbe
);

COGUTIL_API void cogw7_SubmitThreadpoolWork(COGW7_PTP_WORK pwk);
COGUTIL_API void cogw7_WaitForThreadpoolWorkCallbacks(COGW7_PTP_WORK pwk, bool fCancelPendingCallbacks);
COGUTIL_API void cogw7_CloseThreadpoolWork(COGW7_PTP_WORK pwk);

/* Condition variables (Windows Vista+) */
typedef struct {
    void* ptr;
} COGW7_CONDITION_VARIABLE;

COGUTIL_API void cogw7_InitializeConditionVariable(COGW7_CONDITION_VARIABLE* ConditionVariable);
COGUTIL_API bool cogw7_SleepConditionVariableCS(COGW7_CONDITION_VARIABLE* ConditionVariable, cog_mutex_t* CriticalSection, uint32_t dwMilliseconds);
COGUTIL_API void cogw7_WakeConditionVariable(COGW7_CONDITION_VARIABLE* ConditionVariable);
COGUTIL_API void cogw7_WakeAllConditionVariable(COGW7_CONDITION_VARIABLE* ConditionVariable);

/* Slim reader/writer locks (Windows Vista+) */
typedef struct {
    void* ptr;
} COGW7_SRWLOCK;

COGUTIL_API void cogw7_InitializeSRWLock(COGW7_SRWLOCK* SRWLock);
COGUTIL_API void cogw7_AcquireSRWLockExclusive(COGW7_SRWLOCK* SRWLock);
COGUTIL_API void cogw7_AcquireSRWLockShared(COGW7_SRWLOCK* SRWLock);
COGUTIL_API void cogw7_ReleaseSRWLockExclusive(COGW7_SRWLOCK* SRWLock);
COGUTIL_API void cogw7_ReleaseSRWLockShared(COGW7_SRWLOCK* SRWLock);
COGUTIL_API bool cogw7_TryAcquireSRWLockExclusive(COGW7_SRWLOCK* SRWLock);
COGUTIL_API bool cogw7_TryAcquireSRWLockShared(COGW7_SRWLOCK* SRWLock);

/* One-time initialization (Windows Vista+) */
typedef struct {
    void* ptr;
} COGW7_INIT_ONCE;

typedef bool (*COGW7_PINIT_ONCE_FN)(COGW7_INIT_ONCE* InitOnce, void* Parameter, void** Context);

COGUTIL_API void cogw7_InitOnceInitialize(COGW7_INIT_ONCE* InitOnce);
COGUTIL_API bool cogw7_InitOnceExecuteOnce(COGW7_INIT_ONCE* InitOnce, COGW7_PINIT_ONCE_FN InitFn, void* Parameter, void** Context);

/*===========================================================================
 * Extended NTDLL APIs
 *===========================================================================*/

/* Process information classes (extended) */
typedef enum {
    COGW7_ProcessBasicInformation = 0,
    COGW7_ProcessDebugPort = 7,
    COGW7_ProcessWow64Information = 26,
    COGW7_ProcessImageFileName = 27,
    COGW7_ProcessBreakOnTermination = 29,
    COGW7_ProcessSubsystemInformation = 75,
    
    /* Cognitive extensions */
    COGW7_ProcessAtomSpaceInformation = 0x1000,
    COGW7_ProcessAgentInformation = 0x1001,
    COGW7_ProcessAttentionValue = 0x1002
} COGW7_PROCESSINFOCLASS;

/* Thread information classes (extended) */
typedef enum {
    COGW7_ThreadBasicInformation = 0,
    COGW7_ThreadTimes = 1,
    COGW7_ThreadPriority = 2,
    COGW7_ThreadBasePriority = 3,
    COGW7_ThreadAffinityMask = 4,
    COGW7_ThreadImpersonationToken = 5,
    COGW7_ThreadDescriptorTableEntry = 6,
    COGW7_ThreadEnableAlignmentFaultFixup = 7,
    COGW7_ThreadEventPair = 8,
    COGW7_ThreadQuerySetWin32StartAddress = 9,
    COGW7_ThreadZeroTlsCell = 10,
    COGW7_ThreadPerformanceCount = 11,
    COGW7_ThreadAmILastThread = 12,
    COGW7_ThreadIdealProcessor = 13,
    COGW7_ThreadPriorityBoost = 14,
    COGW7_ThreadSetTlsArrayAddress = 15,
    COGW7_ThreadIsIoPending = 16,
    COGW7_ThreadHideFromDebugger = 17,
    
    /* Cognitive extensions */
    COGW7_ThreadCognitiveBoost = 0x1000,
    COGW7_ThreadDisThread = 0x1001
} COGW7_THREADINFOCLASS;

COGUTIL_API cog_result_t cogw7_NtQueryInformationProcess(
    cogw7_handle_t ProcessHandle,
    COGW7_PROCESSINFOCLASS ProcessInformationClass,
    void* ProcessInformation,
    uint32_t ProcessInformationLength,
    uint32_t* ReturnLength
);

COGUTIL_API cog_result_t cogw7_NtSetInformationProcess(
    cogw7_handle_t ProcessHandle,
    COGW7_PROCESSINFOCLASS ProcessInformationClass,
    void* ProcessInformation,
    uint32_t ProcessInformationLength
);

COGUTIL_API cog_result_t cogw7_NtQueryInformationThread(
    cogw7_handle_t ThreadHandle,
    COGW7_THREADINFOCLASS ThreadInformationClass,
    void* ThreadInformation,
    uint32_t ThreadInformationLength,
    uint32_t* ReturnLength
);

COGUTIL_API cog_result_t cogw7_NtSetInformationThread(
    cogw7_handle_t ThreadHandle,
    COGW7_THREADINFOCLASS ThreadInformationClass,
    void* ThreadInformation,
    uint32_t ThreadInformationLength
);

/*===========================================================================
 * Memory Management Extensions
 *===========================================================================*/

/* Virtual memory types (Windows 10+) */
typedef enum {
    COGW7_MemExtendedParameterInvalidType = 0,
    COGW7_MemExtendedParameterAddressRequirements,
    COGW7_MemExtendedParameterNumaNode,
    COGW7_MemExtendedParameterPartitionHandle,
    COGW7_MemExtendedParameterUserPhysicalHandle,
    COGW7_MemExtendedParameterAttributeFlags,
    COGW7_MemExtendedParameterMax
} COGW7_MEM_EXTENDED_PARAMETER_TYPE;

typedef struct {
    uint64_t Type : COGW7_MEM_EXTENDED_PARAMETER_TYPE_BITS;
    uint64_t Reserved : 64 - COGW7_MEM_EXTENDED_PARAMETER_TYPE_BITS;
    union {
        uint64_t ULong64;
        void* Pointer;
        size_t Size;
        cogw7_handle_t Handle;
        uint32_t ULong;
    };
} COGW7_MEM_EXTENDED_PARAMETER;

COGUTIL_API void* cogw7_VirtualAlloc2(
    cogw7_handle_t Process,
    void* BaseAddress,
    size_t Size,
    uint32_t AllocationType,
    uint32_t PageProtection,
    COGW7_MEM_EXTENDED_PARAMETER* ExtendedParameters,
    uint32_t ParameterCount
);

COGUTIL_API void* cogw7_MapViewOfFile3(
    cogw7_handle_t FileMapping,
    cogw7_handle_t Process,
    void* BaseAddress,
    uint64_t Offset,
    size_t ViewSize,
    uint32_t AllocationType,
    uint32_t PageProtection,
    COGW7_MEM_EXTENDED_PARAMETER* ExtendedParameters,
    uint32_t ParameterCount
);

/*===========================================================================
 * File System Extensions
 *===========================================================================*/

/* File ID types (Windows 8+) */
typedef struct {
    uint64_t VolumeSerialNumber;
    uint64_t FileId[2];     /* 128-bit file ID */
} COGW7_FILE_ID_128;

typedef struct {
    uint32_t dwSize;
    uint64_t VolumeSerialNumber;
    COGW7_FILE_ID_128 FileId;
} COGW7_FILE_ID_INFO;

COGUTIL_API bool cogw7_GetFileInformationByHandleEx(
    cogw7_handle_t hFile,
    uint32_t FileInformationClass,
    void* lpFileInformation,
    uint32_t dwBufferSize
);

/*===========================================================================
 * Security Extensions
 *===========================================================================*/

/* Token information classes (extended) */
typedef enum {
    COGW7_TokenUser = 1,
    COGW7_TokenGroups,
    COGW7_TokenPrivileges,
    COGW7_TokenOwner,
    COGW7_TokenPrimaryGroup,
    COGW7_TokenDefaultDacl,
    COGW7_TokenSource,
    COGW7_TokenType,
    COGW7_TokenImpersonationLevel,
    COGW7_TokenStatistics,
    COGW7_TokenRestrictedSids,
    COGW7_TokenSessionId,
    COGW7_TokenGroupsAndPrivileges,
    COGW7_TokenSessionReference,
    COGW7_TokenSandBoxInert,
    COGW7_TokenAuditPolicy,
    COGW7_TokenOrigin,
    COGW7_TokenElevationType,
    COGW7_TokenLinkedToken,
    COGW7_TokenElevation,
    COGW7_TokenHasRestrictions,
    COGW7_TokenAccessInformation,
    COGW7_TokenVirtualizationAllowed,
    COGW7_TokenVirtualizationEnabled,
    COGW7_TokenIntegrityLevel,
    COGW7_TokenUIAccess,
    COGW7_TokenMandatoryPolicy,
    COGW7_TokenLogonSid,
    COGW7_TokenIsAppContainer,
    COGW7_TokenCapabilities,
    COGW7_TokenAppContainerSid,
    COGW7_TokenAppContainerNumber,
    COGW7_TokenUserClaimAttributes,
    COGW7_TokenDeviceClaimAttributes,
    COGW7_TokenRestrictedUserClaimAttributes,
    COGW7_TokenRestrictedDeviceClaimAttributes,
    COGW7_TokenDeviceGroups,
    COGW7_TokenRestrictedDeviceGroups,
    COGW7_TokenSecurityAttributes,
    COGW7_TokenIsRestricted,
    COGW7_TokenProcessTrustLevel,
    COGW7_TokenPrivateNameSpace,
    COGW7_TokenSingletonAttributes,
    COGW7_TokenBnoIsolation,
    COGW7_TokenChildProcessFlags,
    COGW7_TokenIsLessPrivilegedAppContainer,
    COGW7_TokenIsSandboxed,
    COGW7_TokenIsAppSilo,
    
    /* Cognitive extensions */
    COGW7_TokenAgentCapabilities = 0x1000,
    COGW7_TokenAtomSpaceAccess = 0x1001
} COGW7_TOKEN_INFORMATION_CLASS;

/*===========================================================================
 * Cognitive API Extensions
 *===========================================================================*/

/* These APIs extend the NT model with cognitive capabilities */

/**
 * Create a cognitive process with AtomSpace
 */
COGUTIL_API cog_result_t cogw7_CreateCognitiveProcess(
    const char* ApplicationName,
    const char* CommandLine,
    atomspace_t AtomSpace,
    cogw7_handle_t* ProcessHandle,
    cogw7_handle_t* ThreadHandle
);

/**
 * Attach an agent to a process
 */
COGUTIL_API cog_result_t cogw7_AttachAgentToProcess(
    cogw7_handle_t ProcessHandle,
    uint64_t AgentId
);

/**
 * Set cognitive priority boost based on attention
 */
COGUTIL_API cog_result_t cogw7_SetCognitiveBoost(
    cogw7_handle_t ThreadHandle,
    attention_value_t AttentionValue
);

/**
 * Query process AtomSpace
 */
COGUTIL_API atomspace_t cogw7_GetProcessAtomSpace(
    cogw7_handle_t ProcessHandle
);

/**
 * Create a Dis thread within a process
 */
COGUTIL_API cog_result_t cogw7_CreateDisThread(
    cogw7_handle_t ProcessHandle,
    const char* ModuleName,
    const char* FunctionName,
    cogw7_handle_t* ThreadHandle
);

/*===========================================================================
 * Version Spoofing
 *===========================================================================*/

/**
 * Set the version reported to applications
 */
COGUTIL_API cog_result_t cogw7_SetVersionSpoof(
    cogw7_handle_t ProcessHandle,
    uint32_t MajorVersion,
    uint32_t MinorVersion,
    uint32_t BuildNumber
);

/**
 * Get the actual NT version
 */
COGUTIL_API void cogw7_GetRealVersion(
    uint32_t* MajorVersion,
    uint32_t* MinorVersion,
    uint32_t* BuildNumber
);

#ifdef __cplusplus
}
#endif

#endif /* _COGW7OS_W7_COMPAT_H_ */
