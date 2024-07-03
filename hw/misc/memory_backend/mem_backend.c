#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h" /* provides error_fatal() handler */
#include "hw/sysbus.h" /* provides all sysbus registering func */
#include "hw/misc/memory_backend/mem_backend.h"

