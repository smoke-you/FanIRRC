#ifndef SRC_INC_BUTTONS_H_
#define SRC_INC_BUTTONS_H_

/*===============================================
 includes
 ===============================================*/

#include	<stdint.h>
#include	<stdbool.h>
#include	"config.h"

/*===============================================
 public constants
 ===============================================*/

/*===============================================
 public data prototypes
 ===============================================*/

/*===============================================
 public function prototypes
 ===============================================*/

void Buttons_Init(
	const InitButtonHW_t init_hw,
	const ReadButtonHW_t read_hw,
	const GetClock_t read_clk,
	const int32_t numButtons,
	const ButtonSetup_t * const buttons
);
bool Buttons_Service(Triggers_t *triggers);

#endif // SRC_INC_BUTTONS_H_
