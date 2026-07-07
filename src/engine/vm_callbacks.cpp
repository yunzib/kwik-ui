module kwik.engine.vm_callbacks;
import std;

static RenderCallback s_render = nullptr;
static IncrementalCallback s_incremental = nullptr;

void set_render_callback(RenderCallback cb) {
    s_render = std::move(cb);
}

void set_incremental_callback(IncrementalCallback cb) {
    s_incremental = cb;
}

RenderCallback get_render_callback() {
    return s_render;
}

IncrementalCallback get_incremental_callback() {
    return s_incremental;
}