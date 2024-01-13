/*===============================================
 includes
 ===============================================*/

#include	"stm32l0xx.h"
#include	"system.h"

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

typedef struct {
	uint32_t ticks;
} SystemConfig_t;

/*===============================================
 private function prototypes
 ===============================================*/

/*===============================================
 private global variables
 ===============================================*/

static SystemConfig_t cfg = { 0 };

/*===============================================
 public functions
 ===============================================*/

void System_Init(void) {
	// set up the system clock
	RCC->ICSCR = (RCC->ICSCR & ~(7 << 13)) | (MSI_CLK_RANGE << 13);
	// configure the system to drop in and out of STOP mode
	FLASH->ACR |= (1 << 3); // power-down Flash in LP modes
	RCC->APB1ENR |= (1 << 28); // PWREN
	PWR->CR &= ~(1 << 1); // !PDDS - required to enter STOP mode
	PWR->CR |= (1 << 16) + (3 << 11) + (1 << 9) + (1 << 2) + (0 << 1) + (1 << 0); // LDPS,VREG=Range3(1.2V),CWUF,ULP,LPDSR
	RCC->CFGR &= ~(1 << 15); // !STOPWUK, i.e. use MSI on wakeup from STOP
#if BOARD_TYPE == BOARD_CUSTOM
	// STOP mode signalled on PA0
	RCC->IOPENR |= (1 << 0);
	RCC->IOPSMENR |= (1 << 0);
	GPIOA->MODER &= ~(3 << 0);
	GPIOA->MODER |= (1 << 0);
	GPIOA->BSRR = (1 << 0); // RUN->1, STOP->0
#else
	  // STOP mode signalled on PA9
	  RCC->IOPENR |= (1 << 0);
	  RCC->IOPSMENR |= (1 << 0);
	  GPIOA->MODER &= ~(3 << 18);
	  GPIOA->MODER |= (1 << 18);
	  GPIOA->BSRR = (1 << 9);// RUN->0, STOP->1
#endif
	// set up the systick timer
	NVIC_EnableIRQ(SysTick_IRQn);
	NVIC_SetPriority(SysTick_IRQn, 0);
	SysTick->LOAD = (uint16_t)(SYSTICK_OVERFLOW - 1);
	SysTick->VAL = 0;
	SysTick->CTRL = (1 << 2) + (1 << 1) + (1 << 0); // CPUclk(HCLK?),INTE,EN
}

uint32_t System_Ticks(void) {
	return cfg.ticks;
}

void System_Stop(void) {
#if BOARD_TYPE == BOARD_CUSTOM
	GPIOA->BSRR = (1 << 16);
#else
	GPIOA->BSRR = (1 << 25);
#endif
	SysTick->CTRL &= ~(1 << 1); // clear INTE
	NVIC_DisableIRQ(SysTick_IRQn);
	SCB->SCR |= (1 << 4) + (1 << 2); // SEVONPEND, SLEEPDEEP
	__SEV();
	__WFE();
	__WFE();
	SCB->SCR &= ~((1 << 4) + (1 << 2)); // !SEVONPEND, !SLEEPDEEP
	SysTick->CTRL |= (1 << 1); // set INTE
	NVIC_EnableIRQ(SysTick_IRQn);
#if BOARD_TYPE == BOARD_CUSTOM
	GPIOA->BSRR = (1 << 0);
#else
	GPIOA->BSRR = (1 << 9);
#endif
}

void System_InitButtonIO(void) {
#if BOARD_TYPE == BOARD_CUSTOM
	// enable buttons for input - PA9-12 for the custom board
	RCC->IOPENR |= (1 << 0);
	RCC->IOPSMENR |= (1 << 0);
	GPIOA->MODER &= ~((3 << 24) + (3 << 22) + (3 << 20) + (3 << 18));
	GPIOA->PUPDR &= ~((3 << 24) + (3 << 22) + (3 << 20) + (3 << 18));
	GPIOA->PUPDR |= ((1 << 24) + (1 << 22) + (1 << 20) + (1 << 18));
	// enable events and interrupts on NGE of PA9-12
	RCC->APB2ENR |= (1 << 0); // SYSCFEN
	SYSCFG->EXTICR[2] &= ~((15 << 12) + (15 << 8) + (15 << 4)); // map PA11,10,9 to EXTI11,10,9
	SYSCFG->EXTICR[3] &= ~(15 << 0); // map PA12 to EXTI12
	EXTI->IMR |= ((1 << 12) + (1 << 11) + (1 << 10) + (1 << 9)); // unmask I12,11,10,9
	EXTI->EMR |= ((1 << 12) + (1 << 11) + (1 << 10) + (1 << 9)); // unmask E12,11,10,9
	EXTI->FTSR |= ((1 << 12) + (1 << 11) + (1 << 10) + (1 << 9)); // NGE for E/I12,11,10,9
#else
	  // enable buttons for input - PA0,1 and PB4,5 for the Nucleo board
	  RCC->IOPENR |= (1 << 1) + (1 << 0);
	  RCC->IOPSMENR |= (1 << 1) + (1 << 0);
	  GPIOA->MODER &= ~((3 << 2) + (3 << 0));
	  GPIOA->PUPDR &= ~((3 << 2) + (3 << 0));
	  GPIOA->PUPDR |= (1 << 2) + (1 << 0);
	  GPIOB->MODER &= ~((3 << 10) + (3 << 8));
	  GPIOB->PUPDR &= ~((3 << 10) + (3 << 8));
	  GPIOB->PUPDR |= (1 << 10) + (1 << 8);
	  // enable events and interrupts on NGE of PA0,1 and PB4,5
	  RCC->APB2ENR |= (1 << 0);// SYSCFEN
	  SYSCFG->EXTICR[0] &= ~((15 << 4) + (15 << 0));
	  SYSCFG->EXTICR[0] |= ((0 << 4) + (0 << 0));// EXTI1->PA1, EXTI0->PA0
	  SYSCFG->EXTICR[1] &= ~((15 << 4) + (15 << 0));
	  SYSCFG->EXTICR[1] |= ((1 << 4) + (1 << 0));// EXTI5->PB5, EXTI4->PB4
	  EXTI->IMR |= (1 << 5) + (1 << 4) + (1 << 1) + (1 << 0);// unmask E5,4,1,0
	  EXTI->EMR |= (1 << 5) + (1 << 4) + (1 << 1) + (1 << 0);// unmask E5,4,1,0
	  EXTI->FTSR |= (1 << 5) + (1 << 4) + (1 << 1) + (1 << 0);// NGE for E5,4,1,0
#endif
}

