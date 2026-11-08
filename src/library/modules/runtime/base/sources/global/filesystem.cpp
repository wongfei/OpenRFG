// Original: library/modules/runtime/base/sources/global/filesystem.cpp
//
// The keen virtual file system. Mount points form a tree of path components under pRootPoint; each mount point has
// a list of devices (newest first). A name is resolved by walking the tree as far as its leading directories match
// mount points; the rest of the name is handed to the devices of the deepest match in list order, and the first
// device that does not report IoError_FileNotFound (or IoError_NotImplemented, for status queries) wins. Relative
// names resolve at the root. A mount id is (mount point index << 16 | device entry index); a file handle is
// (device entry index << 16 | device file handle).
//
// createFileSystem mounts the native device at "/" with an empty base directory when enableDefaultMount is set, so
// plain OS paths work. The mutex guards the mount tree only; device calls run outside it.

#include "keen/base/filesystem.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/poolallocator.hpp"
#include "keen/base/string.hpp"
#include "keen/base/tree.hpp"

#include "aliaspathfiledevice.hpp"
#include "memoryfiledevice.hpp"
#include "nativefiledevice.hpp"
#include "pakfiledevice.hpp"

#include <new>
#include <windows.h>

namespace keen
{
    // Intrusive list links; T derives from ListEntry<T>.
    // size 0x8
    template<typename T>
    struct ListEntry
    {
        ListEntry(); // <FileSystemDeviceEntry> 0x00BF31D0 (filesystem.cpp:41)

        T* pNext; // 0x00
        T* pPrev; // 0x04
    };

    // A device mounted at a mount point. Several devices can share one mount point; they are searched in list order.
    // size 0x14
    struct FileSystemDeviceEntry : public ListEntry<FileSystemDeviceEntry>
    {
        FileSystemDeviceEntry(); // 0x00BF7960 (:49)

        FileDeviceMountData* pMountData; // 0x08
        FileDeviceInterface* pDevice; // 0x0C
        bool isInternalDevice; // 0x10
    };

    // size 0x94
    struct FileSystemMountPoint : public TreeNode<FileSystemMountPoint>
    {
        FileSystemMountPoint(); // 0x00BF7970 (:58)

        char path[128]; // 0x10
        FileSystemDeviceEntry* pFirstDevice; // 0x90
    };

    // size 0x308
    struct FileSystem
    {
        FileSystem(); // 0x00C050C0
        ~FileSystem(); // 0x00BFBC60

        PoolAllocator<FileSystemDeviceEntry> devices; // 0x00
        PoolAllocator<FileSystemMountPoint> mountPoints; // 0x18
        NativeFileDevice nativeFileDevice; // 0x30
        MemoryFileDevice memoryFileDevice; // 0xC8
        PakFileDevice pakFileDevice; // 0x130
        AliasPathFileDevice aliasPathFileDevice; // 0x264
        FileSystemMountPoint* pRootPoint; // 0x2B8
        uint32 nativeMountHandle; // 0x2BC
        Mutex mutex; // 0x2C0
        uint32 openFileStack; // 0x2F8
        MemoryAllocator* pAllocator; // 0x2FC
        volatile uint32 useIo; // 0x300
        bool ignoreFirstChild; // 0x304
    };

    // <FileSystemDeviceEntry> 0x00BF31D0 (:41)
    template<typename T>
    ListEntry<T>::ListEntry()
    {
        pNext = nullptr;
        pPrev = nullptr;
    }

    // 0x00BF7960 (:49)
    FileSystemDeviceEntry::FileSystemDeviceEntry()
    {
        pMountData = nullptr;
        pDevice = nullptr;
    }

    // 0x00BF7970 (:58)
    FileSystemMountPoint::FileSystemMountPoint()
    {
        path[0] = '\0';
        pFirstDevice = nullptr;
    }

    // 0x00C050C0 (implicit)
    FileSystem::FileSystem()
    {
    }

    // 0x00BFBC60 (implicit)
    FileSystem::~FileSystem()
    {
    }

    namespace file
    {
        // Parsed path used to walk the mount point tree.
        // size 0x110
        class FileSystemPath
        {
        public:
            FileSystemPath(const char* pPath); // 0x00BF7990 (:151)
            bool isValid() const; // 0x00BEFD10 (:103)
            String getPath() const; // 0x00BEFD20 (:104)
            bool isRelativePath() const; // 0x00BEFD40 (:105)
            bool getNextDirectoryPart(String* pPart) const; // 0x00BF7A10 (:174)
            String getRemainingDirectoryExclusive(String part) const; // 0x00BEFD60 (:197)
            String getRemainingDirectoryInclusive(String part) const; // 0x00BEFDA0 (:207)

