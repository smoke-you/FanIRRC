/*===============================================
 includes
 ===============================================*/

#include	"stm32l0xx.h"
#include	<stdint.h>
#include	<stdbool.h>
#include	"buttons.h"
#include	"config.h"
#include	"utils.h"

/*===============================================
 private constants
 ===============================================*/

/*===============================================
 private data prototypes
 ===============================================*/

typedef enum {
	ButtonIdle = 0,
	ButtonTriggered,
	ButtonActive,
} ButtonState_t;

typedef struct {
	ButtonState_t state;
	uint32_t timestamp;
} ButtonStatus_t;

typedef struct {
	void (*initHW)(void);
	int32_t (*readHW)(int32_t id);
	uint32_t (*readClk)(void);
	ButtonStatus_t buttons[NUM_BUTTONS];
} ButtonConfig_t;


/*===============================================
 private function prototypes
 ===============================================*/

static bool Button_AnyActive();

/*===============================================
 private global variables
 ===============================================*/

static ButtonConfig_t cfg = { 0, 0 };

/*===============================================
 public functions
 ===============================================*/

void Buttons_Init(InitButtonHW_t init_hw, ReadButtonHW_t read_hw, GetClock_t read_clk) {
	assert(init_hw && read_hw && read_clk);
	cfg.initHW = init_hw;
	cfg.readHW = read_hw;
	cfg.readClk = read_clk;
	init_hw();
	for (int32_t i = 0; i < NUM_BUTTONS; i++)
		cfg.buttons[i].state = 0;
}

 bool Buttons_Service(Triggers_t *triggers) {
	Triggers_t btrigs;
	btrigs.val = 0;
	int32_t i, bval;
	uint32_t t = cfg.readClk();
	for (i = 0; i < NUM_BUTTONS; i++) {
		bval = cfg.readHW(i);
		switch (cfg.buttons[i].state) {
			case ButtonTriggered:
				if (!bval) {
					cfg.buttons[i].timestamp = t;
					cfg.buttons[i].state = ButtonIdle;
				}
				else if ((t - cfg.buttons[i].timestamp) >= BTN_DEBOUNCE_DELAY) {
					cfg.buttons[i].timestamp = t;
					cfg.buttons[i].state = ButtonActive;
					btrigs.val |= (1 << i);
				}
				// implied else - no change
				break;
			case ButtonActive:
				if (!bval) {
					cfg.buttons[i].timestamp = t;
					cfg.buttons[i].state = ButtonIdle;
				}
				else if ((t - cfg.buttons[i].timestamp) >= BTN_REPEAT_DELAY) {
					cfg.buttons[i].timestamp = t;
					btrigs.val |= (1 << i);
				}
				// implied else - no change
				break;
			case ButtonIdle:
			default:
				cfg.buttons[i].timestamp = t;
				if (bval)
					cfg.buttons[i].state = ButtonTriggered;
				else
					cfg.buttons[i].state = ButtonIdle;
		}
	}
	for (i = 0; i < NUM_BUTTONS; i++) {
		if (cfg.buttons[i].state == ButtonActive)
			btrigs.val &= ((1 << (i + 1)) - 1);
	}
	*triggers = btrigs;
	return Button_AnyActive();
}


/*===============================================
 private functions
 ===============================================*/

static bool Button_AnyActive() {
	for (int32_t i = 0; i < NUM_BUTTONS; i++) {
		if (cfg.buttons[i].state != ButtonIdle)
			return true;
	}
	return false;
}