int32_t System_ReadButtonIO(const int32_t id) {
#if BOARD_TYPE == BOARD_CUSTOM
	/* PA9  -> BTN0
	 * PA11 -> BTN1
	 * PA10 -> BTN2
	 * PA12 -> BTN3
	 * */
	switch (id) {
		case 0:
			return (GPIOA->IDR & (1 << 9)) == 0;
		case 1:
			return (GPIOA->IDR & (1 << 11)) == 0;
		case 2:
			return (GPIOA->IDR & (1 << 10)) == 0;
		case 3:
			return (GPIOA->IDR & (1 << 12)) == 0;
		default:
			return 0;
	}
#else
	/* PA0 -> BTN0
	 * PA1 -> BTN1
	 * PB4 -> BTN2
	 * PB5 -> BTN3
	 * */
	switch (id) {
		case 0:
		return (GPIOA->IDR & (1 << 0)) == 0;
		case 1:
		return (GPIOA->IDR & (1 << 1)) == 0;
		case 2:
		return (GPIOB->IDR & (1 << 4)) == 0;
		case 3:
		return (GPIOB->IDR & (1 << 5)) == 0;
		default:
		return 0;
	}
#endif
}

void System_InitIRIO(uint8_t mod_af, uint8_t level_af) {
#if BOARD_TYPE == BOARD_CUSTOM
	// enable GPIOA
	RCC->IOPENR |= (1 << 0);
	// configure PA3 for IR modulation output
	GPIOA->MODER &= ~(3 << 6);
	GPIOA->MODER |= (2 << 6);
	GPIOA->OSPEEDR &= ~(3 << 6);
	GPIOA->OSPEEDR |= (2 << 6);
	GPIOA->AFR[0] &= ~(15 << 12);
	GPIOA->AFR[0] |= (0 << 12);
	// configure PA2 for IR level output
	GPIOA->MODER &= ~(3 << 4);
	GPIOA->MODER |= (2 << 4);
	GPIOA->OSPEEDR &= ~(3 << 4);
	GPIOA->OSPEEDR |= (2 << 4);
	GPIOA->AFR[0] &= ~(15 << 8);
	GPIOA->AFR[0] |= (2 << 8);
	// configure PA1 for "IR active" signalling
	GPIOA->MODER &= ~((3 << 2));
	GPIOA->MODER |= (1 << 2);
	GPIOA->BSRR = (1 << 17);
#else
	// enable GPIOA,B
	RCC->IOPENR |= (1 << 1) + (1 << 0);
	// configure PA11 for IR modulation output
	GPIOA->MODER &= ~(3 << 22);
	GPIOA->MODER |= (2 << 22);
	GPIOA->OSPEEDR &= ~(3 << 22);
	GPIOA->OSPEEDR |= (2 << 22);
	GPIOA->AFR[1] &= ~(15 << 12);
	GPIOA->AFR[1] |= (5 << 12);
	// configure PB6 for IR level output
	GPIOB->MODER &= ~(3 << 12);
	GPIOB->MODER |= (2 << 12);
	GPIOB->OSPEEDR &= ~(3 << 12);
	GPIOB->OSPEEDR |= (2 << 12);
	GPIOB->AFR[0] &= ~(15 << 24);
	GPIOB->AFR[0] |= (5 << 24);
	// configure PB3 for "IR active" signalling (via the Nucleo user LED)
	GPIOB->MODER &= ~((3 << 6));
	GPIOB->MODER |= (1 << 6);
	GPIOB->BSRR = (1 << 19);
#endif
}

void System_SetIRIO(const int32_t val) {
#if BOARD_TYPE == BOARD_CUSTOM
	if (val)
		GPIOA->BSRR = (1 << 1);
	else
		GPIOA->BSRR = (1 << 17);
#else
	if (val)
	GPIOB->BSRR = (1 << 3);
	else
	GPIOB->BSRR = (1 << 19);
#endif
}

/*===============================================
 private functions
 ===============================================*/

/*===============================================
 interrupt handlers
 ===============================================*/

void SysTick_Handler(void) {
	cfg.ticks++;
}
