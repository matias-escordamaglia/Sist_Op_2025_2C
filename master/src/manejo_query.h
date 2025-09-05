#ifndef QUERY_H_
#define QUERY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

#include "./utils/utils.h"
#include "sync.h"

void* manejar_query(void* arg);



#endif /* QUERY_H_ */