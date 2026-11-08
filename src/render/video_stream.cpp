/**
 * @file video_stream.cpp
 * @brief 视频流数据层实现 — 槽池状态机 / 布局计算 / 命名注册表
 */
module;
#include <cstdlib>
#include <cstring>
#include <unordered_map>

module kwik.render.video_stream;

import std;

// ── 4096 对齐分配（批 3 外部内存导入的页对齐前提）──
namespace {
void *alignedAlloc(size_t bytes) {
#ifdef _WIN32
    return _aligned_malloc(bytes, 4096);
#else
    void *p = nullptr;
    if (posix_memalign(&p, 4096, bytes) != 0) return nullptr;
    return p;
#endif
}
void alignedFree(void *p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}
}    // namespace

// ═══════════════════════════════════════════════════════════════════
// FrameWriter — 生产者写帧句柄
// ═══════════════════════════════════════════════════════════════════
FrameWriter::~FrameWriter() {
    if (stream_ && !committed_) stream_->abortWrite(idx_);    // 未 commit → 槽归还（本帧丢弃）
}
FrameWriter::FrameWriter(FrameWriter &&o) noexcept : stream_(o.stream_), idx_(o.idx_), committed_(o.committed_) {
    o.stream_ = nullptr;    // 移交后源句柄不再触发 abort/重复 commit
}
FrameWriter &FrameWriter::operator=(FrameWriter &&o) noexcept {
    if (this != &o) {
        if (stream_ && !committed_) stream_->abortWrite(idx_);
        stream_ = o.stream_;
        idx_ = o.idx_;
        committed_ = o.committed_;
        o.stream_ = nullptr;
    }
    return *this;
}
std::span<const PlaneView> FrameWriter::planes() const {
    if (!stream_) return {};
    return stream_->planesFor(idx_);
}
void FrameWriter::commit() {
    if (stream_ && !committed_) {
        committed_ = true;
        stream_->commitSlot(idx_);
    }
}

// ═══════════════════════════════════════════════════════════════════
// PinGuard — 消费端固定句柄
// ═══════════════════════════════════════════════════════════════════
PinGuard::~PinGuard() {
    if (stream_) stream_->unpin(idx_);
}
PinGuard::PinGuard(PinGuard &&o) noexcept : stream_(o.stream_), idx_(o.idx_), seq_(o.seq_) {
    o.stream_ = nullptr;
}
PinGuard &PinGuard::operator=(PinGuard &&o) noexcept {
    if (this != &o) {
        if (stream_) stream_->unpin(idx_);
        stream_ = o.stream_;
        idx_ = o.idx_;
        seq_ = o.seq_;
        o.stream_ = nullptr;
    }
    return *this;
}
std::span<const PlaneView> PinGuard::planes() const {
    if (!stream_) return {};
    return stream_->planesFor(idx_);
}
const FrameDesc &PinGuard::desc() const {
    static const FrameDesc kEmpty{};
    if (!stream_) return kEmpty;
    return stream_->descFor(idx_);
}

// ═══════════════════════════════════════════════════════════════════
// VideoStream — 槽池 + 最新帧指针
// ═══════════════════════════════════════════════════════════════════
VideoStream::VideoStream(uint32_t poolSlots) {
    slots_.resize(std::max<uint32_t>(poolSlots, 2));    // 至少 2：一帧在显 + 一帧在写
}
VideoStream::~VideoStream() {
    for (auto &s : slots_)
        if (s.base) alignedFree(s.base);
}

// 槽视图/描述读取：Writing（生产者独占）或被固定（pins>0 期间不被复用）时
// 字段稳定，调用方均处于这两态之一，故无需加锁
std::span<const PlaneView> VideoStream::planesFor(size_t idx) const {
    return std::span<const PlaneView>(slots_[idx].views.data(), FrameLayout::forDesc(slots_[idx].desc).planeCount);
}
const FrameDesc &VideoStream::descFor(size_t idx) const { return slots_[idx].desc; }