        private:
            char m_path[260]; // 0x00
            String m_pathString; // 0x104
            bool m_isValid; // 0x10C
        };

        // 0x00BEFD10 (:103)
        bool FileSystemPath::isValid() const
        {
            return m_isValid;
        }

        // 0x00BEFD20 (:104)
        String FileSystemPath::getPath() const
        {
            return m_pathString;
        }

        // 0x00BEFD40 (:105)
        bool FileSystemPath::isRelativePath() const
        {
            return m_path[0] != '/';
        }

        // 0x00BF7990 (:151)
        FileSystemPath::FileSystemPath(const char* pPath)
        {
            // Backslashes become slashes and a trailing slash is dropped. Empty or too long paths are invalid.
            const uint32 length = copyString(m_path, sizeof(m_path), pPath);
            if (length == 0 || length >= sizeof(m_path))
            {
                m_isValid = false;
                return;
            }
            for (char* p = m_path; *p != '\0'; ++p)
            {
                if (*p == '\\')
                {
                    *p = '/';
                }
            }
            uint32 pathLength = length;
            if (m_path[pathLength - 1] == '/')
            {
                m_path[pathLength - 1] = '\0';
                --pathLength;
            }
            m_pathString = String(m_path, m_path + pathLength);
            m_isValid = true;
        }

        // 0x00BF7A10 (:174)
        bool FileSystemPath::getNextDirectoryPart(String* pPart) const
        {
            if (pPart->pStart == nullptr)
            {
                const char* pStart = m_path[0] == '/' ? m_path + 1 : m_path;
                const char* pEnd = pStart;
                while (*pEnd != '\0' && *pEnd != '/')
                {
                    ++pEnd;
                }
                pPart->pEnd = pEnd;
                pPart->pStart = pStart;
                return *pStart != '\0';
            }
            if (*pPart->pEnd != '\0')
            {
                *pPart = findNextStringToken(pPart->pEnd + 1, '/');
                return *pPart->pStart != '\0';
            }
            pPart->pStart = pPart->pEnd;
            return false;
        }

        // 0x00BEFD60 (:197)
        String FileSystemPath::getRemainingDirectoryExclusive(String part) const
        {
            if (part.pEnd <= m_pathString.pEnd && m_pathString.pStart <= part.pStart)
            {
                return String(part.pEnd + 1, m_pathString.pEnd);
            }
            return String(m_pathString.pEnd, m_pathString.pEnd);
        }

        // 0x00BEFDA0 (:207)
        String FileSystemPath::getRemainingDirectoryInclusive(String part) const
        {
            if (part.pEnd <= m_pathString.pEnd && m_pathString.pStart <= part.pStart)
            {
                return String(part.pStart, m_pathString.pEnd);
            }
            return String(m_pathString.pEnd, m_pathString.pEnd);
        }

        static IoResult<uint32> mountDeviceInternal(FileSystem* pFileSystem, const char* pTargetPath, FileDeviceInterface* pDevice, FileDeviceMountData* pMountData, bool isInternalDevice);
        static IoError unmountInternal(FileSystem* pFileSystem, uint32 mountId);

        static IoResult<uint32> createMountIoResult(uint32 result, IoError error)
        {
            IoResult<uint32> ioResult;
            ioResult.result = result;
            ioResult.error = error;
            return ioResult;
        }

        // 0x00C0B2B0 (:217), LTCG (pFileSystem in EDI)
        bool mountDefaultNativePath(FileSystem* pFileSystem, bool enableMount)
        {
            if (!enableMount)
            {
                pFileSystem->nativeMountHandle = 0xFFFFFFFFu;
                return true;
            }
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->nativeFileDevice.allocateMountData(pFileSystem->pAllocator, "", false);
            if (mountData.error != IoError_Ok)
            {
                return false;
            }
            const IoResult<uint32> mountResult = mountDeviceInternal(pFileSystem, "/", &pFileSystem->nativeFileDevice, mountData.result, true);
            if (mountResult.error != IoError_Ok)
            {
                // mountDeviceInternal already released the mount data of an internal device; the original frees it
                // a second time here.
                pFileSystem->nativeFileDevice.freeMountData(pFileSystem->pAllocator, mountData.result);
                return false;
            }
            pFileSystem->nativeMountHandle = mountResult.result;
            return true;
        }

        // 0x00C04EF0 (:249), LTCG (pFileSystem in ESI)
        void unmountDefaultNativePath(FileSystem* pFileSystem)
        {
            if (pFileSystem->nativeMountHandle != 0xFFFFFFFFu)
            {
                unmountInternal(pFileSystem, pFileSystem->nativeMountHandle);
                pFileSystem->nativeMountHandle = 0xFFFFFFFFu;
            }
        }

