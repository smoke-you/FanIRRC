#ifndef SRC_INC_SYSTEM_H_
#define SRC_INC_SYSTEM_H_

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

void System_Init(void);
uint32_t System_Ticks(void);
void System_Stop(void);
void System_InitButtonIO(void);
int32_t System_ReadButtonIO(const int32_t id);
void System_InitIRIO(uint8_t mod_af, uint8_t level_af);
void System_SetIRIO(const int32_t val);

#endif // SRC_INC_SYSTEM_H_
