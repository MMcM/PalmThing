#ifndef _SECTIONS_H
#define _SECTIONS_H

#define LIST_SECTION
#define VIEW_SECTION LIST_SECTION
#define EDIT_SECTION __attribute__ ((section ("editform")))
#define NOTE_SECTION EDIT_SECTION
#define ISBN_SECTION __attribute__ ((section ("isbnform")))

#endif 
