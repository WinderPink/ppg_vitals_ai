#include "tflite_micro_model_config.h"
#include "ml/third_party/tflm/tlmk-kernel_util.h"

#include "ml/accelerator/mvpv1/mvpv1_kernel_utils.hpp"
#include "ml/tflite_micro_model/tflite_micro_logger.hpp"


namespace npu_toolkit
{

TfLiteStatus populate_convolution_quantization_params(
  TfLiteContext* context,
  const TfLiteTensor* input,
  const TfLiteTensor* filter,
  const TfLiteTensor* output,
  float16_t* per_channel_scalers,
  int num_channels,
  float accumulator_multiplier
)
{
    auto affine_quantization =
        reinterpret_cast<const TfLiteAffineQuantization*>(filter->quantization.params);

  // Populate multiplier and shift using affine quantization.
  const double input_scale = input->params.scale;
  const double output_scale = output->params.scale;
  const float* filter_scales = affine_quantization->scale->data;
  NPU_TOOLKIT_ENSURE(context, num_channels == affine_quantization->scale->size);

  for (int i = 0; i < num_channels; ++i)
  {
    // If per-tensor quantization parameter is specified, broadcast it along the
    // quantization dimension (channels_out).
    const double filter_scale = filter_scales[i];
    const double effective_output_scale = (input_scale * filter_scale) / output_scale;
    const float acc_output_scale = effective_output_scale * accumulator_multiplier;
    per_channel_scalers[i] = normalize_fp16(acc_output_scale);
  }

  return kTfLiteOk;
}

TfLiteStatus allocate_scaled_bias_tensor(
  TfLiteContext* context,
  const TfLiteTensor* bias,
  float16_t** scaled_tensor_ptr
)
{
  if(bias != nullptr && bias->type == kTfLiteInt32)
  {
    const int bias_count = bias->bytes / sizeof(int32_t);
    auto scaled_bias_tensor = NPU_TOOLKIT_ALLOCATE_PERSISTENT_BUFFER(float16_t, bias_count);
    if(scaled_bias_tensor == nullptr)
    {
      return kTfLiteError;
    }

    auto src = tflite::GetTensorData<int32_t>(bias);
    auto dst = scaled_bias_tensor;
    for(int i = bias_count; i > 0; --i)
    {
        const auto b = *src++;
        *dst++ = float16_t(b * npu_toolkit::ACCUMULATOR_SCALER);
    }


    *scaled_tensor_ptr = scaled_bias_tensor;
  }
  else
  {
    *scaled_tensor_ptr = nullptr;
  }

  return kTfLiteOk;
}

TfLiteStatus allocate_output_multiplier_buffer(
  TfLiteContext* context,
  const TfLiteTensor* input,
  const TfLiteTensor* filters,
  const TfLiteTensor* output,
  int n_channels,
  float16_t** buffer
)
{
  bool is_paged = false;

  *buffer = NPU_TOOLKIT_ALLOCATE_PLANNED_PERSISTENT_BUFFER("output_multiplier", float16_t, n_channels, &is_paged);
  NPU_TOOLKIT_ENSURE(context, *buffer != nullptr);

  if(!is_paged)
  {
    TF_LITE_ENSURE_STATUS(npu_toolkit::populate_convolution_quantization_params(
        context, input, filters, output,
        *buffer,
        n_channels, npu_toolkit::ACCUMULATOR_MULTIPLIER
    ));
  }

  return kTfLiteOk;
}



}