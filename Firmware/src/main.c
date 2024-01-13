/*===============================================
 includes
 ===============================================*/

#include	"stm32l0xx.h"
#include	"system.h"
#include	"config.h"
#include	"buttons.h"
#include	"irrc.h"

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

/*===============================================
 private function prototypes
 ===============================================*/

/*===============================================
 private global variables
 ===============================================*/

/*===============================================
 public functions
 ===============================================*/

int main() {
	Triggers_t triggers;
	bool buttons, fan;
	System_Init();
	Buttons_Init(System_InitButtonIO, System_ReadButtonIO, System_Ticks);
	IRRC_Init(System_InitIRIO, System_SetIRIO);
	while (1) {
		buttons = Buttons_Service(&triggers);
		fan = IRRC_Service(triggers);
		if (!buttons && !fan)
			System_Stop();
	}
	return 0;
}

/*===============================================
 private functions
 ===============================================*/

