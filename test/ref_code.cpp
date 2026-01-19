class DiskReader {
    HANDLE hDisk;

public:
    DiskReader(const wchar_t* path) {
        hDisk = CreateFileW(
            path,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
            nullptr
        );
    }

    ~DiskReader() {
        CloseHandle(hDisk);
    }

    bool read(uint64_t offset, void* buf, size_t size) {
        OVERLAPPED ov{};
        ov.Offset     = DWORD(offset & 0xffffffff);
        ov.OffsetHigh = DWORD(offset >> 32);

        DWORD read = 0;
        BOOL ok = ReadFile(
            hDisk,
            buf,
            DWORD(size),
            &read,
            &ov
        );
        
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            ok = GetOverlappedResult(hDisk, &ov, &read, TRUE);
        }
        return ok && read == size;
    }
};

// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx


void hasher_worker(DiskReader& disk, const std::vector<DataRun>& runs) {
    constexpr size_t CHUNK = 1 << 20; // 1 MB
    std::vector<uint8_t> bufA(CHUNK);
    std::vector<uint8_t> bufB(CHUNK);

    size_t runIndex = 0;
    size_t offsetInRun = 0;

    // Prime first read
    disk.read(runs[0].offset, bufA.data(), std::min(CHUNK, runs[0].length));

    while (runIndex < runs.size()) {
        size_t toRead = std::min(CHUNK, runs[runIndex].length - offsetInRun);

        // Submit next read (read-ahead)
        if (offsetInRun + toRead < runs[runIndex].length) {
            disk.read(runs[runIndex].offset + offsetInRun + toRead,
                      bufB.data(), toRead);
        }

        hash(bufA.data(), toRead);

        std::swap(bufA, bufB);
        offsetInRun += toRead;

        if (offsetInRun == runs[runIndex].length) {
            runIndex++;
            offsetInRun = 0;
        }
    }
}



// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx




void hasher_worker(DiskReader& disk, const std::vector<DataRun>& runs) {
    constexpr size_t CHUNK = 1 << 20; // 1 MB

    std::vector<uint8_t> bufA(CHUNK);
    std::vector<uint8_t> bufB(CHUNK);

    size_t runIndex = 0;
    uint64_t offsetInRun = 0;

    size_t bytesInA = 0;
    size_t bytesInB = 0;

    // Prime first read
    {
        const DataRun& run = runs[0];
        bytesInA = std::min<uint64_t>(CHUNK, run.length);
        disk.read(run.offset, bufA.data(), bytesInA);
    }

    while (runIndex < runs.size()) {
        const DataRun& run = runs[runIndex];

        // Submit read-ahead
        if (offsetInRun + bytesInA < run.length) {
            uint64_t nextOffset = run.offset + offsetInRun + bytesInA;
            bytesInB = std::min<uint64_t>(CHUNK, run.length - (offsetInRun + bytesInA));
            disk.read(nextOffset, bufB.data(), bytesInB);
        } else {
            bytesInB = 0;
        }

        // Hash current buffer
        hash(bufA.data(), bytesInA);

        // Advance position
        offsetInRun += bytesInA;

        if (offsetInRun >= run.length) {
            runIndex++;
            offsetInRun = 0;

            if (runIndex < runs.size()) {
                const DataRun& nextRun = runs[runIndex];
                bytesInB = std::min<uint64_t>(CHUNK, nextRun.length);
                disk.read(nextRun.offset, bufB.data(), bytesInB);
            }
        }

        std::swap(bufA, bufB);
        std::swap(bytesInA, bytesInB);
    }
}



// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx



struct DataRun {
    uint64_t offset;  // absolute disk offset in bytes
    uint64_t length;  // length in bytes
    bool     sparse;  // true if sparse
};

class DiskReader {
public:
    bool read_at(uint64_t offset, void* buf, size_t size);
};

void hash(const uint8_t* data, size_t size);

void hasher_worker_3buf(DiskReader& disk, const std::vector<DataRun>& runs)
{
    constexpr size_t CHUNK = 1 << 20; // 1 MB

    std::vector<uint8_t> buf[3] = {
        std::vector<uint8_t>(CHUNK),
        std::vector<uint8_t>(CHUNK),
        std::vector<uint8_t>(CHUNK)
    };

    size_t buf_len[3] = {0, 0, 0};

    size_t cur = 0;   // buffer being hashed
    size_t next1 = 1; // 1st lookahead
    size_t next2 = 2; // 2nd lookahead

    size_t run_idx = 0;
    uint64_t off_in_run = 0;

    auto submit_read = [&](size_t b) {
        while (run_idx < runs.size()) {
            const DataRun& r = runs[run_idx];

            if (off_in_run >= r.length) {
                run_idx++;
                off_in_run = 0;
                continue;
            }

            uint64_t remain = r.length - off_in_run;
            size_t to_read = (size_t)std::min<uint64_t>(CHUNK, remain);

            if (r.sparse) {
                memset(buf[b].data(), 0, to_read);
            } else {
                disk.read_at(r.offset + off_in_run,
                             buf[b].data(),
                             to_read);
            }

            buf_len[b] = to_read;
            off_in_run += to_read;
            return;
        }

        buf_len[b] = 0; // no more data
    };

    /* Prime pipeline */
    submit_read(cur);
    submit_read(next1);
    submit_read(next2);

    while (buf_len[cur] != 0) {
        /* Hash current buffer */
        hash(buf[cur].data(), buf_len[cur]);

        /* Rotate buffers */
        size_t old = cur;
        cur = next1;
        next1 = next2;
        next2 = old;

        /* Refill the freed buffer */
        submit_read(next2);
    }
}
