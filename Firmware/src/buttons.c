/*===============================================
 includes
 ===============================================*/

#include	"stm32l0xx.h"
#include	<stdint.h>
#include	<stdbool.h>
#include	<stdlib.h>
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
	int32_t debounce;
	int32_t repeat;
	ButtonState_t state;
	uint32_t timestamp;
} ButtonStatus_t;

typedef struct {
	void (*initHW)(void);
	int32_t (*readHW)(int32_t id);
	uint32_t (*readClk)(void);
	int32_t numButtons;
	ButtonStatus_t *buttons;
} ButtonsConfig_t;


/*===============================================
 private function prototypes
 ===============================================*/

static bool Button_AnyActive();

/*===============================================
 private global variables
 ===============================================*/

static ButtonsConfig_t cfg = { 0, 0 };

/*===============================================
 public functions
 ===============================================*/

void Buttons_Init(
	const InitButtonHW_t init_hw,
	const ReadButtonHW_t read_hw,
	const GetClock_t read_clk,
	const int32_t numButtons,
	const ButtonSetup_t * const buttons
) {
	assert(init_hw && read_hw && read_clk);
	cfg.initHW = init_hw;
	cfg.readHW = read_hw;
	cfg.readClk = read_clk;
	cfg.numButtons = numButtons > 0 ? numButtons : 0;
	cfg.buttons = cfg.numButtons > 0 ? calloc(cfg.numButtons, sizeof(ButtonStatus_t)) : 0;
	init_hw();
	for (int32_t i = 0; i < cfg.numButtons; i++) {
		cfg.buttons[i].debounce = (buttons[i].debounce + (SYSTICK_MS / 2)) / SYSTICK_MS;
		if (cfg.buttons[i].debounce <= 0 && buttons[i].debounce > 0) cfg.buttons[i].debounce = 1;
		cfg.buttons[i].repeat = (buttons[i].repeat + (SYSTICK_MS / 2)) / SYSTICK_MS;
		if (cfg.buttons[i].repeat <= 0 && buttons[i].repeat > 0) cfg.buttons[i].repeat = 1;
		cfg.buttons[i].state = ButtonIdle;
		cfg.buttons[i].timestamp = 0;
	}
}

 bool Buttons_Service(Triggers_t *triggers) {
	Triggers_t btrigs;
	if (cfg.numButtons <= 0 || !cfg.buttons)
		return false;
	btrigs.val = 0;
	int32_t i, bval;
	uint32_t t = cfg.readClk();
	for (i = 0; i < cfg.numButtons; i++) {
		bval = cfg.readHW(i);
		switch (cfg.buttons[i].state) {
			case ButtonTriggered:
				if (!bval) {
					cfg.buttons[i].timestamp = t;
					cfg.buttons[i].state = ButtonIdle;
				}
				else if ((t - cfg.buttons[i].timestamp) >= cfg.buttons[i].debounce) {
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
				else if ((cfg.buttons[i].repeat > 0) && ((t - cfg.buttons[i].timestamp) >= cfg.buttons[i].repeat)) {
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
	for (i = 0; i < cfg.numButtons; i++) {
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
	for (int32_t i = 0; i < cfg.numButtons; i++) {
		if (cfg.buttons[i].state != ButtonIdle)
			return true;
	}
	return false;
}

