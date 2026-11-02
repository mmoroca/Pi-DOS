/* Included by dos_mscdex.cpp. Linux USB CD-ROM with digital CD audio. */
#ifndef PI_DOS_USB_CDROM_BACKEND_H
#define PI_DOS_USB_CDROM_BACKEND_H
#if defined(LINUX) && !defined(OSFREE)
#include "pidos_usb_cdrom_player.h"

void CALLBACK_IdleNoInts();

namespace pidos_cdrom {
class Interface final : public CDROM_Interface {
    Player player;
    MixerChannel *channel = nullptr;
    uint64_t reported_epoch = 0;
    TCtrl control = {{0, 1, 2, 3}, {255, 255, 255, 255}};
    static Interface *&active() { static Interface *value = nullptr; return value; }
    bool run(std::function<bool()> job)
    {
        const auto old_if = GETFLAG(IF);
        SETFLAGBIT(IF, false);
        try {
            const bool result = player.run(std::move(job), [] { CALLBACK_IdleNoInts(); });
            SETFLAGBIT(IF, old_if);
            return result;
        } catch (...) { SETFLAGBIT(IF, old_if); throw; }
    }
    static void msf(uint32_t sector, TMSF &out, bool absolute = true)
    {
        if (absolute) sector += 150;
        out.min = sector / (60 * 75); out.sec = sector / 75 % 60; out.fr = sector % 75;
    }
    static void audio(Bitu frames)
    {
        Interface *self = active();
        if (!self || !self->channel) return;
        while (frames) {
            std::array<int16_t, 2048> samples = {};
            const size_t count = std::min(size_t(frames), size_t(1024));
            self->player.mix(samples.data(), count);
            for (size_t i = 0; i < count; ++i) {
                const int16_t input[2] = {samples[2 * i], samples[2 * i + 1]};
                for (unsigned ch = 0; ch < 2; ++ch)
                    samples[2 * i + ch] = self->control.out[ch] < 2 ?
                        int32_t(input[self->control.out[ch]]) * self->control.vol[ch] / 255 : 0;
            }
            self->channel->AddSamples_s16(count, samples.data());
            frames -= count;
        }
    }
public:
    Interface() { class_id = ID_USB; }
    ~Interface() override
    {
        if (channel) { channel->Enable(false); MIXER_DelChannel(channel); }
        if (active() == this) active() = nullptr;
    }
    bool SetDevice(const char *, int) override { return true; } // empty drives stay installed
    uint64_t MediaGeneration() const override { return player.epoch.load(); }
    bool IsAudioOnly() const override { return player.audio_only.load(); }
    bool GetMediaTrayStatus(bool &present, bool &changed, bool &open) override
    {
        run([&] { player.device.ready(false); present = player.device.present;
                  open = player.device.tray_open; return true; });
        const uint64_t epoch = MediaGeneration(); changed = epoch != reported_epoch; reported_epoch = epoch;
        return true;
    }
    bool GetUPC(unsigned char &attr, char *upc) override
    { attr = 0; return run([&] { return player.device.upc(upc); }); }
    bool GetAudioTracks(int &first, int &last, TMSF &leadout) override
    {
        return run([&] {
            if (!player.device.ready()) return false;
            first = player.device.tracks.front().number; last = player.device.tracks.back().number;
            msf(player.device.leadout, leadout); return true;
        });
    }
    bool GetAudioTrackInfo(int number, TMSF &start, unsigned char &attr) override
    {
        return run([&] {
            if (!player.device.ready()) return false;
            for (const auto &track : player.device.tracks) if (track.number == number) {
                msf(track.start, start); attr = track.attr; return true;
            }
            if (number == 0xaa) { msf(player.device.leadout, start); attr = 0; return true; }
            return false;
        });
    }
    bool GetAudioSub(unsigned char &attr, unsigned char &track, unsigned char &index, TMSF &rel, TMSF &abs) override
    {
        return run([&] {
            if (!player.device.ready()) return false;
            bool active, paused; uint32_t position;
            player.status(active, paused, position);
            const auto *entry = player.device.track(position);
            if (!entry) return false;
            attr = entry->attr; track = entry->number; index = 1;
            msf(position, abs); msf(position - entry->start, rel, false); return true;
        });
    }
    bool GetAudioStatus(bool &playing, bool &paused) override
    {
        return run([&] {
            player.device.ready(); uint32_t position;
            player.status(playing, paused, position); return player.device.present;
        });
    }
    bool PlayAudioSector(unsigned long start, unsigned long count) override
    {
        if (start > UINT32_MAX || count > UINT32_MAX) return false;
        if (!channel) {
            channel = MIXER_AddChannel(audio, 44100, "USBCD"); active() = this;
        }
        const bool ok = run([&] { return player.play(start, count); });
        channel->Enable(ok); return ok;
    }
    bool PauseAudio(bool resume) override
    { return run([&] { player.device.ready(); return player.pause(resume); }); }
    bool StopAudio() override
    { const bool ok = run([&] { return player.stop(); }); if (channel) channel->Enable(false); return ok; }
    void ChannelControl(TCtrl value) override { control = value; }
    bool ReadSectorsHost(void *buffer, bool raw, unsigned long sector, unsigned long count) override
    {
        if (sector > UINT32_MAX || count > UINT32_MAX - sector) return false;
        return run([&] {
            if (!count) return true;
            if (!player.device.ready(false)) return false;
            const uint64_t epoch = player.device.generation;
            auto *out = static_cast<uint8_t *>(buffer);
            for (unsigned long done = 0; done < count;) {
                const uint32_t chunk = std::min(count - done, 32ul);
                if (!player.device.read(out + done * (raw ? 2352 : 2048), raw, sector + done, chunk) ||
                    player.device.generation != epoch) return false;
                done += chunk;
            }
            return true;
        });
    }
    bool ReadSectors(PhysPt buffer, bool raw, unsigned long sector, unsigned long count) override
    {
        // Guest request count is 16-bit; bound allocations for direct callers.
        if (count > 65535) return false;
        std::vector<uint8_t> data(count * (raw ? 2352 : 2048));
        if (!ReadSectorsHost(data.data(), raw, sector, count)) return false;
        if (!data.empty()) MEM_BlockWrite(buffer, data.data(), data.size());
        return true;
    }
    bool LoadUnloadMedia(bool unload) override
    { return run([&] { player.stop(); return player.device.eject(unload); }); }
};
} // namespace pidos_cdrom
#endif
#endif
