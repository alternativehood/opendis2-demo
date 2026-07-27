#include <SDL3/SDL.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstring>
#include <stdexcept>
#include <string>

namespace d2engine {

void configure_display_p3_output(SDL_Window* window, SDL_Renderer* renderer) {
    SDL_GPUDevice* device = SDL_GetGPURendererDevice(renderer);
    if (device == nullptr) {
        throw std::runtime_error("Display P3 requires an SDL GPU renderer");
    }

    const char* backend = SDL_GetGPUDeviceDriver(device);
    if (backend == nullptr || std::strcmp(backend, "metal") != 0) {
        throw std::runtime_error(
            std::string{"Display P3 requires the SDL GPU Metal backend; backend="} +
            (backend != nullptr ? backend : "unknown"));
    }

    const SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (props == 0) {
        throw std::runtime_error(std::string{"SDL_GetWindowProperties failed: "} + SDL_GetError());
    }

    auto* ns_window = reinterpret_cast<NSWindow*>(SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr));
    const NSInteger metal_view_tag = static_cast<NSInteger>(
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_COCOA_METAL_VIEW_TAG_NUMBER, 0));

    if (ns_window == nil || metal_view_tag == 0) {
        throw std::runtime_error("Could not locate the SDL GPU Metal view for Display P3 output");
    }

    NSView*       metal_view = [ns_window.contentView viewWithTag:metal_view_tag];
    CAMetalLayer* layer = [metal_view.layer isKindOfClass:[CAMetalLayer class]]
                              ? static_cast<CAMetalLayer*>(metal_view.layer)
                              : nil;
    if (layer == nil) {
        throw std::runtime_error("The SDL GPU Metal view does not expose a CAMetalLayer");
    }

    CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
    if (color_space == nullptr) {
        throw std::runtime_error("Could not create Display P3 color space");
    }

    layer.colorspace = color_space;
    CGColorSpaceRelease(color_space);
}

} // namespace d2engine