        // 0x00BF7AA0 (:262), LTCG (pMountPoint EAX, pFileSystem ECX)
        static uint32 createFileMountHandle(FileSystem* pFileSystem, FileSystemMountPoint* pMountPoint, FileSystemDeviceEntry* pDeviceEntry)
        {
            return pFileSystem->devices.getIndexFromObject(pDeviceEntry) | (pFileSystem->mountPoints.getIndexFromObject(pMountPoint) << 16);
        }

        // 0x00BF7AC0 (:270), LTCG (pFileSystem ECX, mountId EDX, ppDeviceEntry EDI)
        static void getValuesFromFileMountHandle(FileSystem* pFileSystem, FileSystemMountPoint** ppMountPoint, FileSystemDeviceEntry** ppDeviceEntry, uint32 mountId)
        {
            *ppMountPoint = pFileSystem->mountPoints.getObjectFromIndex(mountId >> 16);
            *ppDeviceEntry = pFileSystem->devices.getObjectFromIndex(mountId & 0xFFFF);
        }

        // 0x00BEFDE0 (:279), LTCG (deviceIndex EAX, fileHandle ECX)
        static uint32 createFileHandle(uint16 deviceIndex, uint16 fileHandle)
        {
            return ((uint32)deviceIndex << 16) | fileHandle;
        }

        // 0x00BEFDF0 (:284)
        static uint16 getFileDeviceIndex(uint32 fileHandle)
        {
            return (uint16)(fileHandle >> 16);
        }

        // 0x00BEFE00 (:289)
        static uint16 getFileDeviceFileHandle(uint32 fileHandle)
        {
            return (uint16)fileHandle;
        }

        // 0x00C02E60 (:353), LTCG (pMountPoint EAX, pFileSystem ESI)
        static void freeMountPoint(FileSystemMountPoint* pMountPoint, FileSystem* pFileSystem)
        {
            // Releases the mount point and then every parent that became empty (no devices, no children). The root
            // has no parent and stays.
            while (pMountPoint != nullptr && pMountPoint->pParent != nullptr && pMountPoint->pFirstDevice == nullptr && pMountPoint->pFirstChild == nullptr)
            {
                FileSystemMountPoint* pParent = pMountPoint->pParent;
                if (pParent->pFirstChild == pMountPoint)
                {
                    pParent->pFirstChild = pMountPoint->pNextSibling;
                }
                pMountPoint->pParent = nullptr;
                if (pMountPoint->pPrevSibling != nullptr)
                {
                    pMountPoint->pPrevSibling->pNextSibling = pMountPoint->pNextSibling;
                }
                if (pMountPoint->pNextSibling != nullptr)
                {
                    pMountPoint->pNextSibling->pPrevSibling = pMountPoint->pPrevSibling;
                }
                pMountPoint->pPrevSibling = nullptr;
                pMountPoint->pNextSibling = nullptr;
                pFileSystem->mountPoints.freeObject(pMountPoint);
                pMountPoint = pParent;
            }
        }

        static FileSystemMountPoint* findChildMountPoint(FileSystemMountPoint* pParent, String part)
        {
            for (FileSystemMountPoint* pChild = pParent->pFirstChild; pChild != nullptr; pChild = pChild->pNextSibling)
            {
                if (compareString(part.pStart, part.pEnd, pChild->path) == 0)
                {
                    return pChild;
                }
            }
            return nullptr;
        }

        // 0x00C04F10 (:294)
        static FileSystemMountPoint* allocateMountPoint(FileSystem* pFileSystem, const char* pPath)
        {
            // Finds the mount point for pPath, creating the missing tail of the path.
            FileSystemPath path(pPath);
            if (!path.isValid())
            {
                return nullptr;
            }
            FileSystemMountPoint* pMountPoint = pFileSystem->pRootPoint;
            String part;
            while (path.getNextDirectoryPart(&part))
            {
                FileSystemMountPoint* pChild = findChildMountPoint(pMountPoint, part);
                if (pChild == nullptr)
                {
                    break;
                }
                pMountPoint = pChild;
            }
            while (part.pStart != part.pEnd)
            {
                FileSystemMountPoint* pChild = pFileSystem->mountPoints.newObject();
                if (pChild == nullptr)
                {
                    freeMountPoint(pMountPoint, pFileSystem);
                    return nullptr;
                }
                uint32 length = 0;
                char* pDest = pChild->path;
                for (const char* p = part.pStart; p != part.pEnd; ++p)
                {
                    if (length < sizeof(pChild->path) - 1)
                    {
                        *pDest++ = *p;
                    }
                    ++length;
                }
                *pDest = '\0';
                pChild->pParent = pMountPoint;
                FileSystemMountPoint* pFirstChild = pMountPoint->pFirstChild;
                pMountPoint->pFirstChild = pChild;
                pChild->pNextSibling = pFirstChild;
                if (pFirstChild != nullptr)
                {
                    pFirstChild->pPrevSibling = pChild;
                }
                pMountPoint = pChild;
                if (!path.getNextDirectoryPart(&part))
                {
                    break;
                }
            }
            return pMountPoint;
        }

