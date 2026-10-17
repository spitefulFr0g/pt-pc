#define VK_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>

static void record(const std::string& line, std::ofstream& log) {
    std::fprintf(stderr, "%s\n", line.c_str());
    if (log) { log << line << '\n'; log.flush(); }
}

static const char* resultName(XrResult result) {
    switch (result) {
        case XR_ERROR_RUNTIME_UNAVAILABLE: return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case XR_ERROR_RUNTIME_FAILURE: return "XR_ERROR_RUNTIME_FAILURE";
        case XR_ERROR_EXTENSION_NOT_PRESENT: return "XR_ERROR_EXTENSION_NOT_PRESENT";
        default: return "XR_ERROR_OTHER";
    }
}

int main(int argc, char** argv) {
    if (std::getenv("LD_LIBRARY_PATH") || std::getenv("LD_PRELOAD")) return 3;
    std::string logPath;
    bool vr = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--vr") == 0) vr = true;
        if (std::strcmp(argv[i], "--log") == 0 && i + 1 < argc) logPath = argv[++i];
    }
    std::ofstream log(logPath, std::ios::app);
    if (!vr) { record("fixture: VR flag missing", log); return 2; }
    void* loader = dlopen("libopenxr_loader.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!loader) { record("libopenxr_loader.so.1 was not found", log); return 0; }
    auto create = reinterpret_cast<PFN_xrCreateInstance>(dlsym(loader, "xrCreateInstance"));
    auto getProc = reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(loader, "xrGetInstanceProcAddr"));
    if (!create || !getProc) { record("the OpenXR loader has no xrCreateInstance", log); dlclose(loader); return 0; }
    const char* extensions[] = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    std::snprintf(info.applicationInfo.applicationName, sizeof(info.applicationInfo.applicationName), "%s", "P.T. (pt-port)");
    info.applicationInfo.applicationVersion = 1;
    std::snprintf(info.applicationInfo.engineName, sizeof(info.applicationInfo.engineName), "%s", "pt-port");
    info.applicationInfo.engineVersion = 1;
    info.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    info.enabledExtensionCount = 1;
    info.enabledExtensionNames = extensions;
    XrInstance instance = XR_NULL_HANDLE;
    const XrResult initialized = create(&info, &instance);
    if (XR_FAILED(initialized) || instance == XR_NULL_HANDLE) {
        const char* error = resultName(initialized);
        char message[128];
        std::snprintf(message, sizeof(message), "vr: off for this run: %s", error);
        record(message, log);
        dlclose(loader);
        return 0;
    }
    PFN_xrGetInstanceProperties getProperties = nullptr;
    PFN_xrDestroyInstance destroy = nullptr;
    getProc(instance, "xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction*>(&getProperties));
    getProc(instance, "xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction*>(&destroy));
    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    if (getProperties && XR_SUCCEEDED(getProperties(instance, &properties))) {
        record(std::string("vr: runtime ") + properties.runtimeName + " " +
               std::to_string(XR_VERSION_MAJOR(properties.runtimeVersion)) + "." +
               std::to_string(XR_VERSION_MINOR(properties.runtimeVersion)) + "." +
               std::to_string(XR_VERSION_PATCH(properties.runtimeVersion)), log);
    } else {
        record("OpenXR instance created but runtime properties were unavailable", log);
    }
    if (destroy) destroy(instance);
    dlclose(loader);
    return 0;
}
