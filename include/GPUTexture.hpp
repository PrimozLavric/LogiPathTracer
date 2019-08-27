//
// Created by primoz on 27. 08. 19.
//

#ifndef LOGIPATHTRACER_GPUTEXTURE_HPP
#define LOGIPATHTRACER_GPUTEXTURE_HPP
#include <logi/logi.hpp>

struct GPUTexture {
  logi::VMAImage image;
  logi::ImageView imageView;
  logi::Sampler sampler;
};

#endif // LOGIPATHTRACER_GPUTEXTURE_HPP