        // 0x00BFD320 (:381), LTCG (path EAX, pFileSystem EDX, pMountPath EDI, pRemainingPath on the stack)
        static FileSystemMountPoint* findMountPoint(String* pMountPath, String* pRemainingPath, FileSystem* pFileSystem, const FileSystemPath& path)
        {
            if (!path.isValid())
            {
                return nullptr;
            }
            const String pathString = path.getPath();
            if (path.isRelativePath())
            {
                *pMountPath = String(nullptr, nullptr);
                *pRemainingPath = pathString;
                return pFileSystem->pRootPoint;
            }
            FileSystemMountPoint* pMountPoint = pFileSystem->pRootPoint;
            if ((pMountPoint->pFirstChild == nullptr || pFileSystem->ignoreFirstChild) && *pathString.pStart == '/')
            {
                *pMountPath = String(pathString.pStart, pathString.pStart);
                *pRemainingPath = pathString;
                return pMountPoint;
            }

            String part;
            bool hasMatch = false;
            while (path.getNextDirectoryPart(&part))
            {
                FileSystemMountPoint* pChild = findChildMountPoint(pMountPoint, part);
                if (pChild == nullptr)
                {
                    if (hasMatch)
                    {
                        *pMountPath = String(pathString.pStart, part.pStart);
                        const char* pRemainingStart = (pathString.pEnd < part.pEnd || part.pStart < pathString.pStart) ? pathString.pEnd : part.pStart;
                        *pRemainingPath = String(pRemainingStart, pathString.pEnd);
                    }
                    else
                    {
                        *pMountPath = String(nullptr, nullptr);
                        *pRemainingPath = pathString;
                    }
                    return pMountPoint;
                }
                hasMatch = true;
                pMountPoint = pChild;
            }
            if (part.pStart != part.pEnd)
            {
                *pMountPath = String(pathString.pStart, part.pEnd);
                *pRemainingPath = path.getRemainingDirectoryExclusive(part);
                return pMountPoint;
            }
            *pMountPath = pathString;
            *pRemainingPath = String(nullptr, nullptr);
            return pMountPoint;
        }

        // <FileSystemDeviceEntry> 0x00BF3A90 (:458)
        template<typename T>
        T* pushFront(T* pList, T* pEntry)
        {
            if (pList != nullptr)
            {
                pEntry->pNext = pList;
                pList->pPrev = pEntry;
            }
            return pEntry;
        }

        // 0x00C0DB30 (:474)
        FileSystem* createFileSystem(MemoryAllocator* pAllocator, const FileSystemCreationParameters& parameters)
        {
            void* pMemory = pAllocator->allocate(sizeof(FileSystem), 4, 0, nullptr);
            if (pMemory == nullptr)
            {
                return nullptr;
            }
            FileSystem* pFileSystem = new (pMemory) FileSystem();
            pFileSystem->useIo = 1;
            pFileSystem->pAllocator = pAllocator;
            pFileSystem->openFileStack = 0;
            pFileSystem->ignoreFirstChild = false;

            if (!pFileSystem->devices.create(allocateMemoryBlock(pAllocator, parameters.maxDeviceCount * sizeof(FileSystemDeviceEntry), 16, 0, nullptr)))
            {
                deleteObject(pFileSystem, pAllocator);
                return nullptr;
            }
            if (!pFileSystem->mountPoints.create(allocateMemoryBlock(pFileSystem->pAllocator, parameters.maxMountPoints * sizeof(FileSystemMountPoint), 16, 0, nullptr)))
            {
                pFileSystem->devices.destroyFromAllocator(pFileSystem->pAllocator);
                deleteObject(pFileSystem, pAllocator);
                return nullptr;
            }

            if (pFileSystem->nativeFileDevice.create(pFileSystem->pAllocator, parameters.maxNativeStreamCount, parameters.nativeThreadBufferSize, parameters.useReadThreads))
            {
                if (pFileSystem->memoryFileDevice.create(pFileSystem->pAllocator, parameters.maxMemoryFileCount, parameters.maxMemoryStreamCount))
                {
                    if (pFileSystem->pakFileDevice.create(pFileSystem->pAllocator, parameters.maxPakStreamCount, parameters.maxPakFileSize))
                    {
                        if (pFileSystem->aliasPathFileDevice.create(pFileSystem->pAllocator, parameters.maxAliasStreamCount))
                        {
                            pFileSystem->pRootPoint = pFileSystem->mountPoints.newObject();
                            if (pFileSystem->pRootPoint != nullptr)
                            {
                                if (mountDefaultNativePath(pFileSystem, parameters.enableDefaultMount))
                                {
                                    pFileSystem->mutex.create("FileSystem");
                                    return pFileSystem;
                                }
                                pFileSystem->mountPoints.freeObject(pFileSystem->pRootPoint);
                            }
                            pFileSystem->aliasPathFileDevice.destroy(pFileSystem->pAllocator);
                        }
                        pFileSystem->pakFileDevice.destroy(pFileSystem->pAllocator);
                    }
                    pFileSystem->memoryFileDevice.destroy(pFileSystem->pAllocator);
                }
                pFileSystem->nativeFileDevice.destroy(pFileSystem->pAllocator);
            }
            pFileSystem->mountPoints.destroyFromAllocator(pFileSystem->pAllocator);
            pFileSystem->devices.destroyFromAllocator(pFileSystem->pAllocator);
            deleteObject(pFileSystem, pAllocator);
            return nullptr;
        }

