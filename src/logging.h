#ifndef CLASSLMDB_LOGGING_H_INCLUDED
#define CLASSLMDB_LOGGING_H_INCLUDED


#define logg(str, ...) do {                                             \
                           printf ("-------------------------------------\n");   \
                           printf ("XXXXX: %s:%d:  " str , __FILE__, __LINE__, ##__VA_ARGS__);  \
                           printf ("\n");                               \
                           fflush (stdout);    \
                       } while (0);

#define log(str) logg(str "%s", "");


#endif
