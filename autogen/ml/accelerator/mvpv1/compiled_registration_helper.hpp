#include "tflite_micro_model_config.h"
#pragma once


#include "ml/third_party/tflm/common.h"


namespace npu_toolkit
{
typedef TfLiteStatus (*KernelPrepare)(TfLiteContext* context, TfLiteNode* node);
typedef TfLiteStatus (*KernelInvoke)(TfLiteContext* context, TfLiteNode* node);

void* compiled_registration_init(
  TfLiteContext* context,
  const char* buffer,
  size_t length,
  int init_size,
  int fallback_init_size = 0
);

TfLiteStatus compiled_registration_prepare(
  TfLiteContext* context,
  TfLiteNode* node,
  KernelPrepare prepare,
  KernelPrepare fallback_prepare = nullptr
);

TfLiteStatus compiled_registration_invoke(
  TfLiteContext* context,
  TfLiteNode* node,
  KernelInvoke invoke,
  KernelInvoke fallback_invoke = nullptr
);


} // namespace npu_toolkit