FrameWriter VideoStream::acquire(const FrameDesc &d) {
    if (d.width == 0 || d.height == 0) return {};
    FrameLayout L = FrameLayout::forDesc(d);
    if (L.planeCount == 0) return {};    // 非法描述（如 4:2:2 奇宽）→ 空句柄即丢帧

    std::lock_guard<std::mutex> lk(mtx_);
    // ① 优先空闲槽；② 无空闲则回收「未被固定且非最新」的 Ready 槽（最新帧优先，
    //    保证显示端总能拿到最新画面而非排队旧帧）；③ 全忙 → 丢帧不阻塞
    int target = -1;
    for (size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].st == Slot::St::Free) { target = (int)i; break; }
    if (target < 0)
        for (size_t i = 0; i < slots_.size(); ++i) {
            auto &s = slots_[i];
            if (s.st == Slot::St::Ready && s.pins == 0 && (int)i != latest_) { target = (int)i; break; }
        }
    if (target < 0) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return {};
    }
    auto &s = slots_[target];
    if (s.capBytes < L.totalBytes) {    // 容量不足（首帧/换分辨率）→ 重分配
        if (s.base) alignedFree(s.base);
        s.base = static_cast<uint8_t *>(alignedAlloc(L.totalBytes));
        s.capBytes = s.base ? L.totalBytes : 0;
        if (!s.base) return {};    // 分配失败按丢帧处理
    }
    s.st = Slot::St::Writing;
    s.desc = d;
    for (uint32_t p = 0; p < L.planeCount; ++p) {
        const auto &pl = L.planes[p];
        s.views[p] = PlaneView{s.base + pl.offset, pl.stride, pl.width, pl.height};
    }
    return FrameWriter{this, (size_t)target};
}

void VideoStream::push(const FrameDesc &d, std::span<const PlaneView> src) {
    auto w = acquire(d);
    if (!w) return;
    auto dst = w.planes();
    for (uint32_t p = 0; p < dst.size() && p < src.size(); ++p) {
        const auto &dv = dst[p];
        const auto &sv = src[p];
        if (!sv.data || sv.height == 0) continue;
        uint32_t rows = std::min(dv.height, sv.height);
        uint32_t rowBytes = std::min(dv.stride, sv.stride);    // 拷行内容字节（stride 差由两端各自行距消化）
        for (uint32_t y = 0; y < rows; ++y)
            std::memcpy(dv.data + size_t(dv.stride) * y, sv.data + size_t(sv.stride) * y, rowBytes);
    }
    w.commit();
}

void VideoStream::commitSlot(size_t idx) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto &s = slots_[idx];
    s.st = Slot::St::Ready;
    // 最新帧优先：上一个未被固定的 Ready 帧即时作废（还池待复用）
    if (latest_ >= 0 && latest_ != (int)idx) {
        auto &old = slots_[latest_];
        if (old.st == Slot::St::Ready && old.pins == 0) old.st = Slot::St::Free;
    }
    latest_ = (int)idx;
    hasFrame_ = true;
    latestDesc_ = s.desc;
    // seq 递增须与 latest_ 切换同处锁内：pinLatest 锁内读二者，保证
    // 固定槽与其序号一一对应（锁外递增会出现"新槽配旧序号"→ 下帧重复上传）。
    // UI 线程无锁读 seq（acquire）照常成立
    seq_.fetch_add(1, std::memory_order_release);
}

void VideoStream::abortWrite(size_t idx) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (slots_[idx].st == Slot::St::Writing) slots_[idx].st = Slot::St::Free;
}

void VideoStream::unpin(size_t idx) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto &s = slots_[idx];
    if (s.pins > 0) --s.pins;
    // 解除固定后：非最新的 Ready 帧（已被更新的帧替代）即时还池
    if (s.pins == 0 && s.st == Slot::St::Ready && (int)idx != latest_) s.st = Slot::St::Free;
}

FrameDesc VideoStream::desc() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return hasFrame_ ? latestDesc_ : FrameDesc{};
}

PinGuard VideoStream::pinLatest() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (latest_ < 0) return {};
    auto &s = slots_[latest_];
    ++s.pins;
    return PinGuard{this, (size_t)latest_, seq_.load(std::memory_order_acquire)};
}

// ═══════════════════════════════════════════════════════════════════
// 命名注册表 — name → weak_ptr（持有者续命，注册表不延长流寿命）
// ═══════════════════════════════════════════════════════════════════
namespace {
std::mutex &regMutex() {
    static std::mutex m;
    return m;
}
std::unordered_map<std::string, std::weak_ptr<VideoStream>> &registry() {
    static std::unordered_map<std::string, std::weak_ptr<VideoStream>> r;
    return r;
}
}    // namespace

std::shared_ptr<VideoStream> createVideoStream(std::string name, uint32_t poolSlots) {
    auto s = std::make_shared<VideoStream>(poolSlots);
    std::lock_guard<std::mutex> lk(regMutex());
    registry()[std::move(name)] = s;    // 同名覆盖：旧流由既有持有者续命
    return s;
}

std::shared_ptr<VideoStream> findVideoStream(std::string_view name) {
    std::lock_guard<std::mutex> lk(regMutex());
    auto it = registry().find(std::string(name));
    if (it == registry().end()) return nullptr;
    return it->second.lock();    // weak → shared：找不到（已除名）返回空
}

void removeVideoStream(std::string_view name) {
    std::lock_guard<std::mutex> lk(regMutex());
    registry().erase(std::string(name));
}
