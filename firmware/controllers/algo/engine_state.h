/**
 * @file engine_state.h
 * @brief One header which acts as gateway to current engine state
 *
 * @date Dec 20, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

#include "global.h"
#include "engine_parts.h"
#include "engine_state_generated.h"

class EngineState : public engine_state_s {
public:
	EngineState();

	void periodicFastCallback();
	void updateTChargeK(float rpm, float tps);

	void updateSparkSkip();

	/**
	 * always 360 or 720, never zero
	 */
	angle_t engineCycle{};

	bool useOddFireWastedSpark{};

	float injectionStage2Fraction{};

	Timer crankingTimer{};

	WarningCodeState warnings{};

	// Estimated airflow based on whatever airmass model is active
	float airflowEstimate{};

	float auxValveStart{};
	float auxValveEnd{};

	/**
	 * MAP averaging angle start, in relation to '0' trigger index index
	 */
	angle_t mapAveragingStart[MAX_CYLINDER_COUNT]{};
	angle_t mapAveragingDuration{};

	/**
	 * timing advance is angle distance before Top Dead Center (TDP), i.e. "10 degree timing advance" means "happens 10 degrees before TDC"
	 */
	angle_t timingAdvance[MAX_CYLINDER_COUNT]{};

	// Angle between firing the main (primary) spark and the secondary (trailing) spark
	angle_t trailingSparkAngle{};

	Timer timeSinceLastTChargeK{};

	/**
	 * Raw fuel injection duration produced by current fuel algorithm, without any correction
	 */
	floatms_t baseFuel{};

	/**
	 * TPS acceleration: extra fuel amount
	 */
	floatms_t tpsAccelEnrich{};

	/**
	 * Each individual fuel injection duration for current engine cycle, without wall wetting
	 * including everything including injector lag, both cranking and running
	 * @see getInjectionDuration()
	 */
	floatms_t injectionDuration{};
	floatms_t injectionDurationStage2{};

	angle_t injectionOffset{};

	multispark_state multispark{};

	bool shouldUpdateInjectionTiming{true};
};

EngineState * getEngineState();

bool getClutchDownState();
bool getBrakePedalState();
