/*===============================================
 includes
 ===============================================*/

#include	"stm32l0xx.h"
#include	<stdlib.h>
#include	<stdint.h>
#include	<stdbool.h>
#include	"irrc.h"
#include	"config.h"
#include	"utils.h"

/*===============================================
 private constants
 ===============================================*/

// clock config
#if MSI_CLK_DIV == 1
	#define	IRRC_MOD_PERIOD				((uint16_t)112)
	#define	IRRC_MOD_DUTY					((uint16_t)56)
	#define	IRRC_BASE_PRESCALE		(IRRC_MOD_PERIOD)
	#define	IRRC_BASE_DURATION		((uint16_t)29)
#elif MSI_CLK_DIV == 2
	#define	IRRC_MOD_PERIOD				((uint16_t)56)
	#define	IRRC_MOD_DUTY					((uint16_t)28)
	#define	IRRC_BASE_PRESCALE		(IRRC_MOD_PERIOD)
	#define	IRRC_BASE_DURATION		((uint16_t)29)
#elif MSI_CLK_DIV == 4
	#define	IRRC_MOD_PERIOD				((uint16_t)28)
	#define	IRRC_MOD_DUTY					((uint16_t)14)
	#define	IRRC_BASE_PRESCALE		(IRRC_MOD_PERIOD)
	#define	IRRC_BASE_DURATION		((uint16_t)29)
#elif MSI_CLK_DIV == 8
	#define	IRRC_MOD_PERIOD				((uint16_t)14)
	#define	IRRC_MOD_DUTY					((uint16_t)7)
	#define	IRRC_BASE_PRESCALE		(IRRC_MOD_PERIOD)
	#define	IRRC_BASE_DURATION		((uint16_t)29)
#elif MSI_CLK_DIV == 16
	#define	IRRC_MOD_PERIOD				((uint16_t)7)
	#define	IRRC_MOD_DUTY					((uint16_t)3)
	#define	IRRC_BASE_PRESCALE		(IRRC_MOD_PERIOD)
	#define	IRRC_BASE_DURATION		((uint16_t)29)
#else
	#error MSI_CLK_DIV must be an integral power of 2, in the range 1 to 16, for correct operation of the IR output.
#endif

// signalling config
#define		IRRC_MSG_SYMBOLS			((uint16_t)18)		// SOF + 16b (MSB) + IFG
#define		IRRC_MSG_REPEATS			((uint16_t)2)			// 2 is how many repeats the manufacturer uses
#define		IRRC_NUM_SYMBOLS			((uint16_t)(IRRC_MSG_SYMBOLS * IRRC_MSG_REPEATS))
#define		IRRC_SOF_DUTY					((uint16_t)1)
#define		IRRC_SOF_PERIOD				((IRRC_BASE_DURATION*3))
#define		IRRC_ONE_DUTY					((IRRC_BASE_DURATION*2))
#define		IRRC_ONE_PERIOD				((IRRC_BASE_DURATION*3))
#define		IRRC_ZERO_DUTY				((IRRC_BASE_DURATION*1))
#define		IRRC_ZERO_PERIOD			((IRRC_BASE_DURATION*2))
#define		IRRC_IFG_DUTY					((IRRC_BASE_DURATION*134))
#define		IRRC_IFG_PERIOD				((IRRC_BASE_DURATION*134))

// command config
#define		IRRC_NUM_COMMANDS			(4)
#define		IRRC_MAX_COUNTER			((int16_t)3)
#define		IRRC_POWER_TOGGLE			((uint16_t)0x5000)
#define		IRRC_ROTATE_TOGGLE		((uint16_t)0x50a8)
#define		IRRC_SPEED_UP					((uint16_t)0x5054)
#define		IRRC_SPEED_DOWN				((uint16_t)0x50fa)

/*===============================================
 private data prototypes
 ===============================================*/

typedef struct __attribute__((__packed__)) {
	uint16_t value;
	int16_t count;
} IRRCCommand_t;

typedef struct __attribute__((__packed__)) {
	uint16_t Duty[IRRC_NUM_SYMBOLS];
	uint16_t Period[IRRC_NUM_SYMBOLS];
} IRRCBitstream_t;

typedef struct {
	bool busy;
	InitIRRCHW_t initHW;
	SetIRRCHW_t setHW;
	IRRCCommand_t commands[IRRC_NUM_COMMANDS];
	IRRCBitstream_t bitstream;
} IRRCConfig_t;

