#ifndef SRC_INC_IRRC_H_
#define SRC_INC_IRRC_H_

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

void IRRC_Init(InitIRRCHW_t init_io, SetIRRCHW_t read_io);
bool IRRC_Service(Triggers_t triggers);

#endif // SRC_INC_IRRC_H_
