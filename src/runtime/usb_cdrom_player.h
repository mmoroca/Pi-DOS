/* Serialized optical I/O and bounded CD-DA prefetch; no emulator dependencies. */
#ifndef PI_DOS_USB_CDROM_PLAYER_H
#define PI_DOS_USB_CDROM_PLAYER_H
#include "pidos_usb_cdrom_linux.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace pidos_cdrom {

class Player {
    static constexpr size_t frames_per_sector = 588, capacity = 32 * frames_per_sector;
    std::mutex mutex;
    std::condition_variable wake, completed;
    std::function<bool()> request;
    bool quit = false, done = false, result = false;
    std::exception_ptr failure;
    std::array<int16_t, capacity * 2> ring = {};
    size_t head = 0, buffered = 0;
    bool playing = false, paused = false;
    uint32_t next = 0, end = 0, start = 0;
    uint64_t played = 0, play_epoch = 0;
    std::thread worker;

    void stop_locked()
    { playing = paused = false; head = buffered = 0; }
    void publish()
    {
        epoch.store(device.generation);
        audio_only.store(device.present && std::none_of(device.tracks.begin(), device.tracks.end(),
                         [](const Track &t) { return (t.attr & 0x40) != 0; }));
        if (playing && device.generation != play_epoch) stop_locked();
    }
    void work()
    {
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            if (quit) return;
            if (request) {
                auto job = std::move(request);
                request = {};
                lock.unlock();
                try { result = job(); } catch (...) { failure = std::current_exception(); }
                lock.lock();
                publish(); done = true; completed.notify_one();
            } else if (playing && !paused && next < end && capacity - buffered >= 8 * frames_per_sector) {
                const uint32_t count = std::min(uint32_t(8), end - next), sector = next;
                std::array<uint8_t, 8 * 2352> data = {};
                lock.unlock();
                const bool ok = device.read(data.data(), true, sector, count, true);
                lock.lock();
                publish();
                if (!ok || !playing) { stop_locked(); continue; }
                // MMC/ATAPI READ CD returns little-endian signed stereo PCM.
                for (size_t i = 0; i < count * frames_per_sector; ++i) {
                    const size_t out = ((head + buffered + i) % capacity) * 2;
                    ring[out] = int16_t(unsigned(data[4 * i]) | unsigned(data[4 * i + 1]) << 8);
                    ring[out + 1] = int16_t(unsigned(data[4 * i + 2]) | unsigned(data[4 * i + 3]) << 8);
                }
                buffered += count * frames_per_sector; next += count;
                completed.notify_one();
            } else {
                wake.wait(lock);
            }
        }
    }
public:
    Device device; // touched only by worker jobs
    std::atomic<uint64_t> epoch{1};
    std::atomic<bool> audio_only{false};

    explicit Player(std::string sys = "/sys/class/block", std::string dev = "/dev")
        : device(std::move(sys), std::move(dev))
    { worker = std::thread([this] { work(); }); }
    ~Player()
    {
        { std::lock_guard<std::mutex> lock(mutex); quit = true; }
        wake.notify_one(); worker.join();
    }
    // Called by the emulator thread. idle must not permit DOS reentrancy.
    bool run(std::function<bool()> job, const std::function<void()> &idle)
    {
        std::unique_lock<std::mutex> lock(mutex);
        done = false; failure = nullptr; request = std::move(job); wake.notify_one();
        try {
            while (!done) {
                lock.unlock(); idle(); lock.lock();
            }
        } catch (...) {
            if (!lock.owns_lock()) lock.lock();
            completed.wait(lock, [this] { return done; });
            throw;
        }
        if (failure) std::rethrow_exception(failure);
        return result;
    }
    // Playback controls run as worker jobs, after checking media readiness.
    bool play(uint32_t sector, uint32_t count)
    {
        stop();
        if (!device.ready() || !device.audio_range(sector, count)) return false;
        std::lock_guard<std::mutex> lock(mutex);
        stop_locked(); start = next = sector; end = sector + count; played = 0;
        play_epoch = device.generation; playing = true;
        return true;
    }
    bool pause(bool resume)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!playing || device.generation != play_epoch || !device.present) { stop_locked(); return false; }
        paused = !resume;
        return true;
    }
    bool stop()
    { std::lock_guard<std::mutex> lock(mutex); stop_locked(); return true; }
    void status(bool &active, bool &pause, uint32_t &position)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (device.generation != play_epoch) stop_locked();
        active = playing; pause = paused; position = start + played / frames_per_sector;
    }
    // Mixer callback: copy available samples only. Never wait for hardware or
    // advance the reported position across an underrun; silence fills the gap.
    size_t mix(int16_t *samples, size_t frames)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!playing || paused) return 0;
        const size_t count = std::min(frames, buffered);
        for (size_t i = 0; i < count; ++i) {
            samples[2 * i] = ring[((head + i) % capacity) * 2];
            samples[2 * i + 1] = ring[((head + i) % capacity) * 2 + 1];
        }
        head = (head + count) % capacity; buffered -= count; played += count;
        if (!buffered && next == end) playing = false;
        wake.notify_one();
        return count;
    }
};
} // namespace pidos_cdrom
#endif
