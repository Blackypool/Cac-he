#ifndef MAIN_HEADER_H
#define MAIN_HEADER_H


#define DE_BUG_ON
#define TO_str(x) #x


//_________________________________________EXCLUSIVELY_FOR_PROJECT_________________________________________________________________________//

#define XXH_INLINE_ALL      // uint64_t h = XXH3_64bits(const void* input, size_t length);
#include "xx_hash.h"

//_________________________________________________________________________________________________________________________________________//



//____________________________________________FAST_COMMANDS________________________________________________________________________________//
    // git add .
    // git commit -m "update style"
    // git push

    // git pull
//_________________________________________________________________________________________________________________________________________//



//______________________________________________BIBLIOTECS_________________________________________________________________________________//
#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include <cstddef>
#include <sys/stat.h>
#include <cstddef>
#include <vector>
#include <list>
//_________________________________________________________________________________________________________________________________________//



//________________________________________________DEBUGS___________________________________________________________________________________//
#define ERROR_IN_FUN -1
#define ALL_RIGHT     1

enum errors_
{
    memory_aloca    = 1,
    oversize_any    = 2,
    syntax_err      = 3,
    give_null_ptr   = 4,
    file_errorr     = 5,
    error_in_deep   = 6,
    stack_errorr    = 7,
};

#ifdef DE_BUG_ON
    #define AsserT(what_need, type_err, retern)                                               \
            if(what_need) [[unlikely]]                                                         \
            {                                                                                   \
                std::cerr << "\n\nerror in line, " << __FILE__  << ":" << __LINE__ << ". Problem with:" << TO_str(type_err); \
                return retern;                                                                    \
            }
#else 
    #define AsserT(what_need, type_err, retern)
#endif     
//_________________________________________________________________________________________________________________________________________//



//______________________________________________HEADERS_OF_ANOTHER_________________________________________________________________________//
#include "get_any_size.h"
#include "M_cache.hpp"
//_________________________________________________________________________________________________________________________________________//



#endif // MAIN_HEADER_H