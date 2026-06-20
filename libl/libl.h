/**
 * @file libl.h
 * @brief Public interface for the ft_lex companion library (-lfl).
 */
#ifndef LIBL_H
#define LIBL_H

/** @brief Default end-of-file handler; always returns 1 (stop scanning). */
int yywrap(void);

#endif