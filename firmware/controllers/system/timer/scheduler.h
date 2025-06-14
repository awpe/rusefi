/**
 * @file	scheduler.h
 *
 * @date May 18, 2014
 * @author Andrey Belomutskiy, (c) 2012-2020
 */
#pragma once

#if EFI_UNIT_TEST
#include <cassert>
#include <rusefi/rusefi_time_math.h>
#endif

template<class To, class From>
std::enable_if_t<
    sizeof(To) == sizeof(From) &&
    std::is_trivially_copyable_v<From> &&
    std::is_trivially_copyable_v<To>,
    To>
// constexpr support needs compiler magic
bit_cast(const From& src) noexcept
{
    static_assert(std::is_trivially_constructible_v<To>,
        "This implementation additionally requires "
        "destination type to be trivially constructible");

    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

template<typename T>
struct TaggedPointer {
	static_assert(alignof(T) >= 2, "Type must be at least 2-aligned");
	static_assert(sizeof(uintptr_t) == sizeof(void*), "Unexpected uintptr_t size");

	static constexpr TaggedPointer fromRaw(uintptr_t raw) {
		return TaggedPointer{raw};
	}

	template<typename U = T>
	static constexpr TaggedPointer make(U ptr, const bool flag) requires std::is_pointer_v<U> {
#if EFI_UNIT_TEST
		assert((reinterpret_cast<uintptr_t>(ptr) & 1) == 0);
#endif
		return TaggedPointer{reinterpret_cast<uintptr_t>(ptr) | (flag ? 1 : 0)};
	}

	[[nodiscard]] constexpr T* getOriginalPointer() const {
		return reinterpret_cast<T*>(m_raw & ~1UL);
	}

	[[nodiscard]] constexpr bool getFlag() const {
		return m_raw & 1;
	}

	[[nodiscard]] constexpr uintptr_t getRaw() const {
		return m_raw;
	}

	explicit TaggedPointer(uintptr_t const raw) noexcept : m_raw{raw} { }

	TaggedPointer() noexcept = default;

	TaggedPointer(TaggedPointer const&) noexcept = default;
	TaggedPointer(TaggedPointer&& other) noexcept : m_raw{other.m_raw} {
		other.m_raw = 0;
	}

	TaggedPointer& operator=(TaggedPointer const&) noexcept = default;
	TaggedPointer& operator=(TaggedPointer&& other) noexcept {
		m_raw = other.m_raw;
		other.m_raw = 0;
		return *this;
	}

private:
	uintptr_t m_raw{};
};

// Regarding overhead, starting from GCC 10.1 through gcc 15 and clang 15-20 you have everything on -O0
// but at -O1 there are not even constructors in ASM...
// Few more ASM instructions on minGw but due to CRT in WIN requiring some stack alignment by Windows OS
class action_s {
public:
	using schfunc_t = void (*)(uintptr_t);

	action_s() noexcept = default;

	action_s(action_s const&) noexcept = default;
	action_s(action_s&& other) noexcept : m_callback{other.m_callback}, m_param{other.m_param}
	#if EFI_UNIT_TEST
	, fn_name {other.fn_name}
	#endif
	{
		other.m_callback = nullptr;
		other.m_param = 0;
		#if EFI_UNIT_TEST
			other.fn_name = "Moved";
		#endif
	}

	action_s& operator=(action_s const&) noexcept = default;
	action_s& operator=(action_s&& other) noexcept {
		m_callback = other.m_callback;
		m_param = other.m_param;
		other.m_callback = nullptr;
		other.m_param = 0;
		#if EFI_UNIT_TEST
			fn_name = other.fn_name;
			other.fn_name = "Moved";
		#endif
		return *this;
	}

	// Factory: wraps a typed function (integral or pointer) with the unified signature
	template<auto Func, typename Arg>
	static constexpr action_s make(Arg arg) noexcept(std::is_nothrow_constructible_v<action_s, schfunc_t, uintptr_t>) {
		static_assert(std::is_invocable_r_v<void, decltype(Func), Arg>, "Function signature mismatch");
		if constexpr (std::is_pointer_v<Arg>) {
			// alignment is a must because pointers have to be dividable by 2 with no loss (i.e., no loss on bit shifts)
			// if it is 1-aligned, there will be address data in the last bit, so it cannot be used for any flags
			// There could be theoretic architectures where it is the case though I don't know what to do then,
			// Maybe introduce some other member for flag here?
			// Anyway, we have so much code relying on specific architecture, and we are not testing on some fancy
			// PowerPC/mainframe/... machines so should always be true, but just in case...
			static_assert(alignof(std::remove_pointer_t<Arg>) >= 2, "Pointer must be at least 2-aligned");
			static_assert(sizeof(uintptr_t) == sizeof(void*), "Unexpected uintptr_t size");
		}
		#ifdef EFI_UNIT_TEST
			return action_s(&trampoline<Func, Arg>, to_uintptr(arg), __PRETTY_FUNCTION__);
		#else
			return action_s(&trampoline<Func, Arg>, to_uintptr(arg));
		#endif
	}

	// For functions with no arguments
	template<auto Func>
	static constexpr action_s make() noexcept(std::is_nothrow_constructible_v<action_s, schfunc_t>) {
		static_assert(std::is_invocable_r_v<void, decltype(Func)>, "Function signature mismatch");
		#ifdef EFI_UNIT_TEST
			return action_s(&trampoline0<Func>, __PRETTY_FUNCTION__);
		#else
			return action_s(&trampoline0<Func>);
		#endif
	}

	void execute() const {
		if (m_callback) {
			m_callback(m_param);
		} else {
			#ifdef WE_HAVE_CRITICAL_ERROR_METHOD
			efiCriticalError("clear nullptr");
			#endif
			#ifdef EFI_UNIT_TEST
			assert(false);
			#endif
		}
	}

	[[nodiscard]] constexpr schfunc_t getCallback() const { return m_callback; }
	[[nodiscard]] constexpr uintptr_t const& getArgumentRaw() const { return m_param; }

	template<typename T>
	[[nodiscard]] constexpr T getArgument() const { return from_uintptr<T>(m_param); }

	explicit constexpr operator bool() const { return m_callback != nullptr; }

	bool constexpr operator==(const action_s& other) const {
		return m_callback == other.m_callback && m_param == other.m_param;
	}

#ifdef EFI_UNIT_TEST
	[[nodiscard]] constexpr char const* getCallbackName() const { return fn_name ? fn_name : "nullptr"; }
#endif

private:
	schfunc_t m_callback{};
	uintptr_t m_param{};

	// We want to have callback name in tests for easy debug
#ifdef EFI_UNIT_TEST
	char const* fn_name{};
	constexpr action_s(const schfunc_t callback, char const* const fn_name_) noexcept : m_callback(callback), fn_name{fn_name_} {}
	constexpr action_s(const schfunc_t callback, const uintptr_t param, char const* const fn_name_) noexcept : m_callback(callback), m_param(param), fn_name{fn_name_} {}
#else
	constexpr action_s(const schfunc_t callback) noexcept : m_callback(callback) {}
	constexpr action_s(const schfunc_t callback, const uintptr_t param) noexcept : m_callback(callback), m_param(param) {}
#endif

	// We can pass either pointer or integral this enforces the rule at compile time
	// Applies correct cast to argument
	template<typename T>
	static constexpr T from_uintptr(uintptr_t raw) noexcept {
		static_assert(std::is_pointer_v<T> || std::is_integral_v<T>, "Unsupported type");
		if constexpr (std::is_pointer_v<T>) {
			return reinterpret_cast<T>(raw);
		} else {
			return static_cast<T>(raw);
		}
	}

	// We can pass either pointer or integral this enforces the rule at compile time
	// Applies correct cast to argument
	template<typename T>
	static constexpr uintptr_t to_uintptr(T val) noexcept {
		static_assert(std::is_pointer_v<T> || std::is_integral_v<T>, "Unsupported type");
		if constexpr (std::is_pointer_v<T>) {
			return reinterpret_cast<uintptr_t>(val);
		} else {
			return static_cast<uintptr_t>(val);
		}
	}

	// Wraps the original function for unified signature
	// Applies correct cast to argument before dispatching to Func
	template<auto Func, typename Arg>
	static constexpr void trampoline(uintptr_t raw) noexcept {
		static_assert(std::is_pointer_v<Arg> || std::is_integral_v<Arg>, "Unsupported type");
		if constexpr (std::is_pointer_v<Arg>) {
			Func(reinterpret_cast<Arg>(raw));
		} else {
			Func(static_cast<Arg>(raw));
		}
	}

	// Wrapper for zero arg callbacks
	template<auto Func>
	static constexpr void trampoline0(uintptr_t) noexcept {
		Func();  // argument is ignored
	}
};

/**
 * This structure holds information about an event scheduled in the future: when to execute what callback with what parameters
 */
#pragma pack(push, 4)
struct scheduling_s {
  [[nodiscard]] efitick_t getMomentNt() const {
    return momentNt;
  }

#if EFI_UNIT_TEST
	[[nodiscard]] efitick_t getMomentUs() const {
		return NT2US(momentNt);
	}
#endif

  void setMomentNt(efitick_t p_moment) {
    momentNt = p_moment;
  }

#if EFI_SIMULATOR
  // used by signal_executor_sleep executor implementation
	virtual_timer_t timer;
#endif /* EFI_SIMULATOR */

	// Scheduler implementation uses a sorted linked list of these scheduling records.
	scheduling_s *nextScheduling_s = nullptr;

	action_s action;
	/**
	 * timestamp represented as 64-bit value of ticks since MCU start
	 */
private:
	volatile efitick_t momentNt = 0;
};
#pragma pack(pop)

struct Scheduler {
	/**
	 * @brief Schedule an action to be executed in the future.
	 *
	 * scheduleByAngle is useful if you want to schedule something in terms of crank angle instead of time.
	 *
	 * @param msg Name of this event to use for logging in case of an error.
	 * @param scheduling Storage to use for the scheduled event. If null, one will be used from the pool.
	 * @param targetTime When to execute the specified action. If this time is in the past or
	 *                   very near future, it may execute immediately.
	 * @param action An action to execute at the specified time.
	 */
	virtual void schedule(const char *msg, scheduling_s *scheduling, efitick_t targetTime, action_s const& action) = 0;

	/**
	 * @brief Cancel the specified scheduling_s so that, if currently scheduled, it does not execute.
	 *
	 * @param scheduling The scheduling_s to cancel.
	 */
	virtual void cancel(scheduling_s* scheduling) = 0;
};

Scheduler *getScheduler();
