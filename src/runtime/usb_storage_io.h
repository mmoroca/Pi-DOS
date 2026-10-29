/* Serialized host I/O; the caller supplies emulator-safe display processing. */
#ifndef PI_DOS_USB_STORAGE_IO_H
#define PI_DOS_USB_STORAGE_IO_H
#include <cerrno>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace pidos_storage {
using HostOperation = std::function<int()>;
using HostRunner = int (*)(HostOperation);
inline HostRunner &io_runner() { static HostRunner runner = nullptr; return runner; }
inline int host_io(HostOperation operation)
{ return io_runner() ? io_runner()(std::move(operation)) : operation(); }

class IoWorker {
    std::mutex mutex;
    std::condition_variable wake, completed;
    HostOperation request;
    bool quit = false, done = false;
    int result = 0, error = 0;
    std::exception_ptr failure;
    std::thread worker;
    void work()
    {
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            wake.wait(lock, [this] { return quit || bool(request); });
            if (quit) return;
            auto operation = std::move(request);
            request = {};
            lock.unlock();
            int value = -1;
            std::exception_ptr exception;
            try { value = operation(); } catch (...) { exception = std::current_exception(); }
            const int saved_errno = errno;
            lock.lock();
            result = value; error = saved_errno; failure = exception;
            done = true;
            completed.notify_one();
        }
    }
public:
    IoWorker() : worker([this] { work(); }) {}
    ~IoWorker()
    {
        { std::lock_guard<std::mutex> lock(mutex); quit = true; }
        wake.notify_one(); worker.join();
    }
    int run(HostOperation operation, const std::function<void()> &idle)
    {
        std::unique_lock<std::mutex> lock(mutex);
        done = false; failure = nullptr; request = std::move(operation);
        wake.notify_one();
        try {
            while (!done) {
                lock.unlock(); idle(); lock.lock();
            }
        } catch (...) {
            // Quit/reset must not destroy FILEs, borrowed data or stack captures
            // until the host operation has actually finished.
            if (!lock.owns_lock()) lock.lock();
            completed.wait(lock, [this] { return done; });
            throw;
        }
        if (failure) std::rethrow_exception(failure);
        errno = error;
        return result;
    }
};
} // namespace pidos_storage
#endif
