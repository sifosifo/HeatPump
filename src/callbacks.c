#include "callbacks.h"

static error_detected_cb_t error_cb = 0;

void register_error_callback(error_detected_cb_t cb)
{
	error_cb = cb;
}

// Helper that modules will call
void notify_error(uint8_t error_code, uint8_t type)
{
	//error_cb(error_code, type);    
}