        // 0x00C0CA10 (:533)
        void destroyFileSystem(FileSystem* pFileSystem)
        {
            unmountDefaultNativePath(pFileSystem);
            if (pFileSystem->pRootPoint != nullptr)
            {
                pFileSystem->mountPoints.freeObject(pFileSystem->pRootPoint);
            }
            pFileSystem->aliasPathFileDevice.destroy(pFileSystem->pAllocator);
            pFileSystem->pakFileDevice.destroy(pFileSystem->pAllocator);
            pFileSystem->memoryFileDevice.destroy(pFileSystem->pAllocator);
            pFileSystem->nativeFileDevice.destroy(pFileSystem->pAllocator);
            pFileSystem->mountPoints.destroyFromAllocator(pFileSystem->pAllocator);
            pFileSystem->devices.destroyFromAllocator(pFileSystem->pAllocator);
            pFileSystem->mutex.destroy();
            deleteObject(pFileSystem, pFileSystem->pAllocator);
        }

        // 0x00C08180 (:554)
        IoResult<uint32> mountAliasPath(FileSystem* pFileSystem, const char* pTargetPath, const char* pSourcePath)
        {
            MutexLock lock(&pFileSystem->mutex);
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->aliasPathFileDevice.allocateMountData(pFileSystem->pAllocator, pFileSystem, pSourcePath);
            if (mountData.error != IoError_Ok)
            {
                return createMountIoResult(0xFFFFFFFFu, mountData.error);
            }
            return mountDeviceInternal(pFileSystem, pTargetPath, &pFileSystem->aliasPathFileDevice, mountData.result, true);
        }

        // 0x00C05180 (:568), LTCG (pFileSystem in EAX)
        static IoResult<uint32> mountDeviceInternal(FileSystem* pFileSystem, const char* pTargetPath, FileDeviceInterface* pDevice, FileDeviceMountData* pMountData, bool isInternalDevice)
        {
            // Internal devices own their mount data: it is released when mounting fails.
            FileSystemMountPoint* pMountPoint = nullptr;
            if (!pFileSystem->devices.isFull())
            {
                pMountPoint = allocateMountPoint(pFileSystem, pTargetPath);
            }
            if (pMountPoint == nullptr)
            {
                if (isInternalDevice)
                {
                    pDevice->freeMountData(pFileSystem->pAllocator, pMountData);
                }
                return createMountIoResult(0xFFFFFFFFu, IoError_OutOfMemory);
            }
            for (FileSystemDeviceEntry* pEntry = pMountPoint->pFirstDevice; pEntry != nullptr; pEntry = pEntry->pNext)
            {
                if (pEntry->pDevice == pDevice && pEntry->pMountData == pMountData)
                {
                    freeMountPoint(pMountPoint, pFileSystem);
                    if (isInternalDevice)
                    {
                        pDevice->freeMountData(pFileSystem->pAllocator, pMountData);
                    }
                    return createMountIoResult(0xFFFFFFFFu, IoError_Generic);
                }
            }
            FileSystemDeviceEntry* pEntry = pFileSystem->devices.newObject();
            pEntry->pDevice = pDevice;
            pEntry->pMountData = pMountData;
            pEntry->isInternalDevice = isInternalDevice;
            pMountPoint->pFirstDevice = pushFront(pMountPoint->pFirstDevice, pEntry);
            return createMountIoResult(createFileMountHandle(pFileSystem, pMountPoint, pEntry), IoError_Ok);
        }

