#include <stdint.h>
#include "errors.h"

uint8_t ErrorsHistory[100][2];
uint8_t ErrorsActive[10][2];
uint8_t ActiveErrorCount = 0;

void SetError(uint8_t ModuleID, uint8_t error_code)
{
	uint8_t i;
	uint8_t Found = 0;

	for(i = 0; (i<ActiveErrorCount) && (Found==0); i++)
	{
		if(ErrorsActive[i][MODULE_ID]==ModuleID)
		{
			if(ErrorsActive[i][ERROR_ID])
			{
				Found = 1;
			}
		}
	}

	if(Found)
	{
		
	}else
	{
		ActiveErrorCount++;

	}
}
