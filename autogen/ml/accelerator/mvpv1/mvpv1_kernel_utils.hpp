#include "tflite_micro_model_config.h"
#pragma once

#include <cassert>
#include <algorithm>


#include "em_device.h"
#include "ml/third_party/tflm/common.h"
#include "ml/third_party/float16/float16.hpp"


namespace npu_toolkit
{

// Selected values give range up to int32 but not larger, so that final_scale_factor doesn't overflow
//  power-of-two chosen so that adds 0 error during accumulation
// Note: could generate a value based on filter_count to maximize range or even handle extremely large filter counts
constexpr const float ACCUMULATOR_MULTIPLIER = 1 << 15;
constexpr const float ACCUMULATOR_SCALER =  1.0f / ACCUMULATOR_MULTIPLIER;

constexpr const float FP16_MIN = -65504.0f;
constexpr const float FP16_MAX = 65504.0f;
constexpr const float FP16_SMALLEST = 0.000000059605f;

constexpr const uint32_t MVP_FAULT_MASK = (
    MVP_IF_LOOPFAULT | MVP_IF_BUSERRFAULT  | \
    MVP_IF_BUSALIGNFAULT | MVP_IF_ALUFAULT | \
    MVP_IF_ARRAYFAULT
  );


TfLiteStatus populate_convolution_quantization_params(
  TfLiteContext* context,
  const TfLiteTensor* input,
  const TfLiteTensor* filter,
  const TfLiteTensor* output,
  float16_t* per_channel_scalers,
  int num_channels,
  float accumulator_multipler
);


TfLiteStatus allocate_output_multiplier_buffer(
  TfLiteContext* context,
  const TfLiteTensor* input,
  const TfLiteTensor* filters,
  const TfLiteTensor* output,
  int n_channels,
  float16_t** buffer
);


TfLiteStatus allocate_scaled_bias_tensor(
  TfLiteContext* context,
  const TfLiteTensor* bias,
  float16_t** scaled_tensor_ptr
);


template<typename T>
inline T clamp(T f, T min, T max)
{
    return std::min(std::max(f, min), max);
}

inline float16_t normalize_fp16(float f)
{
    return (float16_t)clamp(f, FP16_MIN, FP16_MAX);
}



} // namespace npu_toolkit