        // 0x00C0CB80 (:620)
        IoResult<uint32> mountNativeHostPath(FileSystem* pFileSystem, const char* pTargetPath, const char* pHostPath, bool createIfNotExist)
        {
            MutexLock lock(&pFileSystem->mutex);
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->nativeFileDevice.allocateMountData(pFileSystem->pAllocator, pHostPath, createIfNotExist);
            if (mountData.error != IoError_Ok)
            {
                return createMountIoResult(0xFFFFFFFFu, mountData.error);
            }
            return mountDeviceInternal(pFileSystem, pTargetPath, &pFileSystem->nativeFileDevice, mountData.result, true);
        }

        // 0x00C052A0 (:663)
        IoResult<uint32> mountDevice(FileSystem* pFileSystem, const char* pTargetPath, FileDeviceInterface* pDevice, FileDeviceMountData* pMountData)
        {
            MutexLock lock(&pFileSystem->mutex);
            return mountDeviceInternal(pFileSystem, pTargetPath, pDevice, pMountData, false);
        }

        // 0x00C0B340 (:672)
        IoResult<uint32> mountNativePath(FileSystem* pFileSystem, const char* pTargetPath, const char* pNativePath, bool createIfNotExist)
        {
            MutexLock lock(&pFileSystem->mutex);
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->nativeFileDevice.allocateMountData(pFileSystem->pAllocator, pNativePath, createIfNotExist);
            if (mountData.error != IoError_Ok)
            {
                return createMountIoResult(0xFFFFFFFFu, mountData.error);
            }
            return mountDeviceInternal(pFileSystem, pTargetPath, &pFileSystem->nativeFileDevice, mountData.result, true);
        }

        // 0x00C08230 (:686)
        IoResult<uint32> mountReadOnlyMemoryFile(FileSystem* pFileSystem, const char* pTargetPath, const void* pData, uint32 dataSize)
        {
            // The mount point is the file's own path; the device then sees an empty name and serves the mounted block.
            MutexLock lock(&pFileSystem->mutex);
            FilePath path;
            path.setCompletePath(pTargetPath, false);
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->memoryFileDevice.allocateFileMountData(pFileSystem->pAllocator, createMemoryBlock((void*)pData, dataSize), path.getFileNameWithExtension());
            if (mountData.error != IoError_Ok)
            {
                return createMountIoResult(0xFFFFFFFFu, mountData.error);
            }
            return mountDeviceInternal(pFileSystem, pTargetPath, &pFileSystem->memoryFileDevice, mountData.result, true);
        }

        // 0x00C052F0 (:702)
        IoResult<uint32> mountMemory(FileSystem* pFileSystem, const char* pTargetPath, MemoryAllocator* pAllocator)
        {
            MutexLock lock(&pFileSystem->mutex);
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->memoryFileDevice.allocateDeviceMountData(pFileSystem->pAllocator, pAllocator);
            if (mountData.error != IoError_Ok)
            {
                return createMountIoResult(0xFFFFFFFFu, mountData.error);
            }
            return mountDeviceInternal(pFileSystem, pTargetPath, &pFileSystem->memoryFileDevice, mountData.result, true);
        }

        // 0x00C0B3D0 (:717)
        IoResult<uint32> mountPakFile(FileSystem* pFileSystem, const char* pTargetPath, const char* pPakFilePath)
        {
            MutexLock lock(&pFileSystem->mutex);
            const IoResult<FileDeviceMountData*> mountData = pFileSystem->pakFileDevice.allocateMountData(pFileSystem->pAllocator, pFileSystem, pPakFilePath);
            if (mountData.error != IoError_Ok)
            {
                return createMountIoResult(0xFFFFFFFFu, mountData.error);
            }
            return mountDeviceInternal(pFileSystem, pTargetPath, &pFileSystem->pakFileDevice, mountData.result, true);
        }

        // 0x00C02EE0 (:731), LTCG (pFileSystem EAX, mountId ECX)
        static IoError unmountInternal(FileSystem* pFileSystem, uint32 mountId)
        {
            FileSystemMountPoint* pMountPoint;
            FileSystemDeviceEntry* pEntry;
            getValuesFromFileMountHandle(pFileSystem, &pMountPoint, &pEntry, mountId);
            if (pMountPoint == nullptr || pEntry == nullptr)
            {
                return IoError_FileNotFound;
            }
            if (pEntry->pPrev != nullptr)
            {
                pEntry->pPrev->pNext = pEntry->pNext;
            }
            if (pEntry->pNext != nullptr)
            {
                pEntry->pNext->pPrev = pEntry->pPrev;
            }
            if (pMountPoint->pFirstDevice == pEntry)
            {
                pMountPoint->pFirstDevice = pEntry->pNext;
            }
            IoError error = IoError_Ok;
            if (pEntry->isInternalDevice)
            {
                error = pEntry->pDevice->freeMountData(pFileSystem->pAllocator, pEntry->pMountData);
            }
            pFileSystem->devices.freeObject(pEntry);
            freeMountPoint(pMountPoint, pFileSystem);
            return error;
        }

