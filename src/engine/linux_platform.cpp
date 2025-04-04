#include <cstdio>
#include <sys/mman.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "platform.h"
#include "core.h"


namespace platform {
b32 next_os_event(WindowInfo info, OSEvent* e) {
    /*MSG msg = {};
    e->kind = platform::OSEvent::Kind::none;
    SetWindowLongPtr((HWND)info.surface_handle, GWLP_USERDATA, (LONG_PTR)e);

    // Loop until we have an event we want to pass onto the game.
    while (e->kind == platform::OSEvent::Kind::none) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        return 0;
    }*/

    return 0;
}
	void get_framebuffer_size(platform::WindowInfo window_info, u32* width, u32* height) {
    GLFWwindow* window = (GLFWwindow*)window_info.window_handle;
	int iWidth, iHeight;
	glfwGetFramebufferSize(window, &iWidth, &iHeight);
	*width=iWidth;
	*height=iHeight;
}

[[nodiscard]] OSHandle open_file(const char* path) {
	auto file=fopen(path,"rb");
    return {file};
}

void close_file(OSHandle handle) {
    assert(handle.handle != nullptr);
	fclose((FILE*)handle.handle);
}

[[nodiscard]] u64 get_file_size(OSHandle file) {
	fseek((FILE*)file.handle, 0, SEEK_END); // seek to end of file
	u64 file_size = ftell((FILE*)file.handle); // get current file pointer
	fseek((FILE*)file.handle, 0, SEEK_SET); // seek back to beginning of file
    return file_size;
}

[[nodiscard]] u64 read_from_file(OSHandle file, Slice<u8> out_buffer) {
	auto result=fread_unlocked(out_buffer.ptr, out_buffer.len, 1, (FILE*)file.handle);
    return result;
}

[[noreturn]] void fatal(std::string_view msg) {
	printf("Fatal error occurred: %s\n",msg.data());
	exit(1);
}


[[nodiscard]] void* allocate(size_t length) {
	 void *mem = mmap(NULL, length, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	
    if (mem == nullptr) {
        fatal("Requested memory from the OS but recieved none. (Ran out of memory?)");
    }

    asan_poison_memory_region(mem, length);
    return mem;
}

void free(void* addr, size_t length) {
	munmap(addr,length);
    asan_poison_memory_region(addr, length);
}

}

