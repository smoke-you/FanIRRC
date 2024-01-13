#ifndef SRC_INC_CONFIG_H_
#define SRC_INC_CONFIG_H_

/*===============================================
 user-defined public constants
 ===============================================*/

//#define	BOARD_TYPE				(BOARD_NUCLEO)
#define	BOARD_TYPE				(BOARD_CUSTOM)
/* The board that is being built for.
 * Options are BOARD_NUCLEO (0) or BOARD_CUSTOM (1).
 * This is only relevant for GPIO's, in "system.c".
 * */

#define	MSI_CLK_DIV				(16)
/* The clock divider to be applied to the MSI clock when the CPU is running.
 * Must be an integral power of two in the range 1-64 (2^0-2^6).
 * */
#define	SYSTICK_MS				(10)
/* The (approximate) number of milliseconds between systick counts (and
 * interrupts). Must be a positive integer.
 * Note that the systick timer is a 16-bit counter that is driven by the system
 * clock.  If the value of (SYSTICK_MS * SYS_CLK / 1000) > 65536 then a
 * compiler error will be raised.
 * */

/*===============================================
 public data types
 ===============================================*/

typedef void (*InitButtonHW_t)(void);
typedef int32_t (*ReadButtonHW_t)(const int32_t);
typedef void (*InitIRRCHW_t)(uint8_t, uint8_t);
typedef void (*SetIRRCHW_t)(const int32_t);
typedef uint32_t (*GetClock_t)(void);

typedef union {
	uint32_t val;
	struct __attribute__((__packed__)) {
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
	};
} Triggers_t;

typedef struct {
	const int32_t debounce;
	const int32_t repeat;
} ButtonSetup_t;

/*===============================================
 calculated public constants
 ===============================================*/

#define	BOARD_CUSTOM					(1)
#define	BOARD_NUCLEO					(0)

#ifndef BOARD_TYPE
	#define BOARD_TYPE					(BOARD_NUCLEO)
#endif
#if BOARD_TYPE < BOARD_NUCLEO
	#undef BOARD_TYPE
	#define BOARD_TYPE					(BOARD_NUCLEO)
#elif BOARD_TYPE > BOARD_CUSTOM
	#undef BOARD_TYPE
	#define BOARD_TYPE					(BOARD_NUCLEO)
#endif

#ifndef MSI_CLK_DIV
	#define MSI_CLK_DIV					(2)
#endif
#if MSI_CLK_DIV == 1
	#define		MSI_CLK_RANGE			(6)
#elif MSI_CLK_DIV == 2
	#define		MSI_CLK_RANGE			(5)
#elif MSI_CLK_DIV == 4
	#define		MSI_CLK_RANGE			(4)
#elif MSI_CLK_DIV == 8
	#define		MSI_CLK_RANGE			(3)
#elif MSI_CLK_DIV == 16
	#define		MSI_CLK_RANGE			(2)
#elif MSI_CLK_DIV == 32
	#define		MSI_CLK_RANGE			(1)
#elif MSI_CLK_DIV == 64
	#define		MSI_CLK_RANGE			(0)
#else
	#error MSI_CLK_DIV must be an integral power of 2, in the range 1 to 64.
#endif

#define	MSI_BASE_FREQ					(1<<22)  // 4194304Hz, approximately
#define	SYS_CLK								(MSI_BASE_FREQ / MSI_CLK_DIV)

#ifndef SYSTICK_MS
	#define SYSTICK_MS					(10)
#endif
#if (SYSTICK_MS < 1)
	#undef SYSTICK_MS
	#define SYSTICK_MS 					(1)
#elif (SYSTICK_MS > 100)
	#undef SYSTICK_MS
	#define SYSTICK_MS					(100)
#endif
#define	SYSTICK_OVERFLOW 			((SYS_CLK * SYSTICK_MS) / 1000)
#if (SYSTICK_OVERFLOW > 65536)
	#error "System tick timer will overflow; reduce SYS_CLK or SYSTICK_MS"
#endif

#endif // SRC_INC_CONFIG_H_