/*===============================================
 private function prototypes
 ===============================================*/

static void IRRC_Encode(IRRCCommand_t *cmd);
static void IRRC_Transmit();

/*===============================================
 private global variables
 ===============================================*/

static IRRCConfig_t cfg = {
	false, 0, 0, {
		{ IRRC_POWER_TOGGLE, -1 },
		{ IRRC_SPEED_DOWN, -1 },
		{ IRRC_SPEED_UP, -1 },
		{ IRRC_ROTATE_TOGGLE, -1 }
	},
};

/*===============================================
 public functions
 ===============================================*/

void IRRC_Init(InitIRRCHW_t init_hw, SetIRRCHW_t set_hw) {
	assert(init_hw && set_hw);
	cfg.initHW = init_hw;
	cfg.setHW = set_hw;
	cfg.busy = false;
	init_hw(5, 5);
	// enable TIM2
	RCC->APB1ENR |= (1 << 0);
	// enable TIM21
	RCC->APB2ENR |= (1 << 2);
	// enable DMA
	RCC->AHBENR |= (1 << 0);
	// configure DMA
	DMA1_Channel2->CCR = 0;
	DMA1_Channel5->CCR = 0;
	DMA1->IFCR = (15<<16)+(15<<4); // clear IRQ flags for DMA1_CH5,CH2
	DMA1_CSELR->CSELR &= ~((15<<16)+(15<<4));
	DMA1_CSELR->CSELR |= ((8<<16)+(8<<4)); // TIM2_CH1->DMA1_CH5, TIM2_UP->DMA1_CH2
	NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
	NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0);
	// configure TIM21
	TIM21->CR1 = 0;
	TIM21->SMCR = (0<<4)+(5<<0); // TS=TIM2,SMS=GATED
	TIM21->PSC = 0;
	TIM21->ARR = IRRC_MOD_PERIOD - 1; // ~38kHz
	TIM21->CCR2 = IRRC_MOD_DUTY - 1; // 50% DC
	TIM21->CCMR1 = (6<<12); // CH2:PWM1
	TIM21->CCER = 0; // no output
	TIM21->EGR = (1<<0);
	// configure TIM2
	TIM2->CR1 = 0;
	TIM2->CR2 = (6<<4); // OC3REF:TRGO
	TIM2->DIER = 0;
	TIM2->PSC = IRRC_BASE_PRESCALE - 1;
	TIM2->CCMR2 = (7<<4); // CH3:PWM2
	TIM2->CCER = 0;
	TIM2->CCR3 = 0xffff;
	TIM2->EGR = (1<<0); // UEV
	cfg.busy = false;
}


bool IRRC_Service(Triggers_t triggers) {
	int32_t i;
	if (cfg.busy)
		return true;
	if (!triggers.val)
		return false;
	for (i = 0; i < IRRC_NUM_COMMANDS; i++) {
		if (triggers.val & (1<<i)) {
			cfg.busy = true;
			IRRC_Encode(&cfg.commands[i]);
			IRRC_Transmit();
			return true;
		}
	}
	return false;
}


/*===============================================
 interrupt handlers
 ===============================================*/

void DMA1_Channel2_3_IRQHandler(void) {
	TIM21->CCER = 0;
	TIM2->CCER = 0;
	TIM2->CR1 = 0;
	TIM2->CCR3 = 0xffff;
	TIM21->CR1 = 0;
	DMA1_Channel2->CCR = 0;
	DMA1_Channel5->CCR = 0;
	TIM2->EGR = (1<<0);
	cfg.busy = false;
	cfg.setHW(0);
}


/*===============================================
 private functions
 ===============================================*/

