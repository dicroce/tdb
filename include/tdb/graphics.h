
#ifndef __tdb_graphics_h
#define __tdb_graphics_h

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <utility>
#include <string.h>
#include <functional>

std::shared_ptr<std::vector<uint8_t>> ppm_buffer_to_argb(const uint8_t* p,
                                                         size_t len,
                                                         uint32_t& width,
                                                         uint32_t& height,
                                                         uint32_t& colorMax);

void argb_to_ppm_file(const std::string& fileName,
                      const std::shared_ptr<std::vector<uint8_t>> pixels,
                      uint32_t width,
                      uint32_t height);

void blit(std::shared_ptr<std::vector<uint8_t>> dest_pixels,
          uint32_t dest_width,
          uint32_t dest_height,
          uint32_t dest_x,
          uint32_t dest_y,
          std::shared_ptr<std::vector<uint8_t>> src_pixels,
          uint32_t src_width,
          uint32_t src_height,
          uint32_t src_x,
          uint32_t src_y,
          uint32_t width,
          uint32_t height);

void render_text(std::shared_ptr<std::vector<uint8_t>> pixels,
                 uint32_t width,
                 uint32_t height, 
                 const char* msg, 
                 int x,
                 int y);

void draw_point(std::shared_ptr<std::vector<uint8_t>> input,
                uint32_t w,
                uint32_t h,
                int x,
                int y,
                uint8_t r,
                uint8_t g,
                uint8_t b);

void draw_line(std::shared_ptr<std::vector<uint8_t>> input,
               uint32_t w,
               uint32_t h,
               int x1,
               int y1,
               int x2,
               int y2,
               uint8_t r,
               uint8_t g,
               uint8_t b);

void draw_line_rect(std::shared_ptr<std::vector<uint8_t>> input,
                    uint32_t w,
                    uint32_t h,
                    int x,
                    int y,
                    int rect_w,
                    int rect_h,
                    uint8_t r,
                    uint8_t g,
                    uint8_t b);


#endif
