/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

/*
 * All preprocessors should update in place
 */
typedef void (*preprocessor_t) (char*);


/* =============================================================================
 * preprocessor_convertURNHex
 * -- Translate % hex escape sequences
 * =============================================================================
 */
void
preprocessor_convertURNHex (char* str);


/* =============================================================================
 * preprocesser_toLower
 * -- Translate uppercase letters to lowercase
 * =============================================================================
 */
void
preprocessor_toLower (char* str);
