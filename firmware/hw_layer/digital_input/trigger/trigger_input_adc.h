/**
 * @file	trigger_input_adc.h
 * @brief	Position sensor hardware layer, Using ADC and software comparator
 *
 * @date Jan 27, 2020
 * @author andreika <prometheus.pcb@gmail.com>
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

#include "global.h"
#include "trigger_input.h"

namespace TriggerAdcDetector {
	void init();
	void reset();

	void digitalCallback(efitick_t stamp, bool isPrimary, bool rise);
	void analogCallback(efitick_t stamp, triggerAdcSample_t value);

	void setWeakSignal(bool isWeak);

#if EFI_UNIT_TEST
	void setTriggerAdcMode(triggerAdcMode_t adcMode);
	triggerAdcMode_t getTriggerAdcMode();
	efidur_t getStampCorrectionForAdc();
#endif
};