        // 0x00C02FB0 (:784)
        IoError unmount(FileSystem* pFileSystem, uint32 mountId)
        {
            MutexLock lock(&pFileSystem->mutex);
            return unmountInternal(pFileSystem, mountId);
        }

        // 0x00C02FF0 (:792)
        bool getNativeFilePath(char* pBuffer, uint32 bufferCapacity, FileSystem* pFileSystem, const char* pFileName)
        {
            String mountPath;
            String remainingPath;
            FileSystemPath path(pFileName);
            FileSystemMountPoint* pMountPoint;
            {
                MutexLock lock(&pFileSystem->mutex);
                pMountPoint = findMountPoint(&mountPath, &remainingPath, pFileSystem, path);
            }
            if (pMountPoint == nullptr)
            {
                return false;
            }
            char deviceFileName[260];
            copyString(deviceFileName, sizeof(deviceFileName), remainingPath.pStart, remainingPath.pEnd);
            for (FileSystemDeviceEntry* pEntry = pMountPoint->pFirstDevice; pEntry != nullptr; pEntry = pEntry->pNext)
            {
                if (pEntry->pDevice->getNativeFilePath(pBuffer, bufferCapacity, pEntry->pMountData, deviceFileName))
                {
                    return true;
                }
            }
            return false;
        }

        // 0x00C030F0 (:825)
        IoResult<uint32> openFile(FileSystem* pFileSystem, const char* pFileName, FileAccessMode accessMode)
        {
            String mountPath;
            String remainingPath;
            FileSystemPath path(pFileName);
            FileSystemMountPoint* pMountPoint;
            {
                MutexLock lock(&pFileSystem->mutex);
                pMountPoint = findMountPoint(&mountPath, &remainingPath, pFileSystem, path);
            }
            if (pMountPoint != nullptr)
            {
                char deviceFileName[260];
                copyString(deviceFileName, sizeof(deviceFileName), remainingPath.pStart, remainingPath.pEnd);
                pFileSystem->mutex.lock(0);
                for (FileSystemDeviceEntry* pEntry = pMountPoint->pFirstDevice; pEntry != nullptr; pEntry = pEntry->pNext)
                {
                    pFileSystem->mutex.unlock();
                    const IoResult<uint16> openResult = pEntry->pDevice->openFile(pEntry->pMountData, deviceFileName, accessMode);
                    if (openResult.error != IoError_FileNotFound)
                    {
                        // Any other error ends the search too, and is returned with a handle.
                        const uint16 deviceIndex = (uint16)pFileSystem->devices.getIndexFromObject(pEntry);
                        return createMountIoResult(createFileHandle(deviceIndex, openResult.result), openResult.error);
                    }
                    pFileSystem->mutex.lock(0);
                }
                pFileSystem->mutex.unlock();
            }
            return createMountIoResult(0xFFFFFFFFu, IoError_FileNotFound);
        }

        static FileDeviceInterface* getFileDevice(const FileSystem* pFileSystem, uint32 file)
        {
            return ((PoolAllocator<FileSystemDeviceEntry>&)pFileSystem->devices).getObjectFromIndex(getFileDeviceIndex(file))->pDevice;
        }

        // 0x00BF7B00 (:898)
        IoError closeFile(FileSystem* pFileSystem, uint32 file)
        {
            return getFileDevice(pFileSystem, file)->closeFile(getFileDeviceFileHandle(file));
        }

        // 0x00BF7B40 (:932)
        IoResult<uint32> read(FileSystem* pFileSystem, uint32 file, void* pTarget, uint32 size)
        {
            FileDeviceInterface* pDevice = getFileDevice(pFileSystem, file);
            // setIoControl(false) pauses reads (for up to 10 seconds).
            for (uint32 i = 0; pFileSystem->useIo == 0 && i < 1000; ++i)
            {
                Sleep(10);
            }
            return pDevice->read(getFileDeviceFileHandle(file), pTarget, size);
        }

        // 0x00BEFE10 (:953)
        void setIoControl(FileSystem* pFileSystem, bool useIo)
        {
            pFileSystem->useIo = useIo;
        }

        // 0x00BF7BD0 (:959)
        IoResult<uint32> write(FileSystem* pFileSystem, uint32 file, const void* pSource, uint32 size)
        {
            return getFileDevice(pFileSystem, file)->write(getFileDeviceFileHandle(file), pSource, size);
        }

        // 0x00BF7C20 (:967)
        IoError flushWriteBuffer(FileSystem* pFileSystem, uint32 file)
        {
            return getFileDevice(pFileSystem, file)->flushWriteBuffer(getFileDeviceFileHandle(file));
        }

