/*===============================================
includes
===============================================*/

#include	"utils.h"

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

void assert(bool val) {
	if (val) return;
	while (true)
		__asm("nop");
}


/*===============================================
private functions
===============================================*/
