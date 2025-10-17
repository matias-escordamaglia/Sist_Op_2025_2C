#ifndef OPERACIONES_H_
#define OPERACIONES_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <commons/log.h>
#include <commons/config.h>
#include "./utils/utils.h"
#include <commons/bitarray.h>


void create(char* , char* , char* );


#endif /* OPERACIONES_H_ */