        // 0x00BF7C60 (:975)
        IoResult<uint64> getFileSize(const FileSystem* pFileSystem, uint32 file)
        {
            return getFileDevice(pFileSystem, file)->getSize(getFileDeviceFileHandle(file));
        }

        // 0x00BF7CD0 (:983)
        IoError setFilePosition(FileSystem* pFileSystem, uint32 file, uint64 position)
        {
            return getFileDevice(pFileSystem, file)->setPosition(getFileDeviceFileHandle(file), position);
        }

        // 0x00BF7D10 (:991)
        IoResult<uint64> getFilePosition(const FileSystem* pFileSystem, uint32 file)
        {
            return getFileDevice(pFileSystem, file)->getPosition(getFileDeviceFileHandle(file));
        }

        // 0x00BF7D80 (:999)
        IoError moveFilePosition(FileSystem* pFileSystem, uint32 file, sint64 offset)
        {
            FileDeviceInterface* pDevice = getFileDevice(pFileSystem, file);
            const IoResult<uint64> position = pDevice->getPosition(getFileDeviceFileHandle(file));
            if (position.error != IoError_Ok)
            {
                return position.error;
            }
            const sint64 newPosition = (sint64)position.result + offset;
            if (newPosition < 0)
            {
                return IoError_InvalidPosition;
            }
            return pDevice->setPosition(getFileDeviceFileHandle(file), (uint64)newPosition);
        }

        // 0x00C05370 (:1016)
        bool doesFileExists(FileSystem* pFileSystem, const char* pFileName)
        {
            FileStatus status;
            return getFileStatusByName(&status, pFileSystem, pFileName, FileStatus::ValueMask_Exists) == IoError_Ok;
        }

        // 0x00C03230 (:1022)
        IoError getFileStatusByName(FileStatus* pStatus, FileSystem* pFileSystem, const char* pFileName, uint32 valueMask)
        {
            MutexLock lock(&pFileSystem->mutex);
            String mountPath;
            String remainingPath;
            FileSystemPath path(pFileName);
            FileSystemMountPoint* pMountPoint = findMountPoint(&mountPath, &remainingPath, pFileSystem, path);
            if (pMountPoint != nullptr)
            {
                char deviceFileName[260];
                copyString(deviceFileName, sizeof(deviceFileName), remainingPath.pStart, remainingPath.pEnd);
                for (FileSystemDeviceEntry* pEntry = pMountPoint->pFirstDevice; pEntry != nullptr; pEntry = pEntry->pNext)
                {
                    const IoError error = pEntry->pDevice->getFileStatusByName(pStatus, pEntry->pMountData, deviceFileName, valueMask);
                    if (error != IoError_FileNotFound && error != IoError_NotImplemented)
                    {
                        return error;
                    }
                }
            }
            return IoError_FileNotFound;
        }

        // 0x00C03350 (:1055)
        IoError setFilePermissionByName(FileSystem* pFileSystem, const char* pFileName, bool isWriteable)
        {
            MutexLock lock(&pFileSystem->mutex);
            String mountPath;
            String remainingPath;
            FileSystemPath path(pFileName);
            FileSystemMountPoint* pMountPoint = findMountPoint(&mountPath, &remainingPath, pFileSystem, path);
            if (pMountPoint != nullptr)
            {
                char deviceFileName[260];
                copyString(deviceFileName, sizeof(deviceFileName), remainingPath.pStart, remainingPath.pEnd);
                for (FileSystemDeviceEntry* pEntry = pMountPoint->pFirstDevice; pEntry != nullptr; pEntry = pEntry->pNext)
                {
                    const IoError error = pEntry->pDevice->setFilePermissionByName(pEntry->pMountData, deviceFileName, isWriteable);
                    if (error != IoError_FileNotFound && error != IoError_NotImplemented)
                    {
                        return error;
                    }
                }
            }
            return IoError_FileNotFound;
        }

        // 0x00BEFE30 (:1192)
        bool setIgnoreFirstChild(FileSystem* pFileSystem, bool value)
        {
            const bool oldValue = pFileSystem->ignoreFirstChild;
            pFileSystem->ignoreFirstChild = value;
            return oldValue;
        }
    }

    // 0x00BEFD50 (:146)
    FileDeviceInterface::~FileDeviceInterface()
    {
    }
}

namespace keen
{
    // 0x00C0E010
    FileSystemCreationParameters::FileSystemCreationParameters()
    {
        maxMountPoints = 0x20;
        maxMemoryStreamCount = 0x20;
        maxMemoryFileCount = 0x20;
        maxDeviceCount = 0x10;
        maxNativeStreamCount = 0x10;
        maxPakStreamCount = 4;
        maxAliasStreamCount = 4;
        maxPakFileSize = 550000000;
        nativeThreadBufferSize = 0x10000;
        enableDefaultMount = true;
        useReadThreads = false;
    }
}