static void IRRC_Encode(IRRCCommand_t *cmd) {
	int16_t i, j;
	// pre-increment command-specific counter & ensure in range
	cmd->count++;
	if ((cmd->count > IRRC_MAX_COUNTER) || (cmd->count < 0))
		cmd->count = 0;
	// calculate value to be transmitted from base + count
	uint16_t val = cmd->value + cmd->count;
	// construct bitstream - SOF, message bits, IFG/EOF
	cfg.bitstream.Duty[0] = (uint16_t)IRRC_SOF_DUTY - 1;
	cfg.bitstream.Period[0] = (uint16_t)IRRC_SOF_PERIOD - 1;
	for (i = 0; i < 16; i++) {
		if (val & (1<<(15-i))) {
			cfg.bitstream.Duty[i+1] = (uint16_t)IRRC_ONE_DUTY - 1;
			cfg.bitstream.Period[i+1] = (uint16_t)IRRC_ONE_PERIOD - 1;
		}
		else {
			cfg.bitstream.Duty[i+1] = (uint16_t)IRRC_ZERO_DUTY - 1;
			cfg.bitstream.Period[i+1] = (uint16_t)IRRC_ZERO_PERIOD - 1;
		}
	}
	cfg.bitstream.Duty[17] = (uint16_t)IRRC_IFG_DUTY - 1;
	cfg.bitstream.Period[17] = (uint16_t)IRRC_IFG_PERIOD - 1;
	for (j = 1; j < IRRC_MSG_REPEATS; j++) {
		for (i = 0; i < IRRC_MSG_SYMBOLS; i++) {
			cfg.bitstream.Duty[i+(j*IRRC_MSG_SYMBOLS)] = cfg.bitstream.Duty[i];
			cfg.bitstream.Period[i+(j*IRRC_MSG_SYMBOLS)] = cfg.bitstream.Period[i];
		}
	}
	// Ensure that the duty cycle for the last symbol for all except the last
	// command copy never expires.  This prevents a glitch in the IR output level
	// signal.
	for (j = 0; j < (IRRC_MSG_REPEATS - 1); j++)
		cfg.bitstream.Duty[17+(j*IRRC_MSG_SYMBOLS)] = 0xffff;
}


static void IRRC_Transmit() {
	// configure DMA - CH2 for TIM2_UP, CH5 for TIM2_CH1
	DMA1->IFCR = (15<<12)+(15<<4);
	// DMA1_Ch2: triggered by TIM2_UP, sets new CCR3 value (immediate effect)
	DMA1_Channel2->CCR = (1<<10)+(1<<8)+(1<<7)+(1<<4)+(1<<3)+(1<<1); // MSZ=16,PSZ=16,MINC,M2P,TEIE,TCIE
	DMA1_Channel2->CPAR = (uint32_t)&TIM2->CCR3;
	DMA1_Channel2->CMAR = (uint32_t)&cfg.bitstream.Duty[1];
	DMA1_Channel2->CNDTR = (uint16_t)IRRC_NUM_SYMBOLS - 1; // skip the 1st CCR3 value
	// DMA1_Ch5: triggered by TIM2_CH1, sets new ARR value (buffered write)
	DMA1_Channel5->CCR = (1<<10)+(1<<8)+(1<<7)+(1<<4)+(1<<3)+(1<<1); // MSZ=16,PSZ=16,MINC,M2P,TEIE,TCIE
	DMA1_Channel5->CPAR = (uint32_t)&TIM2->ARR;
	DMA1_Channel5->CMAR = (uint32_t)&cfg.bitstream.Period[1];
	DMA1_Channel5->CNDTR = (uint16_t)IRRC_NUM_SYMBOLS - 1; // skip the 1st ARR value
	// setup TIM2, including forcing a UEV and preloading the first ARR and CCR3 values
	TIM2->CR1 = 0;
	TIM2->DIER = 0;
	TIM2->EGR = (1<<0);
	TIM2->CCER = (1<<8); // CCER3
	TIM2->CCR1 = 4; // some nominal value, large enough to avoid DMA collisions, but far lower than the actual duty cycle
	TIM2->CCR3 = cfg.bitstream.Duty[0];
	TIM2->ARR = cfg.bitstream.Period[0];
	TIM2->DIER = (1<<9)+(1<<8); //CC1DE,UDE
	// enable TIM21
	TIM21->CNT = 0;
	TIM21->EGR = (1<<0);
	TIM21->CR1 = (1<<0);
	// enable DMA and TIM2
	DMA1_Channel2->CCR |= (1<<0); // enable TIM2_UP DMA
	DMA1_Channel5->CCR |= (1<<0); // enable TIM2_CH1 DMA
	TIM21->CCER = (1<<4); // enable TIM21 output
	TIM2->CR1 = (1<<7)+(1<<0); // buffer ARR,EN
	cfg.setHW(1);
}

