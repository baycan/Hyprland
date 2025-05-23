#include "SyncReleaser.hpp"
#include "SyncTimeline.hpp"
#include "../../render/OpenGL.hpp"
#include <sys/ioctl.h>

#if defined(__linux__)
#include <linux/sync_file.h>
#else
struct sync_merge_data {
    char  name[32];
    __s32 fd2;
    __s32 fence;
    __u32 flags;
    __u32 pad;
};
#define SYNC_IOC_MAGIC '>'
#define SYNC_IOC_MERGE _IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)
#endif

using namespace Hyprutils::OS;

CSyncReleaser::CSyncReleaser(SP<CSyncTimeline> timeline, uint64_t point) : m_timeline(timeline), m_point(point) {
    ;
}

CSyncReleaser::~CSyncReleaser() {
    if (!m_timeline) {
        Debug::log(ERR, "CSyncReleaser destructing without a timeline");
        return;
    }

    if (m_fd.isValid())
        m_timeline->importFromSyncFileFD(m_point, m_fd);
    else
        m_timeline->signal(m_point);
}

static CFileDescriptor mergeSyncFds(const CFileDescriptor& fd1, const CFileDescriptor& fd2) {
    // combines the fences of both sync_fds into a dma_fence_array (https://www.kernel.org/doc/html/latest/driver-api/dma-buf.html#c.dma_fence_array_create)
    // with the signal_on_any param set to false, so the new sync_fd will "signal when all fences in the array signal."

    struct sync_merge_data data{
        .name  = "merged release fence",
        .fd2   = fd2.get(),
        .fence = -1,
    };
    int err = -1;
    do {
        err = ioctl(fd1.get(), SYNC_IOC_MERGE, &data);
    } while (err == -1 && (errno == EINTR || errno == EAGAIN));
    if (err < 0)
        return CFileDescriptor{};
    else
        return CFileDescriptor(data.fence);
}

void CSyncReleaser::addSyncFileFd(const Hyprutils::OS::CFileDescriptor& syncFd) {
    if (m_fd.isValid())
        m_fd = mergeSyncFds(m_fd, syncFd);
    else
        m_fd = syncFd.duplicate();
}

void CSyncReleaser::drop() {
    m_timeline.reset();
}
