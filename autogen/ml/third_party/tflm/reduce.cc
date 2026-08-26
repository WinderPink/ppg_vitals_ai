#include "tflite_micro_model_config.h"
/* Copyright 2025 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "ml/third_party/tflm/reduce.h"

#include "ml/third_party/tflm/tlc-builtin_op_data.h"
#include "ml/third_party/tflm/common.h"
#include "ml/third_party/tflm/quantization_util.h"
#include "ml/third_party/tflm/mean.h"
#include "ml/third_party/tflm/tensor_ctypes.h"
#include "ml/third_party/tflm/types.h"
#include "ml/third_party/tflm/kernel_util.h"
#include "ml/third_party/tflm/tlmk-kernel_util.h"
#include "ml/third_party/tflm/tlmk-reduce.h"
#include "ml/third_party/tflm/micro_utils.h"

namespace tflite {

void* InitReduce(TfLiteContext* context, const char* buffer, size_t length) {
  void* op_data =
      context->AllocatePersistentBuffer(context, sizeof(OpDataReduce));
  return new (op_data) OpDataReduce();
}

TfLiteStatus PrepareMinMax(TfLiteContext* context, TfLiteNode* node) {
  return PrepareMinMaxHelper(context, node,
                             static_cast<OpDataReduce*>(node->user_data));
}

TfLiteStatus PrepareMeanOrSum(TfLiteContext* context, TfLiteNode* node) {
  return PrepareMeanOrSumHelper(context, node,
                                static_cast<OpDataReduce*>(node->user_data));
}

TfLiteStatus EvalMean(TfLiteContext* context, TfLiteNode* node) {
  return EvalMeanHelper(context, node,
                        static_cast<OpDataReduce*>(node->user_data));
}

TfLiteStatus EvalMax(TfLiteContext* context, TfLiteNode* node) {
  OpDataReduce* op_data = static_cast<OpDataReduce*>(node->user_data);
  return EvalMaxHelper(context, node, op_data);
}

TfLiteStatus EvalMin(TfLiteContext* context, TfLiteNode* node) {
  OpDataReduce* op_data = static_cast<OpDataReduce*>(node->user_data);
  return EvalMinHelper(context, node, op_data);
}

TfLiteStatus EvalSum(TfLiteContext* context, TfLiteNode* node) {
  return EvalSumHelper(context, node,
                       static_cast<OpDataReduce*>(node->user_data));
}

#if !defined(TFLITE_MICRO_SW_REF_KERNEL_REGISTRATIONS_DISABLED) && !defined(TFLITE_MICRO_SW_REF_MEAN_KERNEL_REGISTRATION_DISABLED)
TFLMRegistration Register_MEAN() {
  return tflite::micro::RegisterOp(InitReduce, PrepareMeanOrSum, EvalMean);
}
#endif

TFLMRegistration Register_REDUCE_MAX() {
  return tflite::micro::RegisterOp(InitReduce, PrepareMinMax, EvalMax);
}

TFLMRegistration Register_REDUCE_MIN() {
  return tflite::micro::RegisterOp(InitReduce, PrepareMinMax, EvalMin);
}

TFLMRegistration Register_SUM() {
  return tflite::micro::RegisterOp(InitReduce, PrepareMeanOrSum, EvalSum);
}

}  // namespace tflite
