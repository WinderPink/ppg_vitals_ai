#include "tflite_micro_model_config.h"
#include "em_device.h"
#include "sl_core.h"
#include "ml/platform/ml_clock_helper.h"

#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_accelerator.hpp"
#include "ml/accelerator/mvpv1/tflite_micro_mvpv1_context.hpp"
#include "ml/tflite_micro_model/tflite_micro_model_helper.hpp"
#include "ml/compiled_model/compiled_model_context.hpp"
#include "ml/accelerator/mvpv1/mvpv1_kernel_utils.hpp"


namespace npu_toolkit
{


const char* TfliteMicroMvpv1Accelerator::name() const
{
  return "mvpv1";
}

const char* TfliteMicroMvpv1Accelerator::description() const
{
  return "Matrix Vector Processor v1";
}

bool TfliteMicroMvpv1Accelerator::init()
{

  if(!_is_initialized)
  {
    ml_set_accelerator_clock_enabled(true);
    MVP->EN_SET = MVP_EN_EN;
    NVIC_EnableIRQ(MVP_IRQn);
    MVP->SWRST_SET = MVP_SWRST_SWRST;
    while(MVP->SWRST & MVP_SWRST_RESETTING)
    {
    }

    _is_initialized = true;
  }

  return true;
}

void TfliteMicroMvpv1Accelerator::deinit(TfLiteContext *context)
{
  auto compiled_context = (context != nullptr) ? CompiledModelContext::get<TfliteMicroMvpv1Context>(context) : nullptr;
  if(compiled_context != nullptr)
  {
    compiled_context->deinit();
  }

  MVP->EN_CLR = MVP_EN_EN;
  ml_set_accelerator_clock_enabled(false);

  _is_initialized = false;
}

TfliteMicroModelContext* TfliteMicroMvpv1Accelerator::create_context(
  TfLiteContext *context
)
{
  return TfliteMicroMvpv1Context::create(context);
}


bool TfliteMicroMvpv1Accelerator::is_model_compiled(TfLiteContext *context)
{
  return CompiledModelContext::get<TfliteMicroMvpv1Context>(context) != nullptr;
}

bool TfliteMicroMvpv1Accelerator::is_current_layer_compiled(TfLiteContext *context)
{
  auto compiled_context = CompiledModelContext::get<TfliteMicroMvpv1Context>(context);
  if(compiled_context == nullptr)
  {
    return false;
  }

  const auto accelerator_id = compiled_context->get_layer_accelerator(
      TfliteMicroModelHelper::current_layer_index(context)
  );

  return accelerator_id != CompiledAcceleratorId::NoAccelerator;
}


void TfliteMicroMvpv1Accelerator::prepare_compiled_execution()
{
  // Enable the MVP PROGDONE interrupt
  MVP->IF_CLR = MVP_IEN_PROGDONE|MVP_FAULT_MASK;
  MVP->IEN_SET = MVP_IEN_PROGDONE|MVP_FAULT_MASK;
}


#define ENABLE_CPU_CYCLE_COUNTER() DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk
#define DISABLE_CPU_CYCLE_COUNTER() DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk

void TfliteMicroMvpv1Accelerator::wait_for_idle()
{
  CORE_DECLARE_IRQ_STATE;
  while (!(MVP->STATUS & MVP_STATUS_IDLE))
  {
    CORE_ENTER_CRITICAL();
    if (!(MVP->STATUS & MVP_STATUS_IDLE))
    {
      DISABLE_CPU_CYCLE_COUNTER();
      //EMU_EnterEM1();
      __WFI();
      ENABLE_CPU_CYCLE_COUNTER();
    }
    CORE_EXIT_CRITICAL();
  }
}

void TfliteMicroMvpv1Accelerator::execute_program(bool wait_for_completion)
{
  ++program_counter;

  // Start the MVP program execution
  // NOTE: MVP_CMD_INIT is optionally included in the compiled "delta" registers
  MVP->CMD_SET = MVP_CMD_START;
  if(wait_for_completion)
  {
    wait_for_idle();
  }
}

void TfliteMicroMvpv1Accelerator::finalize_compiled_execution()
{
  // Disable the MVP PROGDONE interrupt
  MVP->IEN_CLR = MVP_IEN_PROGDONE|MVP_FAULT_MASK;
}



__WEAK TfliteMicroAccelerator* get_tflite_micro_accelerator()
{
  static TfliteMicroMvpv1Accelerator accelerator;
  return &accelerator;
}

__WEAK TfliteMicroAccelerator* register_tflite_micro_accelerator()
{
    return register_tflite_micro_accelerator(get_tflite_micro_accelerator());
}


} // namespace npu_toolkit
